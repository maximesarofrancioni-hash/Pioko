// mods.h - filter-lab's sequencers and modules from Mutable Instruments (pichenettes/eurorack, MIT),
// run unmodified, plus a Turing Machine-style shift register:
//   Shift   - shift register note source for the wave lane
//   Marbles - T (gates, clocked by the 16ths) and X (quantized notes) for the wave lane
//   Tides   - Tides 2 PolySlopeGenerator, 4 outputs routed to 4 destinations (the modulation page)
//   Beads   - the granular processor, on the master bus before the Vinyl Sim
// Everything except Beads runs at control rate (one step per 32-frame engine block).
#pragma once
#include <cstdint>

namespace mods
{

// ---------------------------------------------------------------- shift register
struct Shift
{
    uint16_t reg = 0x9a35;
    int length = 8;          // 2..16
    float change = 0.15f;    // 0 = locked loop, 1 = always new
    int range = 2;           // octaves 1..4
    int scale = 0;           // 0 minor pent, 1 minor, 2 dorian, 3 major, 4 chromatic
    int gateMode = 0;        // 0: Euclid hits, 1: register bit 0, 2: every 16th
    int history[32];         // last notes (semitones above root), newest last
    int histN = 0;
    // Advances one step; returns the note (semitones above A1) and whether bit 0 is set.
    int Step(bool &bit0);
    int Quantize(int value255) const;
};

// ---------------------------------------------------------------- Marbles
struct MarblesState
{
    // T section
    int model = 0;           // T_GENERATOR_MODEL_*
    int rateIndex = 2;       // 0 /4, 1 /2, 2 x1, 3 x2, 4 x4 (relative to the 16ths)
    float bias = 0.5f, jitter = 0.f, dejaVu = 0.f;
    int length = 8;          // deja vu loop 1..16
    // X section
    float spread = 0.5f, xbias = 0.5f, steps = 0.5f;
    int scale = 0;           // 0..5 Marbles preset scales
    int range = 1;           // 0 narrow (2 V), 1 positive (5 V), 2 full (+-5 V)
    // outputs of the last block, for the display and the wave lane
    bool t[3];               // t1, t2, t3 gates
    float x[3];              // X1..X3 volts
    bool rise[3];
};

void MarblesInit(float blockRate);
// Runs one control-rate step. clockTick: a 16th started in this block.
void MarblesStep(MarblesState &m, bool clockTick);

// ---------------------------------------------------------------- Tides
struct TidesState
{
    float rateHz = 0.5f;
    float shape = 0.5f, slope = 0.5f, smooth = 0.5f, shift = 0.2f;
    int rampMode = 1;        // AD, LOOPING, AR
    int outputMode = 2;      // GATES, AMPLITUDE, SLOPE_PHASE, FREQUENCY
    int dest[4] = {0, 0, 0, 0};
    float amount[4] = {0.5f, 0.5f, 0.5f, 0.5f};
    float out[4];            // normalized -1..1 after the step
};

void TidesInit();
// gate: the wave lane's note gate (for AD / AR), one control-rate step
void TidesStep(TidesState &t, float blockRate, bool gate);

// ---------------------------------------------------------------- Beads
struct BeadsState
{
    bool on = true;
    float time = 0.5f, size = 0.5f, shape = 0.5f, pitch = 0.f;   // pitch in semitones, -24..24
    float density = 0.65f, feedback = 0.f, dryWet = 0.35f, reverb = 0.25f;
    bool freeze = false;
    int quality = 3;         // dirty tape, clean tape, vintage digital, digital
    int seed = 0;            // 0 off (density decides), 1 wave notes, 2 every 16th
    float inLevel = 0.f, outLevel = 0.f;   // peaks of the last block, for the display
};

void BeadsInit();
// In place on a stereo block (n <= 32). seedTick: a seed trigger in this block.
void BeadsProcess(BeadsState &b, float *L, float *R, int n, bool seedTick);

} // namespace mods
