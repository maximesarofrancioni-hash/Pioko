#pragma once

#include <cstdint>

#include "synth/voice_group.h"

namespace engine {

// A command for one core's VoiceGroup, queued by the control side and applied
// by the owning core at the start of its next block.
struct Event {
  enum Type : uint8_t { kNoteOn, kNoteOff, kParam };

  Type type;
  uint8_t voice;    // kNoteOn, kNoteOff
  uint16_t id;      // kParam
  float a;          // kNoteOn: pitch (semitones); kParam: value
  float b;          // kNoteOn: velocity
};

inline void apply(synth::VoiceGroup& group, const Event& e) {
  switch (e.type) {
    case Event::kNoteOn: group.note_on(e.voice, e.a, e.b); break;
    case Event::kNoteOff: group.note_off(e.voice); break;
    case Event::kParam: group.set_param(e.id, e.a); break;
  }
}

}  // namespace engine
