// Host test: renders the filter tour to a WAV file and times it.
//   render out.wav [bars_per_filter] [num_filters]
#include "../src/filterlab.h"
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <vector>

static void put32(FILE *f, uint32_t v) { fwrite(&v, 4, 1, f); }
static void put16(FILE *f, uint16_t v) { fwrite(&v, 2, 1, f); }

int main(int argc, char **argv)
{
    const char *path = argc > 1 ? argv[1] : "tour.wav";
    static FilterLab lab;
    lab.Init(48000.f);
    lab.barsPerFilter = argc > 2 ? atoi(argv[2]) : 2;
    int nf = argc > 3 ? atoi(argv[3]) : lab.NumFilters();

    const double secsPerBar = 60.0 / lab.bpm * 4.0;
    const long frames = (long)(secsPerBar * lab.barsPerFilter * nf * 48000.0);
    std::vector<int16_t> pcm;
    pcm.reserve(frames * 2 + 64);

    float l[FilterLab::kBlock], r[FilterLab::kBlock];
    double peak = 0, nanCount = 0;
    auto t0 = std::chrono::steady_clock::now();
    for (long n = 0; n < frames; n += FilterLab::kBlock)
    {
        lab.Render(l, r);
        if (lab.filterChanged)
        {
            printf("%7.2fs  [%2d] %s\n", n / 48000.0, lab.FilterIndex(), lab.FilterName());
            lab.filterChanged = false;
        }
        for (int i = 0; i < FilterLab::kBlock; ++i)
        {
            if (!std::isfinite(l[i]) || !std::isfinite(r[i]))
                nanCount++;
            peak = std::fmax(peak, std::fabs(l[i]));
            pcm.push_back((int16_t)std::lrint(l[i] * 32000.f));
            pcm.push_back((int16_t)std::lrint(r[i] * 32000.f));
        }
    }
    double el = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
    printf("rendered %.1fs of audio in %.2fs (%.1fx realtime), peak %.3f, non-finite %g\n",
           frames / 48000.0, el, frames / 48000.0 / el, peak, nanCount);

    FILE *f = fopen(path, "wb");
    uint32_t bytes = pcm.size() * 2;
    fwrite("RIFF", 1, 4, f); put32(f, 36 + bytes); fwrite("WAVEfmt ", 1, 8, f);
    put32(f, 16); put16(f, 1); put16(f, 2); put32(f, 48000); put32(f, 48000 * 4);
    put16(f, 4); put16(f, 16); fwrite("data", 1, 4, f); put32(f, bytes);
    fwrite(pcm.data(), 2, pcm.size(), f);
    fclose(f);
    return nanCount > 0;
}
