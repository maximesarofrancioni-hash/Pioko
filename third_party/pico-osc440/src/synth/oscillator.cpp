#include "synth/oscillator.h"

#include "config/audio_config.h"
#include "config/platform.h"
#include "dsp/fast_math.h"
#include "dsp/interpolate.h"

namespace synth {
namespace {

constexpr float kMaxHarmonicHz = 20000.0f;
constexpr float kTwo32 = 4294967296.0f;
constexpr float kInvTwo32 = 1.0f / 4294967296.0f;
constexpr float kMaxIncrement = 0.45f * kTwo32;   // keep pitch (and FM peaks) below Nyquist
constexpr float kScale = 1.0f / 32768.0f;

// Supersaw voices: detune (relative to the full spread) and mix weight
constexpr float kSpread[7] = {-1.0f, -0.62f, -0.31f, 0.0f, 0.29f, 0.61f, 0.98f};
constexpr float kWeight[7] = {0.1f, 0.1f, 0.1f, 0.4f, 0.1f, 0.1f, 0.1f};

// Corrections for a discontinuity that happened d samples (0..1) before the
// current sample: polyBLEP for a step of height h, polyBLAMP for a slope change
// of s per sample. `before` goes to the delayed (previous) sample.
struct Correction {
  float before = 0.0f, now = 0.0f;
  void step(float h, float d) {
    before += h * 0.5f * d * d;
    now -= h * 0.5f * (1.0f - d) * (1.0f - d);
  }
  void ramp(float s, float d) {
    const float e = 1.0f - d;
    before += s * (1.0f / 6.0f) * d * d * d;
    now += s * (1.0f / 6.0f) * e * e * e;
  }
};

inline float phase_to_cycles(uint32_t p) { return static_cast<float>(p) * kInvTwo32; }

}  // namespace

void AUDIO_HOT(Oscillator::set_table)(const Wavetable* table) {
  table_ = table;
  level_ = &table_->level[0];
}

void AUDIO_HOT(Oscillator::set_pitch)(float semitones) {
  const float hz = dsp::note_to_hz(semitones);
  const int band = static_cast<int>(dsp::clamp(semitones, 0.0f, 131.0f)) / kSawBandSemitones;
  saw_ = &kSawTables[band];
  increment_ = dsp::clamp(hz * (1.0f / audio::kSampleRate) * kTwo32, 0.0f, kMaxIncrement);
  int l = 0;
  while (l < table_->levels - 1 && table_->level[l].harmonics * hz > kMaxHarmonicHz) ++l;
  level_ = &table_->level[l];
}

void AUDIO_HOT(Oscillator::set_wave)(float position, WaveScan scan) {
  position_target_ = dsp::clamp(position, 0.0f, static_cast<float>(table_->waves - 1));
  scan_ = scan;
}

void AUDIO_HOT(Oscillator::set_pulse_width)(float pw) { pw_target_ = dsp::clamp(pw, 0.02f, 0.98f); }

template <bool kSmooth>
__attribute__((always_inline)) inline float Oscillator::table_sample(uint32_t phase, float pos) const {
  const uint32_t bits = level_->bits;
  const uint32_t stride = Wavetable::stride(bits);
  const uint32_t idx = phase >> (32 - bits);
  const dsp::Lagrange6Q15 interp(phase << bits);   // fixed point: SMLAD on the M33
  constexpr float kScaleQ = kScale / 32768.0f;     // Q15 weights x int16 samples
  if (kSmooth) {
    int w = static_cast<int>(pos);
    if (w > table_->waves - 2) w = table_->waves - 2;
    const float f = pos - static_cast<float>(w);
    const int16_t* w0 = level_->data + w * stride + idx;
    const float a = static_cast<float>(interp(w0));
    const float b = static_cast<float>(interp(w0 + stride));
    return (a + (b - a) * f) * kScaleQ;
  }
  const int w = static_cast<int>(pos + 0.5f);
  return static_cast<float>(interp(level_->data + w * stride + idx)) * kScaleQ;
}

__attribute__((always_inline)) inline float Oscillator::saw_sample(uint32_t phase) const {
  const uint32_t bits = saw_->bits;
  const uint32_t idx = phase >> (32 - bits);
  const float frac = static_cast<float>((phase << bits) >> 8) * (1.0f / 16777216.0f);
  const float a = saw_->data[idx], b = saw_->data[idx + 1];
  return (a + (b - a) * frac) * (kScale / kSawRampAmplitude);
}

template <Oscillator::Shape S>
__attribute__((always_inline)) inline float Oscillator::shape(uint32_t phase, float pos, float pw) const {
  const float p = phase_to_cycles(phase);
  switch (S) {
    case Shape::kTableSmooth: return table_sample<true>(phase, pos);
    case Shape::kTableStepped: return table_sample<false>(phase, pos);
    case Shape::kSine: {
      // Phase distortion: the first half-cycle takes pw of the period
      const float q = p < pw ? 0.5f * p / pw : 0.5f + 0.5f * (p - pw) / (1.0f - pw);
      return dsp::fast_sin_cycles(q);
    }
    case Shape::kTriangle: return p < pw ? -1.0f + 2.0f * p / pw : 1.0f - 2.0f * (p - pw) / (1.0f - pw);
    case Shape::kSaw: return saw_sample(phase);
    // saw(p) - saw(p + pw): high for the last pw of the cycle, no DC
    case Shape::kPulse: return saw_sample(phase) - saw_sample(phase + static_cast<uint32_t>(pw * kTwo32));
  }
  return 0.0f;
}

void AUDIO_HOT(Oscillator::render_noise)(float* out, size_t n, float* sync_out) {
  float pw = pw_;
  const float pw_step = (pw_target_ - pw) / static_cast<float>(n);
  float pending = pending_;
  uint32_t r = random_;
  float lp = noise_lp_;
  const bool coloured = mode_ == Mode::kColoredNoise;
  for (size_t i = 0; i < n; ++i) {
    pw += pw_step;
    r ^= r << 13;
    r ^= r >> 17;
    r ^= r << 5;
    const float white = static_cast<float>(static_cast<int32_t>(r)) * (1.0f / 2147483648.0f);
    float y = white;
    if (coloured) {
      // One-pole low pass, colour set by pw; level held constant
      const float c = 0.002f + 0.998f * pw * pw;
      lp += c * (white - lp);
      y = lp * dsp::clamp(1.0f / (0.05f + 0.95f * pw), 1.0f, 20.0f);
    }
    out[i] = pending;
    pending = y;
    if (sync_out) sync_out[i] = -1.0f;
  }
  random_ = r;
  noise_lp_ = lp;
  pending_ = pending;
  pw_ = pw_target_;
}

void AUDIO_HOT(Oscillator::render_supersaw)(float* out, size_t n, const float* fm, float* sync_out) {
  // Seven band-limited saws spread by pw (up to +/- 0.6 semitone)
  float inc[kSupersawVoices];
  for (int v = 0; v < kSupersawVoices; ++v) {
    inc[v] = increment_ * dsp::fast_exp2(kSpread[v] * 0.05f * pw_target_);
  }
  float pending = pending_;
  for (size_t i = 0; i < n; ++i) {
    const float ratio = fm ? dsp::clamp(1.0f + fm[i], 0.0f, 8.0f) : 1.0f;
    float y = 0.0f;
    float centre_wrap = -1.0f;
    for (int v = 0; v < kSupersawVoices; ++v) {
      const float incf = dsp::clamp(inc[v] * ratio, 0.0f, kMaxIncrement);
      const uint32_t prev = saw_phase_[v];
      const uint32_t now = prev + static_cast<uint32_t>(incf);
      saw_phase_[v] = now;
      if (v == 3 && now < prev) centre_wrap = static_cast<float>(now) / incf;
      y += kWeight[v] * saw_sample(now);
    }
    out[i] = pending;
    pending = y;
    if (sync_out) sync_out[i] = centre_wrap;
  }
  pending_ = pending;
  pw_ = pw_target_;
}

template <Oscillator::Shape S, bool kPlain>
__attribute__((always_inline)) inline void Oscillator::render_shape(float* out, size_t n, const float* fm,
                                                                    const float* sync_in, float* sync_out) {
  const float inv_n = 1.0f / static_cast<float>(n);
  float pos = position_, pw = pw_;
  const float pos_step = (position_target_ - pos) * inv_n;
  const float pw_step = (pw_target_ - pw) * inv_n;
  uint32_t phase = phase_;
  float pending = pending_;
  const float inc = increment_;
  const uint32_t plain_step = static_cast<uint32_t>(inc);

  for (size_t i = 0; i < n; ++i) {
    pos += pos_step;
    pw += pw_step;
    const float incf = (!kPlain && fm) ? dsp::clamp(inc * (1.0f + fm[i]), 0.0f, kMaxIncrement) : inc;
    const uint32_t step = kPlain ? plain_step : static_cast<uint32_t>(incf);
    const uint32_t prev = phase;
    phase = prev + step;

    if (kPlain && S != Shape::kTriangle) {
      // Band-limited by construction, nothing to report: just the shape
      out[i] = pending;
      pending = shape<S>(phase, pos, pw);
      continue;
    }

    float wrap = (phase < prev && step != 0) ? static_cast<float>(phase) / incf : -1.0f;
    Correction c;
    if (!kPlain && sync_in && sync_in[i] >= 0.0f) {
      // Hard sync: the master restarted d samples ago; so does this oscillator
      const float d = sync_in[i];
      const uint32_t at_reset = prev + static_cast<uint32_t>(incf * (1.0f - d));
      c.step(shape<S>(0, pos, pw) - shape<S>(at_reset, pos, pw), d);
      phase = static_cast<uint32_t>(incf * d);
      wrap = d;
    } else if (S == Shape::kTriangle) {
      // The triangle's own corners (the table-based shapes are band-limited already)
      const float inc_c = incf * kInvTwo32;
      const float p1 = phase_to_cycles(prev) + inc_c;   // not wrapped: may pass 1
      const float pw_cross = (p1 >= pw && p1 - inc_c < pw) ? (p1 - pw) / inc_c : -1.0f;
      // Slope breaks at the bottom (wrap) and at the top (pw)
      const float change = (2.0f / pw + 2.0f / (1.0f - pw)) * inc_c;
      if (wrap >= 0.0f) c.ramp(change, wrap);
      if (pw_cross >= 0.0f) c.ramp(-change, pw_cross);
    }

    pending += c.before;
    out[i] = pending;
    pending = shape<S>(phase, pos, pw) + c.now;
    if (!kPlain && sync_out) sync_out[i] = wrap;
  }

  phase_ = phase;
  pending_ = pending;
  position_ = position_target_;
  pw_ = pw_target_;
}

void AUDIO_HOT(Oscillator::render)(float* out, size_t n, const float* fm, const float* sync_in,
                                   float* sync_out) {
  const bool plain = !fm && !sync_in && !sync_out;
#define OSC_RENDER(S)                                                     \
  (plain ? render_shape<S, true>(out, n, fm, sync_in, sync_out)          \
         : render_shape<S, false>(out, n, fm, sync_in, sync_out))
  switch (mode_) {
    case Mode::kWavetable:
      if (scan_ == WaveScan::kSmooth) OSC_RENDER(Shape::kTableSmooth);
      else OSC_RENDER(Shape::kTableStepped);
      return;
    case Mode::kSine: OSC_RENDER(Shape::kSine); return;
    case Mode::kTriangle: OSC_RENDER(Shape::kTriangle); return;
    case Mode::kSaw: OSC_RENDER(Shape::kSaw); return;
    case Mode::kPulse: OSC_RENDER(Shape::kPulse); return;
    case Mode::kSupersaw: render_supersaw(out, n, fm, sync_out); return;
    default: render_noise(out, n, sync_out); return;
  }
#undef OSC_RENDER
}

}  // namespace synth
