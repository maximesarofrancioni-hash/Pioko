#pragma once
// Presets as JSON files, for the host side (demos, tests, a future editor or the
// master's patch storage): one TripleWaveGroup sound, every parameter by name.
// Not built into the voice firmware, which only ever sees parameter ids.
//
// Format (see presets/README.md):
//   { "format": "osc440-preset", "version": 1, "name": "...",
//     "global": {...}, "oscillators": [3], "wave_envelopes": [3],
//     "envelopes": [4], "lfos": [4], "matrix": [slots in use] }

#include <cstdint>
#include <string>
#include <vector>

#include "synth/triple_wave_group.h"

namespace preset {

struct Param {
  uint16_t id;
  float value;
};

// The group's whole sound as JSON
std::string to_json(const synth::TripleWaveGroup& group, const std::string& name,
                    const std::string& description = "");

// Reads a preset into the complete parameter list (every id, in order; anything
// the file leaves out keeps its default). Unknown keys and names are errors.
bool from_json(const std::string& text, std::vector<Param>& params, std::string* name, std::string* error);

// Every parameter id the group defines, in order
const std::vector<uint16_t>& all_ids();

bool save(const std::string& path, const std::string& json);
bool load(const std::string& path, std::string& text);

}  // namespace preset
