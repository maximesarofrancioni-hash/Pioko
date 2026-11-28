#pragma once

#include <cstddef>
#include <cstdint>

#include "synth/saw_tables.h"
#include "synth/wavetable.h"

namespace synth {

// Band-limited saw, reduced from PRA32-U2's oscillator: a phase accumulator
// reading a mipmapped table (one per band of notes) with linear interpolation.
// Pitch is set at block rate; samples come out at about -0.83..+0.83.
// Same interface as WavetableOsc, so both fit PolyGroup.
class SawOsc {
 public:
  void set_pitch(float semitones);
  void set_wave(float, WaveScan) {}   // a saw has no wave position
  void reset_phase() { phase_ = 0; }

  inline float process();
  void render(float* out, size_t n) {
    for (size_t i = 0; i < n; ++i) out[i] = process();
  }

 private:
  uint32_t phase_ = 0;          // full 32-bit cycle
  uint32_t increment_ = 0;      // phase step per sample
  const SawTable* table_ = &kSawTables[0];
};

inline float SawOsc::process() {
  const uint32_t bits = table_->bits;
  const uint32_t index = phase_ >> (32 - bits);
  // The 24 phase bits below the index, as the lerp weight (0 to 1)
  const float frac = static_cast<float>((phase_ << bits) >> 8) * (1.0f / 16777216.0f);
  const float a = table_->data[index];
  const float b = table_->data[index + 1];   // guard sample covers the last index
  phase_ += increment_;
  return (a + (b - a) * frac) * (1.0f / 32768.0f);
}

}  // namespace synth
