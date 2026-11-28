#pragma once

// Lets the synth code build both for the RP2350 and on a desktop host (for tests).
#if defined(PICO_ON_DEVICE) && PICO_ON_DEVICE
  #include "pico.h"
  // Tables read at audio rate: keep them in SRAM, away from XIP cache misses
  #define AUDIO_RAM_DATA(group) __not_in_flash(group)
  // Functions on the audio path: run them from SRAM
  #define AUDIO_HOT(func) __not_in_flash_func(func)
  // Same, as a plain attribute (for template members, whose names can't be section names)
  #define AUDIO_HOT_ATTR __attribute__((section(".time_critical.audio")))
#else
  #define AUDIO_RAM_DATA(group)
  #define AUDIO_HOT(func) func
  #define AUDIO_HOT_ATTR
#endif
