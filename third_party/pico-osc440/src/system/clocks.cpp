#include "system/clocks.h"

#include "hardware/clocks.h"
#include "hardware/structs/qmi.h"
#include "hardware/vreg.h"
#include "hardware/sync.h"
#include "pico/stdlib.h"

namespace clocks {
namespace {

constexpr uint32_t kOverclockAboveKhz = 200000;

// Multiplies the QSPI clock divider and the read-sample delay by `factor`, so the
// flash SCK frequency and the sample point stay where they were, in absolute time.
// Runs from SRAM: flash must not be read while its timing changes.
void __not_in_flash_func(scale_flash_timing)(uint32_t factor) {
  const uint32_t irq_state = save_and_disable_interrupts();
  uint32_t timing = qmi_hw->m[0].timing;
  uint32_t clkdiv = (timing & QMI_M0_TIMING_CLKDIV_BITS) >> QMI_M0_TIMING_CLKDIV_LSB;
  uint32_t rxdelay = (timing & QMI_M0_TIMING_RXDELAY_BITS) >> QMI_M0_TIMING_RXDELAY_LSB;
  clkdiv *= factor;
  rxdelay *= factor;
  if (clkdiv > 255) clkdiv = 255;
  if (rxdelay > 7) rxdelay = 7;
  timing &= ~(QMI_M0_TIMING_CLKDIV_BITS | QMI_M0_TIMING_RXDELAY_BITS);
  timing |= (clkdiv << QMI_M0_TIMING_CLKDIV_LSB) | (rxdelay << QMI_M0_TIMING_RXDELAY_LSB);
  qmi_hw->m[0].timing = timing;
  __dsb();
  __isb();
  restore_interrupts(irq_state);
}

}  // namespace

bool set_sys_clock(uint32_t khz) {
  uint vco, div1, div2;
  if (!check_sys_clock_khz(khz, &vco, &div1, &div2)) return false;

  if (khz > kOverclockAboveKhz) {
    // NOTE: not yet tested on hardware. 1.30 V is the highest setting the
    // regulator allows without unlocking; many RP2350s reach ~300 MHz there.
    vreg_set_voltage(VREG_VOLTAGE_1_30);
    sleep_ms(10);
    const uint32_t boot_khz = clock_get_hz(clk_sys) / 1000;
    scale_flash_timing((khz + boot_khz - 1) / boot_khz);   // round up: slower is safe
  }
  set_sys_clock_pll(vco, div1, div2);
  return true;
}

}  // namespace clocks
