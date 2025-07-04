#import "LuckySpeeder.h"
#import <SpriteKit/SpriteKit.h>
#import <objc/runtime.h>

static float SKScene_update_speed = 1.0;

static void (*original_SKScene_update)(id, SEL, NSTimeInterval) = NULL;

static void my_SKScene_update(id self, SEL _cmd, NSTimeInterval currentTime) {
  if ([self isKindOfClass:[SKScene class]]) {
    SKScene *scene = (SKScene *)self;
    if (scene.physicsWorld) {
      scene.physicsWorld.speed = SKScene_update_speed;
      [scene enumerateChildNodesWithName:@"//*"
                              usingBlock:^(SKNode *node, BOOL *stop) {
                                if ([node hasActions]) [node setSpeed:SKScene_update_speed];
                              }];
    }
  }
}

int hook_SKScene_update(void) {
  if (original_SKScene_update) return 0;

  Class gameSceneClass = nil;
  int numClasses = objc_getClassList(NULL, 0);

  if (numClasses > 0) {
    Class *classes = (__unsafe_unretained Class *)malloc(sizeof(Class) * numClasses);
    numClasses = objc_getClassList(classes, numClasses);
    for (int i = 0; i < numClasses; i++) {
      Class class = classes[i];
      if (class_getSuperclass(class) == [SKScene class] && [NSStringFromClass(class) hasSuffix:@"GameScene"])
        if (class_getInstanceMethod(class, @selector(update:))) {
          gameSceneClass = class;
          break;
        }
    }
    free(classes);
  }

  if (gameSceneClass) {
    Method updateMethod = class_getInstanceMethod(gameSceneClass, @selector(update:));
    if (updateMethod) {
      original_SKScene_update = (void (*)(id, SEL, NSTimeInterval))method_getImplementation(updateMethod);
      method_setImplementation(updateMethod, (IMP)my_SKScene_update);
      return 0;
    }
  }

  return -1;
}

void set_SKScene_update(float value) { SKScene_update_speed = value; }

void reset_SKScene_update(void) { set_SKScene_update(1.0); }