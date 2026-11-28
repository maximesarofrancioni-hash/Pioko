// Voice module test: two TripleWaveGroups (3rd Wave-style voices), one per core,
// behind a VoiceEngine. A 4-note chord (A3 C#4 E4 A4) is retriggered every
// second, gate held for half of it, with the demo patch (synth/demo_patch.h).
//
// Per block, in lockstep:
//   core 0: queue events -> wake core 1 -> render its group -> wait for core 1
//           -> mix both buses -> int32 -> I2S
//   core 1: wait -> render its group -> signal done

#include <cstdio>

#include "config/audio_config.h"
#include "config/board_config.h"
#include "dsp/convert.h"
#include "engine/voice_engine.h"
#include "hardware/gpio.h"
#include "hardware/sync.h"
#include "io/i2s_out.h"
#include "pico/multicore.h"
#include "pico/stdlib.h"
#include "synth/demo_patch.h"
#include "synth/triple_wave_group.h"
#include "system/clocks.h"

static_assert(io::i2s_out::kFrames == audio::kBlockSize, "I2S buffers hold one block");

namespace {

using Group = synth::TripleWaveGroup;

Group g_group0;
Group g_group1;
engine::VoiceEngine g_engine(g_group0, g_group1);
synth::Bus g_bus0, g_bus1, g_mix;

constexpr uint32_t kBlocksPerSecond = audio::kSampleRate / audio::kBlockSize;
constexpr uint8_t kChord[] = {57, 61, 64, 69};

void __not_in_flash_func(core1_loop)() {
  for (;;) {
    multicore_fifo_pop_blocking_inline();   // block number from core 0
    g_engine.render(1, g_bus1);
    __dmb();                                // bus writes land before core 0 hears "done"
    multicore_fifo_push_blocking_inline(0);
  }
}

__attribute__((noinline)) void setup_patch() {   // out of line: the load estimator calls it
  synth::apply_demo_patch([](uint16_t id, float value) { g_engine.set_param(id, value); });
}

void sequence(uint32_t block) {
  const uint32_t phase = block % kBlocksPerSecond;
  if (phase == 0) for (uint8_t n : kChord) g_engine.note_on(n, 0.9f);
  if (phase == kBlocksPerSecond / 2) for (uint8_t n : kChord) g_engine.note_off(n);
}

// Core 0's share of one block, from SRAM: wake core 1, render, wait, mix, convert
void __no_inline_not_in_flash_func(render_block)(uint32_t block, int32_t* out) {
  __dmb();
  multicore_fifo_push_blocking_inline(block);   // core 1: render this block
  g_engine.render(0, g_bus0);
  multicore_fifo_pop_blocking_inline();         // core 1 done
  __dmb();
  engine::VoiceEngine::mix(g_bus0, g_bus1, g_mix);
  dsp::interleave_s32(g_mix.l, g_mix.r, out, audio::kBlockSize);
}

[[noreturn]] void halt(const char* why) {
  printf("error: %s\n", why);
  for (;;) tight_loop_contents();
}

}  // namespace

int main() {
  clocks::set_sys_clock(board::kSysClockKhz);
  stdio_init_all();   // after the clock change, so the UART baud rate is right

  setup_patch();
  multicore_launch_core1(core1_loop);

  const io::i2s_out::Config i2s = {
      .pio = pio0,
      .pin_data = board::kPinI2sData,
      .pin_bclk = board::kPinI2sBclk,
      .sample_rate = audio::kSampleRate,
  };
  if (!io::i2s_out::init(i2s)) halt("I2S init (clock or resources)");
  io::i2s_out::start();

  if (board::kPinDacUnmute >= 0) {
    gpio_init(board::kPinDacUnmute);
    gpio_set_dir(board::kPinDacUnmute, GPIO_OUT);
    gpio_put(board::kPinDacUnmute, true);
  }

  uint32_t block = 0;
  uint32_t last_status_us = time_us_32();
  for (;;) {
    int32_t* out = io::i2s_out::acquire();
    if (out == nullptr) {
      // Idle until the next buffer frees up: room for a status line once a second
      // (short enough to fit the UART FIFO without blocking)
      if (time_us_32() - last_status_us > 1000000) {
        last_status_us = time_us_32();
        printf("underruns %lu dropped %lu\n", static_cast<unsigned long>(io::i2s_out::underruns()),
               static_cast<unsigned long>(g_engine.dropped_events()));
      }
      continue;
    }

    sequence(block);   // control side (later: the SPI command parser)
    render_block(block++, out);
    io::i2s_out::commit();
  }
}
