#pragma once

#include <cstdint>

namespace synth {

// Low-frequency oscillator, advanced once per block. Output -1..+1, faded in
// over the delay time after each note-on.
class Lfo {
 public:
  enum class Shape : uint8_t {
    kSine, kTriangle, kSaw, kSawDown, kSquare, kPulse,   // pulse: 25% duty
    kSampleHold, kSmoothRandom, kExpRamp, kStepped,      // stepped: 8-step staircase
    kCount
  };

  void set_rate(float hz) { rate_hz_ = hz; }
  void set_shape(Shape shape) { shape_ = shape; }
  void set_delay(float seconds);      // fade-in time after note_on(); 0: none
  void retrigger() { phase_ = 0.0f; }
  void seed(uint32_t seed) { random_ = seed ? seed : 1u; }

  // Starts the delay fade-in; with reset, also restarts the cycle
  void note_on(bool reset) {
    fade_ = delay_step_ >= 1.0f ? 1.0f : 0.0f;
    if (reset) retrigger();
  }

  // `rate_octaves` modulates the rate (+1 = twice as fast). Returns the value
  // for this block.
  float process(float rate_octaves);

 private:
  float next_random();

  float rate_hz_ = 1.0f;
  float phase_ = 0.0f;     // 0..1
  float held_ = 0.0f;      // sample & hold value
  float previous_ = 0.0f;  // smooth random: the value it glides from
  float fade_ = 1.0f, delay_step_ = 1.0f;
  uint32_t random_ = 0x12345678u;
  Shape shape_ = Shape::kSine;
};

}  // namespace synth
