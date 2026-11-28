#pragma once

#include <cstdint>

namespace engine {

// Picks which (core, voice) plays each note, across all cores of a module.
//  - the same note, still held, retriggers its own voice
//  - otherwise a free voice (gate off), on the core with the fewest held notes,
//    released the longest ago (so release tails are cut as late as possible)
//  - if every voice is held, the oldest note is stolen
// Runs on the control side only; it tracks gates, not the envelopes themselves.
class VoiceAllocator {
 public:
  static constexpr int kMaxCores = 2;
  static constexpr int kMaxVoicesPerCore = 16;

  struct Slot {
    uint8_t core;
    uint8_t voice;
  };

  VoiceAllocator(int cores, int voices_per_core);

  Slot note_on(uint8_t note);
  // False if the note is not held (already released, or stolen)
  bool note_off(uint8_t note, Slot* slot);

  int held(int core) const;

 private:
  struct Voice {
    uint8_t note = 0;
    bool gate = false;
    uint32_t stamp = 0;   // note-on time if held, release time if not
  };

  Voice& at(Slot s) { return voices_[s.core * voices_per_core_ + s.voice]; }

  int cores_;
  int voices_per_core_;
  uint32_t clock_ = 0;
  Voice voices_[kMaxCores * kMaxVoicesPerCore];
};

}  // namespace engine
