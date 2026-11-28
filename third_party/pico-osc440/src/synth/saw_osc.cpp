#include "synth/saw_osc.h"

#include <cmath>

#include "config/audio_config.h"

namespace synth {

void SawOsc::set_pitch(float semitones) {
  const float hz = 440.0f * std::exp2f((semitones - 69.0f) * (1.0f / 12.0f));
  // Phase step: cycles per sample scaled to the 32-bit accumulator.
  // Capped just under Nyquist, which also keeps the float-to-uint32 cast in range.
  const float cycles = std::fmin(hz / audio::kSampleRate, 0.49f);
  increment_ = static_cast<uint32_t>(cycles * 4294967296.0f);

  int band = static_cast<int>(std::floor(semitones)) / kSawBandSemitones;
  if (band < 0) band = 0;
  if (band >= kSawBands) band = kSawBands - 1;
  table_ = &kSawTables[band];
}

}  // namespace synth
