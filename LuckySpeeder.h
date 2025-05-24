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

#ifndef LuckySpeeder_H
#define LuckySpeeder_H

#define HOOK_SUCCESS 0

int hook_timeScale(void);

void set_timeScale(float value);

void reset_timeScale(void);

int hook_gettimeofday(void);

void set_gettimeofday(float value);

void reset_gettimeofday(void);

int hook_clock_gettime(void);

void set_clock_gettime(float value);

void reset_clock_gettime(void);

int hook_mach_absolute_time(void);

void set_mach_absolute_time(float value);

void reset_mach_absolute_time(void);

int hook_SKScene_update(void);

void set_SKScene_update(float value);

void reset_SKScene_update(void);

#endif // LuckySpeeder_H
