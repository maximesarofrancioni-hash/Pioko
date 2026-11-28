// config.h - Pyoko's Pi 4 kernel: build-time settings
#pragma once

#define SAMPLE_RATE      48000  // osc440's rate (third_party/pico-osc440/src/config/audio_config.h)
#define CHUNK_SIZE       256    // I2S DMA chunk in samples (L+R words): 128 frames = 2.7 ms
#define QUEUE_FRAMES     768    // write queue: 16 ms. Lower = less latency, more underrun risk

// Buttons to GND (internal pull-ups). Set to 0 to disable.
#define PIN_NEXT_PRESET  17     // physical pin 11: track 1's next osc440 preset
#define PIN_PLAY         27     // physical pin 13: play / pause

// sampler memory: mono float frames, from the heap. 16 M frames = 64 MB = 5.8 min at 48 kHz.
#define SAMPLER_FRAMES   (16u << 20)
