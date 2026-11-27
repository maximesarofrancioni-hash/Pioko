# filter-lab

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
```

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

Pages (Elektron-style keys): MODEL, EUCLID | SHIFT (Turing Machine-style register), MARBLES | PLAY, OSC, FLTR, ENV
(osc440) | TIDES (Tides 2, 4 outputs routed to osc440 / filter lab / Beads) | BEADS, VINYL. The Mutable
Instruments code comes from pichenettes/eurorack (MIT), unmodified, via `web/mods/`; `build.sh` needs `MI=` pointing
at a eurorack checkout with stmlib cloned into it.

The page has three tabs: Filter lab (this engine), Wavetable (osc440's TripleWaveGroup on a sixth
Euclidean lane, built from your pico-osc440 sources at 44.1 kHz via `web/wave/`) and Vinyl Sim (the decoded
SP-303 program on the master bus). `web/build.sh` needs `LIBS_DIR`, `MPK4` and `OSC440` pointing at the checkouts.

`web/build.sh` (needs Emscripten) compiles the same engine to WebAssembly and writes one self-contained `filter-lab.html`, which runs in an AudioWorklet with the same controls as the serial keys.

A prebuilt `web/filter-lab.html` is checked in, so the browser version runs without the open-mpk4, pico-osc440 and
eurorack checkouts: open it in a browser and press Play.
