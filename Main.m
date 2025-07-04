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
#import <objc/runtime.h>

extern UIApplication *UIApp;
static LuckySpeederView *luckyspeederview;

@interface UIWindow (LuckySpeeder)
- (void)swizzled_bringSubviewToFront:(UIView *)view;
- (void)swizzled_addSubview:(UIView *)view;
@end

static void (*original_bringSubviewToFront)(UIWindow *self, SEL _cmd,
                                            UIView *view) = NULL;
static void (*original_addSubview)(UIWindow *self, SEL _cmd,
                                   UIView *view) = NULL;

static void my_bringSubviewToFront(UIWindow *self, SEL _cmd, UIView *view) {
  original_bringSubviewToFront(self, _cmd, view);
  if (luckyspeederview && view != luckyspeederview)
    [self bringSubviewToFront:luckyspeederview];
}

static void my_addSubview(UIWindow *self, SEL _cmd, UIView *view) {
  original_addSubview(self, _cmd, view);
  if (luckyspeederview && view != luckyspeederview)
    [self bringSubviewToFront:luckyspeederview];
}

static void swizzleMethod(Class class, SEL originalSelector, SEL swizzledSelector, IMP swizzledImplementation, IMP *originalImplementation) {
  Method originalMethod = class_getInstanceMethod(class, originalSelector);
  Method swizzledMethod = class_getInstanceMethod(class, swizzledSelector);

  if (class_addMethod(class, originalSelector, swizzledImplementation, method_getTypeEncoding(swizzledMethod))) {
    class_replaceMethod(class, swizzledSelector, method_getImplementation(originalMethod), method_getTypeEncoding(originalMethod));
  } else {
    method_exchangeImplementations(originalMethod, swizzledMethod);
  }
  *originalImplementation = method_getImplementation(originalMethod);
}

static id scenesWindowIMP(id self, SEL _cmd) { return nil; }

static void injectLuckySpeederView(void) {
  Class windowClass = objc_getClass("UIWindow");
  swizzleMethod(windowClass, @selector(bringSubviewToFront:), @selector(swizzled_bringSubviewToFront:), (IMP)my_bringSubviewToFront, (IMP *)&original_bringSubviewToFront);
  swizzleMethod(windowClass, @selector(addSubview:), @selector(swizzled_addSubview:), (IMP)my_addSubview, (IMP *)&original_addSubview);
  if (![[UIApp connectedScenes] respondsToSelector:@selector(window)]) {
    Class scenesClass = [[UIApp connectedScenes] class];
    class_addMethod(scenesClass, @selector(window), (IMP)scenesWindowIMP, "@@:");
  }

  UIWindow *keyWindow = nil;
  for (UIScene *scene in UIApp.connectedScenes) {
    if ([scene isKindOfClass:[UIWindowScene class]]) {
      UIWindowScene *windowScene = (UIWindowScene *)scene;
      for (UIWindow *window in windowScene.windows) {
        if (window.isKeyWindow) {
          keyWindow = window;
          break;
        }
      }
    }
    if (keyWindow)
      break;
  }

  static dispatch_once_t onceToken;
  dispatch_once(&onceToken, ^{
    luckyspeederview = [[LuckySpeederView alloc] initWithSize:keyWindow.bounds.size];
  });

  if (!luckyspeederview.superview && [[UIApp connectedScenes] respondsToSelector:@selector(window)]) {
    [keyWindow addSubview:luckyspeederview];
    [keyWindow bringSubviewToFront:luckyspeederview];
  }
}

static void UIApplicationDidFinishLaunching(CFNotificationCenterRef center, void *observer, CFStringRef name, const void *object, CFDictionaryRef userInfo) {
  dispatch_after(dispatch_time(DISPATCH_TIME_NOW, 1 * NSEC_PER_SEC),
                 dispatch_get_main_queue(), ^{
                   injectLuckySpeederView();
                 });
}

__attribute__((constructor)) static void initialize(void) {
  CFNotificationCenterAddObserver(
      CFNotificationCenterGetLocalCenter(), NULL,
      UIApplicationDidFinishLaunching,
      (CFStringRef)UIApplicationDidFinishLaunchingNotification, NULL,
      CFNotificationSuspensionBehaviorCoalesce);
}
