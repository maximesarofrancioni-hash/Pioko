// Host tool: every osc440 preset JSON -> one JSON array of {group, name, p:[[id,value],...]} (only values that
// differ from a fresh TripleWaveGroup), plus "ids": every parameter id with its default.
#include <cstdio>
#include <string>
#include <vector>
#include "preset/preset_json.h"
int main(int argc, char** argv) {
  synth::TripleWaveGroup* def = new synth::TripleWaveGroup();
  printf("{\"defaults\":[");
  bool first = true;
  for (uint16_t id : preset::all_ids()) { printf("%s[%u,%.6g]", first ? "" : ",", id, def->param(id)); first = false; }
  printf("],\"presets\":[");
  for (int a = 1; a + 1 < argc; a += 2) {
    std::string text, name, err; std::vector<preset::Param> ps;
    if (!preset::load(argv[a + 1], text) || !preset::from_json(text, ps, &name, &err)) { fprintf(stderr, "%s: %s\n", argv[a + 1], err.c_str()); return 1; }
    printf("%s{\"group\":\"%s\",\"name\":\"%s\",\"p\":[", a > 1 ? "," : "", argv[a], name.c_str());
    bool f = true;
    for (auto& p : ps) if (p.value != def->param(p.id)) { printf("%s[%u,%.6g]", f ? "" : ",", p.id, p.value); f = false; }
    printf("]}");
  }
  printf("]}\n");
}
