// Host tests for the 3rd Wave-style voice's building blocks: ladder, SVF
// multimode, wave envelope (MSEG), LFO, mod matrix, fast sin/tanh.

#include <cmath>
#include <vector>

#include "check.h"
#include "config/audio_config.h"
#include "dsp/fast_math.h"
#include "dsp/halfband.h"
#include "synth/ladder.h"
#include "synth/lfo.h"
#include "synth/mod_matrix.h"
#include "synth/mseg.h"
#include "synth/svf.h"

namespace {

using test::check;
constexpr double kPi = 3.14159265358979;
constexpr float kFs = 48000.0f;

double db(double x) { return 20.0 * std::log10(x); }

// Steady-state gain for a sine at `hz` through process(x)
template <class F>
double gain_at(float hz, float amplitude, F process) {
  double peak = 0.0;
  for (int i = 0; i < 48000; ++i) {
    const float y = process(amplitude * static_cast<float>(std::sin(2 * kPi * hz * i / kFs)));
    if (i > 24000) peak = std::fmax(peak, std::fabs(y));
  }
  return peak / amplitude;
}

double ladder_gain(float cutoff_hz, float res, float comp, float hz) {
  synth::Ladder f;
  f.set(cutoff_hz / kFs, res, 1.0f, comp);
  return gain_at(hz, 0.02f, [&](float x) { return f.process(x); });   // small: linear region
}

double svf_gain(float morph, bool bp, float res, float hz) {
  synth::Svf f;
  f.set_normalized(1000.0f / kFs, res);
  f.set_mode(morph, bp);
  return gain_at(hz, 0.5f, [&](float x) { return f.process_multimode(x); });
}

void fast_math_tests() {
  double sin_err = 0.0, tanh_err = 0.0;
  bool monotonic = true;
  float last = -2.0f;
  for (int i = -4000; i <= 4000; ++i) {
    const double p = i / 1000.0;
    sin_err = std::fmax(sin_err, std::fabs(dsp::fast_sin_cycles(static_cast<float>(p)) - std::sin(2 * kPi * p)));
    const float x = static_cast<float>(i / 1000.0);
    const float t = dsp::fast_tanh(x);
    tanh_err = std::fmax(tanh_err, std::fabs(t - std::tanh(x)));
    monotonic = monotonic && t >= last - 1e-6f;   // float rounding near the top
    last = t;
  }
  check(sin_err < 0.002, "fast_sin_cycles: max error over 8 cycles", sin_err);
  check(tanh_err < 0.03 && monotonic, "fast_tanh: max error -4..4, monotonic", tanh_err);
}

void ladder_tests() {
  check(std::fabs(ladder_gain(1000, 0, 0, 50) - 1.0) < 0.01, "ladder: pass band gain, no resonance", ladder_gain(1000, 0, 0, 50));
  const double k = 4.2 * 0.5;
  const double loss = ladder_gain(1000, 0.5, 0, 50);
  check(std::fabs(loss - 1.0 / (1.0 + k)) < 0.01, "ladder: pass band 1/(1+k) at res 0.5 (expect 0.323)", loss);
  const double comp = ladder_gain(1000, 0.5, 1, 50);
  check(std::fabs(comp - 1.0) < 0.02, "ladder: full compensation restores the bass", comp);
  const double at_fc = db(ladder_gain(1000, 0, 0, 1000));
  check(std::fabs(at_fc + 12.0) < 0.5, "ladder: -12 dB at cutoff (4 x -3 dB)", at_fc);
  const double two_oct = db(ladder_gain(1000, 0, 0, 4000));
  check(std::fabs(two_oct + 49.2) < 2.0, "ladder: 2 octaves up, 4-pole (expect ~-49 dB)", two_oct);

  // Self-oscillation: ping, then silence; it keeps ringing near the cutoff, bounded
  synth::Ladder f;
  f.set(1000.0f / kFs, 1.0f, 1.0f, 0.0f);
  double peak = 0.0;
  int crossings = 0;
  float prev = 0.0f;
  for (int i = 0; i < 96000; ++i) {
    const float y = f.process(i == 0 ? 1.0f : 0.0f);
    if (i >= 48000) {
      peak = std::fmax(peak, std::fabs(y));
      if (prev < 0.0f && y >= 0.0f) ++crossings;
    }
    prev = y;
  }
  check(peak > 0.05 && peak < 1.5, "ladder: self-oscillates at full resonance, bounded (peak)", peak);
  check(std::fabs(crossings - 1000) < 100, "ladder: oscillation near the cutoff (Hz)", crossings);

  // Hammered: full resonance, heavy drive, loud square input
  synth::Ladder g;
  g.set(300.0f / kFs, 1.0f, 8.0f, 1.0f);
  bool finite = true;
  double loud = 0.0;
  for (int i = 0; i < 480000; ++i) {
    const float y = g.process((i / 80) % 2 ? 4.0f : -4.0f);
    finite = finite && std::isfinite(y);
    loud = std::fmax(loud, std::fabs(y));
  }
  check(finite && loud < 10.0, "ladder: drive 8, res 1, +/-4 input for 10 s (peak, bounded)", loud);
}

void halfband_tests() {
  // Up then down: a 1 kHz sine comes back at the same level
  dsp::Upsampler2x up;
  dsp::Downsampler2x down;
  double peak = 0.0;
  for (int i = 0; i < 48000; ++i) {
    float u[2];
    up.process(static_cast<float>(std::sin(2 * kPi * 1000.0 * i / kFs)), u);
    const float y = down.process(u[0], u[1]);
    if (i > 4800) peak = std::fmax(peak, std::fabs(y));
  }
  check(std::fabs(db(peak)) < 0.01, "halfband: 1 kHz up + down, gain (dB)", db(peak));

  // Downsampler: a 30 kHz tone at 96 kHz would fold to 18 kHz; it must not get through
  dsp::Downsampler2x d2;
  double leak = 0.0;
  for (int i = 0; i < 48000; ++i) {
    const float a = static_cast<float>(std::sin(2 * kPi * 30000.0 * (2 * i) / 96000.0));
    const float b = static_cast<float>(std::sin(2 * kPi * 30000.0 * (2 * i + 1) / 96000.0));
    const float y = d2.process(a, b);
    if (i > 4800) leak = std::fmax(leak, std::fabs(y));
  }
  check(db(leak) < -85.0, "halfband: 30 kHz at 96k, leak after decimation (dB)", db(leak));

  // Upsampler: a 10 kHz input must not leave an image at 48 - 10 = 38 kHz (measured by
  // demodulating the 96k stream at 38 kHz against the 10 kHz component)
  dsp::Upsampler2x u2;
  double re = 0.0, im = 0.0, ref_re = 0.0, ref_im = 0.0;
  for (int i = 0; i < 48000; ++i) {
    float u[2];
    u2.process(static_cast<float>(std::sin(2 * kPi * 10000.0 * i / kFs)), u);
    if (i < 4800) continue;
    for (int j = 0; j < 2; ++j) {
      const double t = (2.0 * i + j) / 96000.0;
      re += u[j] * std::cos(2 * kPi * 38000.0 * t);
      im += u[j] * std::sin(2 * kPi * 38000.0 * t);
      ref_re += u[j] * std::cos(2 * kPi * 10000.0 * t);
      ref_im += u[j] * std::sin(2 * kPi * 10000.0 * t);
    }
  }
  const double image = db(std::hypot(re, im) / std::hypot(ref_re, ref_im));
  check(image < -85.0, "halfband: upsampling image of 10 kHz at 38 kHz (dB)", image);
}

// Block processing must give the per-sample result exactly
void block_equivalence_tests() {
  std::vector<float> in(4800);
  for (size_t i = 0; i < in.size(); ++i) {
    in[i] = 0.8f * static_cast<float>(std::sin(i * 0.05) + 0.5 * std::sin(i * 0.31));
  }
  auto compare = [&](const char* what, auto per_sample, auto per_block) {
    double worst = 0.0;
    for (size_t b = 0; b < in.size(); b += 16) {
      float blk[16];
      for (int i = 0; i < 16; ++i) blk[i] = in[b + i];
      per_block(blk);
      for (int i = 0; i < 16; ++i) worst = std::fmax(worst, std::fabs(blk[i] - per_sample(in[b + i])));
    }
    check(worst == 0.0, what, worst);
  };
  {
    synth::Ladder a, b;
    a.set(0.02f, 0.9f, 3.0f, 0.5f);
    b.set(0.02f, 0.9f, 3.0f, 0.5f);
    compare("block = per sample: ladder", [&](float x) { return a.process(x); },
            [&](float* x) { b.process_block(x, 16); });
  }
  {
    synth::OversampledLadder a, b;
    a.set(0.02f, 0.9f, 3.0f, 0.5f);
    b.set(0.02f, 0.9f, 3.0f, 0.5f);
    compare("block = per sample: 2x ladder (up, ladder, down)", [&](float x) { return a.process(x); },
            [&](float* x) { float o[16]; b.process_block(x, o, 16); for (int i = 0; i < 16; ++i) x[i] = o[i]; });
  }
  {
    synth::Svf a, b;
    a.set_normalized(0.03f, 0.7f); a.set_mode(0.3f, false);
    b.set_normalized(0.03f, 0.7f); b.set_mode(0.3f, false);
    compare("block = per sample: SVF multimode", [&](float x) { return a.process_multimode(x); },
            [&](float* x) { b.process_multimode_block(x, 16); });
  }
}

void soft_clip_tests() {
  check(std::fabs(dsp::soft_clip(1.5f) - 1.0f) < 1e-6 && dsp::soft_clip(5.0f) == 1.0f &&
            dsp::soft_clip(-5.0f) == -1.0f, "soft_clip: reaches exactly +/-1 at 1.5, flat beyond", dsp::soft_clip(1.5f));
  const double slope0 = (dsp::soft_clip(1e-3f) - dsp::soft_clip(-1e-3f)) / 2e-3;
  const double slope_top = (dsp::soft_clip(1.5f) - dsp::soft_clip(1.499f)) / 1e-3;
  check(std::fabs(slope0 - 1.0) < 1e-3 && slope_top < 2e-3, "soft_clip: slope 1 at 0, ~0 at the clip point", slope_top);
}

void ladder2x_tests() {
  synth::OversampledLadder f;
  f.set(1000.0f / kFs, 0.0f, 1.0f, 0.0f);
  const double at_fc = db(gain_at(1000.0f, 0.02f, [&](float x) { return f.process(x); }));
  check(std::fabs(at_fc + 12.0) < 0.5, "ladder 2x: -12 dB at cutoff", at_fc);
  synth::OversampledLadder g;
  g.set(1000.0f / kFs, 0.0f, 1.0f, 0.0f);
  const double pass = gain_at(50.0f, 0.02f, [&](float x) { return g.process(x); });
  check(std::fabs(pass - 1.0) < 0.01, "ladder 2x: pass band gain", pass);
}

void svf_mode_tests() {
  const double notch = db(svf_gain(0.5f, false, 0.0f, 1000));
  check(notch < -30.0, "svf: notch at cutoff (dB)", notch);
  const double hp_low = db(svf_gain(1.0f, false, 0.0f, 100));
  check(std::fabs(hp_low + 40.0) < 2.0, "svf: high pass a decade below, 12 dB/oct (expect -40)", hp_low);
  const double hp_high = db(svf_gain(1.0f, false, 0.0f, 10000));
  check(std::fabs(hp_high) < 0.5, "svf: high pass a decade above (expect 0 dB)", hp_high);
  const double bp_peak = db(svf_gain(0.0f, true, 0.5f, 1000));
  check(std::fabs(bp_peak) < 0.3, "svf: band pass normalised, 0 dB at the peak", bp_peak);
  const double bp_off = db(svf_gain(0.0f, true, 0.5f, 100));
  check(bp_off < -15.0, "svf: band pass a decade below (dB)", bp_off);
}

void mseg_tests() {
  constexpr float kBlock = static_cast<float>(audio::kBlockSize) / audio::kSampleRate;
  synth::Mseg e;
  e.set_stage(0, 1.0f, 0.1f);
  for (int s = 1; s < 5; ++s) e.set_stage(s, 1.0f, 0.0f);
  e.set_stage(5, 0.0f, 0.2f);
  e.gate_on();
  const int half = static_cast<int>(0.05f / kBlock);
  for (int i = 0; i < half; ++i) e.process();
  check(std::fabs(e.level() - 0.5) < 0.02, "mseg: halfway through a 0.1 s rise (expect 0.5)", e.level());
  for (int i = 0; i < 3000; ++i) e.process();
  check(e.level() == 1.0f && !e.idle(), "mseg: holds at the end of stage 4", e.level());
  e.gate_off();
  int blocks = 0;
  while (!e.idle() && blocks < 100000) { e.process(); ++blocks; }
  check(std::fabs(blocks * kBlock - 0.2) < 0.002, "mseg: release over 0.2 s, then idle (s)", blocks * kBlock);

  // Loop stages 1..2 (down to 0, up to 1, 0.1 s each) while the gate is held
  synth::Mseg l;
  l.set_stage(0, 1.0f, 0.0f);
  l.set_stage(1, 0.0f, 0.1f);
  l.set_stage(2, 1.0f, 0.1f);
  l.set_loop(1, 2);
  l.gate_on();
  int rises = 0;
  float prev = 1.0f;
  for (int i = 0; i < static_cast<int>(2.0f / kBlock); ++i) {
    const float v = l.process();
    if (prev < 0.5f && v >= 0.5f) ++rises;
    prev = v;
  }
  check(rises >= 9 && rises <= 10, "mseg: loop of 0.2 s, rises in 2 s (expect ~10)", rises);
}

void lfo_tests() {
  auto count_cycles = [](synth::Lfo::Shape shape, float hz, float octaves, double* lo, double* hi) {
    synth::Lfo lfo;
    lfo.set_rate(hz);
    lfo.set_shape(shape);
    int rises = 0;
    float prev = 0.0f;
    *lo = 1e9; *hi = -1e9;
    for (int i = 0; i < 30000; ++i) {   // 10 s of blocks
      const float v = lfo.process(octaves);
      *lo = std::fmin(*lo, v);
      *hi = std::fmax(*hi, v);
      if (prev < 0.0f && v >= 0.0f) ++rises;
      prev = v;
    }
    return rises;
  };
  double lo, hi;
  const int sine = count_cycles(synth::Lfo::Shape::kSine, 2.0f, 0.0f, &lo, &hi);
  check(std::abs(sine - 20) <= 1 && lo >= -1.001 && hi <= 1.001, "lfo: 2 Hz sine, cycles in 10 s, within -1..1", sine);
  const int faster = count_cycles(synth::Lfo::Shape::kSine, 2.0f, 1.0f, &lo, &hi);
  check(std::abs(faster - 40) <= 1, "lfo: +1 octave of rate modulation doubles it", faster);
  count_cycles(synth::Lfo::Shape::kSampleHold, 2.0f, 0.0f, &lo, &hi);
  check(lo < -0.5 && hi > 0.5 && lo >= -1.0 && hi <= 1.0, "lfo: sample & hold spans -1..1 (range)", hi - lo);
}

void matrix_tests() {
  synth::ModMatrix m;
  m.set(0, synth::mod::kSrcLfo1, synth::mod::kDstPitch1, 2.0f);
  m.set(1, synth::mod::kSrcVelocity, synth::mod::kDstPitch1, 1.0f);
  m.set(2, synth::mod::kSrcNone, synth::mod::kDstPan, 5.0f);   // no source: ignored
  float src[synth::mod::kNumSources] = {};
  src[synth::mod::kSrcLfo1] = 0.5f;
  src[synth::mod::kSrcVelocity] = 0.25f;
  float dst[synth::mod::kNumDestinations] = {};
  m.apply(src, dst);
  check(std::fabs(dst[synth::mod::kDstPitch1] - 1.25f) < 1e-6, "matrix: slots add up on a destination (expect 1.25)", dst[synth::mod::kDstPitch1]);
  check(dst[synth::mod::kDstPan] == 0.0f, "matrix: a slot without a source does nothing", dst[synth::mod::kDstPan]);
}

}  // namespace

int main() {
  fast_math_tests();
  ladder_tests();
  halfband_tests();
  ladder2x_tests();
  block_equivalence_tests();
  soft_clip_tests();
  svf_mode_tests();
  mseg_tests();
  lfo_tests();
  matrix_tests();
  return test::finish("voice");
}
