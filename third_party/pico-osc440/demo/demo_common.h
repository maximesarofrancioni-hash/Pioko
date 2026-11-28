#pragma once
// Shared by the demo renderers: stand-ins for the master's effects (the master
// firmware does not exist yet) and a 24-bit stereo WAV writer.

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

#include "config/audio_config.h"

namespace demo {

constexpr int kRate = audio::kSampleRate;


class Hall {   // 4-line FDN, Householder feedback, damped
 public:
  Hall() {
    const int lengths[4] = {2473, 3011, 3557, 4153};
    for (int i = 0; i < 4; ++i) {
      line_[i].assign(lengths[i], 0.0f);
      gain_[i] = static_cast<float>(std::pow(10.0, -3.0 * lengths[i] / (2.4 * kRate)));   // RT60 2.4 s
    }
    pre_.assign(kRate / 40, 0.0f);   // 25 ms
  }
  void process(float in, float& l, float& r) {
    const float x = pre_[pre_pos_];
    pre_[pre_pos_] = in;
    pre_pos_ = (pre_pos_ + 1) % pre_.size();
    float y[4], sum = 0.0f;
    for (int i = 0; i < 4; ++i) {
      y[i] = line_[i][pos_[i]];
      lp_[i] += 0.35f * (y[i] - lp_[i]);   // high frequencies decay faster
      sum += lp_[i];
    }
    for (int i = 0; i < 4; ++i) {
      line_[i][pos_[i]] = gain_[i] * (lp_[i] - 0.5f * sum) + x;
      pos_[i] = (pos_[i] + 1) % line_[i].size();
    }
    l = 0.5f * (y[0] + y[2]);
    r = 0.5f * (y[1] + y[3]);
  }

 private:
  std::vector<float> line_[4], pre_;
  size_t pos_[4] = {0, 0, 0, 0}, pre_pos_ = 0;
  float gain_[4] = {}, lp_[4] = {};
};

class PingPong {
 public:
  explicit PingPong(double seconds) : l_(static_cast<size_t>(seconds * kRate), 0.0f), r_(l_) {}
  void process(float in, float& out_l, float& out_r) {
    const float dl = l_[pos_], dr = r_[pos_];
    lp_l_ += 0.3f * (dl - lp_l_);
    lp_r_ += 0.3f * (dr - lp_r_);
    l_[pos_] = in + 0.4f * lp_r_;
    r_[pos_] = 0.4f * lp_l_;
    pos_ = (pos_ + 1) % l_.size();
    out_l = dl;
    out_r = dr;
  }

 private:
  std::vector<float> l_, r_;
  size_t pos_ = 0;
  float lp_l_ = 0.0f, lp_r_ = 0.0f;
};

// Peak-normalises to `peak_db` dBFS, with a 20 ms fade at the end
inline void normalize(std::vector<float>& x, float peak_db) {
  float peak = 0.0f;
  for (float v : x) peak = std::max(peak, std::fabs(v));
  const float g = std::pow(10.0f, peak_db / 20.0f) / peak;
  for (float& v : x) v *= g;
  // 20 ms fade at the end
  const size_t fade = kRate / 50;
  for (size_t i = 0; i < fade && 2 * i < x.size(); ++i) {
    const float k = static_cast<float>(i) / fade;
    x[x.size() - 2 * i - 1] *= k;
    x[x.size() - 2 * i - 2] *= k;
  }
}

inline void write_wav24(const std::string& path, const std::vector<float>& lr) {
  FILE* f = std::fopen(path.c_str(), "wb");
  if (!f) { std::perror(path.c_str()); return; }
  const uint32_t frames = static_cast<uint32_t>(lr.size() / 2), bytes = frames * 6;
  auto u32 = [&](uint32_t v) { std::fwrite(&v, 4, 1, f); };
  auto u16 = [&](uint16_t v) { std::fwrite(&v, 2, 1, f); };
  std::fwrite("RIFF", 1, 4, f); u32(36 + bytes);
  std::fwrite("WAVEfmt ", 1, 8, f); u32(16);
  u16(1); u16(2); u32(kRate); u32(kRate * 6); u16(6); u16(24);
  std::fwrite("data", 1, 4, f); u32(bytes);
  uint32_t seed = 22222;
  for (float v : lr) {
    seed = seed * 1664525u + 1013904223u;
    const float d1 = static_cast<float>(seed >> 8) / 16777216.0f;
    seed = seed * 1664525u + 1013904223u;
    const float d2 = static_cast<float>(seed >> 8) / 16777216.0f;
    const float s = std::clamp(v * 8388607.0f + (d1 - d2), -8388608.0f, 8388607.0f);   // TPDF dither
    const int32_t q = static_cast<int32_t>(std::lround(s));
    const uint8_t b[3] = {static_cast<uint8_t>(q), static_cast<uint8_t>(q >> 8), static_cast<uint8_t>(q >> 16)};
    std::fwrite(b, 1, 3, f);
  }
  std::fclose(f);
  std::printf("wrote %s (%.1f s)\n", path.c_str(), static_cast<double>(frames) / kRate);
}


}  // namespace demo
