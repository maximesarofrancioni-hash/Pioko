#pragma once

#include <cstdint>

// Everything that depends on how this board is wired or clocked.
namespace board {

// 153.6 MHz = 25 x 6.144 MHz, where 6.144 MHz is two PIO cycles per BCLK at
// 48 kHz x 64 BCLK. The I2S clock divider is then an exact integer (no jitter).
// 307.2 MHz (divider 50) is the overclocked equivalent: cmake -DOSC440_OVERCLOCK=ON
#if defined(OSC440_OVERCLOCK) && OSC440_OVERCLOCK
constexpr uint32_t kSysClockKhz = 307200;
#else
constexpr uint32_t kSysClockKhz = 153600;
#endif

// I2S to the PCM5102 (tie its SCK pin to GND so it derives MCLK from BCLK)
constexpr uint32_t kPinI2sData = 9;
constexpr uint32_t kPinI2sBclk = 10;   // LRCK must be kPinI2sBclk + 1 (= 11)

// PCM5102 XSMT (soft mute, active low). Set to -1 if it is hard-wired high.
constexpr int kPinDacUnmute = 22;

}  // namespace board
