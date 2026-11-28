// The only translation unit of osc440 that includes sst-filters.
#include "synth/sst_filter.h"

#include <cmath>
#include <cstring>

#include "config/audio_config.h"
#include "sst/filters.h"

namespace synth {
namespace {

namespace sf = sst::filters;

struct Model {
  sf::FilterType type;
  int sub;
  const char* name;
  float gain;   // makeup gain, so the models play at roughly equal loudness
};

// filter-lab's tour: one or two characteristic variants of each model, with its makeup gains
const Model kModels[] = {
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
constexpr int kNumModels = sizeof(kModels) / sizeof(kModels[0]);
constexpr int kDelayLen = sf::utilities::MAX_FB_COMB + sf::utilities::SincTable::FIRipol_N;

// One sst unit per filter. sst runs four lanes at once; a voice's filter uses lane 0.
struct Unit {
  sf::QuadFilterUnitState qfu;
  float delay[kDelayLen];
  sf::FilterCoefficientMaker<> coef;
  sf::FilterUnitQFPtr fn = nullptr;
};
constexpr int kPool = 64;   // 2 filters x 8 voices x 2 groups, with room
alignas(16) Unit g_pool[kPool];
int g_used = 0;

void clear(Unit& u) {
  std::memset(&u.qfu, 0, sizeof(u.qfu));
  std::memset(u.delay, 0, sizeof(u.delay));
  u.qfu.DB[0] = u.delay;
  u.qfu.active[0] = static_cast<int>(0xffffffff);
  u.coef.Reset();
  u.coef.setSampleRateAndBlockSize(static_cast<float>(audio::kSampleRate), audio::kBlockSize);
}

}  // namespace

int SstFilter::models() { return kNumModels; }
const char* SstFilter::model_name(int model) { return model >= 0 && model < kNumModels ? kModels[model].name : ""; }

SstFilter::SstFilter() {
  if (g_used < kPool) {
    unit_ = g_used++;
    clear(g_pool[unit_]);
  }
}

void SstFilter::set_model(int model) {
  if (model < 0) model = 0;
  if (model >= kNumModels) model = kNumModels - 1;
  if (model == model_ || unit_ < 0) return;
  model_ = model;
  Unit& u = g_pool[unit_];
  clear(u);
  u.fn = sf::GetQFPtrFilterUnit(kModels[model].type, static_cast<sf::FilterSubType>(kModels[model].sub));
}

void SstFilter::set(float note, float resonance) {
  if (unit_ < 0 || model_ < 0) return;
  Unit& u = g_pool[unit_];
  float cut = note - 69.0f;   // sst: semitones from A440
  cut = cut < -60.0f ? -60.0f : (cut > 66.0f ? 66.0f : cut);
  resonance = resonance < 0.0f ? 0.0f : (resonance > 0.99f ? 0.99f : resonance);
  u.coef.MakeCoeffs(cut, resonance, kModels[model_].type, static_cast<sf::FilterSubType>(kModels[model_].sub),
                    nullptr, false);
  u.coef.updateState(u.qfu, 0);
}

void SstFilter::process_block(float* x, size_t n) {
  if (unit_ < 0 || model_ < 0) return;
  Unit& u = g_pool[unit_];
  if (!u.fn) return;
  const float gain = kModels[model_].gain;
  alignas(16) float in[4] = {0.0f, 0.0f, 0.0f, 0.0f}, out[4];
  for (size_t i = 0; i < n; ++i) {
    in[0] = x[i];
    SIMD_MM(store_ps)(out, u.fn(&u.qfu, SIMD_MM(load_ps)(in)));
    x[i] = out[0] * gain;
  }
  // a filter that blew up (NaN, runaway) starts again rather than silencing the voice
  if (!(std::fabs(x[n - 1]) < 1e4f)) {
    clear(u);
    for (size_t i = 0; i < n; ++i) x[i] = 0.0f;
  }
}

void SstFilter::reset() {
  if (unit_ < 0) return;
  Unit& u = g_pool[unit_];
  sf::FilterUnitQFPtr fn = u.fn;
  clear(u);
  u.fn = fn;
}

}  // namespace synth
