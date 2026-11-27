// filterlab.h - Euclidean sequencer + 4 voices through the sst-filters QuadFilterUnit.
//
// Plain interface with no sst/ or std:: types, so the Circle side never has to
// see the filter headers. Everything runs at 2x oversampling internally
// (as Surge does) and is decimated with sst's HalfRateFilter.
#pragma once

class FilterLab
{
  public:
    static constexpr int kLanes = 4;      // bass, lead, stab, noise
    static constexpr int kBlock = 32;     // output frames per Render() call

    void Init(float sampleRate);

    // Renders exactly kBlock stereo frames, float in [-1, 1].
    void Render(float *left, float *right);

    // --- filter selection
    int NumFilters() const;
    int FilterIndex() const { return m_filter; }
    const char *FilterName() const;
    void SelectFilter(int index);          // wraps around
    void NextFilter() { SelectFilter(m_filter + 1); }
    void PrevFilter() { SelectFilter(m_filter - 1); }

    // Auto-advance to the next filter every N bars (0 = hold).
    int barsPerFilter = 4;

    // --- macro controls
    float resonance = 0.65f;     // 0..1
    float cutoffOffset = 0.f;    // semitones added to the swept cutoff
    float cutoffMod = 0.f;       // semitones, from a modulation source (web build: Tides)
    float envAmount = 1.f;       // scales every lane's filter envelope
    float drive = 1.f;           // input gain into the filter
    bool sweep = true;           // slow LFO on cutoff
    float bpm = 120.f;
    bool mute[kLanes] = {false, false, false, false};

    // Euclid params per lane (+ accent track)
    void SetEuclid(int lane, int steps, int hits, int rotate);
    void GetEuclid(int lane, int &steps, int &hits, int &rotate) const;

    // Set when the filter changed (auto or manual); caller clears it after logging.
    volatile bool filterChanged = true;
    int Bar() const { return m_bar; }
    int Step() const { return m_step; }
    float CutoffHz() const { return m_cutoffHz; }   // swept base cutoff (before lane offsets / envelopes)   // 1..16 after a tick (last step played)
    const char *FilterNameAt(int i) const;

    // CPU meter helper: OS samples processed so far
    unsigned long long samplesOS = 0;

  private:
    void Tick16th();
    void ResetFilterState();

    float m_sr = 48000.f;
    int m_filter = 0;
    int m_step = 0;
    int m_bar = 0;
    int m_barsOnFilter = 0;
    double m_samplesToNext = 0.0;
    float m_lfoPhase = 0.f;
    float m_cutoffHz = 440.f;
};
