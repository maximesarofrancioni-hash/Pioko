// Synth indie pop test pieces: three original songs, four engines each (as
// four voice modules): a kick, snare and hats on one track (the hats are
// parameter locks on the snare), and two melodic parts. Everything is
// TripleWaveGroup; ducking, delay, reverb and the limiter stand in for the
// master's effects.
//
//   indiepop <out_dir> [--presets <dir>]

#include "song.h"

namespace {

using namespace demo;

// ------------------------------------------------------------------ drums ---

void kick_patch(Track& t, float punch, float decay) {
  t.osc(0, M::kSine, 1.0f);
  t.osc(2, M::kNoise, 0.0f);
  t.set(G::kParamKeyTrack, 0.0f);
  t.env(2, 0.0005f, 0.04f, 0.0f, 0.04f);
  t.slot(0, mod::kSrcEnv3, mod::kDstPitch1, punch);
  t.slot(1, mod::kSrcEnv3, mod::kDstLevel3, 0.2f);
  t.env(1, 0.0005f, decay, 0.0f, 0.08f);
  t.set(G::kParamLadderCutoff, 96.0f);
  t.set(G::kParamDrive, 2.2f);
  t.set(G::kParamCompensation, 1.0f);
  t.set(G::kParamLevel, 0.5f);
}

void snare_patch(Track& t, float body, float decay) {
  t.osc(0, M::kSine, body);
  t.osc(1, M::kNoise, 0.55f);
  t.set(G::kParamKeyTrack, 0.0f);
  t.env(2, 0.0005f, 0.03f, 0.0f, 0.03f);
  t.slot(0, mod::kSrcEnv3, mod::kDstPitch1, 10.0f);
  t.set(G::kParamSvfCutoff, 60.0f);
  t.set(G::kParamSvfMorph, 1.0f);
  t.set(G::kParamLadderCutoff, 114.0f);
  t.env(1, 0.0005f, decay, 0.0f, 0.1f);
  t.set(G::kParamLevel, 0.62f);
  t.reverb = 0.35f;   // the 80s room
}

// Locks that turn the snare into a closed hat, or a shaker
Locks hat(float decay = 0.035f) {
  return {{G::osc_param(0, G::kOscLevel), 0.0f}, {G::kParamSvfCutoff, 108.0f},
          {G::env_param(1, G::kEnvDecay), decay}, {G::env_param(1, G::kEnvRelease), decay}};
}

// ---------------------------------------------------------------- helpers ---

std::vector<int> notes(std::initializer_list<const char*> names) {
  std::vector<int> v;
  for (const char* x : names) v.push_back(n(x));
  return v;
}

struct Line { const char* note; double at, len; };

// ------------------------------------------- 1. Night drive (108, Ab major) ---

Song night_drive() {
  Song s("01_night_drive.wav", 108.0, 20);
  s.tail = 4.0;
  s.delay_beats = 0.75;
  s.kick = 0;
  kick_patch(s.add("kick"), 22.0f, 0.32f);
  Track& sn = s.add("snare+hats");
  snare_patch(sn, 0.45f, 0.18f);

  Track& bass = s.add("bass");
  bass.osc(0, M::kPulse, 0.6f);
  bass.set(G::osc_param(0, G::kOscPw), 0.4f);
  bass.osc(1, M::kSine, 0.4f, -12.0f);
  bass.set(G::kParamLadderCutoff, 58.0f);
  bass.set(G::kParamLadderResonance, 0.25f);
  bass.set(G::kParamLadderEnvAmount, 22.0f);
  bass.env(0, 0.002f, 0.18f, 0.1f, 0.1f);
  bass.env(1, 0.002f, 0.3f, 0.6f, 0.08f);
  bass.set(G::kParamLevel, 0.26f);
  bass.duck = 0.3f;

  // A Juno-flavoured poly: saw, a pulse with slow PWM, a detuned saw, voices
  // drifting across the stereo field
  Track& poly = s.add("poly");
  poly.osc(0, M::kSaw, 0.4f);
  poly.osc(1, M::kPulse, 0.3f);
  poly.set(G::osc_param(1, G::kOscPw), 0.45f);
  poly.osc(2, M::kSaw, 0.25f);
  poly.set(G::osc_param(2, G::kOscFine), 8.0f);
  poly.lfo(0, 0.6f, Shape::kTriangle);
  poly.slot(0, mod::kSrcLfo1, mod::kDstPw2, 0.15f);
  poly.lfo(1, 0.21f, Shape::kSine);
  poly.slot(1, mod::kSrcLfo2, mod::kDstPan, 0.35f);
  poly.set(G::kParamLadderCutoff, 80.0f);
  poly.set(G::kParamLadderResonance, 0.15f);
  poly.set(G::kParamLadderEnvAmount, 16.0f);
  poly.env(0, 0.005f, 0.45f, 0.35f, 0.4f);
  poly.env(1, 0.008f, 0.6f, 0.65f, 0.45f);
  poly.set(G::kParamLevel, 0.13f);
  poly.reverb = 0.3f;
  poly.delay = 0.22f;
  poly.duck = 0.45f;
  poly.automate = [](Track& t, double beat) {   // the filter opens over the song
    t.engine.set_param(G::kParamLadderCutoff, static_cast<float>(72.0 + 16.0 * std::min(1.0, beat / 64.0)));
  };
  enum { kKick, kSnare, kBass, kPoly };

  const std::vector<std::vector<int>> chords = {notes({"F3", "Ab3", "C4", "F4"}), notes({"Db3", "F3", "Ab3", "Db4"}),
                                                notes({"Ab3", "C4", "Eb4", "Ab4"}), notes({"Eb3", "G3", "Bb3", "Eb4"})};
  const int roots[] = {n("F1"), n("Db2"), n("Ab1"), n("Eb2")};
  const Line hook[] = {{"C5", 0, 1.5},  {"Ab4", 1.5, .5}, {"C5", 2, 1},    {"Eb5", 3, 1},
                       {"F5", 4, 1.5},  {"Eb5", 5.5, .5}, {"Db5", 6, 1},   {"C5", 7, 1},
                       {"Eb5", 8, 1.5}, {"C5", 9.5, .5},  {"Ab4", 10, 1},  {"Bb4", 11, 1},
                       {"G4", 12, 2},   {"Bb4", 14, 1},   {"C5", 15, 1}};
  const int arp[16] = {0, 1, 2, 3, 4, 3, 2, 1, 0, 2, 3, 5, 4, 3, 2, 1};

  for (int bar = 0; bar < 20; ++bar) {
    const double b = 4.0 * bar;
    const auto& ch = chords[bar % 4];
    const bool chorus = bar >= 12;
    // Drums: four on the floor from bar 2, backbeat and hats from bar 4
    for (int q = 0; q < 4; ++q) {
      if (bar >= 2) s.trig(kKick, b + q, 33, 0.25, 0.95f);
      if (bar >= 4 && (q == 1 || q == 3)) s.trig(kSnare, b + q, 50, 0.3, 0.95f);
      if (bar >= 4) {
        s.trig(kSnare, b + q + 0.5, 50, 0.2, 0.75f, hat());
        if (chorus) s.trig(kSnare, b + q + 0.25, 50, 0.1, 0.35f, hat(0.02f));
      }
    }
    // Bass: the root bouncing in octaves, eighths
    if (bar >= 4) {
      for (int e = 0; e < 8; ++e) s.trig(kBass, b + 0.5 * e, roots[bar % 4] + (e % 2 ? 12 : 0), 0.4, e % 2 ? 0.7f : 0.9f);
    }
    // Poly: arpeggio in the verse, held chords plus the hook in the chorus
    if (!chorus) {
      for (int st = 0; st < 16; ++st) {
        const int i = arp[st];
        const int note = ch[i % 4] + 12 * (i / 4);
        s.trig(kPoly, b + 0.25 * st, note + 12, 0.22, (st % 4 == 0 ? 0.8f : 0.55f) + 0.1f * rnd());
      }
    } else {
      for (int note : ch) s.trig(kPoly, b, note, 3.6, 0.65f);
      for (const Line& l : hook) {
        if (l.at >= 4.0 * (bar % 4) && l.at < 4.0 * (bar % 4) + 4.0) {
          s.trig(kPoly, b + l.at - 4.0 * (bar % 4), n(l.note), l.len * 0.92, 0.9f);
        }
      }
    }
  }
  s.trig(kKick, 80.0, 33, 0.25, 1.0f);
  s.trig(kBass, 80.0, roots[0], 2.0, 0.9f);
  for (int note : chords[0]) s.trig(kPoly, 80.0, note, 3.0, 0.7f);
  return s;
}

// ----------------------------------- 2. Bedroom pop (88, C major, swung) ---

Song bedroom() {
  Song s("02_bedroom_pop.wav", 88.0, 14);
  s.tail = 4.0;
  s.delay_beats = 0.75;
  s.kick = 0;
  kick_patch(s.add("kick"), 18.0f, 0.4f);
  Track& sn = s.add("snare+shaker");
  snare_patch(sn, 0.6f, 0.12f);
  sn.set(G::kParamLevel, 0.5f);

  // An FM electric piano with tremolo and auto-pan
  Track& ep = s.add("e-piano");
  ep.osc(0, M::kSine, 0.0f);
  ep.osc(1, M::kSine, 0.7f);
  ep.set(G::osc_param(1, G::kOscFm), 0.45f);
  ep.osc(2, M::kSine, 0.08f, 12.0f);
  ep.set(G::kParamLadderCutoff, 98.0f);
  ep.set(G::kParamLadderEnvAmount, 10.0f);
  ep.env(0, 0.002f, 0.6f, 0.2f, 0.4f);
  ep.env(1, 0.002f, 1.6f, 0.3f, 0.4f);
  ep.lfo(0, 4.5f, Shape::kSine);
  ep.slot(0, mod::kSrcLfo1, mod::kDstAmp, 0.008f);
  ep.slot(1, mod::kSrcLfo1, mod::kDstPan, 0.4f);
  ep.set(G::kParamLevel, 0.13f);
  ep.reverb = 0.3f;

  // A soft whistle lead: sine, a breath of octave triangle, delayed vibrato, and
  // scoops into the notes (pitch bends)
  Track& lead = s.add("whistle");
  lead.osc(0, M::kSine, 0.7f);
  lead.osc(1, M::kTriangle, 0.05f, 12.0f);
  lead.osc(2, M::kColoredNoise, 0.02f);
  lead.set(G::kParamLadderCutoff, 110.0f);
  lead.env(1, 0.05f, 0.3f, 0.9f, 0.25f);
  lead.lfo(0, 5.0f, Shape::kSine);
  lead.set(G::lfo_param(0, G::kLfoDelay), 0.4f);
  lead.slot(0, mod::kSrcLfo1, mod::kDstPitch1, 0.15f);
  lead.slot(1, mod::kSrcLfo1, mod::kDstPitch2, 0.15f);
  lead.set(G::kParamLevel, 0.16f);
  lead.set(G::kParamPan, 0.15f);
  lead.reverb = 0.35f;
  lead.delay = 0.28f;
  enum { kKick, kSnare, kEp, kLead };

  const double swing = 0.08;   // late off-beat sixteenths, in beats
  auto sw = [&](double at) {
    const double q = at * 4.0;
    const bool odd = std::fabs(q - std::round(q)) < 1e-6 && static_cast<long>(std::round(q)) % 2 == 1;
    return at + (odd ? swing : 0.0);
  };
  const std::vector<std::vector<int>> chords = {notes({"C4", "E4", "G4", "B4"}), notes({"A3", "C4", "E4", "G4"}),
                                                notes({"F3", "A3", "C4", "E4"}), notes({"G3", "B3", "D4", "E4"})};
  const int roots[] = {n("C3"), n("A2"), n("F2"), n("G2")};
  const Line melody[] = {{"E5", .5, 1},  {"G5", 1.5, .5}, {"B5", 2, 1.5},  {"A5", 3.5, .5},
                         {"G5", 4, 1},   {"E5", 5, 1},    {"C5", 6, 1.5},
                         {"A5", 8.5, .5}, {"C6", 9, 1},   {"B5", 10, .5},  {"A5", 10.5, .5}, {"G5", 11, 1},
                         {"E5", 12, 2.5}, {"D5", 14.5, .5}, {"E5", 15, 1}};

  for (int bar = 0; bar < 14; ++bar) {
    const double b = 4.0 * bar;
    const auto& ch = chords[bar % 4];
    // E-piano: left-hand root, chord on 1 and on the "and" of 2
    s.trig(kEp, b, roots[bar % 4], 1.8, 0.75f);
    for (double at : {0.0, 1.5}) for (int note : ch) s.trig(kEp, b + sw(at), note, at == 0.0 ? 1.4 : 2.3, 0.6f + 0.1f * rnd());
    if (bar >= 2) {
      // A loose beat: kick on 1 and the "and" of 2, snare on 2 and 4, swung shaker
      for (double at : {0.0, 1.5, 2.75}) {
        if (at == 2.75 && bar % 2 == 0) continue;
        s.trig(kKick, b + sw(at), 33, 0.25, at == 0.0 ? 0.95f : 0.75f);
      }
      for (double at : {1.0, 3.0}) s.trig(kSnare, b + at, 50, 0.25, 0.85f);
      for (int st = 0; st < 16; ++st) {
        if (st == 4 || st == 12) continue;
        s.trig(kSnare, b + sw(0.25 * st), 50, 0.12, (st % 2 ? 0.35f : 0.55f) + 0.1f * rnd(), hat(0.06f));
      }
    }
    if (bar >= 4 && bar < 12) {
      const int phrase_bar = (bar - 4) % 4;
      for (const Line& l : melody) {
        if (l.at < 4.0 * phrase_bar || l.at >= 4.0 * phrase_bar + 4.0) continue;
        const double at = b + sw(l.at - 4.0 * phrase_bar);
        s.trig(kLead, at, n(l.note), l.len * 0.95, 0.85f);
        lead.bend(at, at + 0.12, -1.0f, 0.0f);   // a scoop up into each note
      }
    }
  }
  s.trig(kEp, 56.0, roots[0], 3.0, 0.8f);
  for (int note : chords[0]) s.trig(kEp, 56.0, note, 3.0, 0.65f);
  s.trig(kLead, 56.0, n("G5"), 2.5, 0.8f);
  lead.bend(56.0, 56.15, -1.0f, 0.0f);
  return s;
}

// --------------------------------------------- 3. Big chorus (122, A major) ---

Song anthem() {
  Song s("03_big_chorus.wav", 122.0, 20);
  s.tail = 4.0;
  s.delay_beats = 0.75;
  s.kick = 0;
  kick_patch(s.add("kick"), 26.0f, 0.3f);
  Track& sn = s.add("snare+hats");
  snare_patch(sn, 0.5f, 0.22f);
  sn.reverb = 0.45f;

  Track& bass = s.add("bass");
  bass.osc(0, M::kSaw, 0.6f);
  bass.osc(1, M::kSine, 0.4f, -12.0f);
  bass.set(G::kParamLadderCutoff, 56.0f);
  bass.set(G::kParamLadderResonance, 0.2f);
  bass.set(G::kParamLadderEnvAmount, 28.0f);
  bass.env(0, 0.002f, 0.15f, 0.15f, 0.1f);
  bass.env(1, 0.002f, 0.25f, 0.7f, 0.06f);
  bass.set(G::kParamLevel, 0.26f);
  bass.duck = 0.35f;

  // Supersaw: off-beat chord stabs, and the hook in octaves on top
  Track& saw = s.add("supersaw");
  saw.osc(0, M::kSupersaw, 0.45f);
  saw.set(G::osc_param(0, G::kOscPw), 0.6f);
  saw.osc(1, M::kSaw, 0.15f, -12.0f);
  saw.set(G::kParamLadderCutoff, 88.0f);
  saw.set(G::kParamLadderResonance, 0.15f);
  saw.set(G::kParamLadderEnvAmount, 20.0f);
  saw.env(0, 0.002f, 0.25f, 0.3f, 0.3f);
  saw.env(1, 0.002f, 0.4f, 0.55f, 0.3f);
  saw.set(G::kParamLevel, 0.24f);
  saw.reverb = 0.35f;
  saw.delay = 0.22f;
  saw.duck = 0.5f;
  enum { kKick, kSnare, kBass, kSaw };

  const std::vector<std::vector<int>> chords = {notes({"A3", "C#4", "E4"}), notes({"G#3", "B3", "E4"}),
                                                notes({"F#3", "A3", "C#4"}), notes({"F#3", "A3", "D4"})};
  const int roots[] = {n("A1"), n("E2"), n("F#1"), n("D2")};
  const Line hook[] = {{"E5", 0, .5},   {"E5", .5, .5},   {"C#5", 1, .5},  {"E5", 1.5, 1},  {"F#5", 2.5, .5}, {"E5", 3, 1},
                       {"B4", 4, .5},   {"B4", 4.5, .5},  {"G#4", 5, .5},  {"B4", 5.5, 1},  {"C#5", 6.5, .5}, {"B4", 7, 1},
                       {"A4", 8, .5},   {"C#5", 8.5, .5}, {"E5", 9, 1},    {"F#5", 10, 1},  {"E5", 11, 1},
                       {"D5", 12, 1.5}, {"C#5", 13.5, .5}, {"B4", 14, 1},  {"A4", 15, 1}};

  for (int bar = 0; bar < 20; ++bar) {
    const double b = 4.0 * bar;
    const auto& ch = chords[bar % 4];
    const bool full = bar >= 4, chorus = bar >= 12;
    for (int q = 0; q < 4; ++q) {
      if (bar == 11 && q >= 2) continue;   // the pause before the chorus
      s.trig(kKick, b + q, 33, 0.25, 0.95f);
      if (full && (q == 1 || q == 3)) s.trig(kSnare, b + q, 50, 0.3, 1.0f);
      if (full) {
        for (int st = 0; st < 4; ++st) {
          if ((q == 1 || q == 3) && st == 0) continue;
          s.trig(kSnare, b + q + 0.25 * st, 50, 0.1, st == 2 ? 0.75f : 0.4f, hat());
        }
      }
    }
    if (bar == 11) {   // a snare roll into the chorus
      for (int k = 0; k < 8; ++k) s.trig(kSnare, b + 2.0 + 0.25 * k, 50, 0.2, 0.5f + 0.06f * k);
    }
    if (full && bar != 11) {
      for (int e = 0; e < 8; ++e) s.trig(kBass, b + 0.5 * e, roots[bar % 4] + (e == 7 ? 12 : 0), 0.42, e % 2 ? 0.75f : 0.95f);
    }
    // Stabs on the off-beats; the hook (with its octave) in the chorus
    for (double at : {0.5, 1.5, 2.5, 3.5}) {
      if (bar == 11 && at > 2.0) continue;
      for (int note : ch) s.trig(kSaw, b + at, note, 0.3, chorus ? 0.6f : 0.75f);
    }
    if (chorus) {
      for (const Line& l : hook) {
        const double lo = 4.0 * (bar % 4);
        if (l.at < lo || l.at >= lo + 4.0) continue;
        s.trig(kSaw, b + l.at - lo, n(l.note), l.len * 0.9, 0.9f);
        s.trig(kSaw, b + l.at - lo, n(l.note) + 12, l.len * 0.9, 0.6f);
      }
    }
  }
  s.trig(kKick, 80.0, 33, 0.25, 1.0f);
  s.trig(kBass, 80.0, roots[0], 2.0, 1.0f);
  for (int note : chords[0]) s.trig(kSaw, 80.0, note, 2.5, 0.8f);
  s.trig(kSaw, 80.0, n("A5"), 2.5, 0.7f);
  return s;
}

}  // namespace

int main(int argc, char** argv) {
  const std::string dir = argc > 1 ? argv[1] : ".";
  const std::string load_dir = argc > 3 && std::string(argv[2]) == "--presets" ? argv[3] : "";
  const std::string preset_dir = dir + "/presets";

  Song (*const songs[])() = {night_drive, bedroom, anthem};
  for (auto make : songs) {
    const uint32_t seed = g_rand;
    {
      Song s = make();
      if (!load_dir.empty()) import_presets(s, load_dir);
      export_presets(s, preset_dir);
    }
    g_rand = seed;
    Song s = make();
    if (!load_dir.empty()) import_presets(s, load_dir);
    render(s, dir);
  }
  return 0;
}
