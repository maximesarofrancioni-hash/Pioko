#pragma once

#include <cstddef>
#include <cstdint>

// System-wide audio constants, shared by the synth, the DSP helpers and the I/O.
namespace audio {

constexpr uint32_t kSampleRate = 48000;
constexpr size_t   kBlockSize  = 16;   // frames per render() call (333 us)

}  // namespace audio
