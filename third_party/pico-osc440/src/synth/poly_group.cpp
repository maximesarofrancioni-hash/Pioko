#include "synth/poly_group.h"

#include "config/platform.h"
#include "dsp/fast_math.h"

namespace synth {

template <class Osc>
PolyGroup<Osc>::PolyGroup() {
  // A plucky default patch
  set_param(kParamFilterAttack, 0.005f);
  set_param(kParamAmpAttack, 0.005f);
  set_param(kParamFilterDecay, 0.4f);
  set_param(kParamFilterSustain, 0.2f);
  set_param(kParamFilterRelease, 0.3f);
  set_param(kParamAmpDecay, 0.2f);
  set_param(kParamAmpSustain, 0.7f);
  set_param(kParamAmpRelease, 0.3f);
}

template <class Osc>
void PolyGroup<Osc>::note_on(uint8_t voice, float pitch, float velocity) {
  if (voice >= kVoices) return;
  Voice& v = voice_[voice];
  v.osc.set_pitch(pitch);
  v.pitch = pitch;
  v.velocity = velocity;
  v.amp_env.gate_on();
  v.filter_env.gate_on();
}

template <class Osc>
void PolyGroup<Osc>::note_off(uint8_t voice) {
  if (voice >= kVoices) return;
  voice_[voice].amp_env.gate_off();
  voice_[voice].filter_env.gate_off();
}

template <class Osc>
void PolyGroup<Osc>::set_param(uint16_t id, float value) {
  switch (id) {
    case kParamLevel: level_ = value; break;
    case kParamCutoff: cutoff_ = value; break;
    case kParamResonance: resonance_ = value; break;
    case kParamFilterEnvAmount: env_amount_ = value; break;
    case kParamKeyTrack: key_track_ = value; break;
    case kParamWave: wave_ = value; break;
    case kParamWaveEnvAmount: wave_env_ = value; break;
    case kParamWaveScan: scan_ = value >= 0.5f ? WaveScan::kStepped : WaveScan::kSmooth; break;
    default:
      for (Voice& v : voice_) {
        switch (id) {
          case kParamFilterAttack: v.filter_env.set_attack(value); break;
          case kParamFilterDecay: v.filter_env.set_decay(value); break;
          case kParamFilterSustain: v.filter_env.set_sustain(value); break;
          case kParamFilterRelease: v.filter_env.set_release(value); break;
          case kParamAmpAttack: v.amp_env.set_attack(value); break;
          case kParamAmpDecay: v.amp_env.set_decay(value); break;
          case kParamAmpSustain: v.amp_env.set_sustain(value); break;
          case kParamAmpRelease: v.amp_env.set_release(value); break;
          default: break;
        }
      }
      break;
  }
}

template <class Osc>
inline void PolyGroup<Osc>::render_block(Bus& out) {
  constexpr float kInvBlock = 1.0f / audio::kBlockSize;
  constexpr float kInvSampleRate = 1.0f / audio::kSampleRate;
  float osc[audio::kBlockSize];
  out.clear();

  for (Voice& v : voice_) {
    if (v.amp_env.idle() && v.gain == 0.0f) continue;   // silent voice: skip it

    // Control rate: once per block
    const float target = v.amp_env.process() * v.velocity * level_;
    const float fenv = v.filter_env.process();
    const float cutoff = cutoff_ + key_track_ * (v.pitch - 60.0f) + env_amount_ * fenv;
    v.filter.set_normalized(dsp::note_to_hz(cutoff) * kInvSampleRate, resonance_);
    v.osc.set_wave(wave_position(v), scan_);

    // Audio rate
    v.osc.render(osc, audio::kBlockSize);
    float gain = v.gain;
    const float step = (target - gain) * kInvBlock;
    for (size_t i = 0; i < audio::kBlockSize; ++i) {
      gain += step;
      out.l[i] += v.filter.process_lowpass(osc[i]) * gain;
    }
    v.gain = target;
  }

  for (size_t i = 0; i < audio::kBlockSize; ++i) out.r[i] = out.l[i];
}

template class PolyGroup<SawOsc>;
template class PolyGroup<WavetableOsc>;

void AUDIO_HOT(SawGroup::render)(Bus& out) { render_block(out); }
void AUDIO_HOT(WavetableGroup::render)(Bus& out) { render_block(out); }

}  // namespace synth
