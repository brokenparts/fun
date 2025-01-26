#ifndef _COMMON_DSA_HH_
#define _COMMON_DSA_HH_

#include "common_core.hh"

//
// Xorshift RNG
// https://en.wikipedia.org/wiki/Xorshift
//
class Xorshift {
private:
  u32 state;
public:
  Xorshift()
    : state(0xB16B00B5) { }
  Xorshift(u32 state)
    : state(state) { }

  f32 RandomFloat(f32 fmax = 1.0f) {
    return RandomFloat(0.0f, fmax);
  }

  f32 RandomFloat(f32 fmin, f32 fmax) {
    return (fmax - fmin) * ((f32)Shift() / (f32)U32_MAX) + fmin;
  }
private:
  u32 Shift() {
    state ^= state << 13;
    state ^= state >> 17;
    state ^= state << 5;
    return state;
  }
};

#endif // _COMMON_DSA_HH_
