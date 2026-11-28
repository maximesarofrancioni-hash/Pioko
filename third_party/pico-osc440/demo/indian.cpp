// Indian classical style test pieces: original phrases in traditional ragas
// (scales and phrase rules, not anyone's composition), four engines each as
// four voice modules. Every sound is TripleWaveGroup: tanpura, bansuri, sitar,
// santoor, shruti box and both tabla drums. Meend (glides), kan (grace notes)
// and andolan (slow oscillation) are pitch-bend automation on the oscillators'
// tune, sent through set_param like any other change.
//
//   indian <out_dir> [--presets <dir>]
//
// Sa (the tonic) is D throughout. Writes presets like the showcase.

#include "song.h"

namespace {

using namespace demo;

// ---------------------------------------------------------------- notation ---

constexpr int kSa = 62;   // D4

// Raga degrees as semitones above Sa: "S r R g G M m P d D n N", "." below for
// the lower octave and "'" for the upper ("N." is Ni below Sa, "S'" upper Sa).
// m = tivra (sharp) Ma, r/g/d/n = komal (flat)
int swar(const char* s, int sa = kSa) {
  int semis = 0;
  switch (s[0]) {
    case 'S': semis = 0; break;
    case 'r': semis = 1; break;
    case 'R': semis = 2; break;
    case 'g': semis = 3; break;
    case 'G': semis = 4; break;
    case 'M': semis = 5; break;
    case 'm': semis = 6; break;
    case 'P': semis = 7; break;
    case 'd': semis = 8; break;
    case 'D': semis = 9; break;
    case 'n': semis = 10; break;
    case 'N': semis = 11; break;
    default: break;
  }
  for (const char* c = s + 1; *c; ++c) semis += *c == '.' ? -12 : *c == '\'' ? 12 : 0;
  return sa + semis;
}

double beats_of(const Song& s, double seconds) { return seconds * s.bpm / 60.0; }

// --------------------------------------------------------------- the sounds ---

void tanpura_patch(Track& t) {
  // Bright and buzzy (the jivari), slowly shimmering: a Plaits wave whose position
  // drifts, with a saw and an octave-up saw under a gentle ladder
  t.osc(0, M::kWavetable, 0.5f);
  t.set(G::osc_param(0, G::kOscTable), 1.0f);
  t.set(G::osc_param(0, G::kOscWave), 14.0f);
  t.set(G::osc_param(0, G::kOscWaveEnvAmount), 10.0f);
  t.osc(1, M::kSaw, 0.25f);
  t.osc(2, M::kSaw, 0.1f, 12.0f);
  t.set(G::osc_param(2, G::kOscFine), 3.0f);
  t.set(G::wave_env_param(0, 0, false), 1.0f);
  t.set(G::wave_env_param(0, 0, true), 2.5f);
  t.lfo(1, 0.3f, Shape::kSmoothRandom);
  t.slot(0, mod::kSrcLfo2, mod::kDstWave1, 6.0f);
  t.set(G::kParamLadderCutoff, 90.0f);
  t.set(G::kParamLadderResonance, 0.15f);
  t.set(G::kParamLadderEnvAmount, 12.0f);
  t.env(0, 0.005f, 1.5f, 0.3f, 2.0f);
  t.env(1, 0.004f, 5.0f, 0.0f, 4.0f);
  t.set(G::kParamLevel, 0.28f);
  t.reverb = 0.3f;
}

// The four strings, Pa Sa Sa Sa (low), around and around at its own pace
void tanpura_cycle(Song& s, int tr, double from_s, double to_s, double every_s = 1.15) {
  const int strings[] = {swar("P..") , swar("S."), swar("S."), swar("S..")};
  const float vel[] = {0.7f, 0.6f, 0.6f, 0.85f};
  int k = 0;
  for (double t = from_s; t < to_s; t += every_s, ++k) {
    s.trig(tr, beats_of(s, t), strings[k % 4], beats_of(s, 0.9), vel[k % 4] * (0.9f + 0.1f * rnd()));
  }
}

void bansuri_patch(Track& t) {
  // A sine with a little octave triangle, breath noise, and delayed vibrato
  t.osc(0, M::kSine, 0.7f);
  t.osc(1, M::kTriangle, 0.08f, 12.0f);
  t.osc(2, M::kColoredNoise, 0.05f);
  t.set(G::osc_param(2, G::kOscPw), 0.6f);
  t.set(G::kParamLadderCutoff, 104.0f);
  t.set(G::kParamLadderEnvAmount, 10.0f);
  t.env(0, 0.05f, 0.4f, 0.5f, 0.3f);
  t.env(1, 0.09f, 0.3f, 0.85f, 0.35f);
  t.lfo(0, 5.2f, Shape::kSine);
  t.set(G::lfo_param(0, G::kLfoDelay), 0.6f);
  t.slot(0, mod::kSrcLfo1, mod::kDstPitch1, 0.16f);
  t.slot(1, mod::kSrcLfo1, mod::kDstPitch2, 0.16f);
  t.lfo(1, 3.0f, Shape::kSmoothRandom);
  t.slot(2, mod::kSrcLfo2, mod::kDstLevel3, 0.03f);   // the breath wavers
  t.set(G::kParamLevel, 0.18f);
  t.set(G::kParamPan, 0.1f);
  t.reverb = 0.4f;
  t.delay = 0.15f;
}

void shruti_patch(Track& t) {
  // A reed drone: pulses and a detuned saw, the bellows breathing in the level
  t.osc(0, M::kPulse, 0.4f);
  t.set(G::osc_param(0, G::kOscPw), 0.35f);
  t.osc(1, M::kSaw, 0.3f);
  t.set(G::osc_param(1, G::kOscFine), 6.0f);
  t.osc(2, M::kPulse, 0.2f, -12.0f);
  t.set(G::kParamLadderCutoff, 72.0f);
  t.set(G::kParamLadderResonance, 0.1f);
  t.env(1, 1.5f, 1.0f, 0.9f, 2.5f);
  t.lfo(0, 0.25f, Shape::kSine);
  t.slot(0, mod::kSrcLfo1, mod::kDstAmp, 0.008f);
  t.slot(1, mod::kSrcLfo1, mod::kDstCutoff2, 3.0f);
  t.set(G::kParamLevel, 0.07f);
  t.reverb = 0.35f;
}

void swarmandal_patch(Track& t) {
  // A bright zither shimmer: FM, the modulator an octave up
  t.osc(0, M::kSine, 0.0f, 12.0f);
  t.osc(1, M::kSine, 0.7f);
  t.set(G::osc_param(1, G::kOscFm), 0.5f);
  t.osc(2, M::kSine, 0.1f, 24.0f);
  t.set(G::kParamLadderCutoff, 120.0f);
  t.env(1, 0.001f, 2.2f, 0.0f, 1.5f);
  t.set(G::kParamLevel, 0.1f);
  t.set(G::kParamPan, -0.3f);
  t.reverb = 0.55f;
  t.delay = 0.25f;
}

void sitar_patch(Track& t) {
  // Buzzy pluck: a bright Plaits wave, a saw and a thin octave pulse (the jawari
  // edge), the ladder snapping open on each stroke
  t.osc(0, M::kWavetable, 0.45f);
  t.set(G::osc_param(0, G::kOscTable), 1.0f);
  t.set(G::osc_param(0, G::kOscWave), 30.0f);
  t.osc(1, M::kSaw, 0.35f);
  t.osc(2, M::kPulse, 0.12f, 12.0f);
  t.set(G::osc_param(2, G::kOscPw), 0.12f);
  t.set(G::kParamLadderCutoff, 68.0f);
  t.set(G::kParamLadderResonance, 0.35f);
  t.set(G::kParamLadderEnvAmount, 46.0f);
  t.set(G::kParamDrive, 1.8f);
  t.env(0, 0.001f, 0.25f, 0.15f, 0.3f);
  t.env(1, 0.001f, 1.6f, 0.0f, 0.6f);
  t.lfo(0, 5.5f, Shape::kSine);
  t.set(G::lfo_param(0, G::kLfoDelay), 0.4f);
  t.slot(0, mod::kSrcLfo1, mod::kDstPitch1, 0.08f);
  t.slot(1, mod::kSrcLfo1, mod::kDstPitch2, 0.08f);
  t.slot(2, mod::kSrcLfo1, mod::kDstPitch3, 0.08f);
  t.set(G::kParamLevel, 0.38f);
  t.set(G::kParamPan, 0.15f);
  t.reverb = 0.3f;
  t.delay = 0.18f;
}

void santoor_patch(Track& t) {
  // Hammered strings: FM with the modulator an octave up, a short bright decay
  t.osc(0, M::kSine, 0.0f, 12.0f);
  t.osc(1, M::kSine, 0.6f);
  t.set(G::osc_param(1, G::kOscFm), 0.35f);
  t.osc(2, M::kTriangle, 0.15f, 12.0f);
  t.set(G::osc_param(2, G::kOscFine), -4.0f);
  t.set(G::kParamLadderCutoff, 108.0f);
  t.set(G::kParamLadderEnvAmount, 10.0f);
  t.env(0, 0.001f, 0.3f, 0.2f, 0.4f);
  t.env(1, 0.001f, 1.2f, 0.0f, 0.9f);
  t.set(G::kParamLevel, 0.2f);
  t.set(G::kParamPan, -0.2f);
  t.reverb = 0.35f;
  t.delay = 0.2f;
}

void bass_patch(Track& t) {
  t.osc(0, M::kSine, 0.8f);
  t.osc(1, M::kTriangle, 0.3f, 12.0f);
  t.set(G::kParamLadderCutoff, 62.0f);
  t.set(G::kParamLadderEnvAmount, 14.0f);
  t.env(0, 0.002f, 0.2f, 0.0f, 0.1f);
  t.env(1, 0.008f, 0.6f, 0.6f, 0.2f);
  t.set(G::kParamLevel, 0.24f);
  t.duck = 0.25f;
}

// Tabla. The dayan (right, small drum) is tuned to Sa: a ringing fundamental with
// its second harmonic and a finger click. The bayan (left, bass drum) swoops up in
// pitch as the wrist presses the head (the "ge").
void dayan_patch(Track& t) {
  t.osc(0, M::kSine, 0.7f);
  t.osc(1, M::kSine, 0.3f, 12.0f);
  t.osc(2, M::kNoise, 0.0f);
  t.set(G::kParamKeyTrack, 0.0f);
  t.env(2, 0.0005f, 0.02f, 0.0f, 0.02f);
  t.slot(0, mod::kSrcEnv3, mod::kDstLevel3, 0.5f);     // the click
  t.slot(1, mod::kSrcEnv3, mod::kDstPitch1, 0.6f);     // struck slightly sharp
  t.slot(2, mod::kSrcEnv3, mod::kDstPitch2, 0.6f);
  t.set(G::kParamLadderCutoff, 120.0f);
  t.env(1, 0.0005f, 0.5f, 0.0f, 0.25f);
  t.set(G::kParamLevel, 0.32f);
  t.set(G::kParamPan, 0.2f);
  t.reverb = 0.18f;
}

void bayan_patch(Track& t) {
  t.osc(0, M::kSine, 0.9f);
  t.osc(1, M::kTriangle, 0.15f);
  t.osc(2, M::kNoise, 0.0f);
  t.set(G::kParamKeyTrack, 0.0f);
  t.env(2, 0.001f, 0.35f, 0.0f, 0.2f);
  t.slot(0, mod::kSrcEnv3, mod::kDstPitch1, -4.0f);    // starts low, rises: the ge
  t.slot(1, mod::kSrcEnv3, mod::kDstPitch2, -4.0f);
  t.env(1, 0.001f, 0.9f, 0.0f, 0.3f);
  t.set(G::kParamLadderCutoff, 88.0f);
  t.set(G::kParamDrive, 1.6f);
  t.set(G::kParamLevel, 0.3f);
  t.set(G::kParamPan, -0.15f);
  t.reverb = 0.12f;
}

// Strokes as parameter locks on the two drum tracks
const Locks kTin = {{G::osc_param(1, G::kOscLevel), 0.08f}, {G::env_param(1, G::kEnvDecay), 0.9f}};
const Locks kTa = {{G::osc_param(1, G::kOscLevel), 0.45f}, {G::env_param(1, G::kEnvDecay), 0.35f}};
const Locks kTe = {{G::osc_param(0, G::kOscLevel), 0.3f}, {G::env_param(1, G::kEnvDecay), 0.05f},
                   {G::env_param(1, G::kEnvRelease), 0.04f}, {G::matrix_param(0, 2), 0.9f}};
const Locks kKa = {{G::env_param(1, G::kEnvDecay), 0.06f}, {G::env_param(1, G::kEnvRelease), 0.05f},
                   {G::osc_param(2, G::kOscLevel), 0.3f}};

struct Tabla { int dayan, bayan; int sa_d, sa_b; };

// One bol (syllable) at a beat
void bol(Song& s, const Tabla& t, double at, const std::string& b, float v = 0.9f) {
  auto na = [&](const Locks& l, float vel) { s.trig(t.dayan, at, t.sa_d, 0.4, vel, l); };
  auto ge = [&](float vel) { s.trig(t.bayan, at, t.sa_b, 0.5, vel); };
  if (b == "Dha") { na({}, v); ge(v); }
  else if (b == "Dhin") { na(kTin, v * 0.9f); ge(v * 0.85f); }
  else if (b == "Dhi") { na(kTin, v * 0.8f); ge(v * 0.7f); }
  else if (b == "Na") na({}, v);
  else if (b == "Tin") na(kTin, v * 0.85f);
  else if (b == "Ta") na(kTa, v);
  else if (b == "Te" || b == "Ti") na(kTe, v * 0.6f);
  else if (b == "Ge") ge(v);
  else if (b == "Ka" || b == "Ke") s.trig(t.bayan, at, t.sa_b, 0.1, v * 0.7f, kKa);
}

// ------------------------------------------- 1. Raga Bhairav, alap (free time) ---

Song alap() {
  Song s("01_raga_bhairav_alap.wav", 60.0, 11);   // 60 bpm: one beat = one second
  s.tail = 6.0;
  s.delay_beats = 0.6;
  tanpura_patch(s.add("tanpura"));
  shruti_patch(s.add("shruti box"));
  bansuri_patch(s.add("bansuri"));
  swarmandal_patch(s.add("swarmandal"));
  enum { kTanpura, kShruti, kBansuri, kSwar };

  tanpura_cycle(s, kTanpura, 0.0, 44.0);
  for (const char* d : {"S.", "P.", "S"}) s.trig(kShruti, 0.5, swar(d), 43.0, 0.7f);

  // Swarmandal: Bhairav's notes swept up, at the start and the turn of the alap
  const char* sweep[] = {"S", "r", "G", "M", "P", "d", "N", "S'", "r'", "G'"};
  for (double at : {1.0, 26.0}) {
    for (int i = 0; i < 10; ++i) s.trig(kSwar, at + 0.07 * i, swar(sweep[i]), 0.5, 0.5f + 0.04f * i);
  }

  // The bansuri: (time s, swar, length s); bends added per phrase below
  struct Note { double t; const char* swar; double len; };
  const Note line[] = {
      {4.0, "S", 3.0},   {7.2, "r", 2.4},   {9.8, "S", 1.2},   {11.2, "N.", 1.0}, {12.4, "S", 1.6},
      {14.2, "G", 1.0},  {15.3, "M", 1.0},  {16.4, "d", 2.6},  {19.2, "P", 1.8},  {21.2, "G", 0.8},
      {22.1, "M", 0.8},  {23.0, "r", 2.2},  {25.4, "S", 2.0},  {27.6, "M", 0.7},  {28.4, "P", 0.7},
      {29.2, "d", 0.9},  {30.2, "N", 0.9},  {31.2, "S'", 3.2}, {34.6, "d", 2.0},  {36.8, "P", 1.0},
      {37.9, "G", 0.8},  {38.8, "M", 0.8},  {39.7, "r", 1.6},  {41.4, "S", 3.6}};
  Track& flute = *s.tracks[kBansuri];
  for (const Note& x : line) {
    s.trig(kBansuri, x.t, swar(x.swar), x.len, 0.75f);
    flute.bend(x.t, x.t, 0.0f, 0.0f);   // each note starts on its pitch
  }
  // Andolan: komal Re and komal Dha sway slowly towards the note below
  auto andolan = [&](double from, double len) {
    for (double t = from + 0.4; t + 0.6 < from + len; t += 0.6) {
      flute.bend(t, t + 0.3, 0.0f, -0.35f);
      flute.bend(t + 0.3, t + 0.6, -0.35f, 0.0f);
    }
  };
  andolan(7.2, 1.8);
  andolan(16.4, 2.6);
  andolan(23.0, 1.6);
  andolan(34.6, 2.0);
  // Meend: komal Re slides down into Sa; Ni glides up into upper Sa; a kan (grace)
  // from Re' into Sa'
  flute.bend(9.2, 9.75, 0.0f, -1.0f);
  flute.bend(11.9, 12.35, 0.0f, 1.0f);
  flute.bend(31.2, 31.3, 2.0f, 0.0f);
  flute.bend(41.0, 41.35, 0.0f, -1.0f);
  return s;
}

// ------------------------------- 2. Raga Yaman, gat in teentaal (16 beats) ---

Song gat() {
  Song s("02_raga_yaman_gat_teentaal.wav", 96.0, 22);
  s.tail = 5.0;
  s.delay_beats = 0.75;
  tanpura_patch(s.add("tanpura"));
  sitar_patch(s.add("sitar"));
  dayan_patch(s.add("dayan"));
  bayan_patch(s.add("bayan"));
  enum { kTanpura, kSitar, kDayan, kBayan };
  const Tabla tabla{kDayan, kBayan, kSa, kSa - 24};
  Track& sitar = *s.tracks[kSitar];

  const double intro = 8.0;   // beats: tanpura and a short sitar alap first
  tanpura_cycle(s, kTanpura, 0.0, 60.0 * 88 / 96.0);

  auto play = [&](double at, const char* sw, double len, float vel = 0.85f) {
    s.trig(kSitar, at, swar(sw, kSa - 12), len, vel);
    sitar.bend(at, at, 0.0f, 0.0f);
  };
  // A few alap strokes over the drone, with meend
  play(1.0, "N.", 2.0);
  sitar.bend(1.6, 2.4, 0.0f, 1.0f);   // Ni slides up to Sa
  play(3.5, "G", 1.5);
  play(5.0, "R", 1.0);
  play(6.0, "S", 2.0, 0.9f);

  // The gat: (beat, swar, length, meend semitones from the note at half its length)
  struct G16 { double at; const char* sw; double len; float meend; };
  const G16 gat16[] = {
      {0, "G", .5, 0},  {.5, "R", .5, 0},  {1, "S", 1, 0},    {2, "N.", .5, 0}, {2.5, "R", .5, 0},
      {3, "G", 1, 0},   {4, "m", .5, 0},   {4.5, "G", .5, 0}, {5, "R", .5, 0},  {5.5, "G", .5, 0},
      {6, "m", .5, 0},  {6.5, "D", .5, 0}, {7, "D", 1, 2},    {8, "S'", 1.5, 0}, {9.5, "N", .5, 0},
      {10, "D", .5, 0}, {10.5, "P", .5, 0}, {11, "P", 1, -1}, {12, "G", .5, 0}, {12.5, "m", .5, 0},
      {13, "D", 1, 0},  {14, "N", .5, 0},  {14.5, "D", .5, 0}, {15, "P", .5, 0}, {15.5, "m", .5, 0}};
  auto play_gat = [&](double c, int first, int last) {
    for (int i = first; i < last; ++i) {
      const G16& g = gat16[i];
      play(c + g.at, g.sw, g.len * 0.95, g.at == 0 ? 0.95f : 0.8f);
      if (g.meend != 0) sitar.bend(c + g.at + g.len * 0.4, c + g.at + g.len * 0.85, 0.0f, g.meend);
    }
  };
  // A taan: fast sixteenths up and down the raga
  const char* taan[] = {"N.", "R", "G", "m", "D", "N", "S'", "R'", "G'", "R'", "S'", "N", "D", "P", "m", "G",
                        "m", "D", "N", "S'", "N", "D", "P", "m", "G", "m", "G", "R", "S", "N.", "R", "S"};

  const char* theka[16] = {"Dha", "Dhin", "Dhin", "Dha", "Dha", "Dhin", "Dhin", "Dha",
                           "Dha", "Tin", "Tin", "Ta", "Ta", "Dhin", "Dhin", "Dha"};
  const double cycles = 5;
  for (int c = 0; c < cycles; ++c) {
    const double b0 = intro + 16.0 * c;
    for (int m = 0; m < 16; ++m) {
      const bool sam = m == 0, khali = m == 8;
      bol(s, tabla, b0 + m, theka[m], sam ? 1.0f : khali ? 0.75f : 0.85f);
      if (c >= 1 && m % 4 == 3) bol(s, tabla, b0 + m + 0.5, "Te", 0.7f);   // fillers
      if (c >= 2 && m % 2 == 1) bol(s, tabla, b0 + m + 0.75, "Ka", 0.6f);
    }
    if (c == 0 || c == 1) play_gat(b0, 0, 25);
    if (c == 2) {
      for (int i = 0; i < 32; ++i) play(b0 + 0.25 * i, taan[i], 0.24, i % 4 == 0 ? 0.9f : 0.7f);
      play_gat(b0, 13, 25);   // back into the gat's second half
    }
    if (c == 3) play_gat(b0, 0, 25);
    if (c == 4) {
      play_gat(b0, 0, 13);
      // Tihai: the phrase "Ta Te Te Dha" three times, the last Dha on sam
      const double sam = b0 + 16.0;
      for (int k = 0; k < 3; ++k) {
        const double p = sam - 5.0 + 2.0 * k;
        bol(s, tabla, p, "Ta", 0.9f);
        bol(s, tabla, p + 0.5, "Te", 0.7f);
        bol(s, tabla, p + 0.75, "Te", 0.7f);
        bol(s, tabla, p + 1.0, "Dha", 1.0f);
        play(p, "G", 0.5);
        play(p + 0.5, "R", 0.25);
        play(p + 0.75, "N.", 0.25);
        play(p + 1.0, "S", 0.9, 0.95f);
      }
    }
  }
  // Sam: the last stroke and a ringing Sa
  const double end = intro + 16.0 * cycles;
  bol(s, tabla, end, "Dha", 1.0f);
  play(end, "S", 3.0, 1.0f);
  play(end, "S'", 3.0, 0.6f);   // and the chikari's upper Sa
  s.beats = end + 2.0;
  return s;
}

// --------------------------- 3. Raga Bhupali, santoor over keherwa (fusion) ---

Song fusion() {
  Song s("03_raga_bhupali_keherwa_fusion.wav", 104.0, 16);
  s.tail = 4.0;
  s.delay_beats = 0.75;
  s.kick = 1;   // the bayan's strokes duck the bass a little
  dayan_patch(s.add("dayan"));
  bayan_patch(s.add("bayan"));
  santoor_patch(s.add("santoor"));
  bass_patch(s.add("bass"));
  enum { kDayan, kBayan, kSantoor, kBass };
  const Tabla tabla{kDayan, kBayan, kSa, kSa - 24};

  // Keherwa, 8 matras in eighth notes: Dha Ge Na Ti | Na Ka Dhi Na
  const char* keherwa[8] = {"Dha", "Ge", "Na", "Ti", "Na", "Ka", "Dhi", "Na"};
  // Bhupali: S R G P D (pentatonic). The santoor plays it in rolling sixteenths,
  // often double-struck as santoor players do
  const char* scale[] = {"S", "R", "G", "P", "D", "S'", "R'", "G'"};
  const int phrase[16] = {0, 2, 4, 5, 4, 2, 3, 2, 1, 3, 5, 6, 5, 3, 4, 3};
  const int shift[4] = {0, 1, -1, 2};   // the phrase starts higher or lower each bar
  const int roots[4] = {0, 0, 7, 9};    // bass: Sa, Sa, Pa, Dha

  for (int bar = 0; bar < 16; ++bar) {
    const double b0 = 4.0 * bar;
    if (bar >= 2) {
      for (int m = 0; m < 8; ++m) {
        const float v = m == 0 ? 1.0f : (m == 4 ? 0.8f : 0.7f);
        bol(s, tabla, b0 + 0.5 * m, keherwa[m], v);
        if (bar % 4 == 3 && m >= 6) bol(s, tabla, b0 + 0.5 * m + 0.25, "Te", 0.6f);   // a fill
      }
    }
    if (bar >= 4) {
      const int r = roots[bar % 4];
      s.trig(kBass, b0, kSa - 24 + r, 1.4, 0.9f);
      s.trig(kBass, b0 + 1.5, kSa - 24 + r, 0.4, 0.6f);
      s.trig(kBass, b0 + 2.5, kSa - 12 + r, 0.4, 0.7f);
      s.trig(kBass, b0 + 3.0, kSa - 24 + r, 0.9, 0.8f);
    }
    for (int st = 0; st < 16; ++st) {
      if (bar < 1 && st < 8) continue;
      int i = phrase[st] + shift[bar % 4];
      i = std::clamp(i, 0, 7);
      const float v = (st % 4 == 0 ? 0.85f : 0.6f) * (0.85f + 0.15f * rnd());
      s.trig(kSantoor, b0 + 0.25 * st, swar(scale[i]), 0.22, v);
      if (bar >= 8 && st % 4 == 2 && rnd() < 0.5f) {   // the double strike
        s.trig(kSantoor, b0 + 0.25 * st + 0.125, swar(scale[i]), 0.1, v * 0.6f);
      }
    }
  }
  // Ending on sam: Dha, a low Sa, the santoor rolling up to Sa
  bol(s, tabla, 64.0, "Dha", 1.0f);
  s.trig(kBass, 64.0, kSa - 24, 2.0, 1.0f);
  for (int i = 0; i < 6; ++i) s.trig(kSantoor, 64.0 + 0.06 * i, swar(scale[i]), 1.5, 0.7f);
  return s;
}

}  // namespace

int main(int argc, char** argv) {
  const std::string dir = argc > 1 ? argv[1] : ".";
  const std::string load_dir = argc > 3 && std::string(argv[2]) == "--presets" ? argv[3] : "";
  const std::string preset_dir = dir + "/presets";

  Song (*const songs[])() = {alap, gat, fusion};
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
