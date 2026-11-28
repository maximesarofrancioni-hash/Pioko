// scenes.h - osc440 presets and whole-instrument scenes for builds without the page (the Pi kernel, the host
// renderer). The page has its own copy of the presets (presets.json) and of the scenes, which also set its controls.
#pragma once

int pyoko_preset_count();
const char *pyoko_preset_group(int i);
const char *pyoko_preset_name(int i);
int pyoko_preset_find(const char *group, const char *name);   // -1 if none
void pyoko_load_preset(int i);                  // osc440's defaults, then the preset's parameters

// Lofi hip hop, as the page's SEQ button sets it: 78 BPM, 62% swing, the e-piano on Fmaj9 Em9 Dm9 Cmaj9 (3 hits a bar,
// long gates, +6 dB) through Cytomic SVF LP into the Huovilainen ladder, the demo break on every 16th at -3 st
// varispeed, Beads bypassed. (The page also sets the SP-303 Vinyl Sim, which only the page runs.)
void pyoko_scene_lofi();
