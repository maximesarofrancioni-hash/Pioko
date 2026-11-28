// scenes.cpp - see scenes.h
#include "scenes.h"
#include "engine.h"
#include "presets.generated.h"

#include <cstring>

int pyoko_preset_count() { return kNumPresets; }

const char *pyoko_preset_group(int i) { return i >= 0 && i < kNumPresets ? kPresets[i].group : ""; }
const char *pyoko_preset_name(int i) { return i >= 0 && i < kNumPresets ? kPresets[i].name : ""; }

int pyoko_preset_find(const char *group, const char *name)
{
    for (int i = 0; i < kNumPresets; ++i)
        if (!strcmp(kPresets[i].group, group) && !strcmp(kPresets[i].name, name)) return i;
    return -1;
}

void pyoko_load_preset(int i)
{
    if (i < 0 || i >= kNumPresets) return;
    const PyokoPreset &p = kPresets[i];
    float *w = fl_wbuf();
    int n = p.count > 256 ? 256 : p.count;
    for (int k = 0; k < 2 * n; ++k) w[k] = p.pairs[k];
    fl_wave_preset(n);
}

void pyoko_scene_lofi()
{
    fl_set(4, 78.f);                          // tempo
    fl_set(7, (62.f - 50.f) / 50.f);          // swing 62%
    // track 1: the e-piano on the 9th chords, Euclid, 3 hits a bar
    pyoko_load_preset(pyoko_preset_find("bedroom_pop", "e-piano"));
    fl_wave_cfg(0, 0);                        // Euclid, chords
    fl_wave_cfg(5, 1);                        // Fmaj9 Em9 Dm9 Cmaj9
    fl_wave_cfg(4, 0);                        // the lane on
    fl_wave_cfg(1, 0);                        // octave
    fl_wave_euclid(16, 3, 0);
    fl_wave_cfg(2, 6.f);                      // gate, 16ths
    fl_wave_cfg(3, 6.f);                      // volume, dB
    fl_wave_set(20, 17.f); fl_wave_set(21, 4.f);                          // Cytomic SVF LP into the Huovilainen ladder
    fl_wave_set(2, 100.f); fl_wave_set(3, 0.1f); fl_wave_set(6, 6.f);     // filter 1: cutoff, resonance, env 1
    fl_wave_set(7, 86.f); fl_wave_set(8, 0.12f); fl_wave_set(11, 12.f); fl_wave_set(9, 1.2f);   // filter 2, drive
    fl_wave_set(12, 0.f);                     // series
    // track 2: the demo break, every 16th, slower and darker
    fl_smp_demo();
    fl_smp_set(10, 1.f);                      // on
    fl_smp_set(20, 0.f);                      // Euclid
    fl_smp_set(6, 1.f); fl_smp_set(16, 1.f);  // slice, auto
    fl_smp_set(15, 0.f);                      // varispeed
    fl_smp_set(8, 0.f);                       // step order
    fl_smp_set(2, -3.f);                      // pitch
    fl_smp_set(3, 0.45f);                     // level
    fl_smp_set(11, 16.f); fl_smp_set(12, 16.f); fl_smp_set(13, 0.f);   // steps, hits, rotate
    // FX: Beads bypassed
    fl_mod_set(3, 0, 0.f);
}
