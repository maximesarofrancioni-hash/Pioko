#include "io/i2s_out.h"

#include "hardware/clocks.h"
#include "hardware/dma.h"
#include "hardware/irq.h"
#include "hardware/sync.h"
#include "i2s_out.pio.h"

namespace io::i2s_out {
namespace {

alignas(4) int32_t s_buffer[2][kWords];
int s_dma[2] = {-1, -1};
PIO s_pio;
uint s_sm;

// Written by the DMA interrupt and by the main loop
volatile bool s_filled[2] = {true, true};   // both start as (silent) valid data
volatile int s_free = -1;                   // buffer to fill next, -1 if none
volatile uint32_t s_underruns = 0;
int s_writing = -1;                         // buffer handed out by acquire()

void __not_in_flash_func(dma_handler)() {
  for (int i = 0; i < 2; ++i) {
    if (!dma_channel_get_irq0_status(s_dma[i])) continue;
    dma_channel_acknowledge_irq0(s_dma[i]);

    // Buffer i has finished; the chain has already started the other one.
    dma_channel_set_read_addr(s_dma[i], s_buffer[i], false);
    const int playing = 1 - i;
    if (!s_filled[playing]) ++s_underruns;   // it is replaying stale samples
    s_filled[playing] = false;
    s_free = i;
  }
}

}  // namespace

bool init(const Config& config) {
  // 64 BCLK per frame, 2 PIO cycles per BCLK
  const uint32_t pio_hz = config.sample_rate * 64 * 2;
  const uint32_t sys_hz = clock_get_hz(clk_sys);
  if (sys_hz % pio_hz != 0) return false;

  s_pio = config.pio;
  const int sm = pio_claim_unused_sm(s_pio, false);
  if (sm < 0) return false;
  s_sm = static_cast<uint>(sm);
  const uint offset = pio_add_program(s_pio, &i2s_out_program);
  i2s_out_program_init(s_pio, s_sm, offset, config.pin_data, config.pin_bclk,
                       static_cast<float>(sys_hz / pio_hz));

  for (int i = 0; i < 2; ++i) {
    s_dma[i] = dma_claim_unused_channel(false);
    if (s_dma[i] < 0) return false;
  }
  for (int i = 0; i < 2; ++i) {
    dma_channel_config c = dma_channel_get_default_config(s_dma[i]);
    channel_config_set_transfer_data_size(&c, DMA_SIZE_32);
    channel_config_set_read_increment(&c, true);
    channel_config_set_write_increment(&c, false);
    channel_config_set_dreq(&c, pio_get_dreq(s_pio, s_sm, true));
    channel_config_set_chain_to(&c, s_dma[1 - i]);
    dma_channel_configure(s_dma[i], &c, &s_pio->txf[s_sm], s_buffer[i], kWords, false);
    dma_channel_set_irq0_enabled(s_dma[i], true);
  }
  irq_add_shared_handler(DMA_IRQ_0, dma_handler, PICO_SHARED_IRQ_HANDLER_DEFAULT_ORDER_PRIORITY);
  irq_set_enabled(DMA_IRQ_0, true);
  return true;
}

void start() {
  dma_channel_start(s_dma[0]);
  pio_sm_set_enabled(s_pio, s_sm, true);
}

int32_t* __not_in_flash_func(acquire)() {
  const int i = s_free;
  if (i < 0) return nullptr;
  s_writing = i;
  return s_buffer[i];
}

void __not_in_flash_func(commit)() {
  const uint32_t irq_state = save_and_disable_interrupts();
  s_filled[s_writing] = true;
  // If the other buffer freed up while we were rendering, leave it handed out
  if (s_free == s_writing) s_free = -1;
  restore_interrupts(irq_state);
  s_writing = -1;
}

uint32_t underruns() { return s_underruns; }

}  // namespace io::i2s_out
