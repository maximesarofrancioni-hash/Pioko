#include "dsp/interpolate.h"

#include "config/platform.h"

namespace dsp {
namespace {

constexpr int32_t q15(double w) {
  const double v = w * 32768.0;
  const int32_t r = v >= 0.0 ? static_cast<int32_t>(v + 0.5) : -static_cast<int32_t>(-v + 0.5);
  return r > 32767 ? 32767 : r < -32768 ? -32768 : r;
}

constexpr uint32_t pack(int32_t lo, int32_t hi) {
  return (static_cast<uint32_t>(lo) & 0xffffu) | (static_cast<uint32_t>(hi) << 16);
}

// The weights at the centre of each step, so the position error is +/- half a step
constexpr std::array<Lagrange6Q15::Weights, Lagrange6Q15::kSteps> make_table() {
  std::array<Lagrange6Q15::Weights, Lagrange6Q15::kSteps> t{};
  for (int k = 0; k < Lagrange6Q15::kSteps; ++k) {
    const double x = (k + 0.5) / Lagrange6Q15::kSteps;
    double w[6] = {};
    for (int j = 0; j < 6; ++j) {
      double p = 1.0;
      for (int m = 0; m < 6; ++m) {
        if (m != j) p *= (x - (m - 2)) / static_cast<double>(j - m);
      }
      w[j] = p;
    }
    t[k] = {pack(q15(w[0]), q15(w[1])), pack(q15(w[2]), q15(w[3])), pack(q15(w[4]), q15(w[5]))};
  }
  return t;
}

}  // namespace

// In SRAM: read for every wavetable sample (24 KB)
AUDIO_RAM_DATA("lagrange") const std::array<Lagrange6Q15::Weights, Lagrange6Q15::kSteps> Lagrange6Q15::kTable =
    make_table();

}  // namespace dsp
