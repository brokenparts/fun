#ifndef _COMMON_MATH_HH_
#define _COMMON_MATH_HH_

#include "common_core.hh"

#include <cmath>

//
// Vector types
//

enum : u8 {
  VEC_AXIS_X = 0,
  VEC_AXIS_Y,
};

class Vec2 {
public:
  f32 x = 0.0f;
  f32 y = 0.0f;
public:
  Vec2() = default;
  Vec2(f32 x, f32 y)
    : x(x), y(y) { }

  Vec2 operator+(const Vec2& rhs) {
    return Vec2(x + rhs.x, y + rhs.y);
  }
  Vec2 operator-(const Vec2& rhs) {
    return Vec2(x - rhs.x, y - rhs.y);
  }
  Vec2 operator*(f32 scalar) {
    return Scale(scalar);
  }
  Vec2 operator/(f32 divisor) {
    if (divisor == 0.0f) {
      return *this;
    } else {
      return Scale(1.0f / divisor);
    }
  }

  Vec2 operator+=(const Vec2& rhs) {
    return (*this = *this + rhs);
  }
  Vec2 operator-=(const Vec2& rhs) {
    return (*this = *this - rhs);
  }
  Vec2 operator*=(f32 scalar) {
    return (*this = *this * scalar);
  }
  Vec2 operator/=(f32 divisor) {
    return (*this = *this / divisor);
  }

  inline f32 GetAxis(u8 axis) {
    switch (axis) {
    case VEC_AXIS_X: { return x; }
    case VEC_AXIS_Y: { return y; }
    default: { assert(0); }
    }
  }

  inline f32 Dot(const Vec2& rhs) const {
    return x * rhs.x + y * rhs.y;
  }

  inline f32 Length2() const {
    return Dot(*this);
  }

  inline f32 Length() const {
    return std::sqrtf(Length2());
  }

  inline Vec2 Normalize() const {
    const f32 length = Length();
    if (length >= 0.0f) {
      return Scale(1.0f / length);
    } else {
      return *this;
    }
  }

  inline Vec2 Scale(f32 scalar) const {
    return Vec2(x * scalar, y * scalar);
  }
};

#endif // _COMMON_MATH_HH_