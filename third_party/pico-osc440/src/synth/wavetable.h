#pragma once

#include <cstdint>

namespace synth {

// How the wave position moves through the table:
// smooth crossfades neighbouring waves; stepped jumps between them (PPG style).
enum class WaveScan : uint8_t { kSmooth = 0, kStepped = 1 };

// A table of single-cycle waves, stored at several band limits (mip levels).
// Within a level, wave w starts at data + w * stride(bits). Each wave has 2
// guard samples before it and 3 after (copies from its other end), so 6-point
// interpolation around any index 0 .. size-1 never wraps.
struct Wavetable {
  static constexpr int kMaxLevels = 8;
  static constexpr int kGuardsBefore = 2;
  static constexpr int kGuardsAfter = 3;
  static constexpr uint32_t stride(uint32_t bits) { return (1u << bits) + kGuardsBefore + kGuardsAfter; }

  struct Level {
    const int16_t* data;    // sample 0 of wave 0 (the guards sit before it)
    uint8_t bits;           // log2 of samples per wave
    uint8_t harmonics;      // highest harmonic present
  };

  uint8_t waves;
  uint8_t levels;           // level 0 has the most harmonics
  Level level[kMaxLevels];
};

}  // namespace synth
