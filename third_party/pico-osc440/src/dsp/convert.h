#pragma once

#include <cstddef>
#include <cstdint>

namespace dsp {

// Float (-1..+1) to signed 32-bit, clipped. The clamp keeps the conversion defined.
inline int32_t float_to_s32(float x) {
  if (x > 0.99999994f) x = 0.99999994f;   // largest float below 1
  if (x < -1.0f) x = -1.0f;
  return static_cast<int32_t>(x * 2147483648.0f);
}

// Two channels into one interleaved L/R buffer of 2 * frames words.
inline void interleave_s32(const float* l, const float* r, int32_t* dst, size_t frames) {
  for (size_t i = 0; i < frames; ++i) {
    dst[2 * i + 0] = float_to_s32(l[i]);
    dst[2 * i + 1] = float_to_s32(r[i]);
  }
}

}  // namespace dsp
