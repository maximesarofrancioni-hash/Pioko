#include "synth/triple_wave_group.h"

#include "config/platform.h"
#include "dsp/buffer.h"
#include "dsp/fast_math.h"
#include "synth/wavetable_plaits.h"

namespace synth {
namespace {

const Wavetable* const kTableOf[TripleWaveGroup::kTables] = {&kBasicWavetable, &kPlaitsWavetable};

constexpr float kSvfMaxResonance = 0.9f;   // k >= 0.22: rings, never self-oscillates
constexpr float kSvfOpenNote = 134.99f;    // the top of the cutoff range: the SVF is off

}  // namespace

TripleWaveGroup::TripleWaveGroup() {
  // Every parameter goes through set_param once, so params_ holds the whole state
  const struct { uint16_t id; float value; } globals[] = {
      {kParamLevel, 0.125f},       {kParamWaveScan, 0.0f},       {kParamSvfCutoff, 135.0f},
      {kParamSvfResonance, 0.0f},  {kParamSvfMorph, 0.0f},       {kParamSvfBandpass, 0.0f},
      {kParamSvfEnvAmount, 0.0f},  {kParamLadderCutoff, 72.0f},  {kParamLadderResonance, 0.2f},
      {kParamDrive, 1.0f},         {kParamCompensation, 0.5f},   {kParamLadderEnvAmount, 36.0f},
      {kParamFilterRouting, 0.0f}, {kParamKeyTrack, 1.0f},       {kParamLadderOversample, 1.0f},
      {kParamPan, 0.0f},           {kParamModWheel, 0.0f},       {kParamPressure, 0.0f},
      {kParamExpression, 0.0f}};
  for (const auto& g : globals) set_param(g.id, g.value);
  for (int o = 0; o < kOscs; ++o) {
    const float defaults[] = {0.0f, 0.0f, 0.0f, 0.0f, o == 0 ? 1.0f : 0.0f, 0.0f, 0.0f, 0.5f, 0.0f, 0.0f};
    for (uint16_t f = kOscMode; f <= kOscSync; ++f) set_param(osc_param(o, static_cast<OscField>(f)), defaults[f]);
    set_param(wave_env_loop_param(o, false), 1.0f);   // start > end: no loop
    set_param(wave_env_loop_param(o, true), 0.0f);
  }
  for (int e = 0; e < kEnvs; ++e) set_param(env_param(e, kEnvMode), 0.0f);
  for (int l = 0; l < kLfos; ++l) {
    set_param(lfo_param(l, kLfoShape), 0.0f);
    set_param(lfo_param(l, kLfoReset), 0.0f);
    set_param(lfo_param(l, kLfoDelay), 0.0f);
  }
  for (int s = 0; s < ModMatrix::kSlots; ++s) {
    for (int f = 0; f < 3; ++f) set_param(matrix_param(s, f), 0.0f);
  }
  // Envelope 1 (filters): pluck; envelope 2 (VCA): fast attack, held
  const float times[kEnvs][4] = {{0.0f, 0.005f, 0.4f, 0.3f}, {0.0f, 0.005f, 0.2f, 0.3f},
                                 {0.0f, 0.2f, 0.5f, 0.5f}, {0.0f, 1.0f, 1.0f, 1.0f}};
  const float sustain[kEnvs] = {0.2f, 0.7f, 0.5f, 1.0f};
  for (int e = 0; e < kEnvs; ++e) {
    set_param(env_param(e, kEnvDelay), times[e][0]);
    set_param(env_param(e, kEnvAttack), times[e][1]);
    set_param(env_param(e, kEnvDecay), times[e][2]);
    set_param(env_param(e, kEnvRelease), times[e][3]);
    set_param(env_param(e, kEnvSustain), sustain[e]);
  }
  for (int o = 0; o < kOscs; ++o) {
    // Wave envelopes: rise to 1 over 0.5 s, hold, fall back over 0.3 s on release
    set_param(wave_env_param(o, 0, false), 1.0f);
    set_param(wave_env_param(o, 0, true), 0.5f);
    for (int s = 1; s < Mseg::kReleaseStage; ++s) {
      set_param(wave_env_param(o, s, false), 1.0f);
      set_param(wave_env_param(o, s, true), 0.0f);
    }
    set_param(wave_env_param(o, Mseg::kReleaseStage, false), 0.0f);
    set_param(wave_env_param(o, Mseg::kReleaseStage, true), 0.3f);
  }
  for (int l = 0; l < kLfos; ++l) set_param(lfo_param(l, kLfoRate), 1.0f + l);
  for (int v = 0; v < kVoices; ++v) {   // distinct random sequences per voice
    for (int l = 0; l < kLfos; ++l) voice_[v].lfo[l].seed(0x9e3779b9u * static_cast<uint32_t>(v * kLfos + l + 1));
    voice_[v].noise_state = 0x2545f491u * static_cast<uint32_t>(v + 1);
  }
}

void AUDIO_HOT(TripleWaveGroup::note_on)(uint8_t voice, float pitch, float velocity) {
  if (voice >= kVoices) return;
  Voice& v = voice_[voice];
  v.pitch = pitch;
  v.velocity = velocity;
  for (int o = 0; o < kOscs; ++o) v.wave_env[o].gate_on();
  for (int e = 0; e < kEnvs; ++e) v.env[e].gate_on();
  for (int l = 0; l < kLfos; ++l) v.lfo[l].note_on(lfo_reset_[l]);
}

void AUDIO_HOT(TripleWaveGroup::note_off)(uint8_t voice) {
  if (voice >= kVoices) return;
  Voice& v = voice_[voice];
  for (int o = 0; o < kOscs; ++o) v.wave_env[o].gate_off();
  for (int e = 0; e < kEnvs; ++e) v.env[e].gate_off();
}

void AUDIO_HOT(TripleWaveGroup::update_matrix_flags)() {
  for (bool& u : source_used_) u = false;
  for (bool& a : audio_level_) a = false;
  for (bool& a : audio_dst_) a = false;
  for (int i = 0; i < ModMatrix::kSlots; ++i) {
    const ModMatrix::Slot& slot = matrix_.slot(i);
    if (!mod::is_audio_source(slot.source)) continue;
    source_used_[slot.source - mod::kSrcOsc1] = true;
    if (mod::is_audio_destination(slot.destination)) audio_dst_[slot.destination] = true;
    if (slot.destination >= mod::kDstLevel1 && slot.destination <= mod::kDstLevel3) {
      audio_level_[slot.destination - mod::kDstLevel1] = true;
    }
  }
}

void AUDIO_HOT(TripleWaveGroup::update_usage)() {
  update_matrix_flags();
  bool src[mod::kNumSources] = {};
  for (int i = 0; i < ModMatrix::kSlots; ++i) {
    const ModMatrix::Slot& s = matrix_.slot(i);
    if (s.source != mod::kSrcNone && s.amount != 0.0f) src[s.source] = true;
  }
  for (int l = 0; l < kLfos; ++l) lfo_used_[l] = src[mod::kSrcLfo1 + l];
  env_used_[0] = src[mod::kSrcEnv1] || svf_env_ != 0.0f || ladder_env_ != 0.0f;
  env_used_[1] = true;   // the VCA, and how a voice knows it has finished
  for (int e = 2; e < kEnvs; ++e) env_used_[e] = src[mod::kSrcEnv1 + e];
  for (int o = 0; o < kOscs; ++o) {
    wave_env_used_[o] = src[mod::kSrcWaveEnv1 + o] || osc_[o].wave_env != 0.0f;
    // Read next block: as a matrix source, or osc 3 as osc 1's FM
    keep_last_[o] = source_used_[o] || (o == kOscs - 1 && osc_[0].fm != 0.0f);
  }
  usage_dirty_ = false;
}

// Events run on the audio core between blocks: in SRAM like the rest of it
void AUDIO_HOT(TripleWaveGroup::set_param)(uint16_t id, float value) {
  if (id < kParamCount) params_[id] = value;
  usage_dirty_ = true;
  if (id >= kParamMatrixBase) {
    const int slot = (id - kParamMatrixBase) / 3;
    switch ((id - kParamMatrixBase) % 3) {
      case 0: matrix_.set_source(slot, value); break;
      case 1: matrix_.set_destination(slot, value); break;
      default: matrix_.set_amount(slot, value); break;
    }
    return;   // the matrix flags follow in update_usage(), once per block
  }
  if (id >= kParamLfoBase) {
    const int lfo = (id - kParamLfoBase) / kLfoStride;
    if (lfo >= kLfos) return;
    switch ((id - kParamLfoBase) % kLfoStride) {
      case kLfoRate: for (Voice& v : voice_) v.lfo[lfo].set_rate(value); break;
      case kLfoShape: {
        const int s = static_cast<int>(value) % static_cast<int>(Lfo::Shape::kCount);
        for (Voice& v : voice_) v.lfo[lfo].set_shape(static_cast<Lfo::Shape>(s));
        break;
      }
      case kLfoReset: lfo_reset_[lfo] = value >= 0.5f; break;
      case kLfoDelay: for (Voice& v : voice_) v.lfo[lfo].set_delay(value); break;
      default: break;
    }
    return;
  }
  if (id >= kParamWaveEnvBase) {
    const int osc = (id - kParamWaveEnvBase) / kWaveEnvStride;
    const int field = (id - kParamWaveEnvBase) % kWaveEnvStride;
    if (osc >= kOscs) return;
    if (field < 2 * Mseg::kStages) {
      // Level and time arrive separately: the other half comes from what was set before
      const int stage = field / 2;
      (field % 2 ? wave_env_seconds_ : wave_env_level_)[osc][stage] = value;
      for (Voice& v : voice_) {
        v.wave_env[osc].set_stage(stage, wave_env_level_[osc][stage], wave_env_seconds_[osc][stage]);
      }
    } else if (field == 12 || field == 13) {
      wave_env_loop_[osc][field - 12] = static_cast<int>(value);
      for (Voice& v : voice_) v.wave_env[osc].set_loop(wave_env_loop_[osc][0], wave_env_loop_[osc][1]);
    }
    return;
  }
  if (id >= kParamOscBase) {
    const int osc = (id - kParamOscBase) / kOscStride;
    if (osc >= kOscs) return;
    OscSettings& s = osc_[osc];
    switch ((id - kParamOscBase) % kOscStride) {
      case kOscMode: {
        const int m = static_cast<int>(value) % static_cast<int>(Oscillator::Mode::kCount);
        for (Voice& v : voice_) v.osc[osc].set_mode(static_cast<Oscillator::Mode>(m));
        break;
      }
      case kOscTable: {
        const int t = static_cast<int>(value);
        if (t >= 0 && t < kTables) for (Voice& v : voice_) v.osc[osc].set_table(kTableOf[t]);
        break;
      }
      case kOscTune: s.tune = value; break;
      case kOscFine: s.fine = value; break;
      case kOscLevel: s.level = value; break;
      case kOscWave: s.wave = value; break;
      case kOscWaveEnvAmount: s.wave_env = value; break;
      case kOscPw: s.pw = value; break;
      case kOscFm: s.fm = value; break;
      case kOscSync: s.sync = osc > 0 && value >= 0.5f; break;
      default: break;
    }
    return;
  }
  if (id >= kParamEnvBase) {
    const int env = (id - kParamEnvBase) / kEnvStride;
    if (env >= kEnvs) return;
    for (Voice& v : voice_) {
      Dadsr& e = v.env[env];
      switch ((id - kParamEnvBase) % kEnvStride) {
        case kEnvMode: e.set_mode(value >= 0.5f ? Dadsr::Mode::kPpg : Dadsr::Mode::kExponential); break;
        case kEnvDelay: e.set_delay(value); break;
        case kEnvAttack: e.set_attack(value); break;
        case kEnvDecay: e.set_decay(value); break;
        case kEnvSustain: e.set_sustain(value); break;
        case kEnvRelease: e.set_release(value); break;
        default: break;
      }
    }
    return;
  }
  switch (id) {
    case kParamLevel: level_ = value; break;
    case kParamWaveScan: scan_ = value >= 0.5f ? WaveScan::kStepped : WaveScan::kSmooth; break;
    case kParamSvfCutoff: svf_cutoff_ = value; break;
    case kParamSvfResonance: svf_resonance_ = value; break;
    case kParamSvfMorph: morph_ = value; break;
    case kParamSvfBandpass: bandpass_ = value >= 0.5f; break;
    case kParamSvfEnvAmount: svf_env_ = value; break;
    case kParamLadderCutoff: ladder_cutoff_ = value; break;
    case kParamLadderResonance: ladder_resonance_ = value; break;
    case kParamDrive: drive_ = value; break;
    case kParamCompensation: compensation_ = value; break;
    case kParamLadderEnvAmount: ladder_env_ = value; break;
    case kParamFilterRouting: parallel_ = value >= 0.5f; break;
    case kParamKeyTrack: key_track_ = value; break;
    case kParamLadderOversample: oversample_ = value >= 0.5f; break;
    case kParamPan: pan_ = value; break;
    case kParamModWheel: mod_wheel_ = value; break;
    case kParamPressure: pressure_ = value; break;
    case kParamExpression: expression_ = value; break;
    default: break;
  }
}

void AUDIO_HOT(TripleWaveGroup::render_voice)(Voice& v, Bus& out) {
  constexpr float kInvBlock = 1.0f / kN;
  constexpr float kInvSampleRate = 1.0f / audio::kSampleRate;

  // ---- Control rate: sources, matrix, destinations ----
  float src[mod::kNumSources];
  src[mod::kSrcNone] = 0.0f;
  for (int l = 0; l < kLfos; ++l) src[mod::kSrcLfo1 + l] = lfo_used_[l] ? v.lfo[l].process(v.lfo_rate_mod[l]) : 0.0f;
  for (int e = 0; e < kEnvs; ++e) src[mod::kSrcEnv1 + e] = env_used_[e] ? v.env[e].process() : 0.0f;
  for (int o = 0; o < kOscs; ++o) src[mod::kSrcWaveEnv1 + o] = wave_env_used_[o] ? v.wave_env[o].process() : 0.0f;
  src[mod::kSrcVelocity] = v.velocity;
  src[mod::kSrcKey] = (v.pitch - 60.0f) * (1.0f / 60.0f);
  src[mod::kSrcModWheel] = mod_wheel_;
  src[mod::kSrcPressure] = pressure_;
  src[mod::kSrcExpression] = expression_;
  for (int o = 0; o < kOscs; ++o) src[mod::kSrcOsc1 + o] = v.last_osc[o][kN - 1];   // for block-rate uses
  src[mod::kSrcNoise] = v.last_noise;

  float dst[mod::kNumDestinations];
  dsp::clear(dst, mod::kNumDestinations);
  matrix_.apply(src, dst);
  for (int l = 0; l < kLfos; ++l) v.lfo_rate_mod[l] = dst[mod::kDstLfo1Rate + l];

  float level_target[kOscs];
  bool needed[kOscs];
  for (int o = 0; o < kOscs; ++o) {
    const OscSettings& s = osc_[o];
    v.osc[o].set_pitch(v.pitch + s.tune + s.fine * 0.01f + dst[mod::kDstPitch1 + o]);
    v.osc[o].set_wave(s.wave + s.wave_env * src[mod::kSrcWaveEnv1 + o] + dst[mod::kDstWave1 + o], scan_);
    v.osc[o].set_pulse_width(s.pw + dst[mod::kDstPw1 + o]);
    level_target[o] = dsp::clamp(s.level + dst[mod::kDstLevel1 + o], 0.0f, 2.0f);
  }
  // An oscillator renders when something can hear it: its level, its FM or sync
  // partner, or the matrix
  for (int o = 0; o < kOscs; ++o) {
    const int next = (o + 1) % kOscs;
    needed[o] = level_target[o] != 0.0f || v.osc_level[o] != 0.0f || audio_level_[o] ||
                osc_[next].fm != 0.0f || (o < kOscs - 1 && osc_[next].sync) || source_used_[o];
  }

  const float key = key_track_ * (v.pitch - 60.0f);
  const float fenv = src[mod::kSrcEnv1];
  const float svf_note = svf_cutoff_ + key + svf_env_ * fenv + dst[mod::kDstCutoff1];
  const float svf_res = dsp::clamp(svf_resonance_ + dst[mod::kDstResonance1], 0.0f, 1.0f);
  // Fully open, no resonance, low pass: the SVF would only shave the top octave
  // a little, so it is left out
  const bool svf_bypass = svf_note >= kSvfOpenNote && svf_res == 0.0f && morph_ <= 0.0f && !bandpass_;
  if (!svf_bypass || !v.svf_bypassed) {
    v.svf.set_normalized(dsp::note_to_hz(svf_note) * kInvSampleRate, kSvfMaxResonance * svf_res);
    v.svf.set_mode(morph_, bandpass_);
  }
  const float ladder_fc = dsp::note_to_hz(ladder_cutoff_ + key + ladder_env_ * fenv + dst[mod::kDstCutoff2]) * kInvSampleRate;
  const float res = ladder_resonance_ + dst[mod::kDstResonance2], drive = drive_ + dst[mod::kDstDrive];
  if (oversample_) {
    v.ladder2x.set(ladder_fc, res, drive, compensation_);
  } else {
    v.ladder.set(ladder_fc, res, drive, compensation_);
  }

  const float amp = dsp::clamp(src[mod::kSrcEnv2] * v.velocity * level_ + dst[mod::kDstAmp], 0.0f, 1.0f);
  const float angle = (dsp::clamp(pan_ + dst[mod::kDstPan], -1.0f, 1.0f) + 1.0f) * 0.125f;
  const float pan_l = dsp::fast_sin_cycles(angle + 0.25f), pan_r = dsp::fast_sin_cycles(angle);

  // ---- Audio rate: oscillators in order, so 1 -> 2 -> 3 see each other's
  // current block; anything later in line comes from its last block ----
  float cur[kOscs][kN];
  float noise[kN];
  if (source_used_[3]) {
    uint32_t r = v.noise_state;
    for (size_t i = 0; i < kN; ++i) {
      r ^= r << 13;
      r ^= r >> 17;
      r ^= r << 5;
      noise[i] = static_cast<float>(static_cast<int32_t>(r)) * (1.0f / 2147483648.0f);
    }
    v.noise_state = r;
    v.last_noise = noise[kN - 1];
  }
  float sync[kOscs][kN];
  for (int o = 0; o < kOscs; ++o) {
    if (!needed[o]) {
      dsp::clear(cur[o], kN);
      continue;
    }
    const float* signal[4] = {o > 0 ? cur[0] : v.last_osc[0], o > 1 ? cur[1] : v.last_osc[1],
                              v.last_osc[2], noise};
    float fm[kN];
    bool has_fm = false;
    const float* neighbour = o == 0 ? v.last_osc[kOscs - 1] : cur[o - 1];
    const float depth = osc_[o].fm, block_fm = dst[mod::kDstFm1 + o];
    if (depth != 0.0f || block_fm != 0.0f) {
      for (size_t i = 0; i < kN; ++i) fm[i] = depth * neighbour[i] + block_fm;
      has_fm = true;
    } else if (audio_dst_[mod::kDstFm1 + o]) {
      dsp::clear(fm, kN);
    }
    if (audio_dst_[mod::kDstFm1 + o]) has_fm |= matrix_.apply_audio(mod::kDstFm1 + o, signal, fm, kN);
    const bool slave = o > 0 && osc_[o].sync;
    const bool master = o < kOscs - 1 && osc_[o + 1].sync;
    v.osc[o].render(cur[o], kN, has_fm ? fm : nullptr, slave ? sync[o - 1] : nullptr, master ? sync[o] : nullptr);
  }

  // Mix, with levels ramped and modulated at audio rate (AM, ring modulation)
  const float* signal[4] = {cur[0], cur[1], cur[2], noise};
  float mix[kN];
  dsp::clear(mix, kN);
  for (int o = 0; o < kOscs; ++o) {
    if (!needed[o]) continue;
    float g = v.osc_level[o];
    const float step = (level_target[o] - g) * kInvBlock;
    if (audio_level_[o]) {
      float am[kN];
      dsp::clear(am, kN);
      matrix_.apply_audio(mod::kDstLevel1 + o, signal, am, kN);
      for (size_t i = 0; i < kN; ++i) {
        g += step;
        mix[i] += cur[o][i] * (g + am[i]);
      }
    } else if (level_target[o] != 0.0f || g != 0.0f) {
      for (size_t i = 0; i < kN; ++i) {
        g += step;
        mix[i] += cur[o][i] * g;
      }
    }
    v.osc_level[o] = level_target[o];
  }

  // Filters: SVF -> ladder (1x or 2x), or both in parallel
  float filtered[kN];
  if (parallel_) {
    for (size_t i = 0; i < kN; ++i) filtered[i] = mix[i];
    svf_block(v, filtered, svf_bypass);
    float ladder_out[kN];
    if (oversample_) {
      v.ladder2x.process_block(mix, ladder_out, kN);
    } else {
      for (size_t i = 0; i < kN; ++i) ladder_out[i] = mix[i];
      v.ladder.process_block(ladder_out, kN);
    }
    for (size_t i = 0; i < kN; ++i) filtered[i] = 0.5f * (filtered[i] + ladder_out[i]);
  } else {
    svf_block(v, mix, svf_bypass);
    if (oversample_) {
      v.ladder2x.process_block(mix, filtered, kN);
    } else {
      for (size_t i = 0; i < kN; ++i) filtered[i] = mix[i];
      v.ladder.process_block(filtered, kN);
    }
  }

  // VCA (ramped, plus audio-rate modulation) and pan
  const float target_l = amp * pan_l, target_r = amp * pan_r;
  float gl = v.gain_l, gr = v.gain_r;
  const float step_l = (target_l - gl) * kInvBlock, step_r = (target_r - gr) * kInvBlock;
  if (audio_dst_[mod::kDstAmp]) {
    float am[kN];
    dsp::clear(am, kN);
    matrix_.apply_audio(mod::kDstAmp, signal, am, kN);
    for (size_t i = 0; i < kN; ++i) {
      gl += step_l;
      gr += step_r;
      const float y = filtered[i];
      out.l[i] += y * (gl + am[i] * pan_l);
      out.r[i] += y * (gr + am[i] * pan_r);
    }
  } else {
    for (size_t i = 0; i < kN; ++i) {
      gl += step_l;
      gr += step_r;
      out.l[i] += filtered[i] * gl;
      out.r[i] += filtered[i] * gr;
    }
  }
  v.gain_l = target_l;
  v.gain_r = target_r;

  for (int o = 0; o < kOscs; ++o) {
    if (keep_last_[o]) dsp::copy(v.last_osc[o], cur[o], kN);
  }
}

// The SVF in place, or skipped; on a change between the two, one block's crossfade
void AUDIO_HOT(TripleWaveGroup::svf_block)(Voice& v, float* x, bool bypass) {
  if (bypass && v.svf_bypassed) return;
  if (!bypass && !v.svf_bypassed) {
    v.svf.process_multimode_block(x, kN);
    return;
  }
  float wet[kN];
  dsp::copy(wet, x, kN);
  v.svf.process_multimode_block(wet, kN);
  for (size_t i = 0; i < kN; ++i) {
    const float g = static_cast<float>(i + 1) * (1.0f / kN);   // towards the new state
    x[i] = bypass ? wet[i] + (x[i] - wet[i]) * g : x[i] + (wet[i] - x[i]) * g;
  }
  if (bypass) v.svf.reset();   // starts clean when it comes back
  v.svf_bypassed = bypass;
}

void AUDIO_HOT(TripleWaveGroup::render)(Bus& out) {
  if (usage_dirty_) update_usage();
  out.clear();
  for (Voice& v : voice_) {
    if (v.env[1].idle() && v.gain_l == 0.0f && v.gain_r == 0.0f) continue;   // silent voice
    render_voice(v, out);
  }
}

}  // namespace synth
