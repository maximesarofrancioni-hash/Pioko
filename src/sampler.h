// filter-lab sampler: one sample in RAM, played on its own Euclidean lane.
//
// Plain C++ with no allocation and no OS calls, so the same code runs in the Circle kernel on the Pi 4
// (the sample memory is a static array in RAM) and in the web page (WebAssembly memory).
//
//  - Memory: mono float frames at the engine rate. The host owns the array and passes it to Init().
//  - Play modes: ONE SHOT plays START..END; SLICE cuts START..END into 4/8/16 equal slices (or AUTO, below) and
//    each hit plays the slice under the step (the region laid over the bars it lasts) or a random one, choking the
//    previous slice, like an MPC/SP chop; a hit inside the slice already playing lets it run on.
//  - AUTO slices: onset detection over START..END (energy rise in 5.8 ms hops, high frequencies weighted), the
//    strongest onsets up to 16 slices; SENS sets the threshold. Slice edges are moved back to just before the hit.
//  - Pitch +-24 semitones + fine +-100 cents, 4-point Hermite interpolation. VARISPEED changes pitch and length
//    together (tape / SP style); KEEP TIME shifts the pitch and keeps the length (two 40 ms Hann grains, 50% overlap).
//  - Amp: linear attack, then exponential decay (0 = hold to the end of the region); 2 ms fade at the end.
//  - Resample: Arm(bars) records the mix fed to Capture() from the next downbeat for 1/2/4 bars.
//  - Demo: MakeDemo() writes a one-bar drum break (synthesised, 120 BPM) so there is something to play.
#pragma once
#include <stdint.h>

class Sampler
{
public:
    static constexpr int kVoices = 4;

    void Init(float *ram, int capacityFrames, float sampleRate);
    void MakeDemo();                      // one bar at 120 BPM, kick / snare / hats

    // memory
    float *Ram() { return m_ram; }
    int Capacity() const { return m_cap; }
    int Length() const { return m_len; }
    void SetLength(int frames);           // after writing frames into Ram(): stops the voices

    // parameters
    float start = 0.f, end = 1.f;         // region, 0..1 of the sample
    float pitch = 0.f;                    // semitones
    float fine = 0.f;                     // cents
    int pitchMode = 1;                    // 0 varispeed, 1 keep time
    float level = 0.8f;
    float attackMs = 1.f, decayMs = 0.f;  // decay 0: hold to the end of the region
    int mode = 1;                         // 0 one shot, 1 slice
    int slices = 16;                      // 4, 8 or 16 equal slices
    bool autoSlice = true;                // slices at the onsets instead
    float sens = 0.5f;                    // auto: onset sensitivity 0..1
    int order = 0;                        // slice order: 0 the step's slice, 1 random
    bool reverse = false;
    bool on = true;
    int steps = 16, hits = 12, rotate = 0;   // its lane

    // sequencing: call on every new 16th (step 0..15, bar, samples per 16th)
    bool Hit(int step) const;
    void Tick(int step, int bar, double samplesPer16th);
    void Trigger(int slice);              // slice < 0: the whole region

    // resampling
    void Arm(int bars) { m_armBars = bars; m_rec = bars > 0 ? 1 : 0; }
    int RecState() const { return m_rec; }        // 0 idle, 1 armed, 2 recording
    float RecProgress() const { return m_recTarget ? (float)m_recPos / (float)m_recTarget : 0.f; }
    bool TakeRecorded() { bool r = m_recDone; m_recDone = false; return r; }
    void Capture(const float *l, const float *r, int n);

    // audio: adds the voices into l / r
    void Render(float *l, float *r, int n);

    // slices: call Update() after changing the sample, the region or the slice settings (Tick() calls it too)
    void Update();
    int NumSlices() const { return m_nSlices; }
    int SliceStart(int i) const { return m_bound[i < 0 ? 0 : (i > m_nSlices ? m_nSlices : i)]; }   // i = n: the end

    // display
    float VoicePos(int v) const;          // 0..1 of the sample, or -1 when idle
    int LastSlice() const { return m_lastSlice; }
    unsigned Triggers() const { return m_trigs; }
    void Overview(uint8_t *out, int cols) const;   // peak per column over the whole sample, 0..255

private:
    struct Voice
    {
        bool active = false;
        double pos = 0.0, inc = 1.0, stop = 0.0;  // stop: frame where it ends (inc's direction)
        float env = 0.f, att = 0.f, dec = 1.f, choke = 1.f, chokeStep = 0.f;
        bool attacking = false;
        unsigned age = 0;
        bool stretch = false;                 // keep time: two grains read at the pitch ratio
        double lo = 0.0, hi = 0.0;            // the slice / region read
        double gAnchor[2] = {0.0, 0.0};
        float gPh[2] = {0.f, 0.f};
        float ratio = 1.f;
    };
    void Analyse(int a, int b);
    float Read(double pos) const;

    float *m_ram = nullptr;
    int m_cap = 0, m_len = 0;
    float m_sr = 44100.f;
    Voice m_v[kVoices];
    unsigned m_clock = 0, m_trigs = 0, m_rand = 22222;
    int m_lastSlice = -1;
    int m_bound[17] = {0};                // slice edges in frames; slice i = [m_bound[i], m_bound[i + 1])
    int m_nSlices = 1;
    unsigned m_gen = 1, m_doneGen = 0;    // sample generation (bumped on SetLength / record / demo)
    float m_cS = -1.f, m_cE = -1.f, m_cSens = -1.f;
    int m_cSl = -1, m_cAuto = -1, m_cMode = -1;
    int m_rec = 0, m_armBars = 0, m_recPos = 0, m_recTarget = 0;
    bool m_recDone = false;
};
