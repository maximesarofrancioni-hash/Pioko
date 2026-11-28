// Renders the synth on the host, block by block as the firmware does, and writes
// mono 32-bit float WAV files: note_* and wt_* for test/check_wav.py (pitch and
// aliasing), demo_* and sweep_* for listening.
//
//   host_render <out_dir>

#include <cstdio>
#include <string>
#include <vector>

#include "config/audio_config.h"
#include "engine/voice_engine.h"
#include "synth/demo_patch.h"
#include "synth/poly_group.h"
#include "synth/triple_wave_group.h"
#include "synth/wavetable_plaits.h"

namespace {

void write_wav(const std::string& path, const std::vector<float>& samples) {
  FILE* f = std::fopen(path.c_str(), "wb");
  if (!f) { std::perror(path.c_str()); return; }
  const uint32_t rate = audio::kSampleRate;
  const uint32_t data_bytes = static_cast<uint32_t>(samples.size() * 4);
  auto u32 = [&](uint32_t v) { std::fwrite(&v, 4, 1, f); };
  auto u16 = [&](uint16_t v) { std::fwrite(&v, 2, 1, f); };
  std::fwrite("RIFF", 1, 4, f); u32(36 + data_bytes);
  std::fwrite("WAVEfmt ", 1, 8, f); u32(16);
  u16(3); u16(1); u32(rate); u32(rate * 4); u16(4); u16(32);   // IEEE float, mono
  std::fwrite("data", 1, 4, f); u32(data_bytes);
  std::fwrite(samples.data(), 4, samples.size(), f);
  std::fclose(f);
  std::printf("wrote %s\n", path.c_str());
}

constexpr size_t kN = audio::kBlockSize;
size_t blocks_for(float seconds) { return static_cast<size_t>(seconds * audio::kSampleRate / kN); }

// Saw through the whole voice, filter wide open
std::vector<float> saw_note(float pitch) {
  synth::SawGroup group;
  group.set_param(synth::SawGroup::kParamCutoff, 135.0f);
  group.set_param(synth::SawGroup::kParamKeyTrack, 0.0f);
  group.set_param(synth::SawGroup::kParamFilterEnvAmount, 0.0f);
  group.set_param(synth::SawGroup::kParamAmpSustain, 1.0f);
  group.note_on(0, pitch, 1.0f);
  synth::Bus bus;
  std::vector<float> out;
  for (size_t b = 0; b < blocks_for(2.0f); ++b) {
    group.render(bus);
    out.insert(out.end(), bus.l, bus.l + kN);
  }
  return out;
}

// The wavetable oscillator alone, one wave, static
std::vector<float> wavetable_note(float pitch, int wave) {
  synth::WavetableOsc osc;
  osc.set_pitch(pitch);
  osc.set_wave(static_cast<float>(wave), synth::WaveScan::kStepped);
  std::vector<float> out(blocks_for(2.0f) * kN);
  for (size_t b = 0; b < blocks_for(2.0f); ++b) osc.render(&out[b * kN], kN);
  return out;
}

// Full-table sweep at A2, for listening to the two scan modes
std::vector<float> sweep(synth::WaveScan scan) {
  synth::WavetableOsc osc;
  osc.set_pitch(45.0f);
  const size_t blocks = blocks_for(4.0f);
  std::vector<float> out(blocks * kN);
  for (size_t b = 0; b < blocks; ++b) {
    osc.set_wave(63.0f * (b + 1) / blocks, scan);
    osc.render(&out[b * kN], kN);
    for (size_t i = 0; i < kN; ++i) out[b * kN + i] *= 0.5f;
  }
  return out;
}

// The 3-oscillator voice with both filters wide open, oscillators an octave
// apart (so every component is a harmonic of the root): pitch and aliasing
std::vector<float> triple_note(float pitch) {
  using G = synth::TripleWaveGroup;
  G group;
  const float waves[] = {31.0f, 40.0f, 56.0f}, levels[] = {0.5f, 0.3f, 0.2f};
  for (int o = 0; o < 3; ++o) {
    group.set_param(G::osc_param(o, G::kOscTune), 12.0f * o);
    group.set_param(G::osc_param(o, G::kOscWave), waves[o]);
    group.set_param(G::osc_param(o, G::kOscLevel), levels[o]);
  }
  group.set_param(G::kParamLadderCutoff, 135.0f);
  group.set_param(G::kParamLadderResonance, 0.0f);
  group.set_param(G::kParamLadderEnvAmount, 0.0f);
  group.set_param(G::env_param(1, G::kEnvSustain), 1.0f);
  group.set_param(G::kParamKeyTrack, 0.0f);
  group.note_on(0, pitch, 1.0f);
  synth::Bus bus;
  std::vector<float> out;
  for (size_t b = 0; b < blocks_for(2.0f); ++b) {
    group.render(bus);
    out.insert(out.end(), bus.l, bus.l + kN);
  }
  return out;
}

// One oscillator alone, static: the VA modes (va_*) and Plaits' bank (pt_*)
std::vector<float> osc_note(float pitch, synth::Oscillator::Mode mode, float pw,
                            const synth::Wavetable* table = nullptr, int wave = 0) {
  synth::Oscillator osc;
  osc.set_mode(mode);
  if (table) osc.set_table(table);
  osc.set_wave(static_cast<float>(wave), synth::WaveScan::kStepped);
  osc.set_pulse_width(pw);
  osc.set_pitch(pitch);
  std::vector<float> out(blocks_for(2.0f) * kN);
  for (size_t b = 0; b < blocks_for(2.0f); ++b) {
    osc.render(&out[b * kN], kN, nullptr, nullptr, nullptr);
    for (size_t i = 0; i < kN; ++i) out[b * kN + i] *= 0.5f;
  }
  return out;
}

// Hard sync in the voice: osc 2 (+7.3 semitones) reset by osc 1 (silent), so the
// output repeats at osc 1's period: every component is a harmonic of `pitch`
std::vector<float> sync_note(float pitch, synth::Oscillator::Mode mode) {
  using G = synth::TripleWaveGroup;
  G group;
  group.set_param(G::osc_param(0, G::kOscLevel), 0.0f);
  group.set_param(G::osc_param(1, G::kOscMode), static_cast<float>(mode));
  group.set_param(G::osc_param(1, G::kOscWave), 24.0f);
  group.set_param(G::osc_param(1, G::kOscTune), 7.3f);
  group.set_param(G::osc_param(1, G::kOscLevel), 0.5f);
  group.set_param(G::osc_param(1, G::kOscSync), 1.0f);
  group.set_param(G::kParamLadderCutoff, 135.0f);
  group.set_param(G::kParamLadderResonance, 0.0f);
  group.set_param(G::kParamLadderEnvAmount, 0.0f);
  group.set_param(G::env_param(1, G::kEnvSustain), 1.0f);
  group.set_param(G::kParamKeyTrack, 0.0f);
  group.note_on(0, pitch, 1.0f);
  synth::Bus bus;
  std::vector<float> out;
  for (size_t b = 0; b < blocks_for(2.0f); ++b) {
    group.render(bus);
    out.insert(out.end(), bus.l, bus.l + kN);
  }
  return out;
}

// What the firmware plays: two cores, a 4-note chord each second, demo patch
std::vector<float> firmware_demo() {
  using G = synth::TripleWaveGroup;
  G g0, g1;
  engine::VoiceEngine e(g0, g1);
  synth::apply_demo_patch([&](uint16_t id, float v) { e.set_param(id, v); });
  const uint8_t chord[] = {57, 61, 64, 69};
  const uint32_t per_second = audio::kSampleRate / kN;
  synth::Bus b0, b1, mix;
  std::vector<float> out;
  for (uint32_t block = 0; block < 4 * per_second; ++block) {
    const uint32_t phase = block % per_second;
    if (phase == 0) for (uint8_t n : chord) e.note_on(n, 0.9f);
    if (phase == per_second / 2) for (uint8_t n : chord) e.note_off(n);
    e.render(0, b0);
    e.render(1, b1);
    engine::VoiceEngine::mix(b0, b1, mix);
    out.insert(out.end(), mix.l, mix.l + kN);
  }
  return out;
}

}  // namespace

int main(int argc, char** argv) {
  const std::string dir = argc > 1 ? argv[1] : ".";

  for (int note : {69, 100, 113, 125}) {
    write_wav(dir + "/note_n" + std::to_string(note) + ".wav", saw_note(static_cast<float>(note)));
  }
  // One wave from each region: sine, saw, saw->square, square, pulses, formants
  for (int wave : {0, 15, 24, 31, 40, 47, 56, 63}) {
    for (int note : {33, 69, 100, 113, 125}) {
      write_wav(dir + "/wt_w" + std::to_string(wave) + "_n" + std::to_string(note) + ".wav",
                wavetable_note(static_cast<float>(note), wave));
    }
  }
  for (int note : {33, 57, 69, 96}) {
    write_wav(dir + "/tw_n" + std::to_string(note) + ".wav", triple_note(static_cast<float>(note)));
  }
  using M = synth::Oscillator::Mode;
  const struct { const char* name; M mode; float pw; } va[] = {
      {"saw", M::kSaw, 0.5f}, {"pulse50", M::kPulse, 0.5f}, {"pulse10", M::kPulse, 0.1f},
      {"tri", M::kTriangle, 0.5f}, {"sine", M::kSine, 0.5f}};
  for (const auto& v : va) {
    for (int note : {33, 69, 93, 105}) {
      write_wav(dir + "/va_" + v.name + "_n" + std::to_string(note) + ".wav",
                osc_note(static_cast<float>(note), v.mode, v.pw));
    }
  }
  // Wave 23 keeps its energy near harmonic 53: above A1 that is past Nyquist, so
  // the band-limited tables (rightly) leave almost nothing to measure
  for (int wave : {0, 10, 23, 40, 63}) {
    for (int note : {33, 45, 69, 100}) {
      if (wave == 23 && note > 45) continue;
      write_wav(dir + "/pt_w" + std::to_string(wave) + "_n" + std::to_string(note) + ".wav",
                osc_note(static_cast<float>(note), M::kWavetable, 0.5f, &synth::kPlaitsWavetable, wave));
    }
  }
  for (int note : {45, 69, 93}) {
    write_wav(dir + "/sync_saw_n" + std::to_string(note) + ".wav", sync_note(static_cast<float>(note), M::kSaw));
    write_wav(dir + "/sync_wt_n" + std::to_string(note) + ".wav", sync_note(static_cast<float>(note), M::kWavetable));
  }
  write_wav(dir + "/sweep_smooth.wav", sweep(synth::WaveScan::kSmooth));
  write_wav(dir + "/sweep_stepped.wav", sweep(synth::WaveScan::kStepped));
  write_wav(dir + "/demo_chord.wav", firmware_demo());
  return 0;
}
