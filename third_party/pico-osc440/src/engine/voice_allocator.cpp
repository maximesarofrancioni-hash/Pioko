#include "engine/voice_allocator.h"

namespace engine {

VoiceAllocator::VoiceAllocator(int cores, int voices_per_core)
    : cores_(cores), voices_per_core_(voices_per_core) {}

int VoiceAllocator::held(int core) const {
  int n = 0;
  for (int v = 0; v < voices_per_core_; ++v) n += voices_[core * voices_per_core_ + v].gate;
  return n;
}

VoiceAllocator::Slot VoiceAllocator::note_on(uint8_t note) {
  // The same note still held: retrigger it where it is
  for (int c = 0; c < cores_; ++c) {
    for (int v = 0; v < voices_per_core_; ++v) {
      Voice& x = at({static_cast<uint8_t>(c), static_cast<uint8_t>(v)});
      if (x.gate && x.note == note) {
        x.stamp = ++clock_;
        return {static_cast<uint8_t>(c), static_cast<uint8_t>(v)};
      }
    }
  }

  // Best free voice: fewest held notes on its core, then released longest ago.
  // Failing that, the oldest held note.
  Slot free_slot{0, 0}, steal_slot{0, 0};
  bool have_free = false, have_steal = false;
  int free_held = 0;
  uint32_t free_stamp = 0, steal_stamp = 0;
  for (int c = 0; c < cores_; ++c) {
    const int core_held = held(c);
    for (int v = 0; v < voices_per_core_; ++v) {
      const Slot s{static_cast<uint8_t>(c), static_cast<uint8_t>(v)};
      const Voice& x = at(s);
      if (!x.gate) {
        if (!have_free || core_held < free_held ||
            (core_held == free_held && x.stamp < free_stamp)) {
          free_slot = s;
          free_held = core_held;
          free_stamp = x.stamp;
          have_free = true;
        }
      } else if (!have_steal || x.stamp < steal_stamp) {
        steal_slot = s;
        steal_stamp = x.stamp;
        have_steal = true;
      }
    }
  }

  const Slot best = have_free ? free_slot : steal_slot;
  Voice& x = at(best);
  x.note = note;
  x.gate = true;
  x.stamp = ++clock_;
  return best;
}

bool VoiceAllocator::note_off(uint8_t note, Slot* slot) {
  for (int c = 0; c < cores_; ++c) {
    for (int v = 0; v < voices_per_core_; ++v) {
      const Slot s{static_cast<uint8_t>(c), static_cast<uint8_t>(v)};
      Voice& x = at(s);
      if (x.gate && x.note == note) {
        x.gate = false;
        x.stamp = ++clock_;
        *slot = s;
        return true;
      }
    }
  }
  return false;
}

}  // namespace engine
