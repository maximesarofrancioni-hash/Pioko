#include "engine/voice_engine.h"

#include "config/platform.h"

namespace engine {

VoiceEngine::VoiceEngine(synth::VoiceGroup& core0, synth::VoiceGroup& core1)
    : group_{&core0, &core1}, allocator_(kCores, core0.voices() < core1.voices() ? core0.voices() : core1.voices()) {}

void VoiceEngine::send(int core, const Event& e) {
  if (!queue_[core].push(e)) ++dropped_;
}

void VoiceEngine::note_on(uint8_t note, float velocity) {
  const VoiceAllocator::Slot s = allocator_.note_on(note);
  send(s.core, {Event::kNoteOn, s.voice, 0, static_cast<float>(note), velocity});
}

void VoiceEngine::note_off(uint8_t note) {
  VoiceAllocator::Slot s;
  if (allocator_.note_off(note, &s)) send(s.core, {Event::kNoteOff, s.voice, 0, 0.0f, 0.0f});
}

void VoiceEngine::set_param(uint16_t id, float value) {
  for (int c = 0; c < kCores; ++c) send(c, {Event::kParam, 0, id, value, 0.0f});
}

void AUDIO_HOT(VoiceEngine::render)(int core, synth::Bus& out) {
  // A bounded number per block, so a burst (a whole preset is 179 parameters)
  // spreads over a few blocks instead of making one of them late
  Event e;
  for (int n = 0; n < kMaxEventsPerBlock && queue_[core].pop(e); ++n) apply(*group_[core], e);
  group_[core]->render(out);
}

void AUDIO_HOT(VoiceEngine::mix)(const synth::Bus& a, const synth::Bus& b, synth::Bus& out) {
  const float* pa = a.l;
  const float* pb = b.l;
  float* po = out.l;
  for (size_t i = 0; i < sizeof(synth::Bus) / sizeof(float); ++i) po[i] = pa[i] + pb[i];
}

}  // namespace engine
