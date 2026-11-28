# osc440

A voice module for an RP2350: two cores each render a group of 3rd Wave-style
voices (3 oscillators, wavetable or VA, with FM and sync; SEM-style SVF + ladder;
4 DADSRs, 3 wave envelopes, 4 LFOs, 16-slot matrix with audio-rate sources),
mixed and played through I2S to a PCM5102. The demo
retriggers a 4-note chord every second with the patch in `synth/demo_patch.h`.

The synth knows nothing about the output, and the output knows nothing about
the synth. Everything under `synth/` and `engine/` also builds on a desktop,
where the tests run it.

## Layout

```
src/
  main.cpp                 control loop (core 0), per-block render on both cores
  config/
    audio_config.h         sample rate (48 kHz), block size (16)
    board_config.h         pins, system clock
    platform.h             SRAM placement macros (empty on the host)
  engine/                  portable: runs on the device and on the host
    voice_engine.h/.cpp    2 cores x VoiceGroup, per-core event queues, mix
    voice_allocator.h/.cpp which core/voice plays each note, stealing
    event.h                note on/off and parameter events
  synth/                   portable
    voice_group.h          VoiceGroup interface + Bus (L, R, send A, send B)
    triple_wave_group.*    the 3rd Wave-style voice (see below)
    demo_patch.h           the demo sound, shared by firmware, tests, estimator
    ladder.h               4-pole ladder (soft clip, resonance compensation), 1x/2x
    oscillator.h/.cpp      wavetable / sine / triangle / saw / pulse / supersaw /
                           noise / coloured noise; FM in, sync in and out
    dadsr.h/.cpp           delay-attack-decay-sustain-release, exponential or PPG
    mseg.h/.cpp            6-stage loopable wave envelope
    lfo.h/.cpp             10 shapes, delay (fade-in), key reset
    mod_matrix.h           16 slots; sources and destinations, audio-rate ones
    poly_group.h/.cpp      simpler voice: osc -> SVF -> VCA, 2 ADSRs
                           (WavetableGroup, SawGroup)
    wavetable.h            table layout, scan modes
    wavetable_osc.h/.cpp   PPG-style oscillator: smooth or stepped scan
    wavetable_data.h/.cpp  generated: 64 waves x 7 mip levels (140 KB, SRAM)
    wavetable_plaits.h/.cpp generated: Plaits' bank A in the same format (140 KB)
    saw_osc.h/.cpp         band-limited saw (from PRA32-U2's oscillator)
    saw_tables.h/.cpp      generated band-limited saw tables (VA saw, pulse, supersaw)
    adsr.h/.cpp            analog-style ADSR, block rate
    svf.h                  ZDF state-variable filter (Simper), multimode
  preset/                  host only (not in the firmware)
    preset_json.h/.cpp     presets as JSON files: write, read, check
  dsp/
    interpolate.h          6-point Lagrange interpolation
    halfband.h             2x up/downsampling, polyphase IIR (7 coefficients)
    spsc_queue.h           lock-free queue between cores
    fast_math.h            exp2, tan, sin, tanh, soft clip (no library calls)
    convert.h              float -> int32, L/R interleave
    buffer.h/.cpp          clear() that stays out of flash
  system/
    clocks.h/.cpp          sys clock; for overclocking: vreg + flash timing
  io/
    i2s_out.pio/.h/.cpp    PIO I2S master + DMA ping-pong
tools/
  gen_wavetable.py         regenerates wavetable_data.h/.cpp
  gen_saw_tables.py        regenerates saw_tables.h/.cpp
  load_estimate.py         cycle estimate on an emulated Cortex-M33
test/                      host tests (see below)
demo/                      demo renderers (station melodies, showcase tracks)
presets/                   the demos' sounds as JSON (format in presets/README.md)
```

## How the two cores share the work

Per block, in lockstep through the SIO FIFO:

- core 0: queue events → wake core 1 → render its group → wait → mix → int32 → I2S
- core 1: wait → render its group → signal done

Each group is touched only by the core that renders it. The control side
(core 0's main loop now, the SPI command parser later) allocates voices and
queues events; each core applies its own events at the start of its block,
at most 32 per block (the queues hold 255): a whole preset (179 parameters)
loads over 6 blocks, 2 ms, adding ~12% to each instead of overrunning one.
Every function on the per-block path runs from SRAM, event handlers included.

## The 3rd Wave-style voice (TripleWaveGroup)

Modelled on the Groove Synthesis 3rd Wave's voice (after the Sound On Sound
review), without its effects:

- 3 oscillators, each **wavetable** (our table or Plaits' bank A) or **VA**: sine
  (phase distortion), triangle (skewable), saw, pulse (PWM), supersaw (7 saws,
  detune), white or coloured noise. Pulse width shapes most modes.
- **Linear FM** from the neighbour (1 <- 3, 2 <- 1, 3 <- 2) and **hard sync**
  (2 to 1, 3 to 2), with polyBLEP on the resets
- a 6-stage loopable **wave envelope** per oscillator
- **SEM-style SVF** (low pass -> notch -> high pass, or band pass; fully open
  low pass with no resonance takes it out of the path) **then** the
  4-pole ladder (saturation, resonance compensation, 1x/2x), or in parallel
- **4 DADSRs** (1: filters, 2: VCA, 3-4: free), exponential or PPG (linear)
- **4 LFOs**: sine, triangle, saw up/down, square, 25% pulse, sample & hold,
  smooth random, exponential ramp, 8 steps; delay (fade-in), key reset
- **16-slot matrix**. Block-rate sources (LFOs, envelopes, velocity, key,
  wheel, pressure, expression) go anywhere; the oscillators and noise are
  **audio-rate** sources, and on FM, levels (AM, ring mod) and the VCA they are
  applied per sample.
- constant-power pan; an oscillator nobody can hear is not rendered

Parameter ids are in `triple_wave_group.h` (`osc_param`, `env_param`,
`lfo_param`, `matrix_param`, ...).

**Why the ladder runs at 2x:** its saturation creates harmonics above Nyquist.
At 1x, a 3-oscillator note at 2 kHz with the filter driven aliased at -55 dB;
at 2x (polyphase IIR halfband, -88 dB stopband) the same note measures -102 dB.
`kParamLadderOversample = 0` trades that for ~19% less CPU per voice.

## The oscillators

PPG-style: 64 single-cycle waves, a position scanning them, **smooth** (crossfade
the two nearest waves) or **stepped** (nearest wave only, with the PPG-style
jumps). The filter envelope can sweep the position (`kParamWaveEnvAmount`).

Each wave is stored at 7 band limits (64, 32 ... 1 harmonics); the pitch picks
the fullest one whose top harmonic stays under 20 kHz. Tables hold 8 samples per
top harmonic and are read with **6-point Lagrange interpolation**. Measured worst
case over all waves, at the top of each level's pitch range:

| interpolation | table | worst non-harmonic |
|---|---|---|
| linear | 136 KB | -35 dB |
| 4-point cubic (Catmull-Rom) | 136 KB | -44 dB |
| 6-point Lagrange (used, fixed point) | 140 KB | -71 dB |
| 4-point Lagrange, 16x tables | 260 KB | -78 dB |

The interpolation runs in fixed point: the weights for 2048 positions between
samples are precomputed in Q15 (24 KB), and each pair of taps is one SMLAD
(dual 16-bit multiply-accumulate) straight from the int16 table. Against the
float version: -95 dB rms difference, every aliasing test unchanged, about 40%
less time per wavetable sample.

Table 0 is an original set (sine → saw → square → narrow pulse → formant
sweep). Table 1 is **Plaits' bank A** (MIT, see THIRD_PARTY.md), converted
into the same format, so it plays with the same quality. Plaits' own oscillator
(128-sample integrated tables, linear interpolation, no mips) was not adopted:
simulated, it aliases at about -28 dB on bright waves at A4.

The VA modes are built from band-limited saw tables (PRA32-U2's method): saw
directly, pulse as the difference of two shifted saws (no DC at any width),
supersaw as 7 of them. Measured worst non-harmonic component, A1 to A7:

| mode | worst | |
|---|---|---|
| saw, 50% pulse, sine | -75 to -88 dB | |
| 10% pulse | -66 dB (A4) | |
| triangle | -57 dB at A6, -42 dB at A7 (above 16 kHz) | polyBLAMP corners |
| hard sync, wavetable | -75 / -62 / -48 dB at A2 / A4 / A6 | polyBLEP resets |
| hard sync, saw | -68 / -56 / -44 dB at A2 / A4 / A6 | |
| Plaits bank (5 waves, A1 to E7) | below -71 dB | |

## Build

```
export PICO_SDK_PATH=/path/to/pico-sdk     # tested with 2.2.0
cmake -S . -B build -DPICO_BOARD=pico2
make -C build -j
```

Flash `build/osc440.uf2`. Status on UART0 (GP0, 115200): underruns and dropped events.

Overclocked to 307.2 MHz (core at 1.30 V, flash divider and sample delay scaled
so the flash keeps its boot timing; **not yet tested on hardware**):

```
cmake -S . -B build-oc -DPICO_BOARD=pico2 -DOSC440_OVERCLOCK=ON
make -C build-oc -j
```

## Wiring (PCM5102 module)

| PCM5102 | RP2350 |
|---|---|
| DIN | GP9 |
| BCK | GP10 |
| LRCK | GP11 |
| XSMT | GP22 (or tie high) |
| SCK | GND |

## Host tests

```
./test/run_host_test.sh
```

- `unit_tests`: exp2 accuracy, filter response and stability, envelope timing
- `voice_tests`: ladder (gain, slope, compensation, self-oscillation, stability
  under drive), halfband (transparency, -88 dB both ways), 2x ladder, block =
  per-sample processing, SVF modes, wave envelope stages and loops, LFO rates,
  mod matrix
- `wavetable_tests`: mip level choice, smooth scan = exact crossfade, stepped scan
  = nearest wave, full-table sweep adds no jumps, equal level across waves
- `engine_tests`: queue (incl. 1M items across two threads), allocator (alternation,
  retrigger, release, stealing), and **2 cores mixed = 1 group playing all voices**
  for both voice types
- `osc_tests`: oscillator modes (levels, no DC, FM pitch, supersaw, noise colour),
  Plaits bank levels, DADSR timing (delay, exponential, PPG), LFO shapes and
  delay, ring modulation and FM through the real voice
- `preset_tests`: JSON presets round-trip bit for bit (parameters and audio),
  partial files take defaults, typos and unknown names are reported
- `check_wav.py`: pitch and aliasing (non-harmonic components below -70 dB, or
  the per-family limits in the table above) for 92 renders: wavetables, VA
  modes, Plaits waves, sync, and the 3-oscillator voice
- listening: `build-host/demo_chord.wav` (what the firmware plays),
  `sweep_smooth.wav`, `sweep_stepped.wav`

## Demos

`demo/station_demo.cpp` renders original departure melodies and chimes in the
style of Tokyo's train stations, with four engines standing in for the four
voice modules (FM bell, mallet arpeggio, bass, pad) and a stand-in for the
master's effects (ping-pong delay, FDN hall):

```
g++ -std=c++17 -O2 -Isrc demo/station_demo.cpp src/synth/*.cpp src/engine/*.cpp src/dsp/buffer.cpp src/preset/preset_json.cpp -o station_demo
./station_demo out/
```

`demo/showcase.cpp` renders four product-page style tracks (acid techno,
wavetable ambient, sync/FM electro-funk, supersaw/ring-mod house), four tracks
each, drums included, all from the voice. Per-step parameter locks and
automation go through `set_param`, as the master's will. Ducking, delay,
reverb and the limiter are stand-ins for the master (`demo/demo_common.h`).
`demo/indian.cpp` renders three pieces in Indian classical style, original
phrases in traditional ragas with Sa on D: a Bhairav alap (tanpura, shruti box,
bansuri, swarmandal), a Yaman gat in teentaal (tanpura, sitar, both tabla drums)
and a Bhupali santoor piece over keherwa. Meend, kan and andolan are pitch
bends: automation of the oscillators' tune, per block, through `set_param`.
`demo/indiepop.cpp` renders three synth indie pop songs: a night-drive
arpeggio piece, a swung bedroom-pop track with an FM e-piano and a scooped
whistle lead, and a big supersaw chorus.
The song machinery these share is in `demo/song.h`.

Build it the same way, from `demo/showcase.cpp`, adding `src/preset/preset_json.cpp`
(both demos also do this). Both write their sounds as presets and can play
edited ones back: see `presets/README.md`.

## Load estimate (no hardware)

```
pip install unicorn capstone pyelftools
python3 tools/load_estimate.py build/osc440.elf --profile [--patch demo|full|worst] [--scan stepped] [--ladder 1x]
```

Runs the firmware ELF on an emulated Cortex-M33: core 0's real per-block
function and core 1's render, with the same first-order cost model as PRA32-U2's
estimator. XIP flash waits, bus contention and interrupts are not modelled.
Patches: `demo` (the firmware patch, 5 slots); `full` (all 16 slots in use);
`worst` (supersaw, FM and sync on every oscillator, 4 audio-rate slots).

Cycles per voice and sample, and voices per module within 80% of the budget:

| patch | scan, ladder | cycles | 153.6 | 200 | 250 | 307.2 MHz |
|---|---|---|---|---|---|---|
| demo | smooth, 2x | 638 | 6 | 10 | 12 | 14 |
| full | smooth, 2x | 658 | 6 | 8 | 12 | 14 |
| full | stepped, 2x | 611 | 8 | 10 | 12 | 16 |
| full | smooth, 1x | 536 | 8 | 12 | 14 | 18 |
| full | stepped, 1x | 488 | 10 | 12 | 16 | 20 |
| worst | smooth, 2x | 1195 | 4 | 4 | 6 | 8 |
| worst | stepped, 1x | 1007 | 4 | 6 | 8 | 8 |

What brought it there from 864 (full, smooth, 2x): the halfband filters run
sample by sample with their state in registers (-100), fixed-point SMLAD
interpolation (-85), and nothing computed that nobody listens to: unused
LFOs, envelopes and wave envelopes, a fully open SVF (skipped, crossfaded
in and out), oscillator copies nobody reads (-20).

The firmware's output in the emulator matches the host build (with FMA)
bit for bit.

## What was kept from PRA32-U2

The mipmapped saw (one table per band of notes, longer tables where there are
more harmonics) and the 153.6 MHz clock that makes the I2S divider an exact
integer. The filter, envelopes, wavetable oscillator and engine are new.

## Next

- Replace `io/i2s_out` with a TDM lane slave (`io/tdm_lane`) for the master.
- SPI command parser feeding `VoiceEngine` (replaces `sequence()` in main.cpp).
- Tables in flash, the active ones copied to SRAM on patch change. Two tables
  (280 KB) are in SRAM now; .data + .bss is 384 KB of 520 KB.
- Check the estimates on hardware (XIP misses, the two cores sharing SRAM
  banks and the wavetables are not modelled).
- picoboot3: move the app to the 32 KB offset in the linker script.
