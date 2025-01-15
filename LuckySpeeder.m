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
#include <UIKit/UIKit.h>

@interface LuckySpeederView : UIView

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

  CGFloat initialW = initialH * 5;
  CGFloat initialX = windowWidth - initialH * 5 - 20;
  CGFloat initialY = windowHeight / 5;

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
  [self addSubview:self.button1];

  self.button2 = [UIButton buttonWithType:UIButtonTypeCustom];
  self.button2.frame = CGRectMake(buttonWidth, 0, buttonWidth, buttonWidth);
  [self.button2 setImage:[UIImage systemImageNamed:@"backward.fill"
                                 withConfiguration:self.symbolConfiguration]
                forState:UIControlStateNormal];
  [self.button2 addTarget:self
                   action:@selector(Button2Changed)
         forControlEvents:UIControlEventTouchUpInside];
  [self addSubview:self.button2];

  self.button3 = [UIButton buttonWithType:UIButtonTypeCustom];
  self.button3.frame = CGRectMake(2 * buttonWidth, 0, buttonWidth, buttonWidth);
  [self.button3 setTitle:[self.speedValues[self.currentIndex] stringValue]
                forState:UIControlStateNormal];
  self.button3.titleLabel.font = [UIFont systemFontOfSize:fontSize];
  [self.button3 setTitleColor:self.tintColor forState:UIControlStateNormal];
  [self.button3 addTarget:self
                   action:@selector(Button3Changed)
         forControlEvents:UIControlEventTouchUpInside];
  [self addSubview:self.button3];

  self.button4 = [UIButton buttonWithType:UIButtonTypeCustom];
  self.button4.frame = CGRectMake(3 * buttonWidth, 0, buttonWidth, buttonWidth);
  [self.button4 setImage:[UIImage systemImageNamed:@"forward.fill"
                                 withConfiguration:self.symbolConfiguration]
                forState:UIControlStateNormal];
  [self.button4 addTarget:self
                   action:@selector(Button4Changed)
         forControlEvents:UIControlEventTouchUpInside];
  [self addSubview:self.button4];

  self.button5 = [UIButton buttonWithType:UIButtonTypeCustom];
  self.button5.frame = CGRectMake(4 * buttonWidth, 0, buttonWidth, buttonWidth);
  [self.button5 setImage:[UIImage systemImageNamed:@"play.fill"
                                 withConfiguration:self.symbolConfiguration]
                forState:UIControlStateNormal];
  [self.button5 addTarget:self
                   action:@selector(Button5Changed)
         forControlEvents:UIControlEventTouchUpInside];
  [self addSubview:self.button5];

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
    [self resetHook];
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
    [self resetHook];
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
    set_mach_absolute_time(value);
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
    hook_mach_absolute_time();
    break;
  }
}

- (void)resetHook {
  switch (self.currentMod) {
  case Heart:
    reset_timeScale();
    break;
  case Spade:
    reset_gettimeofday();
    break;
  case Club:
    reset_clock_gettime();
    break;
  case Diamond:
    reset_mach_absolute_time();
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
