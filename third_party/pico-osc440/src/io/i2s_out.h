#pragma once

#include <cstddef>
#include <cstdint>

#include "hardware/pio.h"

// Stereo I2S output, fed one block at a time.
//
// Two DMA buffers play in turn (each chained to the other). When one finishes,
// it becomes free: acquire() hands it out, the caller fills it with interleaved
// L/R 32-bit samples, then commit(). If a buffer is not refilled in time, the
// DMA plays it again and underruns() counts it.
namespace io::i2s_out {

struct Config {
  PIO pio;
  uint32_t pin_data;
  uint32_t pin_bclk;      // LRCK is pin_bclk + 1
  uint32_t sample_rate;
};

// Frames per buffer, and 32-bit words per buffer (L/R interleaved)
constexpr size_t kFrames = 16;
constexpr size_t kWords = kFrames * 2;

// Returns false if no state machine or DMA channel is free, or if the system
// clock is not an exact multiple of the bit clock (the output would jitter).
bool init(const Config& config);
void start();

// The buffer to fill next (kWords words), or nullptr if none is free yet.
int32_t* acquire();
void commit();

uint32_t underruns();

}  // namespace io::i2s_out
