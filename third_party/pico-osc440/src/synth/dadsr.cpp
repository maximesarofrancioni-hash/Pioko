#include "synth/dadsr.h"

#include <cmath>

#include "config/audio_config.h"
#include "config/platform.h"
#include "dsp/fast_math.h"

namespace synth {
namespace {

constexpr float kBlockSeconds = static_cast<float>(audio::kBlockSize) / audio::kSampleRate;
constexpr float kAttackTarget = 1.3f;
constexpr float kSilent = 1.0e-4f;   // -80 dB
// Time constants per stage time (as Adsr): decay and release cover 99% of their
// way in the set time; the attack reaches 1.0 aiming at 1.3
constexpr float kDecayTaus = 4.6f;
constexpr float kAttackTaus = 1.466f;

__attribute__((always_inline)) inline float linear_step(float seconds) { return seconds <= kBlockSeconds ? 1.0f : kBlockSeconds / seconds; }
// 1 - e^-x, the share of the remaining way covered per block. Without the C
// library's expf (in flash, and slow): a series where 1 - e^-x would lose its
// digits (long stages: x is small), fast_exp2 elsewhere. Within 0.02% of expf.
__attribute__((always_inline)) inline float exp_factor(float seconds, float taus) {
  if (seconds <= kBlockSeconds) return 1.0f;
  const float x = kBlockSeconds * taus / seconds;
  if (x < 0.03f) return x * (1.0f - x * (0.5f - x * (1.0f / 6.0f)));
  return 1.0f - dsp::fast_exp2(-1.44269504f * x);
}

}  // namespace

void AUDIO_HOT(Dadsr::set_delay)(float seconds) { delay_step_ = linear_step(seconds); }
void AUDIO_HOT(Dadsr::set_attack)(float seconds) {
  attack_lin_ = linear_step(seconds);
  attack_exp_ = exp_factor(seconds, kAttackTaus);
}
void AUDIO_HOT(Dadsr::set_decay)(float seconds) {
  decay_lin_ = linear_step(seconds);
  decay_exp_ = exp_factor(seconds, kDecayTaus);
}
void AUDIO_HOT(Dadsr::set_release)(float seconds) {
  release_lin_ = linear_step(seconds);
  release_exp_ = exp_factor(seconds, kDecayTaus);
}

void AUDIO_HOT(Dadsr::gate_on)() {
  stage_ = delay_step_ >= 1.0f ? Stage::kAttack : Stage::kDelay;
  t_ = 0.0f;
}

void AUDIO_HOT(Dadsr::gate_off)() {
  if (stage_ == Stage::kIdle) return;
  stage_ = Stage::kRelease;
  release_from_ = level_;
  t_ = 0.0f;
}

float AUDIO_HOT(Dadsr::process)() {
  const bool ppg = mode_ == Mode::kPpg;
  switch (stage_) {
    case Stage::kIdle:
      break;
    case Stage::kDelay:
      t_ += delay_step_;
      if (t_ >= 1.0f) stage_ = Stage::kAttack;
      break;
    case Stage::kAttack:
      level_ = ppg ? level_ + attack_lin_ : level_ + (kAttackTarget - level_) * attack_exp_;
      if (level_ >= 1.0f) {
        level_ = 1.0f;
        stage_ = Stage::kDecay;
      }
      break;
    case Stage::kDecay:
      level_ = ppg ? level_ - decay_lin_ * (1.0f - sustain_) : level_ + (sustain_ - level_) * decay_exp_;
      if (level_ - sustain_ < (ppg ? 0.0f : kSilent)) {
        level_ = sustain_;
        stage_ = Stage::kSustain;
      }
      break;
    case Stage::kSustain:
      level_ = sustain_;
      break;
    case Stage::kRelease:
      if (ppg) {
        t_ += release_lin_;
        level_ = release_from_ * (1.0f - t_);
        if (t_ >= 1.0f) level_ = 0.0f;
      } else {
        level_ -= level_ * release_exp_;
      }
      if (level_ < kSilent) {
        level_ = 0.0f;
        stage_ = Stage::kIdle;
      }
      break;
  }
  return level_;
}

}  // namespace synth
