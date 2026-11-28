#pragma once

#include <cstddef>

namespace dsp {

// Zeroes n floats. Runs from SRAM and never becomes a memset() call (memset
// lives in flash, where an XIP cache miss would stall the audio core).
void clear(float* dst, size_t n);

// Copies n floats, from SRAM too (never a memcpy() call)
void copy(float* dst, const float* src, size_t n);

}  // namespace dsp
