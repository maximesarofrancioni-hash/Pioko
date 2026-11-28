#pragma once

#include <cstdint>

namespace clocks {

// Sets clk_sys. Above 200 MHz it first raises the core voltage and slows the
// flash interface by the same factor as the core speeds up, so that flash keeps
// the timing it had at boot. Returns false if the PLL cannot make that frequency.
bool set_sys_clock(uint32_t khz);

}  // namespace clocks
