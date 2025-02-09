#include "common_core.hh"

#include <Foundation/Foundation.h>
#include <objc/NSObject.h>

#define Log(...) std::printf(__VA_ARGS__); std::printf("\n")

// #define DEMO1
// #define DEMO2
#define DEMO3

//
// Demo 1 - Alloc/Dealloc single object
//

#ifdef DEMO1

@interface User : NSObject

@property int user_id;

- (instancetype)initWithId:(int)user_id;

@end

@implementation User

+ (instancetype)alloc {
  Log("+ Allocated User");
  return [super alloc];
}

- (void)dealloc {
#if !__has_feature(objc_arc)
  [super dealloc];
#endif
  Log("- Deallocated User");
}

- (instancetype)initWithId:(int)user_id {
  if ((self = [super init])) {
    _user_id = user_id;
  }
  return self;
}

@end

void Run() {
  User* user1 = [[User alloc] initWithId:1];
  (void)user1;
#if !__has_feature(objc_arc)
  [user1 release];
#endif

  @autoreleasepool {
    User* user2 = [[User alloc] initWithId:2];
    (void)user2;
#if !__has_feature(objc_arc)
    [user2 autorelease];
#endif
  }
}

#endif // DEMO1

//
// Demo 2 - Allocate/deallocate object that owns another
//

#ifdef DEMO2

@interface ProfileAvatar : NSObject

@property int image_hash;

- (instancetype)init;

@end

@implementation ProfileAvatar

+ (instancetype)alloc {
  Log("+ Allocated ProfileAvatar");
  return [super alloc];
}

- (void)dealloc {
  Log("- Deallocated ProfileAvatar");
#if !__has_feature(objc_arc)
  [super dealloc];
#endif
}

- (instancetype)init {
  if ((self = [super init])) {
    _image_hash = 1;
  }
  return self;
}

@end

@interface Profile : NSObject

@property (nonatomic, retain) ProfileAvatar* avatar;

- (instancetype)initWithAvatar:(ProfileAvatar*)avatar;

@end

@implementation Profile

+ (instancetype)alloc {
  Log("+ Allocated Profile");
  return [super alloc];
}

- (void)dealloc {
#if !__has_feature(objc_arc)
  [_avatar release];
  [super dealloc];
#endif
  Log("- Deallocated Profile");
}

- (instancetype)initWithAvatar:(ProfileAvatar*)avatar {
  if ((self = [super init])) {
    _avatar = avatar;
  }
  return self;
}

@end

#include <objc/objc.h>

void Run() {
  ProfileAvatar* avatar1 = [[ProfileAvatar alloc] init];
  Profile* profile1 = [[Profile alloc] initWithAvatar:avatar1];
  (void)profile1;
#if !__has_feature(objc_arc)
  [profile1 release];
#endif

  @autoreleasepool {
    Profile* profile2 = [[Profile alloc] initWithAvatar:[[ProfileAvatar alloc] init]];
    (void)profile2;
#if !__has_feature(objc_arc)
    [profile2 autorelease];
#endif
  }
}

#endif // DEMO2

//
// Demo 3 - C-style wrapper around Objective-C class
//

#ifdef DEMO3

@interface MyThing : NSObject

@property (nonatomic, retain) NSString* name;

- (instancetype)initWithName:(NSString*)name;

@end

@implementation MyThing

+ (instancetype)alloc {
  Log("+ Allocated MyThing");
  return [super alloc];
}

- (void)dealloc {
#if !__has_feature(objc_arc)
  [_name release];
  [super dealloc];
#endif
  Log("- Deallocated MyThing");
}

- (instancetype)initWithName:(NSString*)name {
  if ((self = [super init])) {
    _name = name;
  }
  return self;
}

@end

struct MyThingWrapper {
  void* thing;
};

#if __has_feature(objc_arc)
# define THING_FROM_OPAQUE(ptr) ((__bridge MyThing*)(ptr))
#else
# define THING_FROM_OPAQUE(ptr) ((MyThing*)(ptr))
#endif

MyThingWrapper* MyThingWrapper_Create(const char* name) {
  MyThingWrapper* thing_wrapper = MemAllocZ<MyThingWrapper>();
  MyThing* t = [[MyThing alloc] initWithName:@(name)];
#if __has_feature(objc_arc)
  thing_wrapper->thing = (__bridge_retained void *)t;
#else
  thing_wrapper->thing = (void*)t;
#endif
  return thing_wrapper;
}

void MyThingWrapper_Destroy(MyThingWrapper* thing_wrapper) {
#if __has_feature(objc_arc)
  MyThing* thing = (__bridge_transfer MyThing*)thing_wrapper->thing;
  (void)thing;
#else
  MyThing* thing = (MyThing*)thing_wrapper->thing;
  [thing release];
#endif
  MemFree(thing_wrapper);
}

const char* MyThingWrapper_GetName(MyThingWrapper* thing_wrapper) {
  MyThing* thing = THING_FROM_OPAQUE(thing_wrapper->thing);
  return [[thing name] UTF8String];
  return "a";
}

void Run() {
  MyThingWrapper* thing = MyThingWrapper_Create("hello world");
  Log("Thing name: %s", MyThingWrapper_GetName(thing));
  MyThingWrapper_Destroy(thing);
}

#endif // DEMO3

int main(int argc, const char* argv[]) {
#if __has_feature(objc_arc)
  Log("ARC enabled");
#else
  Log("ARC disabled");
#endif
  Run();
  Log("Exiting");
}