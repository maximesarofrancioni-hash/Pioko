// engine.h - Pyoko's engine as a C API (core/engine.cpp). The browser build exports these from WebAssembly; the Pi 4
// kernel and the host renderer call them directly. fl_render() renders FL_BLOCK frames into fl_left() / fl_right().
#pragma once
#include <stdint.h>

#define FL_BLOCK 32

#ifdef __cplusplus
extern "C" {
#endif

// supplied by the platform: the sampler's memory (mono float frames) and its size in frames
float *pyoko_sample_ram(int *frames);

void fl_init(float sr);
void fl_render();
float *fl_left();
float *fl_right();
int fl_block();
int fl_num_filters();
const char *fl_filter_name(int i);
void fl_set(int id, float v);
int fl_step();
int fl_bar();
float *fl_wbuf();
void fl_wave_preset(int count);
void fl_wave_set(int id, float v);
float fl_wave_get(int id);
void fl_wave_cfg(int k, float v);
void fl_wave_euclid(int n, int k, int r);
float *fl_wave_points(int table, float pos);
int fl_wave_chord();
int fl_wave_held();
float *fl_wave_live();
float *fl_wleft();
float *fl_wright();
void fl_mod_set(int group, int key, float v);
float *fl_mod_info();
void fl_smp_set(int key, float v);
float *fl_smp_ram();
int fl_smp_cap();
void fl_smp_loaded(int frames);
void fl_smp_demo();
void fl_smp_arm(int bars);
void fl_smp_trigger(int slice);
float *fl_smp_info();
uint8_t *fl_smp_overview(int cols);

#ifdef __cplusplus
}
#endif
