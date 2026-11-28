#pragma once

#include <cstddef>

namespace dsp {

// 2x oversampling with a polyphase IIR halfband filter: two chains of
// first-order allpass sections in z^-2, H(z) = (A(z^2) + z^-1 B(z^2)) / 2
// (Valenzuela & Constantinides; the structure of de Soras' HIIR library).
// The 7 coefficients come from the elliptic halfband design for a passband to
// 20 kHz and a stopband from 28 kHz at 96 kHz: measured flat to 20 kHz,
// -88 dB in the stopband. Each direction costs 7 multiplies per base-rate sample.
// Not linear phase, which does not matter inside a filter.
namespace halfband {

constexpr int kCoefs = 7;
constexpr float kCoef[kCoefs] = {0.0506389195f, 0.1842043130f, 0.3589579757f, 0.5353462365f,
                                 0.6914925242f, 0.8238403803f, 0.9414809388f};

constexpr size_t kMaxBlock = 32;   // base-rate samples per process_block call

struct Allpass {
  float x1 = 0.0f, y1 = 0.0f;
  float process(float x, float c) {   // y[n] = c (x[n] - y[n-1]) + x[n-1]
    const float y = c * (x - y1) + x1;
    x1 = x;
    y1 = y;
    return y;
  }
  // The same over a whole buffer, in place: state and coefficient stay in
  // registers instead of a load and store per sample and section
  void process_block(float* x, size_t n, float c) {
    float px = x1, py = y1;
#pragma GCC unroll 4
    for (size_t i = 0; i < n; ++i) {
      const float in = x[i];
      py = c * (in - py) + px;
      px = in;
      x[i] = py;
    }
    x1 = px;
    y1 = py;
  }
};

}  // namespace halfband

class Upsampler2x {
 public:
  // One sample in, two out (earlier first)
  void process(float in, float out[2]) {
    float a = in, b = in;
    for (int i = 0; i < halfband::kCoefs; i += 2) a = ap_[i].process(a, halfband::kCoef[i]);
    for (int i = 1; i < halfband::kCoefs; i += 2) b = ap_[i].process(b, halfband::kCoef[i]);
    out[0] = a;
    out[1] = b;
  }

  // n samples in, 2n out: same result as n calls of process(). Sample by
  // sample through all seven sections, every state in a register for the block
  // (section by section over the buffer would load and store each sample 7 times)
  void process_block(const float* in, float* out, size_t n) {
    using namespace halfband;
    float x0 = ap_[0].x1, y0 = ap_[0].y1, x2 = ap_[2].x1, y2 = ap_[2].y1;
    float x4 = ap_[4].x1, y4 = ap_[4].y1, x6 = ap_[6].x1, y6 = ap_[6].y1;
    float x1 = ap_[1].x1, y1 = ap_[1].y1, x3 = ap_[3].x1, y3 = ap_[3].y1;
    float x5 = ap_[5].x1, y5 = ap_[5].y1;
    for (size_t i = 0; i < n; ++i) {
      const float s = in[i];
      // Even sections: the earlier output
      y0 = kCoef[0] * (s - y0) + x0;  x0 = s;
      y2 = kCoef[2] * (y0 - y2) + x2; x2 = y0;
      y4 = kCoef[4] * (y2 - y4) + x4; x4 = y2;
      y6 = kCoef[6] * (y4 - y6) + x6; x6 = y4;
      // Odd sections: the later output
      y1 = kCoef[1] * (s - y1) + x1;  x1 = s;
      y3 = kCoef[3] * (y1 - y3) + x3; x3 = y1;
      y5 = kCoef[5] * (y3 - y5) + x5; x5 = y3;
      out[2 * i] = y6;
      out[2 * i + 1] = y5;
    }
    ap_[0] = {x0, y0}; ap_[2] = {x2, y2}; ap_[4] = {x4, y4}; ap_[6] = {x6, y6};
    ap_[1] = {x1, y1}; ap_[3] = {x3, y3}; ap_[5] = {x5, y5};
  }

 private:
  halfband::Allpass ap_[halfband::kCoefs];
};

class Downsampler2x {
 public:
  // Two samples in (earlier first), one out
  float process(float earlier, float later) {
    float a = later, b = earlier;
    for (int i = 0; i < halfband::kCoefs; i += 2) a = ap_[i].process(a, halfband::kCoef[i]);
    for (int i = 1; i < halfband::kCoefs; i += 2) b = ap_[i].process(b, halfband::kCoef[i]);
    return 0.5f * (a + b);
  }

  // 2n samples in, n out: same result as n calls of process(), states in
  // registers as in the upsampler
  void process_block(const float* in, float* out, size_t n) {
    using namespace halfband;
    float x0 = ap_[0].x1, y0 = ap_[0].y1, x2 = ap_[2].x1, y2 = ap_[2].y1;
    float x4 = ap_[4].x1, y4 = ap_[4].y1, x6 = ap_[6].x1, y6 = ap_[6].y1;
    float x1 = ap_[1].x1, y1 = ap_[1].y1, x3 = ap_[3].x1, y3 = ap_[3].y1;
    float x5 = ap_[5].x1, y5 = ap_[5].y1;
    for (size_t i = 0; i < n; ++i) {
      const float later = in[2 * i + 1], earlier = in[2 * i];
      y0 = kCoef[0] * (later - y0) + x0; x0 = later;
      y2 = kCoef[2] * (y0 - y2) + x2;    x2 = y0;
      y4 = kCoef[4] * (y2 - y4) + x4;    x4 = y2;
      y6 = kCoef[6] * (y4 - y6) + x6;    x6 = y4;
      y1 = kCoef[1] * (earlier - y1) + x1; x1 = earlier;
      y3 = kCoef[3] * (y1 - y3) + x3;    x3 = y1;
      y5 = kCoef[5] * (y3 - y5) + x5;    x5 = y3;
      out[i] = 0.5f * (y6 + y5);
    }
    ap_[0] = {x0, y0}; ap_[2] = {x2, y2}; ap_[4] = {x4, y4}; ap_[6] = {x6, y6};
    ap_[1] = {x1, y1}; ap_[3] = {x3, y3}; ap_[5] = {x5, y5};
  }

 private:
  halfband::Allpass ap_[halfband::kCoefs];
};

}  // namespace dsp
