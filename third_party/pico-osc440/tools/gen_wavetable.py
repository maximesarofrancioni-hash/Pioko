#!/usr/bin/env python3
"""Generates a PPG-style wavetable (src/synth/wavetable_data.h/.cpp).

64 single-cycle waves, each defined by its harmonic amplitudes, all in sine
phase so that crossfading neighbours never cancels. Each wave is rendered at
several band limits (mip levels: 64, 32, 16 ... 1 harmonics), and the
oscillator picks the level whose top harmonic stays below 20 kHz at the
current pitch. Tables get 8 samples per top harmonic, with a 32-sample
minimum. The oscillator reads them with 6-point Lagrange interpolation, which
at 8x oversampling keeps every interpolation image below -70 dB (linear would
reach -35 dB on the brightest waves). Each wave carries guard samples, 2 before
and 3 after, so the 6 points never wrap.

The waves (an original set, not copied from any instrument):
   0-15  sine -> saw          harmonics fade in, 1/k
  16-31  saw -> square        even harmonics fade out
  32-47  square -> pulse      width 50% -> 6%
  48-63  formant sweep        a resonant bump moving up the harmonics

    python3 tools/gen_wavetable.py                  # our waves -> wavetable_data.*

It can also convert one bank of Plaits' wavetables (Emilie Gillet, MIT
licence) into the same band-limited format, keeping each wave's own harmonic
phases. Plaits stores 192 integrated 128-sample waves (3 banks of 8 x 8);
differentiating one cycle gives back the wave:

    python3 tools/gen_wavetable.py --plaits path/to/plaits/resources.cc --bank 0
                                                    # -> wavetable_plaits.*
"""

import argparse
import math
import os
import re

import numpy as np

WAVES = 64
MAX_HARMONICS = 64
SAMPLES_PER_TOP_HARMONIC = 8
MIN_BITS = 5
PEAK = 0.98

OUT_DIR = os.path.join(os.path.dirname(__file__), "..", "src", "synth")


def harmonics_of(wave):
    """Amplitudes of harmonics 1..MAX_HARMONICS for one wave."""
    k = np.arange(1, MAX_HARMONICS + 1, dtype=float)
    t = (wave % 16) / 15.0          # position within its group, 0..1
    group = wave // 16
    saw = 1.0 / k
    if group == 0:                  # sine -> saw: harmonic k fades in as t passes k/count
        count = 1.0 + t * (MAX_HARMONICS - 1)
        return saw * np.clip(count - k + 1.0, 0.0, 1.0)
    if group == 1:                  # saw -> square
        even = (k % 2 == 0)
        return np.where(even, saw * (1.0 - t), saw)
    if group == 2:                  # square -> narrow pulse (pulse spectrum: sin(pi k d) / k)
        width = 0.5 - t * 0.44
        return np.abs(np.sin(np.pi * k * width)) / k * 2.0
    centre = 2.0 + t * 38.0         # formant: bump from harmonic 2 to 40 over a soft saw
    return saw * 0.3 + np.exp(-0.5 * ((k - centre) / 2.5) ** 2) * 0.6


def render(amps, phases, harmonics, bits):
    n = 1 << bits
    x = 2.0 * np.pi * np.arange(n) / n
    k = np.arange(1, harmonics + 1)
    return (np.sin(np.outer(x, k) + phases[:harmonics]) * amps[:harmonics]).sum(axis=1)


def basic_spectra():
    """Our waves: (amplitudes, phases) per wave, all in sine phase."""
    zero = np.zeros(MAX_HARMONICS)
    return [(harmonics_of(w), zero) for w in range(WAVES)]


def plaits_spectra(resources_cc, bank):
    """One bank of Plaits' integrated waves as (amplitudes, phases) per wave."""
    with open(resources_cc) as f:
        text = f.read()
    start = text.index("const int16_t wav_integrated_waves[] = {")
    body = text[start + 40:text.index("};", start)]
    values = np.array([int(v) for v in re.findall(r"-?\d+", body)], dtype=float)
    tables = values.reshape(-1, 132)            # 128 samples + 4 wrap-around guards
    spectra = []
    for w in range(bank * WAVES, (bank + 1) * WAVES):
        wave = np.diff(tables[w][:129])          # one cycle of the derivative
        c = np.fft.rfft(wave) / len(wave) * 2.0  # c_k for k = 0..64
        k = np.arange(1, MAX_HARMONICS + 1)
        # a sin(kx + phi) = Re(c e^{ikx})  ->  a = |c|, phi = arg(c) + pi/2
        spectra.append((np.abs(c[k]), np.angle(c[k]) + np.pi / 2))
    return spectra


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--plaits", help="Plaits resources.cc: convert one of its banks instead")
    ap.add_argument("--bank", type=int, default=0, help="Plaits bank, 0..2")
    args = ap.parse_args()
    if args.plaits:
        spectra = plaits_spectra(args.plaits, args.bank)
        stem, symbol = "wavetable_plaits", "kPlaitsWavetable"
        note = (f"// Converted from Plaits' wavetable bank {args.bank} (Copyright 2016 Emilie Gillet,\n"
                "// MIT licence: see THIRD_PARTY.md) to band-limited mip levels.\n")
    else:
        spectra = basic_spectra()
        stem, symbol, note = "wavetable_data", "kBasicWavetable", ""

    levels = []
    h = MAX_HARMONICS
    while h >= 1:
        bits = max(MIN_BITS, math.ceil(math.log2(h * SAMPLES_PER_TOP_HARMONIC)))
        levels.append((h, bits))
        h //= 2

    # Each wave gets one scale for all its levels, so its loudness does not jump
    # with pitch. It is set by the loudest level's true peak, measured on a 4096-
    # sample rendering: the peak can fall between table samples, and the
    # interpolator reconstructs it
    data = [[None] * WAVES for _ in levels]
    for w in range(WAVES):
        amps, phases = spectra[w]
        rendered = [render(amps, phases, h, bits) for h, bits in levels]
        true_peak = max(np.abs(render(amps, phases, h, 12)).max() for h, _ in levels)
        scale = PEAK * 32767 / true_peak
        for li, r in enumerate(rendered):
            q = np.round(r * scale)
            assert np.abs(q).max() <= 32767
            # guards: the last 2 samples before, the first 3 after
            data[li][w] = np.concatenate([q[-2:], q, q[:3]]).astype(np.int16)

    total = sum(a.size * 2 for lv in data for a in lv)

    with open(os.path.join(OUT_DIR, stem + ".h"), "w") as f:
        f.write(
            "// Generated by tools/gen_wavetable.py - do not edit\n"
            + note +
            "#pragma once\n\n"
            '#include "synth/wavetable.h"\n\n'
            "namespace synth {\n\n"
            f"extern const Wavetable {symbol};\n\n"
            "}  // namespace synth\n"
        )

    with open(os.path.join(OUT_DIR, stem + ".cpp"), "w") as f:
        f.write(
            "// Generated by tools/gen_wavetable.py - do not edit\n"
            + note +
            f"// {WAVES} waves x {len(levels)} mip levels, {total} bytes\n"
            f'#include "synth/{stem}.h"\n\n'
            '#include "config/platform.h"\n\n'
            "namespace synth {\n"
            "namespace {\n\n"
        )
        for li, (h, bits) in enumerate(levels):
            f.write(f"// level {li}: {h} harmonics, {1 << bits} samples per wave (+5 guards)\n")
            f.write(f'const int16_t AUDIO_RAM_DATA("wavetable") kLevel{li}[] = {{\n')
            for w in range(WAVES):
                arr = data[li][w]
                for i in range(0, arr.size, 16):
                    f.write("  " + ", ".join(str(int(v)) for v in arr[i:i + 16]) + ",\n")
            f.write("};\n\n")
        f.write("}  // namespace\n\n")
        f.write(f'const Wavetable AUDIO_RAM_DATA("wavetable") {symbol} = {{\n')
        f.write(f"  {WAVES}, {len(levels)},\n  {{\n")
        for li, (h, bits) in enumerate(levels):
            f.write(f"    {{kLevel{li} + 2, {bits}, {h}}},   // + 2: past the leading guards\n")
        f.write("  },\n};\n\n}  // namespace synth\n")

    print(f"{stem}: {WAVES} waves, levels {[(h, 1 << b) for h, b in levels]}, {total} bytes")


if __name__ == "__main__":
    main()
