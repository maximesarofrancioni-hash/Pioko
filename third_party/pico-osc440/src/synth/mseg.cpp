#include "synth/mseg.h"

#include "config/audio_config.h"
#include "config/platform.h"

namespace synth {
namespace {

constexpr float kBlockSeconds = static_cast<float>(audio::kBlockSize) / audio::kSampleRate;
constexpr int kHolding = Mseg::kStages;   // past stage 4 with the gate held, no loop

}  // namespace

void AUDIO_HOT(Mseg::set_stage)(int stage, float level, float seconds) {
  if (stage < 0 || stage >= kStages) return;
  level_of_[stage] = level;
  step_of_[stage] = seconds <= kBlockSeconds ? 1.0f : kBlockSeconds / seconds;
}

void AUDIO_HOT(Mseg::enter)(int stage) {
  stage_ = stage;
  from_ = level_;
  t_ = 0.0f;
}

void AUDIO_HOT(Mseg::gate_on)() { enter(0); }

void AUDIO_HOT(Mseg::gate_off)() {
  if (stage_ >= 0 && stage_ != kReleaseStage) enter(kReleaseStage);
}

float AUDIO_HOT(Mseg::process)() {
  if (stage_ < 0 || stage_ == kHolding) return level_;

  t_ += step_of_[stage_];
  const float target = level_of_[stage_];
  if (t_ < 1.0f) {
    level_ = from_ + (target - from_) * t_;
    return level_;
  }

  level_ = target;   // stage done: where next?
  if (stage_ == kReleaseStage) {
    stage_ = -1;
  } else if (stage_ == loop_end_ && loop_start_ <= loop_end_) {
    enter(loop_start_);
  } else if (stage_ == kReleaseStage - 1) {
    stage_ = kHolding;
  } else {
    enter(stage_ + 1);
  }
  return level_;
}

}  // namespace synth
