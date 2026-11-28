#include "synth/wavetable_osc.h"

#include <cmath>

#include "config/audio_config.h"
#include "config/platform.h"
#include "dsp/fast_math.h"
#include "dsp/interpolate.h"

namespace synth {
namespace {

constexpr float kMaxHarmonicHz = 20000.0f;   // below Nyquist (24 kHz): no aliasing
constexpr float kScale = 1.0f / 32768.0f;

}  // namespace

// Called every block when pitch is modulated: SRAM, and no library calls
void AUDIO_HOT(WavetableOsc::set_pitch)(float semitones) {
  const float hz = dsp::note_to_hz(semitones);
  float cycles = hz * (1.0f / audio::kSampleRate);
  if (cycles > 0.49f) cycles = 0.49f;   // (std::fmin may become a library call)
  increment_ = static_cast<uint32_t>(cycles * 4294967296.0f);

  // The fullest level whose top harmonic stays below the limit at this pitch
  int l = 0;
  while (l < table_->levels - 1 && table_->level[l].harmonics * hz > kMaxHarmonicHz) ++l;
  level_ = &table_->level[l];
}

void AUDIO_HOT(WavetableOsc::render)(float* out, size_t n) {
  const uint32_t bits = level_->bits;
  const uint32_t stride = Wavetable::stride(bits);
  const int16_t* const base = level_->data;
  const int last_pair = table_->waves - 2;   // the lower wave of the last crossfade pair

  float pos = position_;
  const float step = (target_ - pos) / static_cast<float>(n);
  uint32_t phase = phase_;
  const uint32_t inc = increment_;

  if (scan_ == WaveScan::kSmooth) {
    for (size_t i = 0; i < n; ++i) {
      pos += step;
      int w = static_cast<int>(pos);
      if (w > last_pair) w = last_pair;
      const float wave_frac = pos - static_cast<float>(w);

      const uint32_t idx = phase >> (32 - bits);
      const dsp::Lagrange6 interp(static_cast<float>((phase << bits) >> 8) * (1.0f / 16777216.0f));
      const int16_t* w0 = base + w * stride + idx;
      const float a = interp(w0);
      const float b = interp(w0 + stride);
      out[i] = (a + (b - a) * wave_frac) * kScale;
      phase += inc;
    }
  } else {
    for (size_t i = 0; i < n; ++i) {
      pos += step;
      const int w = static_cast<int>(pos + 0.5f);   // nearest wave
      const uint32_t idx = phase >> (32 - bits);
      const dsp::Lagrange6 interp(static_cast<float>((phase << bits) >> 8) * (1.0f / 16777216.0f));
      out[i] = interp(base + w * stride + idx) * kScale;
      phase += inc;
    }
  }

  phase_ = phase;
  position_ = target_;
}

}  // namespace synth
