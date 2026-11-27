// filter-lab sampler - see sampler.h
#include "sampler.h"
#include <math.h>

void Sampler::Init(float *ram, int capacityFrames, float sampleRate)
{
    m_ram = ram; m_cap = capacityFrames; m_sr = sampleRate; m_len = 0;
    for (Voice &v : m_v) v.active = false;
}

void Sampler::SetLength(int frames)
{
    for (Voice &v : m_v) v.active = false;
    m_len = frames < 0 ? 0 : (frames > m_cap ? m_cap : frames);
    m_gen++;
    Update();
}

// ---- slices: equal, or at the onsets
static float s_env[1 << 16];               // onset analysis scratch: one value per hop

void Sampler::Update()
{
    if (m_gen == m_doneGen && start == m_cS && end == m_cE && sens == m_cSens && slices == m_cSl
        && (int)autoSlice == m_cAuto && mode == m_cMode) return;
    m_doneGen = m_gen; m_cS = start; m_cE = end; m_cSens = sens; m_cSl = slices; m_cAuto = autoSlice; m_cMode = mode;
    int a = (int)((start < 0.f ? 0.f : (start > 1.f ? 1.f : start)) * m_len);
    int b = (int)((end < 0.f ? 0.f : (end > 1.f ? 1.f : end)) * m_len);
    if (b < a) { int t = a; a = b; b = t; }
    if (b - a < 64) b = a + 64 > m_len ? m_len : a + 64;
    if (mode == 1 && autoSlice && b - a > 2048) { Analyse(a, b); return; }
    int n = mode == 0 ? 1 : (slices < 1 ? 1 : (slices > 16 ? 16 : slices));
    for (int i = 0; i <= n; ++i) m_bound[i] = a + (int)((long long)(b - a) * i / n);
    m_nSlices = n;
}

void Sampler::Analyse(int a, int b)
{
    int hop = (int)(0.0058f * m_sr);                         // 256 frames at 44.1 kHz
    while ((b - a) / hop >= (1 << 16)) hop *= 2;
    int nh = (b - a) / hop;
    float emax = 1e-12f;
    for (int h = 0; h < nh; ++h)
    {
        float e = 0.f;
        const float *x = m_ram + a + h * hop;
        for (int i = 0; i < hop; ++i)
        {
            float d = x[i] - (a + h * hop + i > 0 ? x[i - 1] : 0.f);   // high frequencies weighted
            e += d * d + 0.25f * x[i] * x[i];
        }
        s_env[h] = e;
        if (e > emax) emax = e;
    }
    // onset strength: log rise over the mean of the three hops before; peaks over the threshold
    const float thr = 2.6f - 2.2f * (sens < 0.f ? 0.f : (sens > 1.f ? 1.f : sens));   // ln units: x13 .. x1.5
    const int gap = (int)(0.06f * m_sr / hop) + 1;          // 60 ms between slices at least
    struct Cand { int h; float s; } cand[64];
    int nc = 0;
    auto odf = [&](int h) {
        if (h < 1 || h >= nh) return 0.f;
        float p = 0.f; int k = 0;
        for (int j = h - 3; j < h; ++j) if (j >= 0) { p += s_env[j]; k++; }
        if (s_env[h] < emax * 0.001f) return 0.f;          // under -30 dB of the loudest: not a hit
        return logf((s_env[h] + 1e-9f) / (p / k + 1e-9f));
    };
    for (int h = gap; h < nh; ++h)
    {
        float o = odf(h);
        if (o < thr || o < odf(h - 1) || o < odf(h + 1)) continue;
        if (nc && h - cand[nc - 1].h < gap) { if (o > cand[nc - 1].s) cand[nc - 1] = {h, o}; continue; }
        if (nc < 64) cand[nc++] = {h, o};
        else
        {
            int w = 0; for (int i = 1; i < 64; ++i) if (cand[i].s < cand[w].s) w = i;
            if (o > cand[w].s) { for (int i = w; i < 63; ++i) cand[i] = cand[i + 1]; cand[63] = {h, o}; }
        }
    }
    // keep the 15 strongest (the region start is slice 1), in time order
    while (nc > 15)
    {
        int w = 0; for (int i = 1; i < nc; ++i) if (cand[i].s < cand[w].s) w = i;
        for (int i = w; i < nc - 1; ++i) cand[i] = cand[i + 1];
        nc--;
    }
    m_bound[0] = a;
    int n = 1;
    for (int i = 0; i < nc; ++i)
    {
        // the edge: the first frame of the hit (over 30% of the window's peak), 1 ms earlier
        int w0 = a + (cand[i].h - 1) * hop, w1 = a + (cand[i].h + 1) * hop;
        if (w1 > b) w1 = b;
        float pk = 0.f;
        for (int j = w0; j < w1; ++j) { float x = fabsf(m_ram[j]); if (x > pk) pk = x; }
        int at = w0;
        while (at < w1 && fabsf(m_ram[at]) < 0.3f * pk) at++;
        at -= (int)(0.001f * m_sr);
        if (at <= m_bound[n - 1] + hop) continue;
        m_bound[n++] = at;
    }
    m_bound[n] = b;
    m_nSlices = n;
}

bool Sampler::Hit(int step) const
{
    int n = steps, k = hits;
    if (n <= 0 || k <= 0) return false;
    int i = ((step + rotate) % n + n) % n;
    return (i * k) % n < k;
}

void Sampler::Tick(int step, int bar, double samplesPer16th)
{
    if (m_rec == 1 && step == 0)             // armed: start on the downbeat
    {
        double t = (double)m_armBars * 16.0 * samplesPer16th;
        m_recTarget = t > m_cap ? m_cap : (int)t;
        m_recPos = 0; m_rec = 2;
        for (Voice &v : m_v) v.active = false;
    }
    if (m_rec == 2 || !on || m_len < 64) return;
    Update();
    if (!Hit(step)) return;
    int n = m_nSlices;
    if (mode == 0) { Trigger(-1); return; }
    if (order == 1) { m_rand = m_rand * 1664525u + 1013904223u; Trigger((int)((m_rand >> 16) % (unsigned)n)); return; }
    // STEP order: the slice under the step. The region is laid over the bars it lasts at this tempo (at least
    // one), so a two-bar loop plays its second half on odd bars. A hit inside the slice already playing lets it
    // run on instead of restarting it.
    double len = (double)(m_bound[n] - m_bound[0]);
    int bars = (int)floor(len / (16.0 * samplesPer16th) + 0.5);
    if (bars < 1) bars = 1;
    double at = m_bound[0] + len * (((bar % bars) * 16 + step) + 0.25) / (16.0 * bars);
    int k = 0;
    while (k < n - 1 && at >= m_bound[k + 1]) k++;
    if (k == m_lastSlice && at > m_bound[k] + 0.5 * samplesPer16th)
        for (const Voice &v : m_v)
            if (v.active && v.chokeStep == 0.f && v.lo == m_bound[k]) return;
    Trigger(k);
}

void Sampler::Trigger(int slice)
{
    if (m_len < 64) return;
    Update();
    double a = m_bound[0], b = m_bound[m_nSlices];
    if (slice >= 0 && mode == 1)
    {
        int k = slice % m_nSlices;
        a = m_bound[k]; b = m_bound[k + 1];
        for (Voice &v : m_v)                 // a new slice chokes the one playing (3 ms)
            if (v.active && v.chokeStep == 0.f) v.chokeStep = 1.f / (0.003f * m_sr);
    }
    // a free voice, else the oldest
    Voice *best = &m_v[0];
    for (Voice &v : m_v)
    {
        if (!v.active) { best = &v; break; }
        if (v.age < best->age) best = &v;
    }
    Voice &v = *best;
    double r = pow(2.0, (pitch + fine * 0.01) / 12.0);
    v.stretch = pitchMode == 1 && fabs(r - 1.0) > 1e-4;
    v.ratio = (float)r;
    double dir = reverse ? -1.0 : 1.0;
    v.inc = v.stretch ? dir : dir * r;                       // keep time: the voice moves at 1x, grains read at r
    v.pos = reverse ? b - 1.0 : a;
    v.stop = reverse ? a : b;
    v.lo = a; v.hi = b;
    if (v.stretch)
    {
        // grain 0 at the top of its window reading the first frame; grain 1 rising from zero at the same spot
        float G = 0.04f * m_sr;
        v.gPh[0] = 0.5f * G; v.gAnchor[0] = v.pos - dir * 0.5 * G * r;
        v.gPh[1] = 0.f;      v.gAnchor[1] = v.pos;
    }
    float att = attackMs * 0.001f * m_sr;
    v.att = 1.f / (att < 1.f ? 1.f : att);
    v.env = 0.f; v.attacking = true;
    v.dec = decayMs > 0.f ? expf(logf(0.001f) / (decayMs * 0.001f * m_sr)) : 1.f;   // -60 dB after decayMs
    v.choke = 1.f; v.chokeStep = 0.f;
    v.age = ++m_clock;
    v.active = true;
    m_lastSlice = slice;
    m_trigs++;
}

float Sampler::Read(double pos) const
{
    int i = (int)floor(pos);
    float f = (float)(pos - i);
    auto at = [&](int k) { return k < 0 || k >= m_len ? 0.f : m_ram[k]; };
    float xm = at(i - 1), x0 = at(i), x1 = at(i + 1), x2 = at(i + 2);
    float c1 = 0.5f * (x1 - xm), c2 = xm - 2.5f * x0 + 2.f * x1 - 0.5f * x2, c3 = 0.5f * (x2 - xm) + 1.5f * (x0 - x1);
    return ((c3 * f + c2) * f + c1) * f + x0;
}

void Sampler::Render(float *l, float *r, int n)
{
    if (m_rec == 2) return;
    const float fadeLen = 0.002f * m_sr;
    for (Voice &v : m_v)
    {
        if (!v.active) continue;
        for (int i = 0; i < n; ++i)
        {
            if (v.attacking) { v.env += v.att; if (v.env >= 1.f) { v.env = 1.f; v.attacking = false; } }
            else v.env *= v.dec;
            double left = v.inc > 0 ? v.stop - v.pos : v.pos - v.stop;
            float fade = left < fadeLen ? (float)left / fadeLen : 1.f;
            if (v.chokeStep > 0.f) v.choke -= v.chokeStep;
            if (left <= 0.0 || v.choke <= 0.f || (!v.attacking && v.env < 1e-4f)) { v.active = false; break; }
            float x;
            if (!v.stretch) x = Read(v.pos);
            else
            {
                const float G = 0.04f * m_sr;
                const double dir = v.inc > 0 ? 1.0 : -1.0;
                x = 0.f;
                for (int g = 0; g < 2; ++g)
                {
                    double rp = v.gAnchor[g] + dir * (double)v.gPh[g] * v.ratio;
                    float w = 0.5f - 0.5f * cosf(6.2831853f * v.gPh[g] / G);
                    if (rp >= v.lo && rp < v.hi) x += w * Read(rp);
                    if ((v.gPh[g] += 1.f) >= G) { v.gPh[g] -= G; v.gAnchor[g] = v.pos; }
                }
            }
            float s = x * v.env * fade * v.choke * level;
            l[i] += s; r[i] += s;
            v.pos += v.inc;
        }
    }
}

void Sampler::Capture(const float *l, const float *r, int n)
{
    if (m_rec != 2) return;
    for (int i = 0; i < n && m_recPos < m_recTarget; ++i) m_ram[m_recPos++] = 0.5f * (l[i] + r[i]);
    if (m_recPos >= m_recTarget)
    {
        m_len = m_recTarget; m_rec = 0; m_recDone = true;
        start = 0.f; end = 1.f; m_gen++; Update();
    }
}

float Sampler::VoicePos(int v) const
{
    if (v < 0 || v >= kVoices || !m_v[v].active || m_len <= 0) return -1.f;
    return (float)(m_v[v].pos / m_len);
}

void Sampler::Overview(uint8_t *out, int cols) const
{
    float peak = 1e-6f;
    for (int i = 0; i < m_len; ++i) { float a = fabsf(m_ram[i]); if (a > peak) peak = a; }
    for (int c = 0; c < cols; ++c)
    {
        int a = (int)((long long)m_len * c / cols), b = (int)((long long)m_len * (c + 1) / cols);
        float m = 0.f;
        for (int i = a; i < b; ++i) { float x = fabsf(m_ram[i]); if (x > m) m = x; }
        int v = (int)(m / peak * 255.f + 0.5f);
        out[c] = (uint8_t)(v > 255 ? 255 : v);
    }
}

// ---- the demo break: one bar of 16ths at 120 BPM (2 s)
void Sampler::MakeDemo()
{
    int len = (int)(2.0f * m_sr);
    if (len > m_cap) len = m_cap;
    for (int i = 0; i < len; ++i) m_ram[i] = 0.f;
    const float s16 = 0.125f * m_sr;
    //                     0 1 2 3 4 5 6 7 8 9 A B C D E F
    static const char K[] = "x......x..x.....";
    static const char S[] = "....x.......x..g";
    static const char H[] = "x.x.x.x.x.x.x.o.";
    unsigned rnd = 1;
    auto noise = [&]() { rnd = rnd * 1664525u + 1013904223u; return (float)(int)(rnd >> 8) / 8388608.f - 1.f; };
    for (int st = 0; st < 16; ++st)
    {
        int at = (int)(st * s16);
        if (K[st] == 'x')                    // kick: sine 150 -> 45 Hz, 300 ms
        {
            float ph = 0.f;
            for (int i = 0; i < (int)(0.35f * m_sr) && at + i < len; ++i)
            {
                float t = i / m_sr, f = 45.f + 105.f * expf(-t * 28.f);
                ph += 6.2831853f * f / m_sr;
                m_ram[at + i] += 0.9f * sinf(ph) * expf(-t * 9.f) + (i < 40 ? 0.3f * noise() * (1.f - i / 40.f) : 0.f);
            }
        }
        if (S[st] != '.')                    // snare: 185 Hz tone + noise, 160 ms (g: ghost)
        {
            float g = S[st] == 'g' ? 0.3f : 1.f, lp = 0.f;
            for (int i = 0; i < (int)(0.2f * m_sr) && at + i < len; ++i)
            {
                float t = i / m_sr, nz = noise();
                lp += 0.45f * (nz - lp);
                m_ram[at + i] += g * (0.35f * sinf(6.2831853f * 185.f * t) * expf(-t * 30.f) + 0.55f * (nz - lp * 0.5f) * expf(-t * 22.f));
            }
        }
        if (H[st] != '.')                    // hats: high-passed noise, closed 40 ms, open 180 ms
        {
            float d = H[st] == 'o' ? 14.f : 90.f, prev = 0.f, a = (st % 4 == 2) ? 0.32f : 0.22f;
            for (int i = 0; i < (int)(0.25f * m_sr) && at + i < len; ++i)
            {
                float t = i / m_sr, nz = noise(), hp = nz - prev; prev = nz;
                m_ram[at + i] += a * hp * expf(-t * d);
            }
        }
    }
    for (int i = 0; i < len; ++i) m_ram[i] = tanhf(m_ram[i]);
    SetLength(len);
    start = 0.f; end = 1.f;
}
