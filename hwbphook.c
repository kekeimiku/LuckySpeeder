// Modified by kekeimiku
// Modified by Euclid Jan G.
/* Original owner: Saagar Jha */
#include "hwbphook.h"
#include "mach_excServer.h"
#include <mach/mach.h>
#include <pthread.h>
#include <stdlib.h>
#include <sys/sysctl.h>

kern_return_t catch_mach_exception_raise(
    mach_port_t exception_port,
    mach_port_t thread,
    mach_port_t task,
    exception_type_t exception,
    mach_exception_data_t code,
    mach_msg_type_number_t codeCnt) {
  abort(); // only calls if not hooked
}

kern_return_t catch_mach_exception_raise_state_identity(
    mach_port_t exception_port,
    mach_port_t thread,
    mach_port_t task,
    exception_type_t exception,
    mach_exception_data_t code,
    mach_msg_type_number_t codeCnt,
    int *flavor,
    thread_state_t old_state,
    mach_msg_type_number_t old_stateCnt,
    thread_state_t new_state,
    mach_msg_type_number_t *new_stateCnt) {
  abort(); // will call only if not hooked
}

// i guess you can change those
// two abort calls to KERN_FAILURE (.mm)
// just in case. not needed though...

mach_port_t server;
static mach_port_t orig_handler_port = MACH_PORT_NULL; // for debuggers

struct hook {
  uintptr_t old;
  uintptr_t new;
};
static struct hook hooks[16];
static int active_hooks;

kern_return_t catch_mach_exception_raise_state(
    mach_port_t exception_port,
    exception_type_t exception,
    const mach_exception_data_t code,
    mach_msg_type_number_t codeCnt,
    int *flavor,
    const thread_state_t old_state,
    mach_msg_type_number_t old_stateCnt,
    thread_state_t new_state,
    mach_msg_type_number_t *new_stateCnt) {

  arm_thread_state64_t *old = (arm_thread_state64_t *)old_state;
  arm_thread_state64_t *new = (arm_thread_state64_t *)new_state;

  for (int i = 0; i < active_hooks; ++i) {
    if (hooks[i].old == arm_thread_state64_get_pc(*old)) { // this checks if
      *new = *old;                                         // brk is from hook
      *new_stateCnt = old_stateCnt;                        // or not
      arm_thread_state64_set_pc_fptr(*new, hooks[i].new);
      return KERN_SUCCESS;
    }
  }

  // forwards the applicable events to lldb or whatever debugger there is
  if (orig_handler_port != MACH_PORT_NULL) { // will not be null is debugger attached
    return mach_msg_server(mach_exc_server, sizeof(union __RequestUnion__catch_mach_exc_subsystem), orig_handler_port, MACH_MSG_OPTION_NONE);
  }

  return KERN_FAILURE;
}

// custom exception handler which tells the cpu to redirect execution
void *exception_handler(void *unused) {
  while (1) {
    mach_msg_server(mach_exc_server, sizeof(union __RequestUnion__catch_mach_exc_subsystem), server, MACH_MSG_OPTION_NONE);
  }
  return NULL;
}

bool hwbp_hook(void *old[], void *new[], int count) {
  if (count > 6) return false; // max 6

  static bool initialized;
  static bool thread_initialized = false;
  static int breakpoints;

  if (!initialized) {
    size_t size = sizeof(breakpoints);
    sysctlbyname("hw.optional.breakpoint", &breakpoints, &size, NULL, 0); // usually 6

    // save existing exception port (LLDB's/debugger's)
    mach_port_t current_ports[EXC_TYPES_COUNT];
    mach_msg_type_number_t port_count = EXC_TYPES_COUNT;
    exception_mask_t masks[EXC_TYPES_COUNT];
    exception_behavior_t behaviors[EXC_TYPES_COUNT];
    thread_state_flavor_t flavors[EXC_TYPES_COUNT];

    if (task_get_exception_ports(mach_task_self(), EXC_MASK_BREAKPOINT, masks, &port_count, current_ports, behaviors, flavors) == KERN_SUCCESS) {
      if (port_count > 0) orig_handler_port = current_ports[0];
    }

    // allocs a new exception port
    mach_port_allocate(mach_task_self(), MACH_PORT_RIGHT_RECEIVE, &server);
    mach_port_insert_right(mach_task_self(), server, server, MACH_MSG_TYPE_MAKE_SEND);
    task_set_exception_ports(mach_task_self(), EXC_MASK_BREAKPOINT, server, EXCEPTION_STATE | MACH_EXCEPTION_CODES, ARM_THREAD_STATE64);

    if (!thread_initialized) {
      pthread_t thread;
      pthread_create(&thread, NULL, exception_handler, NULL);
      thread_initialized = true;
    }

    initialized = true;
  }

  if (count > breakpoints) return false; // max are the available HW breakpoints ( 6 by default )

  arm_debug_state64_t state = {};
  for (int i = 0; i < count; i++) {
    state.__bvr[i] = (uintptr_t)old[i]; // set
    state.__bcr[i] = 0x1e5;             // enable
    hooks[active_hooks] = (struct hook){(uintptr_t)old[i], (uintptr_t) new[i]};
    active_hooks++;
  }

  if (task_set_state(mach_task_self(), ARM_DEBUG_STATE64, (thread_state_t)&state, ARM_DEBUG_STATE64_COUNT) != KERN_SUCCESS) return false;

  thread_act_array_t threads;
  mach_msg_type_number_t thread_count = 0;
  task_threads(mach_task_self(), &threads, &thread_count);

  bool success = true;
  for (int i = 0; i < thread_count; ++i) {
    if (thread_set_state(threads[i], ARM_DEBUG_STATE64, (thread_state_t)&state, ARM_DEBUG_STATE64_COUNT) != KERN_SUCCESS) success = false;
  }

  for (int i = 0; i < thread_count; ++i) mach_port_deallocate(mach_task_self(), threads[i]);
  vm_deallocate(mach_task_self(), (vm_address_t)threads, thread_count * sizeof(*threads));

  return success;
}

bool hwbp_unhook(void *old[], int count) {
  arm_debug_state64_t state = {};

  for (int i = 0; i < count; i++) {
    state.__bvr[i] = 0;
    state.__bcr[i] = 0;

    // so eux, you set this to 0. that'd mess with total count.
    // If i set 5 breakpoints and unhook 2 of them, all would become 'inactive'
    // because this makes the global active hooks 0.
    // so, it should be decrementing for every successful call. :)
    for (int j = 0; j < active_hooks; j++) {
      if (hooks[j].old == (uintptr_t)old[i]) {
        hooks[j] = hooks[active_hooks - 1]; // we move the last hook into a current slot
        active_hooks--;
        break;
      }
    }
  }

  thread_act_array_t threads;
  mach_msg_type_number_t thread_count = 0;
  task_threads(mach_task_self(), &threads, &thread_count);

  bool success = true;
  for (int i = 0; i < thread_count; ++i) {
    if (thread_set_state(threads[i], ARM_DEBUG_STATE64, (thread_state_t)&state, ARM_DEBUG_STATE64_COUNT) != KERN_SUCCESS) {
      success = false;
    }
  }

  for (int i = 0; i < thread_count; ++i) mach_port_deallocate(mach_task_self(), threads[i]);
  vm_deallocate(mach_task_self(), (vm_address_t)threads, thread_count * sizeof(*threads));

  return success;
}
