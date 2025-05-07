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

#ifndef LuckySpeederWrap_H
#define LuckySpeederWrap_H

#if !defined(LuckySpeederWrap_EXPORT)
#define LuckySpeederWrap_VISIBILITY __attribute__((visibility("hidden")))
#else
#define LuckySpeederWrap_VISIBILITY __attribute__((visibility("default")))
#endif

const float speedValues[] = {0.1, 0.25, 0.5, 0.75, 0.9, 1.0, 1.1, 1.2,
                             1.3, 1.4,  1.5, 1.6,  1.7, 1.8, 1.9, 2.0,
                             2.1, 2.2,  2.3, 2.4,  2.5, 5.0, 10.0};
const int speedValuesCount = sizeof(speedValues) / sizeof(float);
int currentIndex = 5;
float currentValue = 1.0;

enum SpeedMode { Heart, Spade, Club, Diamond };
enum SpeedMode currentMod = Heart;

LuckySpeederWrap_VISIBILITY void updateSpeed(float value);

LuckySpeederWrap_VISIBILITY int initHook(void);

LuckySpeederWrap_VISIBILITY void resetHook(void);

#endif // LuckySpeederWrap_H

int initHook(void) {
  switch (currentMod) {
  case Heart:
    return hook_timeScale();
  case Spade:
    return hook_gettimeofday();
  case Club:
    return hook_clock_gettime();
  case Diamond:
    return hook_mach_absolute_time();
  }
}

void resetHook(void) {
  switch (currentMod) {
  case Heart:
    reset_timeScale();
    return;
  case Spade:
    reset_gettimeofday();
    return;
  case Club:
    reset_clock_gettime();
    return;
  case Diamond:
    reset_mach_absolute_time();
    return;
  }
}

void updateSpeed(float value) {
  switch (currentMod) {
  case Heart:
    set_timeScale(value);
    return;
  case Spade:
    set_gettimeofday(value);
    return;
  case Club:
    set_clock_gettime(value);
    return;
  case Diamond:
    set_mach_absolute_time(value);
    return;
  }
}