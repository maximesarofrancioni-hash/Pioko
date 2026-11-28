#pragma once

#include <cstdint>

namespace synth {

// Analog-style ADSR, advanced once per block (3 kHz control rate at 48 kHz / 16).
// Each stage approaches its target exponentially; attack aims past 1.0 so that
// its curve stays convex-to-linear like an RC charging towards a higher rail.
// Times are set off the audio path (they cost an expf each); process() is cheap.
class Adsr {
 public:
  enum class Stage : uint8_t { kIdle, kAttack, kDecay, kSustain, kRelease };

  void set_attack(float seconds);
  void set_decay(float seconds);
  void set_sustain(float level) { sustain_ = level; }
  void set_release(float seconds);

  void gate_on() { stage_ = Stage::kAttack; }
  void gate_off() { if (stage_ != Stage::kIdle) stage_ = Stage::kRelease; }

  // Returns the level for the end of this block (0..1)
  inline float process();

  float level() const { return level_; }
  bool idle() const { return stage_ == Stage::kIdle; }

 private:
  static float coefficient(float seconds, float taus);

  Stage stage_ = Stage::kIdle;
  float level_ = 0.0f;
  float attack_ = 0.0f, decay_ = 0.0f, release_ = 0.0f;   // per-block approach factors
  float sustain_ = 1.0f;
};

inline float Adsr::process() {
  constexpr float kAttackTarget = 1.3f;
  constexpr float kSilent = 1.0e-4f;   // -80 dB
  switch (stage_) {
    case Stage::kIdle:
      break;
    case Stage::kAttack:
      level_ += (kAttackTarget - level_) * attack_;
      if (level_ >= 1.0f) { level_ = 1.0f; stage_ = Stage::kDecay; }
      break;
    case Stage::kDecay:
      level_ += (sustain_ - level_) * decay_;
      if (level_ - sustain_ < kSilent) { level_ = sustain_; stage_ = Stage::kSustain; }
      break;
    case Stage::kSustain:
      level_ = sustain_;
      break;
    case Stage::kRelease:
      level_ -= level_ * release_;
      if (level_ < kSilent) { level_ = 0.0f; stage_ = Stage::kIdle; }
      break;
  }
  return level_;
}

}  // namespace synth
