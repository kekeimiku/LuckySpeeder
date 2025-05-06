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
#include <string.h>
#include <sys/time.h>

static float timeScale_speed = 1.0;

static void (*real_timeScale)(float) = NULL;

static void my_timeScale(void) {
  if (real_timeScale)
    real_timeScale(timeScale_speed);
}

int hook_timeScale(void) {
  if (real_timeScale)
    return 0;

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
  if (!unity_vmaddr_slide)
    return -1;

  size_t size;

  uint8_t *cstring_section_data =
      getsectiondata((const struct mach_header_64 *)unity_vmaddr_slide,
                     "__TEXT", "__cstring", &size);
  if (!cstring_section_data)
    return -1;

  uint8_t *time_scale_function_address =
      memmem(cstring_section_data, size,
             "UnityEngine.Time::set_timeScale(System.Single)", 0x2F);
  if (!time_scale_function_address)
    return -1;

  uintptr_t il2cpp_section_base = (uintptr_t)getsectiondata(
      (const struct mach_header_64 *)unity_vmaddr_slide, "__TEXT", "il2cpp",
      &size);
  if (!il2cpp_section_base)
    return -1;

  uint8_t *il2cpp_end = (uint8_t *)(size + il2cpp_section_base);
  if (il2cpp_section_base + 4 >= size + il2cpp_section_base)
    return -1;

  uintptr_t first_instruction = *(uint32_t *)il2cpp_section_base;
  uintptr_t resolved_address, function_offset, second_instruction;

  while (1) {
    second_instruction = *(uint32_t *)(il2cpp_section_base + 4);
    if ((first_instruction & 0x9F000000) == 0x90000000 &&
        (second_instruction & 0xFF800000) == 0x91000000) {
      resolved_address = (il2cpp_section_base & 0xFFFFFFFFFFFFF000LL) +
                         (uint32_t)((((first_instruction >> 3) & 0xFFFFFFFC) |
                                     ((first_instruction >> 29) & 3))
                                    << 12);
      function_offset = (second_instruction >> 10) & 0xFFF;
      if ((second_instruction & 0xC00000) != 0)
        function_offset <<= 12;
      if ((uint8_t *)(resolved_address + function_offset) ==
          time_scale_function_address)
        break;
    }
    il2cpp_section_base += 4;
    first_instruction = second_instruction;
    if ((uint8_t *)(il2cpp_section_base + 8) >= il2cpp_end)
      return -1;
  }

  uintptr_t current_address = il2cpp_section_base;
  uintptr_t current_instruction, code_section_address;

  do {
    current_instruction = *(uint32_t *)(current_address - 4);
    current_address -= 4;
  } while ((current_instruction & 0x9F000000) != 0x90000000);

  code_section_address = (current_address & 0xFFFFFFFFFFFFF000LL) +
                         (uint32_t)((((current_instruction >> 3) & 0xFFFFFFFC) |
                                     ((current_instruction >> 29) & 3))
                                    << 12);

  uintptr_t method_data = *(uint32_t *)(current_address + 4);
  uintptr_t function_data_offset;

  if ((method_data & 0x1000000) != 0)
    function_data_offset = 8 * ((method_data >> 10) & 0xFFF);
  else
    function_data_offset = (method_data >> 10) & 0xFFF;

  if (*(uintptr_t *)(code_section_address + function_data_offset)) {
    real_timeScale =
        *(void (**)(float))(function_data_offset + code_section_address);
  } else {
    uint32_t instruction_operand = *(uint32_t *)(il2cpp_section_base + 8);
    uint8_t *code_section_start = (uint8_t *)(il2cpp_section_base + 8);
    uintptr_t instruction_offset = (4 * instruction_operand) & 0xFFFFFFC;
    uintptr_t address_offset =
        (4 * (instruction_operand & 0x3FFFFFF)) | 0xFFFFFFFFF0000000LL;

    if (((4 * instruction_operand) & 0x8000000) != 0)
      function_offset = address_offset;
    else
      function_offset = instruction_offset;

    real_timeScale = (void (*)(float))((uintptr_t(*)(void *)) &
                                       code_section_start[function_offset])(
        time_scale_function_address);
  }

  if (real_timeScale) {
    *(uintptr_t *)(function_data_offset + code_section_address) =
        (uintptr_t)my_timeScale;
    return 0;
  }

  return -1;
}

void set_timeScale(float value) {
  timeScale_speed = value;
  my_timeScale();
}

void reset_timeScale(void) { set_timeScale(1.0); }

static float gettimeofday_speed = 1.0;
static float clock_gettime_speed = 1.0;
static float mach_absolute_time_speed = 1.0;

#define USec_Scale (1000000)
static time_t gettimeofday_pre_sec;
static suseconds_t gettimeofday_pre_usec;
static time_t gettimeofday_true_pre_sec;
static suseconds_t gettimeofday_true_pre_usec;

static int (*real_gettimeofday)(struct timeval *, void *) = NULL;

// my_gettimeofday fix from AccDemo
static int my_gettimeofday(struct timeval *tv, struct timezone *tz) {
  int ret = real_gettimeofday(tv, tz);
  if (!ret) {
    if (!gettimeofday_pre_sec) {
      gettimeofday_pre_sec = tv->tv_sec;
      gettimeofday_true_pre_sec = tv->tv_sec;
      gettimeofday_pre_usec = tv->tv_usec;
      gettimeofday_true_pre_usec = tv->tv_usec;
    } else {
      int64_t true_curSec = tv->tv_sec * USec_Scale + tv->tv_usec;
      int64_t true_preSec =
          gettimeofday_true_pre_sec * USec_Scale + gettimeofday_true_pre_usec;
      int64_t invl = true_curSec - true_preSec;
      invl *= gettimeofday_speed;

      int64_t curSec =
          gettimeofday_pre_sec * USec_Scale + gettimeofday_pre_usec;
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
  return ret;
}

int hook_gettimeofday(void) {
  if (real_gettimeofday)
    return 0;

  struct rebinding rebindings = {"gettimeofday", my_gettimeofday,
                                 (void *)&real_gettimeofday};
  return rebind_symbols(&rebindings, 1);
}

void reset_gettimeofday(void) { gettimeofday_speed = 1.0; }

void set_gettimeofday(float value) { gettimeofday_speed = value; }

static int (*real_clock_gettime)(clockid_t clock_id,
                                 struct timespec *tp) = NULL;

#define NSec_Scale (1000000000)
static time_t clock_gettime_pre_sec;
static long clock_gettime_pre_nsec;
static time_t clock_gettime_true_pre_sec;
static long clock_gettime_true_pre_nsec;

// my_clock_gettime fix from AccDemo
static int my_clock_gettime(clockid_t clk_id, struct timespec *tp) {
  int ret = real_clock_gettime(clk_id, tp);
  if (!ret) {
    if (!clock_gettime_pre_sec) {
      clock_gettime_pre_sec = tp->tv_sec;
      clock_gettime_true_pre_sec = tp->tv_sec;
      clock_gettime_pre_nsec = tp->tv_nsec;
      clock_gettime_true_pre_nsec = tp->tv_nsec;
    } else {
      int64_t true_curSec = tp->tv_sec * NSec_Scale + tp->tv_nsec;
      int64_t true_preSec =
          clock_gettime_true_pre_sec * NSec_Scale + clock_gettime_true_pre_nsec;
      int64_t invl = true_curSec - true_preSec;
      invl *= clock_gettime_speed;

      int64_t curSec =
          clock_gettime_pre_sec * NSec_Scale + clock_gettime_pre_nsec;
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
  return ret;
}

int hook_clock_gettime(void) {
  if (real_clock_gettime)
    return 0;

  struct rebinding rebindings = {"clock_gettime", my_clock_gettime,
                                 (void *)&real_clock_gettime};
  return rebind_symbols(&rebindings, 1);
}

void reset_clock_gettime(void) { clock_gettime_speed = 1.0; }

void set_clock_gettime(float value) { clock_gettime_speed = value; }

static uint64_t (*real_mach_absolute_time)(void) = NULL;

static bool init_mach_absolute_time;
static uint64_t mach_absolute_base_time;
static uint64_t mach_absolute_start_time;
static uint64_t mach_absolute_last_time;

static uint64_t my_mach_absolute_time(void) {
  uint64_t result;
  uint64_t current_time = real_mach_absolute_time();

  if (!init_mach_absolute_time) {
    init_mach_absolute_time = true;
    mach_absolute_base_time = current_time;
    mach_absolute_start_time =
        (mach_absolute_last_time != 0) ? mach_absolute_last_time : current_time;
    result = mach_absolute_start_time;
  } else {
    result =
        mach_absolute_start_time +
        (current_time - mach_absolute_base_time) * mach_absolute_time_speed;
  }

  mach_absolute_last_time = result;

  return result;
}

int hook_mach_absolute_time(void) {
  if (real_mach_absolute_time)
    return 0;

  struct rebinding rebindings = {"mach_absolute_time", my_mach_absolute_time,
                                 (void *)&real_mach_absolute_time};
  return rebind_symbols(&rebindings, 1);
}

void reset_mach_absolute_time(void) { mach_absolute_time_speed = 1.0; }

void set_mach_absolute_time(float value) { mach_absolute_time_speed = value; }
