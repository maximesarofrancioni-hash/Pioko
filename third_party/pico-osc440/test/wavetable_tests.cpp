// Host tests for the wavetable oscillator. Exit code 0 when all pass.

#include <cmath>
#include <vector>

#include "check.h"
#include "synth/wavetable_osc.h"

namespace {

using synth::WaveScan;
using synth::WavetableOsc;
using test::check;

constexpr size_t kN = 16;

// One settled block: the first block ramps the position from where it was
// (0) to the target; the second is played entirely at the target.
std::vector<float> settled_block(float pitch, float position, WaveScan scan) {
  WavetableOsc osc;
  osc.set_pitch(pitch);
  osc.set_wave(position, scan);
  std::vector<float> a(kN), b(kN);
  osc.render(a.data(), kN);
  osc.render(b.data(), kN);
  return b;
}

double max_diff(const std::vector<float>& a, const std::vector<float>& b) {
  double d = 0.0;
  for (size_t i = 0; i < a.size(); ++i) d = std::fmax(d, std::fabs(a[i] - b[i]));
  return d;
}

// Largest jump between consecutive samples
double max_step(const std::vector<float>& x) {
  double d = 0.0;
  for (size_t i = 1; i < x.size(); ++i) d = std::fmax(d, std::fabs(x[i] - x[i - 1]));
  return d;
}

std::vector<float> sweep(float pitch, WaveScan scan, float seconds) {
  WavetableOsc osc;
  osc.set_pitch(pitch);
  const size_t blocks = static_cast<size_t>(seconds * 48000 / kN);
  std::vector<float> out(blocks * kN);
  for (size_t b = 0; b < blocks; ++b) {
    osc.set_wave(63.0f * (b + 1) / blocks, scan);
    osc.render(&out[b * kN], kN);
  }
  return out;
}

}  // namespace

int main() {
  // Mip level: top harmonic below 20 kHz
  {
    WavetableOsc osc;
    osc.set_pitch(33.0f);   // 55 Hz: 64 harmonics reach 3.5 kHz
    check(osc.level() == 0, "level at 55 Hz (64 harmonics fit: expect 0)", osc.level());
    osc.set_pitch(69.0f);   // 440 Hz: 64 x 440 = 28 kHz, too high; 32 x 440 = 14 kHz fits
    check(osc.level() == 1, "level at 440 Hz (expect 1: 32 harmonics)", osc.level());
    osc.set_pitch(120.0f);  // 8.4 kHz: only 2 harmonics fit (16.7 kHz)
    check(osc.level() == 5, "level at 8.4 kHz (expect 5: 2 harmonics)", osc.level());
  }

  // Smooth scan is an exact crossfade of the two neighbouring waves
  {
    const auto w10 = settled_block(45.0f, 10.0f, WaveScan::kSmooth);
    const auto w11 = settled_block(45.0f, 11.0f, WaveScan::kSmooth);
    const auto mid = settled_block(45.0f, 10.25f, WaveScan::kSmooth);
    std::vector<float> expect(kN);
    for (size_t i = 0; i < kN; ++i) expect[i] = 0.75f * w10[i] + 0.25f * w11[i];
    check(max_diff(mid, expect) < 1e-5, "smooth: position 10.25 = 0.75 w10 + 0.25 w11 (max err)",
          max_diff(mid, expect));
    const auto last = settled_block(45.0f, 63.0f, WaveScan::kSmooth);
    const auto last_stepped = settled_block(45.0f, 63.0f, WaveScan::kStepped);
    check(max_diff(last, last_stepped) < 1e-6, "smooth: position 63 is exactly the last wave",
          max_diff(last, last_stepped));
  }

  // Stepped scan plays the nearest wave only
  {
    const auto w10 = settled_block(45.0f, 10.0f, WaveScan::kStepped);
    const auto w11 = settled_block(45.0f, 11.0f, WaveScan::kStepped);
    check(max_diff(settled_block(45.0f, 10.4f, WaveScan::kStepped), w10) == 0.0,
          "stepped: position 10.4 plays wave 10 exactly", 0.0);
    check(max_diff(settled_block(45.0f, 10.6f, WaveScan::kStepped), w11) == 0.0,
          "stepped: position 10.6 plays wave 11 exactly", 0.0);
  }

  // A full-table sweep in smooth mode adds no jumps beyond those of the waves
  // themselves; stepped mode does jump (that is its character), reported only
  {
    double static_step = 0.0;
    for (int w = 0; w < 64; ++w) {
      WavetableOsc osc;
      osc.set_pitch(45.0f);
      osc.set_wave(static_cast<float>(w), WaveScan::kSmooth);
      // A full second: steep edges get sampled at many phases, as in the sweep
      std::vector<float> x(48000);
      osc.render(x.data(), x.size());
      static_step = std::fmax(static_step, max_step(x));
    }
    const double smooth = max_step(sweep(45.0f, WaveScan::kSmooth, 1.0f));
    check(smooth <= static_step * 1.02, "smooth sweep: largest sample step / static waves'",
          smooth / static_step);
    const double stepped = max_step(sweep(45.0f, WaveScan::kStepped, 1.0f));
    check(true, "stepped sweep: largest sample step / static waves' (info)", stepped / static_step);
  }

  // Level stays consistent across waves (each normalised to the same peak)
  {
    double lo = 1e9, hi = 0.0;
    for (int w = 0; w < 64; ++w) {
      WavetableOsc osc;
      osc.set_pitch(33.0f);
      osc.set_wave(static_cast<float>(w), WaveScan::kStepped);
      std::vector<float> x(4800);
      osc.render(x.data(), x.size());
      double peak = 0.0;
      for (float v : x) peak = std::fmax(peak, std::fabs(v));
      lo = std::fmin(lo, peak);
      hi = std::fmax(hi, peak);
    }
    check(lo > 0.95 && hi < 0.99, "peak level across all 64 waves at 55 Hz (lowest)", lo);
  }

  return test::finish("wavetable");
}
