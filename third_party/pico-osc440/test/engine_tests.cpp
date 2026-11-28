// Host tests for the dual-core engine: queue, allocator, and the key property
// that two cores mixed sound exactly like one group playing all the voices.

#include <atomic>
#include <cstdio>
#include <functional>
#include <cmath>
#include <thread>
#include <vector>

#include "check.h"
#include "dsp/spsc_queue.h"
#include "engine/voice_allocator.h"
#include "engine/voice_engine.h"
#include "synth/demo_patch.h"
#include "synth/poly_group.h"
#include "synth/triple_wave_group.h"

namespace {

using engine::VoiceAllocator;
using test::check;

void queue_tests() {
  dsp::SpscQueue<int, 8> q;
  int pushed = 0;
  while (q.push(pushed)) ++pushed;
  check(pushed == 7, "queue: holds N - 1 items (N = 8)", pushed);
  bool order = true;
  int v;
  for (int i = 0; i < 7; ++i) order = order && q.pop(v) && v == i;
  check(order && !q.pop(v), "queue: pops in order, then empty", order);

  // Two threads, a million items: nothing lost, duplicated or reordered
  static dsp::SpscQueue<uint32_t, 64> shared;
  constexpr uint32_t kItems = 1000000;
  std::atomic<bool> ok{true};
  std::thread consumer([&] {
    uint32_t expect = 0, item;
    while (expect < kItems) {
      if (shared.pop(item)) {
        if (item != expect) ok = false;
        ++expect;
      }
    }
  });
  for (uint32_t i = 0; i < kItems;) {
    if (shared.push(i)) ++i;
  }
  consumer.join();
  check(ok, "queue: 1M items across two threads, in order", kItems);
}

void allocator_tests() {
  VoiceAllocator a(2, 8);
  const VoiceAllocator::Slot s0 = a.note_on(60), s1 = a.note_on(62);
  const VoiceAllocator::Slot s2 = a.note_on(64), s3 = a.note_on(65);
  check(s0.core == 0 && s1.core == 1 && s2.core == 0 && s3.core == 1,
        "allocator: notes alternate between the cores", s1.core);
  const VoiceAllocator::Slot again = a.note_on(62);
  check(again.core == s1.core && again.voice == s1.voice,
        "allocator: a held note retriggers its own voice", again.voice);

  VoiceAllocator::Slot off;
  check(a.note_off(64, &off) && off.core == s2.core && off.voice == s2.voice,
        "allocator: note_off finds the voice", off.voice);
  check(!a.note_off(64, &off), "allocator: a second note_off finds nothing", 0);

  // Fill all 16 voices, then one more: the oldest held note is stolen
  VoiceAllocator b(2, 8);
  VoiceAllocator::Slot first = b.note_on(40);
  for (int n = 41; n < 56; ++n) b.note_on(static_cast<uint8_t>(n));
  check(b.held(0) == 8 && b.held(1) == 8, "allocator: 16 notes fill both cores", b.held(0) + b.held(1));
  const VoiceAllocator::Slot stolen = b.note_on(90);
  check(stolen.core == first.core && stolen.voice == first.voice,
        "allocator: the 17th note steals the oldest", stolen.voice);

  // A free voice released long ago is preferred over one just released
  VoiceAllocator c(1, 4);
  for (int n = 0; n < 4; ++n) c.note_on(static_cast<uint8_t>(60 + n));
  VoiceAllocator::Slot early, late;
  c.note_off(61, &early);
  c.note_off(63, &late);
  const VoiceAllocator::Slot next = c.note_on(70);
  check(next.voice == early.voice, "allocator: reuses the voice released longest ago", next.voice);
}

// The engine (two groups, queues, allocator, mix) against one group
// with every note on its own voice: same patch, same notes, same timing.
template <class Group, class Patch>
void equivalence_test(const char* name, Patch patch) {
  Group g0, g1, reference;
  engine::VoiceEngine e(g0, g1);
  int set = 0;
  patch([&](uint16_t id, float v) {
    e.set_param(id, v);
    reference.set_param(id, v);
    if (++set % 32 == 0) {   // drain the queues now and then: they hold 127 events
      synth::Bus scratch;
      e.render(0, scratch);
      e.render(1, scratch);
    }
  });

  const uint8_t notes[] = {45, 52, 57, 61, 64, 69, 73, 76};
  synth::Bus b0, b1, mixed, ref;
  double worst = 0.0, peak = 0.0;
  for (int block = 0; block < 6000; ++block) {   // 2 s
    if (block == 0) {
      for (int i = 0; i < 8; ++i) {
        e.note_on(notes[i], 0.8f);
        reference.note_on(static_cast<uint8_t>(i), notes[i], 0.8f);
      }
    }
    if (block == 3000) {
      for (int i = 0; i < 8; ++i) {
        e.note_off(notes[i]);
        reference.note_off(static_cast<uint8_t>(i));
      }
    }
    e.render(0, b0);
    e.render(1, b1);
    engine::VoiceEngine::mix(b0, b1, mixed);
    reference.render(ref);
    for (size_t i = 0; i < audio::kBlockSize; ++i) {
      worst = std::fmax(worst, std::fmax(std::fabs(mixed.l[i] - ref.l[i]), std::fabs(mixed.r[i] - ref.r[i])));
      peak = std::fmax(peak, std::fabs(ref.l[i]));
    }
  }
  char label[96];
  std::snprintf(label, sizeof(label), "engine (%s): reference is not silent (peak)", name);
  check(peak > 0.05, label, peak);
  std::snprintf(label, sizeof(label), "engine (%s): 2 cores mixed vs 1 group, max diff", name);
  check(worst < 1e-5, label, worst);
  std::snprintf(label, sizeof(label), "engine (%s): all released, no events dropped", name);
  check(e.held(0) == 0 && e.held(1) == 0 && e.dropped_events() == 0, label, e.dropped_events());
}

void wavetable_patch(const std::function<void(uint16_t, float)>& set) {
  using G = synth::WavetableGroup;
  set(G::kParamCutoff, 70.0f);
  set(G::kParamResonance, 0.5f);
  set(G::kParamWave, 12.0f);
  set(G::kParamWaveEnvAmount, 40.0f);
  set(G::kParamAmpRelease, 0.2f);
}

}  // namespace

int main() {
  queue_tests();
  allocator_tests();
  equivalence_test<synth::WavetableGroup>("wavetable", wavetable_patch);
  equivalence_test<synth::TripleWaveGroup>("3-osc", [](const std::function<void(uint16_t, float)>& set) {
    synth::apply_demo_patch(set);
  });
  return test::finish("engine");
}
