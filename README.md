# Pyoko ぴょこ

(The repository and files keep their old filter-lab names.)

A bare-metal Pi 4 kernel ([Circle](https://github.com/rsta2/circle)) for
auditioning the Surge Synth Team's
[sst-filters](https://github.com/surge-synthesizer/sst-filters). A
Euclidean sequencer plays 4 voices through the same filter model, and the
kernel steps to the next model every 4 bars.

## Sound

| Lane  | Source                     | Rhythm         | Notes                    |
|-------|----------------------------|----------------|--------------------------|
| bass  | saw + sub square           | E(5,16)        | A1 riff                  |
| lead  | 2 detuned saws             | E(7,16) rot 2  | A minor pentatonic arp   |
| stab  | 30% pulse                  | E(3,8) rot 1   | 5ths / 3rds              |
| noise | white noise                | E(11,16)       | hats, good for HP/BP/comb |
| accent| -                          | E(3,8)         | +env, +reso, longer decay |

Each lane is one lane of a `QuadFilterUnit`, so all 4 voices run in one
SIMD call. Each lane has its own filter envelope. A triangle LFO sweeps the
base cutoff over 8 bars so you hear each model across its range.
Everything runs at 2x oversampling (96 kHz), which is the rate sst-filters
is tuned for, then sst's `HalfRateFilter` decimates to 48 kHz. Each model
has a measured makeup gain so the tour plays at roughly equal loudness.

There are 27 models: Vember LP12/LP24, Legacy Ladder, 3 Vintage Ladders,
Diode 24/12, K35 LP/HP (MS-20), OB-Xd 2-pole/4-pole/Xpander, Cytomic
SVF, Cascade SVF, Tri-pole, Cutoff Warp, Resonance Warp, BP24, Notch and
Comb. Edit `kFilters[]` in `src/filterlab.cpp` to change the tour.

## Wiring (Pi 4 -> PCM5102)

| Pi 4   | Pin | PCM5102 |
|--------|-----|---------|
| GPIO18 | 12  | BCK     |
| GPIO19 | 35  | LCK     |
| GPIO21 | 40  | DIN     |
| 3V3    | 1   | VIN     |
| GND    | 6   | GND     |

On GY-PCM5102 boards, connect SCK to GND and set the back jumpers:
1L 2L 3H 4L.

Optional buttons to GND: GPIO17 (pin 11) for next filter, and GPIO27
(pin 13) to toggle hold/auto.

UART: USB-serial adapter on GPIO14/15, 115200 baud. The current filter
name is printed on every change, and the ACT LED flashes.

## Keys (serial terminal)

```
n / p      next / previous filter       h        hold / auto-advance
r / R      resonance -/+                c / C    cutoff -/+ 3 st
e / E      filter env amount -/+        d / D    drive -/+
s          cutoff sweep on/off          t / T    tempo -/+ 5 BPM
1-4        mute bass/lead/stab/noise    i        print status
l          pick lane to edit (bass, lead, stab, noise, accent)
z / x      hits -/+     , / .   steps -/+     v / b   rotate -/+

sampler
m          mute / unmute                k        demo break
w          resample 2 bars (from the next downbeat)
g          slices 4 / 8 / 16 / auto     y / Y    auto sensitivity -/+
o          step / random slice order    O        one shot / slice
j / J      pitch -/+ 1 st               q        varispeed / keep time
a / A      sampler hits -/+
```

## Sampler

`src/sampler.h/.cpp`: plain C++, shared by the Pi kernel and the page. One mono sample in RAM (on the Pi
`SAMPLER_FRAMES` floats from the heap, 64 MB = 5.8 min at 48 kHz by default), on its own Euclidean lane. Slice
mode cuts the START..END region into 4/8/16 slices or at its onsets (auto: energy-rise detection, up to 16), and each
hit plays the slice under its step (or a random one), choking the previous one; one shot plays the region. Pitch
+-24 st + fine cents (Hermite), varispeed or keep time (two 40 ms grains), attack, decay (0 = hold),
reverse, 4 voices. Resample records the mix from the next downbeat for 1/2/4 bars. A synthesised one-bar break is
loaded at boot. Loading files from the SD card is not in the kernel yet (the page loads any audio file).

The DSP load (% of one core), clock and SoC temperature are logged every
10 s.

## Build without a compiler (GitHub Actions)

1. Push this folder to a GitHub repo.
2. Go to Actions -> build. It runs on every push, or start it with
   "Run workflow".
3. Download the `filter-lab-sdcard` artifact and copy it to a FAT32 SD card.

## Build locally

```sh
git clone https://github.com/rsta2/circle.git
mkdir libs && cd libs
git clone https://github.com/surge-synthesizer/sst-filters.git
git clone https://github.com/surge-synthesizer/sst-basic-blocks.git
git clone https://github.com/simd-everywhere/simde.git
cd ..
cp circle-config/Config.mk circle/
(cd circle && ./makeall && cd boot && make firmware && make armstub64)
make -C src
```

Toolchain: Arm GNU `aarch64-none-elf`.

`filterlab.cpp` is the only file that includes sst-filters. It's built
hosted, with libstdc++ headers, while the rest of the kernel stays
freestanding Circle. Nothing from libstdc++ is linked: only `libm`, which
Circle links by default (`STDLIB_SUPPORT=1`).

## Host preview

```sh
make -C host          # needs libs/ as above
./host/render tour.wav 2     # 2 bars per filter
```

This renders the same engine to a WAV file on your PC, to check a change
before flashing.

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

The browser version is a different instrument from the Pi kernel: osc440's voice (the 3rd Wave-style
TripleWaveGroup, 16 voices) on a Euclidean lane, with **two sst-filters models per voice**, in series or parallel.
Filter 1 sits in osc440's SVF place and filter 2 in its ladder's, with the same cutoff, resonance, envelope 1 amount
and matrix destinations; each can be any of the 27 models the Pi kernel tours (`synth/sst_filter.cpp`). With them:
the sampler, the shift register, Mutable Instruments Marbles, Tides 2 and Beads, and the SP-303 Vinyl Sim.

Pages (Elektron-style keys): FILTER | SHIFT (Turing Machine-style register), MARBLES | PLAY, OSC, ENV (osc440) |
SAMPLER | TIDES (Tides 2, 4 outputs routed to osc440's oscillators and filters or Beads), MATRIX | BEADS, VINYL.
Tempo is in the header.

The display is 150 x 112, shown at 4x on a 600 x 450 AMOLED. Its footer has two parameter lines: line 1 is what
the four front panel encoders turn on the current page, line 2 the four encoders under the screen (the page maps eight
parameters per page in `PAGES`; the panel under the display mirrors them).

Sources: osc440 is in `third_party/pico-osc440`; built with `OSC440_SST_FILTERS` it uses the sst filters (without
the flag it keeps its own SVF and ladder, so its RP2350 build is unchanged). The Mutable Instruments code comes from
pichenettes/eurorack (MIT), unmodified, via `web/mods/` and `web/tides_view/`. `web/build.sh` (needs Emscripten)
needs `LIBS_DIR` (sst-filters, sst-basic-blocks, simde), `MPK4` (open-mpk4) and `MI` (eurorack with stmlib cloned into
it), compiles the engine, the display and the TIDES view to WebAssembly and writes one self-contained
`web/filter-lab.html`, which runs in an AudioWorklet.

A prebuilt `web/filter-lab.html` is checked in, so the browser version runs without any checkout: open it in a
browser and press Play.
