// mods.cpp - see mods.h
#include "mods.h"

#include <algorithm>
#include <cmath>
#include <cstring>

#include "stmlib/stmlib.h"
#include "stmlib/utils/gate_flags.h"

#include "marbles/random/random_generator.h"
#include "marbles/random/random_stream.h"
#include "marbles/random/t_generator.h"
#include "marbles/random/x_y_generator.h"
#include "marbles/random/quantizer.h"

#include "tides2/poly_slope_generator.h"

#include "beads/dsp/processor.h"

namespace marbles
{
#include "marbles_scales.inc"
}

namespace mods
{

// ================================================================ shift register
int Shift::Quantize(int v) const
{
    static const int kScales[5][12] = {
        {0, 3, 5, 7, 10, -1}, {0, 2, 3, 5, 7, 8, 10, -1}, {0, 2, 3, 5, 7, 9, 10, -1},
        {0, 2, 4, 5, 7, 9, 11, -1}, {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11}};
    const int *sc = kScales[scale < 0 ? 0 : (scale > 4 ? 4 : scale)];
    int n = 0;
    while (n < 12 && sc[n] >= 0)
        ++n;
    // spread the 8-bit value over range octaves of scale degrees
    int degrees = n * range;
    int d = v * degrees / 256;
    return (d / n) * 12 + sc[d % n];
}

int Shift::Step(bool &bit0)
{
    // Turing Machine: the bit leaving the loop comes back in, flipped with probability `change`
    int len = length < 2 ? 2 : (length > 16 ? 16 : length);
    uint16_t out = (reg >> (len - 1)) & 1;
    static uint32_t rng = 0x12345678u;
    rng = rng * 1664525u + 1013904223u;
    float r = (float)(rng >> 8) * (1.f / 16777216.f);
    if (r < change)
        out ^= 1;
    reg = (uint16_t)((reg << 1) | out);
    bit0 = reg & 1;
    int note = Quantize(reg & 0xff);
    if (histN < 32)
        history[histN++] = note;
    else
    {
        std::memmove(history, history + 1, 31 * sizeof(int));
        history[31] = note;
    }
    return note;
}

// ================================================================ Marbles
namespace
{
marbles::RandomGenerator g_rng;
marbles::RandomStream g_stream;
marbles::TGenerator g_t;
marbles::XYGenerator g_xy;
float g_ramp[4];
bool g_gates[2];
float g_volts[4];
stmlib::GateFlags g_clock = stmlib::GATE_FLAG_LOW;
bool g_prevT[3];
} // namespace

void MarblesInit(float blockRate)
{
    g_rng.Init(1);
    g_stream.Init(&g_rng);
    g_t.Init(&g_stream, blockRate);
    g_xy.Init(&g_stream, blockRate);
    for (int i = 0; i < 6; ++i)
        g_xy.LoadScale(i, marbles::preset_scales[i]);
}

void MarblesStep(MarblesState &m, bool clockTick)
{
    // the 16th clock as Marbles' external T clock; the rate knob picks a divider/multiplier
    static const float kRate[5] = {-48.f, -24.f, 0.f, 24.f, 48.f};   // the rate pot, in Marbles' ADC units
    g_clock = stmlib::ExtractGateFlags(g_clock, clockTick);

    marbles::Ramps ramps;
    ramps.master = &g_ramp[0];
    ramps.external = &g_ramp[1];
    ramps.slave[0] = &g_ramp[2];
    ramps.slave[1] = &g_ramp[3];

    g_t.set_model(marbles::TGeneratorModel(m.model));
    g_t.set_range(marbles::T_GENERATOR_RANGE_1X);
    g_t.set_rate(kRate[m.rateIndex < 0 ? 0 : (m.rateIndex > 4 ? 4 : m.rateIndex)]);
    g_t.set_bias(m.bias);
    g_t.set_jitter(m.jitter);
    g_t.set_deja_vu(m.dejaVu);
    g_t.set_length(m.length);
    g_t.set_pulse_width_mean(0.5f);
    g_t.set_pulse_width_std(0.f);
    bool reset = false;
    g_t.Process(true, &reset, &g_clock, ramps, g_gates, 1);

    marbles::GroupSettings x, y;
    x.control_mode = marbles::CONTROL_MODE_IDENTICAL;
    x.voltage_range = marbles::VoltageRange(m.range);
    x.register_mode = false;
    x.register_value = 0.f;
    x.spread = m.spread;
    x.bias = m.xbias;
    x.steps = m.steps;
    x.deja_vu = m.dejaVu;
    x.scale_index = m.scale;
    x.length = m.length;
    x.ratio.p = 1;
    x.ratio.q = 1;
    y = x;
    y.deja_vu = 0.f;
    y.length = 1;
    bool xreset = false;
    g_xy.Process(marbles::CLOCK_SOURCE_INTERNAL_T1_T2_T3, x, y, &xreset, &g_clock, ramps, g_volts, 1);

    bool t[3] = {g_gates[0], ramps.master[0] < 0.5f, g_gates[1]};
    for (int i = 0; i < 3; ++i)
    {
        m.rise[i] = t[i] && !g_prevT[i];
        g_prevT[i] = t[i];
        m.t[i] = t[i];
        m.x[i] = g_volts[i];
    }
}

// ================================================================ Tides
namespace
{
tides::PolySlopeGenerator g_tides;
stmlib::GateFlags g_tgate = stmlib::GATE_FLAG_LOW;
int g_prevOutputMode = -1;
} // namespace

void TidesInit() { g_tides.Init(); }

void TidesStep(TidesState &t, float blockRate, bool gate)
{
    if (t.outputMode != g_prevOutputMode)
    {
        g_tides.Reset();
        g_prevOutputMode = t.outputMode;
    }
    g_tgate = stmlib::ExtractGateFlags(g_tgate, gate);
    tides::PolySlopeGenerator::OutputSample o;
    float f = t.rateHz / blockRate;
    if (f > 0.25f) f = 0.25f;
    g_tides.Render(tides::RampMode(t.rampMode), tides::OutputMode(t.outputMode), tides::RANGE_CONTROL, f,
                   t.slope, t.shape, t.smooth, t.shift, &g_tgate, nullptr, &o, 1);
    // Tides' outputs are in volts: unipolar modes 0..8 V, bipolar -5..5 V. Normalize to -1..1 / 0..1.
    for (int i = 0; i < 4; ++i)
    {
        float v = o.channel[i];
        bool unipolar = t.rampMode != tides::RAMP_MODE_LOOPING || t.outputMode == tides::OUTPUT_MODE_GATES;
        t.out[i] = unipolar ? v / 8.f : v / 5.f;
        if (t.out[i] > 1.f) t.out[i] = 1.f;
        if (t.out[i] < -1.f) t.out[i] = -1.f;
    }
}

// ================================================================ Beads
namespace
{
uint8_t g_buf1[512 * 1024];
uint8_t g_buf2[256 * 1024];
uint16_t g_reverb[32768];
beads::Processor *g_beads = nullptr;
alignas(16) char g_beadsMem[sizeof(beads::Processor)];
int g_quality = -1;
stmlib::GateFlags g_seed[32];
stmlib::GateFlags g_prevSeed = stmlib::GATE_FLAG_LOW;
} // namespace

void BeadsInit()
{
    g_beads = new (g_beadsMem) beads::Processor();
    uint8_t *bufs[3] = {g_buf1, g_buf1 + 256 * 1024, g_buf2};
    g_beads->Init(bufs, g_reverb);
    g_beads->Configure(beads::Processor::MEDIA_DIGITAL, false);
    g_quality = 3;
}

static beads::RandomizedParameter rp(float v)
{
    beads::RandomizedParameter p;
    p.base = v;
    p.attenuverter = 0.f;    // attenuverter at noon, no CV: no random modulation
    p.has_cv = false;
    p.cv = 0.f;
    return p;
}

void BeadsProcess(BeadsState &b, float *L, float *R, int n, bool seedTick)
{
    if (!g_beads)
        return;
    if (b.quality != g_quality)
    {
        g_beads->Configure(beads::Processor::Media(b.quality), false);
        g_quality = b.quality;
    }
    float inPk = 0.f, outPk = 0.f;
    for (int off = 0; off < n; off += 16)
    {
        int m = std::min(16, n - off);
        beads::FloatFrame in[16], out[16];
        bool fr[16];
        for (int i = 0; i < m; ++i)
        {
            in[i].l = L[off + i];
            in[i].r = R[off + i];
            fr[i] = b.freeze;
            inPk = std::max(inPk, std::fabs(in[i].l));
            bool s = seedTick && off == 0 && i == 0;
            g_prevSeed = g_seed[i] = stmlib::ExtractGateFlags(g_prevSeed, s);
        }
        beads::Parameters p;
        p.time = rp(b.time);
        p.size = rp(b.size);
        p.shape = rp(b.shape);
        p.pitch = rp(b.pitch);
        p.monophonic_mode = false;
        beads::ModulatedParameter d;
        d.base = b.density;
        d.cv = 0.f;
        beads::Processor::MixParameters mx;
        mx.feedback = b.feedback;
        mx.dry_wet = b.dryWet;
        mx.reverb_amount = b.reverb;
        mx.output_gate = false;
        g_beads->Process(beads::SEED_BUTTON_STATE_OFF, b.seed ? g_seed : nullptr, fr, d, p, mx, in, out, (size_t)m);
        for (int i = 0; i < m; ++i)
        {
            L[off + i] = out[i].l;
            R[off + i] = out[i].r;
            outPk = std::max(outPk, std::fabs(out[i].l));
        }
    }
    b.inLevel = inPk;
    b.outLevel = outPk;
}

} // namespace mods
