// Presets as JSON: what is written reads back to exactly the same sound.

#include <cstring>
#include <memory>
#include <string>
#include <vector>

#include "check.h"
#include "preset/preset_json.h"
#include "synth/demo_patch.h"
#include "synth/triple_wave_group.h"

namespace {

using G = synth::TripleWaveGroup;
using test::check;

void load_into(G& g, const std::vector<preset::Param>& params) {
  for (const auto& p : params) g.set_param(p.id, p.value);
}

int differences(const G& a, const G& b) {
  int n = 0;
  for (uint16_t id : preset::all_ids()) {
    const float x = a.param(id), y = b.param(id);
    n += std::memcmp(&x, &y, sizeof x) != 0;   // bit for bit
  }
  return n;
}

std::vector<float> play(G& g) {
  g.note_on(0, 57.0f, 0.9f);
  g.note_on(1, 64.0f, 0.7f);
  synth::Bus bus;
  std::vector<float> out;
  for (int b = 0; b < 3000; ++b) {
    if (b == 2000) g.note_off(0);
    g.render(bus);
    out.insert(out.end(), bus.l, bus.l + 16);
    out.insert(out.end(), bus.r, bus.r + 16);
  }
  return out;
}

}  // namespace

int main() {
  {
    auto a = std::make_unique<G>(), b = std::make_unique<G>();
    synth::apply_demo_patch([&](uint16_t id, float v) { a->set_param(id, v); });
    // Something in every section that the demo patch leaves alone
    a->set_param(G::env_param(3, G::kEnvMode), 1.0f);
    a->set_param(G::lfo_param(2, G::kLfoShape), static_cast<float>(synth::Lfo::Shape::kSampleHold));
    a->set_param(G::lfo_param(2, G::kLfoReset), 1.0f);
    a->set_param(G::wave_env_loop_param(1, false), 0.0f);
    a->set_param(G::wave_env_loop_param(1, true), 2.0f);
    a->set_param(G::kParamSvfBandpass, 1.0f);
    a->set_param(G::matrix_param(15, 0), synth::mod::kSrcNoise);
    a->set_param(G::matrix_param(15, 1), synth::mod::kDstAmp);
    a->set_param(G::matrix_param(15, 2), 0.1f / 3.0f);

    const std::string json = preset::to_json(*a, "round trip", "a \"quoted\" description");
    std::vector<preset::Param> params;
    std::string name, error;
    const bool ok = preset::from_json(json, params, &name, &error);
    test::check_msg(ok && name == "round trip", "preset: reads back what it wrote", error.empty() ? "ok" : error.c_str());
    load_into(*b, params);
    check(differences(*a, *b) == 0, "preset: every parameter identical after the round trip (differences)", differences(*a, *b));
    check(params.size() == preset::all_ids().size(), "preset: a complete parameter list (ids)", params.size());

    const auto x = play(*a), y = play(*b);
    check(x == y, "preset: the reloaded sound renders bit for bit the same (samples)", x.size());
  }
  {
    // A short hand-written preset: what it leaves out keeps the defaults
    const char* text = R"({
      "name": "hand made",
      "oscillators": [{"mode": "supersaw", "pw": 0.3}],
      "ladder": {"cutoff": 80, "oversample": false},
      "matrix": [{"source": "lfo1", "destination": "ladder_cutoff", "amount": 12}]
    })";
    std::vector<preset::Param> params;
    std::string error;
    const bool ok = preset::from_json(text, params, nullptr, &error);
    auto g = std::make_unique<G>(), fresh = std::make_unique<G>();
    load_into(*g, params);
    const bool fields = g->param(G::osc_param(0, G::kOscMode)) == static_cast<float>(synth::Oscillator::Mode::kSupersaw) &&
                        g->param(G::osc_param(0, G::kOscPw)) == 0.3f && g->param(G::kParamLadderCutoff) == 80.0f &&
                        g->param(G::kParamLadderOversample) == 0.0f &&
                        g->param(G::matrix_param(0, 1)) == synth::mod::kDstCutoff2 &&
                        g->param(G::env_param(1, G::kEnvRelease)) == fresh->param(G::env_param(1, G::kEnvRelease));
    test::check_msg(ok && fields, "preset: partial file, names resolved, the rest default", error.empty() ? "ok" : error.c_str());
  }
  {
    std::vector<preset::Param> params;
    std::string e1, e2, e3;
    const bool bad_key = !preset::from_json(R"({"oscillators": [{"mod": "saw"}]})", params, nullptr, &e1);
    const bool bad_name = !preset::from_json(R"({"lfos": [{"shape": "wobble"}]})", params, nullptr, &e2);
    const bool bad_json = !preset::from_json("{\"global\": {\"level\": }", params, nullptr, &e3);
    test::check_msg(bad_key && e1.find("unknown key \"mod\"") != std::string::npos, "preset: typo in a key is reported", e1.c_str());
    test::check_msg(bad_name && e2.find("wobble") != std::string::npos, "preset: unknown name is reported", e2.c_str());
    test::check_msg(bad_json && e3.find("line 1") != std::string::npos, "preset: malformed JSON is reported", e3.c_str());
  }
  return test::finish("preset");
}
