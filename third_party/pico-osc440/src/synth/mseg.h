#pragma once

#include <cstdint>

namespace synth {

// 6-stage loopable envelope, advanced once per block (as the wave envelopes of
// PPG-style synths). Stages 0..4 run while the gate is held; with a loop set,
// stages loop_start..loop_end repeat until release. Stage 5 is the release:
// gate off jumps there from wherever the envelope is. Each stage ramps linearly
// from the current level to its own level over its time.
class Mseg {
 public:
  static constexpr int kStages = 6;
  static constexpr int kReleaseStage = kStages - 1;

  void set_stage(int stage, float level, float seconds);
  // Loop stages start..end (both <= 4) while the gate is held; start > end: no loop
  void set_loop(int start, int end) { loop_start_ = start; loop_end_ = end; }

  void gate_on();
  void gate_off();

  // Level at the end of this block
  float process();

  float level() const { return level_; }
  bool idle() const { return stage_ < 0; }
  int stage() const { return stage_; }

 private:
  void enter(int stage);

  float level_ = 0.0f;
  float from_ = 0.0f;       // level when the current stage began
  float t_ = 0.0f;          // progress through the current stage, 0..1
  int stage_ = -1;          // -1: idle; kStages: holding at the end of stage 4
  int loop_start_ = 1, loop_end_ = 0;
  float level_of_[kStages] = {1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 0.0f};
  float step_of_[kStages] = {1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f};   // progress per block
};

}  // namespace synth
