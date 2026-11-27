// tides_cycle.cc - one cycle of Tides 2 for the TIDES screen, from a second PolySlopeGenerator.
//
// The page calls tdv_render() with the same settings the engine's Tides runs with (mods.cpp: TidesStep) and
// draws the result. The generator is rendered at a fixed number of samples per cycle, after a few cycles to
// settle, so the picture is one clean cycle whatever the rate and is never cut short by a note. Outputs are
// normalized as the engine does: unipolar modes /8 V, bipolar /5 V, clamped to -1..1.
#include "tides2/poly_slope_generator.h"
#include "stmlib/utils/gate_flags.h"

#define API extern "C" __attribute__((used, visibility("default")))

static tides::PolySlopeGenerator g;
static float buf[4 * 512];                  // channel-major: buf[c * n + i]

API float *tdv_buf() { return buf; }

// ramp: 0 AD, 1 looping, 2 AR; outputs: 0 gates, 1 amplitude, 2 slope / phase, 3 frequency
// n samples (<= 512) span `cycles` cycles of the ramp. AD is triggered at the first sample; AR's gate is held
// for the first half.
API void tdv_render(int ramp, int outputs, float shape, float slope, float smooth, float shift, int n, float cycles)
{
    if (n > 512) n = 512;
    if (n < 2) n = 2;
    const float f = cycles / (float)n;
    const bool unipolar = ramp != tides::RAMP_MODE_LOOPING || outputs == tides::OUTPUT_MODE_GATES;
    g.Init();
    tides::PolySlopeGenerator::OutputSample o;
    stmlib::GateFlags gate = stmlib::GATE_FLAG_LOW;
    // settle: four cycles (looping), or one untriggered stretch (AD / AR)
    for (int i = 0; i < 4 * n; ++i)
    {
        gate = stmlib::ExtractGateFlags(gate, false);
        g.Render(tides::RampMode(ramp), tides::OutputMode(outputs), tides::RANGE_CONTROL, f,
                 slope, shape, smooth, shift, &gate, nullptr, &o, 1);
    }
    for (int i = 0; i < n; ++i)
    {
        bool high = ramp == tides::RAMP_MODE_AR ? i < n / 2 : (ramp == tides::RAMP_MODE_AD && i == 0);
        gate = stmlib::ExtractGateFlags(gate, high);
        g.Render(tides::RampMode(ramp), tides::OutputMode(outputs), tides::RANGE_CONTROL, f,
                 slope, shape, smooth, shift, &gate, nullptr, &o, 1);
        for (int c = 0; c < 4; ++c)
        {
            float v = unipolar ? o.channel[c] / 8.f : o.channel[c] / 5.f;
            buf[c * n + i] = v > 1.f ? 1.f : (v < -1.f ? -1.f : v);
        }
    }
}
