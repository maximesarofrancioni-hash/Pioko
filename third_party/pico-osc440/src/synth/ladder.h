#pragma once

#include <cstddef>

#include "config/platform.h"
#include "dsp/fast_math.h"
#include "dsp/halfband.h"

namespace synth {

// 4-pole ladder low pass (24 dB/oct), a digital stand-in for an analogue OTA
// ladder such as the 2140. Four trapezoidal one-pole stages with the global
// feedback solved without delay (Zavalishin), and a tanh-like saturation
// (dsp::soft_clip) on the feedback node: the loop is solved linearly, then the
// stage input saturates.
// Self-oscillates near full resonance; saturation keeps it bounded.
// Resonance compensation restores the bass that resonance takes away
// (a ladder's passband gain falls to 1 / (1 + k)).
class Ladder {
 public:
  // cutoff = fc / fs (clamped to 0.0002 .. 0.4); resonance 0..1;
  // drive >= 1 saturates earlier (small signals pass at unity, the clip level is
  // 1 / drive); compensation 0..1.
  inline void set(float cutoff, float resonance, float drive, float compensation);
  inline float process(float in);
  // In place over a buffer: same result as process() per sample, with the state
  // in registers for the whole block
  inline void process_block(float* x, size_t n);
  void reset() { s1_ = s2_ = s3_ = s4_ = 0.0f; }

 private:
  float G_ = 0.0f, h_ = 1.0f;                 // stage gain g/(1+g), and 1/(1+g)
  float G2_ = 0.0f, G3_ = 0.0f, G4_ = 0.0f;
  float G3h_ = 0.0f, G2h_ = 0.0f, Gh_ = 0.0f;   // products used per sample
  float k_ = 0.0f, inv_den_ = 1.0f, in_gain_ = 1.0f;
  float drive_ = 1.0f, inv_drive_ = 1.0f;
  float s1_ = 0.0f, s2_ = 0.0f, s3_ = 0.0f, s4_ = 0.0f;
};

inline void Ladder::set(float cutoff, float resonance, float drive, float compensation) {
  const float fc = dsp::clamp(cutoff, 0.0002f, 0.4f);
  const float g = dsp::fast_tan(3.14159265f * fc);
  h_ = 1.0f / (1.0f + g);
  G_ = g * h_;
  G2_ = G_ * G_;
  G3_ = G2_ * G_;
  G4_ = G3_ * G_;
  G3h_ = G3_ * h_;
  G2h_ = G2_ * h_;
  Gh_ = G_ * h_;
  k_ = 4.2f * dsp::clamp(resonance, 0.0f, 1.0f);   // oscillation starts at k = 4
  inv_den_ = 1.0f / (1.0f + k_ * G4_);
  in_gain_ = 1.0f + dsp::clamp(compensation, 0.0f, 1.0f) * k_;
  drive_ = drive < 1.0f ? 1.0f : drive;
  inv_drive_ = 1.0f / drive_;
}

namespace ladder_detail {

// One sample through the loop; the states are references so the block version
// can keep them in locals (registers)
struct Coefs {
  float G, h, G4, G3h, G2h, Gh, k, inv_den, in_gain, drive, inv_drive;
};

inline float step(const Coefs& c, float in, float& s1, float& s2, float& s3, float& s4) {
  const float u = in * c.in_gain;
  // Each stage is y = G x + h s: solve the loop for y4 with the states as they are
  const float sigma = c.G3h * s1 + c.G2h * s2 + c.Gh * s3 + c.h * s4;
  const float y4 = (c.G4 * u + sigma) * c.inv_den;
  const float x = dsp::soft_clip((u - c.k * y4) * c.drive) * c.inv_drive;
  float v = (x - s1) * c.G;
  const float y1 = v + s1;
  s1 = y1 + v;
  v = (y1 - s2) * c.G;
  const float y2 = v + s2;
  s2 = y2 + v;
  v = (y2 - s3) * c.G;
  const float y3 = v + s3;
  s3 = y3 + v;
  v = (y3 - s4) * c.G;
  const float out = v + s4;
  s4 = out + v;
  return out;
}

}  // namespace ladder_detail

inline float Ladder::process(float in) {
  const ladder_detail::Coefs c{G_, h_, G4_, G3h_, G2h_, Gh_, k_, inv_den_, in_gain_, drive_, inv_drive_};
  return ladder_detail::step(c, in, s1_, s2_, s3_, s4_);
}

AUDIO_HOT_ATTR inline void Ladder::process_block(float* x, size_t n) {
  const ladder_detail::Coefs c{G_, h_, G4_, G3h_, G2h_, Gh_, k_, inv_den_, in_gain_, drive_, inv_drive_};
  float s1 = s1_, s2 = s2_, s3 = s3_, s4 = s4_;
  for (size_t i = 0; i < n; ++i) x[i] = ladder_detail::step(c, x[i], s1, s2, s3, s4);
  s1_ = s1;
  s2_ = s2;
  s3_ = s3;
  s4_ = s4;
}

// The same ladder run at twice the sample rate, so the saturation's harmonics
// up to 48 kHz are filtered out instead of folding back as aliasing. Roughly
// doubles the ladder's cost, plus 14 multiplies for the two halfband filters.
class OversampledLadder {
 public:
  // cutoff = fc / fs at the base rate (up to 0.2 x 2 = 0.4 of the oversampled rate)
  void set(float cutoff, float resonance, float drive, float compensation) {
    ladder_.set(0.5f * dsp::clamp(cutoff, 0.0f, 0.4f), resonance, drive, compensation);
  }
  float process(float in) {
    float up[2];
    up_.process(in, up);
    const float a = ladder_.process(up[0]);
    const float b = ladder_.process(up[1]);
    return down_.process(a, b);
  }
  // n samples (n <= 32) in to out: same result as process() per sample
  AUDIO_HOT_ATTR void process_block(const float* in, float* out, size_t n) {
    float up[2 * dsp::halfband::kMaxBlock];
    up_.process_block(in, up, n);
    ladder_.process_block(up, 2 * n);
    down_.process_block(up, out, n);
  }

 private:
  Ladder ladder_;
  dsp::Upsampler2x up_;
  dsp::Downsampler2x down_;
};

}  // namespace synth
