#pragma once

#include <cstddef>

#include "dsp/fast_math.h"

namespace synth {

// Zero-delay-feedback state variable filter (trapezoidal integrators, as in
// A. Simper's "Linear Trap Integrated SVF"). Stable under fast modulation and
// self-oscillates near zero damping. Coefficients are set once per block.
class Svf {
 public:
  // cutoff_hz is clamped to 20 Hz .. 0.4 x sample rate; resonance 0..1
  inline void set(float cutoff_hz, float resonance, float sample_rate);
  // Same, with the cutoff already divided by the sample rate (0..0.4): saves a divide
  inline void set_normalized(float cutoff, float resonance);
  inline float process_lowpass(float in);

  // SEM-style multimode: `morph` sweeps low pass (0) -> notch (0.5) -> high pass (1);
  // `bandpass` overrides it with a band pass normalised to unity gain at the peak.
  inline void set_mode(float morph, bool bandpass);
  inline float process_multimode(float in);
  // In place over a buffer, state in registers: same result as per sample
  inline void process_multimode_block(float* x, size_t n);

  void reset() { ic1_ = ic2_ = 0.0f; }

 private:
  float a1_ = 1.0f, a2_ = 0.0f, a3_ = 0.0f, k_ = 2.0f;
  float lp_gain_ = 1.0f, bp_gain_ = 0.0f, hp_gain_ = 0.0f;
  float ic1_ = 0.0f, ic2_ = 0.0f;
};

inline void Svf::set(float cutoff_hz, float resonance, float sample_rate) {
  set_normalized(cutoff_hz / sample_rate, resonance);
}

inline void Svf::set_normalized(float cutoff, float resonance) {
  const float fc = dsp::clamp(cutoff, 20.0f / 48000.0f, 0.4f);
  const float g = dsp::fast_tan(3.14159265f * fc);
  // Damping k = 1/Q: 2 (no resonance) down to 0.02 (Q = 50, on the edge of oscillation)
  const float k = 2.0f - 1.98f * dsp::clamp(resonance, 0.0f, 1.0f);
  k_ = k;
  a1_ = 1.0f / (1.0f + g * (g + k));
  a2_ = g * a1_;
  a3_ = g * a2_;
}

inline float Svf::process_lowpass(float in) {
  const float v3 = in - ic2_;
  const float v1 = a1_ * ic1_ + a2_ * v3;          // band pass
  const float v2 = ic2_ + a2_ * ic1_ + a3_ * v3;   // low pass
  ic1_ = 2.0f * v1 - ic1_;
  ic2_ = 2.0f * v2 - ic2_;
  return v2;
}

inline void Svf::set_mode(float morph, bool bandpass) {
  const float m = dsp::clamp(morph, 0.0f, 1.0f);
  if (bandpass) {
    lp_gain_ = hp_gain_ = 0.0f;
    bp_gain_ = 1.0f;
  } else {
    // notch = low pass + high pass, so the sweep is continuous through it
    lp_gain_ = dsp::clamp(2.0f * (1.0f - m), 0.0f, 1.0f);
    hp_gain_ = dsp::clamp(2.0f * m, 0.0f, 1.0f);
    bp_gain_ = 0.0f;
  }
}

inline float Svf::process_multimode(float in) {
  const float v3 = in - ic2_;
  const float v1 = a1_ * ic1_ + a2_ * v3;          // band pass (peak gain 1/k)
  const float v2 = ic2_ + a2_ * ic1_ + a3_ * v3;   // low pass
  ic1_ = 2.0f * v1 - ic1_;
  ic2_ = 2.0f * v2 - ic2_;
  const float hp = in - k_ * v1 - v2;
  return lp_gain_ * v2 + bp_gain_ * k_ * v1 + hp_gain_ * hp;
}

inline void Svf::process_multimode_block(float* x, size_t n) {
  const float a1 = a1_, a2 = a2_, a3 = a3_, k = k_;
  const float gl = lp_gain_, gb = bp_gain_ * k_, gh = hp_gain_;
  float ic1 = ic1_, ic2 = ic2_;
  for (size_t i = 0; i < n; ++i) {
    const float in = x[i];
    const float v3 = in - ic2;
    const float v1 = a1 * ic1 + a2 * v3;
    const float v2 = ic2 + a2 * ic1 + a3 * v3;
    ic1 = 2.0f * v1 - ic1;
    ic2 = 2.0f * v2 - ic2;
    x[i] = gl * v2 + gb * v1 + gh * (in - k * v1 - v2);
  }
  ic1_ = ic1;
  ic2_ = ic2;
}

}  // namespace synth
