#include "synth/lfo.h"

#include "config/audio_config.h"
#include "config/platform.h"
#include "dsp/fast_math.h"

namespace synth {
namespace {

constexpr float kBlockSeconds = static_cast<float>(audio::kBlockSize) / audio::kSampleRate;

}  // namespace

void AUDIO_HOT(Lfo::set_delay)(float seconds) { delay_step_ = seconds <= kBlockSeconds ? 1.0f : kBlockSeconds / seconds; }

float AUDIO_HOT(Lfo::next_random)() {   // xorshift32, -1..1
  random_ ^= random_ << 13;
  random_ ^= random_ >> 17;
  random_ ^= random_ << 5;
  return static_cast<float>(static_cast<int32_t>(random_)) * (1.0f / 2147483648.0f);
}

float AUDIO_HOT(Lfo::process)(float rate_octaves) {
  phase_ += rate_hz_ * (rate_octaves == 0.0f ? 1.0f : dsp::fast_exp2(rate_octaves)) * kBlockSeconds;
  if (phase_ >= 1.0f) {
    phase_ -= static_cast<float>(static_cast<int>(phase_));
    previous_ = held_;
    held_ = next_random();
  }
  if (fade_ < 1.0f) {
    fade_ += delay_step_;
    if (fade_ > 1.0f) fade_ = 1.0f;
  }

  const float p = phase_;
  float v = 0.0f;
  switch (shape_) {
    case Shape::kSine: v = dsp::fast_sin_cycles(p); break;
    case Shape::kTriangle: v = p < 0.5f ? 4.0f * p - 1.0f : 3.0f - 4.0f * p; break;
    case Shape::kSaw: v = 2.0f * p - 1.0f; break;
    case Shape::kSawDown: v = 1.0f - 2.0f * p; break;
    case Shape::kSquare: v = p < 0.5f ? 1.0f : -1.0f; break;
    case Shape::kPulse: v = p < 0.25f ? 1.0f : -1.0f; break;
    case Shape::kSampleHold: v = held_; break;
    case Shape::kSmoothRandom: {   // glides from the last random value to the next
      const float s = p * p * (3.0f - 2.0f * p);
      v = previous_ + (held_ - previous_) * s;
      break;
    }
    case Shape::kExpRamp: {        // falls from 1 to -1 exponentially (e^-5 at the end)
      v = 2.0f * dsp::fast_exp2(-7.2f * p) - 1.0f;
      break;
    }
    case Shape::kStepped: v = static_cast<float>(static_cast<int>(p * 8.0f)) * (2.0f / 7.0f) - 1.0f; break;
    case Shape::kCount: break;
  }
  return v * fade_;
}

}  // namespace synth
