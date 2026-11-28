#pragma once

#include <cstddef>
#include <cstdint>

#include "synth/wavetable.h"
#include "synth/wavetable_data.h"

namespace synth {

// PPG-style wavetable oscillator: a phase accumulator reading one wave of a
// table, with the wave position scanning through the table. Smooth scan
// crossfades the two nearest waves; stepped scan plays the nearest one only.
// Pitch picks the mip level (band limit); position is ramped across each block.
class WavetableOsc {
 public:
  void set_table(const Wavetable* table) { table_ = table; }
  void set_pitch(float semitones);
  // Position 0 .. waves-1 (fractional), reached at the end of the next render()
  void set_wave(float position, WaveScan scan) {
    const float last = static_cast<float>(table_->waves - 1);
    target_ = position < 0.0f ? 0.0f : (position > last ? last : position);
    scan_ = scan;
  }
  void reset_phase() { phase_ = 0; }

  void render(float* out, size_t n);

  // For tests: the mip level in use
  int level() const { return static_cast<int>(level_ - table_->level); }

 private:
  const Wavetable* table_ = &kBasicWavetable;
  const Wavetable::Level* level_ = &kBasicWavetable.level[0];
  uint32_t phase_ = 0;
  uint32_t increment_ = 0;
  float position_ = 0.0f;   // at the end of the last block
  float target_ = 0.0f;
  WaveScan scan_ = WaveScan::kSmooth;
};

}  // namespace synth
