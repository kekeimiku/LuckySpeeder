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

#include "fishhook.h"
#include <UIKit/UIKit.h>
#include <mach-o/dyld.h>
#include <mach-o/getsect.h>
#include <string.h>
#include <sys/time.h>

// hook unity timeScale

static float timeScale_speed = 1.0;

static void (*real_timeScale)(float) = NULL;

void my_timeScale(void) {
  if (real_timeScale) {
    real_timeScale(timeScale_speed);
  }
}

int hook_timeScale(void) {
  if (real_timeScale) {
    return 0;
  }

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
                         (int32_t)(((first_instruction >> 3) & 0xFFFFFFFC |
                                    (first_instruction >> 29) & 3)
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
                         (int32_t)(((current_instruction >> 3) & 0xFFFFFFFC |
                                    (current_instruction >> 29) & 3)
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

void set_timeScale(float a1) {
  timeScale_speed = a1;
  my_timeScale();
}

void restore_timeScale(void) { set_timeScale(1.0); }

// hook system gettimeofday and clock_gettime

static float gettimeofday_speed = 1.0;
static float clock_gettime_speed = 1.0;

static time_t pre_sec;
static suseconds_t pre_usec;
static time_t true_pre_sec;
static suseconds_t true_pre_usec;

#define USec_Scale (1000000LL)
#define NSec_Scale (1000000000LL)

static int (*real_gettimeofday)(struct timeval *, void *) = NULL;

int my_gettimeofday(struct timeval *tv, struct timezone *tz) {
  int ret = real_gettimeofday(tv, tz);
  if (!ret) {
    if (!pre_sec) {
      pre_sec = tv->tv_sec;
      true_pre_sec = tv->tv_sec;
      pre_usec = tv->tv_usec;
      true_pre_usec = tv->tv_usec;
    } else {
      int64_t true_curSec = tv->tv_sec * USec_Scale + tv->tv_usec;
      int64_t true_preSec = true_pre_sec * USec_Scale + true_pre_usec;
      int64_t invl = true_curSec - true_preSec;
      invl *= gettimeofday_speed;

      int64_t curSec = pre_sec * USec_Scale + pre_usec;
      curSec += invl;

      time_t used_sec = curSec / USec_Scale;
      suseconds_t used_usec = curSec % USec_Scale;

      true_pre_sec = tv->tv_sec;
      true_pre_usec = tv->tv_usec;
      tv->tv_sec = used_sec;
      tv->tv_usec = used_usec;
      pre_sec = used_sec;
      pre_usec = used_usec;
    }
  }
  return ret;
}

int hook_gettimeofday(void) {
  if (real_gettimeofday) {
    return 0;
  }
  return rebind_symbols((struct rebinding[1]){{"gettimeofday", my_gettimeofday,
                                               (void *)&real_gettimeofday}},
                        1);
}

void restore_gettimeofday(void) { gettimeofday_speed = 1.0; }

void set_gettimeofday(float a1) { gettimeofday_speed = a1; }

static int (*real_clock_gettime)(clockid_t clock_id,
                                 struct timespec *tp) = NULL;

int my_clock_gettime(clockid_t clk_id, struct timespec *tp) {
  int ret = real_clock_gettime(clk_id, tp);
  if (!ret) {
    if (!pre_sec) {
      pre_sec = tp->tv_sec;
      true_pre_sec = tp->tv_sec;
      pre_usec = tp->tv_nsec;
      true_pre_usec = tp->tv_nsec;
    } else {
      int64_t true_curSec = tp->tv_sec * NSec_Scale + tp->tv_nsec;
      int64_t true_preSec = true_pre_sec * NSec_Scale + true_pre_usec;
      int64_t invl = true_curSec - true_preSec;
      invl *= clock_gettime_speed;

      int64_t curSec = pre_sec * NSec_Scale + pre_usec;
      curSec += invl;

      time_t used_sec = curSec / NSec_Scale;
      suseconds_t used_usec = curSec % NSec_Scale;

      true_pre_sec = tp->tv_sec;
      true_pre_usec = tp->tv_nsec;
      tp->tv_sec = used_sec;
      tp->tv_nsec = used_usec;
      pre_sec = used_sec;
      pre_usec = used_usec;
    }
  }
  return ret;
}

int hook_clock_gettime(void) {
  if (real_clock_gettime) {
    return 0;
  }

  return rebind_symbols(
      (struct rebinding[1]){
          {"clock_gettime", my_clock_gettime, (void *)&real_clock_gettime}},
      1);
}

void restore_clock_gettime(void) { clock_gettime_speed = 1.0; }

void set_clock_gettime(float a1) { clock_gettime_speed = a1; }

// UI

@interface LuckySpeederView : UIView

@property(nonatomic, strong) UIView *uiContainer;

@property(nonatomic, assign) CGPoint lastLocation;

typedef NS_ENUM(NSUInteger, SpeedMode) { Heart, Spade, Club, Diamond };

@property(nonatomic, assign) SpeedMode currentMod;

@property(nonatomic, strong) UIButton *button1;
@property(nonatomic, strong) UIButton *button2;
@property(nonatomic, strong) UIButton *button3;
@property(nonatomic, strong) UIButton *button4;
@property(nonatomic, strong) UIButton *button5;

@property(nonatomic, strong) UIImageSymbolConfiguration *symbolConfiguration;

@property(nonatomic, strong) NSArray *speedValues;

@property(nonatomic, assign) NSInteger currentIndex;

@end

@implementation LuckySpeederView

+ (id)sharedInstance {
  static UIView *ui;
  static dispatch_once_t token;
  dispatch_once(&token, ^{
    ui = [[self alloc] init];
  });

  return ui;
}

- (instancetype)init {

  UIWindowScene *windowScene = (UIWindowScene *)
      [[UIApplication sharedApplication].connectedScenes anyObject];
  CGSize windowSize = windowScene.windows.firstObject.bounds.size;
  CGFloat windowWidth = windowSize.width;
  CGFloat windowHeight = windowSize.height;

  CGFloat initialH;

  UIDevice *device = [UIDevice currentDevice];
  UIUserInterfaceIdiom idiom = device.userInterfaceIdiom;
  if (idiom == UIUserInterfaceIdiomPhone) {
    initialH = 34;
  } else if (idiom == UIUserInterfaceIdiomPad) {
    initialH = 48;
  } else {
    initialH = 72;
  }

  CGFloat initialY = windowHeight / 5;
  CGFloat initialX = windowWidth - initialH * 5 - 20;
  CGFloat initialW = initialH * 5;

  self =
      [super initWithFrame:CGRectMake(initialX, initialY, initialW, initialH)];

  CGFloat buttonWidth = self.bounds.size.height;
  CGFloat fontSize = buttonWidth * 0.44;
  self.symbolConfiguration =
      [UIImageSymbolConfiguration configurationWithPointSize:fontSize];

  self.backgroundColor = [UIColor opaqueSeparatorColor];
  self.layer.masksToBounds = YES;
  self.layer.cornerRadius = buttonWidth / 2 - 5;
  self.layer.shadowColor = [UIColor opaqueSeparatorColor].CGColor;
  self.layer.shadowOpacity = 0.5;
  self.layer.shadowOffset = CGSizeMake(0, 0);

  self.uiContainer = [[UIView alloc] initWithFrame:self.bounds];
  [self addSubview:self.uiContainer];

  self.currentMod = Heart;

  self.speedValues = @[
    @0.1, @0.25, @0.5, @0.75, @0.9, @1,   @1.1, @1.2, @1.3, @1.4, @1.5, @1.6,
    @1.7, @1.8,  @1.9, @2,    @2.1, @2.2, @2.3, @2.4, @2.5, @5,   @10
  ];

  self.currentIndex = 5;

  self.button1 = [UIButton buttonWithType:UIButtonTypeCustom];
  self.button1.frame = CGRectMake(0, 0, buttonWidth, buttonWidth);
  [self.button1 setImage:[UIImage systemImageNamed:@"suit.heart.fill"
                                 withConfiguration:self.symbolConfiguration]
                forState:UIControlStateNormal];
  self.button1.titleLabel.font = [UIFont systemFontOfSize:fontSize];
  [self.button1 addTarget:self
                   action:@selector(Button1Changed)
         forControlEvents:UIControlEventTouchUpInside];
  [self.uiContainer addSubview:self.button1];

  self.button2 = [UIButton buttonWithType:UIButtonTypeCustom];
  self.button2.frame = CGRectMake(buttonWidth, 0, buttonWidth, buttonWidth);
  [self.button2 setImage:[UIImage systemImageNamed:@"backward.fill"
                                 withConfiguration:self.symbolConfiguration]
                forState:UIControlStateNormal];
  [self.button2 addTarget:self
                   action:@selector(Button2Changed)
         forControlEvents:UIControlEventTouchUpInside];
  [self.uiContainer addSubview:self.button2];

  self.button3 = [UIButton buttonWithType:UIButtonTypeCustom];
  self.button3.frame = CGRectMake(2 * buttonWidth, 0, buttonWidth, buttonWidth);
  [self.button3 setTitle:[self.speedValues[self.currentIndex] stringValue]
                forState:UIControlStateNormal];
  self.button3.titleLabel.font = [UIFont systemFontOfSize:fontSize];
  [self.button3 setTitleColor:self.tintColor forState:UIControlStateNormal];
  [self.button3 addTarget:self
                   action:@selector(Button3Changed)
         forControlEvents:UIControlEventTouchUpInside];
  [self.uiContainer addSubview:self.button3];

  self.button4 = [UIButton buttonWithType:UIButtonTypeCustom];
  self.button4.frame = CGRectMake(3 * buttonWidth, 0, buttonWidth, buttonWidth);
  [self.button4 setImage:[UIImage systemImageNamed:@"forward.fill"
                                 withConfiguration:self.symbolConfiguration]
                forState:UIControlStateNormal];
  [self.button4 addTarget:self
                   action:@selector(Button4Changed)
         forControlEvents:UIControlEventTouchUpInside];
  [self.uiContainer addSubview:self.button4];

  self.button5 = [UIButton buttonWithType:UIButtonTypeCustom];
  self.button5.frame = CGRectMake(4 * buttonWidth, 0, buttonWidth, buttonWidth);
  [self.button5 setImage:[UIImage systemImageNamed:@"play.fill"
                                 withConfiguration:self.symbolConfiguration]
                forState:UIControlStateNormal];
  [self.button5 addTarget:self
                   action:@selector(Button5Changed)
         forControlEvents:UIControlEventTouchUpInside];
  [self.uiContainer addSubview:self.button5];

  UIPanGestureRecognizer *panGesture = [[UIPanGestureRecognizer alloc]
      initWithTarget:self
              action:@selector(handlePanGesture:)];
  [self addGestureRecognizer:panGesture];

  return self;
}

- (void)handlePanGesture:(UIPanGestureRecognizer *)gesture {
  CGPoint translation = [gesture translationInView:self.superview];
  if (gesture.state == UIGestureRecognizerStateBegan) {
    self.lastLocation = self.center;
  }

  CGPoint newCenter = CGPointMake(self.lastLocation.x + translation.x,
                                  self.lastLocation.y + translation.y);

  CGFloat minX = self.bounds.size.width / 2;
  CGFloat maxX = self.superview.bounds.size.width - self.bounds.size.width / 2;
  CGFloat minY = self.bounds.size.height / 2;
  CGFloat maxY =
      self.superview.bounds.size.height - self.bounds.size.height / 2;

  newCenter.x = MAX(minX, MIN(newCenter.x, maxX));
  newCenter.y = MAX(minY, MIN(newCenter.y, maxY));

  self.center = newCenter;

  if (gesture.state == UIGestureRecognizerStateEnded) {
    self.lastLocation = self.center;
  }
}

- (void)Button1Changed {
  self.userInteractionEnabled = NO;

  if (self.button5.isSelected) {
    [self restoreHook];
    [self.button5 setImage:[UIImage systemImageNamed:@"play.fill"
                                   withConfiguration:self.symbolConfiguration]
                  forState:UIControlStateNormal];
    self.button5.selected = NO;
  }

  NSString *stateSymbol = @"";
  switch (self.currentMod) {
  case Heart:
    stateSymbol = @"suit.spade.fill";
    self.currentMod = Spade;
    break;
  case Spade:
    stateSymbol = @"suit.club.fill";
    self.currentMod = Club;
    break;
  case Club:
    stateSymbol = @"suit.diamond.fill";
    self.currentMod = Diamond;
    break;
  case Diamond:
    stateSymbol = @"suit.heart.fill";
    self.currentMod = Heart;
    break;
  }

  [self.button1 setImage:[UIImage systemImageNamed:stateSymbol
                                 withConfiguration:self.symbolConfiguration]
                forState:UIControlStateNormal];

  self.userInteractionEnabled = YES;
}

- (void)Button2Changed {
  if (self.currentIndex > 0) {
    self.currentIndex--;
    if (self.button5.isSelected) {
      [self updateSpeed:[self.speedValues[self.currentIndex] floatValue]];
    }
    [self.button3 setTitle:[self.speedValues[self.currentIndex] stringValue]
                  forState:UIControlStateNormal];
  }
}

- (void)Button3Changed {
  // TODO
}

- (void)Button4Changed {
  if (self.currentIndex < self.speedValues.count - 1) {
    self.currentIndex++;
    if (self.button5.isSelected) {
      [self updateSpeed:[self.speedValues[self.currentIndex] floatValue]];
    }
    [self.button3 setTitle:[self.speedValues[self.currentIndex] stringValue]
                  forState:UIControlStateNormal];
  }
}

- (void)Button5Changed {
  self.userInteractionEnabled = NO;

  if (self.button5.isSelected) {
    [self restoreHook];
    [self.button5 setImage:[UIImage systemImageNamed:@"play.fill"
                                   withConfiguration:self.symbolConfiguration]
                  forState:UIControlStateNormal];
  } else {
    [self initHook];
    [self updateSpeed:[self.speedValues[self.currentIndex] floatValue]];

    [self.button5 setImage:[UIImage systemImageNamed:@"pause.fill"
                                   withConfiguration:self.symbolConfiguration]
                  forState:UIControlStateNormal];
  }
  self.button5.selected = !self.button5.selected;

  self.userInteractionEnabled = YES;
}

- (void)updateSpeed:(float)value {
  switch (self.currentMod) {
  case Heart:
    set_timeScale(value);
    break;
  case Spade:
    set_gettimeofday(value);
    break;
  case Club:
    set_clock_gettime(value);
    break;
  case Diamond:
    break;
  }
}

- (void)initHook {
  switch (self.currentMod) {
  case Heart:
    hook_timeScale();
    break;
  case Spade:
    hook_gettimeofday();
    break;
  case Club:
    hook_clock_gettime();
    break;
  case Diamond:
    break;
  }
}

- (void)restoreHook {
  switch (self.currentMod) {
  case Heart:
    restore_timeScale();
    break;
  case Spade:
    restore_gettimeofday();
    break;
  case Club:
    restore_clock_gettime();
    break;
  case Diamond:
    break;
  }
}

@end

static void didFinishLaunching(CFNotificationCenterRef center, void *observer,
                               CFStringRef name, const void *object,
                               CFDictionaryRef info) {
  dispatch_after(
      dispatch_time(DISPATCH_TIME_NOW, 1 * NSEC_PER_SEC),
      dispatch_get_main_queue(), ^{
        UIWindowScene *windowScene = (UIWindowScene *)
            [[UIApplication sharedApplication].connectedScenes anyObject];
        UIViewController *controller =
            windowScene.windows.firstObject.rootViewController;
        [controller.view addSubview:LuckySpeederView.sharedInstance];
      });
}

__attribute__((constructor)) static void initialize(void) {
  CFNotificationCenterAddObserver(
      CFNotificationCenterGetLocalCenter(), NULL, &didFinishLaunching,
      (CFStringRef)UIApplicationDidFinishLaunchingNotification, NULL,
      CFNotificationSuspensionBehaviorDeliverImmediately);
}
