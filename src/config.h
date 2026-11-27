// config.h - filter-lab build-time settings
#pragma once

#define SAMPLE_RATE      48000
#define CHUNK_SIZE       256    // I2S DMA chunk in samples (L+R words): 128 frames = 2.7 ms
#define QUEUE_FRAMES     768    // write queue: 16 ms. Lower = less latency, more underrun risk

// Buttons to GND (internal pull-ups). Set to 0 to disable.
#define PIN_NEXT_FILTER  17     // physical pin 11
#define PIN_HOLD         27     // physical pin 13

#define DEFAULT_BARS_PER_FILTER 4

// sampler memory: mono float frames, from the heap. 16 M frames = 64 MB = 5.8 min at 48 kHz.
#define SAMPLER_FRAMES   (16u << 20)
