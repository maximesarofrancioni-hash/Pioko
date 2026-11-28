#pragma once

#include <cstdint>

namespace synth {

// Delay-Attack-Decay-Sustain-Release envelope, advanced once per block, in one
// of two curve modes:
//   exponential: RC-like segments (attack aims past 1 so it ends steep)
//   PPG:         linear segments, as the more linear envelopes of PPG-era synths
// Times are set off the audio path; process() is cheap.
class Dadsr {
 public:
  enum class Mode : uint8_t { kExponential, kPpg };
  enum class Stage : uint8_t { kIdle, kDelay, kAttack, kDecay, kSustain, kRelease };

  void set_mode(Mode mode) { mode_ = mode; }
  void set_delay(float seconds);
  void set_attack(float seconds);
  void set_decay(float seconds);
  void set_sustain(float level) { sustain_ = level; }
  void set_release(float seconds);

  void gate_on();
  void gate_off();

  float process();   // level at the end of this block, 0..1

  float level() const { return level_; }
  bool idle() const { return stage_ == Stage::kIdle; }
  Stage stage() const { return stage_; }

 private:
  Mode mode_ = Mode::kExponential;
  Stage stage_ = Stage::kIdle;
  float level_ = 0.0f;
  float release_from_ = 0.0f;   // PPG mode: level when release began
  float t_ = 0.0f;              // progress through delay or a linear stage
  // Per-block steps: linear progress (PPG, delay) and exponential factors
  float delay_step_ = 1.0f;
  float attack_lin_ = 1.0f, decay_lin_ = 1.0f, release_lin_ = 1.0f;
  float attack_exp_ = 1.0f, decay_exp_ = 1.0f, release_exp_ = 1.0f;
  float sustain_ = 1.0f;
};

}  // namespace synth
