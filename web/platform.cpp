// platform.cpp - what the browser build supplies to the engine (core/engine.h)
#include "engine.h"

// the sampler's RAM: 2^21 mono frames (47.5 s at 44.1 kHz, 8 MB) in the WebAssembly memory
static float g_ram[1 << 21];

extern "C" float *pyoko_sample_ram(int *frames)
{
    *frames = 1 << 21;
    return g_ram;
}
