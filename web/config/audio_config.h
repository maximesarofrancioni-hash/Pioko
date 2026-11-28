#pragma once

#include <cstddef>
#include <cstdint>

// System-wide audio constants, shared by the synth, the DSP helpers and the I/O.
namespace audio {

constexpr uint32_t kSampleRate = 44100;   // filter-lab web build: the page runs at 44.1 kHz (the SP Vinyl Sim rate)
constexpr size_t   kBlockSize  = 16;   // frames per render() call (333 us)

}  // namespace audio
