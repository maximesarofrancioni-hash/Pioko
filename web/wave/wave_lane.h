// wave_lane.h - osc440's 3rd Wave-style voice engine (TripleWaveGroup x 2 groups = 16 voices,
// unmodified osc440 sources) played by a sixth Euclidean lane of filter-lab.
// Web build only: the Pi kernel keeps filter-lab alone (osc440 has its own Pi port in pi-synth).
#pragma once
#include <cstdint>

class WaveLane
{
  public:
    enum Mode { kChord = 0, kArp = 1, kBass = 2, kShift = 3, kMarbles = 4 };   // 3, 4: notes come from outside

    void Init();
    // Restores every parameter to osc440's default, then applies (id, value) pairs.
    void LoadPreset(const float *pairs, int count);
    void SetParam(uint16_t id, float v);
    float Param(uint16_t id) const;

    // Called on every 16th step (0..15) of bar `bar`; samplesPer16th sets the gate length.
    void Tick(int step, int bar, double samplesPer16th);
    // Adds n frames (multiple of 16) of the wave lane into L/R.
    void Render(float *L, float *R, int n);
    void AllNotesOff();
    // A note from an outside source (shift register, Marbles), gate in samples
    void ExternalNote(int note, float vel, double gateSamples);
    // Modulation: base + offset, without changing the stored base value
    void ModParam(uint16_t id, float offset);
    bool Hit(int s) const;
    unsigned noteOns = 0;    // counts note-ons (Tides gate, Beads seed)

    // 128 points of the wave at `position` (0..63, fractional) of table 0/1, -1..1
    const float *WavePoints(int table, float position);
    // The loudest sounding voice's live state: [0..2] wave position per osc, [3..5] pulse width,
    // [6] 1 if a voice sounds (else the parameter values), [7] env 1 level, [8] env 2 level, [9] LFO 1 phase, [10] env 3 level
    const float *Live();

    int mode = kChord;
    int octave = 0;          // -2..+2
    float gate = 1.5f;       // in 16ths
    float volume = 1.f;      // linear
    bool mute = false;
    int steps = 16, hits = 4, rotate = 0;
    int lastChord = 0;       // index in the progression, for the display
    int notesHeld = 0;

  private:
    void NoteOn(int note, float vel);
    struct Held { int note; double left; };
    Held m_held[24];
    int m_nHeld = 0;
    int m_arp = 0;
};
