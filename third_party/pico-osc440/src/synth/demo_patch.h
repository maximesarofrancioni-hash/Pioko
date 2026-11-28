#pragma once

#include "synth/mod_matrix.h"
#include "synth/oscillator.h"
#include "synth/triple_wave_group.h"

namespace synth {

// The demo sound, shared by the firmware, the host renders and the load
// estimator, so they all play the same thing. `set(id, value)` is whatever
// applies a parameter: VoiceEngine::set_param, or a group's own set_param.
template <class Set>
void apply_demo_patch(Set&& set) {
  using G = TripleWaveGroup;
  using M = Oscillator::Mode;
  // Osc 1: our table, swept up by its wave envelope
  set(G::osc_param(0, G::kOscWave), 20.0f);
  set(G::osc_param(0, G::kOscLevel), 0.5f);
  set(G::osc_param(0, G::kOscWaveEnvAmount), 25.0f);
  // Osc 2: Plaits' bank A, a fifth up, slightly detuned
  set(G::osc_param(1, G::kOscTable), 1.0f);
  set(G::osc_param(1, G::kOscWave), 10.0f);
  set(G::osc_param(1, G::kOscTune), 7.0f);
  set(G::osc_param(1, G::kOscFine), 4.0f);
  set(G::osc_param(1, G::kOscLevel), 0.3f);
  // Osc 3: a VA pulse an octave down, its width swept by LFO 3
  set(G::osc_param(2, G::kOscMode), static_cast<float>(M::kPulse));
  set(G::osc_param(2, G::kOscTune), -12.0f);
  set(G::osc_param(2, G::kOscPw), 0.35f);
  set(G::osc_param(2, G::kOscLevel), 0.35f);
  // SVF high pass first, then the resonant, driven ladder swept by envelope 1
  set(G::kParamSvfCutoff, 40.0f);
  set(G::kParamSvfMorph, 1.0f);
  set(G::kParamLadderCutoff, 62.0f);
  set(G::kParamLadderResonance, 0.55f);
  set(G::kParamDrive, 2.0f);
  set(G::kParamCompensation, 0.6f);
  set(G::kParamLadderEnvAmount, 42.0f);
  set(G::kParamKeyTrack, 0.7f);
  // LFOs: vibrato (faded in), a slow wave drift, PWM
  set(G::lfo_param(0, G::kLfoRate), 5.0f);
  set(G::lfo_param(0, G::kLfoDelay), 0.3f);
  set(G::lfo_param(1, G::kLfoRate), 0.3f);
  set(G::lfo_param(1, G::kLfoShape), static_cast<float>(Lfo::Shape::kTriangle));
  set(G::lfo_param(2, G::kLfoRate), 0.5f);
  const struct { uint8_t src, dst; float amount; } slots[] = {
      {mod::kSrcLfo1, mod::kDstPitch1, 0.08f},
      {mod::kSrcLfo1, mod::kDstPitch2, 0.08f},
      {mod::kSrcLfo2, mod::kDstWave2, 10.0f},
      {mod::kSrcVelocity, mod::kDstCutoff2, 12.0f},
      {mod::kSrcLfo3, mod::kDstPw3, 0.25f},
  };
  for (int i = 0; i < 5; ++i) {
    set(G::matrix_param(i, 0), slots[i].src);
    set(G::matrix_param(i, 1), slots[i].dst);
    set(G::matrix_param(i, 2), slots[i].amount);
  }
}

}  // namespace synth
