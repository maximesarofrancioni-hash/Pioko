#pragma once

#include <cstdint>

#include "config/audio_config.h"
#include "dsp/buffer.h"

namespace synth {

// One block of a module's output, in the slot layout of its TDM lane.
struct Bus {
  float l[audio::kBlockSize];
  float r[audio::kBlockSize];
  float send_a[audio::kBlockSize];   // reverb send
  float send_b[audio::kBlockSize];   // delay send

  void clear() { dsp::clear(l, sizeof(*this) / sizeof(float)); }   // the 4 arrays are contiguous
};

// A set of voices rendered together on one core: a PRA32-style engine, a group
// of Plaits voices, or (here) plain saws all sit behind this interface.
class VoiceGroup {
 public:
  // pitch in semitones (MIDI note numbers, fractional allowed; 69 = A4 = 440 Hz)
  virtual void note_on(uint8_t voice, float pitch, float velocity) = 0;
  virtual void note_off(uint8_t voice) = 0;
  virtual void set_param(uint16_t id, float value) = 0;

  // Overwrites one block of `out`. Called once per block, so the virtual call is free.
  virtual void render(Bus& out) = 0;

  virtual uint8_t voices() const = 0;

 protected:
  ~VoiceGroup() = default;   // not deleted through the base: no heap needed
};

}  // namespace synth
