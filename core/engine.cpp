// Pyoko's engine: two tracks (osc440's voice with two sst-filters models per voice; the sampler through its own
// two) sequenced by their Euclidean patterns, Marbles or the shift register, Tides modulating, Beads on the mix, all
// on one 16th-note clock. Its C API (engine.h) is the WebAssembly exports of the browser build and what the Pi 4
// kernel and the host renderer call. The platform supplies the sampler's memory (pyoko_sample_ram()).
#include "engine.h"
#include "wave_lane.h"
#include "sampler.h"
#include "mods/mods.h"
#include "synth/sst_filter.h"
#include "config/audio_config.h"
#include <cmath>
static constexpr int kBlock = FL_BLOCK;   // frames per fl_render()
// the sequencer clock: 16ths at `bpm`, resolved at block starts; step counts 1..16 after each tick, as
// filter-lab's did (the page and the wave lane read it that way)
// swing: each pair of 16ths lasts two 16ths, the first (1 + swing) of one, the second (1 - swing); 0 = straight,
// 1/3 = triplet
static struct Clock { float bpm = 120.f, swing = 0.f; int step = 0, bar = 0; double toNext = 0.0; } clk;
static Sampler smp;
// The sampler track: its sequencer and its VCF (two sst-filters models, as the osc440 voice has). Each filter is
// left out while fully open with no resonance (the defaults), so the sampler alone sounds as before.
static struct SmpTrack
{
    int seq = 0;                       // 0 its Euclidean lane, 1 Marbles (t2 hits, X2 picks the slice), 2 shift register
    synth::SstFilter f[2];
    int model[2] = {17, 4};            // Cytomic SVF LP, Huovilainen ladder
    float cut[2] = {135.f, 135.f};     // MIDI notes; 135: open
    float res[2] = {0.f, 0.f};
    bool parallel = false;
    float drive = 1.f;
    float envAmt = 0.f, envMs = 200.f; // filter envelope: semitones, decay; restarts on every hit
    float env = 0.f;
    unsigned lastTrig = 0;
} trk;
static float smpInfo[32];
static uint8_t smpOv[256];
static WaveLane wave;
static float wbuf[512];
static float waveL[kBlock], waveR[kBlock];   // the wave lane alone, for its spectrum
static int lastStep = -1, lastBar = -1;
static float sampleRate = 44100.f;
static mods::Shift shiftReg;
static mods::MarblesState marbles;
static mods::TidesState tides;
static mods::BeadsState beads;
static float modInfo[40];
static unsigned lastNoteOns = 0;
static float applied[16];          // Tides offset applied per destination last block
static float outL[kBlock], outR[kBlock];

#define API extern "C" __attribute__((used, visibility("default")))   // exported from the WebAssembly build

API void fl_init(float sr)
{
    sampleRate = sr;
    clk = Clock(); wave.Init(); lastStep = -1; lastBar = -1;
    mods::MarblesInit(sr / kBlock);
    mods::TidesInit();
    mods::BeadsInit();
    int frames = 0;
    float *ram = pyoko_sample_ram(&frames);
    smp.Init(ram, frames, sr); smp.MakeDemo();
}

// Tides destinations: osc440 parameter id (or -1 for the others) and the range of a full-scale output
struct Dest { int id; float range; };
static const Dest kDest[] = {
    {-1, 0.f},                          // 0 off
    {105, 32.f}, {121, 32.f}, {137, 32.f},   // 1-3 osc 1-3 wave position
    {7, 36.f}, {2, 36.f}, {3, 0.5f},    // 4 filter 2 cutoff, 5 filter 1 cutoff, 6 filter 1 resonance
    {107, 0.45f},                       // 7 osc 1 pulse width
    {8, 0.5f},                          // 8 filter 2 resonance
    {-1, 0.5f}, {-1, 0.5f}, {-1, 12.f}, {-1, 0.5f},    // 9-12 Beads time, size, pitch (semitones), density
};
static const int kNumDest = sizeof(kDest) / sizeof(kDest[0]);
API void fl_render()
{
    for (int i = 0; i < kBlock; ++i) outL[i] = outR[i] = 0.f;
    clk.toNext -= kBlock;
    if (clk.toNext <= 0.0)
    {
        if (clk.step >= 16) { clk.step = 0; ++clk.bar; }
        ++clk.step;
        clk.toNext += sampleRate * 60.0 / clk.bpm / 4.0 * (clk.step & 1 ? 1.0 + clk.swing : 1.0 - clk.swing);
    }
    bool tick = false;
    // each new 16th triggers the wave lane and the sampler
    int st = clk.step, bar = clk.bar;
    if (st != lastStep || bar != lastBar)
    {
        lastStep = st; lastBar = bar;
        double sp16 = sampleRate * 60.0 / clk.bpm / 4.0;
        int step = (st + 15) % 16;
        wave.Tick(step, bar, sp16);
        smp.external = trk.seq != 0;
        smp.Tick(step, bar, sp16);
        if (wave.mode == WaveLane::kShift || trk.seq == 2)
        {
            // the register advances on every 16th (once, whichever tracks it plays); the gate mode decides which
            // steps play: each track's Euclidean hits, bit 0, or every 16th
            bool bit0 = false;
            int n = shiftReg.Step(bit0);
            if (wave.mode == WaveLane::kShift && (shiftReg.gateMode == 0 ? wave.Hit(step) : (shiftReg.gateMode == 1 ? bit0 : true)))
                wave.ExternalNote(45 + 12 * wave.octave + n - 12, step % 4 == 0 ? 0.95f : 0.8f, wave.gate * sp16);
            if (trk.seq == 2 && smp.on && (shiftReg.gateMode == 0 ? smp.Hit(step) : (shiftReg.gateMode == 1 ? bit0 : true)))
                smp.Trigger((shiftReg.reg & 0xff) * smp.NumSlices() / 256);   // the register's low byte picks the slice
        }
        tick = true;
    }
    // Marbles, every block, clocked by the 16ths
    mods::MarblesStep(marbles, tick);
    if (wave.mode == WaveLane::kMarbles)
    {
        double sp16 = sampleRate * 60.0 / clk.bpm / 4.0;
        for (int c = 0; c < 3; c += 2)          // t1 plays X1, t3 plays X3
            if (marbles.rise[c])
                wave.ExternalNote(45 + 12 * wave.octave + (int)std::lround(marbles.x[c] * 12.f), c ? 0.75f : 0.9f, wave.gate * sp16);
    }
    if (trk.seq == 1 && smp.on && marbles.rise[1])          // t2 plays the sampler, X2 picks the slice
    {
        static const float kLo[3] = {0.f, 0.f, -5.f}, kHi[3] = {2.f, 5.f, 5.f};   // the X range, volts
        int r = marbles.range < 0 ? 0 : (marbles.range > 2 ? 2 : marbles.range);
        float u = (marbles.x[1] - kLo[r]) / (kHi[r] - kLo[r]);
        u = u < 0.f ? 0.f : (u > 0.999f ? 0.999f : u);
        smp.Trigger((int)(u * smp.NumSlices()));
    }
    // Tides, every block: the wave lane's notes are its gate (for AD / AR)
    mods::TidesStep(tides, sampleRate / kBlock, wave.notesHeld > 0);
    float sum[16] = {0};
    for (int i = 0; i < 4; ++i)
        if (tides.dest[i] > 0 && tides.dest[i] < kNumDest)
            sum[tides.dest[i]] += tides.out[i] * tides.amount[i] * kDest[tides.dest[i]].range;
    for (int d = 1; d < kNumDest; ++d)
        if (kDest[d].id >= 0 && std::fabs(sum[d] - applied[d]) > 1e-4f)
            wave.ModParam((uint16_t)kDest[d].id, sum[d]);
    for (int d = 0; d < kNumDest; ++d) applied[d] = sum[d];
    for (int i = 0; i < kBlock; ++i) waveL[i] = waveR[i] = 0.f;
    wave.Render(waveL, waveR, kBlock);
    for (int i = 0; i < kBlock; ++i) { outL[i] += waveL[i]; outR[i] += waveR[i]; }
    // the sampler: resamples the filter lab + wave lane mix, then plays into it (before Beads and the Vinyl Sim)
    smp.Capture(outL, outR, kBlock);
    {
        // the sampler track: sample (with its attack / decay, the AMP) -> VCF -> into the mix
        static float sL[kBlock], sR[kBlock];
        for (int i = 0; i < kBlock; ++i) sL[i] = sR[i] = 0.f;
        smp.Render(sL, sR, kBlock);
        if (smp.Triggers() != trk.lastTrig) { trk.lastTrig = smp.Triggers(); trk.env = 1.f; }
        const float envK = std::exp(-(float)kBlock / (trk.envMs * 0.001f * sampleRate));
        const int H = (int)audio::kBlockSize;          // the filters ramp over osc440's block
        for (int o = 0; o < kBlock; o += H)
        {
            bool open[2];
            for (int j = 0; j < 2; ++j)
            {
                open[j] = trk.cut[j] >= 134.99f && trk.res[j] == 0.f && trk.envAmt == 0.f;
                trk.f[j].set_model(trk.model[j]);
                if (!open[j]) trk.f[j].set(trk.cut[j] + trk.envAmt * trk.env, trk.res[j]);
            }
            float *x = sL + o, y[audio::kBlockSize];
            if (trk.parallel)
            {
                for (int i = 0; i < H; ++i) y[i] = x[i] * trk.drive;
                if (!open[0]) trk.f[0].process_block(x, H);
                if (!open[1]) trk.f[1].process_block(y, H);
                for (int i = 0; i < H; ++i) x[i] = 0.5f * (x[i] + y[i]);
            }
            else
            {
                if (!open[0]) trk.f[0].process_block(x, H);
                if (!open[1]) { for (int i = 0; i < H; ++i) x[i] *= trk.drive; trk.f[1].process_block(x, H); }
            }
        }
        trk.env *= envK;
        for (int i = 0; i < kBlock; ++i) { outL[i] += sL[i]; outR[i] += sL[i]; }
    }
    // Beads on the mix, before the Vinyl Sim (which runs in the worklet)
    if (beads.on)
    {
        mods::BeadsState b = beads;
        auto cl = [](float v) { return v < 0.f ? 0.f : (v > 1.f ? 1.f : v); };
        b.time = cl(b.time + sum[9]); b.size = cl(b.size + sum[10]);
        b.pitch = std::fmax(-24.f, std::fmin(24.f, b.pitch + sum[11])); b.density = cl(b.density + sum[12]);
        bool seed = beads.seed == 1 ? wave.noteOns != lastNoteOns : (beads.seed == 2 ? tick : false);
        mods::BeadsProcess(b, outL, outR, kBlock, seed);
        beads.inLevel = b.inLevel; beads.outLevel = b.outLevel;
    }
    lastNoteOns = wave.noteOns;
    for (int i = 0; i < kBlock; ++i)   // soft limit above 0.8 so the sum stays inside +-1
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
API int fl_block() { return kBlock; }

// the sst-filters models filter 1 and 2 choose from (osc440 parameters 20 and 21)
API int fl_num_filters() { return synth::SstFilter::models(); }
API const char *fl_filter_name(int i) { return synth::SstFilter::model_name(i); }
API void fl_set(int id, float v)
{
    if (id == 4) clk.bpm = v;   // tempo
    if (id == 7) clk.swing = v < 0.f ? 0.f : (v > 0.4f ? 0.4f : v);
}
API int fl_step() { return clk.step; }
API int fl_bar() { return clk.bar; }

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
    case 5: wave.progression = (int)v; break;
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

// ---- sampler
// keys: 0 start, 1 end, 2 pitch, 3 level, 4 attack ms, 5 decay ms, 6 mode, 7 slices, 8 order, 9 reverse, 10 on,
//       11 steps, 12 hits, 13 rotate, 14 fine (cents), 15 pitch mode, 16 auto slices, 17 sensitivity
API void fl_smp_set(int key, float v)
{
    switch (key)
    {
    case 0: smp.start = v; break; case 1: smp.end = v; break; case 2: smp.pitch = v; break; case 3: smp.level = v; break;
    case 4: smp.attackMs = v; break; case 5: smp.decayMs = v; break; case 6: smp.mode = (int)v; break;
    case 7: smp.slices = (int)v; break; case 8: smp.order = (int)v; break; case 9: smp.reverse = v > 0.5f; break;
    case 10: smp.on = v > 0.5f; break; case 11: smp.steps = (int)v; break; case 12: smp.hits = (int)v; break;
    case 13: smp.rotate = (int)v; break;
    case 14: smp.fine = v; break; case 15: smp.pitchMode = (int)v; break;
    case 16: smp.autoSlice = v > 0.5f; break; case 17: smp.sens = v; break;
    // the sampler track: sequencer, VCF
    case 20: trk.seq = (int)v; break;
    case 21: case 22: trk.model[key - 21] = (int)v; break;
    case 23: case 24: trk.cut[key - 23] = v; break;
    case 25: case 26: trk.res[key - 25] = v; break;
    case 27: trk.parallel = v > 0.5f; break;
    case 28: trk.drive = v; break;
    case 29: trk.envAmt = v; break;
    case 30: trk.envMs = v < 1.f ? 1.f : v; break;
    }
    smp.Update();
}
API float *fl_smp_ram() { return smp.Ram(); }
API int fl_smp_cap() { return smp.Capacity(); }
API void fl_smp_loaded(int frames) { smp.SetLength(frames); }
API void fl_smp_demo() { smp.MakeDemo(); }
API void fl_smp_arm(int bars) { smp.Arm(bars); }
API void fl_smp_trigger(int slice) { smp.Trigger(slice); }
// [0..3] voice positions (0..1, -1 idle), [4] last slice, [5] rec state, [6] rec progress, [7] length (frames),
// [8] triggers, [9] 1 when a recording just finished, [10] slices n, [11..11+n] slice edges (0..1)
API float *fl_smp_info()
{
    for (int v = 0; v < 4; ++v) smpInfo[v] = smp.VoicePos(v);
    smpInfo[4] = (float)smp.LastSlice(); smpInfo[5] = (float)smp.RecState(); smpInfo[6] = smp.RecProgress();
    smpInfo[7] = (float)smp.Length(); smpInfo[8] = (float)smp.Triggers(); smpInfo[9] = smp.TakeRecorded() ? 1.f : 0.f;
    int n = smp.NumSlices(); smpInfo[10] = (float)n;
    for (int i = 0; i <= n; ++i) smpInfo[11 + i] = smp.Length() ? (float)smp.SliceStart(i) / smp.Length() : 0.f;
    return smpInfo;
}
API uint8_t *fl_smp_overview(int cols) { if (cols > 256) cols = 256; smp.Overview(smpOv, cols); return smpOv; }
