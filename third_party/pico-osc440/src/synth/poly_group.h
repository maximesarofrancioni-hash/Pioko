#pragma once

#include "config/platform.h"
#include "synth/adsr.h"
#include "synth/saw_osc.h"
#include "synth/svf.h"
#include "synth/voice_group.h"
#include "synth/wavetable_osc.h"

namespace synth {

// Oscillator -> SVF low pass -> VCA, per voice, for any oscillator with
// set_pitch(), set_wave() and render(out, n) (SawOsc, WavetableOsc).
// The filter envelope sweeps the cutoff and the wave position; the amp envelope
// drives the VCA. Envelopes and coefficients run at block rate; the oscillator,
// filter and VCA ramp at audio rate.
template <class Osc>
class PolyGroup : public VoiceGroup {
 public:
  static constexpr uint8_t kVoices = 8;   // per core

  enum Param : uint16_t {
    kParamLevel = 0,        // 0..1
    kParamCutoff,           // semitones (MIDI note of the cutoff at note 60)
    kParamResonance,        // 0..1
    kParamFilterEnvAmount,  // semitones added at full filter envelope
    kParamKeyTrack,         // 0..1: cutoff follows the note
    kParamFilterAttack, kParamFilterDecay, kParamFilterSustain, kParamFilterRelease,
    kParamAmpAttack, kParamAmpDecay, kParamAmpSustain, kParamAmpRelease,
    kParamWave,             // wave position, 0 .. waves-1
    kParamWaveEnvAmount,    // waves added at full filter envelope
    kParamWaveScan,         // 0 smooth, 1 stepped
  };

  PolyGroup();

  void note_on(uint8_t voice, float pitch, float velocity) override;
  void note_off(uint8_t voice) override;
  void set_param(uint16_t id, float value) override;
  uint8_t voices() const override { return kVoices; }

 protected:
  // The block render, force-inlined into each concrete group's render(): GCC
  // ignores section attributes on template instantiations, so the SRAM
  // placement has to sit on a plain function.
  __attribute__((always_inline)) inline void render_block(Bus& out);

 private:
  struct Voice {
    Osc osc;
    Svf filter;
    Adsr amp_env;
    Adsr filter_env;
    float pitch = 69.0f;
    float velocity = 0.0f;
    float gain = 0.0f;   // VCA gain at the end of the last block
  };

  float wave_position(const Voice& v) const { return wave_ + wave_env_ * v.filter_env.level(); }

  Voice voice_[kVoices];
  float level_ = 0.125f;   // 8 voices at full velocity stay under full scale
  float cutoff_ = 60.0f;
  float resonance_ = 0.3f;
  float env_amount_ = 48.0f;
  float key_track_ = 1.0f;
  float wave_ = 0.0f;
  float wave_env_ = 0.0f;
  WaveScan scan_ = WaveScan::kSmooth;
};

class SawGroup final : public PolyGroup<SawOsc> {
 public:
  void render(Bus& out) override;
};

class WavetableGroup final : public PolyGroup<WavetableOsc> {
 public:
  void render(Bus& out) override;
};

}  // namespace synth
