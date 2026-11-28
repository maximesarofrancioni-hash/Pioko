// Host test: renders Pyoko's engine (core/) to a WAV file and times it, at 48 kHz like the Pi 4 kernel.
//   render out.wav [seconds] [lofi]
// "lofi" starts from the Lofi hip hop scene; without it, the engine's defaults.
#include "engine.h"
#include "scenes.h"
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

static std::vector<float> g_ram(1 << 22);   // 87 s of sample memory at 48 kHz
extern "C" float *pyoko_sample_ram(int *frames) { *frames = (int)g_ram.size(); return g_ram.data(); }

static void put32(FILE *f, uint32_t v) { fwrite(&v, 4, 1, f); }
static void put16(FILE *f, uint16_t v) { fwrite(&v, 2, 1, f); }

int main(int argc, char **argv)
{
    const char *path = argc > 1 ? argv[1] : "pyoko.wav";
    const double secs = argc > 2 ? atof(argv[2]) : 16.0;
    const int sr = 48000;
    fl_init((float)sr);
    if (argc > 3 && !strcmp(argv[3], "lofi")) pyoko_scene_lofi();

    const long frames = (long)(secs * sr);
    std::vector<int16_t> pcm;
    pcm.reserve(frames * 2 + 64);
    double peak = 0, sum = 0;
    long nonFinite = 0;
    auto t0 = std::chrono::steady_clock::now();
    for (long n = 0; n < frames; n += FL_BLOCK)
    {
        fl_render();
        const float *l = fl_left(), *r = fl_right();
        for (int i = 0; i < FL_BLOCK; ++i)
        {
            if (!std::isfinite(l[i]) || !std::isfinite(r[i])) nonFinite++;
            peak = std::fmax(peak, std::fmax(std::fabs(l[i]), std::fabs(r[i])));
            sum += l[i] * l[i];
            pcm.push_back((int16_t)std::lrint(std::fmax(-1.f, std::fmin(1.f, l[i])) * 32767.f));
            pcm.push_back((int16_t)std::lrint(std::fmax(-1.f, std::fmin(1.f, r[i])) * 32767.f));
        }
    }
    double el = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
    printf("rendered %.1f s in %.2f s (%.1fx realtime), peak %.3f, rms %.4f, non-finite %ld\n",
           frames / (double)sr, el, frames / (double)sr / el, peak, std::sqrt(sum / frames), nonFinite);

    FILE *f = fopen(path, "wb");
    if (!f) { perror(path); return 1; }
    uint32_t bytes = (uint32_t)pcm.size() * 2;
    fwrite("RIFF", 1, 4, f); put32(f, 36 + bytes); fwrite("WAVEfmt ", 1, 8, f);
    put32(f, 16); put16(f, 1); put16(f, 2); put32(f, sr); put32(f, sr * 4);
    put16(f, 4); put16(f, 16); fwrite("data", 1, 4, f); put32(f, bytes);
    fwrite(pcm.data(), 2, pcm.size(), f);
    fclose(f);
    return nonFinite > 0;
}
