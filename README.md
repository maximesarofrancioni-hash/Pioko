# Pyoko ぴょこ

(Some files keep their old filter-lab names.)

A two-track groovebox: osc440's 3rd Wave-style voice and a slicing sampler, each through two
[sst-filters](https://github.com/surge-synthesizer/sst-filters) models, sequenced by Euclidean patterns, Mutable
Instruments Marbles or a shift register, modulated by Tides 2, through Beads. It runs as a bare-metal Pi 4 kernel
([Circle](https://github.com/rsta2/circle)) into a PCM5102 DAC, and in the browser with a display and an effects
page (see [Browser version](#browser-version)). Both run the same engine, `core/`.

```
            SEQ (per track: Euclid | Marbles | shift register)
Track 1:  OSC (osc440, 16 voices) -> VCF (2 sst filters) -> AMP (env 2) --+
Track 2:  SAMPLER                 -> VCF (2 sst filters) -> AMP (att/dec) +-> FX: Beads (-> Vinyl Sim, browser only)
```

## The Pi 4 kernel

It boots into the **Lofi hip hop** scene: 78 BPM, 62% swing, the e-piano on Fmaj9 Em9 Dm9 Cmaj9 through warm low
passes, the demo break on every 16th at -3 st varispeed. The serial port plays it, the ACT LED flashes on every bar,
and the DSP load (% of one core), clock and SoC temperature are logged every 10 s. The page's SP-303 Vinyl Sim is
JavaScript and does not run on the Pi yet; neither do the display and the screen pages.

### Wiring (Pi 4 -> PCM5102)

| Pi 4   | Pin | PCM5102 |
|--------|-----|---------|
| GPIO18 | 12  | BCK     |
| GPIO19 | 35  | LCK     |
| GPIO21 | 40  | DIN     |
| 3V3    | 1   | VIN     |
| GND    | 6   | GND     |

On GY-PCM5102 boards, connect SCK to GND and set the back jumpers:
1L 2L 3H 4L.

Optional buttons to GND: GPIO17 (pin 11) for track 1's next preset, and GPIO27 (pin 13) for play / pause.

UART: USB-serial adapter on GPIO14/15, 115200 baud.

### Keys (serial terminal)

```
space      play / pause                 L        Lofi hip hop scene
t / T      tempo -/+ 2 BPM              s / S    swing -/+ 2%
1 / 2      edit track 1 (osc440) / 2 (sampler)
i          status                       ?        help

the edited track
q          sequencer: Euclid > Marbles > shift register
z / x      hits -/+     , / .   steps -/+     v / b   rotate -/+   (its Euclidean pattern)
[ / ]      filter 1 model -/+           { / }    filter 2 model -/+
f / F      filter 1 cutoff -/+ 3 st     g / G    filter 2 cutoff -/+ 3 st
e / E      filter 1 resonance -/+       r / R    filter 2 resonance -/+
p          series / parallel

track 1, osc440                         track 2, the sampler
n / N      next / previous preset       k        demo break
c          Am F C G / 9th chords        w        resample 2 bars (from the next downbeat)
y          chords / arp / bass          j / J    pitch -/+ 1 st
o / O      octave -/+                   m        mute

B          Beads on / off
```

### Build without a compiler (GitHub Actions)

1. Push this folder to a GitHub repo.
2. Go to Actions -> build. It runs on every push, or start it with
   "Run workflow".
3. Download the `filter-lab-sdcard` artifact and copy it to a FAT32 SD card.

### Build locally

```sh
git clone https://github.com/rsta2/circle.git
mkdir libs && cd libs
git clone https://github.com/surge-synthesizer/sst-filters.git
git clone https://github.com/surge-synthesizer/sst-basic-blocks.git
git clone https://github.com/simd-everywhere/simde.git
git clone https://github.com/pichenettes/eurorack.git
git clone https://github.com/pichenettes/stmlib.git eurorack/stmlib
cd ..
cp circle-config/Config.mk circle/
(cd circle && ./makeall && cd boot && make firmware && make armstub64)
make -C src
```

Toolchain: Arm GNU `aarch64-none-elf`.

The engine (`core/`, osc440 in `third_party/pico-osc440`, eurorack) is built hosted, with libstdc++ headers, into
`src/build/`, while the rest of the kernel stays freestanding Circle. Nothing from libstdc++ is linked: only
`libm`, which Circle links by default (`STDLIB_SUPPORT=1`). `circle-config/Config.mk` raises Circle's
`KERNEL_MAX_SIZE` to 32 MB for the engine's data; the build checks the kernel fits it.

### Host preview

```sh
make -C host                          # needs libs/ as above
./host/render out.wav 16 lofi         # 16 s of the lofi scene; without "lofi", the engine's defaults
```

This renders the same engine at 48 kHz, as the Pi does, to a WAV file on your PC, to check a change before flashing.

### Sampler

`core/sampler.h/.cpp`: one mono sample in RAM (on the Pi `SAMPLER_FRAMES` floats from the heap, 64 MB = 5.8 min at
48 kHz by default). Slice mode cuts the START..END region into 4/8/16 slices or at its onsets (auto: energy-rise
detection, up to 16), and each hit plays the slice under its step (or a random one), choking the previous one; one
shot plays the region. Pitch +-24 st + fine cents (Hermite), varispeed or keep time (two 40 ms grains), attack, decay
(0 = hold), reverse, 4 voices. Resample records the mix from the next downbeat for 1/2/4 bars. A synthesised one-bar
break is loaded at boot. Loading files from the SD card is not in the kernel yet (the page loads any audio file).

## License

sst-filters is GPL-3, so this project is GPL-3 too.

## Browser version on a Pi 4

For quick tests, the full browser version runs on a Pi 4 with Raspberry Pi OS
(with desktop), with no build step and no DAC: sound comes out of the
headphone jack or HDMI.

```sh
git clone https://github.com/maximesarofrancioni-hash/Pioko.git
cd Pioko
./pi/run-browser.sh             # full screen; Alt+F4 quits
./pi/run-browser.sh --window    # in a window
./pi/install-autostart.sh       # optional: start it at every desktop login
```

Press Play on the page. Pick the audio output from the speaker icon in the
taskbar.

## Browser version

The browser version is a different instrument from the Pi kernel, laid out as a signal path in two tracks:

```
            SEQ (per track: Euclid | Marbles | shift register)
Track 1:  OSC (osc440, 16 voices) -> VCF (2 sst filters) -> AMP (env 2) --+
Track 2:  SAMPLER                 -> VCF (2 sst filters) -> AMP (att/dec) +-> FX: Beads -> Vinyl Sim
```

Each track's VCF is two sst-filters models (any of the 27 the Pi kernel tours, `synth/sst_filter.cpp`) in series or
parallel. On the osc track they sit in each osc440 voice (filter 1 in its SVF's place, filter 2 in its ladder's, with
envelope 1, key tracking and the matrix); on the sampler track they filter its output, with their own filter
envelope. Each track picks its sequencer: its own Euclidean pattern, Marbles (T1 / X1 and T3 / X3 play osc440, T2
plays the sampler with X2 choosing the slice) or the shift register (notes for osc440, its low byte choosing the
slice for the sampler). The FX page switches Beads and the Vinyl Sim on and off.

Page keys, left to right: SEQ (SEQ, MARBLES, SHIFT) | GEN (OSC, SAMPLER) | VCF (FILTER) | AMP (ENV) | MOD (TIDES,
MATRIX) | FX. The header holds tempo, swing and the track the SEQ, FILTER and ENV pages show. SEQ's **Lofi hip hop**
button sets up a whole sequence: 78 BPM, 62% swing, the e-piano on Fmaj9 Em9 Dm9 Cmaj9 through warm low passes, the
demo break slowed by varispeed, the Vinyl Sim's noise and wow.

The display is 150 x 112, shown at 4x on a 600 x 450 AMOLED. Its footer has two parameter lines: line 1 is what
the four front panel encoders turn on the current page, line 2 the four encoders under the screen (the page maps eight
parameters per page in `PAGES`; the panel under the display mirrors them).

Sources: osc440 is in `third_party/pico-osc440`; built with `OSC440_SST_FILTERS` it uses the sst filters (without
the flag it keeps its own SVF and ladder, so its RP2350 build is unchanged). The Mutable Instruments code comes from
pichenettes/eurorack (MIT), unmodified, via `core/mods/` and `web/tides_view/`. `web/build.sh` (needs Emscripten)
needs `LIBS_DIR` (sst-filters, sst-basic-blocks, simde), `MPK4` (open-mpk4) and `MI` (eurorack with stmlib cloned into
it), compiles the engine, the display and the TIDES view to WebAssembly and writes one self-contained
`web/filter-lab.html`, which runs in an AudioWorklet.

A prebuilt `web/filter-lab.html` is checked in, so the browser version runs without any checkout: open it in a
browser and press Play.
