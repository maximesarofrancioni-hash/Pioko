#pragma once

#include <cstdint>
#include <cstring>

// Approximations for the audio path: no library calls, no doubles, branch-light.
namespace dsp {

// Branch-free: on the M33 these builtins are single VMAXNM / VMINNM instructions
inline float clamp(float x, float lo, float hi) { return __builtin_fminf(__builtin_fmaxf(x, lo), hi); }

// 2^x, within 0.01 cent over the whole range (degree-4 fit, exact at integers).
inline float fast_exp2(float x) {
  x = clamp(x, -126.0f, 126.0f);
  int i = static_cast<int>(x);
  if (x < static_cast<float>(i)) --i;   // floor for negatives
  const float f = x - static_cast<float>(i);
  const float p = 1.0f + f * (0.69299566f + f * (0.24156560f + f * (0.05175228f + f * 0.01368647f)));
  const uint32_t bits = static_cast<uint32_t>(i + 127) << 23;
  float scale;
  std::memcpy(&scale, &bits, sizeof(scale));
  return p * scale;
}

// tan(x) for 0 <= x <= 1.26 (filter prewarp up to 0.4 x sample rate):
// Pade [3/2], within 0.1% up to 15 kHz at 48 kHz and 0.8% at the cap.
inline float fast_tan(float x) {
  const float x2 = x * x;
  return x * (15.0f - x2) / (15.0f - 6.0f * x2);
}

// Semitones (MIDI note number) to Hz
inline float note_to_hz(float note) { return 440.0f * fast_exp2((note - 69.0f) * (1.0f / 12.0f)); }

// tanh(x): rational approximation, exact at 0 and at +/-3 where it reaches +/-1
// (clamped beyond). Smooth and monotonic: fine for saturation stages.
inline float fast_tanh(float x) {
  x = clamp(x, -3.0f, 3.0f);
  const float x2 = x * x;
  return x * (27.0f + x2) / (27.0f + 9.0f * x2);
}

// Cubic soft clip, tanh-like: slope 1 at 0, reaches exactly +/-1 at +/-1.5 with
// zero slope, flat beyond. No divide (fast_tanh needs one: 14 cycles on the M33),
// and below the clip point it only adds a 3rd harmonic, so it aliases less.
inline float soft_clip(float x) {
  const float c = clamp(x, -1.5f, 1.5f);
  return c - (4.0f / 27.0f) * c * c * c;
}

// sin(2 pi phase) for any phase in cycles, within ~0.1% (parabola + one refinement).
inline float fast_sin_cycles(float phase) {
  int i = static_cast<int>(phase + 0.5f);
  if (phase + 0.5f < static_cast<float>(i)) --i;      // floor
  const float t = phase - static_cast<float>(i);       // -0.5 .. 0.5
  const float y = 8.0f * t - 16.0f * t * (t < 0.0f ? -t : t);
  return 0.225f * (y * (y < 0.0f ? -y : y) - y) + y;
}

}  // namespace dsp
