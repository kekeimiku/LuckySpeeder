//
//  hook.c
//  iSH
//
//  Created by Saagar Jha on 12/29/22.
//

#include "hwbphook.h"
#include "mach_excServer.h"
#include <mach-o/dyld_images.h>
#include <pthread.h>
#include <stdlib.h>
#include <sys/sysctl.h>

kern_return_t catch_mach_exception_raise(mach_port_t exception_port,
                                         mach_port_t thread, mach_port_t task,
                                         exception_type_t exception,
                                         mach_exception_data_t code,
                                         mach_msg_type_number_t codeCnt) {
  abort();
}

kern_return_t catch_mach_exception_raise_state_identity(
    mach_port_t exception_port, mach_port_t thread, mach_port_t task,
    exception_type_t exception, mach_exception_data_t code,
    mach_msg_type_number_t codeCnt, int *flavor, thread_state_t old_state,
    mach_msg_type_number_t old_stateCnt, thread_state_t new_state,
    mach_msg_type_number_t *new_stateCnt) {
  abort();
}

static bool initialized;

struct hook {
  uintptr_t old;
  uintptr_t new;
};
static struct hook hooks[16];
static int active_hooks;
static int breakpoints;

mach_port_t server;

kern_return_t catch_mach_exception_raise_state(
    mach_port_t exception_port, exception_type_t exception,
    const mach_exception_data_t code, mach_msg_type_number_t codeCnt,
    int *flavor, const thread_state_t old_state,
    mach_msg_type_number_t old_stateCnt, thread_state_t new_state,
    mach_msg_type_number_t *new_stateCnt) {
  arm_thread_state64_t *old = (arm_thread_state64_t *)old_state;
  arm_thread_state64_t *new = (arm_thread_state64_t *)new_state;

  for (int i = 0; i < active_hooks; ++i) {
    if (hooks[i].old == arm_thread_state64_get_pc(*old)) {
      *new = *old;
      *new_stateCnt = old_stateCnt;
      arm_thread_state64_set_pc_fptr(*new, hooks[i].new);
      return KERN_SUCCESS;
    }
  }

  return KERN_FAILURE;
}

void *exception_handler(void *unused) {
  mach_msg_server(mach_exc_server,
                  sizeof(union __RequestUnion__catch_mach_exc_subsystem),
                  server, MACH_MSG_OPTION_NONE);
  abort();
}

static bool initialize_if_needed(void) {
  if (initialized) {
    return true;
  }

#define CHECK(x)                                                               \
  do {                                                                         \
    if (!(x)) {                                                                \
      return false;                                                            \
    }                                                                          \
  } while (0)

  size_t size = sizeof(breakpoints);
  CHECK(!sysctlbyname("hw.optional.breakpoint", &breakpoints, &size, NULL, 0));

  CHECK(mach_port_allocate(mach_task_self(), MACH_PORT_RIGHT_RECEIVE,
                           &server) == KERN_SUCCESS);
  CHECK(mach_port_insert_right(mach_task_self(), server, server,
                               MACH_MSG_TYPE_MAKE_SEND) == KERN_SUCCESS);

  // This will break any connected debuggers. Unfortunately the workarounds
  // for this are not very good, so we're not going to bother with them.
  CHECK(task_set_exception_ports(mach_task_self(), EXC_MASK_BREAKPOINT, server,
                                 EXCEPTION_STATE | MACH_EXCEPTION_CODES,
                                 ARM_THREAD_STATE64) == KERN_SUCCESS);

  pthread_t thread;
  CHECK(!pthread_create(&thread, NULL, exception_handler, NULL));

#undef CHECK

  return initialized = true;
}

bool hwbp_rebind(void *old, void *new) {
  initialize_if_needed();

#define CHECK(x)                                                               \
  do {                                                                         \
    if (!(x)) {                                                                \
      return false;                                                            \
    }                                                                          \
  } while (0)

  CHECK(active_hooks < breakpoints);

  arm_debug_state64_t state = {};
  state.__bvr[active_hooks] = (uintptr_t)old;
  // DBGBCR_EL1
  //  .BT = 0b0000 << 20 (unlinked address match)
  //  .BAS = 0xF << 5 (A64)
  //  .PMC = 0b10 << 1 (user)
  //  .E = 0b1 << 0 (enable)
  state.__bcr[active_hooks] = 0x1e5;

  CHECK(task_set_state(mach_task_self(), ARM_DEBUG_STATE64,
                       (thread_state_t)&state,
                       ARM_DEBUG_STATE64_COUNT) == KERN_SUCCESS);

#undef CHECK

  bool success = true;

  thread_act_array_t threads;
  mach_msg_type_number_t thread_count = ARM_DEBUG_STATE64_COUNT;
  task_threads(mach_task_self(), &threads, &thread_count);
  for (int i = 0; i < thread_count; ++i) {
    if (thread_set_state(threads[i], ARM_DEBUG_STATE64, (thread_state_t)&state,
                         ARM_DEBUG_STATE64_COUNT) != KERN_SUCCESS) {
      success = false;
      goto done;
    }
  }

  hooks[active_hooks++] = (struct hook){(uintptr_t)old, (uintptr_t) new};

done:
  for (int i = 0; i < thread_count; ++i) {
    mach_port_deallocate(mach_task_self(), threads[i]);
  }
  vm_deallocate(mach_task_self(), (vm_address_t)threads,
                thread_count * sizeof(*threads));

  return success;
}
