// wave_lane.cpp - see wave_lane.h
#include "wave_lane.h"

#include <cmath>
#include <cstring>
#include <new>

// Read-only peek at the voices' live oscillator state (wave position and pulse width after the wave
// envelope, LFOs and matrix) for the display, without touching osc440's sources: its private members are
// opened to this file only. Nothing here writes to them.
#include <cstdint>
#include <cstddef>
#include <initializer_list>
#define private public
#include "engine/voice_engine.h"
#include "synth/triple_wave_group.h"
#undef private
#include "synth/wavetable_data.h"
#include "synth/wavetable_plaits.h"

namespace
{
synth::TripleWaveGroup g_core0, g_core1;
engine::VoiceEngine *g_engine = nullptr;
alignas(16) char g_engineMem[sizeof(engine::VoiceEngine)];
synth::Bus g_b0, g_b1;
float g_defaults[synth::TripleWaveGroup::kParamCount];
float g_base[synth::TripleWaveGroup::kParamCount];   // values set by the user/preset (modulation adds to these)
float g_points[128];

// A minor: Am F C G, one chord per bar (MIDI notes of the triad around A3)
const int kProg[4][3] = {{57, 60, 64}, {53, 57, 60}, {55, 60, 64}, {55, 59, 62}};
const int kArpOrder[8] = {0, 1, 2, 1, 0, 2, 1, 2};
} // namespace

void WaveLane::Init()
{
    g_engine = new (g_engineMem) engine::VoiceEngine(g_core0, g_core1);
    for (uint16_t id = 0; id < synth::TripleWaveGroup::kParamCount; ++id)
        g_base[id] = g_defaults[id] = g_core0.param(id);
    m_nHeld = 0;
}

void WaveLane::SetParam(uint16_t id, float v)
{
    if (g_engine && id < synth::TripleWaveGroup::kParamCount)
    {
        g_base[id] = v;
        g_engine->set_param(id, v);
    }
}

void WaveLane::ModParam(uint16_t id, float offset)
{
    if (g_engine && id < synth::TripleWaveGroup::kParamCount)
        g_engine->set_param(id, g_base[id] + offset);
}

void WaveLane::ExternalNote(int note, float vel, double gateSamples)
{
    if (!g_engine || mute)
        return;
    const int first = m_nHeld;
    NoteOn(note, vel);
    for (int i = 0; i < m_nHeld; ++i)
        if (i >= first || m_held[i].left <= 0.0)
            m_held[i].left = gateSamples;
    notesHeld = m_nHeld;
}

float WaveLane::Param(uint16_t id) const { return id < synth::TripleWaveGroup::kParamCount ? g_base[id] : 0.f; }

void WaveLane::LoadPreset(const float *pairs, int count)
{
    AllNotesOff();
    // only the ids osc440 defines (gaps between the blocks keep their zero)
    float target[synth::TripleWaveGroup::kParamCount];
    std::memcpy(target, g_defaults, sizeof target);
    for (int i = 0; i < count; ++i)
    {
        int id = (int)pairs[2 * i];
        if (id >= 0 && id < synth::TripleWaveGroup::kParamCount)
            target[id] = pairs[2 * i + 1];
    }
    for (uint16_t id = 0; id < synth::TripleWaveGroup::kParamCount; ++id)
    {
        g_base[id] = target[id];
        if (target[id] != g_core0.param(id) || target[id] != g_core1.param(id))
            g_engine->set_param(id, target[id]);
    }
}

bool WaveLane::Hit(int s) const
{
    if (steps <= 0 || hits <= 0)
        return false;
    int i = ((s + rotate) % steps + steps) % steps;
    return (i * hits) % steps < hits;
}

void WaveLane::NoteOn(int note, float vel)
{
    if (note < 0 || note > 127)
        return;
    for (int i = 0; i < m_nHeld; ++i)       // retrigger: release the old one first
        if (m_held[i].note == note)
        {
            g_engine->note_off((uint8_t)note);
            m_held[i] = m_held[--m_nHeld];
            break;
        }
    if (m_nHeld >= 24)
    {
        g_engine->note_off((uint8_t)m_held[0].note);
        m_held[0] = m_held[--m_nHeld];
    }
    g_engine->note_on((uint8_t)note, vel);
    ++noteOns;
    m_held[m_nHeld++] = {note, 0.0};
}

void WaveLane::Tick(int step, int bar, double samplesPer16th)
{
    if (!g_engine || mute || mode >= kShift || !Hit(step))
        return;
    lastChord = bar & 3;
    const int *c = kProg[lastChord];
    const int oct = 12 * octave;
    const float vel = (step % 4 == 0) ? 0.95f : 0.75f;
    const int first = m_nHeld;
    switch (mode)
    {
    case kChord:
        for (int i = 0; i < 3; ++i)
            NoteOn(c[i] + oct, vel);
        NoteOn(c[0] - 12 + oct, vel * 0.8f);
        break;
    case kArp:
    {
        int k = kArpOrder[m_arp++ & 7];
        NoteOn(c[k] + oct + ((m_arp & 8) ? 12 : 0), vel);
        break;
    }
    default:
        NoteOn(c[0] - 24 + oct, vel);
        break;
    }
    const double len = gate * samplesPer16th;
    for (int i = 0; i < m_nHeld; ++i)
        if (i >= first || m_held[i].left <= 0.0)
            m_held[i].left = len;
    notesHeld = m_nHeld;
}

void WaveLane::AllNotesOff()
{
    if (!g_engine)
        return;
    for (int i = 0; i < m_nHeld; ++i)
        g_engine->note_off((uint8_t)m_held[i].note);
    m_nHeld = 0;
    notesHeld = 0;
}

void WaveLane::Render(float *L, float *R, int n)
{
    if (!g_engine)
        return;
    constexpr int B = (int)audio::kBlockSize;
    for (int off = 0; off + B <= n; off += B)
    {
        // gates
        for (int i = 0; i < m_nHeld;)
        {
            m_held[i].left -= B;
            if (m_held[i].left <= 0.0)
            {
                g_engine->note_off((uint8_t)m_held[i].note);
                m_held[i] = m_held[--m_nHeld];
            }
            else
                ++i;
        }
        notesHeld = m_nHeld;
        g_engine->render(0, g_b0);
        g_engine->render(1, g_b1);
        for (int i = 0; i < B; ++i)
        {
            L[off + i] += (g_b0.l[i] + g_b1.l[i]) * volume;
            R[off + i] += (g_b0.r[i] + g_b1.r[i]) * volume;
        }
    }
}

const float *WaveLane::WavePoints(int table, float position)
{
    const synth::Wavetable &t = table ? synth::kPlaitsWavetable : synth::kBasicWavetable;
    const synth::Wavetable::Level &lv = t.level[0];
    const int len = 1 << lv.bits, stride = (int)synth::Wavetable::stride(lv.bits);
    float last = (float)(t.waves - 1);
    position = position < 0.f ? 0.f : (position > last ? last : position);
    int w0 = (int)position, w1 = w0 + 1 > t.waves - 1 ? w0 : w0 + 1;
    float fr = position - (float)w0;
    const int16_t *a = lv.data + w0 * stride, *b = lv.data + w1 * stride;
    for (int i = 0; i < 128; ++i)
    {
        int k = i * len / 128;
        g_points[i] = ((1.f - fr) * a[k] + fr * b[k]) * (1.f / 32768.f);
    }
    return g_points;
}

const float *WaveLane::Live()
{
    static float out[11];
    const synth::TripleWaveGroup::Voice *best = nullptr;
    float lv = 0.f;
    for (synth::TripleWaveGroup *g : {&g_core0, &g_core1})
        for (const auto &v : g->voice_)
            if (!v.env[1].idle() && v.env[1].level() > lv)
            {
                lv = v.env[1].level();
                best = &v;
            }
    for (int o = 0; o < 3; ++o)
    {
        out[o] = best ? best->osc[o].position_ : Param(synth::TripleWaveGroup::osc_param(o, synth::TripleWaveGroup::kOscWave));
        out[3 + o] = best ? best->osc[o].pw_ : Param(synth::TripleWaveGroup::osc_param(o, synth::TripleWaveGroup::kOscPw));
    }
    out[6] = best ? 1.f : 0.f;
    out[7] = best ? best->env[0].level() : 0.f;     // env 1 (filters)
    out[8] = best ? best->env[1].level() : 0.f;     // env 2 (amp)
    out[9] = best ? best->lfo[0].phase_ : -1.f;     // LFO 1 phase, -1: no voice
    out[10] = best ? best->env[2].level() : 0.f;    // env 3
    return out;
}
