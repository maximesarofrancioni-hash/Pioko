// filterlab.cpp - the only translation unit that includes sst-filters.
#include "filterlab.h"
#include "euclid.h"

#include <cmath>
#include <cstring>
#include <cstdint>

#include "sst/filters.h"
#include "sst/filters/HalfRateFilter.h"

namespace sf = sst::filters;

namespace
{
constexpr int L = FilterLab::kLanes;
constexpr int kOS = 2;                          // oversampling factor
constexpr int kBlockOS = FilterLab::kBlock * kOS;
constexpr int kDelayLen = sf::utilities::MAX_FB_COMB + sf::utilities::SincTable::FIRipol_N;

struct FilterDef
{
    sf::FilterType type;
    int sub;
    const char *name;
    float gain; // makeup gain so the tour plays at roughly equal loudness
};

// Curated tour: one or two characteristic variants of each model.
const FilterDef kFilters[] = {
    {sf::fut_lp12, sf::st_Standard, "LP 12 dB (Vember SVF)", 0.98f},
    {sf::fut_lp24, sf::st_Driven, "LP 24 dB driven", 1.02f},
    {sf::fut_lpmoog, sf::st_lpmoog_24dB, "Legacy Ladder 24 dB", 0.84f},
    {sf::fut_vintageladder, sf::st_vintage_type1, "Vintage Ladder - Runge-Kutta", 2.71f},
    {sf::fut_vintageladder, sf::st_vintage_type2_compensated, "Vintage Ladder - Huovilainen (bass comp)", 0.68f},
    {sf::fut_vintageladder, sf::st_vintage_type3, "Vintage Ladder - Huovilainen 2010", 3.02f},
    {sf::fut_diode, sf::st_diode_24dB, "Diode Ladder 24 dB (TB-303-ish)", 1.07f},
    {sf::fut_diode, sf::st_diode_12dB, "Diode Ladder 12 dB", 0.95f},
    {sf::fut_k35_lp, sf::st_k35_moderate, "K35 LP (MS-20) moderate", 0.48f},
    {sf::fut_k35_lp, sf::st_k35_extreme, "K35 LP (MS-20) extreme", 0.39f},
    {sf::fut_k35_hp, sf::st_k35_heavy, "K35 HP (MS-20) heavy", 0.49f},
    {sf::fut_obxd_2pole_lp, sf::st_obxd2pole_pushed, "OB-Xd 12 dB LP pushed (SEM-ish)", 0.71f},
    {sf::fut_obxd_2pole_bp, sf::st_obxd2pole_standard, "OB-Xd 12 dB BP", 2.46f},
    {sf::fut_obxd_4pole, sf::st_obxd4pole_24dB, "OB-Xd 24 dB LP", 1.27f},
    {sf::fut_obxd_xpander, sf::st_obxdxpander_lp4, "OB-Xd Xpander LP4", 1.28f},
    {sf::fut_obxd_xpander, sf::st_obxdxpander_hp2lp1, "OB-Xd Xpander HP2+LP1", 1.49f},
    {sf::fut_obxd_xpander, sf::st_obxdxpander_ph3, "OB-Xd Xpander phaser 3", 0.63f},
    {sf::fut_cytomic_svf, sf::st_cytomic_lp, "Cytomic SVF LP", 0.55f},
    {sf::fut_cytomic_svf, sf::st_cytomic_bp, "Cytomic SVF BP", 0.95f},
    {sf::fut_cytomic_cascade_lp, sf::st_cascade_driven_compensated, "Cascade SVF LP driven", 0.36f},
    {sf::fut_tripole, sf::st_tripole_LLL3, "Tri-pole LLL (out 3)", 2.69f},
    {sf::fut_tripole, sf::st_tripole_LHL2, "Tri-pole LHL (out 2)", 2.35f},
    {sf::fut_cutoffwarp_lp, sf::st_cutoffwarp_ojd3, "Cutoff Warp LP - OJD x3", 0.63f},
    {sf::fut_resonancewarp_lp, sf::st_resonancewarp_tanh4, "Resonance Warp LP - tanh x4", 1.95f},
    {sf::fut_bp24, sf::st_Driven, "BP 24 dB driven", 0.73f},
    {sf::fut_notch12, sf::st_Notch, "Notch 12 dB", 0.61f},
    {sf::fut_comb_pos, sf::st_comb_pos_100, "Comb + 100%", 0.55f},
};
constexpr int kNumFilters = sizeof(kFilters) / sizeof(kFilters[0]);

// ---------------------------------------------------------------- state
alignas(16) sf::QuadFilterUnitState g_qfu;
alignas(16) float g_delay[L][kDelayLen];
sf::FilterCoefficientMaker<> g_coef[L];
sf::FilterUnitQFPtr g_fn = nullptr;
sf::HalfRate::HalfRateFilter g_down(6, true);

alignas(16) float g_osL[kBlockOS];
alignas(16) float g_osR[kBlockOS];

struct Voice
{
    float phase = 0.f, phase2 = 0.f;
    float inc = 0.f;
    float amp = 0.f, ampDecay = 0.999f;   // per OS sample multiplier
    float fenv = 0.f, fenvDecay = 0.99f;  // per block multiplier
    float level = 0.f;                    // per-note velocity
    float accent = 0.f;
    uint32_t noise = 0x12345678u;
};
Voice g_v[L];

EuclidTrack g_euclid[L + 1]; // + accent track

// lane: decay times (s), env amount (semitones), cutoff offset, pan, level
const float kAmpDecay[L] = {0.28f, 0.20f, 0.45f, 0.045f};
const float kFenvDecay[L] = {0.18f, 0.14f, 0.30f, 0.03f};
const float kFenvAmt[L] = {42.f, 34.f, 24.f, 18.f};
const float kCutOfs[L] = {-6.f, 6.f, 0.f, 24.f};
const float kPan[L] = {0.f, -0.35f, 0.4f, 0.15f};
const float kLevel[L] = {0.9f, 0.55f, 0.45f, 0.35f};

// note patterns, cycled on each hit (semitones above A1 = 55 Hz)
const int kBassNotes[] = {0, 0, 12, 0, 7, 0, 10, 5};
const int kLeadNotes[] = {24, 27, 31, 34, 36, 31, 29, 27, 39, 36};
const int kStabNotes[] = {19, 15, 22, 17};
int g_noteIdx[L] = {0, 0, 0, 0};

inline float noteHz(float semisAboveA1) { return 55.f * std::exp2(semisAboveA1 / 12.f); }

inline float polyblep(float t, float dt)
{
    if (t < dt)
    {
        t /= dt;
        return t + t - t * t - 1.f;
    }
    if (t > 1.f - dt)
    {
        t = (t - 1.f) / dt;
        return t * t + t + t + 1.f;
    }
    return 0.f;
}

inline float saw(float p, float dt) { return 2.f * p - 1.f - polyblep(p, dt); }

inline float wrap1(float p) { return p - (float)(int)p; }

inline float whiteNoise(uint32_t &s)
{
    s ^= s << 13;
    s ^= s >> 17;
    s ^= s << 5;
    return (float)(int32_t)s * (1.f / 2147483648.f);
}

inline float decayCoef(float seconds, float rate)
{
    return std::exp(-6.9f / (seconds * rate)); // -60 dB after 'seconds'
}
} // namespace

// ---------------------------------------------------------------- API
int FilterLab::NumFilters() const { return kNumFilters; }
const char *FilterLab::FilterName() const { return kFilters[m_filter].name; }
const char *FilterLab::FilterNameAt(int i) const
{
    return (i >= 0 && i < kNumFilters) ? kFilters[i].name : "";
}

void FilterLab::SetEuclid(int lane, int steps, int hits, int rotate)
{
    if (lane >= 0 && lane <= L)
        g_euclid[lane].set(steps, hits, rotate);
}

void FilterLab::GetEuclid(int lane, int &steps, int &hits, int &rotate) const
{
    const EuclidTrack &t = g_euclid[lane < 0 ? 0 : (lane > L ? L : lane)];
    steps = t.steps;
    hits = t.hits;
    rotate = t.rotate;
}

void FilterLab::Init(float sampleRate)
{
    m_sr = sampleRate;
    const float srOS = sampleRate * kOS;

    std::memset(&g_qfu, 0, sizeof(g_qfu));
    for (int i = 0; i < L; ++i)
    {
        g_qfu.DB[i] = g_delay[i];
        g_qfu.active[i] = (int)0xffffffff;
        g_qfu.WP[i] = 0;
        g_coef[i].setSampleRateAndBlockSize(srOS, kBlockOS);
    }

    // bass, lead, stab, noise, accent
    g_euclid[0].set(16, 5, 0);
    g_euclid[1].set(16, 7, 2);
    g_euclid[2].set(8, 3, 1);
    g_euclid[3].set(16, 11, 0);
    g_euclid[4].set(8, 3, 0);

    m_step = 0;
    m_bar = 0;
    m_barsOnFilter = 0;
    m_samplesToNext = 0.0;
    SelectFilter(0);
}

void FilterLab::ResetFilterState()
{
    std::memset(g_qfu.R, 0, sizeof(g_qfu.R));
    std::memset(g_qfu.C, 0, sizeof(g_qfu.C));
    std::memset(g_qfu.dC, 0, sizeof(g_qfu.dC));
    std::memset(g_delay, 0, sizeof(g_delay));
    for (int i = 0; i < L; ++i)
    {
        g_qfu.WP[i] = 0;
        g_coef[i].Reset();
        g_coef[i].setSampleRateAndBlockSize(m_sr * kOS, kBlockOS);
    }
}

void FilterLab::SelectFilter(int index)
{
    index %= kNumFilters;
    if (index < 0)
        index += kNumFilters;
    m_filter = index;
    const FilterDef &d = kFilters[index];
    g_fn = sf::GetQFPtrFilterUnit(d.type, (sf::FilterSubType)d.sub);
    ResetFilterState();
    m_barsOnFilter = 0;
    filterChanged = true;
}

void FilterLab::Tick16th()
{
    const float srOS = m_sr * kOS;
    if (m_step >= 16) // bar line: maybe move on to the next filter before the downbeat
    {
        m_step = 0;
        ++m_bar;
        if (barsPerFilter > 0 && ++m_barsOnFilter >= barsPerFilter)
            NextFilter();
    }
    const bool accent = g_euclid[L].hit(m_step);

    for (int lane = 0; lane < L; ++lane)
    {
        if (mute[lane] || !g_euclid[lane].hit(m_step))
            continue;
        Voice &v = g_v[lane];
        int n = 0;
        switch (lane)
        {
        case 0: n = kBassNotes[g_noteIdx[0]++ % 8]; break;
        case 1: n = kLeadNotes[g_noteIdx[1]++ % 10]; break;
        case 2: n = kStabNotes[g_noteIdx[2]++ % 4]; break;
        default: n = 0; break;
        }
        v.inc = noteHz((float)n) / srOS;
        v.amp = 1.f;
        v.ampDecay = decayCoef(kAmpDecay[lane] * (accent ? 1.4f : 1.f), srOS);
        v.fenv = 1.f;
        v.fenvDecay = decayCoef(kFenvDecay[lane], srOS / kBlockOS);
        v.accent = accent ? 1.f : 0.f;
        v.level = kLevel[lane] * (accent ? 1.25f : 0.9f);
    }

    ++m_step;
}

void FilterLab::Render(float *outL, float *outR)
{
    const float srOS = m_sr * kOS;
    const double samplesPer16th = (double)m_sr * 60.0 / (double)bpm / 4.0;

    // sequencer runs at output rate; resolve the tick at block start (0.67 ms jitter max)
    m_samplesToNext -= FilterLab::kBlock;
    if (m_samplesToNext <= 0.0)
    {
        Tick16th();
        m_samplesToNext += samplesPer16th;
    }

    // slow triangle LFO over 8 bars sweeps the base cutoff
    const float lfoInc = (float)FilterLab::kBlock / (float)(samplesPer16th * 16.0 * 8.0);
    m_lfoPhase = wrap1(m_lfoPhase + lfoInc);
    const float tri = m_lfoPhase < 0.5f ? 4.f * m_lfoPhase - 1.f : 3.f - 4.f * m_lfoPhase;
    const float base = (sweep ? -18.f + 18.f * tri : -12.f) + cutoffOffset + cutoffMod; // A440-relative
    m_cutoffHz = 440.f * std::exp2(base / 12.f);

    // coefficients once per OS block; sst ramps them per sample through dC
    for (int lane = 0; lane < L; ++lane)
    {
        Voice &v = g_v[lane];
        float cut = base + kCutOfs[lane] + envAmount * kFenvAmt[lane] * v.fenv * (1.f + 0.4f * v.accent);
        if (cut > 60.f) cut = 60.f;
        if (cut < -60.f) cut = -60.f;
        float res = resonance + 0.1f * v.accent;
        if (res > 0.99f) res = 0.99f;
        g_coef[lane].MakeCoeffs(cut, res, kFilters[m_filter].type,
                                (sf::FilterSubType)kFilters[m_filter].sub, nullptr, false);
        g_coef[lane].updateState(g_qfu, lane);
        v.fenv *= v.fenvDecay;
    }

    float panL[L], panR[L];
    for (int i = 0; i < L; ++i)
    {
        panL[i] = std::sqrt(0.5f * (1.f - kPan[i]));
        panR[i] = std::sqrt(0.5f * (1.f + kPan[i]));
    }

    const float makeup = kFilters[m_filter].gain;
    alignas(16) float in[4];
    alignas(16) float out[4];
    for (int s = 0; s < kBlockOS; ++s)
    {
        // oscillators
        {
            Voice &b = g_v[0]; // bass: saw + sub square
            b.phase = wrap1(b.phase + b.inc);
            b.phase2 = wrap1(b.phase2 + b.inc * 0.5f);
            in[0] = 0.8f * saw(b.phase, b.inc) + 0.35f * (b.phase2 < 0.5f ? 1.f : -1.f);
        }
        {
            Voice &l = g_v[1]; // lead: two detuned saws
            l.phase = wrap1(l.phase + l.inc);
            l.phase2 = wrap1(l.phase2 + l.inc * 1.006f);
            in[1] = 0.5f * (saw(l.phase, l.inc) + saw(l.phase2, l.inc * 1.006f));
        }
        {
            Voice &p = g_v[2]; // stab: 30% pulse
            p.phase = wrap1(p.phase + p.inc);
            float p2 = wrap1(p.phase + 0.7f);
            in[2] = 0.7f * (saw(p.phase, p.inc) - saw(p2, p.inc));
        }
        in[3] = whiteNoise(g_v[3].noise);

        for (int i = 0; i < L; ++i)
            in[i] *= drive;

        auto y = g_fn(&g_qfu, SIMD_MM(load_ps)(in));
        SIMD_MM(store_ps)(out, y);

        float mixL = 0.f, mixR = 0.f;
        for (int i = 0; i < L; ++i)
        {
            Voice &v = g_v[i];
            float o = out[i] * v.amp * v.level * makeup;
            v.amp *= v.ampDecay;
            mixL += o * panL[i];
            mixR += o * panR[i];
        }
        g_osL[s] = mixL;
        g_osR[s] = mixR;
    }

    // a filter that blew up (NaN) gets reset rather than killing the output
    if (!(std::fabs(g_osL[0]) < 1e6f) || !(std::fabs(g_osR[kBlockOS - 1]) < 1e6f))
    {
        ResetFilterState();
        std::memset(g_osL, 0, sizeof(g_osL));
        std::memset(g_osR, 0, sizeof(g_osR));
    }

    g_down.process_block_D2(g_osL, g_osR, kBlockOS);

    for (int i = 0; i < FilterLab::kBlock; ++i)
    {
        // gentle bus saturation, keeps resonant peaks out of hard clipping
        outL[i] = std::tanh(g_osL[i] * 1.6f);
        outR[i] = std::tanh(g_osR[i] * 1.6f);
    }
    samplesOS += kBlockOS;
}
