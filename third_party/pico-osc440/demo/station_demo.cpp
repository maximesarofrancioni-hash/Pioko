// Station-style demo: original departure melodies and chimes in the manner of
// Tokyo's train stations (not copies of any real station melody), played by
// four instances of the voice-module engine, one timbre each, as in the planned
// synth: bell lead, mallet arpeggio, bass, pad.
//
// The master's effects are not written yet, so this file stands in for them:
// a ping-pong delay on the bell and a small FDN hall reverb, then 24-bit stereo
// WAV files.
//
//   station_demo <out_dir> [--presets <dir>]
//
// Also writes the four sounds to <out_dir>/presets/station/<sound>.json; with
// --presets, sounds found in <dir>/station/ replace the built-in ones.

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

#include "config/audio_config.h"
#include "engine/voice_engine.h"
#include "preset/preset_json.h"
#include "synth/mod_matrix.h"
#include "synth/oscillator.h"
#include "synth/triple_wave_group.h"
#include "demo_common.h"

namespace {

using namespace demo;

using G = synth::TripleWaveGroup;
using M = synth::Oscillator::Mode;
constexpr size_t kN = audio::kBlockSize;

// ---------------------------------------------------------------- modules ---

struct Module {
  G g0, g1;
  engine::VoiceEngine engine{g0, g1};
  float reverb_send = 0.0f, delay_send = 0.0f;
  template <class F> void patch(F&& f) { f([&](uint16_t id, float v) { engine.set_param(id, v); }); }
};

enum { kBell, kMallet, kBass, kPad, kModules };

template <class Set>
void base_patch(Set& set) {
  for (int o = 0; o < 3; ++o) set(G::osc_param(o, G::kOscLevel), 0.0f);
  set(G::kParamLadderResonance, 0.0f);
  set(G::kParamKeyTrack, 0.5f);
  set(G::kParamSvfCutoff, 135.0f);
}

void bell_patch(Module& m) {
  m.patch([](auto set) {
    base_patch(set);
    // 2-operator FM: osc 1 (sine, x3.5, silent) modulates osc 2 (sine)
    set(G::osc_param(0, G::kOscMode), static_cast<float>(M::kSine));
    set(G::osc_param(0, G::kOscTune), 21.686f);
    set(G::osc_param(1, G::kOscMode), static_cast<float>(M::kSine));
    set(G::osc_param(1, G::kOscLevel), 0.7f);
    set(G::osc_param(1, G::kOscFm), 0.8f);
    set(G::osc_param(2, G::kOscMode), static_cast<float>(M::kSine));
    set(G::osc_param(2, G::kOscTune), 12.0f);
    set(G::osc_param(2, G::kOscLevel), 0.15f);
    // Brightness falls faster than the level
    set(G::kParamLadderCutoff, 96.0f);
    set(G::kParamLadderEnvAmount, 30.0f);
    set(G::env_param(0, G::kEnvAttack), 0.001f);
    set(G::env_param(0, G::kEnvDecay), 0.5f);
    set(G::env_param(0, G::kEnvSustain), 0.0f);
    set(G::env_param(0, G::kEnvRelease), 0.6f);
    set(G::env_param(1, G::kEnvAttack), 0.001f);
    set(G::env_param(1, G::kEnvDecay), 1.6f);
    set(G::env_param(1, G::kEnvSustain), 0.0f);
    set(G::env_param(1, G::kEnvRelease), 1.2f);
    // Vibraphone-style tremolo
    set(G::lfo_param(0, G::kLfoRate), 5.5f);
    set(G::matrix_param(0, 0), synth::mod::kSrcLfo1);
    set(G::matrix_param(0, 1), synth::mod::kDstAmp);
    set(G::matrix_param(0, 2), 0.012f);
    set(G::kParamLevel, 0.2f);
    set(G::kParamPan, 0.2f);
  });
  m.reverb_send = 0.35f;
  m.delay_send = 0.22f;
}

void mallet_patch(Module& m) {
  m.patch([](auto set) {
    base_patch(set);
    // Marimba-like: a sine, plus its 4th harmonic that dies in ~80 ms, plus a
    // touch of Plaits' wavetable an octave up
    set(G::osc_param(0, G::kOscMode), static_cast<float>(M::kSine));
    set(G::osc_param(0, G::kOscLevel), 0.8f);
    set(G::osc_param(1, G::kOscMode), static_cast<float>(M::kSine));
    set(G::osc_param(1, G::kOscTune), 24.0f);
    set(G::osc_param(2, G::kOscTable), 1.0f);
    set(G::osc_param(2, G::kOscWave), 12.0f);
    set(G::osc_param(2, G::kOscTune), 12.0f);
    set(G::osc_param(2, G::kOscLevel), 0.08f);
    set(G::env_param(2, G::kEnvAttack), 0.001f);
    set(G::env_param(2, G::kEnvDecay), 0.08f);
    set(G::env_param(2, G::kEnvSustain), 0.0f);
    set(G::env_param(2, G::kEnvRelease), 0.05f);
    set(G::matrix_param(0, 0), synth::mod::kSrcEnv3);
    set(G::matrix_param(0, 1), synth::mod::kDstLevel2);
    set(G::matrix_param(0, 2), 0.5f);
    set(G::kParamLadderCutoff, 88.0f);
    set(G::kParamLadderEnvAmount, 22.0f);
    set(G::env_param(0, G::kEnvAttack), 0.001f);
    set(G::env_param(0, G::kEnvDecay), 0.2f);
    set(G::env_param(0, G::kEnvSustain), 0.0f);
    set(G::env_param(1, G::kEnvAttack), 0.001f);
    set(G::env_param(1, G::kEnvDecay), 0.45f);
    set(G::env_param(1, G::kEnvSustain), 0.0f);
    set(G::env_param(1, G::kEnvRelease), 0.35f);
    set(G::kParamLevel, 0.24f);
    set(G::kParamPan, -0.35f);
  });
  m.reverb_send = 0.25f;
}

void bass_patch(Module& m) {
  m.patch([](auto set) {
    base_patch(set);
    set(G::osc_param(0, G::kOscMode), static_cast<float>(M::kSaw));
    set(G::osc_param(0, G::kOscLevel), 0.5f);
    set(G::osc_param(1, G::kOscMode), static_cast<float>(M::kPulse));
    set(G::osc_param(1, G::kOscPw), 0.3f);
    set(G::osc_param(1, G::kOscFine), -7.0f);
    set(G::osc_param(1, G::kOscLevel), 0.4f);
    set(G::kParamLadderCutoff, 50.0f);
    set(G::kParamLadderResonance, 0.35f);
    set(G::kParamLadderEnvAmount, 30.0f);
    set(G::kParamDrive, 1.5f);
    set(G::env_param(0, G::kEnvAttack), 0.002f);
    set(G::env_param(0, G::kEnvDecay), 0.25f);
    set(G::env_param(0, G::kEnvSustain), 0.2f);
    set(G::env_param(0, G::kEnvRelease), 0.2f);
    set(G::env_param(1, G::kEnvAttack), 0.002f);
    set(G::env_param(1, G::kEnvDecay), 0.5f);
    set(G::env_param(1, G::kEnvSustain), 0.7f);
    set(G::env_param(1, G::kEnvRelease), 0.12f);
    set(G::kParamLevel, 0.3f);
  });
  m.reverb_send = 0.05f;
}

void pad_patch(Module& m) {
  m.patch([](auto set) {
    base_patch(set);
    set(G::osc_param(0, G::kOscMode), static_cast<float>(M::kSupersaw));
    set(G::osc_param(0, G::kOscPw), 0.4f);
    set(G::osc_param(0, G::kOscLevel), 0.35f);
    set(G::osc_param(1, G::kOscWave), 20.0f);   // our table, its wave drifting
    set(G::osc_param(1, G::kOscTune), 12.0f);
    set(G::osc_param(1, G::kOscLevel), 0.3f);
    set(G::osc_param(2, G::kOscTable), 1.0f);   // Plaits' bank
    set(G::osc_param(2, G::kOscWave), 40.0f);
    set(G::osc_param(2, G::kOscFine), 5.0f);
    set(G::osc_param(2, G::kOscLevel), 0.2f);
    set(G::lfo_param(1, G::kLfoRate), 0.2f);
    set(G::lfo_param(1, G::kLfoShape), static_cast<float>(synth::Lfo::Shape::kTriangle));
    set(G::matrix_param(0, 0), synth::mod::kSrcLfo2);
    set(G::matrix_param(0, 1), synth::mod::kDstWave2);
    set(G::matrix_param(0, 2), 8.0f);
    set(G::kParamSvfCutoff, 50.0f);   // SVF as a high pass: keep the low end for the bass
    set(G::kParamSvfMorph, 1.0f);
    set(G::kParamLadderCutoff, 78.0f);
    set(G::kParamLadderResonance, 0.15f);
    set(G::kParamLadderEnvAmount, 6.0f);
    set(G::env_param(0, G::kEnvAttack), 1.0f);
    set(G::env_param(0, G::kEnvDecay), 2.0f);
    set(G::env_param(0, G::kEnvSustain), 0.6f);
    set(G::env_param(0, G::kEnvRelease), 2.0f);
    set(G::env_param(1, G::kEnvAttack), 0.9f);
    set(G::env_param(1, G::kEnvDecay), 1.0f);
    set(G::env_param(1, G::kEnvSustain), 0.8f);
    set(G::env_param(1, G::kEnvRelease), 2.2f);
    set(G::kParamLevel, 0.16f);
  });
  m.reverb_send = 0.4f;
}

// ------------------------------------------------------------------ score ---

struct Note { double start, length; int module, note; float velocity; };

struct Score {
  std::vector<Note> notes;
  double bpm = 120.0;
  double offset = 0.0;   // seconds
  double beat(double b) const { return offset + b * 60.0 / bpm; }
  void add(int module, int note, double start_beat, double beats, float velocity = 0.9f) {
    notes.push_back({beat(start_beat), beats * 60.0 / bpm, module, note, velocity});
  }
};

// MIDI note from a name like "F#5"
int n(const char* name) {
  static const int base[] = {9, 11, 0, 2, 4, 5, 7};   // A B C D E F G
  int note = base[name[0] - 'A'];
  int i = 1;
  if (name[i] == '#') { ++note; ++i; }
  if (name[i] == 'b') { --note; ++i; }
  return note + 12 * (std::atoi(name + i) + 1);
}

struct Chord { double start, beats; std::vector<const char*> tones; const char* bass; };

void arrange(Score& s, const std::vector<std::pair<const char*, std::pair<double, double>>>& melody,
             const std::vector<Chord>& chords, int repeats, double bar_beats, bool final_hold) {
  const double length = bar_beats * 4;
  for (int r = 0; r < repeats; ++r) {
    const double t0 = r * length;
    for (const auto& [name, at] : melody) s.add(kBell, n(name), t0 + at.first, at.second * 0.9, 0.85f);
    for (const Chord& c : chords) {
      // Mallet: broken chord in eighths over the chord's span
      static const int pattern[] = {0, 1, 2, 3, 1, 2, 3, 2};
      const int steps = static_cast<int>(c.beats * 2);
      for (int k = 0; k < steps; ++k) {
        const int idx = pattern[k % 8] % static_cast<int>(c.tones.size());
        s.add(kMallet, n(c.tones[idx]), t0 + c.start + 0.5 * k, 0.45, k % 2 ? 0.6f : 0.8f);
      }
      // Bass: root, then the fifth's octave push on the last eighth
      s.add(kBass, n(c.bass), t0 + c.start, c.beats - 0.6, 0.9f);
      s.add(kBass, n(c.bass) + 12, t0 + c.start + c.beats - 0.5, 0.4, 0.7f);
      // Pad: the chord, held
      for (const char* t : c.tones) s.add(kPad, n(t) - 12, t0 + c.start, c.beats, 0.7f);
    }
  }
  if (final_hold) {
    const Chord& last = chords.back();
    const double t = repeats * length;
    for (const char* tn : last.tones) s.add(kPad, n(tn) - 12, t, 4.0, 0.7f);
    s.add(kBass, n(last.bass), t, 3.0, 0.8f);
    s.add(kMallet, n(last.tones[0]) + 12, t, 1.0, 0.8f);
  }
}

// Departure melody A: D major, bright, 132 bpm, 4 bars
void departure_a(Score& s, int repeats) {
  s.bpm = 132.0;
  arrange(s,
          {{"A5", {0, .5}}, {"F#5", {.5, .5}}, {"A5", {1, .5}}, {"D6", {1.5, 1.5}}, {"C#6", {3, .5}}, {"B5", {3.5, .5}},
           {"A5", {4, 1}}, {"F#5", {5, .5}}, {"G5", {5.5, .5}}, {"A5", {6, 1}}, {"E5", {7, 1}},
           {"G5", {8, .5}}, {"B5", {8.5, .5}}, {"D6", {9, .5}}, {"E6", {9.5, 1.5}}, {"D6", {11, .5}}, {"C#6", {11.5, .5}},
           {"D6", {12, 1}}, {"A5", {13, .5}}, {"F#5", {13.5, .5}}, {"D5", {14, 2}}},
          {{0, 4, {"D4", "F#4", "A4", "D5"}, "D2"},
           {4, 2, {"B3", "D4", "F#4", "B4"}, "B1"},
           {6, 2, {"A3", "C#4", "E4", "A4"}, "A1"},
           {8, 4, {"G3", "B3", "D4", "G4"}, "G1"},
           {12, 2, {"A3", "D4", "E4", "A4"}, "A1"},
           {14, 2, {"D4", "F#4", "A4", "D5"}, "D2"}},
          repeats, 4, true);
}

// Departure melody B: F major with a lydian turn, gentler, 100 bpm, 4 bars
void departure_b(Score& s, int repeats) {
  s.bpm = 100.0;
  arrange(s,
          {{"C6", {0, .5}}, {"A5", {.5, .5}}, {"F5", {1, .5}}, {"G5", {1.5, .5}}, {"A5", {2, 1}}, {"C6", {3, 1}},
           {"B5", {4, .5}}, {"A5", {4.5, .5}}, {"G5", {5, .5}}, {"D5", {5.5, .5}}, {"G5", {6, 2}},
           {"E6", {8, .5}}, {"D6", {8.5, .5}}, {"C6", {9, .5}}, {"B5", {9.5, .5}}, {"A5", {10, 1}}, {"E5", {11, 1}},
           {"F5", {12, .5}}, {"G5", {12.5, .5}}, {"A5", {13, .5}}, {"C6", {13.5, .5}}, {"F6", {14, 2}}},
          {{0, 4, {"F3", "A3", "C4", "E4"}, "F1"},
           {4, 4, {"G3", "B3", "D4", "F4"}, "F1"},
           {8, 2, {"E3", "G3", "B3", "D4"}, "E1"},
           {10, 2, {"A3", "C4", "E4", "G4"}, "A1"},
           {12, 2, {"Bb3", "D4", "F4", "A4"}, "Bb1"},
           {14, 2, {"F3", "A3", "C4", "F4"}, "F1"}},
          repeats, 4, true);
}

// Announcement chimes: a rising arpeggio before, falling after
void chime(Score& s, bool up) {
  s.bpm = 96.0;
  const char* rising[] = {"E5", "G#5", "B5", "E6"};
  for (int i = 0; i < 4; ++i) {
    const char* name = rising[up ? i : 3 - i];
    s.add(kBell, n(name), i, i == 3 ? 3.0 : 0.9, 0.9f);
    s.add(kMallet, n(name) - 12, i, 0.5, 0.5f);
  }
}

// ---------------------------------------------------------------- presets ---

const char* const kNames[kModules] = {"bell", "mallet", "bass", "pad"};
std::string g_load_dir;   // --presets

void patch_all(std::unique_ptr<Module> (&mods)[kModules]) {
  for (auto& m : mods) m = std::make_unique<Module>();
  bell_patch(*mods[kBell]);
  mallet_patch(*mods[kMallet]);
  bass_patch(*mods[kBass]);
  pad_patch(*mods[kPad]);
  if (g_load_dir.empty()) return;
  synth::Bus bus;
  for (int k = 0; k < kModules; ++k) {
    const std::string path = g_load_dir + "/station/" + kNames[k] + ".json";
    std::string text, error;
    std::vector<preset::Param> params;
    if (!preset::load(path, text)) continue;
    if (!preset::from_json(text, params, nullptr, &error)) {
      std::fprintf(stderr, "%s: %s (kept the built-in sound)\n", path.c_str(), error.c_str());
      continue;
    }
    // More than the event queue holds: empty it, then set the groups directly
    while (mods[k]->engine.pending()) {
      mods[k]->engine.render(0, bus);
      mods[k]->engine.render(1, bus);
    }
    for (const auto& p : params) {
      mods[k]->g0.set_param(p.id, p.value);
      mods[k]->g1.set_param(p.id, p.value);
    }
  }
}

void export_presets(const std::string& dir) {
  std::unique_ptr<Module> mods[kModules];
  patch_all(mods);
  std::filesystem::create_directories(dir + "/station");
  synth::Bus bus;
  for (int k = 0; k < kModules; ++k) {
    while (mods[k]->engine.pending()) {   // applies the queued parameters
      mods[k]->engine.render(0, bus);
      mods[k]->engine.render(1, bus);
    }
    preset::save(dir + "/station/" + kNames[k] + ".json",
                 preset::to_json(mods[k]->g0, kNames[k], "From demo/station_demo.cpp"));
  }
}

// ------------------------------------------------------------------ render ---

std::vector<float> render(const Score& score, double seconds, double delay_seconds) {
  std::unique_ptr<Module> mods[kModules];
  patch_all(mods);

  struct Event { size_t block; int module, note; float velocity; bool on; };
  std::vector<Event> events;
  for (const Note& nt : score.notes) {
    events.push_back({static_cast<size_t>(nt.start * kRate / kN), nt.module, nt.note, nt.velocity, true});
    events.push_back({static_cast<size_t>((nt.start + nt.length) * kRate / kN), nt.module, nt.note, 0.0f, false});
  }
  // Offs before ons in the same block, so a repeated note retriggers
  std::stable_sort(events.begin(), events.end(), [](const Event& a, const Event& b) {
    return a.block != b.block ? a.block < b.block : (!a.on && b.on);
  });

  Hall hall;
  PingPong delay(delay_seconds);
  const size_t blocks = static_cast<size_t>(seconds * kRate / kN);
  std::vector<float> out;
  out.reserve(blocks * kN * 2);
  size_t next = 0;
  double energy[kModules] = {};
  float dc_l = 0.0f, dc_r = 0.0f;
  synth::Bus b0, b1, mix[kModules];
  for (size_t block = 0; block < blocks; ++block) {
    for (; next < events.size() && events[next].block <= block; ++next) {
      const Event& e = events[next];
      auto& eng = mods[e.module]->engine;
      if (e.on) eng.note_on(static_cast<uint8_t>(e.note), e.velocity);
      else eng.note_off(static_cast<uint8_t>(e.note));
    }
    for (int m = 0; m < kModules; ++m) {
      mods[m]->engine.render(0, b0);
      mods[m]->engine.render(1, b1);
      engine::VoiceEngine::mix(b0, b1, mix[m]);
    }
    for (size_t i = 0; i < kN; ++i) {
      float l = 0.0f, r = 0.0f, rev_in = 0.0f, dly_in = 0.0f;
      for (int m = 0; m < kModules; ++m) {
        energy[m] += mix[m].l[i] * mix[m].l[i] + mix[m].r[i] * mix[m].r[i];
        l += mix[m].l[i];
        r += mix[m].r[i];
        const float mono = 0.5f * (mix[m].l[i] + mix[m].r[i]);
        rev_in += mods[m]->reverb_send * mono;
        dly_in += mods[m]->delay_send * mono;
      }
      float dl, dr, rl, rr;
      delay.process(dly_in, dl, dr);
      hall.process(rev_in + 0.3f * (dl + dr), rl, rr);
      // DC blocker (~5 Hz high pass), as the master's output stage would have
      const float yl = l + dl + rl, yr = r + dr + rr;
      dc_l += 0.00065f * (yl - dc_l);
      dc_r += 0.00065f * (yr - dc_r);
      out.push_back(yl - dc_l);
      out.push_back(yr - dc_r);
    }
  }
  double total = 0.0;
  for (double e : energy) total += e;
  std::printf("  balance (share of dry energy): bell %.0f%%, mallet %.0f%%, bass %.0f%%, pad %.0f%%\n",
              100 * energy[0] / total, 100 * energy[1] / total, 100 * energy[2] / total, 100 * energy[3] / total);
  return out;
}

void make(const std::string& path, const Score& s, double seconds, double delay_seconds) {
  auto x = render(s, seconds, delay_seconds);
  normalize(x, -1.0f);
  write_wav24(path, x);
}

}  // namespace

int main(int argc, char** argv) {
  const std::string dir = argc > 1 ? argv[1] : ".";
  if (argc > 3 && std::string(argv[2]) == "--presets") g_load_dir = argv[3];
  export_presets(dir + "/presets");
  const double dotted_eighth_a = 0.75 * 60.0 / 132.0, dotted_eighth_b = 0.75 * 60.0 / 100.0;

  { Score s; chime(s, true); make(dir + "/01_chime.wav", s, 4.5, dotted_eighth_a); }
  { Score s; departure_a(s, 2); make(dir + "/02_departure_melody_A.wav", s, 2 * 16 * 60.0 / 132.0 + 5.0, dotted_eighth_a); }
  { Score s; departure_b(s, 2); make(dir + "/03_departure_melody_B.wav", s, 2 * 16 * 60.0 / 100.0 + 5.0, dotted_eighth_b); }

  // The whole scene: chime, melody A, closing chime, melody B, closing chime
  Score all;
  double t = 0.0;
  auto append = [&](auto fill, double bpm_seconds) {
    Score s;
    s.offset = t;
    fill(s);
    all.notes.insert(all.notes.end(), s.notes.begin(), s.notes.end());
    t += bpm_seconds;
  };
  append([](Score& s) { chime(s, true); }, 3.5);
  append([](Score& s) { departure_a(s, 2); }, 2 * 16 * 60.0 / 132.0 + 3.5);
  append([](Score& s) { chime(s, false); }, 5.0);
  append([](Score& s) { chime(s, true); }, 3.5);
  append([](Score& s) { departure_b(s, 2); }, 2 * 16 * 60.0 / 100.0 + 3.5);
  append([](Score& s) { chime(s, false); }, 5.0);
  make(dir + "/04_station_scene.wav", all, t + 1.0, dotted_eighth_a);
  return 0;
}
