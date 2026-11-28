#include "preset/preset_json.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <memory>
#include <sstream>

#include "synth/dadsr.h"
#include "synth/lfo.h"
#include "synth/mod_matrix.h"
#include "synth/mseg.h"
#include "synth/oscillator.h"

namespace preset {
namespace {

using G = synth::TripleWaveGroup;

// ---------------------------------------------------------------- names ---

const char* const kOscModes[] = {"wavetable", "sine", "triangle", "saw", "pulse", "supersaw", "noise", "colored_noise"};
const char* const kTables[] = {"basic", "plaits_a"};
const char* const kEnvModes[] = {"exponential", "ppg"};
const char* const kLfoShapes[] = {"sine", "triangle", "saw", "saw_down", "square", "pulse",
                                  "sample_hold", "smooth_random", "exp_ramp", "stepped"};
const char* const kSources[] = {"none", "lfo1", "lfo2", "lfo3", "lfo4", "env1", "env2", "env3", "env4",
                                "wave_env1", "wave_env2", "wave_env3", "velocity", "key", "mod_wheel",
                                "pressure", "expression", "osc1", "osc2", "osc3", "noise"};
const char* const kDestinations[] = {"pitch1", "pitch2", "pitch3", "wave1", "wave2", "wave3", "pw1", "pw2", "pw3",
                                     "fm1", "fm2", "fm3", "level1", "level2", "level3", "amp",
                                     "svf_cutoff", "svf_resonance", "ladder_cutoff", "ladder_resonance",
                                     "drive", "pan", "lfo1_rate", "lfo2_rate", "lfo3_rate", "lfo4_rate"};

template <size_t N>
constexpr int count(const char* const (&)[N]) { return static_cast<int>(N); }

static_assert(count(kOscModes) == static_cast<int>(synth::Oscillator::Mode::kCount), "oscillator mode names");
static_assert(count(kTables) == G::kTables, "table names");
static_assert(count(kLfoShapes) == static_cast<int>(synth::Lfo::Shape::kCount), "LFO shape names");
static_assert(count(kSources) == synth::mod::kNumSources, "matrix source names");
static_assert(count(kDestinations) == synth::mod::kNumDestinations, "matrix destination names");

// Fields of each section: JSON key -> parameter id (as a function of the index)
struct Field { const char* key; uint16_t (*id)(int); enum Kind { kNumber, kBool, kName } kind; const char* const* names; int n_names; };

#define NUM(key, expr) {key, [](int i) -> uint16_t { (void)i; return static_cast<uint16_t>(expr); }, Field::kNumber, nullptr, 0}
#define BOOL(key, expr) {key, [](int i) -> uint16_t { (void)i; return static_cast<uint16_t>(expr); }, Field::kBool, nullptr, 0}
#define NAME(key, expr, names) {key, [](int i) -> uint16_t { (void)i; return static_cast<uint16_t>(expr); }, Field::kName, names, count(names)}

const char* const kScan[] = {"smooth", "stepped"};
const char* const kRouting[] = {"series", "parallel"};

const std::vector<Field> kGlobal = {
    NUM("level", G::kParamLevel),
    NAME("wave_scan", G::kParamWaveScan, kScan),
    NAME("filter_routing", G::kParamFilterRouting, kRouting),
    NUM("key_track", G::kParamKeyTrack),
    NUM("pan", G::kParamPan),
};
const std::vector<Field> kSvf = {
    NUM("cutoff", G::kParamSvfCutoff),       NUM("resonance", G::kParamSvfResonance),
    NUM("morph", G::kParamSvfMorph),         BOOL("bandpass", G::kParamSvfBandpass),
    NUM("env_amount", G::kParamSvfEnvAmount),
    NUM("model", G::kParamFilter1Model),     // OSC440_SST_FILTERS: filter 1's sst model
};
const std::vector<Field> kLadder = {
    NUM("cutoff", G::kParamLadderCutoff),    NUM("resonance", G::kParamLadderResonance),
    NUM("drive", G::kParamDrive),            NUM("compensation", G::kParamCompensation),
    NUM("env_amount", G::kParamLadderEnvAmount), BOOL("oversample", G::kParamLadderOversample),
    NUM("model", G::kParamFilter2Model),     // OSC440_SST_FILTERS: filter 2's sst model
};
const std::vector<Field> kControls = {
    NUM("mod_wheel", G::kParamModWheel), NUM("pressure", G::kParamPressure), NUM("expression", G::kParamExpression),
};
const std::vector<Field> kOsc = {
    NAME("mode", G::osc_param(i, G::kOscMode), kOscModes),
    NAME("table", G::osc_param(i, G::kOscTable), kTables),
    NUM("tune", G::osc_param(i, G::kOscTune)),
    NUM("fine", G::osc_param(i, G::kOscFine)),
    NUM("level", G::osc_param(i, G::kOscLevel)),
    NUM("wave", G::osc_param(i, G::kOscWave)),
    NUM("wave_env_amount", G::osc_param(i, G::kOscWaveEnvAmount)),
    NUM("pw", G::osc_param(i, G::kOscPw)),
    NUM("fm", G::osc_param(i, G::kOscFm)),
    BOOL("sync", G::osc_param(i, G::kOscSync)),
};
const std::vector<Field> kEnv = {
    NAME("mode", G::env_param(i, G::kEnvMode), kEnvModes),
    NUM("delay", G::env_param(i, G::kEnvDelay)),
    NUM("attack", G::env_param(i, G::kEnvAttack)),
    NUM("decay", G::env_param(i, G::kEnvDecay)),
    NUM("sustain", G::env_param(i, G::kEnvSustain)),
    NUM("release", G::env_param(i, G::kEnvRelease)),
};
const std::vector<Field> kLfo = {
    NUM("rate", G::lfo_param(i, G::kLfoRate)),
    NAME("shape", G::lfo_param(i, G::kLfoShape), kLfoShapes),
    BOOL("reset", G::lfo_param(i, G::kLfoReset)),
    NUM("delay", G::lfo_param(i, G::kLfoDelay)),
};
const std::vector<Field> kWaveEnvLoop = {
    NUM("loop_start", G::wave_env_loop_param(i, false)),
    NUM("loop_end", G::wave_env_loop_param(i, true)),
};

// Shortest decimal that reads back as the same float
std::string number(float v) {
  char buf[32];
  for (int digits = 6; digits <= 9; ++digits) {
    std::snprintf(buf, sizeof buf, "%.*g", digits, static_cast<double>(v));
    if (std::strtof(buf, nullptr) == v) break;
  }
  return buf;
}

// --------------------------------------------------------------- writing ---

class Writer {
 public:
  explicit Writer(const G& g) : g_(g) {}

  void fields(const std::vector<Field>& fs, int index, int indent) {
    out_ << "{";
    bool first = true;
    for (const Field& f : fs) {
      out_ << (first ? "\n" : ",\n") << std::string(indent + 2, ' ') << '"' << f.key << "\": " << value(f, index);
      first = false;
    }
    out_ << "\n" << std::string(indent, ' ') << "}";
  }
  std::string value(const Field& f, int index) const {
    const float v = g_.param(f.id(index));
    switch (f.kind) {
      case Field::kBool: return v >= 0.5f ? "true" : "false";
      case Field::kName: {
        const int k = static_cast<int>(v);
        return k >= 0 && k < f.n_names ? std::string("\"") + f.names[k] + "\"" : number(v);
      }
      default: return number(v);
    }
  }
  std::ostringstream& out() { return out_; }

 private:
  const G& g_;
  std::ostringstream out_;
};

std::string escape(const std::string& s) {
  std::string r;
  for (char c : s) {
    if (c == '"' || c == '\\') r += '\\';
    if (c == '\n') { r += "\\n"; continue; }
    r += c;
  }
  return r;
}

// --------------------------------------------------------------- reading ---

struct Value {
  enum Type { kNull, kBool, kNumber, kString, kArray, kObject } type = kNull;
  bool b = false;
  double num = 0.0;
  std::string str;
  std::vector<Value> arr;
  std::vector<std::pair<std::string, Value>> obj;
};

class Parser {
 public:
  explicit Parser(const std::string& s) : s_(s) {}
  bool parse(Value& v, std::string& error) {
    if (!value(v) || (skip(), pos_ != s_.size())) {
      if (error_.empty()) error_ = "unexpected text";
      int line = 1;
      for (size_t i = 0; i < pos_ && i < s_.size(); ++i) line += s_[i] == '\n';
      error = "line " + std::to_string(line) + ": " + error_;
      return false;
    }
    return true;
  }

 private:
  void skip() {
    while (pos_ < s_.size() && (s_[pos_] == ' ' || s_[pos_] == '\n' || s_[pos_] == '\r' || s_[pos_] == '\t')) ++pos_;
  }
  bool fail(const char* what) { error_ = what; return false; }
  bool literal(const char* word) {
    const size_t n = std::string(word).size();
    if (s_.compare(pos_, n, word) != 0) return false;
    pos_ += n;
    return true;
  }
  bool string(std::string& out) {
    if (s_[pos_] != '"') return fail("expected a string");
    ++pos_;
    while (pos_ < s_.size() && s_[pos_] != '"') {
      char c = s_[pos_++];
      if (c == '\\' && pos_ < s_.size()) {
        c = s_[pos_++];
        if (c == 'n') c = '\n';
        else if (c == 't') c = '\t';
      }
      out += c;
    }
    if (pos_ >= s_.size()) return fail("unterminated string");
    ++pos_;
    return true;
  }
  bool value(Value& v) {
    skip();
    if (pos_ >= s_.size()) return fail("unexpected end");
    const char c = s_[pos_];
    if (c == '{') {
      v.type = Value::kObject;
      ++pos_;
      skip();
      if (s_[pos_] == '}') { ++pos_; return true; }
      for (;;) {
        skip();
        std::string key;
        if (!string(key)) return false;
        skip();
        if (s_[pos_] != ':') return fail("expected ':'");
        ++pos_;
        v.obj.emplace_back(key, Value());
        if (!value(v.obj.back().second)) return false;
        skip();
        if (s_[pos_] == ',') { ++pos_; continue; }
        if (s_[pos_] == '}') { ++pos_; return true; }
        return fail("expected ',' or '}'");
      }
    }
    if (c == '[') {
      v.type = Value::kArray;
      ++pos_;
      skip();
      if (s_[pos_] == ']') { ++pos_; return true; }
      for (;;) {
        v.arr.emplace_back();
        if (!value(v.arr.back())) return false;
        skip();
        if (s_[pos_] == ',') { ++pos_; continue; }
        if (s_[pos_] == ']') { ++pos_; return true; }
        return fail("expected ',' or ']'");
      }
    }
    if (c == '"') { v.type = Value::kString; return string(v.str); }
    if (literal("true")) { v.type = Value::kBool; v.b = true; return true; }
    if (literal("false")) { v.type = Value::kBool; v.b = false; return true; }
    if (literal("null")) { v.type = Value::kNull; return true; }
    char* end = nullptr;
    v.num = std::strtod(s_.c_str() + pos_, &end);
    if (end == s_.c_str() + pos_) return fail("expected a value");
    v.type = Value::kNumber;
    pos_ = static_cast<size_t>(end - s_.c_str());
    return true;
  }

  const std::string& s_;
  size_t pos_ = 0;
  std::string error_;
};

class Reader {
 public:
  Reader(std::vector<float>& values, std::string& error) : values_(values), error_(error) {}

  bool fields(const Value& obj, const std::vector<Field>& fs, int index, const std::string& where) {
    if (obj.type != Value::kObject) return fail(where + ": expected an object");
    for (const auto& [key, v] : obj.obj) {
      const Field* f = nullptr;
      for (const Field& x : fs) if (key == x.key) f = &x;
      if (!f) return fail(where + ": unknown key \"" + key + "\"");
      float out = 0.0f;
      if (!field(*f, v, out, where + "." + key)) return false;
      values_[f->id(index)] = out;
    }
    return true;
  }
  bool field(const Field& f, const Value& v, float& out, const std::string& where) {
    if (v.type == Value::kNumber) { out = static_cast<float>(v.num); return true; }
    if (f.kind == Field::kBool && v.type == Value::kBool) { out = v.b ? 1.0f : 0.0f; return true; }
    if (f.kind == Field::kName && v.type == Value::kString) {
      for (int k = 0; k < f.n_names; ++k) if (v.str == f.names[k]) { out = static_cast<float>(k); return true; }
      std::string known;
      for (int k = 0; k < f.n_names; ++k) known += std::string(k ? ", " : "") + f.names[k];
      return fail(where + ": unknown name \"" + v.str + "\" (one of: " + known + ")");
    }
    return fail(where + ": wrong type");
  }
  bool fail(const std::string& e) { error_ = e; return false; }

 private:
  std::vector<float>& values_;
  std::string& error_;
};

const Value* member(const Value& obj, const char* key) {
  for (const auto& [k, v] : obj.obj) if (k == key) return &v;
  return nullptr;
}

std::vector<float> defaults() {
  const auto g = std::make_unique<G>();
  std::vector<float> v(G::kParamCount, 0.0f);
  for (uint16_t id = 0; id < G::kParamCount; ++id) v[id] = g->param(id);
  return v;
}

}  // namespace

const std::vector<uint16_t>& all_ids() {
  static const std::vector<uint16_t> ids = [] {
    std::vector<bool> used(G::kParamCount, false);
    auto mark = [&](const std::vector<Field>& fs, int n) {
      for (int i = 0; i < n; ++i) for (const Field& f : fs) used[f.id(i)] = true;
    };
    mark(kGlobal, 1);
    mark(kSvf, 1);
    mark(kLadder, 1);
    mark(kControls, 1);
    mark(kOsc, G::kOscs);
    mark(kEnv, G::kEnvs);
    mark(kLfo, G::kLfos);
    mark(kWaveEnvLoop, G::kOscs);
    for (int o = 0; o < G::kOscs; ++o) {
      for (int s = 0; s < synth::Mseg::kStages; ++s) {
        used[G::wave_env_param(o, s, false)] = used[G::wave_env_param(o, s, true)] = true;
      }
    }
    for (int s = 0; s < synth::ModMatrix::kSlots; ++s) for (int f = 0; f < 3; ++f) used[G::matrix_param(s, f)] = true;
    std::vector<uint16_t> out;
    for (uint16_t id = 0; id < G::kParamCount; ++id) if (used[id]) out.push_back(id);
    return out;
  }();
  return ids;
}

std::string to_json(const G& g, const std::string& name, const std::string& description) {
  Writer w(g);
  auto& o = w.out();
  o << "{\n  \"format\": \"osc440-preset\",\n  \"version\": 1,\n  \"name\": \"" << escape(name) << "\",\n";
  if (!description.empty()) o << "  \"description\": \"" << escape(description) << "\",\n";
  o << "  \"global\": ";
  w.fields(kGlobal, 0, 2);
  o << ",\n  \"svf\": ";
  w.fields(kSvf, 0, 2);
  o << ",\n  \"ladder\": ";
  w.fields(kLadder, 0, 2);
  o << ",\n  \"controls\": ";
  w.fields(kControls, 0, 2);
  o << ",\n  \"oscillators\": [";
  for (int i = 0; i < G::kOscs; ++i) {
    o << (i ? ", " : "\n    ");
    w.fields(kOsc, i, 4);
  }
  o << "\n  ],\n  \"wave_envelopes\": [";
  for (int i = 0; i < G::kOscs; ++i) {
    o << (i ? ",\n    " : "\n    ") << "{\n      \"stages\": [";
    for (int s = 0; s < synth::Mseg::kStages; ++s) {
      o << (s ? ", " : "") << "{\"level\": " << number(g.param(G::wave_env_param(i, s, false)))
        << ", \"time\": " << number(g.param(G::wave_env_param(i, s, true))) << "}";
    }
    o << "],\n      \"loop_start\": " << number(g.param(G::wave_env_loop_param(i, false)))
      << ",\n      \"loop_end\": " << number(g.param(G::wave_env_loop_param(i, true))) << "\n    }";
  }
  o << "\n  ],\n  \"envelopes\": [";
  for (int i = 0; i < G::kEnvs; ++i) {
    o << (i ? ", " : "\n    ");
    w.fields(kEnv, i, 4);
  }
  o << "\n  ],\n  \"lfos\": [";
  for (int i = 0; i < G::kLfos; ++i) {
    o << (i ? ", " : "\n    ");
    w.fields(kLfo, i, 4);
  }
  o << "\n  ],\n  \"matrix\": [";
  bool first = true;
  for (int s = 0; s < synth::ModMatrix::kSlots; ++s) {
    const int src = static_cast<int>(g.param(G::matrix_param(s, 0)));
    const int dst = static_cast<int>(g.param(G::matrix_param(s, 1)));
    const float amount = g.param(G::matrix_param(s, 2));
    if (src == synth::mod::kSrcNone) continue;
    o << (first ? "\n    " : ",\n    ") << "{\"slot\": " << s << ", \"source\": \"" << kSources[src]
      << "\", \"destination\": \"" << kDestinations[dst] << "\", \"amount\": " << number(amount) << "}";
    first = false;
  }
  o << (first ? "]\n}\n" : "\n  ]\n}\n");
  return o.str();
}

bool from_json(const std::string& text, std::vector<Param>& params, std::string* name, std::string* error) {
  std::string err;
  Value root;
  if (!Parser(text).parse(root, err)) {
    if (error) *error = err;
    return false;
  }
  std::vector<float> values = defaults();
  Reader r(values, err);
  auto fail = [&](const std::string& e) { if (error) *error = e; return false; };
  if (root.type != Value::kObject) return fail("expected an object");

  for (const auto& [key, v] : root.obj) {
    if (key == "format") {
      if (v.str != "osc440-preset") return fail("not an osc440 preset");
    } else if (key == "version") {
      if (v.num > 1) return fail("preset version " + std::to_string(static_cast<int>(v.num)) + " is newer than this reader");
    } else if (key == "name") {
      if (name) *name = v.str;
    } else if (key == "description") {
    } else if (key == "global" || key == "svf" || key == "ladder" || key == "controls") {
      const auto& fs = key == "global" ? kGlobal : key == "svf" ? kSvf : key == "ladder" ? kLadder : kControls;
      if (!r.fields(v, fs, 0, key)) return fail(err);
    } else if (key == "oscillators" || key == "envelopes" || key == "lfos") {
      const auto& fs = key == "oscillators" ? kOsc : key == "envelopes" ? kEnv : kLfo;
      const size_t max = key == "oscillators" ? G::kOscs : key == "envelopes" ? G::kEnvs : G::kLfos;
      if (v.type != Value::kArray || v.arr.size() > max) return fail(key + ": expected an array of up to " + std::to_string(max));
      for (size_t i = 0; i < v.arr.size(); ++i) {
        if (!r.fields(v.arr[i], fs, static_cast<int>(i), key + "[" + std::to_string(i) + "]")) return fail(err);
      }
    } else if (key == "wave_envelopes") {
      if (v.type != Value::kArray || v.arr.size() > G::kOscs) return fail("wave_envelopes: expected an array of up to 3");
      for (size_t i = 0; i < v.arr.size(); ++i) {
        const std::string where = "wave_envelopes[" + std::to_string(i) + "]";
        const Value& e = v.arr[i];
        if (e.type != Value::kObject) return fail(where + ": expected an object");
        for (const auto& [k, x] : e.obj) {
          if (k == "stages") {
            if (x.type != Value::kArray || x.arr.size() > synth::Mseg::kStages) return fail(where + ".stages: up to 6 stages");
            for (size_t s = 0; s < x.arr.size(); ++s) {
              const Value* level = member(x.arr[s], "level");
              const Value* time = member(x.arr[s], "time");
              if (level) values[G::wave_env_param(static_cast<int>(i), static_cast<int>(s), false)] = static_cast<float>(level->num);
              if (time) values[G::wave_env_param(static_cast<int>(i), static_cast<int>(s), true)] = static_cast<float>(time->num);
            }
          } else if (k == "loop_start" || k == "loop_end") {
            values[G::wave_env_loop_param(static_cast<int>(i), k == "loop_end")] = static_cast<float>(x.num);
          } else {
            return fail(where + ": unknown key \"" + k + "\"");
          }
        }
      }
    } else if (key == "matrix") {
      if (v.type != Value::kArray) return fail("matrix: expected an array");
      // Slots the file does not list are empty
      for (int s = 0; s < synth::ModMatrix::kSlots; ++s) {
        for (int f = 0; f < 3; ++f) values[G::matrix_param(s, f)] = 0.0f;
      }
      int next_free = 0;
      for (size_t i = 0; i < v.arr.size(); ++i) {
        const Value& m = v.arr[i];
        const std::string where = "matrix[" + std::to_string(i) + "]";
        const Value* slot = member(m, "slot");
        const int s = slot ? static_cast<int>(slot->num) : next_free;
        if (s < 0 || s >= synth::ModMatrix::kSlots) return fail(where + ": slot out of range (0-15)");
        next_free = s + 1;
        static const Field src_field = NAME("source", 0, kSources);
        static const Field dst_field = NAME("destination", 0, kDestinations);
        float src = 0.0f, dst = 0.0f, amount = 0.0f;
        for (const auto& [k, x] : m.obj) {
          if (k == "slot") continue;
          if (k == "source") { if (!r.field(src_field, x, src, where + ".source")) return fail(err); }
          else if (k == "destination") { if (!r.field(dst_field, x, dst, where + ".destination")) return fail(err); }
          else if (k == "amount") amount = static_cast<float>(x.num);
          else return fail(where + ": unknown key \"" + k + "\"");
        }
        values[G::matrix_param(s, 0)] = src;
        values[G::matrix_param(s, 1)] = dst;
        values[G::matrix_param(s, 2)] = amount;
      }
    } else {
      return fail("unknown key \"" + key + "\"");
    }
  }
  params.clear();
  for (uint16_t id : all_ids()) params.push_back({id, values[id]});
  return true;
}

bool save(const std::string& path, const std::string& json) {
  std::ofstream f(path);
  f << json;
  return static_cast<bool>(f);
}

bool load(const std::string& path, std::string& text) {
  std::ifstream f(path);
  if (!f) return false;
  std::ostringstream ss;
  ss << f.rdbuf();
  text = ss.str();
  return true;
}

}  // namespace preset
