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

#import "LuckySpeeder.h"
#import <UIKit/UIKit.h>

static const float speedValues[] = {0.1, 0.25, 0.5, 0.75, 0.9, 1.0, 1.1, 1.2,
                                    1.3, 1.4,  1.5, 1.6,  1.7, 1.8, 1.9, 2.0,
                                    2.1, 2.2,  2.3, 2.4,  2.5, 5.0, 10.0};
static int currentIndex = 5;
static float currentValue = 1.0;
static const int speedValuesCount = sizeof(speedValues) / sizeof(float);

enum SpeedMode { Heart, Spade, Club, Diamond };
static enum SpeedMode currentMod = Heart;

static void updateSpeed(float value) {
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

static int initHook() {
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

static void resetHook() {
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

@interface LuckySpeederView : UIView

@property(nonatomic, assign) CGPoint lastLocation;
@property(nonatomic, assign) CGFloat windowWidth;
@property(nonatomic, assign) CGFloat windowHeight;
@property(nonatomic, strong) UIButton *button1;
@property(nonatomic, strong) UIButton *button2;
@property(nonatomic, strong) UIButton *button3;
@property(nonatomic, strong) UIButton *button4;
@property(nonatomic, strong) UIButton *button5;
@property(nonatomic, strong) UIButton *button6;
@property(nonatomic, strong) NSTimer *idleTimer;
@property(nonatomic, strong) UIImageSymbolConfiguration *symbolConfiguration;

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
  self.windowWidth = windowSize.width;
  self.windowHeight = windowSize.height;

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
  CGFloat initialX = self.windowWidth - initialH * 5 - 20;
  CGFloat initialY = self.windowHeight / 5;

  self =
      [super initWithFrame:CGRectMake(initialX, initialY, initialW, initialH)];

  CGFloat buttonWidth = self.bounds.size.height;
  CGFloat fontSize = buttonWidth * 0.44;
  self.symbolConfiguration =
      [UIImageSymbolConfiguration configurationWithPointSize:fontSize];

  self.backgroundColor = [UIColor opaqueSeparatorColor];
  self.layer.masksToBounds = YES;
  self.layer.cornerRadius = buttonWidth / 2;
  self.layer.shadowColor = [UIColor opaqueSeparatorColor].CGColor;
  self.layer.shadowOpacity = 0.5;
  self.layer.shadowOffset = CGSizeMake(0, 0);

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
  [self.button3 setTitle:[@(currentValue) stringValue]
                forState:UIControlStateNormal];
  self.button3.titleLabel.font = [UIFont systemFontOfSize:fontSize];
  self.button3.titleLabel.adjustsFontSizeToFitWidth = YES;
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

  self.button6 = [UIButton buttonWithType:UIButtonTypeCustom];
  [self.button6 setImage:[UIImage systemImageNamed:@"clock.fill"
                                 withConfiguration:self.symbolConfiguration]
                forState:UIControlStateNormal];
  self.button6.hidden = YES;
  [self.button6 addTarget:self
                   action:@selector(Button6Changed)
         forControlEvents:UIControlEventTouchUpInside];
  [self addSubview:self.button6];

  UIPanGestureRecognizer *panGesture = [[UIPanGestureRecognizer alloc]
      initWithTarget:self
              action:@selector(handlePanGesture:)];
  [self addGestureRecognizer:panGesture];

  [self resetIdleTimer];

  return self;
}

- (void)resetIdleTimer {
  [self.idleTimer invalidate];
  self.idleTimer =
      [NSTimer scheduledTimerWithTimeInterval:5.0
                                       target:self
                                     selector:@selector(hideSpeedView)
                                     userInfo:nil
                                      repeats:NO];
}

- (void)handlePanGesture:(UIPanGestureRecognizer *)gesture {
  CGPoint translation = [gesture translationInView:self.superview];
  if (gesture.state == UIGestureRecognizerStateBegan) {
    self.lastLocation = self.center;
    [self.idleTimer invalidate];
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
    [self resetIdleTimer];
  }
}

- (void)hideSpeedView {
  if (!self.button6.hidden) {
    return;
  }

  CGFloat buttonWidth = self.bounds.size.height;
  CGFloat newX = self.center.x < self.windowWidth / 2
                     ? self.frame.origin.x
                     : self.frame.origin.x + 4 * buttonWidth;

  [UIView animateWithDuration:0.4
                   animations:^{
                     self.frame = CGRectMake(newX, self.frame.origin.y,
                                             buttonWidth, buttonWidth);
                     self.alpha = 0.5;
                     self.layer.cornerRadius = buttonWidth / 2;
                   }];

  self.button1.hidden = YES;
  self.button2.hidden = YES;
  self.button3.hidden = YES;
  self.button4.hidden = YES;
  self.button5.hidden = YES;
  self.button6.frame = self.bounds;
  self.button6.hidden = NO;
}

- (void)Button1Changed {
  self.userInteractionEnabled = NO;

  if (self.button5.isSelected) {
    resetHook();
    [self.button5 setImage:[UIImage systemImageNamed:@"play.fill"
                                   withConfiguration:self.symbolConfiguration]
                  forState:UIControlStateNormal];
    self.button5.selected = NO;
  }

  NSString *stateSymbol = @"";
  switch (currentMod) {
  case Heart:
    stateSymbol = @"suit.spade.fill";
    currentMod = Spade;
    break;
  case Spade:
    stateSymbol = @"suit.club.fill";
    currentMod = Club;
    break;
  case Club:
    stateSymbol = @"suit.diamond.fill";
    currentMod = Diamond;
    break;
  case Diamond:
    stateSymbol = @"suit.heart.fill";
    currentMod = Heart;
    break;
  }

  [self.button1 setImage:[UIImage systemImageNamed:stateSymbol
                                 withConfiguration:self.symbolConfiguration]
                forState:UIControlStateNormal];

  [self resetIdleTimer];

  self.userInteractionEnabled = YES;
}

- (void)Button2Changed {
  if (currentIndex > 0) {
    currentIndex--;
    currentValue = speedValues[currentIndex];
    if (self.button5.isSelected) {
      updateSpeed(currentValue);
    }
  }
  [self.button3 setTitle:[@(currentValue) stringValue]
                forState:UIControlStateNormal];
  [self resetIdleTimer];
}

- (void)Button3Changed {
  self.userInteractionEnabled = NO;
  [self.idleTimer invalidate];

  UIAlertController *alertController = [UIAlertController
      alertControllerWithTitle:@"Custom Speed"
                       message:@"Open Source: "
                               @"\nhttps://github.com/kekeimiku/LuckySpeeder"
                preferredStyle:UIAlertControllerStyleAlert];

  [alertController
      addTextFieldWithConfigurationHandler:^(UITextField *_Nonnull textField) {
        textField.placeholder = @"0.1~999";
        textField.keyboardType = UIKeyboardTypeDecimalPad;
      }];

  UIAlertAction *confirmAction = [UIAlertAction
      actionWithTitle:@"OK"
                style:UIAlertActionStyleDefault
              handler:^(UIAlertAction *_Nonnull action) {
                NSString *inputText =
                    alertController.textFields.firstObject.text;
                CGFloat inputValue = [inputText floatValue];
                if (inputValue >= 0.1 && inputValue <= 999) {
                  currentValue = inputValue;
                  if (self.button5.isSelected) {
                    updateSpeed(currentValue);
                  }
                  [self.button3
                      setTitle:[NSString stringWithFormat:@"%.2f", currentValue]
                      forState:UIControlStateNormal];
                }
                self.userInteractionEnabled = YES;
                [self resetIdleTimer];
              }];

  UIAlertAction *cancelAction =
      [UIAlertAction actionWithTitle:@"Cancel"
                               style:UIAlertActionStyleDefault
                             handler:^(UIAlertAction *_Nonnull action) {
                               self.userInteractionEnabled = YES;
                               [self resetIdleTimer];
                             }];

  [alertController addAction:confirmAction];
  [alertController addAction:cancelAction];

  UIWindowScene *windowScene = (UIWindowScene *)
      [[UIApplication sharedApplication].connectedScenes anyObject];
  UIViewController *controller =
      windowScene.windows.firstObject.rootViewController;
  [controller presentViewController:alertController
                           animated:YES
                         completion:nil];
}

- (void)Button4Changed {
  if (currentIndex < speedValuesCount - 1) {
    currentIndex++;
    currentValue = speedValues[currentIndex];
    if (self.button5.isSelected) {
      updateSpeed(currentValue);
    }
  }
  [self.button3 setTitle:[@(currentValue) stringValue]
                forState:UIControlStateNormal];
  [self resetIdleTimer];
}

- (void)Button5Changed {
  self.userInteractionEnabled = NO;

  if (self.button5.isSelected) {
    resetHook();
    [self.button5 setImage:[UIImage systemImageNamed:@"play.fill"
                                   withConfiguration:self.symbolConfiguration]
                  forState:UIControlStateNormal];
  } else {
    initHook();
    updateSpeed(currentValue);
    [self.button5 setImage:[UIImage systemImageNamed:@"pause.fill"
                                   withConfiguration:self.symbolConfiguration]
                  forState:UIControlStateNormal];
  }
  self.button5.selected = !self.button5.selected;

  [self resetIdleTimer];

  self.userInteractionEnabled = YES;
}

- (void)Button6Changed {
  CGFloat buttonWidth = self.frame.size.width;
  CGFloat expandedWidth = buttonWidth * 5;
  CGFloat newX = self.center.x < self.windowWidth / 2
                     ? self.frame.origin.x
                     : self.frame.origin.x - 4 * buttonWidth;

  [UIView animateWithDuration:0.4
      animations:^{
        self.frame = CGRectMake(newX, self.frame.origin.y, expandedWidth,
                                self.frame.size.height);
        self.alpha = 1.0;
        self.layer.cornerRadius = buttonWidth / 2;
      }
      completion:^(BOOL finished) {
        self.button1.hidden = NO;
        self.button2.hidden = NO;
        self.button3.hidden = NO;
        self.button4.hidden = NO;
        self.button5.hidden = NO;
        self.button6.hidden = YES;
      }];

  [self resetIdleTimer];
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
