// Host unit tests for the DSP building blocks. Exit code 0 when all pass.

#include <cmath>
#include <cstdio>

#include "check.h"
#include "config/audio_config.h"
#include "dsp/fast_math.h"
#include "synth/adsr.h"
#include "synth/svf.h"

namespace {

using test::check;

constexpr float kFs = static_cast<float>(audio::kSampleRate);
constexpr double kPi = 3.14159265358979;

// Steady-state gain of the low pass for a sine at `hz`
double svf_gain(float cutoff, float resonance, float hz) {
  synth::Svf f;
  f.set(cutoff, resonance, kFs);
  double peak = 0.0;
  for (int i = 0; i < 48000; ++i) {
    const float y = f.process_lowpass(static_cast<float>(std::sin(2 * kPi * hz * i / kFs)));
    if (i > 24000) peak = std::fmax(peak, std::fabs(y));
  }
  return peak;
}

double db(double x) { return 20.0 * std::log10(x); }

}  // namespace

int main() {
  // fast_exp2 against exp2 over the pitch range used
  double worst_cents = 0.0;
  for (float x = -12.0f; x <= 12.0f; x += 0.001f) {
    worst_cents = std::fmax(worst_cents, std::fabs(1200.0 * std::log2(dsp::fast_exp2(x) / std::exp2(x))));
  }
  check(worst_cents < 0.05, "fast_exp2: worst error over +/-12 octaves (cents)", worst_cents);

  // Low pass at Q = 0.707 (k = sqrt 2): -3 dB at the cutoff, -24 dB two octaves up
  const float butterworth = (2.0f - std::sqrt(2.0f)) / 1.98f;
  const double at_fc = db(svf_gain(1000.0f, butterworth, 1000.0f));
  check(std::fabs(at_fc + 3.01) < 0.2, "svf: gain at cutoff 1 kHz, Q 0.707 (dB, expect -3.0)", at_fc);
  const double two_oct = db(svf_gain(1000.0f, butterworth, 4000.0f));
  check(std::fabs(two_oct + 24.0) < 1.5, "svf: gain at 4 kHz, 2 octaves up (dB, expect ~-24)", two_oct);
  const double pass = db(svf_gain(1000.0f, butterworth, 100.0f));
  check(std::fabs(pass) < 0.1, "svf: gain at 100 Hz, pass band (dB, expect 0)", pass);

  // Full resonance stays bounded (Q = 50: peak gain ~ +34 dB) and never blows up
  synth::Svf f;
  f.set(1000.0f, 1.0f, kFs);
  double peak = 0.0;
  bool finite = true;
  for (int i = 0; i < 480000; ++i) {
    const float y = f.process_lowpass(i == 0 ? 1.0f : 0.0f);   // impulse, 10 s of ringing
    finite = finite && std::isfinite(y);
    peak = std::fmax(peak, std::fabs(y));
  }
  check(finite && peak < 1.0, "svf: impulse at full resonance, 10 s (peak, bounded)", peak);

  // Envelope timing: release of 0.3 s gets below -80 dB in roughly that time
  synth::Adsr env;
  env.set_attack(0.005f);
  env.set_decay(0.1f);
  env.set_sustain(0.5f);
  env.set_release(0.3f);
  env.gate_on();
  int blocks = 0;
  while (env.process() < 1.0f && blocks < 10000) ++blocks;
  const double attack_s = blocks * audio::kBlockSize / kFs;
  check(attack_s > 0.004 && attack_s < 0.0065, "adsr: attack 5 ms, time to reach 1.0 (s)", attack_s);
  for (int i = 0; i < 3000; ++i) env.process();   // 1 s: well into sustain
  check(std::fabs(env.level() - 0.5) < 1e-6, "adsr: sustain level after decay", env.level());
  env.gate_off();
  blocks = 0;
  while (env.level() > 0.5f * 0.01f && blocks < 100000) { env.process(); ++blocks; }
  const double release_s = blocks * audio::kBlockSize / kFs;
  check(release_s > 0.28 && release_s < 0.32, "adsr: release 0.3 s, time to fall 40 dB (s)", release_s);
  while (!env.idle() && blocks < 100000) env.process();
  check(env.idle() && env.level() == 0.0f, "adsr: idle and exactly 0 after release", env.level());

  return test::finish("dsp");
}
