#pragma once

#include <cstddef>
#include <cstdint>

#include "synth/saw_tables.h"
#include "synth/wavetable.h"
#include "synth/wavetable_data.h"

namespace synth {

// The 3rd Wave-style oscillator: a wavetable (as WavetableOsc: mipmaps, 6-point
// Lagrange, smooth or stepped scan) or one of the virtual-analogue waveforms,
// with linear FM and hard sync from a neighbouring oscillator.
//
// Saw, pulse and supersaw read the band-limited saw tables (one per 3 semitones,
// harmonics up to 20 kHz); a pulse of any width is a saw minus a shifted saw.
// The triangle's slope breaks get a polyBLAMP, and a sync reset a polyBLEP.
// Both need the sample before the event, so the output is delayed by one
// sample (identically in every mode, so oscillators stay aligned).
//
// Pulse width shapes most waveforms: pulse duty, triangle skew, sine phase
// distortion, supersaw detune, noise colour.
class Oscillator {
 public:
  enum class Mode : uint8_t {
    kWavetable, kSine, kTriangle, kSaw, kPulse, kSupersaw, kNoise, kColoredNoise, kCount
  };

  void set_mode(Mode mode) { mode_ = mode; }
  void set_table(const Wavetable* table);
  void set_pitch(float semitones);                    // block rate
  void set_wave(float position, WaveScan scan);       // wavetable: reached at block end
  void set_pulse_width(float pw);                     // 0..1, reached at block end
  void reset_phase() { phase_ = 0; }

  // fm:       per-sample frequency ratio offset (frequency x (1 + fm)), or nullptr
  // sync_in:  per sample, < 0: nothing; >= 0: the master wrapped that many samples
  //           before this one (0..1), so this oscillator restarts there; or nullptr
  // sync_out: written like sync_in, with this oscillator's own wraps; or nullptr
  void render(float* out, size_t n, const float* fm, const float* sync_in, float* sync_out);

  int level() const { return level_ ? static_cast<int>(level_ - table_->level) : 0; }

 private:
  static constexpr int kSupersawVoices = 7;

  // The periodic shapes, each with its own render loop so the per-sample code
  // has no mode branches (wavetables split by scan mode)
  enum class Shape : uint8_t { kTableSmooth, kTableStepped, kSine, kTriangle, kSaw, kPulse };

  // The value at a phase, without any band-limiting correction (wavetables
  // and the saw tables are band-limited by construction)
  template <Shape S>
  inline float shape(uint32_t phase, float wave_pos, float pw) const;
  template <bool kSmooth>
  inline float table_sample(uint32_t phase, float wave_pos) const;
  inline float saw_sample(uint32_t phase) const;   // band-limited saw, +/-1
  // kPlain: no FM, no sync in or out (the common case, a tighter loop)
  template <Shape S, bool kPlain>
  inline void render_shape(float* out, size_t n, const float* fm, const float* sync_in, float* sync_out);
  void render_noise(float* out, size_t n, float* sync_out);
  void render_supersaw(float* out, size_t n, const float* fm, float* sync_out);

  Mode mode_ = Mode::kWavetable;
  const Wavetable* table_ = &kBasicWavetable;
  const Wavetable::Level* level_ = &kBasicWavetable.level[0];
  const SawTable* saw_ = &kSawTables[0];
  uint32_t phase_ = 0;
  float increment_ = 0.0f;              // phase step per sample, in 1/2^32 cycles
  float position_ = 0.0f, position_target_ = 0.0f;
  float pw_ = 0.5f, pw_target_ = 0.5f;
  WaveScan scan_ = WaveScan::kSmooth;
  float pending_ = 0.0f;                // the delayed output sample
  // Supersaw
  uint32_t saw_phase_[kSupersawVoices] = {0, 0x2493e2c1u, 0x49a52e0fu, 0x6db6c9a3u,
                                          0x92492a7du, 0xb6db6f45u, 0xdb6dae37u};
  // Noise
  uint32_t random_ = 0x6b8b4567u;
  float noise_lp_ = 0.0f;
};

}  // namespace synth
