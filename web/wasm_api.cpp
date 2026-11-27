// C API for the browser build of filter-lab
#include "../src/filterlab.h"
#include "wave/wave_lane.h"
#include "mods/mods.h"
#include <cmath>

static FilterLab lab;
static WaveLane wave;
static float wbuf[512];
static float waveL[FilterLab::kBlock], waveR[FilterLab::kBlock];   // the wave lane alone, for its spectrum
static int lastStep = -1, lastBar = -1;
static float sampleRate = 44100.f;
static mods::Shift shiftReg;
static mods::MarblesState marbles;
static mods::TidesState tides;
static mods::BeadsState beads;
static float modInfo[40];
static unsigned lastNoteOns = 0;
static float applied[16];          // Tides offset applied per destination last block
static float outL[FilterLab::kBlock], outR[FilterLab::kBlock];

#define API extern "C" __attribute__((used, visibility("default")))

API void fl_init(float sr)
{
    sampleRate = sr;
    lab.Init(sr); lab.barsPerFilter = 4; wave.Init(); lastStep = -1; lastBar = -1;
    mods::MarblesInit(sr / FilterLab::kBlock);
    mods::TidesInit();
    mods::BeadsInit();
}

// Tides destinations: osc440 parameter id (or -1 for the others) and the range of a full-scale output
struct Dest { int id; float range; };
static const Dest kDest[] = {
    {-1, 0.f},                          // 0 off
    {105, 32.f}, {121, 32.f}, {137, 32.f},   // 1-3 osc 1-3 wave position
    {7, 36.f}, {2, 36.f}, {4, 0.5f},    // 4 ladder cutoff, 5 SVF cutoff, 6 SVF morph
    {107, 0.45f},                       // 7 osc 1 pulse width
    {-1, 36.f},                         // 8 filter lab cutoff (semitones)
    {-1, 0.5f}, {-1, 0.5f}, {-1, 12.f}, {-1, 0.5f},    // 9-12 Beads time, size, pitch (semitones), density
};
static const int kNumDest = sizeof(kDest) / sizeof(kDest[0]);
API void fl_render()
{
    lab.Render(outL, outR);
    bool tick = false;
    // the wave lane follows filter-lab's sequencer: each new 16th triggers it
    int st = lab.Step(), bar = lab.Bar();
    if (st != lastStep || bar != lastBar)
    {
        lastStep = st; lastBar = bar;
        double sp16 = sampleRate * 60.0 / lab.bpm / 4.0;
        int step = (st + 15) % 16;
        wave.Tick(step, bar, sp16);
        if (wave.mode == WaveLane::kShift)
        {
            // the register advances on every 16th; the gate mode decides which steps play
            bool bit0 = false;
            int n = shiftReg.Step(bit0);
            bool play = shiftReg.gateMode == 0 ? wave.Hit(step) : (shiftReg.gateMode == 1 ? bit0 : true);
            if (play)
                wave.ExternalNote(45 + 12 * wave.octave + n - 12, step % 4 == 0 ? 0.95f : 0.8f, wave.gate * sp16);
        }
        tick = true;
    }
    // Marbles, every block, clocked by the 16ths
    mods::MarblesStep(marbles, tick);
    if (wave.mode == WaveLane::kMarbles)
    {
        double sp16 = sampleRate * 60.0 / lab.bpm / 4.0;
        for (int c = 0; c < 3; c += 2)          // t1 plays X1, t3 plays X3
            if (marbles.rise[c])
                wave.ExternalNote(45 + 12 * wave.octave + (int)std::lround(marbles.x[c] * 12.f), c ? 0.75f : 0.9f, wave.gate * sp16);
    }
    // Tides, every block: the wave lane's notes are its gate (for AD / AR)
    mods::TidesStep(tides, sampleRate / FilterLab::kBlock, wave.notesHeld > 0);
    float sum[16] = {0};
    for (int i = 0; i < 4; ++i)
        if (tides.dest[i] > 0 && tides.dest[i] < kNumDest)
            sum[tides.dest[i]] += tides.out[i] * tides.amount[i] * kDest[tides.dest[i]].range;
    for (int d = 1; d < kNumDest; ++d)
        if (kDest[d].id >= 0 && std::fabs(sum[d] - applied[d]) > 1e-4f)
            wave.ModParam((uint16_t)kDest[d].id, sum[d]);
    for (int d = 0; d < kNumDest; ++d) applied[d] = sum[d];
    lab.cutoffMod = sum[8];
    for (int i = 0; i < FilterLab::kBlock; ++i) waveL[i] = waveR[i] = 0.f;
    wave.Render(waveL, waveR, FilterLab::kBlock);
    for (int i = 0; i < FilterLab::kBlock; ++i) { outL[i] += waveL[i]; outR[i] += waveR[i]; }
    // Beads on the mix, before the Vinyl Sim (which runs in the worklet)
    if (beads.on)
    {
        mods::BeadsState b = beads;
        auto cl = [](float v) { return v < 0.f ? 0.f : (v > 1.f ? 1.f : v); };
        b.time = cl(b.time + sum[9]); b.size = cl(b.size + sum[10]);
        b.pitch = std::fmax(-24.f, std::fmin(24.f, b.pitch + sum[11])); b.density = cl(b.density + sum[12]);
        bool seed = beads.seed == 1 ? wave.noteOns != lastNoteOns : (beads.seed == 2 ? tick : false);
        mods::BeadsProcess(b, outL, outR, FilterLab::kBlock, seed);
        beads.inLevel = b.inLevel; beads.outLevel = b.outLevel;
    }
    lastNoteOns = wave.noteOns;
    for (int i = 0; i < FilterLab::kBlock; ++i)   // soft limit above 0.8 so the sum stays inside +-1
    {
        float *p[2] = {&outL[i], &outR[i]};
        for (float *x : p)
        {
            float a = std::fabs(*x);
            if (a > 0.8f) *x = std::copysign(0.8f + 0.2f * std::tanh((a - 0.8f) * 5.f), *x);
        }
    }
}
API float *fl_left() { return outL; }
API float *fl_right() { return outR; }
API int fl_block() { return FilterLab::kBlock; }

API int fl_num_filters() { return lab.NumFilters(); }
API const char *fl_filter_name(int i) { return lab.FilterNameAt(i); }
API int fl_filter() { return lab.FilterIndex(); }
API void fl_select(int i) { lab.SelectFilter(i); }
API int fl_changed() { int c = lab.filterChanged; lab.filterChanged = false; return c; }

API void fl_set(int id, float v)
{
    switch (id)
    {
    case 0: lab.resonance = v; break;
    case 1: lab.cutoffOffset = v; break;
    case 2: lab.envAmount = v; break;
    case 3: lab.drive = v; break;
    case 4: lab.bpm = v; break;
    case 5: lab.sweep = v > 0.5f; break;
    case 6: lab.barsPerFilter = (int)v; break;
    case 10: case 11: case 12: case 13: lab.mute[id - 10] = v > 0.5f; break;
    }
}
API void fl_euclid(int lane, int steps, int hits, int rot) { lab.SetEuclid(lane, steps, hits, rot); }
API int fl_step() { return lab.Step(); }
API int fl_bar() { return lab.Bar(); }
API float fl_cutoff() { return lab.CutoffHz(); }

// ---- osc440 wave lane
API float *fl_wbuf() { return wbuf; }
API void fl_wave_preset(int count) { wave.LoadPreset(wbuf, count > 256 ? 256 : count); }
API void fl_wave_set(int id, float v) { wave.SetParam((uint16_t)id, v); }
API float fl_wave_get(int id) { return wave.Param((uint16_t)id); }
API void fl_wave_cfg(int k, float v)
{
    switch (k)
    {
    case 0: wave.mode = (int)v; break;
    case 1: wave.octave = (int)v; break;
    case 2: wave.gate = v; break;
    case 3: wave.volume = std::pow(10.f, v / 20.f); break;
    case 4: wave.mute = v > 0.5f; if (wave.mute) wave.AllNotesOff(); break;
    }
}
API void fl_wave_euclid(int n, int k, int r) { wave.steps = n; wave.hits = k; wave.rotate = r; }
API float *fl_wave_points(int table, float pos) { return (float *)wave.WavePoints(table, pos); }
API int fl_wave_chord() { return wave.lastChord; }
API int fl_wave_held() { return wave.notesHeld; }
API float *fl_wave_live() { return (float *)wave.Live(); }
API float *fl_wleft() { return waveL; }
API float *fl_wright() { return waveR; }

// ---- shift register, Marbles, Tides, Beads
// group: 0 shift, 1 marbles, 2 tides, 3 beads, 4 tides routing (key = output, value = dest), 5 tides amount
API void fl_mod_set(int group, int key, float v)
{
    switch (group)
    {
    case 0:
        switch (key) { case 0: shiftReg.length = (int)v; break; case 1: shiftReg.change = v; break;
                       case 2: shiftReg.range = (int)v; break; case 3: shiftReg.scale = (int)v; break;
                       case 4: shiftReg.gateMode = (int)v; break; }
        break;
    case 1:
        switch (key) { case 0: marbles.model = (int)v; break; case 1: marbles.rateIndex = (int)v; break;
                       case 2: marbles.bias = v; break; case 3: marbles.jitter = v; break; case 4: marbles.dejaVu = v; break;
                       case 5: marbles.length = (int)v; break; case 6: marbles.spread = v; break; case 7: marbles.xbias = v; break;
                       case 8: marbles.steps = v; break; case 9: marbles.scale = (int)v; break; case 10: marbles.range = (int)v; break; }
        break;
    case 2:
        switch (key) { case 0: tides.rateHz = v; break; case 1: tides.shape = v; break; case 2: tides.slope = v; break;
                       case 3: tides.smooth = v; break; case 4: tides.shift = v; break; case 5: tides.rampMode = (int)v; break;
                       case 6: tides.outputMode = (int)v; break; }
        break;
    case 3:
        switch (key) { case 0: beads.on = v > 0.5f; break; case 1: beads.time = v; break; case 2: beads.size = v; break;
                       case 3: beads.shape = v; break; case 4: beads.pitch = v; break; case 5: beads.density = v; break;
                       case 6: beads.feedback = v; break; case 7: beads.dryWet = v; break; case 8: beads.reverb = v; break;
                       case 9: beads.freeze = v > 0.5f; break; case 10: beads.quality = (int)v; break; case 11: beads.seed = (int)v; break; }
        break;
    case 4: if (key >= 0 && key < 4) tides.dest[key] = (int)v; break;
    case 5: if (key >= 0 && key < 4) tides.amount[key] = v; break;
    }
}

// [0] register, [1] length, [2..17] last 16 shift notes, [18..20] Marbles t1-t3, [21..23] X1-X3 volts,
// [24..27] Tides outputs, [28] Beads in peak, [29] Beads out peak
API float *fl_mod_info()
{
    modInfo[0] = shiftReg.reg; modInfo[1] = shiftReg.length;
    for (int i = 0; i < 16; ++i)
    {
        int k = shiftReg.histN - 16 + i;
        modInfo[2 + i] = k >= 0 ? (float)shiftReg.history[k] : -1.f;
    }
    for (int i = 0; i < 3; ++i) { modInfo[18 + i] = marbles.t[i] ? 1.f : 0.f; modInfo[21 + i] = marbles.x[i]; }
    for (int i = 0; i < 4; ++i) modInfo[24 + i] = tides.out[i];
    modInfo[28] = beads.inLevel; modInfo[29] = beads.outLevel;
    return modInfo;
}
