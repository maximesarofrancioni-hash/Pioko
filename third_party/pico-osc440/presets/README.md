# Presets

One file per sound (one `TripleWaveGroup` timbre), JSON, every parameter by
name. Written and read by `src/preset/preset_json.cpp` (host side: demos,
tests, later the editor or the master's patch storage). The voice firmware never
sees JSON, only `set_param(id, value)`; a preset is the list of those.

| folder | from |
|---|---|
| `acid_line/`, `wavetable_drift/`, `sync_fm_funk/`, `supersaw_ring_house/` | `demo/showcase.cpp` |
| `station/` | `demo/station_demo.cpp` |
| `raga_bhairav_alap/`, `raga_yaman_gat_teentaal/`, `raga_bhupali_keherwa_fusion/` | `demo/indian.cpp` |
| `night_drive/`, `bedroom_pop/`, `big_chorus/` | `demo/indiepop.cpp` |
| `firmware/demo_patch.json` | `src/synth/demo_patch.h` (what `osc440.uf2` plays) |

To hear an edited preset, render a demo with it:

```
./showcase out/ --presets presets/
./station_demo out/ --presets presets/
./indian out/ --presets presets/
./indiepop out/ --presets presets/
```

Only the sound is in a preset. The notes, parameter locks, automation and the
mix (levels between tracks, delay/reverb sends, ducking) belong to the song and
stay in the demo code.

## Format

Exported files are complete. Hand-written ones can be partial: anything left out
keeps its default, and matrix slots not listed are empty. Unknown keys and names
are errors, with the place named (`lfos[0].shape: unknown name "wobble"`).

```
format       "osc440-preset"
version      1
name, description
global       level (0..1, before velocity), wave_scan ("smooth" | "stepped"),
             filter_routing ("series": SVF then ladder | "parallel"),
             key_track (0..1, both filters), pan (-1..1)
svf          cutoff (MIDI note), resonance (0..1), morph (0 low pass, 0.5 notch,
             1 high pass), bandpass (bool), env_amount (semitones at full env 1)
ladder       cutoff (MIDI note), resonance (0..1, self-oscillates near 1),
             drive (>= 1), compensation (0..1), env_amount (semitones),
             oversample (bool: 2x)
controls     mod_wheel, pressure, expression (0..1, matrix sources)
oscillators  [3] mode ("wavetable" "sine" "triangle" "saw" "pulse" "supersaw"
             "noise" "colored_noise"), table ("basic" | "plaits_a"),
             tune (semitones), fine (cents), level, wave (0..63),
             wave_env_amount (waves at full wave envelope), pw (0..1),
             fm (depth from the neighbour: 1 <- 3, 2 <- 1, 3 <- 2),
             sync (bool, oscillators 2 and 3: reset by the previous one)
wave_envelopes [3] stages [6] {level, time (s)} (stage 6 is the release),
             loop_start, loop_end (stage indices; start > end: no loop)
envelopes    [4] mode ("exponential" | "ppg"), delay, attack, decay,
             release (s), sustain (0..1). 1: filters, 2: VCA, 3-4: free
lfos         [4] rate (Hz), shape ("sine" "triangle" "saw" "saw_down" "square"
             "pulse" "sample_hold" "smooth_random" "exp_ramp" "stepped"),
             reset (bool: restart on each note), delay (s, fade-in)
matrix       [up to 16] {slot (0..15, optional), source, destination, amount}
  sources       none lfo1-4 env1-4 wave_env1-3 velocity key mod_wheel pressure
                expression osc1-3 noise (osc and noise: audio rate)
  destinations  pitch1-3 (semitones) wave1-3 pw1-3 fm1-3 level1-3 amp
                svf_cutoff svf_resonance ladder_cutoff ladder_resonance drive
                pan lfo1_rate-lfo4_rate (octaves)
                (fm, level and amp take the audio-rate sources per sample)
```

Numbers are written in the shortest form that reads back as the same float,
so save, load and save again gives the same file and the same sound, bit for
bit (`test/preset_tests.cpp`).
