#include <dlfcn.h>
#include <mach/mach.h>
#include <mach/mach_vm.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/sysctl.h>
#include <unistd.h>

extern const char *const APP_SANDBOX_READ;
extern char *sandbox_extension_issue_file(const char *extension_class, const char *path, uint32_t flags);

extern char m_start[];
extern char m_end[];
extern char m_pthread_create_addr[];
extern char m_sandbox_consume_addr[];
extern char m_dlopen_addr[];
extern char m_payload_path[];
extern char m_sandbox_token[];

#include <ptrauth.h>
kern_return_t (*_thread_convert_thread_state)(
    thread_act_t thread, int direction, thread_state_flavor_t flavor,
    thread_state_t in_state, mach_msg_type_number_t in_stateCnt,
    thread_state_t out_state, mach_msg_type_number_t *out_stateCnt);

char *resolvePath(char *pathToResolve) {
  if (strlen(pathToResolve) == 0)
    return NULL;
  if (pathToResolve[0] == '/') {
    return strdup(pathToResolve);
  } else {
    char absolutePath[PATH_MAX];
    if (realpath(pathToResolve, absolutePath) == NULL) {
      perror("[resolvePath] realpath");
      return NULL;
    }
    return strdup(absolutePath);
  }
}

int main(int argc, char **argv) {
  if (argc < 3 || argc > 4) {
    fprintf(stderr, "Usage: %s <pid> <path/to/dylib>\n", argv[0]);
    return -1;
  }

  pid_t pid = atoi(argv[1]);
  char *payload_path = resolvePath(argv[2]);

  int result = 0;
  mach_port_t task = 0;
  thread_act_t thread = 0;
  mach_vm_address_t code = 0;
  mach_vm_address_t stack = 0;
  vm_size_t stack_size = 16 * 1024;
  uint64_t stack_contents = 0x00000000CAFEBABE;

  char *sandbox_token = sandbox_extension_issue_file(APP_SANDBOX_READ, payload_path, 0);
  if (!sandbox_token) {
    fprintf(stderr, "could not issue sandbox extension token\n");
    return 1;
  }

  if (task_for_pid(mach_task_self(), pid, &task) != KERN_SUCCESS) {
    fprintf(stderr, "could not retrieve task port for pid: %d\n", pid);
    return 1;
  }

  if (mach_vm_allocate(task, &stack, stack_size, VM_FLAGS_ANYWHERE) != KERN_SUCCESS) {
    fprintf(stderr, "could not allocate stack segment\n");
    return 1;
  }

  if (mach_vm_write(task, stack, (vm_address_t)&stack_contents,
                    sizeof(uint64_t)) != KERN_SUCCESS) {
    fprintf(stderr, "could not copy dummy return address into stack segment\n");
    return 1;
  }

  if (vm_protect(task, stack, stack_size, 1, VM_PROT_READ | VM_PROT_WRITE) != KERN_SUCCESS) {
    fprintf(stderr, "could not change protection for stack segment\n");
    return 1;
  }

  unsigned char *SHELLCODE = (unsigned char *)m_start;
  const uintptr_t SHELLCODE_SIZE = m_end - m_start;
  const uintptr_t PTHREAD_CREATE = m_pthread_create_addr - m_start;
  const uintptr_t SANDBOX_CONSUME = m_sandbox_consume_addr - m_start;
  const uintptr_t DLOPEN = m_dlopen_addr - m_start;
  const uintptr_t PAYLOAD_PATH = m_payload_path - m_start;
  const uintptr_t SANDBOX_TOKEN = m_sandbox_token - m_start;

  if (mach_vm_allocate(task, &code, SHELLCODE_SIZE, VM_FLAGS_ANYWHERE) != KERN_SUCCESS) {
    fprintf(stderr, "could not allocate code segment\n");
    return 1;
  }

  uint64_t pcfmt_address = (uint64_t)ptrauth_strip(dlsym(RTLD_DEFAULT, "pthread_create_from_mach_thread"), ptrauth_key_function_pointer);
  uint64_t dlopen_address = (uint64_t)ptrauth_strip(dlsym(RTLD_DEFAULT, "dlopen"), ptrauth_key_function_pointer);
  uint64_t sandbox_consume_address = (uint64_t)ptrauth_strip(dlsym(RTLD_DEFAULT, "sandbox_extension_consume"), ptrauth_key_function_pointer);

  memcpy(SHELLCODE + PTHREAD_CREATE, &pcfmt_address, sizeof(uint64_t));
  memcpy(SHELLCODE + SANDBOX_CONSUME, &sandbox_consume_address, sizeof(uint64_t));
  memcpy(SHELLCODE + DLOPEN, &dlopen_address, sizeof(uint64_t));
  memcpy(SHELLCODE + PAYLOAD_PATH, payload_path, strlen(payload_path));
  memcpy(SHELLCODE + SANDBOX_TOKEN, sandbox_token, strlen(sandbox_token));

  if (mach_vm_write(task, code, (vm_address_t)SHELLCODE, SHELLCODE_SIZE) != KERN_SUCCESS) {
    fprintf(stderr, "could not copy shellcode into code segment\n");
    return 1;
  }

  if (vm_protect(task, code, SHELLCODE_SIZE, 0, VM_PROT_EXECUTE | VM_PROT_READ) != KERN_SUCCESS) {
    fprintf(stderr, "could not change protection for code segment\n");
    return 1;
  }

  void *handle = dlopen("/usr/lib/system/libsystem_kernel.dylib", RTLD_GLOBAL | RTLD_LAZY);
  if (handle) {
    _thread_convert_thread_state = dlsym(handle, "thread_convert_thread_state");
    dlclose(handle);
  }

  if (!_thread_convert_thread_state) {
    fprintf(stderr, "could not load symbol: thread_convert_thread_state\n");
    return 1;
  }

  arm_thread_state64_t thread_state = {}, machine_thread_state = {};
  thread_state_flavor_t thread_flavor = ARM_THREAD_STATE64;
  mach_msg_type_number_t thread_flavor_count = ARM_THREAD_STATE64_COUNT, machine_thread_flavor_count = ARM_THREAD_STATE64_COUNT;

  __darwin_arm_thread_state64_set_pc_fptr(thread_state, ptrauth_sign_unauthenticated((void *)code, ptrauth_key_asia, 0));
  __darwin_arm_thread_state64_set_sp(thread_state, stack + (stack_size / 2));

  kern_return_t error = thread_create(task, &thread);
  if (error != KERN_SUCCESS) {
    fprintf(stderr, "could not create remote thread: %s\n", mach_error_string(error));
    return 1;
  }

  error = _thread_convert_thread_state(
      thread, 2, thread_flavor, (thread_state_t)&thread_state,
      thread_flavor_count, (thread_state_t)&machine_thread_state,
      &machine_thread_flavor_count);
  if (error != KERN_SUCCESS) {
    fprintf(stderr, "could not convert thread state: %s\n", mach_error_string(error));
    return 1;
  }

  char os_version_str[32];
  size_t size = sizeof(os_version_str);
  sysctlbyname("kern.osproductversion", os_version_str, &size, NULL, 0);

  int major = 0, minor = 0;
  sscanf(os_version_str, "%d.%d", &major, &minor);

  if ((major == 14 && minor >= 4) || (major >= 15)) {
    thread_terminate(thread);
    error = thread_create_running(task, thread_flavor, (thread_state_t)&machine_thread_state, machine_thread_flavor_count, &thread);
    if (error != KERN_SUCCESS) {
      fprintf(stderr, "could not spawn remote thread: %s\n", mach_error_string(error));
      return 1;
    }
  } else {
    error = thread_set_state(thread, thread_flavor, (thread_state_t)&machine_thread_state, machine_thread_flavor_count);
    if (error != KERN_SUCCESS) {
      fprintf(stderr, "could not set thread state: %s\n", mach_error_string(error));
      return 1;
    }

    error = thread_resume(thread);
    if (error != KERN_SUCCESS) {
      fprintf(stderr, "could not resume remote thread: %s\n", mach_error_string(error));
      return 1;
    }
  }

  usleep(10000);

  for (int i = 0; i < 10; ++i) {
    kern_return_t error = thread_get_state(thread, thread_flavor, (thread_state_t)&thread_state, &thread_flavor_count);

    if (error != KERN_SUCCESS) {
      result = 1;
      goto terminate;
    }

    if (thread_state.__x[0] == 0x79616265) {
      result = 0;
      goto terminate;
    }

    usleep(20000);
  }

terminate:
  error = thread_terminate(thread);
  if (error != KERN_SUCCESS) {
    fprintf(stderr, "failed to terminate remote thread: %s\n", mach_error_string(error));
  }

  if (sandbox_token) {
    free(sandbox_token);
  }

  if (payload_path) {
    free(payload_path);
  }

  return result;
}
