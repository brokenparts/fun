#ifndef _COMMON_CORE_HH_
#define _COMMON_CORE_HH_

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstdlib>

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

#endif // _COMMON_CORE_HH_