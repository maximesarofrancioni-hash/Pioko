#pragma once

#include <cstddef>
#include <cstdint>

namespace synth {

// Modulation sources and destinations of TripleWaveGroup, and the routing
// between them: 16 slots of (source, destination, amount).
//
// Control sources (LFOs, envelopes, performance) change once per block and
// reach every destination. Audio sources (the oscillators' signals, noise) are
// applied per sample to the audio-rate destinations: FM, oscillator levels
// (AM / ring modulation) and the VCA. Routed elsewhere, an audio source acts
// at block rate, sampled at the end of the block.
namespace mod {

enum Source : uint8_t {
  kSrcNone = 0,
  kSrcLfo1, kSrcLfo2, kSrcLfo3, kSrcLfo4,      // -1..1
  kSrcEnv1, kSrcEnv2, kSrcEnv3, kSrcEnv4,      // DADSRs, 0..1 (1: filter, 2: amp)
  kSrcWaveEnv1, kSrcWaveEnv2, kSrcWaveEnv3,    // 0..1 (typically)
  kSrcVelocity,                                // 0..1
  kSrcKey,                                     // (note - 60) / 60
  kSrcModWheel, kSrcPressure, kSrcExpression,  // 0..1, per group
  kSrcOsc1, kSrcOsc2, kSrcOsc3, kSrcNoise,     // audio: -1..1
  kNumSources
};
constexpr bool is_audio_source(uint8_t s) { return s >= kSrcOsc1 && s < kNumSources; }

enum Destination : uint8_t {
  kDstPitch1, kDstPitch2, kDstPitch3,          // semitones
  kDstWave1, kDstWave2, kDstWave3,             // wave positions
  kDstPw1, kDstPw2, kDstPw3,                   // pulse width, added
  kDstFm1, kDstFm2, kDstFm3,                   // linear FM depth (frequency x (1 + v))   [audio]
  kDstLevel1, kDstLevel2, kDstLevel3,          // added to the oscillator levels           [audio]
  kDstAmp,                                     // added to the VCA gain                   [audio]
  kDstCutoff1, kDstResonance1,                 // SVF (first in line): semitones, 0..1
  kDstCutoff2, kDstResonance2,                 // ladder: semitones, 0..1
  kDstDrive,                                   // added to the ladder drive
  kDstPan,                                     // -1..1
  kDstLfo1Rate, kDstLfo2Rate, kDstLfo3Rate, kDstLfo4Rate,   // octaves
  kNumDestinations
};
constexpr bool is_audio_destination(uint8_t d) { return d >= kDstFm1 && d <= kDstAmp; }

}  // namespace mod

class ModMatrix {
 public:
  static constexpr int kSlots = 16;

  struct Slot {
    uint8_t source = mod::kSrcNone;
    uint8_t destination = 0;
    float amount = 0.0f;
  };

  void set(int slot, uint8_t source, uint8_t destination, float amount) {
    if (slot < 0 || slot >= kSlots || source >= mod::kNumSources ||
        destination >= mod::kNumDestinations) return;
    slots_[slot] = {source, destination, amount};
  }
  void set_source(int slot, float v) { if (slot >= 0 && slot < kSlots && v >= 0 && v < mod::kNumSources) slots_[slot].source = static_cast<uint8_t>(v); }
  void set_destination(int slot, float v) { if (slot >= 0 && slot < kSlots && v >= 0 && v < mod::kNumDestinations) slots_[slot].destination = static_cast<uint8_t>(v); }
  void set_amount(int slot, float v) { if (slot >= 0 && slot < kSlots) slots_[slot].amount = v; }
  const Slot& slot(int i) const { return slots_[i]; }

  // Block rate: every slot whose source is not applied per sample. `src` holds
  // one value per source (audio sources: their last sample); dst is zeroed by
  // the caller.
  void apply(const float* src, float* dst) const {
    for (const Slot& s : slots_) {
      if (s.source == mod::kSrcNone) continue;
      if (mod::is_audio_source(s.source) && mod::is_audio_destination(s.destination)) continue;
      dst[s.destination] += src[s.source] * s.amount;
    }
  }

  // Audio rate: adds amount x source signal to the destination's buffer, for the
  // audio-to-audio slots aimed at `destination`. Returns false if there are none.
  bool apply_audio(uint8_t destination, const float* const* signal, float* buffer, size_t n) const {
    bool any = false;
    for (const Slot& s : slots_) {
      if (s.destination != destination || !mod::is_audio_source(s.source)) continue;
      const float* x = signal[s.source - mod::kSrcOsc1];
      const float a = s.amount;
      for (size_t i = 0; i < n; ++i) buffer[i] += a * x[i];
      any = true;
    }
    return any;
  }

 private:
  Slot slots_[kSlots];
};

}  // namespace synth
