#!/usr/bin/env python3
"""Checks the host renders: fundamental frequency, peak level, and aliasing.

Aliasing is measured as the strongest spectral peak that is not a harmonic of
the expected fundamental, relative to the fundamental.

    python3 test/check_wav.py <dir with note_*.wav>
"""

import glob
import os
import re
import sys
import wave

import numpy as np

SAMPLE_RATE = 48000
ALIAS_LIMIT_DB = -70.0
# Families that do not reach -70 dB everywhere, with what they do reach: limits
# on the whole band, and below 16 kHz. The triangle's corners (polyBLAMP) and the
# sync resets (polyBLEP) are 2-sample corrections: fine low, weak at high pitch
# (sync at A4: about -56 dB, at A6: about -45 dB)
LIMITS = [  # (name prefix, whole band, below 16 kHz)
    ("va_pulse10", -60.0, -60.0),
    ("va_tri", -40.0, -55.0),
    ("sync_", -40.0, -40.0),
]


def read_wav(path):
    with wave.open(path, "rb") as w:
        assert w.getframerate() == SAMPLE_RATE
        raw = w.readframes(w.getnframes())
    return np.frombuffer(raw, dtype=np.float32)


def read_float_wav(path):
    # The wave module rejects IEEE-float WAVs: skip the 44-byte header ourselves
    with open(path, "rb") as f:
        data = f.read()
    return np.frombuffer(data[44:], dtype=np.float32)


def analyse(path):
    name = os.path.basename(path)[:-4]
    note = int(re.search(r"_n(\d+)", name).group(1))
    f0 = 440.0 * 2 ** ((note - 69) / 12)
    x = read_float_wav(path)[SAMPLE_RATE // 10:]            # skip the gate ramp
    x = x[: (len(x) // SAMPLE_RATE) * SAMPLE_RATE]          # whole seconds: 1 Hz bins
    # 4-term Blackman-Harris: sidelobes near -92 dB, so leakage from a harmonic
    # that falls between bins does not read as aliasing
    n = np.arange(len(x)) / len(x)
    window = (0.35875 - 0.48829 * np.cos(2 * np.pi * n) + 0.14128 * np.cos(4 * np.pi * n)
              - 0.01168 * np.cos(6 * np.pi * n))
    spec = np.abs(np.fft.rfft(x * window))
    freqs = np.fft.rfftfreq(len(x), 1 / SAMPLE_RATE)
    db = 20 * np.log10(spec / spec.max() + 1e-20)

    # The strongest component must be a harmonic of the expected pitch (for the
    # formant waves it is deliberately not the fundamental)
    measured = freqs[np.argmax(spec)]
    harmonic_number = max(1, round(measured / f0))
    harmonic = np.abs(freqs / f0 - np.round(freqs / f0)) * f0 < 10.0   # main lobe
    alias_db = db[~harmonic & (freqs > 20)].max()
    audible_alias_db = db[~harmonic & (freqs > 20) & (freqs < 16000)].max()

    limit, audible_limit = ALIAS_LIMIT_DB, ALIAS_LIMIT_DB
    for prefix, whole, audible in LIMITS:
        if name.startswith(prefix):
            limit, audible_limit = whole, audible
    ok = (abs(measured - harmonic_number * f0) < 1.0 and alias_db < limit
          and audible_alias_db < audible_limit)
    print(f"{name:16s}: f0 {f0:8.2f} Hz, peak = harmonic {harmonic_number:2d} at {measured:8.2f} Hz, "
          f"level {x.max():.3f}, worst non-harmonic {alias_db:6.1f} dB "
          f"(below 16 kHz: {audible_alias_db:6.1f} dB)  {'OK' if ok else 'FAIL'}")
    return ok


def main():
    paths = sorted(glob.glob(os.path.join(sys.argv[1], "note_*.wav")) +
                   glob.glob(os.path.join(sys.argv[1], "wt_*.wav")) +
                   glob.glob(os.path.join(sys.argv[1], "tw_*.wav")) +
                   glob.glob(os.path.join(sys.argv[1], "va_*.wav")) +
                   glob.glob(os.path.join(sys.argv[1], "pt_*.wav")) +
                   glob.glob(os.path.join(sys.argv[1], "sync_*.wav")))
    results = [analyse(p) for p in paths]
    print(f"spectra: {sum(results)}/{len(results)} passed")
    sys.exit(0 if results and all(results) else 1)


if __name__ == "__main__":
    main()
