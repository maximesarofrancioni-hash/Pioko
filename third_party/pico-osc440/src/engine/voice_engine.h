#pragma once

#include <cstdint>

#include "dsp/spsc_queue.h"
#include "engine/event.h"
#include "engine/voice_allocator.h"
#include "synth/voice_group.h"

namespace engine {

// One module's voices, split over two cores, each owning a VoiceGroup.
//
// Control side (one thread: core 0's main loop, later the SPI command parser):
//   note_on / note_off / set_param, which allocate voices and queue events.
// Audio side (each core, for its own group only):
//   render(core, bus) applies that core's queued events, then renders a block.
// So a group is only ever touched by the core that renders it: no locks.
// Portable: the host tests run both "cores" one after the other.
class VoiceEngine {
 public:
  static constexpr int kCores = 2;
  static constexpr int kMaxEventsPerBlock = 32;   // a preset loads over 6 blocks (2 ms)

  VoiceEngine(synth::VoiceGroup& core0, synth::VoiceGroup& core1);

  void note_on(uint8_t note, float velocity);
  void note_off(uint8_t note);
  void set_param(uint16_t id, float value);   // to both groups

  void render(int core, synth::Bus& out);
  // Events still waiting for a block (at most kMaxEventsPerBlock go per block)
  bool pending() const { return !queue_[0].empty() || !queue_[1].empty(); }

  // out = a + b
  static void mix(const synth::Bus& a, const synth::Bus& b, synth::Bus& out);

  uint32_t dropped_events() const { return dropped_; }
  int held(int core) const { return allocator_.held(core); }

 private:
  void send(int core, const Event& e);

  synth::VoiceGroup* group_[kCores];
  dsp::SpscQueue<Event, 256> queue_[kCores];   // room for a whole preset (179 parameters) and notes
  VoiceAllocator allocator_;
  uint32_t dropped_ = 0;
};

}  // namespace engine
