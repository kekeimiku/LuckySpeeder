#import "LuckySpeeder.h"
#import <SpriteKit/SpriteKit.h>
#import <objc/runtime.h>

static float SKScene_update_speed = 1.0;

static void (*original_SKScene_update)(id, SEL, NSTimeInterval) = NULL;

static void my_SKScene_update(id self, SEL cmd, NSTimeInterval currentTime) {
  if ([self isKindOfClass:[SKScene class]]) {
    SKScene *scene = (SKScene *)self;
    scene.speed = SKScene_update_speed;
    if (scene.physicsWorld) scene.physicsWorld.speed = SKScene_update_speed;
  }
  if (original_SKScene_update) original_SKScene_update(self, cmd, currentTime);
}

BOOL classNameHasSuffix(Class cls, const char *suffix) {
  const char *name = class_getName(cls);
  size_t nlen = strlen(name);
  size_t slen = strlen(suffix);
  if (nlen < slen) return NO;
  return strcmp(name + nlen - slen, suffix) == 0;
}

int hook_SKScene_update(void) {
  if (original_SKScene_update) return 0;

  int numClasses = objc_getClassList(NULL, 0);
  if (numClasses < 1) return -1;

  unsigned int numClassesUnsigned = (unsigned int)numClasses;

  Class *classes = objc_copyClassList(&numClassesUnsigned);
  if (!classes) return -1;

  for (unsigned int i = 0; i < numClassesUnsigned; i++) {
    Class cls = classes[i];

    if (class_getSuperclass(cls) != [SKScene class]) continue;

    if (!classNameHasSuffix(cls, "GameScene")) continue;

    Method updateMethod = class_getInstanceMethod(cls, @selector(update:));
    if (updateMethod) {
      original_SKScene_update = (void (*)(id, SEL, NSTimeInterval))method_getImplementation(updateMethod);
      method_setImplementation(updateMethod, (IMP)my_SKScene_update);
      free(classes);
      return 0;
    }
  }

  free(classes);

  return -1;
}

void set_SKScene_update(float value) { SKScene_update_speed = value; }

void reset_SKScene_update(void) { set_SKScene_update(1.0); }
