// Product-page style showcase tracks: four short original pieces, each played
// by four engines (one timbre per voice module, as in the planned synth), with
// per-step parameter locks and automation sent through the same set_param path
// the master will use.
//
// Every sound comes from TripleWaveGroup, drums included. The sidechain ducking,
// delay, reverb and final limiter stand in for the master's effects, which are
// not written yet.
//
//   showcase <out_dir> [--presets <dir>]
//
// Also writes every track's sound to <out_dir>/presets/<song>/<track>.json.
// With --presets, sounds found there replace the ones written in this file, so
// an edited preset can be heard.

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "config/audio_config.h"
#include "demo_common.h"
#include "song.h"
#include "engine/voice_engine.h"
#include "preset/preset_json.h"
#include "synth/demo_patch.h"
#include "synth/lfo.h"
#include "synth/mod_matrix.h"
#include "synth/oscillator.h"
#include "synth/triple_wave_group.h"

namespace {

using namespace demo;

// ----------------------------------------------------------- shared sounds ---

void kick_patch(Track& t, float punch = 30.0f) {
  t.osc(0, M::kSine, 1.0f);
  t.osc(2, M::kNoise, 0.0f);
  t.set(G::kParamKeyTrack, 0.0f);
  // Pitch drop and click from envelope 3
  t.env(2, 0.0005f, 0.045f, 0.0f, 0.05f);
  t.slot(0, mod::kSrcEnv3, mod::kDstPitch1, punch);
  t.slot(1, mod::kSrcEnv3, mod::kDstLevel3, 0.25f);
  t.env(1, 0.0005f, 0.38f, 0.0f, 0.1f);
  t.set(G::kParamLadderCutoff, 105.0f);
  t.set(G::kParamDrive, 2.5f);
  t.set(G::kParamCompensation, 1.0f);
  t.set(G::kParamLevel, 0.55f);
}

void hat_patch(Track& t) {
  t.osc(0, M::kNoise, 0.8f);
  t.set(G::kParamSvfCutoff, 106.0f);   // high pass, ~3.7 kHz
  t.set(G::kParamSvfMorph, 1.0f);
  t.set(G::kParamSvfResonance, 0.25f);
  t.set(G::kParamKeyTrack, 0.0f);
  t.env(1, 0.0005f, 0.035f, 0.0f, 0.03f);
  t.set(G::kParamLevel, 0.26f);
}

// Locks that turn the hat into an open hat
Locks open_hat() { return {{G::env_param(1, G::kEnvDecay), 0.25f}, {G::env_param(1, G::kEnvRelease), 0.2f}}; }

// ---------------------------------------------------- 1. Acid line (128) ---

Song acid() {
  Song s("01_acid_line.wav", 128.0, 16);
  s.delay_beats = 0.75;
  s.kick = 0;
  kick_patch(s.add("kick"));
  Track& hats = s.add("hats");
  hat_patch(hats);
  hats.set(G::kParamPan, 0.25f);

  Track& bass = s.add("acid");
  bass.osc(0, M::kSaw, 0.8f);
  bass.set(G::kParamLadderCutoff, 42.0f);
  bass.set(G::kParamLadderResonance, 0.8f);
  bass.set(G::kParamLadderEnvAmount, 42.0f);
  bass.set(G::kParamDrive, 2.2f);
  bass.set(G::kParamCompensation, 0.3f);
  bass.env(0, 0.001f, 0.18f, 0.0f, 0.1f);
  bass.env(1, 0.002f, 0.3f, 0.6f, 0.05f);
  bass.slot(0, mod::kSrcVelocity, mod::kDstCutoff2, 16.0f);   // accents open the filter
  bass.set(G::kParamLevel, 0.3f);
  bass.duck = 0.3f;
  bass.delay = 0.08f;
  bass.automate = [](Track& t, double beat) {
    // The cutoff knob, ridden up over 12 bars with a slow wobble
    const double rise = std::min(1.0, beat / 48.0);
    t.engine.set_param(G::kParamLadderCutoff, static_cast<float>(38.0 + 30.0 * rise + 6.0 * std::sin(beat * 0.4)));
  };

  Track& stab = s.add("stab");
  stab.osc(0, M::kWavetable, 0.5f);
  stab.set(G::osc_param(0, G::kOscTable), 1.0f);   // Plaits' bank
  stab.osc(1, M::kWavetable, 0.3f, 12.0f);
  stab.set(G::osc_param(1, G::kOscWave), 40.0f);
  stab.set(G::kParamLadderCutoff, 78.0f);
  stab.set(G::kParamLadderEnvAmount, 26.0f);
  stab.env(0, 0.001f, 0.22f, 0.0f, 0.15f);
  stab.env(1, 0.001f, 0.32f, 0.0f, 0.2f);
  stab.set(G::kParamLevel, 0.22f);
  stab.set(G::kParamPan, -0.2f);
  stab.reverb = 0.3f;
  stab.delay = 0.35f;
  stab.duck = 0.5f;

  struct Step { int step, interval; bool accent; double len; };
  const Step line[] = {{0, 0, true, .2},  {2, 12, false, .2}, {3, 0, false, .2}, {5, 3, false, .2},
                       {6, 0, true, .45}, {8, 0, false, .2},  {9, 15, false, .2}, {10, 7, false, .2},
                       {11, 0, true, .2}, {12, 12, false, .2}, {13, 10, true, .2}, {14, 0, false, .2},
                       {15, 3, false, .2}};
  const int transpose[] = {0, 0, -4, -2};   // A, A, F, G
  const int chord[] = {n("A3"), n("C4"), n("E4"), n("G4"), n("B4")};
  const float waves[] = {8.0f, 22.0f, 36.0f, 50.0f};
  int stab_count = 0;

  for (int bar = 0; bar < 16; ++bar) {
    const double b = 4.0 * bar;
    const int tr = transpose[bar % 4];
    for (int q = 0; q < 4; ++q) {
      if (bar == 15 && q >= 2) break;   // the break before the end
      s.trig(0, b + q, 31, 0.25, 1.0f);
    }
    if (bar >= 4) {
      for (int st = 0; st < 16; ++st) {
        const double at = b + st * 0.25;
        if (st % 4 == 2) {
          if (bar >= 8) s.trig(1, at, 60, 0.25, 0.8f, open_hat());
          else s.trig(1, at, 60, 0.2, 0.8f);
        } else if (bar >= 6 && st % 2 == 1) {
          s.trig(1, at, 60, 0.15, 0.35f + 0.15f * rnd());
        }
      }
    }
    for (const Step& st : line) {
      s.trig(2, b + st.step * 0.25, 33 + tr + st.interval, st.len, st.accent ? 1.0f : 0.6f);
    }
    if (bar >= 8) {
      for (int st : {2, 10}) {
        // Each stab locks a different wave position, Elektron style
        const float w = waves[stab_count++ % 4];
        for (int c : chord) s.trig(3, b + st * 0.25, c + tr, 0.4, 0.8f, {{G::osc_param(0, G::kOscWave), w}});
      }
    }
  }
  s.trig(0, 64.0, 31, 0.25, 1.0f);
  s.trig(2, 64.0, 33, 0.5, 1.0f);
  for (int c : chord) s.trig(3, 64.0, c, 1.0, 0.9f);
  return s;
}

// ------------------------------------------------ 2. Wavetable drift (80) ---

Song drift() {
  Song s("02_wavetable_drift.wav", 80.0, 12);
  s.tail = 7.0;
  s.delay_beats = 0.75;

  Track& pad = s.add("pad");
  pad.osc(0, M::kWavetable, 0.35f);
  pad.set(G::osc_param(0, G::kOscWave), 6.0f);
  pad.set(G::osc_param(0, G::kOscWaveEnvAmount), 34.0f);
  pad.osc(1, M::kWavetable, 0.22f, 7.0f);
  pad.set(G::osc_param(1, G::kOscTable), 1.0f);
  pad.set(G::osc_param(1, G::kOscWave), 18.0f);
  pad.set(G::osc_param(1, G::kOscWaveEnvAmount), 20.0f);
  pad.osc(2, M::kTriangle, 0.15f, -12.0f);
  for (int o = 0; o < 2; ++o) {
    // Wave envelopes that loop: up over 3 s, back over 4 s (osc 2 a little slower)
    pad.set(G::wave_env_param(o, 0, false), 1.0f);
    pad.set(G::wave_env_param(o, 0, true), 3.0f + o);
    pad.set(G::wave_env_param(o, 1, false), 0.0f);
    pad.set(G::wave_env_param(o, 1, true), 4.0f + o);
    pad.set(G::wave_env_loop_param(o, false), 0.0f);
    pad.set(G::wave_env_loop_param(o, true), 1.0f);
  }
  pad.set(G::kParamSvfCutoff, 46.0f);
  pad.set(G::kParamSvfMorph, 1.0f);
  pad.set(G::kParamLadderResonance, 0.2f);
  pad.env(1, 2.5f, 2.0f, 0.8f, 4.0f);
  pad.lfo(0, 0.09f, Shape::kSine);
  pad.slot(0, mod::kSrcLfo1, mod::kDstPan, 0.5f);
  pad.set(G::kParamLevel, 0.15f);
  pad.reverb = 0.45f;
  pad.automate = [](Track& t, double beat) {
    t.engine.set_param(G::kParamLadderCutoff, static_cast<float>(70.0 + 24.0 * std::sin(3.14159 * beat / 48.0)));
  };

  Track& bleep = s.add("bleeps");
  bleep.osc(0, M::kWavetable, 0.7f);
  bleep.set(G::osc_param(0, G::kOscTable), 1.0f);
  bleep.set(G::kParamWaveScan, 1.0f);   // stepped: PPG-style jumps
  bleep.lfo(1, 5.33f, Shape::kSampleHold);
  bleep.lfo(2, 2.67f, Shape::kSampleHold);
  bleep.slot(0, mod::kSrcLfo2, mod::kDstWave1, 28.0f);
  bleep.set(G::osc_param(0, G::kOscWave), 30.0f);
  bleep.slot(1, mod::kSrcLfo3, mod::kDstCutoff2, 14.0f);
  bleep.set(G::kParamLadderCutoff, 92.0f);
  bleep.set(G::kParamLadderResonance, 0.45f);
  bleep.set(G::kParamLadderEnvAmount, 18.0f);
  bleep.env(0, 0.001f, 0.12f, 0.0f, 0.1f);
  bleep.env(1, 0.001f, 0.16f, 0.0f, 0.12f);
  bleep.set(G::kParamLevel, 0.26f);
  bleep.delay = 0.45f;
  bleep.reverb = 0.35f;

  Track& sub = s.add("sub");
  sub.osc(0, M::kTriangle, 0.8f);
  sub.osc(1, M::kSine, 0.5f, -12.0f);
  sub.set(G::kParamLadderCutoff, 66.0f);
  sub.env(1, 0.08f, 1.5f, 0.7f, 1.2f);
  sub.set(G::kParamLevel, 0.11f);

  Track& bell = s.add("glass");
  bell.osc(0, M::kSine, 0.0f, 5.825f);   // modulator at 1.4x: glassy, inharmonic
  bell.osc(1, M::kSine, 0.7f);
  bell.set(G::osc_param(1, G::kOscFm), 0.6f);
  bell.set(G::kParamLadderCutoff, 100.0f);
  bell.env(1, 0.002f, 3.0f, 0.0f, 2.0f);
  bell.set(G::kParamLevel, 0.17f);
  bell.set(G::kParamPan, 0.35f);
  bell.delay = 0.3f;
  bell.reverb = 0.55f;

  struct Chord { std::vector<const char*> tones; const char* root; };
  const Chord chords[] = {{{"D3", "F#3", "A3", "C#4", "E4"}, "D2"}, {{"B2", "D3", "A3", "C#4", "E4"}, "B1"},
                          {{"G2", "B2", "F#3", "A3", "C#4"}, "G1"}, {{"A2", "D3", "E3", "F#3", "B3"}, "A1"},
                          {{"G2", "B2", "D3", "F#3", "A3"}, "G1"}, {{"D3", "A3", "C#4", "E4", "F#4"}, "D2"}};
  const char* scale[] = {"D5", "E5", "F#5", "A5", "B5", "D6", "E6", "F#6"};
  for (int c = 0; c < 6; ++c) {
    const double b = 8.0 * c;
    for (const char* t : chords[c].tones) s.trig(0, b, n(t), 7.9, 0.7f);
    if (c >= 1) s.trig(2, b, n(chords[c].root), 7.5, 0.8f);
    if (c >= 2) {
      for (double at : {0.0, 2.5, 5.0}) {
        if (rnd() < 0.6f) s.trig(3, b + at, n(chords[c].tones[1 + static_cast<int>(rnd() * 4)]) + 24, 0.5, 0.6f);
      }
    }
  }
  // Bleeps: a random pentatonic sixteenth line, each note locked to its own pan
  for (int st = 16; st < 4 * 44; ++st) {
    const float density = st < 64 ? 0.22f : 0.38f;
    if (rnd() < density) {
      const int note = n(scale[static_cast<int>(rnd() * 8)]);
      s.trig(1, st * 0.25, note, 0.2, 0.5f + 0.4f * rnd(), {{G::kParamPan, -0.7f + 1.4f * rnd()}});
    }
  }
  return s;
}

// ------------------------------------------------- 3. Sync & FM funk (112) ---

Song funk() {
  Song s("03_sync_fm_funk.wav", 112.0, 16);
  s.delay_beats = 0.75;
  s.kick = 0;
  kick_patch(s.add("kick"), 26.0f);

  // One track for snare and hats: the hats are parameter locks on the snare
  Track& perc = s.add("snare+hats");
  perc.osc(0, M::kSine, 0.55f);
  perc.osc(1, M::kNoise, 0.5f);
  perc.set(G::kParamKeyTrack, 0.0f);
  perc.env(2, 0.0005f, 0.03f, 0.0f, 0.03f);
  perc.slot(0, mod::kSrcEnv3, mod::kDstPitch1, 12.0f);
  perc.set(G::kParamSvfCutoff, 58.0f);
  perc.set(G::kParamSvfMorph, 1.0f);
  perc.set(G::kParamLadderCutoff, 118.0f);
  perc.env(1, 0.0005f, 0.17f, 0.0f, 0.12f);
  perc.set(G::kParamLevel, 0.5f);
  perc.reverb = 0.22f;
  const Locks hat = {
      {G::osc_param(0, G::kOscLevel), 0.0f}, {G::osc_param(1, G::kOscLevel), 0.5f},
      {G::kParamSvfCutoff, 110.0f}, {G::env_param(1, G::kEnvDecay), 0.035f},
      {G::env_param(1, G::kEnvRelease), 0.03f}};

  Track& bass = s.add("fm bass");
  bass.osc(0, M::kSine, 0.0f, 12.0f);   // modulator, an octave up
  bass.osc(1, M::kSine, 0.7f);
  bass.set(G::osc_param(1, G::kOscFm), 1.0f);
  bass.osc(2, M::kSine, 0.35f, -12.0f);
  bass.set(G::kParamLadderCutoff, 74.0f);
  bass.set(G::kParamLadderEnvAmount, 22.0f);
  bass.env(0, 0.001f, 0.15f, 0.0f, 0.1f);
  bass.env(1, 0.001f, 0.25f, 0.4f, 0.06f);
  bass.set(G::kParamLevel, 0.3f);
  bass.duck = 0.25f;

  Track& lead = s.add("sync lead");
  lead.osc(0, M::kSaw, 0.0f);             // the master: silent, sets the pitch
  lead.osc(1, M::kSaw, 0.7f);             // synced, its pitch swept
  lead.set(G::osc_param(1, G::kOscSync), 1.0f);
  lead.osc(2, M::kPulse, 0.18f, -12.0f);
  lead.env(2, 0.001f, 0.35f, 0.0f, 0.2f);
  lead.slot(0, mod::kSrcEnv3, mod::kDstPitch2, 24.0f);   // the sync sweep
  lead.lfo(0, 5.5f, Shape::kSine);
  lead.set(G::lfo_param(0, G::kLfoDelay), 0.35f);
  lead.slot(1, mod::kSrcLfo1, mod::kDstPitch1, 0.25f);   // delayed vibrato, both
  lead.slot(2, mod::kSrcLfo1, mod::kDstPitch2, 0.25f);
  lead.slot(3, mod::kSrcLfo2, mod::kDstPitch2, 5.0f);    // slow drift of the sync timbre
  lead.lfo(1, 0.3f, Shape::kTriangle);
  lead.set(G::kParamLadderCutoff, 96.0f);
  lead.set(G::kParamLadderResonance, 0.25f);
  lead.env(1, 0.004f, 0.6f, 0.7f, 0.22f);
  lead.set(G::kParamLevel, 0.11f);
  lead.set(G::kParamPan, -0.1f);
  lead.delay = 0.28f;
  lead.reverb = 0.25f;

  const int roots[] = {n("E2"), n("E2"), n("C2"), n("D2")};
  struct B { int step, interval; double len; };
  const B line[] = {{0, 0, .45}, {3, 12, .2}, {4, 0, .2}, {6, 10, .2}, {8, 0, .45}, {11, 7, .2}, {12, 12, .2}, {14, 10, .4}};
  const float fm_locks[] = {0.5f, 1.6f, 0.8f, 2.2f, 0.6f, 1.2f, 2.6f, 0.9f};
  struct L { const char* note; double at, len; };
  const L melody[] = {{"B4", 0, .7},   {"D5", .75, .7}, {"E5", 1.5, 1},  {"G5", 2.5, .5}, {"F#5", 3, 1},
                      {"E5", 4, .5},   {"D5", 4.5, .5}, {"B4", 5, 1.4},  {"A4", 6.5, .5}, {"B4", 7, 1},
                      {"G5", 8, .7},   {"F#5", 8.75, .7}, {"E5", 9.5, .5}, {"D5", 10, 1}, {"E5", 11, 1},
                      {"F#5", 12, .5}, {"A5", 12.5, .5}, {"B5", 13, 2},  {"A5", 15, .5}, {"F#5", 15.5, .5}};

  for (int bar = 0; bar < 16; ++bar) {
    const double b = 4.0 * bar;
    for (int st : (bar % 2 ? std::vector<int>{0, 10, 13} : std::vector<int>{0, 7, 10})) s.trig(0, b + st * 0.25, 33, 0.25, 1.0f);
    for (int st = 0; st < 16; ++st) {
      if (st == 4 || st == 12) s.trig(1, b + st * 0.25, 54, 0.3, 1.0f);
      else if (bar >= 2 && st % 2 == 0) s.trig(1, b + st * 0.25, 54, 0.15, st % 4 == 2 ? 0.8f : 0.45f, hat);
    }
    int k = 0;
    for (const B& x : line) {
      s.trig(2, b + x.step * 0.25, roots[bar % 4] + x.interval, x.len, 0.9f,
             {{G::osc_param(1, G::kOscFm), fm_locks[(k++ + bar) % 8]}});
    }
  }
  for (int rep = 1; rep < 4; ++rep) {
    const bool wild = rep == 3;   // last pass: a deeper sync sweep, locked per note
    for (const L& x : melody) {
      if (wild) s.trig(3, 16.0 * rep + x.at, n(x.note), x.len, 0.9f, {{G::matrix_param(0, 2), 38.0f}});
      else s.trig(3, 16.0 * rep + x.at, n(x.note), x.len, 0.9f);
    }
  }
  s.trig(0, 64.0, 33, 0.25, 1.0f);
  s.trig(2, 64.0, n("E1"), 1.0, 1.0f);
  s.trig(3, 64.0, n("E5"), 2.0, 0.9f);
  return s;
}

// ---------------------------------------------- 4. Supersaw & ring house (124) ---

Song house() {
  Song s("04_supersaw_ring_house.wav", 124.0, 16);
  s.delay_beats = 0.75;
  s.kick = 0;
  kick_patch(s.add("kick"));

  Track& hats = s.add("hats+clap");
  hat_patch(hats);
  hats.reverb = 0.15f;
  const Locks clap = {
      {G::kParamSvfCutoff, 80.0f}, {G::env_param(1, G::kEnvDecay), 0.14f},
      {G::env_param(1, G::kEnvRelease), 0.12f}, {G::kParamLevel, 0.5f}};

  Track& bass = s.add("bass");
  bass.osc(0, M::kPulse, 0.7f);
  bass.lfo(0, 0.4f, Shape::kTriangle);
  bass.slot(0, mod::kSrcLfo1, mod::kDstPw1, 0.25f);   // PWM
  bass.osc(1, M::kSine, 0.4f, -12.0f);
  bass.set(G::kParamLadderCutoff, 56.0f);
  bass.set(G::kParamLadderResonance, 0.3f);
  bass.set(G::kParamLadderEnvAmount, 24.0f);
  bass.env(0, 0.001f, 0.15f, 0.0f, 0.1f);
  bass.env(1, 0.002f, 0.2f, 0.5f, 0.06f);
  bass.set(G::kParamLevel, 0.28f);
  bass.duck = 0.35f;

  Track& chords = s.add("chords");
  chords.osc(0, M::kSine, 0.0f, 19.02f);   // ring modulator, x3
  chords.osc(1, M::kWavetable, 0.0f);
  chords.set(G::osc_param(1, G::kOscTable), 1.0f);
  chords.set(G::osc_param(1, G::kOscWave), 8.0f);
  chords.slot(0, mod::kSrcOsc1, mod::kDstLevel2, 0.45f);   // ring modulation, at audio rate
  chords.osc(2, M::kSupersaw, 0.45f);
  chords.set(G::osc_param(2, G::kOscPw), 0.55f);
  chords.set(G::kParamLadderResonance, 0.3f);
  chords.set(G::kParamLadderEnvAmount, 14.0f);
  chords.env(0, 0.001f, 0.25f, 0.0f, 0.2f);
  chords.env(1, 0.002f, 0.3f, 0.3f, 0.25f);
  chords.set(G::kParamLevel, 0.21f);
  chords.reverb = 0.3f;
  chords.delay = 0.22f;
  chords.duck = 0.6f;
  chords.automate = [](Track& t, double beat) {
    // The classic filter-house sweep, from bar 4 to bar 13
    const double x = std::clamp((beat - 16.0) / 36.0, 0.0, 1.0);
    t.engine.set_param(G::kParamLadderCutoff, static_cast<float>(54.0 + 50.0 * x * x));
  };

  const std::vector<std::vector<const char*>> prog = {{"F3", "Ab3", "C4", "Eb4", "G4"},
                                                      {"Db3", "F3", "Ab3", "C4"},
                                                      {"Ab3", "C4", "Eb4", "G4"},
                                                      {"Eb3", "G3", "Bb3", "Db4"}};
  const int roots[] = {n("F1"), n("Db2"), n("Ab1"), n("Eb2")};

  for (int bar = 0; bar < 16; ++bar) {
    const double b = 4.0 * bar;
    for (int q = 0; q < 4; ++q) {
      if (bar == 11 && q >= 2) break;
      s.trig(0, b + q, 33, 0.25, 1.0f);
    }
    for (int st = 0; st < 16; ++st) {
      const double at = b + st * 0.25;
      if (bar >= 4 && (st == 4 || st == 12)) s.trig(1, at, 60, 0.3, 1.0f, clap);
      else if (st % 4 == 2) s.trig(1, at, 60, 0.25, 0.85f, bar >= 8 ? open_hat() : Locks{});
      else if (bar >= 2) s.trig(1, at, 60, 0.12, st % 2 ? 0.35f : 0.5f);
    }
    if (bar >= 8) {
      for (int st : {2, 6, 10, 14}) s.trig(2, b + st * 0.25, roots[bar % 4] + (st == 14 ? 12 : 0), 0.22, 0.9f);
    }
    if (bar >= 4) {
      for (int st : {3, 6, 10, 14}) {
        for (const char* t : prog[bar % 4]) s.trig(3, b + st * 0.25, n(t), st == 14 ? 0.6 : 0.35, 0.85f);
      }
    }
  }
  s.trig(0, 64.0, 33, 0.25, 1.0f);
  s.trig(2, 64.0, roots[0], 0.5, 1.0f);
  for (const char* t : prog[0]) s.trig(3, 64.0, n(t), 1.5, 0.9f);
  return s;
}

}  // namespace

int main(int argc, char** argv) {
  const std::string dir = argc > 1 ? argv[1] : ".";
  const std::string load_dir = argc > 3 && std::string(argv[2]) == "--presets" ? argv[3] : "";
  const std::string preset_dir = dir + "/presets";

  Song (*const songs[])() = {acid, drift, funk, house};
  for (auto make : songs) {
    const uint32_t seed = g_rand;
    {
      Song s = make();   // its own instance: exporting renders a block
      if (!load_dir.empty()) import_presets(s, load_dir);
      export_presets(s, preset_dir);
    }
    g_rand = seed;      // the same random choices as without the export
    Song s = make();
    if (!load_dir.empty()) import_presets(s, load_dir);
    render(s, dir);
  }

  // The firmware's own demo patch
  auto g = std::make_unique<synth::TripleWaveGroup>();
  synth::apply_demo_patch([&](uint16_t id, float v) { g->set_param(id, v); });
  std::filesystem::create_directories(preset_dir + "/firmware");
  preset::save(preset_dir + "/firmware/demo_patch.json",
               preset::to_json(*g, "firmware demo", "src/synth/demo_patch.h: what osc440.uf2 plays"));
  return 0;
}
