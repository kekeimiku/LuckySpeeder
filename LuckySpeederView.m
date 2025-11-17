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

#import "LuckySpeederView.h"
#import "LuckySpeeder.h"
#import "LuckySpeederWrap.h"

@interface LuckySpeederView ()

@property(nonatomic, assign) CGPoint lastLocation;
@property(nonatomic, strong) UIButton *button1;
@property(nonatomic, strong) UIButton *button2;
@property(nonatomic, strong) UIButton *button3;
@property(nonatomic, strong) UIButton *button4;
@property(nonatomic, strong) UIButton *button5;
@property(nonatomic, strong) UIButton *button6;
@property(nonatomic, strong) UIButton *button7;
@property(nonatomic, strong) UIButton *button8;
@property(nonatomic, strong) UITextField *textField;
@property(nonatomic, strong) NSTimer *idleTimer;
@property(nonatomic, strong) UIImageSymbolConfiguration *symbolConfiguration;

@end

@implementation LuckySpeederView

- (LuckySpeederView *)initWithSize:(CGSize)size {
  CGFloat initialH;
  UIUserInterfaceIdiom idiom = [UIDevice currentDevice].userInterfaceIdiom;
  if (idiom == UIUserInterfaceIdiomPhone) {
    initialH = 34;
  } else if (idiom == UIUserInterfaceIdiomPad) {
    initialH = 48;
  } else {
    initialH = 72;
  }

  CGFloat initialW = initialH * 5;
  CGFloat initialX = size.width - initialW - 20;
  CGFloat initialY = size.height / 5;

  self = [super initWithFrame:CGRectMake(initialX, initialY, initialW, initialH)];

  CGFloat buttonWidth = self.bounds.size.height;
  CGFloat fontSize = buttonWidth * 0.44;
  self.symbolConfiguration = [UIImageSymbolConfiguration configurationWithPointSize:fontSize];

  self.backgroundColor = [UIColor opaqueSeparatorColor];
  self.layer.masksToBounds = YES;
  self.layer.cornerRadius = buttonWidth / 2;

  self.button1 = [UIButton buttonWithType:UIButtonTypeCustom];
  self.button1.frame = CGRectMake(0, 0, buttonWidth, buttonWidth);
  [self.button1 setImage:[UIImage systemImageNamed:@(modeSymbols[currentMod]) withConfiguration:self.symbolConfiguration] forState:UIControlStateNormal];
  [self.button1 addTarget:self action:@selector(Button1Changed) forControlEvents:UIControlEventTouchUpInside];
  [self addSubview:self.button1];

  self.button2 = [UIButton buttonWithType:UIButtonTypeCustom];
  self.button2.frame = CGRectMake(buttonWidth, 0, buttonWidth, buttonWidth);
  [self.button2 setImage:[UIImage systemImageNamed:@"backward.fill" withConfiguration:self.symbolConfiguration] forState:UIControlStateNormal];
  [self.button2 addTarget:self action:@selector(Button2Changed) forControlEvents:UIControlEventTouchUpInside];
  [self addSubview:self.button2];

  self.button3 = [UIButton buttonWithType:UIButtonTypeCustom];
  self.button3.frame = CGRectMake(2 * buttonWidth, 0, buttonWidth, buttonWidth);
  [self.button3 setTitle:[@(speedValue) stringValue] forState:UIControlStateNormal];
  self.button3.titleLabel.font = [UIFont systemFontOfSize:fontSize];
  self.button3.titleLabel.adjustsFontSizeToFitWidth = YES;
  [self.button3 setTitleColor:self.tintColor forState:UIControlStateNormal];
  [self.button3 addTarget:self action:@selector(Button3Changed) forControlEvents:UIControlEventTouchUpInside];
  [self addSubview:self.button3];

  self.button4 = [UIButton buttonWithType:UIButtonTypeCustom];
  self.button4.frame = CGRectMake(3 * buttonWidth, 0, buttonWidth, buttonWidth);
  [self.button4 setImage:[UIImage systemImageNamed:@"forward.fill" withConfiguration:self.symbolConfiguration] forState:UIControlStateNormal];
  [self.button4 addTarget:self action:@selector(Button4Changed) forControlEvents:UIControlEventTouchUpInside];
  [self addSubview:self.button4];

  self.button5 = [UIButton buttonWithType:UIButtonTypeCustom];
  self.button5.frame = CGRectMake(4 * buttonWidth, 0, buttonWidth, buttonWidth);
  [self.button5 setImage:[UIImage systemImageNamed:@"play.fill" withConfiguration:self.symbolConfiguration] forState:UIControlStateNormal];
  [self.button5 addTarget:self action:@selector(Button5Changed) forControlEvents:UIControlEventTouchUpInside];
  [self addSubview:self.button5];

  UIImageSymbolConfiguration *symbolConfiguration = [UIImageSymbolConfiguration configurationWithPointSize:fontSize weight:UIImageSymbolWeightBold];

  self.button6 = [UIButton buttonWithType:UIButtonTypeCustom];
  [self.button6 setImage:[UIImage systemImageNamed:@"gear" withConfiguration:symbolConfiguration] forState:UIControlStateNormal];
  self.button6.hidden = YES;
  [self.button6 addTarget:self action:@selector(Button6Changed) forControlEvents:UIControlEventTouchUpInside];
  [self addSubview:self.button6];

  self.button7 = [UIButton buttonWithType:UIButtonTypeCustom];
  self.button7.hidden = YES;
  self.button7.frame = CGRectMake(0, 0, buttonWidth, buttonWidth);
  [self.button7 setImage:[UIImage systemImageNamed:@"xmark" withConfiguration:symbolConfiguration] forState:UIControlStateNormal];
  [self.button7 addTarget:self action:@selector(Button7Changed) forControlEvents:UIControlEventTouchUpInside];
  [self addSubview:self.button7];

  self.button8 = [UIButton buttonWithType:UIButtonTypeCustom];
  self.button8.hidden = YES;
  self.button8.frame = CGRectMake(4 * buttonWidth, 0, buttonWidth, buttonWidth);
  [self.button8 setImage:[UIImage systemImageNamed:@"checkmark" withConfiguration:symbolConfiguration] forState:UIControlStateNormal];
  [self.button8 addTarget:self action:@selector(Button8Changed) forControlEvents:UIControlEventTouchUpInside];
  [self addSubview:self.button8];

  self.textField = [[UITextField alloc] initWithFrame:CGRectMake(buttonWidth, 0, buttonWidth * 3, buttonWidth)];
  self.textField.hidden = YES;
  self.textField.placeholder = @"0.1~999";
  self.textField.textAlignment = NSTextAlignmentCenter;
  self.textField.backgroundColor = self.backgroundColor;
  self.textField.textColor = self.tintColor;
  self.textField.borderStyle = UITextBorderStyleNone;
  self.textField.font = [UIFont systemFontOfSize:fontSize];
  [self addSubview:self.textField];

  UIPanGestureRecognizer *panGesture = [[UIPanGestureRecognizer alloc] initWithTarget:self action:@selector(handlePanGesture:)];
  [self addGestureRecognizer:panGesture];

  [self resetIdleTimer];

  return self;
}

- (void)resetIdleTimer {
  [self.idleTimer invalidate];
  self.idleTimer = [NSTimer scheduledTimerWithTimeInterval:5.0 target:self selector:@selector(hideSpeedView) userInfo:nil repeats:NO];
}

- (void)handlePanGesture:(UIPanGestureRecognizer *)gesture {
  CGPoint translation = [gesture translationInView:self.superview];
  if (gesture.state == UIGestureRecognizerStateBegan) {
    self.lastLocation = self.center;
    [self.idleTimer invalidate];
  }

  CGPoint newCenter = CGPointMake(self.lastLocation.x + translation.x, self.lastLocation.y + translation.y);

  CGFloat minX = self.bounds.size.width / 2;
  CGFloat maxX = self.superview.bounds.size.width - self.bounds.size.width / 2;
  CGFloat minY = self.bounds.size.height / 2;
  CGFloat maxY = self.superview.bounds.size.height - self.bounds.size.height / 2;

  newCenter.x = MAX(minX, MIN(newCenter.x, maxX));
  newCenter.y = MAX(minY, MIN(newCenter.y, maxY));

  self.center = newCenter;

  if (gesture.state == UIGestureRecognizerStateEnded) {
    self.lastLocation = self.center;
    [self resetIdleTimer];
  }
}

- (void)hideSpeedView {
  if (!self.button6.hidden || !self.textField.hidden) return;

  CGFloat buttonWidth = self.bounds.size.height;
  CGFloat newX = self.center.x < self.superview.bounds.size.width / 2 ? self.frame.origin.x : self.frame.origin.x + 4 * buttonWidth;

  void (^animations)(void) = ^{
    self.frame = CGRectMake(newX, self.frame.origin.y, buttonWidth, buttonWidth);
    self.alpha = 0.5;
    self.layer.cornerRadius = buttonWidth / 2;
  };

  void (^completion)(BOOL) = ^(BOOL finished) {
    self.button1.hidden = YES;
    self.button2.hidden = YES;
    self.button3.hidden = YES;
    self.button4.hidden = YES;
    self.button5.hidden = YES;
    self.button6.frame = self.bounds;
    self.button6.hidden = NO;
    [self resetIdleTimer];
  };

  [UIView animateWithDuration:0.4 animations:animations completion:completion];
}

- (void)Button1Changed {
  self.userInteractionEnabled = NO;

  if (self.button5.isSelected) {
    resetHook();
    [self.button5 setImage:[UIImage systemImageNamed:@"play.fill" withConfiguration:self.symbolConfiguration] forState:UIControlStateNormal];
    self.button5.selected = NO;
  }

  currentMod = (currentMod + 1) % modeSymbolsCount;

  [self.button1 setImage:[UIImage systemImageNamed:@(modeSymbols[currentMod]) withConfiguration:self.symbolConfiguration] forState:UIControlStateNormal];

  [self resetIdleTimer];

  self.userInteractionEnabled = YES;
}

- (void)Button2Changed {
  self.userInteractionEnabled = NO;
  if (speedValuesIndex > 0) {
    speedValuesIndex--;
    speedValue = speedValues[speedValuesIndex];
    if (self.button5.isSelected) updateSpeed(speedValue);
  }
  [self.button3 setTitle:[@(speedValue) stringValue] forState:UIControlStateNormal];
  [self resetIdleTimer];
  self.userInteractionEnabled = YES;
}

- (void)Button3Changed {
  self.userInteractionEnabled = NO;
  [self.idleTimer invalidate];

  self.button1.hidden = YES;
  self.button2.hidden = YES;
  self.button3.hidden = YES;
  self.button4.hidden = YES;
  self.button5.hidden = YES;
  self.button7.hidden = NO;
  self.button8.hidden = NO;
  self.textField.hidden = NO;
  [self.textField becomeFirstResponder];

  self.userInteractionEnabled = YES;
}

- (void)Button4Changed {
  self.userInteractionEnabled = NO;
  if (speedValuesIndex < speedValuesCount - 1) {
    speedValuesIndex++;
    speedValue = speedValues[speedValuesIndex];
    if (self.button5.isSelected) updateSpeed(speedValue);
  }
  [self.button3 setTitle:[@(speedValue) stringValue] forState:UIControlStateNormal];
  [self resetIdleTimer];
  self.userInteractionEnabled = YES;
}

- (void)Button5Changed {
  self.userInteractionEnabled = NO;

  if (self.button5.isSelected) {
    resetHook();
    [self.button5 setImage:[UIImage systemImageNamed:@"play.fill" withConfiguration:self.symbolConfiguration] forState:UIControlStateNormal];
  } else {
    initHook();
    updateSpeed(speedValue);
    [self.button5 setImage:[UIImage systemImageNamed:@"pause.fill" withConfiguration:self.symbolConfiguration] forState:UIControlStateNormal];
  }
  self.button5.selected = !self.button5.selected;

  [self resetIdleTimer];

  self.userInteractionEnabled = YES;
}

- (void)Button6Changed {
  self.userInteractionEnabled = NO;
  CGFloat buttonWidth = self.frame.size.width;
  CGFloat expandedWidth = buttonWidth * 5;
  CGFloat newX = self.center.x < self.superview.bounds.size.width / 2 ? self.frame.origin.x : self.frame.origin.x - 4 * buttonWidth;

  self.button1.hidden = NO;
  self.button2.hidden = NO;
  self.button3.hidden = NO;
  self.button4.hidden = NO;
  self.button5.hidden = NO;
  self.button6.hidden = YES;

  void (^animations)(void) = ^{
    self.frame = CGRectMake(newX, self.frame.origin.y, expandedWidth, self.frame.size.height);
    self.alpha = 1.0;
    self.layer.cornerRadius = buttonWidth / 2;
  };

  void (^completion)(BOOL) = ^(BOOL finished) {
    [self resetIdleTimer];
  };

  [UIView animateWithDuration:0.4 animations:animations completion:completion];

  self.userInteractionEnabled = YES;
}

- (void)Button7Changed {
  self.userInteractionEnabled = NO;
  [self.textField resignFirstResponder];
  self.button7.hidden = YES;
  self.button8.hidden = YES;
  self.textField.hidden = YES;
  self.button1.hidden = NO;
  self.button2.hidden = NO;
  self.button3.hidden = NO;
  self.button4.hidden = NO;
  self.button5.hidden = NO;
  [self resetIdleTimer];
  self.userInteractionEnabled = YES;
}

- (void)Button8Changed {
  self.userInteractionEnabled = NO;
  float inputValue = [self.textField.text floatValue];
  if (inputValue >= 0.1 && inputValue <= 999) {
    speedValue = inputValue;
    [self.button3 setTitle:[NSString stringWithFormat:@"%.2f", speedValue] forState:UIControlStateNormal];
    if (self.button5.isSelected) updateSpeed(speedValue);
  }

  [self.textField resignFirstResponder];

  self.button7.hidden = YES;
  self.button8.hidden = YES;
  self.textField.hidden = YES;
  self.button1.hidden = NO;
  self.button2.hidden = NO;
  self.button3.hidden = NO;
  self.button4.hidden = NO;
  self.button5.hidden = NO;

  [self resetIdleTimer];
  self.userInteractionEnabled = YES;
}

@end
