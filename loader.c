/*

MIT License

Copyright (c) 2024 kekeimiku

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.

*/

#include <dlfcn.h>
#include <mach/mach.h>
#include <mach/mach_vm.h>
#include <ptrauth.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/sysctl.h>
#include <unistd.h>

enum sandbox_filter_type {
  SANDBOX_FILTER_NONE,
  SANDBOX_FILTER_PATH,
  SANDBOX_FILTER_GLOBAL_NAME,
  SANDBOX_FILTER_LOCAL_NAME,
  SANDBOX_FILTER_APPLEEVENT_DESTINATION,
  SANDBOX_FILTER_RIGHT_NAME,
  SANDBOX_FILTER_PREFERENCE_DOMAIN,
  SANDBOX_FILTER_KEXT_BUNDLE_ID,
  SANDBOX_FILTER_INFO_TYPE,
  SANDBOX_FILTER_NOTIFICATION,
  SANDBOX_FILTER_DESCRIPTOR,
  SANDBOX_FILTER_FILE_ID,
  SANDBOX_FILTER_XPC_SERVICE_NAME,
  SANDBOX_FILTER_IOKIT_CONNECTION,
  SANDBOX_FILTER_SYSCALL_NUMBER,
};

extern const enum sandbox_filter_type SANDBOX_CHECK_NO_REPORT;

extern const char *const APP_SANDBOX_READ;
extern const char *const APP_SANDBOX_READ_WRITE;
#define APP_SANDBOX_EXECUTABLE "com.apple.sandbox.executable"

int sandbox_check(pid_t, const char *operation, enum sandbox_filter_type, ...);
char *sandbox_extension_issue_file(const char *extension_class, const char *path, uint32_t flags);

extern char __shellcode_start[];
extern char __shellcode_end[];
extern char __patch_pthread_create[];
extern char __patch_sandbox_consume[];
extern char __patch_dlopen[];
extern char __patch_payload_path[];
extern char __patch_sandbox_token_read[];
extern char __patch_sandbox_token_exec[];
extern char __patch_dlerror[];
extern char __patch_error_buffer[];

kern_return_t (*_thread_convert_thread_state)(
    thread_act_t thread, int direction, thread_state_flavor_t flavor,
    thread_state_t in_state, mach_msg_type_number_t in_stateCnt,
    thread_state_t out_state, mach_msg_type_number_t *out_stateCnt);

char *resolvePath(char *pathToResolve) {
  if (strlen(pathToResolve) == 0) return NULL;
  if (pathToResolve[0] == '/') {
    return strdup(pathToResolve);
  } else {
    char absolutePath[PATH_MAX];
    if (realpath(pathToResolve, absolutePath) == NULL) {
      fprintf(stderr, "could not resolve path: %s\n", pathToResolve);
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
  uintptr_t stack_contents = 0x00000000CAFEBABE;

  size_t payload_path_len = strlen(payload_path) + 1;
  kern_return_t kr = KERN_SUCCESS;

  kr = task_for_pid(mach_task_self(), pid, &task);
  if (kr != KERN_SUCCESS) return -1;

  kr = mach_vm_allocate(task, &stack, stack_size, VM_FLAGS_ANYWHERE);
  if (kr != KERN_SUCCESS) return -1;

  kr = mach_vm_protect(task, stack, stack_size, TRUE, VM_PROT_READ | VM_PROT_WRITE);
  if (kr != KERN_SUCCESS) return -1;

  kr = mach_vm_write(task, stack, (vm_address_t)&stack_contents, sizeof(uintptr_t));
  if (kr != KERN_SUCCESS) return -1;

  int sandbox_file_read_data = sandbox_check(pid, "file-read-data", SANDBOX_FILTER_PATH | SANDBOX_CHECK_NO_REPORT, payload_path);
  int sandbox_file_map_executable = sandbox_check(pid, "file-map-executable", SANDBOX_FILTER_PATH | SANDBOX_CHECK_NO_REPORT, payload_path);

  unsigned char *SHELLCODE = (unsigned char *)__shellcode_start;
  const uintptr_t SHELLCODE_SIZE = __shellcode_end - __shellcode_start;
  const uintptr_t PTHREAD_CREATE = __patch_pthread_create - __shellcode_start;
  const uintptr_t SANDBOX_CONSUME = __patch_sandbox_consume - __shellcode_start;
  const uintptr_t DLOPEN = __patch_dlopen - __shellcode_start;
  const uintptr_t PAYLOAD_PATH = __patch_payload_path - __shellcode_start;
  const uintptr_t DLERROR = __patch_dlerror - __shellcode_start;
  const uintptr_t ERROR_BUFFER = __patch_error_buffer - __shellcode_start;

  kr = mach_vm_allocate(task, &code, SHELLCODE_SIZE, VM_FLAGS_ANYWHERE);
  if (kr != KERN_SUCCESS) return -1;

  uintptr_t pcfmt_address = (uintptr_t)ptrauth_strip(dlsym(RTLD_DEFAULT, "pthread_create_from_mach_thread"), ptrauth_key_function_pointer);
  uintptr_t dlopen_address = (uintptr_t)ptrauth_strip(dlsym(RTLD_DEFAULT, "dlopen"), ptrauth_key_function_pointer);
  uintptr_t dlerror_address = (uintptr_t)ptrauth_strip(dlsym(RTLD_DEFAULT, "dlerror"), ptrauth_key_function_pointer);

  memcpy(SHELLCODE + PTHREAD_CREATE, &pcfmt_address, sizeof(uintptr_t));
  memcpy(SHELLCODE + DLOPEN, &dlopen_address, sizeof(uintptr_t));
  memcpy(SHELLCODE + DLERROR, &dlerror_address, sizeof(uintptr_t));

  mach_vm_address_t error_buffer_address = 0;
  kr = mach_vm_allocate(task, &error_buffer_address, sizeof(uintptr_t), VM_FLAGS_ANYWHERE);
  if (kr != KERN_SUCCESS) return -1;
  kr = mach_vm_protect(task, error_buffer_address, sizeof(uintptr_t), TRUE, VM_PROT_READ | VM_PROT_WRITE);
  if (kr != KERN_SUCCESS) return -1;

  memcpy(SHELLCODE + ERROR_BUFFER, &error_buffer_address, sizeof(uintptr_t));

  mach_vm_address_t payload_path_address = 0;
  kr = mach_vm_allocate(task, &payload_path_address, payload_path_len, VM_FLAGS_ANYWHERE);
  if (kr != KERN_SUCCESS) return -1;
  kr = mach_vm_protect(task, payload_path_address, payload_path_len, TRUE, VM_PROT_READ | VM_PROT_WRITE);
  if (kr != KERN_SUCCESS) return -1;
  kr = mach_vm_write(task, payload_path_address, (vm_address_t)payload_path, payload_path_len);
  if (kr != KERN_SUCCESS) return -1;

  memcpy(SHELLCODE + PAYLOAD_PATH, &payload_path_address, sizeof(uintptr_t));

  if (sandbox_file_read_data || sandbox_file_map_executable) {
    uintptr_t sandbox_consume_address = (uintptr_t)ptrauth_strip(dlsym(RTLD_DEFAULT, "sandbox_extension_consume"), ptrauth_key_function_pointer);
    memcpy(SHELLCODE + SANDBOX_CONSUME, &sandbox_consume_address, sizeof(uintptr_t));
  }

  if (sandbox_file_read_data) {
    char *token = sandbox_extension_issue_file(APP_SANDBOX_READ, payload_path, 0);
    if (!token) return -1;

    mach_vm_address_t token_address = 0;
    size_t token_len = strlen(token) + 1;
    kr = mach_vm_allocate(task, &token_address, token_len, VM_FLAGS_ANYWHERE);
    if (kr != KERN_SUCCESS) return -1;
    kr = mach_vm_protect(task, token_address, token_len, TRUE, VM_PROT_READ | VM_PROT_WRITE);
    if (kr != KERN_SUCCESS) return -1;
    kr = mach_vm_write(task, token_address, (vm_address_t)token, token_len);
    if (kr != KERN_SUCCESS) return -1;

    const uintptr_t SANDBOX_TOKEN_READ = __patch_sandbox_token_read - __shellcode_start;
    memcpy(SHELLCODE + SANDBOX_TOKEN_READ, &token_address, sizeof(uintptr_t));
  }

  if (sandbox_file_map_executable) {
    char *token = sandbox_extension_issue_file(APP_SANDBOX_EXECUTABLE, payload_path, 0);
    if (!token) return -1;

    mach_vm_address_t token_address = 0;
    size_t token_len = strlen(token) + 1;
    kr = mach_vm_allocate(task, &token_address, token_len, VM_FLAGS_ANYWHERE);
    if (kr != KERN_SUCCESS) return -1;
    kr = mach_vm_protect(task, token_address, token_len, TRUE, VM_PROT_READ | VM_PROT_WRITE);
    if (kr != KERN_SUCCESS) return -1;
    kr = mach_vm_write(task, token_address, (vm_address_t)token, token_len);
    if (kr != KERN_SUCCESS) return -1;

    const uintptr_t SANDBOX_TOKEN_EXEC = __patch_sandbox_token_exec - __shellcode_start;
    memcpy(SHELLCODE + SANDBOX_TOKEN_EXEC, &token_address, sizeof(uintptr_t));
  }

  kr = mach_vm_write(task, code, (vm_address_t)SHELLCODE, SHELLCODE_SIZE);
  if (kr != KERN_SUCCESS) return -1;
  kr = mach_vm_protect(task, code, SHELLCODE_SIZE, FALSE, VM_PROT_EXECUTE | VM_PROT_READ);
  if (kr != KERN_SUCCESS) return -1;

  void *handle = dlopen("/usr/lib/system/libsystem_kernel.dylib", RTLD_GLOBAL | RTLD_LAZY);
  if (handle) {
    _thread_convert_thread_state = dlsym(handle, "thread_convert_thread_state");
    dlclose(handle);
  }

  if (!_thread_convert_thread_state) return -1;

  arm_thread_state64_t thread_state = {}, machine_thread_state = {};
  thread_state_flavor_t thread_flavor = ARM_THREAD_STATE64;
  mach_msg_type_number_t thread_flavor_count = ARM_THREAD_STATE64_COUNT, machine_thread_flavor_count = ARM_THREAD_STATE64_COUNT;

  __darwin_arm_thread_state64_set_pc_fptr(thread_state, ptrauth_sign_unauthenticated((void *)code, ptrauth_key_asia, 0));
  __darwin_arm_thread_state64_set_sp(thread_state, stack + (stack_size / 2));

  kr = thread_create(task, &thread);
  if (kr != KERN_SUCCESS) return -1;

  kr = _thread_convert_thread_state(thread, 2, thread_flavor, (thread_state_t)&thread_state, thread_flavor_count, (thread_state_t)&machine_thread_state, &machine_thread_flavor_count);
  if (kr != KERN_SUCCESS) return -1;

  char os_version_str[32];
  size_t size = sizeof(os_version_str);
  sysctlbyname("kern.osproductversion", os_version_str, &size, NULL, 0);

  int major = 0, minor = 0;
  sscanf(os_version_str, "%d.%d", &major, &minor);

  if ((major == 14 && minor >= 4) || (major >= 15)) {
    thread_terminate(thread);
    kr = thread_create_running(task, thread_flavor, (thread_state_t)&machine_thread_state, machine_thread_flavor_count, &thread);
    if (kr != KERN_SUCCESS) return -1;
  } else {
    kr = thread_set_state(thread, thread_flavor, (thread_state_t)&machine_thread_state, machine_thread_flavor_count);
    if (kr != KERN_SUCCESS) return -1;

    kr = thread_resume(thread);
    if (kr != KERN_SUCCESS) return -1;
  }

  usleep(10000);

  for (int i = 0; i < 10; ++i) {
    kr = thread_get_state(thread, thread_flavor, (thread_state_t)&thread_state, &thread_flavor_count);

    if (kr != KERN_SUCCESS) {
      result = -1;
      goto terminate;
    }

    if (thread_state.__x[0] == 0x444f4e45) {
      result = 0;

      uintptr_t error_buffer[2] = { 0 };
      size_t maxSize = sizeof(error_buffer);
      kr = vm_read_overwrite(task, error_buffer_address, sizeof(error_buffer), (vm_address_t)error_buffer, &maxSize);
      if (kr != KERN_SUCCESS) {
        fprintf(stderr, "failed to read error buffer: %s\n", mach_error_string(kr));
        result = -1;
        goto terminate;
      }

      if (!error_buffer[0]) {
        size_t len = 0;
        char c;
        do {
          size_t sz = sizeof(c);
          kr = vm_read_overwrite(task, error_buffer[1] + len++, sizeof(c), (vm_address_t)&c, &sz);
          if (kr != KERN_SUCCESS) break;
        } while (c != '\0');

        char *error_msg = malloc(len);
        size_t sz = len;
        kr = vm_read_overwrite(task, error_buffer[1], len, (vm_address_t)error_msg, &sz);
        if (kr == KERN_SUCCESS) {
          fprintf(stderr, "remote dlerror: %s\n", error_msg);
        }
        free(error_msg);
      }
      goto terminate;
    }

    usleep(20000);
  }

terminate:
  kr = thread_terminate(thread);
  if (kr != KERN_SUCCESS) return -1;

  return result;
}
