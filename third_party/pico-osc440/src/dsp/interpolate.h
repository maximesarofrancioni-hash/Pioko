#pragma once

#include <array>
#include <cstdint>
#include <cstring>

#if defined(__ARM_FEATURE_DSP) && __ARM_FEATURE_DSP
#include <arm_acle.h>
#endif

namespace dsp {

// 6-point Lagrange interpolation weights for the points at offsets -2..3
// around position t (0 <= t < 1 past offset 0). The polynomial goes exactly
// through all 6 points; on 8x-oversampled tables it keeps interpolation images
// below -70 dB, where linear interpolation leaves them as high as -35 dB.
struct Lagrange6 {
  float w[6];

  explicit Lagrange6(float t) {
    const float d0 = t + 2.0f, d1 = t + 1.0f, d2 = t, d3 = t - 1.0f, d4 = t - 2.0f, d5 = t - 3.0f;
    // Products of all distances but one, from prefix and suffix products
    const float p01 = d0 * d1, p012 = p01 * d2, p0123 = p012 * d3, p01234 = p0123 * d4;
    const float s45 = d4 * d5, s345 = d3 * s45, s2345 = d2 * s345, s12345 = d1 * s2345;
    w[0] = s12345 * (-1.0f / 120.0f);
    w[1] = d0 * s2345 * (1.0f / 24.0f);
    w[2] = p01 * s345 * (-1.0f / 12.0f);
    w[3] = p012 * s45 * (1.0f / 12.0f);
    w[4] = p0123 * d5 * (-1.0f / 24.0f);
    w[5] = p01234 * (1.0f / 120.0f);
  }

  // x points at offset 0; x[-2] .. x[3] must be readable
  float operator()(const int16_t* x) const {
    return w[0] * x[-2] + w[1] * x[-1] + w[2] * x[0] + w[3] * x[1] + w[4] * x[2] + w[5] * x[3];
  }
};

// Two 16 x 16 multiplies added to acc: lo(x) lo(y) + hi(x) hi(y) + acc. One
// SMLAD instruction on the M33; the same integer result everywhere else.
inline int32_t smlad(uint32_t x, uint32_t y, int32_t acc) {
#if defined(__ARM_FEATURE_DSP) && __ARM_FEATURE_DSP
  return __smlad(x, y, acc);
#else
  return acc + static_cast<int32_t>(static_cast<int16_t>(x)) * static_cast<int16_t>(y) +
         static_cast<int32_t>(static_cast<int16_t>(x >> 16)) * static_cast<int16_t>(y >> 16);
#endif
}

// Two adjacent int16 as one 32-bit word (first in the low half); the M33 reads
// it with one unaligned LDR
inline uint32_t load_pair(const int16_t* p) {
  uint32_t v;
  std::memcpy(&v, p, sizeof v);
  return v;
}

// Lagrange6 in fixed point for int16 tables: the weights for 2^kBits positions
// between two samples, precomputed in Q15 and packed in pairs, so a 6-point
// interpolation is three loads and three SMLADs, with no conversions.
// Position quantisation (+/- 1/2^(kBits+1) sample) and Q15 weights keep the
// error below -80 dB on the 8x-oversampled tables.
struct Lagrange6Q15 {
  static constexpr int kBits = 11;
  static constexpr int kSteps = 1 << kBits;
  struct Weights { uint32_t w01, w23, w45; };
  static const std::array<Weights, kSteps> kTable;

  // frac: the position between samples as a 32-bit fraction
  explicit Lagrange6Q15(uint32_t frac) : w(kTable[frac >> (32 - kBits)]) {}

  // x points at offset 0; x[-2] .. x[3] readable. Result: Q15 x sample units
  int32_t operator()(const int16_t* x) const {
    int32_t acc = smlad(load_pair(x - 2), w.w01, 0);
    acc = smlad(load_pair(x), w.w23, acc);
    return smlad(load_pair(x + 2), w.w45, acc);
  }

  const Weights& w;
};

}  // namespace dsp
