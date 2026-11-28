// Host tests for the 3rd Wave-style additions: oscillator modes, FM, DADSR,
// LFO shapes and delay, and the audio-rate matrix inside the real voice.

#include <cmath>
#include <vector>

#include "check.h"
#include "synth/dadsr.h"
#include "synth/lfo.h"
#include "synth/oscillator.h"
#include "synth/triple_wave_group.h"
#include "synth/wavetable_plaits.h"

namespace {

using synth::Oscillator;
using test::check;
constexpr double kPi = 3.14159265358979;
constexpr float kBlock = 16.0f / 48000.0f;

std::vector<float> render(Oscillator& o, size_t n, const float* fm = nullptr) {
  std::vector<float> x(n);
  for (size_t b = 0; b < n; b += 16) o.render(&x[b], 16, fm ? fm + b : nullptr, nullptr, nullptr);
  return x;
}

int rising_crossings(const std::vector<float>& x, size_t from) {
  int n = 0;
  for (size_t i = from + 1; i < x.size(); ++i) n += x[i - 1] < 0.0f && x[i] >= 0.0f;
  return n;
}

// Magnitude of the component at `hz` (normalised so a full-scale sine reads 1)
double tone(const std::vector<float>& x, double hz, size_t from) {
  double re = 0.0, im = 0.0;
  for (size_t i = from; i < x.size(); ++i) {
    re += x[i] * std::cos(2 * kPi * hz * i / 48000.0);
    im += x[i] * std::sin(2 * kPi * hz * i / 48000.0);
  }
  return 2.0 * std::hypot(re, im) / static_cast<double>(x.size() - from);
}

void oscillator_tests() {
  {
    Oscillator o;
    o.set_mode(Oscillator::Mode::kSaw);
    o.set_pitch(45.0f);
    auto x = render(o, 48000);
    double peak = 0.0;
    for (size_t i = 4800; i < x.size(); ++i) peak = std::fmax(peak, std::fabs(x[i]));
    check(peak > 1.0 && peak < 1.25, "osc saw: +/-1 ramp (peak with Gibbs overshoot)", peak);
  }
  {
    Oscillator o;
    o.set_mode(Oscillator::Mode::kPulse);
    o.set_pulse_width(0.1f);
    o.set_pitch(45.0f);
    auto x = render(o, 48000 + 32);
    o.set_pulse_width(0.1f);
    double mean = 0.0;
    for (size_t i = 4800; i < 4800 + 43636; ++i) mean += x[i];   // 100 cycles of 110 Hz
    mean /= 43636.0;
    check(std::fabs(mean) < 0.01, "osc pulse 10%: no DC (mean over 100 cycles)", mean);
  }
  {
    Oscillator o;
    o.set_mode(Oscillator::Mode::kSine);
    o.set_pitch(69.0f);
    std::vector<float> fm(48000, 0.5f);   // frequency x 1.5
    auto x = render(o, 48000, fm.data());
    check(std::abs(rising_crossings(x, 0) - 660) <= 1, "osc FM: constant +0.5 plays 440 x 1.5 Hz (cycles)",
          rising_crossings(x, 0));
  }
  {
    Oscillator o;
    o.set_mode(Oscillator::Mode::kSupersaw);
    o.set_pulse_width(0.5f);
    o.set_pitch(57.0f);
    auto x = render(o, 48000);
    double peak = 0.0;
    for (float v : x) peak = std::fmax(peak, std::fabs(v));
    check(peak < 1.3 && tone(x, 220.0, 4800) > 0.15, "osc supersaw: bounded, strong at its pitch (peak)", peak);
  }
  {
    // Coloured noise keeps its level across the colour range
    double lo = 1e9, hi = 0.0;
    for (float pw : {0.1f, 0.4f, 0.9f}) {
      Oscillator o;
      o.set_mode(Oscillator::Mode::kColoredNoise);
      o.set_pulse_width(pw);
      auto x = render(o, 48000);
      o.set_pulse_width(pw);
      x = render(o, 48000);
      double rms = 0.0;
      for (float v : x) rms += v * v;
      rms = std::sqrt(rms / x.size());
      lo = std::fmin(lo, rms);
      hi = std::fmax(hi, rms);
    }
    check(20 * std::log10(hi / lo) < 6.0, "osc coloured noise: level spread over colour (dB)", 20 * std::log10(hi / lo));
  }
  {
    // Plaits' bank: every wave plays at our normalised level
    double lo = 1e9, hi = 0.0;
    for (int w = 0; w < 64; ++w) {
      Oscillator o;
      o.set_table(&synth::kPlaitsWavetable);
      o.set_wave(static_cast<float>(w), synth::WaveScan::kStepped);
      o.set_pitch(33.0f);
      auto x = render(o, 4800);
      double peak = 0.0;
      for (size_t i = 32; i < x.size(); ++i) peak = std::fmax(peak, std::fabs(x[i]));
      lo = std::fmin(lo, peak);
      hi = std::fmax(hi, peak);
    }
    // Each wave's scale is set by the true peak of its loudest mip level (so
    // loudness does not jump with pitch, and the interpolator never overshoots 1)
    check(lo > 0.85 && hi < 1.0, "Plaits bank: peak level of all 64 waves (lowest)", lo);
    check(hi < 1.0, "Plaits bank: peak level of all 64 waves (highest)", hi);
  }
}

void dadsr_tests() {
  auto seconds_until = [](synth::Dadsr& e, auto done) {
    int blocks = 0;
    while (!done(e) && blocks < 1000000) { e.process(); ++blocks; }
    return blocks * kBlock;
  };
  synth::Dadsr e;
  e.set_delay(0.1f);
  e.set_attack(0.05f);
  e.set_decay(0.2f);
  e.set_sustain(0.5f);
  e.set_release(0.3f);
  e.gate_on();
  const double delay = seconds_until(e, [](synth::Dadsr& x) { return x.level() > 0.0f; });
  check(std::fabs(delay - 0.1) < 0.001, "dadsr: delay 0.1 s before the attack starts (s)", delay);
  const double attack = seconds_until(e, [](synth::Dadsr& x) { return x.level() >= 1.0f; });
  check(std::fabs(attack - 0.05) < 0.002, "dadsr exp: attack 0.05 s to the top (s)", attack);

  synth::Dadsr p;
  p.set_mode(synth::Dadsr::Mode::kPpg);
  p.set_attack(0.1f);
  p.set_decay(0.1f);
  p.set_sustain(0.5f);
  p.set_release(0.2f);
  p.gate_on();
  for (int i = 0; i < static_cast<int>(0.05f / kBlock); ++i) p.process();
  check(std::fabs(p.level() - 0.5) < 0.01, "dadsr PPG: linear attack, halfway at half time", p.level());
  for (int i = 0; i < 3000; ++i) p.process();
  check(p.level() == 0.5f, "dadsr PPG: sustain", p.level());
  p.gate_off();
  const double release = seconds_until(p, [](synth::Dadsr& x) { return x.idle(); });
  check(std::fabs(release - 0.2) < 0.002, "dadsr PPG: linear release over 0.2 s to idle (s)", release);
}

void lfo_tests() {
  bool in_range = true;
  for (int s = 0; s < static_cast<int>(synth::Lfo::Shape::kCount); ++s) {
    synth::Lfo l;
    l.set_shape(static_cast<synth::Lfo::Shape>(s));
    l.set_rate(3.0f);
    for (int i = 0; i < 30000; ++i) {
      const float v = l.process(0.0f);
      in_range = in_range && v >= -1.0001f && v <= 1.0001f;
    }
  }
  check(in_range, "lfo: all 10 shapes stay within -1..1", static_cast<int>(synth::Lfo::Shape::kCount));

  synth::Lfo d;
  d.set_shape(synth::Lfo::Shape::kSquare);
  d.set_rate(20.0f);
  d.set_delay(1.0f);
  d.note_on(true);
  double at_half = 0.0;
  for (int i = 0; i < static_cast<int>(0.5f / kBlock); ++i) at_half = std::fabs(d.process(0.0f));
  check(std::fabs(at_half - 0.5) < 0.01, "lfo: delay fade-in, amplitude at half the delay", at_half);

  synth::Lfo st;
  st.set_shape(synth::Lfo::Shape::kStepped);
  st.set_rate(1.0f);
  std::vector<float> levels;
  for (int i = 0; i < 3000; ++i) {
    const float v = st.process(0.0f);
    bool seen = false;
    for (float x : levels) seen = seen || std::fabs(x - v) < 1e-6f;
    if (!seen) levels.push_back(v);
  }
  check(levels.size() == 8, "lfo: stepped shape has 8 levels", levels.size());
}

// Inside the real voice: oscillators as audio-rate matrix sources
std::vector<float> voice_render(void (*patch)(synth::TripleWaveGroup&)) {
  using G = synth::TripleWaveGroup;
  G g;
  g.set_param(G::kParamLadderCutoff, 135.0f);
  g.set_param(G::kParamLadderResonance, 0.0f);
  g.set_param(G::kParamLadderEnvAmount, 0.0f);
  g.set_param(G::kParamKeyTrack, 0.0f);
  g.set_param(G::env_param(1, G::kEnvSustain), 1.0f);
  g.set_param(G::kParamLevel, 1.0f);
  g.set_param(G::kParamDrive, 1.0f);
  patch(g);
  g.note_on(0, 69.0f, 1.0f);
  synth::Bus bus;
  std::vector<float> out;
  for (int b = 0; b < 6000; ++b) {
    g.render(bus);
    out.insert(out.end(), bus.l, bus.l + 16);
  }
  return out;
}

void audio_matrix_tests() {
  using G = synth::TripleWaveGroup;
  // Ring modulation: osc 1 (sine, 440 Hz, level 0) x osc 2 (sine, +12.5 semitones, level 0)
  auto ring = voice_render([](G& g) {
    for (int o = 0; o < 2; ++o) g.set_param(G::osc_param(o, G::kOscMode), static_cast<float>(Oscillator::Mode::kSine));
    g.set_param(G::osc_param(0, G::kOscLevel), 0.0f);
    g.set_param(G::osc_param(1, G::kOscTune), 12.5f);
    g.set_param(G::matrix_param(0, 0), synth::mod::kSrcOsc1);
    g.set_param(G::matrix_param(0, 1), synth::mod::kDstLevel2);
    g.set_param(G::matrix_param(0, 2), 1.0f);
  });
  const double f2 = 440.0 * std::pow(2.0, 12.5 / 12.0);
  const double sum = tone(ring, f2 + 440.0, 9600), diff = tone(ring, f2 - 440.0, 9600);
  const double leak = std::fmax(tone(ring, 440.0, 9600), tone(ring, f2, 9600));
  check(sum > 0.1 && diff > 0.1 && leak < 0.01 * sum,
        "matrix: osc 1 -> level 2 rings (sum, difference; no carriers)", 20 * std::log10(leak / sum));

  // FM: osc 2 (sine) frequency-modulated by osc 1 (sine, level 0): sidebands at f2 +/- f1
  auto fm = voice_render([](G& g) {
    for (int o = 0; o < 2; ++o) g.set_param(G::osc_param(o, G::kOscMode), static_cast<float>(Oscillator::Mode::kSine));
    g.set_param(G::osc_param(0, G::kOscLevel), 0.0f);
    g.set_param(G::osc_param(0, G::kOscTune), -12.0f);   // modulator 220 Hz
    g.set_param(G::osc_param(1, G::kOscLevel), 1.0f);
    g.set_param(G::osc_param(1, G::kOscFm), 0.5f);        // from osc 1
  });
  const double carrier = tone(fm, 440.0, 9600), side = tone(fm, 660.0, 9600), off = tone(fm, 550.0, 9600);
  check(side > 0.05 * carrier && off < 0.001, "FM: osc 1 -> osc 2 gives sidebands at f2 + f1 (vs carrier)",
        20 * std::log10(side / carrier));
}

}  // namespace

int main() {
  oscillator_tests();
  dadsr_tests();
  lfo_tests();
  audio_matrix_tests();
  return test::finish("osc");
}
