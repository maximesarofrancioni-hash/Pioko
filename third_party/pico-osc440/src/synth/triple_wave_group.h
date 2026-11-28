#pragma once

#include "config/audio_config.h"
#include "synth/dadsr.h"
#include "synth/ladder.h"
#include "synth/lfo.h"
#include "synth/mod_matrix.h"
#include "synth/mseg.h"
#include "synth/oscillator.h"
#include "synth/svf.h"
#ifdef OSC440_SST_FILTERS
#include "synth/sst_filter.h"
#endif
#include "synth/voice_group.h"

namespace synth {

// A 3rd Wave-style voice, without the effects (after the Sound On Sound review):
//   3 oscillators: wavetable (2 tables) or VA (sine, triangle, saw, pulse,
//     supersaw, 2 noises), pulse width, linear FM from and hard sync to the
//     neighbouring oscillator, each with a 6-stage loopable wave envelope
//   -> mix (levels modulated at audio rate: AM, ring modulation)
//   -> SEM-style multimode SVF (no self-oscillation) -> 4-pole ladder
//      (saturation, resonance compensation, 1x or 2x), or both in parallel;
//      built with OSC440_SST_FILTERS, two sst-filters models instead (filter 1
//      in the SVF's place and parameters, filter 2 in the ladder's), each any of
//      SstFilter's models
//   -> VCA -> pan
// 4 DADSRs (1: filters, 2: VCA, 3-4: free; exponential or PPG curves), 4 LFOs
// (10 shapes, delay, reset) and a 16-slot matrix whose oscillator and noise
// sources run at audio rate.
class TripleWaveGroup final : public VoiceGroup {
 public:
  static constexpr uint8_t kVoices = 8;   // per core
  static constexpr int kOscs = 3;
  static constexpr int kLfos = 4;
  static constexpr int kEnvs = 4;
  static constexpr int kTables = 2;       // 0: basic, 1: Plaits bank A

  enum Param : uint16_t {
    kParamLevel = 0,          // 0..1
    kParamWaveScan,           // 0 smooth, 1 stepped (all oscillators)
    kParamSvfCutoff,          // semitones (MIDI note, at note 60)
    kParamSvfResonance,       // 0..1 (stops short of self-oscillation)
    kParamSvfMorph,           // 0 low pass .. 0.5 notch .. 1 high pass
    kParamSvfBandpass,        // 0/1
    kParamSvfEnvAmount,       // semitones at full envelope 1
    kParamLadderCutoff,       // semitones
    kParamLadderResonance,    // 0..1, self-oscillates near 1
    kParamDrive,              // >= 1
    kParamCompensation,       // 0..1
    kParamLadderEnvAmount,    // semitones at full envelope 1
    kParamFilterRouting,      // 0 series (SVF -> ladder), 1 parallel
    kParamKeyTrack,           // 0..1, both filters
    kParamLadderOversample,   // 1 (default): 2x against aliasing; 0: 1x
    kParamPan,                // -1..1
    kParamModWheel, kParamPressure, kParamExpression,   // 0..1 (matrix sources)
    kParamFilter1Model = 20,  // OSC440_SST_FILTERS: SstFilter model of filter 1 (in the SVF's place)
    kParamFilter2Model,       // OSC440_SST_FILTERS: SstFilter model of filter 2 (in the ladder's place)

    kParamEnvBase = 50,       // + env * kEnvStride + EnvField
    kParamOscBase = 100,      // + osc * kOscStride + OscField
    kParamWaveEnvBase = 200,  // + osc * kWaveEnvStride + stage * 2 + (0 level, 1 time)
                              //   + 12: loop start, + 13: loop end
    kParamLfoBase = 300,      // + lfo * kLfoStride + LfoField
    kParamMatrixBase = 400,   // + slot * 3 + (0 source, 1 destination, 2 amount)
  };
  enum EnvField : uint16_t { kEnvMode, kEnvDelay, kEnvAttack, kEnvDecay, kEnvSustain, kEnvRelease };
  enum OscField : uint16_t {
    kOscMode,           // Oscillator::Mode
    kOscTable,          // 0..kTables-1
    kOscTune, kOscFine, // semitones, cents
    kOscLevel,
    kOscWave, kOscWaveEnvAmount,
    kOscPw,             // 0..1
    kOscFm,             // FM depth from the neighbour (osc 1 <- osc 3, 2 <- 1, 3 <- 2)
    kOscSync,           // 0/1: hard sync to the previous oscillator (oscs 2 and 3)
  };
  enum LfoField : uint16_t { kLfoRate, kLfoShape, kLfoReset, kLfoDelay };
  static constexpr uint16_t kEnvStride = 8, kOscStride = 16, kWaveEnvStride = 16, kLfoStride = 8;

  static constexpr uint16_t env_param(int env, EnvField f) { return kParamEnvBase + env * kEnvStride + f; }
  static constexpr uint16_t osc_param(int osc, OscField f) { return kParamOscBase + osc * kOscStride + f; }
  static constexpr uint16_t wave_env_param(int osc, int stage, bool time) {
    return kParamWaveEnvBase + osc * kWaveEnvStride + stage * 2 + (time ? 1 : 0);
  }
  static constexpr uint16_t wave_env_loop_param(int osc, bool end) {
    return kParamWaveEnvBase + osc * kWaveEnvStride + 12 + (end ? 1 : 0);
  }
  static constexpr uint16_t lfo_param(int lfo, LfoField f) { return kParamLfoBase + lfo * kLfoStride + f; }
  static constexpr uint16_t matrix_param(int slot, int field) { return kParamMatrixBase + slot * 3 + field; }
  static constexpr uint16_t kParamCount = kParamMatrixBase + ModMatrix::kSlots * 3;

  // The value last set for a parameter id (every id starts with its default):
  // the group's whole sound, as a preset reads it
  float param(uint16_t id) const { return id < kParamCount ? params_[id] : 0.0f; }

  TripleWaveGroup();

  void note_on(uint8_t voice, float pitch, float velocity) override;
  void note_off(uint8_t voice) override;
  void set_param(uint16_t id, float value) override;
  void render(Bus& out) override;
  uint8_t voices() const override { return kVoices; }

 private:
  static constexpr size_t kN = audio::kBlockSize;

  struct OscSettings {
    float tune = 0.0f, fine = 0.0f, level = 0.0f, wave = 0.0f, wave_env = 0.0f;
    float pw = 0.5f, fm = 0.0f;
    bool sync = false;
  };

  struct Voice {
    Oscillator osc[kOscs];
    Mseg wave_env[kOscs];
    Dadsr env[kEnvs];
    Lfo lfo[kLfos];
#ifdef OSC440_SST_FILTERS
    SstFilter f1, f2;
    bool f1_bypassed = true;   // filter 1 fully open, as the SVF: skipped
#else
    Svf svf;
    Ladder ladder;
    OversampledLadder ladder2x;
#endif
    float pitch = 69.0f, velocity = 0.0f;
    float osc_level[kOscs] = {0.0f, 0.0f, 0.0f};   // at the end of the last block
    float gain_l = 0.0f, gain_r = 0.0f;
    float lfo_rate_mod[kLfos] = {0.0f, 0.0f, 0.0f, 0.0f};   // from the last block's matrix
    float last_osc[kOscs][kN] = {};                 // the oscillators' last block
    float last_noise = 0.0f;
    uint32_t noise_state = 0x2545f491u;
    bool svf_bypassed = true;   // fully open low pass: skipped (crossfaded in and out)
  };

  void render_voice(Voice& v, Bus& out);
  void update_matrix_flags();
  void update_usage();
#ifdef OSC440_SST_FILTERS
  void f1_block(Voice& v, float* x, bool bypass);
#else
  void svf_block(Voice& v, float* x, bool bypass);
#endif

  Voice voice_[kVoices];
  float params_[kParamCount] = {};
  OscSettings osc_[kOscs];
  bool lfo_reset_[kLfos] = {false, false, false, false};
  float wave_env_level_[kOscs][Mseg::kStages] = {};
  float wave_env_seconds_[kOscs][Mseg::kStages] = {};
  int wave_env_loop_[kOscs][2] = {{1, 0}, {1, 0}, {1, 0}};   // start > end: no loop
  ModMatrix matrix_;
  // What the matrix needs at audio rate (recomputed when a slot changes)
  bool source_used_[4] = {false, false, false, false};   // osc 1-3, noise
  bool audio_level_[kOscs] = {false, false, false};       // a level modulated at audio rate
  bool audio_dst_[mod::kNumDestinations] = {};             // any audio-rate slot aimed here
  // Modulators something listens to (the rest are not computed); recomputed
  // when a parameter changes
  bool usage_dirty_ = true;
  bool lfo_used_[kLfos] = {}, env_used_[kEnvs] = {}, wave_env_used_[kOscs] = {};
  bool keep_last_[kOscs] = {};   // the oscillator's last block is read in the next one
  float level_ = 0.125f;
  WaveScan scan_ = WaveScan::kSmooth;
  float svf_cutoff_ = 135.0f, svf_resonance_ = 0.0f, morph_ = 0.0f, svf_env_ = 0.0f;
  float ladder_cutoff_ = 72.0f, ladder_resonance_ = 0.2f, drive_ = 1.0f, compensation_ = 0.5f;
  float ladder_env_ = 36.0f;
  bool bandpass_ = false, parallel_ = false, oversample_ = true;
  int filter_model_[2] = {11, 4};   // OSC440_SST_FILTERS: OB-Xd 12 dB LP (SEM-ish), Huovilainen ladder (bass comp)
  float key_track_ = 1.0f, pan_ = 0.0f;
  float mod_wheel_ = 0.0f, pressure_ = 0.0f, expression_ = 0.0f;
};

}  // namespace synth
