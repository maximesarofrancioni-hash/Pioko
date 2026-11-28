#pragma once
// The demo song machinery shared by the showcase renderers: tracks (one voice
// engine each, as one voice module), notes with parameter locks, automation,
// the render loop with stand-ins for the master's effects (sidechain ducking,
// delay, reverb, limiter), and preset export/import.

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
#include "engine/voice_engine.h"
#include "preset/preset_json.h"
#include "synth/lfo.h"
#include "synth/mod_matrix.h"
#include "synth/oscillator.h"
#include "synth/triple_wave_group.h"

namespace demo {

using G = synth::TripleWaveGroup;
using M = synth::Oscillator::Mode;
using Shape = synth::Lfo::Shape;
namespace mod = synth::mod;
constexpr size_t kN = audio::kBlockSize;

// ----------------------------------------------------------------- tracks ---

struct Track {
  const char* name = "";
  G g0, g1;
  engine::VoiceEngine engine{g0, g1};
  std::map<uint16_t, float> base;   // patch values, restored after a lock
  float gain = 1.0f, reverb = 0.0f, delay = 0.0f, duck = 0.0f;
  std::function<void(Track&, double beat)> automate;

  // Pitch bends (meend, kan, andolan): an offset in semitones added to all three
  // oscillators' tune. Each segment ramps from..to between two beats, then holds
  // its end value until the next segment starts.
  struct Bend { double from_beat, to_beat; float from, to; };
  std::vector<Bend> bends;
  float bend_now = 0.0f;
  void bend(double from_beat, double to_beat, float from, float to) { bends.push_back({from_beat, to_beat, from, to}); }
  float bend_at(double beat) const {
    const Bend* cur = nullptr;
    for (const Bend& b : bends) if (b.from_beat <= beat && (!cur || b.from_beat >= cur->from_beat)) cur = &b;
    if (!cur) return 0.0f;
    if (beat >= cur->to_beat) return cur->to;
    const double x = (beat - cur->from_beat) / (cur->to_beat - cur->from_beat);
    return static_cast<float>(cur->from + (cur->to - cur->from) * x);
  }
  float base_of(uint16_t id) const {
    auto it = base.find(id);
    return it == base.end() ? 0.0f : it->second;
  }

  void set(uint16_t id, float v) {
    base[id] = v;
    engine.set_param(id, v);
  }
  void osc(int o, M mode, float level, float tune = 0.0f) {
    set(G::osc_param(o, G::kOscMode), static_cast<float>(mode));
    set(G::osc_param(o, G::kOscLevel), level);
    set(G::osc_param(o, G::kOscTune), tune);
  }
  void env(int e, float a, float d, float s, float r) {
    set(G::env_param(e, G::kEnvAttack), a);
    set(G::env_param(e, G::kEnvDecay), d);
    set(G::env_param(e, G::kEnvSustain), s);
    set(G::env_param(e, G::kEnvRelease), r);
  }
  void slot(int i, uint8_t src, uint8_t dst, float amount) {
    set(G::matrix_param(i, 0), src);
    set(G::matrix_param(i, 1), dst);
    set(G::matrix_param(i, 2), amount);
  }
  void lfo(int l, float hz, Shape shape) {
    set(G::lfo_param(l, G::kLfoRate), hz);
    set(G::lfo_param(l, G::kLfoShape), static_cast<float>(shape));
  }
  void reset() {   // a neutral starting point: everything silent and open
    for (int o = 0; o < 3; ++o) set(G::osc_param(o, G::kOscLevel), 0.0f);
    set(G::kParamSvfCutoff, 135.0f);
    set(G::kParamLadderCutoff, 135.0f);
    set(G::kParamLadderResonance, 0.0f);
    set(G::kParamLadderEnvAmount, 0.0f);
    set(G::kParamKeyTrack, 0.5f);
  }
};

// ------------------------------------------------------------------- song ---

using Locks = std::vector<std::pair<uint16_t, float>>;

struct Song {
  std::string file;
  double bpm = 120.0, beats = 64.0, tail = 4.0, delay_beats = 0.75;
  std::vector<std::unique_ptr<Track>> tracks;
  int kick = -1;   // the track whose hits duck the others

  enum Kind { kOff, kRevert, kLock, kOn };
  struct Event { double t; int track; Kind kind; int note; float value; uint16_t id; };
  std::vector<Event> events;
  std::vector<double> kick_times;

  Song(std::string f, double tempo, int bars) : file(std::move(f)), bpm(tempo), beats(4.0 * bars) {}
  double sec(double beat) const { return beat * 60.0 / bpm; }
  Track& add(const char* name) {
    tracks.push_back(std::make_unique<Track>());
    tracks.back()->name = name;
    tracks.back()->reset();
    return *tracks.back();
  }

  // A note, with parameter locks that hold for its length (then the patch value returns)
  void trig(int tr, double beat, int note, double len, float vel,
            const Locks& locks = {}) {
    for (const auto& [id, v] : locks) {
      events.push_back({sec(beat), tr, kLock, 0, v, id});
      auto it = tracks[tr]->base.find(id);
      events.push_back({sec(beat + len), tr, kRevert, 0, it != tracks[tr]->base.end() ? it->second : v, id});
    }
    events.push_back({sec(beat), tr, kOn, note, vel, 0});
    events.push_back({sec(beat + len), tr, kOff, note, 0.0f, 0});
    if (tr == kick) kick_times.push_back(sec(beat));
  }
};

inline int n(const char* name) {   // "F#5" -> MIDI note
  static const int base[] = {9, 11, 0, 2, 4, 5, 7};
  int note = base[name[0] - 'A'], i = 1;
  if (name[i] == '#') { ++note; ++i; }
  if (name[i] == 'b') { --note; ++i; }
  return note + 12 * (std::atoi(name + i) + 1);
}

inline uint32_t g_rand = 12345;
inline float rnd() {   // 0..1, reproducible
  g_rand = g_rand * 1664525u + 1013904223u;
  return static_cast<float>(g_rand >> 8) / 16777216.0f;
}

// ---------------------------------------------------------------- presets ---

inline std::string slug(const std::string& s) {
  std::string r;
  for (char c : s) r += std::isalnum(static_cast<unsigned char>(c)) ? c : '_';
  return r;
}

// "01_acid_line.wav" -> "acid_line"
inline std::string song_id(const Song& s) { return s.file.substr(3, s.file.size() - 7); }

inline void export_presets(Song& s, const std::string& dir) {
  const std::string folder = dir + "/" + song_id(s);
  std::filesystem::create_directories(folder);
  synth::Bus b;
  for (auto& t : s.tracks) {
    while (t->engine.pending()) {   // applies the queued parameters to the groups
      t->engine.render(0, b);
      t->engine.render(1, b);
    }
    const std::string path = folder + "/" + slug(t->name) + ".json";
    preset::save(path, preset::to_json(t->g0, t->name, "From demo/showcase.cpp, " + song_id(s)));
  }
}

// Replaces track sounds with the presets found in dir (the mix sends stay)
inline void import_presets(Song& s, const std::string& dir) {
  for (auto& t : s.tracks) {
    const std::string path = dir + "/" + song_id(s) + "/" + slug(t->name) + ".json";
    std::string text, error;
    if (!preset::load(path, text)) continue;
    std::vector<preset::Param> params;
    if (!preset::from_json(text, params, nullptr, &error)) {
      std::fprintf(stderr, "%s: %s (kept the built-in sound)\n", path.c_str(), error.c_str());
      continue;
    }
    // A whole preset is more than the engine's event queue holds: empty the
    // queue, then set both cores' groups directly (nothing is playing yet)
    synth::Bus bus;
    while (t->engine.pending()) {
      t->engine.render(0, bus);
      t->engine.render(1, bus);
    }
    for (const auto& p : params) {
      t->g0.set_param(p.id, p.value);
      t->g1.set_param(p.id, p.value);
      t->base[p.id] = p.value;
    }
    std::printf("  %s: loaded %s\n", t->name, path.c_str());
  }
}

// ------------------------------------------------------------------ render ---

inline void render(Song& s, const std::string& dir) {
  std::stable_sort(s.events.begin(), s.events.end(), [](const Song::Event& a, const Song::Event& b) {
    const long ba = std::lround(a.t * kRate / kN - 0.5), bb = std::lround(b.t * kRate / kN - 0.5);
    return ba != bb ? ba < bb : a.kind < b.kind;
  });
  std::sort(s.kick_times.begin(), s.kick_times.end());

  Hall hall;
  PingPong delay(s.sec(s.delay_beats));
  const size_t tracks = s.tracks.size();
  const size_t blocks = static_cast<size_t>((s.sec(s.beats) + s.tail) * kRate / kN);
  std::vector<float> out;
  out.reserve(blocks * kN * 2);
  std::vector<double> energy(tracks, 0.0);
  std::vector<synth::Bus> bus(tracks);
  synth::Bus b0, b1;
  size_t next = 0, next_kick = 0;
  double last_kick = -1e9;

  for (size_t block = 0; block < blocks; ++block) {
    const double t_block = static_cast<double>(block * kN) / kRate;
    const double beat = t_block * s.bpm / 60.0;
    for (; next < s.events.size() && s.events[next].t * kRate / kN < static_cast<double>(block + 1); ++next) {
      const Song::Event& e = s.events[next];
      auto& eng = s.tracks[e.track]->engine;
      switch (e.kind) {
        case Song::kOn: eng.note_on(static_cast<uint8_t>(e.note), e.value); break;
        case Song::kOff: eng.note_off(static_cast<uint8_t>(e.note)); break;
        default: eng.set_param(e.id, e.value); break;
      }
    }
    for (size_t k = 0; k < tracks; ++k) {
      Track& tr = *s.tracks[k];
      if (tr.automate && beat <= s.beats) tr.automate(tr, beat);
      if (!tr.bends.empty()) {
        const float b = tr.bend_at(beat);
        if (b != tr.bend_now) {
          for (int o = 0; o < 3; ++o) {
            const uint16_t id = G::osc_param(o, G::kOscTune);
            tr.engine.set_param(id, tr.base_of(id) + b);
          }
          tr.bend_now = b;
        }
      }
      tr.engine.render(0, b0);
      tr.engine.render(1, b1);
      engine::VoiceEngine::mix(b0, b1, bus[k]);
    }
    for (size_t i = 0; i < kN; ++i) {
      const double t = t_block + static_cast<double>(i) / kRate;
      while (next_kick < s.kick_times.size() && s.kick_times[next_kick] <= t) last_kick = s.kick_times[next_kick++];
      // Sidechain: dips 4 ms after each kick, back over ~200 ms
      const double dt = t - last_kick;
      const float pump = static_cast<float>(std::min(1.0, dt / 0.004) * std::exp(-dt / 0.13));
      float l = 0.0f, r = 0.0f, rev = 0.0f, dly = 0.0f;
      for (size_t k = 0; k < tracks; ++k) {
        const Track& tr = *s.tracks[k];
        const float g = tr.gain * (1.0f - tr.duck * pump);
        const float tl = bus[k].l[i] * g, trr = bus[k].r[i] * g;
        energy[k] += tl * tl + trr * trr;
        l += tl;
        r += trr;
        rev += tr.reverb * 0.5f * (tl + trr);
        dly += tr.delay * 0.5f * (tl + trr);
      }
      float dl, dr, rl, rr;
      delay.process(dly, dl, dr);
      hall.process(rev + 0.3f * (dl + dr), rl, rr);
      out.push_back(l + dl + rl);
      out.push_back(r + dr + rr);
    }
  }

  // Master: DC blocker, then a soft limiter driven ~4 dB into its knee
  float dc_l = 0.0f, dc_r = 0.0f, peak = 0.0f;
  for (size_t i = 0; i < out.size(); i += 2) {
    dc_l += 0.00065f * (out[i] - dc_l);
    dc_r += 0.00065f * (out[i + 1] - dc_r);
    out[i] -= dc_l;
    out[i + 1] -= dc_r;
    peak = std::max({peak, std::fabs(out[i]), std::fabs(out[i + 1])});
  }
  for (float& v : out) v = std::tanh(1.6f * v / peak);
  normalize(out, -1.0f);
  write_wav24(dir + "/" + s.file, out);

  double total = 0.0;
  for (double e : energy) total += e;
  std::printf("  ");
  for (size_t k = 0; k < tracks; ++k) {
    std::printf("%s %.1f dB%s", s.tracks[k]->name, 10 * std::log10(energy[k] / total + 1e-12), k + 1 < tracks ? ", " : "\n");
  }
}


}  // namespace demo
