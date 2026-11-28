#include "synth/adsr.h"

#include <cmath>

#include "config/audio_config.h"

namespace synth {
namespace {

constexpr float kBlockSeconds = static_cast<float>(audio::kBlockSize) / audio::kSampleRate;

// Number of time constants a stage spends in its set time:
// decay and release cover 99% of their way (e^-4.6 = 1%, i.e. -40 dB);
// attack, aiming at 1.3, reaches 1.0 after ln(1.3 / 0.3) = 1.466 time constants.
constexpr float kDecayTaus = 4.6f;
constexpr float kAttackTaus = 1.466f;

}  // namespace

float Adsr::coefficient(float seconds, float taus) {
  if (seconds <= kBlockSeconds) return 1.0f;   // instant: done in one block
  return 1.0f - std::exp(-kBlockSeconds * taus / seconds);
}

void Adsr::set_attack(float seconds) { attack_ = coefficient(seconds, kAttackTaus); }
void Adsr::set_decay(float seconds) { decay_ = coefficient(seconds, kDecayTaus); }
void Adsr::set_release(float seconds) { release_ = coefficient(seconds, kDecayTaus); }

}  // namespace synth
