/*

MIT License

Copyright (c) 2024 kekeimiku
Copyright (c) 2024 ac0d3r

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

#include "LuckySpeeder.h"
#include "fishhook.h"
#include <mach-o/dyld.h>
#include <mach-o/getsect.h>
#include <os/lock.h>
#include <string.h>
#include <sys/time.h>
#include <unistd.h>

#if !TARGET_OS_TV
#include "hwbphook.h"
#include "port_clock_gettime.h"
#include <dlfcn.h>
#endif

static float timeScale_speed = 1.0;

static void (*original_timeScale)(float) = NULL;

static void my_timeScale(void) {
  original_timeScale(timeScale_speed);
}

int hook_timeScale(void) {
  if (original_timeScale) return 0;

  intptr_t unity_vmaddr_slide = 0;
  uint32_t image_count = _dyld_image_count();
  const char *image_name;
  for (uint32_t i = 0; i < image_count; ++i) {
    image_name = _dyld_get_image_name(i);
    if (strstr(image_name, "UnityFramework.framework/UnityFramework")) {
      unity_vmaddr_slide = _dyld_get_image_vmaddr_slide(i);
      break;
    }
  }
  if (!unity_vmaddr_slide) return -1;

  size_t size;

  uint8_t *cstring_section_data = getsectiondata((const struct mach_header_64 *)unity_vmaddr_slide, "__TEXT", "__cstring", &size);
  if (!cstring_section_data) return -1;

  uint8_t *time_scale_function_address = memmem(cstring_section_data, size, "UnityEngine.Time::set_timeScale(System.Single)", 0x2F);
  if (!time_scale_function_address) return -1;

  uintptr_t il2cpp_section_base = 0;
  il2cpp_section_base = (uintptr_t)getsectiondata((const struct mach_header_64 *)unity_vmaddr_slide, "__TEXT", "il2cpp", &size);
  if (!il2cpp_section_base) {
    il2cpp_section_base = (uintptr_t)getsectiondata((const struct mach_header_64 *)unity_vmaddr_slide, "__TEXT", "__text", &size);
    if (!il2cpp_section_base) return -1;
  };

  uint8_t *il2cpp_end = (uint8_t *)(size + il2cpp_section_base);
  if (il2cpp_section_base + 4 >= size + il2cpp_section_base) return -1;

  uintptr_t first_instruction = *(uint32_t *)il2cpp_section_base;
  uintptr_t resolved_address, function_offset, second_instruction;

  while (1) {
    second_instruction = *(uint32_t *)(il2cpp_section_base + 4);
    if ((first_instruction & 0x9F000000) == 0x90000000 && (second_instruction & 0xFF800000) == 0x91000000) {
      resolved_address = (il2cpp_section_base & 0xFFFFFFFFFFFFF000LL) + (uint32_t)((((first_instruction >> 3) & 0xFFFFFFFC) | ((first_instruction >> 29) & 3)) << 12);
      function_offset = (second_instruction >> 10) & 0xFFF;
      if ((second_instruction & 0xC00000) != 0) function_offset <<= 12;
      if ((uint8_t *)(resolved_address + function_offset) == time_scale_function_address) break;
    }
    il2cpp_section_base += 4;
    first_instruction = second_instruction;
    if ((uint8_t *)(il2cpp_section_base + 8) >= il2cpp_end) return -1;
  }

  uintptr_t current_address = il2cpp_section_base;
  uintptr_t current_instruction, code_section_address;

  do {
    current_instruction = *(uint32_t *)(current_address - 4);
    current_address -= 4;
  } while ((current_instruction & 0x9F000000) != 0x90000000);

  code_section_address = (current_address & 0xFFFFFFFFFFFFF000LL) + (uint32_t)((((current_instruction >> 3) & 0xFFFFFFFC) | ((current_instruction >> 29) & 3)) << 12);

  uintptr_t method_data = *(uint32_t *)(current_address + 4);
  uintptr_t function_data_offset;

  if ((method_data & 0x1000000) != 0)
    function_data_offset = 8 * ((method_data >> 10) & 0xFFF);
  else
    function_data_offset = (method_data >> 10) & 0xFFF;

  if (*(uintptr_t *)(code_section_address + function_data_offset)) {
    original_timeScale = *(void (**)(float))(function_data_offset + code_section_address);
  } else {
    uint32_t instruction_operand = *(uint32_t *)(il2cpp_section_base + 8);
    uint8_t *code_section_start = (uint8_t *)(il2cpp_section_base + 8);
    uintptr_t instruction_offset = (4 * instruction_operand) & 0xFFFFFFC;
    uintptr_t address_offset = (4 * (instruction_operand & 0x3FFFFFF)) | 0xFFFFFFFFF0000000LL;

    if (((4 * instruction_operand) & 0x8000000) != 0)
      function_offset = address_offset;
    else
      function_offset = instruction_offset;

    original_timeScale = (void (*)(float))((uintptr_t (*)(void *))&code_section_start[function_offset])(time_scale_function_address);
  }

  if (original_timeScale) {
    *(uintptr_t *)(function_data_offset + code_section_address) = (uintptr_t)my_timeScale;
    return 0;
  }

  return -1;
}

void set_timeScale(float value) {
  if (!original_timeScale) return;
  timeScale_speed = value;
  my_timeScale();
}

void reset_timeScale(void) { set_timeScale(1.0); }

static float gettimeofday_speed = 1.0;

#define USec_Scale (1000000LL)
static time_t gettimeofday_pre_sec = 0;
static suseconds_t gettimeofday_pre_usec = 0;
static time_t gettimeofday_true_pre_sec = 0;
static suseconds_t gettimeofday_true_pre_usec = 0;
static os_unfair_lock gettimeofday_lock = OS_UNFAIR_LOCK_INIT;

static int (*original_gettimeofday)(struct timeval *, void *) = NULL;

// my_gettimeofday fix from AccDemo
static int my_gettimeofday(struct timeval *tv, struct timezone *tz) {
  os_unfair_lock_lock(&gettimeofday_lock);
  int ret = original_gettimeofday(tv, tz);
  if (!ret) {
    if (!gettimeofday_pre_sec) {
      gettimeofday_pre_sec = tv->tv_sec;
      gettimeofday_true_pre_sec = tv->tv_sec;
      gettimeofday_pre_usec = tv->tv_usec;
      gettimeofday_true_pre_usec = tv->tv_usec;
    } else {
      int64_t true_curSec = tv->tv_sec * USec_Scale + tv->tv_usec;
      int64_t true_preSec = gettimeofday_true_pre_sec * USec_Scale + gettimeofday_true_pre_usec;
      int64_t invl = true_curSec - true_preSec;
      invl *= gettimeofday_speed;

      int64_t curSec = gettimeofday_pre_sec * USec_Scale + gettimeofday_pre_usec;
      curSec += invl;

      time_t used_sec = curSec / USec_Scale;
      suseconds_t used_usec = curSec % USec_Scale;

      gettimeofday_true_pre_sec = tv->tv_sec;
      gettimeofday_true_pre_usec = tv->tv_usec;
      tv->tv_sec = used_sec;
      tv->tv_usec = used_usec;
      gettimeofday_pre_sec = used_sec;
      gettimeofday_pre_usec = used_usec;
    }
  }
  os_unfair_lock_unlock(&gettimeofday_lock);
  return ret;
}

int hook_gettimeofday(void) {
  if (original_gettimeofday) return 0;

  struct rebinding rebindings = {"gettimeofday", my_gettimeofday, (void *)&original_gettimeofday};
  return rebind_symbols(&rebindings, 1);
}

void set_gettimeofday(float value) {
  os_unfair_lock_lock(&gettimeofday_lock);
  gettimeofday_speed = value;
  os_unfair_lock_unlock(&gettimeofday_lock);
}

void reset_gettimeofday(void) { set_gettimeofday(1.0); }

static float clock_gettime_speed = 1.0;

#define NSec_Scale (1000000000LL)
static time_t clock_gettime_pre_sec = 0;
static long clock_gettime_pre_nsec = 0;
static time_t clock_gettime_true_pre_sec = 0;
static long clock_gettime_true_pre_nsec = 0;
static os_unfair_lock clock_gettime_lock = OS_UNFAIR_LOCK_INIT;

static int (*original_clock_gettime)(clockid_t clock_id, struct timespec *tp) = NULL;

// my_clock_gettime fix from AccDemo
static int my_clock_gettime(clockid_t clk_id, struct timespec *tp) {
  os_unfair_lock_lock(&clock_gettime_lock);
#if TARGET_OS_TV
  int ret = original_clock_gettime(clk_id, tp);
#else
  int ret = port_clock_gettime(clk_id, tp);
#endif
  if (!ret) {
    if (!clock_gettime_pre_sec) {
      clock_gettime_pre_sec = tp->tv_sec;
      clock_gettime_true_pre_sec = tp->tv_sec;
      clock_gettime_pre_nsec = tp->tv_nsec;
      clock_gettime_true_pre_nsec = tp->tv_nsec;
    } else {
      int64_t true_curSec = tp->tv_sec * NSec_Scale + tp->tv_nsec;
      int64_t true_preSec = clock_gettime_true_pre_sec * NSec_Scale + clock_gettime_true_pre_nsec;
      int64_t invl = true_curSec - true_preSec;
      invl *= clock_gettime_speed;

      int64_t curSec = clock_gettime_pre_sec * NSec_Scale + clock_gettime_pre_nsec;
      curSec += invl;

      time_t used_sec = curSec / NSec_Scale;
      long used_nsec = curSec % NSec_Scale;

      clock_gettime_true_pre_sec = tp->tv_sec;
      clock_gettime_true_pre_nsec = tp->tv_nsec;
      tp->tv_sec = used_sec;
      tp->tv_nsec = used_nsec;
      clock_gettime_pre_sec = used_sec;
      clock_gettime_pre_nsec = used_nsec;
    }
  }
  os_unfair_lock_unlock(&clock_gettime_lock);
  return ret;
}

#if TARGET_OS_TV
int hook_clock_gettime(void) {
  if (original_clock_gettime) return 0;

  struct rebinding rebindings = {"clock_gettime", my_clock_gettime, (void *)&original_clock_gettime};
  return rebind_symbols(&rebindings, 1);
}
#else
int hook_clock_gettime(void) {
  if (original_clock_gettime) return 0;

  original_clock_gettime = dlsym(RTLD_DEFAULT, "clock_gettime");
  if (!original_clock_gettime) return -1;

  void *original[] = {(void *)original_clock_gettime};
  void *hooked[] = {(void *)my_clock_gettime};
  bool success = hwbp_hook(original, hooked, 1);

  if (!success) return -1;

  return 0;
}
#endif

void set_clock_gettime(float value) {
  os_unfair_lock_lock(&clock_gettime_lock);
  clock_gettime_speed = value;
  os_unfair_lock_unlock(&clock_gettime_lock);
}

#if TARGET_OS_TV
void reset_clock_gettime(void) { set_clock_gettime(1.0); }
#else
void reset_clock_gettime(void) {
  if (!original_clock_gettime) return;

  void *original[] = {(void *)original_clock_gettime};
  hwbp_unhook(original, 1);
  set_clock_gettime(1.0);
  original_clock_gettime = NULL;
}
#endif

static float mach_absolute_time_speed = 1.0;

static uint64_t mach_absolute_base_time = 0;
static uint64_t mach_absolute_start_time = 0;
static uint64_t mach_absolute_last_time = 0;
static os_unfair_lock mach_absolute_base_time_lock = OS_UNFAIR_LOCK_INIT;

static uint64_t (*original_mach_absolute_time)(void) = NULL;

static uint64_t my_mach_absolute_time(void) {
  os_unfair_lock_lock(&mach_absolute_base_time_lock);
  uint64_t current_time = original_mach_absolute_time();
  uint64_t result;

  if (mach_absolute_last_time) {
    uint64_t delta = current_time - mach_absolute_base_time;
    result = mach_absolute_start_time + (uint64_t)(delta * mach_absolute_time_speed);
    if (result <= mach_absolute_last_time) {
      mach_absolute_base_time = current_time;
      mach_absolute_start_time = mach_absolute_last_time + 1;
      result = mach_absolute_start_time;
    }
  } else {
    mach_absolute_base_time = current_time;
    mach_absolute_start_time = current_time;
    result = current_time;
  }

  mach_absolute_last_time = result;
  os_unfair_lock_unlock(&mach_absolute_base_time_lock);
  return result;
}

int hook_mach_absolute_time(void) {
  if (original_mach_absolute_time) return 0;

  struct rebinding rebindings = {"mach_absolute_time", my_mach_absolute_time, (void *)&original_mach_absolute_time};
  return rebind_symbols(&rebindings, 1);
}

void set_mach_absolute_time(float value) {
  os_unfair_lock_lock(&mach_absolute_base_time_lock);
  uint64_t current_time = original_mach_absolute_time();
  mach_absolute_base_time = current_time;
  mach_absolute_start_time = mach_absolute_last_time;
  mach_absolute_time_speed = value;
  os_unfair_lock_unlock(&mach_absolute_base_time_lock);
}

void reset_mach_absolute_time(void) { set_mach_absolute_time(1.0); }

static float sleep_speed = 1.0;
static os_unfair_lock sleep_lock = OS_UNFAIR_LOCK_INIT;

static unsigned int (*original_sleep)(unsigned int seconds) = NULL;

static unsigned int my_sleep(unsigned int seconds) {
  os_unfair_lock_lock(&sleep_lock);
  float current_speed = sleep_speed;
  os_unfair_lock_unlock(&sleep_lock);

  unsigned int adjusted_seconds = (unsigned int)(seconds * current_speed);
  return original_sleep(adjusted_seconds);
}

int hook_sleep(void) {
  if (original_sleep) return 0;

  struct rebinding rebindings = {"sleep", my_sleep, (void *)&original_sleep};
  return rebind_symbols(&rebindings, 1);
}

void set_sleep(float value) {
  os_unfair_lock_lock(&sleep_lock);
  sleep_speed = value;
  os_unfair_lock_unlock(&sleep_lock);
}

void reset_sleep(void) { set_sleep(1.0); }

static float usleep_speed = 1.0;
static os_unfair_lock usleep_lock = OS_UNFAIR_LOCK_INIT;

static int (*original_usleep)(useconds_t usec) = NULL;

static int my_usleep(useconds_t usec) {
  os_unfair_lock_lock(&usleep_lock);
  float current_speed = usleep_speed;
  os_unfair_lock_unlock(&usleep_lock);

  useconds_t adjusted_usec = (useconds_t)(usec * current_speed);
  return original_usleep(adjusted_usec);
}

int hook_usleep(void) {
  if (original_usleep) return 0;

  struct rebinding rebindings = {"usleep", my_usleep, (void *)&original_usleep};
  return rebind_symbols(&rebindings, 1);
}

void set_usleep(float value) {
  os_unfair_lock_lock(&usleep_lock);
  usleep_speed = value;
  os_unfair_lock_unlock(&usleep_lock);
}

void reset_usleep(void) { set_usleep(1.0); }

static float nanosleep_speed = 1.0;
static os_unfair_lock nanosleep_lock = OS_UNFAIR_LOCK_INIT;

static int (*original_nanosleep)(const struct timespec *req, struct timespec *rem) = NULL;

static int my_nanosleep(const struct timespec *req, struct timespec *rem) {
  os_unfair_lock_lock(&nanosleep_lock);
  float current_speed = nanosleep_speed;
  os_unfair_lock_unlock(&nanosleep_lock);

  struct timespec adjusted_req;
  int64_t total_nsec = req->tv_sec * NSec_Scale + req->tv_nsec;
  int64_t adjusted_total_nsec = (int64_t)(total_nsec * current_speed);

  adjusted_req.tv_sec = adjusted_total_nsec / NSec_Scale;
  adjusted_req.tv_nsec = adjusted_total_nsec % NSec_Scale;

  int result = original_nanosleep(&adjusted_req, rem);

  if (result != 0 && rem != NULL) {
    int64_t rem_nsec = rem->tv_sec * NSec_Scale + rem->tv_nsec;
    int64_t original_rem_nsec = (int64_t)(rem_nsec / current_speed);
    rem->tv_sec = original_rem_nsec / NSec_Scale;
    rem->tv_nsec = original_rem_nsec % NSec_Scale;
  }

  return result;
}

int hook_nanosleep(void) {
  if (original_nanosleep) return 0;

  struct rebinding rebindings = {"nanosleep", my_nanosleep, (void *)&original_nanosleep};
  return rebind_symbols(&rebindings, 1);
}

void set_nanosleep(float value) {
  os_unfair_lock_lock(&nanosleep_lock);
  nanosleep_speed = value;
  os_unfair_lock_unlock(&nanosleep_lock);
}

void reset_nanosleep(void) { set_nanosleep(1.0); }

static float Godot_Engine_speed = 1.0;
static int original_physics_ticks = 60;
static int original_max_fps = 0;
static os_unfair_lock Godot_Engine_lock = OS_UNFAIR_LOCK_INIT;

static void (*Engine_set_time_scale)(void *, double) = NULL;
static void (*Engine_set_physics_ticks_per_second)(void *, int) = NULL;
static int (*Engine_get_physics_ticks_per_second)(void *) = NULL;
static void (*Engine_set_max_fps)(void *, int) = NULL;
static int (*Engine_get_max_fps)(void *) = NULL;
static void *(*Engine_get_singleton)(void) = NULL;

int hook_Godot_Engine(void) {
  if (Engine_set_time_scale) return 0;

  Engine_set_time_scale = dlsym(RTLD_DEFAULT, "_ZN6Engine14set_time_scaleEd");
  Engine_set_physics_ticks_per_second = dlsym(RTLD_DEFAULT, "_ZN6Engine28set_physics_ticks_per_secondEi");
  Engine_get_physics_ticks_per_second = dlsym(RTLD_DEFAULT, "_ZNK6Engine28get_physics_ticks_per_secondEv");
  Engine_set_max_fps = dlsym(RTLD_DEFAULT, "_ZN6Engine11set_max_fpsEi");
  Engine_get_max_fps = dlsym(RTLD_DEFAULT, "_ZNK6Engine11get_max_fpsEv");
  Engine_get_singleton = dlsym(RTLD_DEFAULT, "_ZN6Engine13get_singletonEv");

  if (!Engine_set_time_scale || !Engine_set_physics_ticks_per_second ||
      !Engine_get_physics_ticks_per_second || !Engine_set_max_fps ||
      !Engine_get_max_fps || !Engine_get_singleton) return -1;

  return 0;
}

void set_Godot_Engine(float value) {
  os_unfair_lock_lock(&Godot_Engine_lock);

  void *engine = Engine_get_singleton();
  if (engine) {
    original_physics_ticks = Engine_get_physics_ticks_per_second(engine);
    original_max_fps = Engine_get_max_fps(engine);
  }

  Godot_Engine_speed = value;

  if (Engine_set_time_scale && Engine_get_singleton) {
    void *engine = Engine_get_singleton();
    if (engine) {
      Engine_set_time_scale(engine, (double)Godot_Engine_speed);

      Engine_set_physics_ticks_per_second(engine, (int)(original_physics_ticks * Godot_Engine_speed));

      if (original_max_fps > 0) {
        Engine_set_max_fps(engine, (int)(original_max_fps * Godot_Engine_speed));
      } else {
        Engine_set_max_fps(engine, (int)(60 * Godot_Engine_speed));
      }
    }
  }

  os_unfair_lock_unlock(&Godot_Engine_lock);
}

void reset_Godot_Engine(void) {
  set_Godot_Engine(1.0);
}
