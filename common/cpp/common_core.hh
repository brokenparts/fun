#ifndef _COMMON_CORE_HH_
#define _COMMON_CORE_HH_

//
// Platform detection
//

#if defined(_WIN32)
# define FUN_WINDOWS
#elif defined(__APPLE__)
# define FUN_APPLE
#elif defined(__linux__)
# define FUN_LINUX
#else
# error Unknown platform
#endif

#if defined(FUN_APPLE) || defined(FUN_LINUX)
# define FUN_POSIX
#endif

//
// Other headers
//

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#ifdef FUN_POSIX
# include <unistd.h>
#endif

#ifdef FUN_WINDOWS
# include <Windows.h>
# include <direct.h>
#endif

//
// Fixed-size integer types
//

using u8  = std::uint8_t;
using u16 = std::uint16_t;
using u32 = std::uint32_t;
using u64 = std::uint64_t;
using i8  = std::int8_t;
using i16 = std::int16_t;
using i32 = std::int32_t;
using i64 = std::int64_t;

using f32 = float;
using f64 = double;

using usize = std::size_t;

//
// Ranges
//

constexpr u32 U32_MIN = 0;
constexpr u32 U32_MAX = 0xFFFFFFFF;

//
// Core type helpers
//

template <typename T>
static inline T Min(T val1, T val2) {
  if (val1 < val2) {
    return val1;
  } else {
    return val2;
  }
}

template <typename T>
static inline T Max(T val1, T val2) {
  if (val1 > val2) {
    return val1;
  } else {
    return val2;
  }
}

template <typename T>
static inline T Clamp(T val, T minval, T maxval) {
  return Min(Max(val, minval), maxval);
}

template <typename T>
static inline bool InRange(T val, T minval, T maxval) {
  return val >= minval && val <= maxval;
}

template <typename T>
static inline void Swap(T& left, T& right) {
  T tmp = left;
  left = right;
  right = tmp;
}

static inline f32 Remap(f32 val, f32 min1, f32 max1, f32 min2, f32 max2) {
  return min2 + (val - min1) * (max2 - min2) / (max1 - min1);
}

static inline f32 RemapClamp(f32 val, f32 min1, f32 max1, f32 min2, f32 max2) {
  return Clamp(Remap(val, min1, max1, min2, max2), min2, max2);
}

//
// Memory allocation
//

template <typename T>
static inline T* MemAlloc(usize count = 1) {
  T* result = (T*)std::malloc(sizeof(T) * count);
  assert(result);
  return result;
}

template <typename T>
static inline T* MemAllocZ(usize count = 1) {
  T* result = (T*)std::calloc(sizeof(T), count);
  assert(result);
  return result;
}

template <typename T>
static inline void MemFree(T* ptr) {
  std::free(ptr);
}

//
// CWD reset
//

static inline void _ResetCWDToSourceDir(const char* source_file) {
  char buf[1024] = { };
  std::strncpy(buf, source_file, sizeof(buf));
  char* last_delim = 0;
  for (usize i = 0; i < sizeof(buf); ++i) {
    if (buf[i] == '/' || buf[i] == '\\') {
      last_delim = &buf[i];
    }
  }
  if (last_delim) {
    *last_delim = '\0';
  }
  std::printf("Changing working directory to %s: ", buf);
  if (chdir(buf) == 0) {
    std::printf("Success\n");
  } else {
    std::printf("Failed\n");
  }
}

#define RESET_CWD_TO_SOURCE_DIR() _ResetCWDToSourceDir(__FILE__)

#endif // _COMMON_CORE_HH_
