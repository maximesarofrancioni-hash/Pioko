/* filter-lab screens, for a 600 x 450 AMOLED: drawn on a 150 x 112 RGB565 canvas, shown at 4x = 600 x 448
 * (each canvas pixel a 4x4 block - the chunky dot-matrix look of the original 160 x 80 screen; 1 row spare top and bottom).
 * Built on the firmware's own gfx.c (primitives, 3x5 dot-matrix font, themes); the splash is a terminal boot log,
 * following its layout rules: status strip y 0..7, hero zone y 8..101, footer y 102..111 with the four knob columns,
 * popups that take over the hero zone (enter 140 ms, hold 800 ms, exit 110 ms), afterglow on phosphor themes.
 *
 * Screens: HOME (model number + name, bar ring, lane pads), FILTER (live spectrum + cutoff marker),
 *          EUCLID (5 lanes x 16 steps), VINYL (the SP-303 Vinyl Sim on the master bus: platter, gain reduction). */
#include <string.h>
#include <stdio.h>
/* the firmware's board.h says 160 x 80; build.sh makes a copy with 150 x 112 (screen/gen) that gfx.c builds with too */
#include "ui_int.h"

/* layout */
#define FOOT_Y (H - 10)              /* footer rule */
#define GT 37                        /* graph zone top */
#define GB (H - 14)                  /* graph zone bottom (floor) */
#define GH (GB - GT)                 /* graph height */
#define NCOL ((W - 8) / 4 + 1)       /* LED columns on the 4 px pitch: 36 */
#define XR (W - 3)                   /* right edge for right-aligned text */

#define API __attribute__((used, visibility("default")))

const theme_t *T = &EMBER;          /* the theme of the frame being drawn (ui_boot.c reads it) */
uint32_t ui_chord_flash;

enum { SCR_FL_HOME, SCR_FL_FILTER, SCR_FL_EUCLID, SCR_FL_VINYL, SCR_FL_WAVE, SCR_FL_ENV, SCR_FL_LFO, SCR_FL_WFLT, SCR_FL_PLAY, SCR_FL_SHIFT, SCR_FL_MARBLES, SCR_FL_BEADS, SCR_FL_MATRIX, SCR_FL_N };
enum { POP_OFF, POP_FL_KNOB, POP_FL_MODEL };
enum { ENTER_MS = 140, HOLD_MS = 800, EXIT_MS = 110, LANES = 5, BINS = 64 };

static uint16_t fb[W * H];
static uint8_t glow[W * H / 4];
static char sbuf[96];                /* string in-box from JS */

static struct {
    int screen, theme, playing, bpm;
    int filter, nfilt, step, bar, bars_per, sweep;
    int res, cut, env, drv;          /* res x100, cut st, env %, drive dB x10 */
    int fc;                          /* current base cutoff, Hz */
    int sel;                         /* lane being edited */
    int steps[LANES], hits[LANES], rot[LANES], mute[LANES];
    uint32_t trig[LANES];
    uint8_t spec[BINS];              /* 0..42 px (no longer drawn) */
    uint8_t resp[2][64];             /* filter responses (0 filter lab, 1 osc440), one point per LED column, 0..GH px */
    char fam[40], var[40];           /* model name, split: family / variant */
    int v_on, v_mode, v_comp, v_noise, v_wow, v_in, v_gr;   /* vinyl: knobs 0..127, in dB x10, gain reduction dB x10 (<= 0) */
    uint32_t v_peak_t; int v_peak;   /* gain-reduction peak hold */
    /* wave (osc440): the selected oscillator's wave, 128 points -64..63 */
    int8_t w_pts[128];
    int w_osc, w_pos, w_lvl, w_cut, w_res, w_chord, w_held, w_wt;   /* pos x10, lvl %, cut note, res % */
    char w_name[24], w_mode[24], w_info[24];
    /* PLAY: the wave lane - preset group, play mode, octave, volume dB x10, chord 0..3, its Euclid pattern */
    char p_group[24]; int p_mode, p_oct, p_vol, p_steps, p_hits, p_rot, p_on;
    /* shared by SHIFT / MARBLES / TIDES / BEADS: title, subtitle and footer text set from the page */
    char m_title[20], m_sub[44], m_fl[4][6], m_fv[4][8];
    uint16_t sh_bits; int sh_len; int8_t sh_hist[16];
    uint8_t mb_t[32], mb_x1[32], mb_x3[32]; int mb_n;          /* per 16th: t1|t3<<1, X in 0..255 */
    int8_t td[4][128]; int td_n;                                /* Tides outputs, -64..63 */
    int b_time, b_size, b_dens, b_pitch, b_freeze, b_in, b_out; /* Beads, 0..100 */
    char mx_txt[10][16]; int mx_amt[10], mx_n, mx_focus;         /* MATRIX: up to 10 rows, amount -1000..1000 */
    /* ENV: per envelope attack/decay/release ms, sustain %, live level 0..1000 */
    int e_a[3], e_d[3], e_s[3], e_r[3], e_lv[3], e_amt_lad, e_amt_svf, e_gate10, e_ppg[3];
    char e_sub[40];                  /* ENV subtitle: where env 3 goes */
    int e_focus;                     /* 0 env 1 (filters), 1 env 2 (amp), 2 env 3 */
    /* LFO 1: one cycle of its shape (-64..63), phase 0..1000 (-1: no voice), rate x100, delay ms */
    int8_t l_pts[128]; int l_ph, l_rate, l_dly; char l_shape[12];
    /* W FILTER: response in dB, 128 points 20 Hz..20 kHz (0 = -48 dB .. 60 = +12 dB), cutoff x positions */
    uint8_t w_spec[BINS]; int f_svf_hz, f_lad_hz, f_res, f_drv10, f_par, f_svf_note, f_lad_note;   /* WT FLT: wave-lane spectrum, cutoffs in Hz (svf 0: out of the path) */
    /* popup */
    int pop, pop_t0, pop_last, pop_touch, pop_seen, kpos;
    char kname[16], kval[16], kunit[8];
    /* frame pacing */
    int theme_drawn, drawn_any;
    uint32_t drawn, boot_t0;
    int boot_on;
} M;

static const char *const LANE_NAME[LANES] = { "BASS", "LEAD", "STAB", "NOISE", "ACC" };
static const char *const LANE_TAG[LANES] = { "BS", "LD", "ST", "NZ", "AC" };


static bool hit(int l, int s)
{
    int n = M.steps[l], k = M.hits[l];
    if (n <= 0 || k <= 0) return false;
    int i = ((s + M.rot[l]) % n + n) % n;
    return (i * k) % n < k;
}

/* ---------------------------------------------------------------- chrome */
static void status(uint32_t now)
{
    static const char *const NAMES[SCR_FL_N] = { "HOME", "FILTER", "EUCLID", "VINYL", "WAVE", "ENV", "TIDES", "WT FLT", "PLAY", "SHIFT", "MARBLES", "BEADS", "MATRIX" };
    char b[16];
    gfx_rect(0, 0, W, 7, T->bg);
    snprintf(b, sizeof b, "%02d", M.screen + 1); gfx_text(b, 2, 1, T->dim, 1);
    gfx_text(NAMES[M.screen], 12, 1, T->ink, 1);
    snprintf(b, sizeof b, "F%02d/%02d", M.filter + 1, M.nfilt); gfx_text(b, 46, 1, tk(1), 1);
    int x = W - 2 - gfx_text_w("BPM", 1);
    gfx_text("BPM", x, 1, T->dim, 1);
    snprintf(b, sizeof b, "%d", M.bpm); x -= 2 + gfx_text_w(b, 1); gfx_text(b, x, 1, T->ink, 1);
    int beat = (M.step >> 2) & 3;
    bool early = (M.step & 3) == 0;
    x -= 18;
    for (int i = 0; i < 4; i++)
        gfx_rect(x + i * 4, 2, 3, 3, i == beat && M.playing && early ? (i == 0 ? T->accent : T->ink) : T->line);
    const char *mode = M.bars_per ? "AUTO" : "HOLD";
    int cx = x - 4 - gfx_text_w(mode, 1);
    gfx_text(mode, cx, 1, M.bars_per ? T->dim : T->warn, 1);
    gfx_rect(0, UI_STATUS_H - 1, W, 1, T->line);
}

void ui_footer(const char *l0, const char *v0, const char *l1, const char *v1,
               const char *l2, const char *v2, const char *l3, const char *v3)
{
    const char *c[4][2] = { { l0, v0 }, { l1, v1 }, { l2, v2 }, { l3, v3 } };
    gfx_rect(0, FOOT_Y, W, 1, T->line);
    for (int i = 0; i < 4; i++) {
        int x = i * (W / 4) + 2;
        gfx_text(c[i][0], x, FOOT_Y + 3, T->dim, 1);
        gfx_text(c[i][1], x + gfx_text_w(c[i][0], 1) + 3, FOOT_Y + 3, tk(i), 1);
    }
}

static void knob_footer(void)
{
    char r[8], c[8], e[8], d[8];
    snprintf(r, sizeof r, "%d.%02d", M.res / 100, M.res % 100);
    snprintf(c, sizeof c, "%+d", M.cut);
    snprintf(e, sizeof e, "%d", M.env);
    snprintf(d, sizeof d, "%+d", M.drv / 10);
    ui_footer("RES", r, "CUT", c, "ENV", e, "DRV", d);
}

static void hz_text(char *b, int n, int hz)
{
    if (hz >= 10000) snprintf(b, n, "%dK", hz / 1000);
    else if (hz >= 1000) snprintf(b, n, "%d.%dK", hz / 1000, hz / 100 % 10);
    else snprintf(b, n, "%d", hz);
}

/* ---------------------------------------------------------------- HOME: model number, name, bar ring, lane pads */
static void scr_home(uint32_t now)
{
    char b[24];
    snprintf(b, sizeof b, "%02d", M.filter + 1);
    gfx_text(b, 3, 11, T->ink, 6);
    int nx = 3 + gfx_text_w(b, 6) + 4;
    int sc = nx + gfx_text_w(M.fam, 2) <= 104 ? 2 : 1;
    gfx_text(M.fam, nx, 11, T->ink, sc);
    gfx_text(M.var, nx, 11 + 5 * sc + 3, T->dim, 1);
    /* chip: auto/hold, like the track chip */
    snprintf(b, sizeof b, M.bars_per ? "%dB" : "HLD", M.bars_per);
    int cw = gfx_text_w(b, 1) + 3;
    gfx_rect(nx, 33, cw, 7, tk(1)); gfx_text(b, nx + 2, 34, T->bg, 1);
    gfx_text(M.sweep ? "SWEEP ON" : "SWEEP OFF", nx + cw + 4, 34, T->ink, 1);
    char hz[12]; hz_text(hz, sizeof hz, M.fc);
    snprintf(b, sizeof b, "FC %sHZ", hz); gfx_text(b, 3, 47, T->accent, 2);   /* what the filter is at now */
    /* bar ring: 16 steps, the bar of this filter in the centre */
    for (int k = 0; k < 16; k++) {
        int x, y; gfx_ring_pt(128, 32, 17, 16, 49152, 65536, k, &x, &y);
        gfx_rect(x - 1, y - 1, 2, 2, !M.playing ? T->line : k == M.step ? T->accent : k < M.step ? T->ink : T->line);
    }
    int per = M.bars_per ? M.bars_per : 1;
    snprintf(b, sizeof b, "%d/%d", M.bars_per ? (M.bar % per) + 1 : M.bar + 1, M.bars_per ? per : 0);
    if (!M.bars_per) snprintf(b, sizeof b, "%d", M.bar + 1);
    gfx_text(b, 128 - (gfx_text_w(b, 1) >> 1), 28, T->ink, 1);
    gfx_text("BAR", 128 - (gfx_text_w("BAR", 1) >> 1), 34, T->dim, 1);
    /* lane pads: lit as they trigger */
    for (int i = 0; i < LANES; i++) {
        int x = i * 30 + 1, w = 28, y = 66, h = 30;
        bool lit = M.playing && (int32_t)(now - M.trig[i]) < 110 && !M.mute[i];
        if (lit) {
            gfx_rect(x, y, w, h, T->surf); gfx_dither(x, y, w, h, T->line);
            gfx_frame(x, y, w, h, T->accent);
        } else gfx_frame(x, y, w, h, i == M.sel ? T->dim : T->line);
        gfx_text(LANE_NAME[i], x + w / 2 - (gfx_text_w(LANE_NAME[i], 1) >> 1), y + h / 2 - 2,
                 M.mute[i] ? T->line : lit ? T->accent : T->dim, 1);
    }
    knob_footer();
}

/* ---------------------------------------------------------------- FILTER: spectrum, cutoff marker */
static int fx_of(int hz)            /* 30 Hz .. 16 kHz, log, onto x 2..W-3 */
{
    if (hz < 30) hz = 30;
    float r = __builtin_logf((float)hz / 30.f) / __builtin_logf(16000.f / 30.f);
    int x = 2 + (int)(r * (float)(W - 5));
    return x < 2 ? 2 : x > W - 3 ? W - 3 : x;
}

/* Filter response as an LED matrix: 39 columns on a 4 px pitch (30 Hz .. 16 kHz), rows every 4 px.
 * The curve is a round LED (3x3 without corners) at its exact height; the matrix below it is lit dim
 * (the infill), the LEDs above are faint. The column at the cutoff is lit in the marker colour. */
static void led_round(int x, int y, uint16_t c) { gfx_rect(x - 1, y, 3, 1, c); gfx_rect(x, y - 1, 1, 3, c); }
static int led_col(int x) { int i = (x - 4 + 2) / 4; return i < 0 ? 0 : (i > NCOL - 1 ? NCOL - 1 : i); }
static void resp_graph(const uint8_t *r, int mark1, uint16_t c1, int mark2, uint16_t c2)
{
    const int top = GT, bot = GB;
    for (int i = 0; i < NCOL; i++) {
        int x = 4 + i * 4, h = r[i] > GH ? GH : r[i], cy = bot - h;
        uint16_t mc = i == mark1 ? c1 : (i == mark2 ? c2 : 0);
        for (int y = bot; y >= top; y -= 4) {
            if (y > cy + 2) gfx_rect(x - 1, y - 1, 2, 2, mc ? mc : T->dim);
            else if (y < cy - 2) gfx_px(x, y, T->line);
        }
        led_round(x, cy, mc ? mc : T->ink);
    }
}

/* every screen's head: the title as big as fits (3x, 2x or 1x), the subtitle under it; returns its y */
static int mod_head_text(const char *title, const char *sub)
{
    int sc = 4 + gfx_text_w(title, 2) <= W - 4 ? 2 : 1;
    gfx_text(title, 4, 11, T->ink, sc);
    int y = 11 + 5 * sc + 4;
    gfx_text(sub, 4, y, T->dim, 1);
    return y;
}

static void scr_filter(void)
{
    char b[48];
    int ly = mod_head_text(M.fam, M.var);
    /* legend, right of the subtitle: the lab's curve (filled) and the wave lane's (osc440 SVF + ladder) */
    gfx_rect(XR - 41, ly + 1, 3, 3, T->ink); gfx_text("LAB", XR - 36, ly, T->ink, 1);
    led_round(XR - 16, ly + 2, tk(1)); gfx_text("WT", XR - 12 + 4, ly, tk(1), 1);
    int cx = fx_of(M.fc);
    resp_graph(M.resp[0], led_col(cx), T->accent, -1, 0);
    /* the wave lane's filters over it: round LEDs in the second colour, no infill */
    for (int i = 0; i < NCOL; i++) {
        int h = M.resp[1][i] > GH ? GH : M.resp[1][i];
        led_round(4 + i * 4, GB - h, tk(1));
    }
    char hz[12]; hz_text(hz, sizeof hz, M.fc);
    snprintf(b, sizeof b, "%sHZ", hz);
    int tw = gfx_text_w(b, 1), tx = cx + 3 + tw > W - 2 ? cx - 3 - tw : cx + 3;
    gfx_rect(tx - 1, GT - 9, tw + 2, 7, T->bg);
    gfx_text(b, tx, GT - 8, T->accent, 1);
    knob_footer();
}

/* ---------------------------------------------------------------- EUCLID: 5 lanes x 16 steps */
static void scr_euclid(void)
{
    char b[12];
    for (int l = 0; l < LANES; l++) {
        int y = 11 + l * 18, ch = 15;                          /* rows of 18 px, cells 15 px tall */
        bool act = l == M.sel;
        if (act) { gfx_rect(0, y + 4, 12, 7, tk(1)); gfx_text(LANE_TAG[l], 2, y + 5, T->bg, 1); }
        else gfx_text(LANE_TAG[l], 2, y + 5, M.mute[l] ? T->line : T->dim, 1);
        if (M.mute[l]) gfx_text("M", 14, y + 5, T->warn, 1);
        for (int s = 0; s < 16; s++) {
            int x = 18 + s * 6;
            bool h = hit(l, s), now = M.playing && s == M.step;
            if (h) gfx_rect(x, y, 5, ch, now ? T->accent : M.mute[l] ? T->line : act ? T->ink : T->dim);
            else if (now) gfx_frame(x, y, 5, ch, T->accent);
            else gfx_rect(x + 2, y + ch / 2, 1, 1, T->line);
        }
        snprintf(b, sizeof b, "%d/%d", M.hits[l], M.steps[l]);
        gfx_text(b, XR - gfx_text_w(b, 1), y + 5, act ? T->ink : T->dim, 1);
    }
    char h[6], n[6], r[6];
    int s = M.sel;
    snprintf(h, sizeof h, "%d", M.hits[s]); snprintf(n, sizeof n, "%d", M.steps[s]); snprintf(r, sizeof r, "%d", M.rot[s]);
    ui_footer("LN", LANE_TAG[s], "HIT", h, "STP", n, "ROT", r);
}

/* ---------------------------------------------------------------- VINYL: platter, mode, gain reduction meter */
static void scr_vinyl(uint32_t now)
{
    char b[24];
    /* platter: 24 dots turning at 33 1/3 rpm, the label dot in the accent; wow wobbles its speed */
    int32_t wob = M.v_wow * (int32_t)gfx_sin(now * 65536u / 1700u) / 32768;      /* +-127 */
    uint32_t ph = (uint32_t)((uint64_t)now * 65536u * 5u / 9000u) + (uint32_t)(wob * 40);   /* 33.3 rpm = 0.555 rev/s */
    const int px = 32, py = 55;                                 /* the platter's centre */
    for (int r = 0; r < 4; r++) {
        int rad = 29 - r * 7, n = 26 - r * 5;
        for (int k = 0; k < n; k++) {
            int x, y; gfx_ring_pt(px, py, rad, n, (int32_t)ph, 65536, k, &x, &y);
            bool mark = r == 0 && k == 0;
            gfx_rect(x - 1, y - 1, 2, 2, !M.playing ? T->line : mark ? T->accent : r == 0 ? T->ink : T->dim);
        }
    }
    gfx_rect(px - 1, py - 1, 3, 3, M.v_on ? T->accent : T->line);  /* spindle */
    if (M.v_noise && M.playing && M.v_on) {                      /* crackle: a few hashed dots on the record */
        uint32_t h = now / 45 * 2654435761u;
        int c = 1 + M.v_noise / 32;
        for (int i = 0; i < c; i++) {
            h ^= h >> 13; h *= 0x5bd1e995u; h ^= h >> 15;
            int x, y; gfx_ring_pt(px, py, 6 + (int)(h % 22), 48, 0, 65536, (int)(h >> 8) % 48, &x, &y);
            gfx_px(x, y, T->ink);
        }
    }
    /* right: name, model, state */
    const int rx = 68;
    gfx_text("VINYL SIM", rx, 11, T->ink, 2);
    gfx_text(M.v_mode ? "RECONSTRUCTED" : "FIRMWARE", rx, 25, T->dim, 1);
    if (!M.v_on) { gfx_text("BYPASS", rx, 44, T->warn, 2); }
    else {
        int gr = M.v_gr < -240 ? -240 : M.v_gr;                   /* down to -24 dB */
        if (gr < M.v_peak || (int32_t)(now - M.v_peak_t) > 900) { M.v_peak = gr; M.v_peak_t = now; }
        snprintf(b, sizeof b, "%d.%d", -gr / 10, -gr % 10);
        int e = gfx_text(b, rx, 36, T->ink, 3);
        gfx_text("GR", e + 3, 40, T->dim, 1);
        gfx_text("DB", e + 3, 46, T->dim, 1);
        /* meter: 20 cells over 24 dB, filled from the left as the gain drops */
        for (int i = 0; i < 20; i++) {
            int lo = i * 12, hi = (i + 1) * 12;
            bool on = lo < -gr, pk = M.v_peak < 0 && lo <= -M.v_peak && hi > -M.v_peak;
            gfx_rect(rx + i * 4, 62, 3, 12, on ? (i < 5 ? T->ink : T->accent) : pk ? T->dim : T->line);
        }
        gfx_text("0", rx, 77, T->dim, 1); gfx_text("12", rx + 37, 77, T->dim, 1); gfx_text("24", rx + 79 - gfx_text_w("24", 1), 77, T->dim, 1);
    }
    char c[6], n[6], w[6], in[8];
    snprintf(c, sizeof c, "%d", M.v_comp); snprintf(n, sizeof n, "%d", M.v_noise); snprintf(w, sizeof w, "%d", M.v_wow);
    snprintf(in, sizeof in, "%+d", M.v_in / 10);
    ui_footer("CMP", c, "NOI", n, "WOW", w, "IN", in);
}

/* ---------------------------------------------------------------- WAVE: osc440, the selected oscillator's wave */
static void scr_wave(uint32_t now)
{
    char b[24];
    /* the wave: 96 x 40 box, dotted centre line, 128 points squeezed to 96 columns */
    const int x0 = 3, y0 = 12, bw = 82, bh = 62, cy = y0 + bh / 2;
    gfx_frame(x0 - 1, y0 - 1, bw + 2, bh + 2, T->line);
    gfx_line(x0, cy, x0 + bw - 1, cy, T->line, true);
    int py = -1;
    for (int x = 0; x < bw; x++) {
        int v = M.w_pts[x * 128 / bw];   /* 128 points over the box */
        int y = cy - v * (bh / 2 - 1) / 64;
        if (py >= 0) gfx_line(x0 + x - 1, py, x0 + x, y, T->ink, false); else gfx_px(x0 + x, y, T->ink);
        py = y;
    }
    /* table position: 64 waves as ticks, the current one in the accent */
    if (M.w_wt) {
        for (int i = 0; i < 64; i++) {
            int x = x0 + i * 5 / 4;
            bool cur = i == M.w_pos / 10;
            gfx_rect(x, 79, 1, cur ? 7 : (i % 8 ? 2 : 4), cur ? T->accent : T->dim);
        }
        snprintf(b, sizeof b, "%d.%d", M.w_pos / 10, M.w_pos % 10);
        gfx_text(b, x0 + bw - gfx_text_w(b, 2), 89, T->accent, 2);
        gfx_text("POS", x0, 92, T->dim, 1);
    } else gfx_text(M.w_mode, x0, 82, T->dim, 2);
    /* right column: preset, oscillator, what plays */
    int rx = 90;
    int sc = rx + gfx_text_w(M.w_name, 2) <= W - 2 ? 2 : 1;
    gfx_text(M.w_name, rx, 11, T->ink, sc);
    int y = sc == 2 ? 26 : 20;
    for (int i = 0; i < 3; i++) {
        bool act = i == M.w_osc;
        snprintf(b, sizeof b, "%d", i + 1);
        if (act) { gfx_rect(rx + i * 9, y, 8, 7, tk(1)); gfx_text(b, rx + i * 9 + 2, y + 1, T->bg, 1); }
        else { gfx_frame(rx + i * 9, y, 8, 7, T->line); gfx_text(b, rx + i * 9 + 2, y + 1, T->dim, 1); }
    }
    gfx_text("OSC", rx + 29, y + 1, T->dim, 1);
    gfx_text(M.w_wt ? M.w_mode : "VA", rx, y + 11, T->ink, 1);
    gfx_text(M.w_info, rx, y + 20, T->dim, 1);
    /* voices held */
    gfx_text("VOICES", rx, y + 33, T->dim, 1);
    for (int i = 0; i < 16; i++) gfx_rect(rx + (i % 8) * 7, y + 41 + (i / 8) * 7, 5, 5, i < M.w_held ? (M.playing ? T->accent : T->ink) : T->line);
    char p[8], l[8], c[8], r[8];
    snprintf(p, sizeof p, "%d", M.w_pos / 10); snprintf(l, sizeof l, "%d", M.w_lvl);
    snprintf(c, sizeof c, "%d", M.w_cut); snprintf(r, sizeof r, "%d", M.w_res);
    ui_footer("POS", p, "LVL", l, "CUT", c, "RES", r);
}

/* ---------------------------------------------------------------- ENV: env 1 (filters) and env 2 (amp) */
static int tw(int ms) { /* time -> width, log-ish: 1 ms ~ 1 px, 10 s ~ 15 px */
    int w = 1; while (ms > 1 && w < 16) { ms = ms * 10 / 18; w++; } return w;
}
static void ms_text(char *b, int n, int ms)
{
    if (ms < 1000) snprintf(b, n, "%d", ms); else snprintf(b, n, "%d.%dS", ms / 1000, ms / 100 % 10);
}
/* Both envelopes share one time axis in the same graph zone as the FILTER spectrum (y 30..69):
 * env 2 (amp) solid in the ink, env 1 (filters) dotted in dim; the live levels in the accent. */
/* osc440's Dadsr shapes: exponential mode attacks toward 1.3 (1.466 time constants, reaching 1 at the end),
 * decays and releases fall over 4.6 time constants; PPG mode is linear. One point per pixel. */
static int env_level(int i, int seg, float t, int sy, int top, int bot)
{
    float s = M.e_s[i] / 100.f, v;
    bool ex = !M.e_ppg[i];
    switch (seg) {
    case 0: v = ex ? 1.3f * (1.f - __builtin_expf(-1.466f * t)) : t; break;
    case 1: v = ex ? s + (1.f - s) * __builtin_expf(-4.6f * t) : 1.f - (1.f - s) * t; break;
    case 2: v = s; break;
    default: v = ex ? s * __builtin_expf(-4.6f * t) : s * (1.f - t); break;
    }
    if (v > 1.f) v = 1.f;
    if (v < 0.f) v = 0.f;
    (void)sy;
    return bot - (int)(v * (float)(bot - top) + 0.5f);
}
static void env_curve(int i, int x0, int w, float k, int top, int bot, uint16_t col, bool dotted)
{
    int len[4];
    len[0] = (int)(tw(M.e_a[i]) * k); len[1] = (int)(tw(M.e_d[i]) * k); len[3] = (int)(tw(M.e_r[i]) * k);
    len[2] = w - len[0] - len[1] - len[3];
    int x = x0, py = bot;
    for (int seg = 0; seg < 4; seg++) {
        int n = len[seg] < 1 ? 1 : len[seg];
        for (int j = 1; j <= n; j++) {
            int y = env_level(i, seg, (float)j / (float)n, 0, top, bot);
            int xx = x + j;
            if (!dotted || (xx & 1) == 0) gfx_line(xx - 1, py, xx, y, col, false);
            py = y;
        }
        x += n;
    }
}
/* ENV in the FILTER screen's style: the same LED matrix. The amp envelope is the filled curve, the filter
 * envelope round LEDs in the second colour; while a note sounds, the infill lights in the accent up to the
 * amp envelope's live level. Both share one time axis (the longer one fills the width). */
static void env_columns(int i, float k, uint8_t *out)
{
    int len[4], w = NCOL * 4;
    len[0] = (int)(tw(M.e_a[i]) * k); len[1] = (int)(tw(M.e_d[i]) * k); len[3] = (int)(tw(M.e_r[i]) * k);
    len[2] = w - len[0] - len[1] - len[3];
    for (int c = 0; c < NCOL; c++) {
        int x = c * 4 + 2, seg = 0;
        while (seg < 3 && x >= len[seg]) { x -= len[seg]; seg++; }
        int n = len[seg] < 1 ? 1 : len[seg];
        float t = (float)x / (float)n; if (t > 1.f) t = 1.f;
        out[c] = (uint8_t)(GB - env_level(i, seg, t, 0, GT, GB));
    }
}
/* three envelopes, told apart by the shape of their LEDs as well as their colour:
 *   AMP (env 2) - the filled curve: round LEDs in the ink, the matrix lit below it (accent up to the live level)
 *   FLT (env 1) - crosses (round LEDs) in the second colour
 *   E3  (env 3) - hollow squares in the third colour
 * All on one time axis; the longest fills the width. */
static void led_ring(int x, int y, uint16_t c) { gfx_frame(x - 1, y - 1, 3, 3, c); }
/* the two overlay envelopes, big enough to read over the filled one: a 5x5 plus and a 5x5 box,
 * each cut out of what is under it by a background halo */
static void env_plus(int x, int y, uint16_t c) { gfx_rect(x - 3, y - 1, 7, 3, T->bg); gfx_rect(x - 1, y - 3, 3, 7, T->bg); gfx_rect(x - 2, y, 5, 1, c); gfx_rect(x, y - 2, 1, 5, c); }
static void env_box(int x, int y, uint16_t c) { gfx_rect(x - 3, y - 3, 7, 7, T->bg); gfx_frame(x - 2, y - 2, 5, 5, c); }
/* The envelope in focus (the one being edited) is drawn brightest and filled; the others step down in
 * brightness: the next one in the dim colour, the last one in the line colour. Each keeps its own mark
 * (AMP round LEDs, FLT +, E3 boxes) so they stay recognisable in any theme. */
static void env_mark(int env, int x, int y, uint16_t c)
{
    if (env == 1) { gfx_rect(x - 2, y - 2, 5, 5, T->bg); led_round(x, y, c); }
    else if (env == 0) env_plus(x, y, c);
    else env_box(x, y, c);
}
static void scr_env(void)
{
    static const char *const NAME[3] = { "1 FILTERS", "2 AMP", "3 MOD" };
    static const char *const SHORT[3] = { "FLT", "AMP", "E3" };
    const int f = M.e_focus % 3, order[3] = { f, (f + 1) % 3, (f + 2) % 3 };
    uint16_t col[3];
    col[order[0]] = T->ink; col[order[1]] = T->dim; col[order[2]] = T->line;
    /* subtitle: the one in focus (and where env 3 goes when it is) */
    char b[48];
    snprintf(b, sizeof b, "%s  %s", NAME[f], f == 2 ? M.e_sub + (M.e_sub[0] == 'E' && M.e_sub[1] == '3' ? 3 : 0) : (M.e_ppg[f] ? "LINEAR" : "EXP"));
    int ly = mod_head_text("ENVELOPES", b);
    for (int i = 0; i < 3; i++) gfx_text(M.e_ppg[i] ? "L" : "E", XR - 11 + i * 4, 12, col[i], 1);
    /* legend, brightest first */
    int lx = XR + 1;
    for (int k = 2; k >= 0; k--) {
        int e = order[k];
        lx -= gfx_text_w(SHORT[e], 1);
        gfx_text(SHORT[e], lx, ly, col[e], 1);
        lx -= 5; env_mark(e, lx + 1, ly + 2, col[e]); lx -= 5;
    }
    int n = 0;
    for (int i = 0; i < 3; i++) { int m = tw(M.e_a[i]) + tw(M.e_d[i]) + tw(M.e_r[i]); if (m > n) n = m; }
    float k = (float)(NCOL * 4 - NCOL * 4 / 5) / (float)(n ? n : 1);
    uint8_t cv[3][NCOL];
    for (int i = 0; i < 3; i++) env_columns(i, k, cv[i]);
    const int top = GT, bot = GB;
    /* the focused one's fill: dim, lit in the accent up to its live level while it sounds */
    int live = M.playing && M.e_lv[f] > 0 ? (bot - top) * M.e_lv[f] / 1000 : -1;
    for (int c = 0; c < NCOL; c++) {
        int x = 4 + c * 4, cy = bot - (cv[f][c] > GH ? GH : cv[f][c]);
        for (int y = bot; y >= top; y -= 4) {
            if (y > cy + 2) gfx_rect(x - 1, y - 1, 2, 2, bot - y <= live ? T->accent : T->dim);
            else if (y < cy - 2) gfx_px(x, y, T->line);
        }
    }
    /* darkest first, so the brighter ones sit on top; the two others on alternate columns */
    for (int c = 1; c < NCOL; c += 2) env_mark(order[2], 4 + c * 4, bot - (cv[order[2]][c] > GH ? GH : cv[order[2]][c]), col[order[2]]);
    for (int c = 0; c < NCOL; c += 2) env_mark(order[1], 4 + c * 4, bot - (cv[order[1]][c] > GH ? GH : cv[order[1]][c]), col[order[1]]);
    for (int c = 0; c < NCOL; c++) {
        int y = bot - (cv[f][c] > GH ? GH : cv[f][c]);
        if (f == 1) led_round(4 + c * 4, y, T->ink); else env_mark(f, 4 + c * 4, y, T->ink);
    }
    /* live levels at the right edge, in each one's brightness */
    for (int i = 0; i < 3; i++)
        if (M.playing && M.e_lv[i] > 0) env_mark(i, W - 3, bot - (bot - top) * M.e_lv[i] / 1000, i == f ? T->accent : col[i]);
    char a[8], d[8], s[8], r[8];
    ms_text(a, sizeof a, M.e_a[f]); ms_text(d, sizeof d, M.e_d[f]); snprintf(s, sizeof s, "%d", M.e_s[f]); ms_text(r, sizeof r, M.e_r[f]);
    ui_footer("ATK", a, "DEC", d, "SUS", s, "REL", r);
}

/* ---------------------------------------------------------------- LFO 1: one cycle, live phase */
static void scr_lfo(void)
{
    const int x0 = 2, y0 = 10, bw = 112, bh = 44, cy = y0 + bh / 2;
    gfx_frame(x0 - 1, y0 - 1, bw + 2, bh + 2, T->line);
    gfx_line(x0, cy, x0 + bw - 1, cy, T->line, true);
    int py = -1;
    for (int x = 0; x < bw; x++) {
        int y = cy - M.l_pts[x * 128 / bw] * (bh / 2 - 2) / 64;
        if (py >= 0) gfx_line(x0 + x - 1, py, x0 + x, y, T->ink, false); else gfx_px(x0 + x, y, T->ink);
        py = y;
    }
    if (M.l_ph >= 0 && M.playing) {
        int x = x0 + M.l_ph * (bw - 1) / 1000, y = cy - M.l_pts[M.l_ph * 127 / 1000] * (bh / 2 - 2) / 64;
        gfx_line(x, y0, x, y0 + bh - 1, T->accent, true);
        gfx_rect(x - 1, y - 1, 3, 3, T->accent);
    }
    char b[16];
    gfx_text(M.l_shape, 118, 10, T->ink, 2);
    snprintf(b, sizeof b, "%d.%02dHZ", M.l_rate / 100, M.l_rate % 100); gfx_text(b, 118, 24, T->ink, 1);
    if (M.l_dly) { snprintf(b, sizeof b, "DLY %d", M.l_dly); gfx_text(b, 118, 32, T->dim, 1); }
    gfx_text(M.w_name, 118, 46, T->dim, 1);
    char r[10], d[8];
    snprintf(r, sizeof r, "%d.%02d", M.l_rate / 100, M.l_rate % 100); snprintf(d, sizeof d, "%d", M.l_dly);
    ui_footer("RATE", r, "SHP", M.l_shape, "DLY", d, "PH", M.l_ph >= 0 && M.playing ? "RUN" : "-");
}

/* ---------------------------------------------------------------- WT FLT: the FILTER screen for osc440's filters:
 * live spectrum of the wave lane alone, ladder and SVF cutoff lines */
static void scr_wflt(void)
{
    char b[40];
    const char *route = M.f_par ? "PARALLEL" : "SVF > LADDER";
    int sc = 4 + gfx_text_w(route, 2) <= W - 4 ? 2 : 1;
    gfx_text(route, 4, 10, T->ink, sc);
    snprintf(b, sizeof b, "LADDER RES %d  DRIVE %d.%d", M.f_res, M.f_drv10 / 10, M.f_drv10 % 10);
    gfx_text(b, 4, sc == 2 ? 22 : 17, T->dim, 1);
    char hz[12];
    int cx = fx_of(M.f_lad_hz), sx = M.f_svf_hz > 0 ? fx_of(M.f_svf_hz) : -100;
    resp_graph(M.resp[1], led_col(cx), T->accent, M.f_svf_hz > 0 ? led_col(sx) : -1, tk(1));
    if (M.f_svf_hz > 0) { gfx_rect(sx - 2, 29, 5, 7, T->bg); gfx_text("S", sx - 1, 30, tk(1), 1); }
    hz_text(hz, sizeof hz, M.f_lad_hz);
    snprintf(b, sizeof b, "%sHZ", hz);
    int tw = gfx_text_w(b, 1), tx = cx + 3 + tw > W - 2 ? cx - 3 - tw : cx + 3;
    gfx_rect(tx - 1, 29, tw + 2, 7, T->bg);
    gfx_text(b, tx, 30, T->accent, 1);
    char s[8], l[8], r[8], d[8];
    snprintf(s, sizeof s, "%d", M.f_svf_note); snprintf(l, sizeof l, "%d", M.f_lad_note);
    snprintf(r, sizeof r, "%d", M.f_res); snprintf(d, sizeof d, "%d.%d", M.f_drv10 / 10, M.f_drv10 % 10);
    ui_footer("SVF", s, "LAD", l, "RES", r, "DRV", d);
}

/* ---------------------------------------------------------------- PLAY: the wave lane - preset, chords, pattern */
static void scr_play(uint32_t now)
{
    static const char *const CH[4] = { "AM", "F", "C", "G" };
    static const char *const MODE[5] = { "CHORD", "ARP", "BASS", "SHIFT", "MARBLES" };
    char b[40];
    snprintf(b, sizeof b, "%s  %s  OCT %+d", M.p_group, MODE[M.p_mode % 5], M.p_oct);
    mod_head_text(M.w_name, b);
    /* chord pads, like the lane pads on HOME: the bar's chord framed, lit while notes sound */
    int cur = M.bar & 3;
    for (int i = 0; i < 4; i++) {
        int x = i * 37 + 1, w = 35, y = 36, h = 30;
        bool now_ = M.playing && i == cur, lit = now_ && M.w_held > 0 && M.p_on;
        if (lit) { gfx_rect(x, y, w, h, T->surf); gfx_dither(x, y, w, h, T->line); gfx_frame(x, y, w, h, T->accent); }
        else gfx_frame(x, y, w, h, now_ ? T->dim : T->line);
        gfx_text(CH[i], x + w / 2 - (gfx_text_w(CH[i], 2) >> 1), y + h / 2 - 4, lit ? T->accent : now_ ? T->ink : T->dim, 2);
    }
    /* the lane's Euclid pattern, cells as on EUCLID, playhead in the accent */
    gfx_text("WV", 2, 78, M.p_on ? T->dim : T->line, 1);
    if (!M.p_on) gfx_text("M", 12, 78, T->warn, 1);
    for (int s = 0; s < 16; s++) {
        int x = 18 + s * 6, y = 72, n = M.p_steps, k = M.p_hits;
        bool h = false;
        if (n > 0 && k > 0) { int i = ((s + M.p_rot) % n + n) % n; h = (i * k) % n < k; }
        bool here = M.playing && s == M.step;
        if (h) gfx_rect(x, y, 5, 17, here ? T->accent : M.p_on ? T->ink : T->line);
        else if (here) gfx_frame(x, y, 5, 17, T->accent);
        else gfx_rect(x + 2, y + 8, 1, 1, T->line);
    }
    snprintf(b, sizeof b, "%d/%d", M.p_hits, M.p_steps);
    gfx_text(b, XR - gfx_text_w(b, 1), 78, T->dim, 1);
    char o[8], g[8], v[8];
    snprintf(o, sizeof o, "%+d", M.p_oct); snprintf(g, sizeof g, "%d.%d", M.e_gate10 / 10, M.e_gate10 % 10);
    snprintf(v, sizeof v, "%+d", M.p_vol / 10);
    static const char *const MS[5] = { "CHD", "ARP", "BAS", "SHF", "MRB" };
    ui_footer("MOD", MS[M.p_mode % 5], "OCT", o, "GATE", g, "VOL", v);
}

/* ---------------------------------------------------------------- shared head and foot of the module screens */
static int mod_head(void) { return mod_head_text(M.m_title, M.m_sub); }
static void mod_foot(void) { ui_footer(M.m_fl[0], M.m_fv[0], M.m_fl[1], M.m_fv[1], M.m_fl[2], M.m_fv[2], M.m_fl[3], M.m_fv[3]); }

/* SHIFT: the 16 register bits as LEDs (the loop length lit), then the last 16 notes as bars */
static void scr_shift(void)
{
    mod_head();
    for (int i = 0; i < 16; i++) {
        int x = 3 + i * 9, bit = 15 - i;                      /* bit 15 left, bit 0 (the output) right */
        bool in = bit < M.sh_len, on = (M.sh_bits >> bit) & 1;
        if (!in) { gfx_rect(x + 3, GT + 3, 1, 1, T->line); continue; }
        if (on) gfx_rect(x, GT, 7, 7, bit == 0 ? T->accent : T->ink);
        else gfx_frame(x, GT, 7, 7, T->line);
    }
    int mx = 1;
    for (int i = 0; i < 16; i++) if (M.sh_hist[i] > mx) mx = M.sh_hist[i];
    for (int i = 0; i < 16; i++) {
        if (M.sh_hist[i] < 0) continue;
        int h = 2 + M.sh_hist[i] * (GB - GT - 14) / (mx < 12 ? 12 : mx), x = 3 + i * 9;
        for (int j = 0; j < h; j += 2) gfx_rect(x, GB - j, 7, 1, i == 15 ? T->accent : (j + 2 >= h ? T->ink : T->dim));
    }
    mod_foot();
}

/* MARBLES: X1 (ink) and X3 (dim) as dots over the last 32 steps, T1 and T3 as gate rows */
static void scr_marbles(void)
{
    mod_head();
    for (int i = 1; i < 4; i++) gfx_line(2 + i * (W - 4) / 4, GT, 2 + i * (W - 4) / 4, GB, T->line, true);
    const int xb = GB - 14;                                   /* X zone floor; the gate rows below it */
    int n = M.mb_n < 32 ? M.mb_n : 32;
    for (int k = 0; k < n; k++) {
        int i = (M.mb_n - n + k) & 31, x = 3 + (32 - n + k) * (W - 8) / 32;
        bool last = k == n - 1;
        int y1 = xb - M.mb_x1[i] * (xb - GT) / 255, y3 = xb - M.mb_x3[i] * (xb - GT) / 255;
        gfx_rect(x, y3, 2, 2, T->dim);
        gfx_rect(x, y1, 3, 3, last ? T->accent : T->ink);
        if (M.mb_t[i] & 1) gfx_rect(x, xb + 6, 3, 3, last ? T->accent : T->ink);
        if (M.mb_t[i] & 2) gfx_rect(x, xb + 11, 3, 3, T->dim);
    }
    mod_foot();
}

/* TIDES: the four outputs scrolling, one colour each (the footer's column colours) */
static void scr_tides(void)
{
    mod_head();
    const int top = GT, bot = GB, cy = (top + bot) / 2;
    gfx_line(2, cy, W - 3, cy, T->line, true);
    for (int ch = 3; ch >= 0; ch--) {
        int py = -1;
        for (int x = 0; x < 128; x++) {
            int i = (M.td_n - 128 + x) & 127;
            int y = cy - M.td[ch][i] * (bot - top) / 2 / 64;
            int px = 2 + x * (W - 5) / 128;
            static const int8_t kCol[4] = { 0, 1, 2, 2 };    /* ink, accent, dim, dim dotted */
            uint16_t col = kCol[ch] == 0 ? T->ink : kCol[ch] == 1 ? T->accent : T->dim;
            if (py >= 0) gfx_line(px - 1, py, px, y, col, ch == 3);
            py = y;
        }
    }
    mod_foot();
}

/* BEADS: the grain cloud - where (TIME) and how wide (SIZE) it reads in the buffer, how dense (DENSITY),
 * pitch as the dots' height; FREEZE as a band; input and output levels at the right */
static void scr_beads(uint32_t now)
{
    mod_head();
    const int x0 = 2, w = W - 30, top = GT, bot = GB;
    gfx_line(x0, bot + 1, x0 + w, bot + 1, T->line, false);
    int cx = x0 + w - M.b_time * w / 100;                       /* TIME: right = now, left = the past */
    int half = 2 + M.b_size * 30 / 100;
    gfx_line(cx, top, cx, bot, T->line, true);
    if (M.b_freeze) gfx_dither(x0, top, w, bot - top, T->line);
    int dens = M.b_dens > 50 ? M.b_dens - 50 : 50 - M.b_dens;  /* noon: none; both sides: more grains */
    int count = dens * 50 / 50;
    uint32_t h = (now / 60) * 2654435761u;
    int py = bot - (bot - top) * M.b_pitch / 100;
    for (int i = 0; i < count && M.playing; i++) {
        h ^= h >> 13; h *= 0x5bd1e995u; h ^= h >> 15;
        int x = cx - half + (int)(h % (uint32_t)(2 * half + 1));
        int y = py - 7 + (int)((h >> 9) % 15);
        if (x < x0 || x > x0 + w || y < top || y > bot) continue;
        gfx_rect(x, y, 2, 2, (h >> 20) & 3 ? T->ink : T->accent);
    }
    /* levels: in (dim), out (accent) */
    int hi = (bot - top) * M.b_in / 100, ho = (bot - top) * M.b_out / 100;
    gfx_rect(W - 20, bot - hi, 5, hi + 1, T->dim);
    gfx_rect(W - 10, bot - ho, 5, ho + 1, T->accent);
    gfx_text("I", W - 19, top - 7, T->dim, 1); gfx_text("O", W - 9, top - 7, T->dim, 1);
    mod_foot();
}

/* MATRIX: the slots in use, one row each: SOURCE>DEST, then the amount as a bipolar LED bar from the
 * centre (round LEDs on the 4 px pitch, as on FILTER). The slot in focus is bright, the others step darker
 * the further they are from it. */
static void scr_matrix(void)
{
    mod_head();
    if (M.mx_n == 0) { gfx_text("NO SLOT IN USE", 4, 60, T->dim, 1); mod_foot(); return; }
    for (int r = 0; r < M.mx_n && r < 7; r++) {
        int y = GT + 3 + r * 8, d = r > M.mx_focus ? r - M.mx_focus : M.mx_focus - r;
        uint16_t col = d == 0 ? T->ink : d <= 2 ? T->dim : T->line;
        gfx_text(M.mx_txt[r], 4, y - 2, col, 1);
        /* 25 LEDs from x 60 to 156, centre at 108 */
        /* square-root scale so small amounts still light an LED or two */
        int mag = M.mx_amt[r] < 0 ? -M.mx_amt[r] : M.mx_amt[r];
        int lit = mag ? (int)(__builtin_sqrtf((float)mag / 1000.f) * 10.f + 0.5f) : 0;
        if (mag && lit < 1) lit = 1;
        if (M.mx_amt[r] < 0) lit = -lit;
        for (int i = -10; i <= 10; i++) {                      /* 21 LEDs, centre at x 104 */
            int x = 104 + i * 4;
            bool on = lit >= 0 ? (i >= 0 && i <= lit) : (i <= 0 && i >= lit);
            if (on) led_round(x, y, i == lit ? (d == 0 ? T->accent : col) : col);
            else gfx_px(x, y, i == 0 ? col : T->line);
        }
    }
    mod_foot();
}

/* ---------------------------------------------------------------- splash: a terminal boot log
 * Lines type on at the cursor (a blinking block in the accent), OK tags in the third colour, the prompt in
 * the accent; the log scrolls once it fills the screen, then the first page appears. */
enum { TERM_MS = 4110, TERM_CPS = 100 };     /* total time, characters typed per second */
static const char *const TERM[] = {
    "OPEN-MPK4 MONITOR 0.4 / FILTER LAB",
    "",
    "> MEM 4096K ............... OK",
    "> GFX 600X450 AMOLED X4 ... OK",
    "> AUDIO 44.1K / 32 ........ OK",
    "> SST-FILTERS  27 MODELS .. OK",
    "> OSC440  16 VOICES ....... OK",
    "> MARBLES  TIDES  BEADS ... OK",
    "> SP-303 VINYL MR2 ........ OK",
    "> EUCLID 5 + WAVE 1 ....... OK",
    "",
    "READY.",
    "> ",
};
enum { TERM_N = sizeof(TERM) / sizeof(TERM[0]) };
static void scr_term(uint32_t bt)
{
    gfx_rect(0, 0, W, H, T->bg);
    /* how many characters have been typed, line by line (blank lines take a beat) */
    int budget = (int)(bt * TERM_CPS / 1000), line = 0, col = 0;
    while (line < TERM_N) {
        int len = (int)strlen(TERM[line]) + 3;           /* +3: the pause at the end of a line */
        if (budget < len) { col = budget; break; }
        budget -= len; line++;
    }
    int last = line < TERM_N ? line : TERM_N - 1;
    const int lh = 7, rows = 15;
    int first = last - rows + 1 < 0 ? 0 : last - rows + 1;
    int cx = 2, cy = 2;
    for (int l = first; l <= last; l++) {
        const char *s = TERM[l];
        int n = l < line ? (int)strlen(s) : col;
        if (n > (int)strlen(s)) n = (int)strlen(s);
        int y = 3 + (l - first) * lh, x = 3;
        for (int i = 0; i < n; i++) {
            char ch[2] = { s[i], 0 };
            uint16_t c = T->ink;
            if (l == 0) c = T->ink;
            else if (s[0] == '>' && i == 0) c = T->accent;
            else if (i >= 2 && s[i - 1] == ' ' && s[i] == 'O' && s[i + 1] == 'K') c = tk(2);
            else if (i >= 1 && s[i - 1] == 'O' && s[i] == 'K') c = tk(2);
            else if (s[i] == '.') c = T->line;
            else if (s[0] == '>') c = T->dim;
            x = gfx_text(ch, x, y, c, 1);
        }
        cx = x; cy = y;
    }
    /* the cursor: a 4x5 block, blinking at 3 Hz (solid while typing) */
    bool typing = line < TERM_N && col > 0;
    if (typing || (bt / 160) % 2 == 0) gfx_rect(cx + 1, cy, 3, 5, T->accent);
}

/* ---------------------------------------------------------------- popups */
static int32_t pop_progress(uint32_t now)
{
    int32_t since = (int32_t)(now - (uint32_t)M.pop_last);
    if (since < HOLD_MS) {
        int32_t d = (int32_t)(now - (uint32_t)M.pop_t0);
        d = d < 0 ? 0 : d > ENTER_MS ? ENTER_MS : d;
        return ui_ease_out(d * 32768 / ENTER_MS);
    }
    int32_t d = since - HOLD_MS;
    return d >= EXIT_MS ? -1 : 32768 - ui_ease_in(d * 32768 / EXIT_MS);
}

static int32_t disp = -1;
static void overlay(uint32_t now, int32_t dt)
{
    if (M.pop == POP_OFF) return;
    int32_t p = pop_progress(now);
    if (p < 0) { M.pop = POP_OFF; return; }
    if (p == 0) return;
    gfx_clip(0, 8, (W * p + 16384) >> 15, FOOT_Y);
    int ox = (6 * (32768 - p) + 16384) >> 15;
    char b[32];
    gfx_rect(0, 8, W, FOOT_Y - 8, T->bg);
    if (M.pop == POP_FL_MODEL) {                       /* like the effect popup: number, name, "MODEL n OF m", dots */
        snprintf(b, sizeof b, "%d", M.filter + 1);
        int nx = 6 + gfx_text_w(b, 9) + 6;
        gfx_text(b, 6 + ox, 16, tk(1), 9);
        int sc = 3; while (sc > 1 && nx + gfx_text_w(M.fam, sc) > W - 4) sc--;
        gfx_text(M.fam, nx + ox, 17, T->ink, sc);
        gfx_text(M.var, nx + ox, 17 + 5 * sc + 4, T->dim, 1);
        snprintf(b, sizeof b, "MODEL %d OF %d", M.filter + 1, M.nfilt); gfx_text(b, nx + ox, 50, T->dim, 1);
        /* all the models as cells, 9 per row, under the name */
        for (int i = 0; i < M.nfilt; i++)
            gfx_rect(6 + ox + (i % 27) * 5, 76 + (i / 27) * 6, 4, 4, i == M.filter ? T->accent : i < M.filter ? T->dim : T->line);
    } else {                                        /* knob: ring of 24 dots, name, big value with unit */
        int v = M.kpos;
        if (disp < 0) disp = v;
        else { int32_t s = (v - disp) * (dt > 50 ? 50 : dt) / 140; disp = s ? disp + s : v; }
        for (int i = 0; i < 24; i++) {
            int x, y; gfx_ring_pt(30 + ox, 40, 19, 24, 24576, 49152, i, &x, &y);
            gfx_rect(x - 1, y - 1, 2, 2, i * 4096 <= disp * 23 ? T->accent : T->line);
        }
        gfx_text(M.kname, 62 + ox, 13, T->dim, 1);
        int sc = 6, len = (int)strlen(M.kval);
        while (sc > 1 && 62 + len * 4 * sc + 1 + gfx_text_w(M.kunit, 1) > W - 2) sc--;
        int e = gfx_text(M.kval, 62 + ox, 22, T->ink, sc);
        gfx_text(M.kunit, e + 1, 22 + 5 * sc - 6, T->dim, 1);
    }
    gfx_clip(0, 0, W, H);
}

/* phosphor afterglow, as ui.c */
static void afterglow(void)
{
#define BE(c) ((uint16_t)(((c) >> 8) | ((c) << 8)))
    const uint16_t ink = BE(T->ink), acc = BE(T->accent), bg = BE(T->bg), dim = BE(T->dim), line = BE(T->line);
    for (int q = 0; q < W * H / 4; q++) {
        uint16_t *p = fb + q * 4;
        unsigned g = glow[q], ng = 0;
        if (!g) {
            for (int k = 0; k < 4; k++) if (p[k] == ink || p[k] == acc) ng |= 3u << (k * 2);
        } else for (int k = 0; k < 4; k++) {
            unsigned c = p[k], v = g >> (k * 2) & 3;
            if (c == ink || c == acc) v = 3;
            else if (v && c == bg) { p[k] = v > 1 ? dim : line; v--; }
            else v = 0;
            ng |= v << (k * 2);
        }
        glow[q] = (uint8_t)ng;
    }
#undef BE
}

/* ---------------------------------------------------------------- API */
API void scr_init(void)
{
    memset(&M, 0, sizeof M);
    M.theme_drawn = -1; M.bpm = 120; M.nfilt = 1; M.bars_per = 4; M.sweep = 1; M.res = 65; M.env = 100;
    M.fc = 440; M.boot_on = 1; M.sel = 1; M.v_on = 1; M.v_comp = 64; M.p_on = 1; M.p_steps = 16; M.e_focus = 1;
    strcpy(M.fam, "FILTER LAB");
    for (int i = 0; i < LANES; i++) { M.steps[i] = 16; M.hits[i] = 4; }
}
API uint16_t *scr_fb(void) { return fb; }
API int scr_w(void) { return W; }
API int scr_ncol(void) { return NCOL; }
API int scr_gh(void) { return GH; }
API int scr_h(void) { return H; }
API char *scr_buf(void) { return sbuf; }
API uint8_t *scr_spec(void) { return M.spec; }
API int8_t *scr_wpts(void) { return M.w_pts; }
API int8_t *scr_lpts(void) { return M.l_pts; }
API uint8_t *scr_wspec(void) { return M.w_spec; }
API uint8_t *scr_resp(int i) { return M.resp[i & 1]; }
API void scr_wlane(int n, int k, int r) { M.p_steps = n; M.p_hits = k; M.p_rot = r; }
/* module screens: text slots 0 title, 1 subtitle, 2..5 footer labels, 6..9 footer values (from sbuf) */
API void scr_mtext(int slot)
{
    char *d = slot == 0 ? M.m_title : slot == 1 ? M.m_sub : slot < 6 ? M.m_fl[slot - 2] : M.m_fv[(slot - 6) & 3];
    size_t n = slot == 0 ? sizeof M.m_title : slot == 1 ? sizeof M.m_sub : slot < 6 ? sizeof M.m_fl[0] : sizeof M.m_fv[0];
    strncpy(d, sbuf, n - 1); d[n - 1] = 0;
}
API void scr_mx_row(int r, int amt) { if (r >= 0 && r < 7) { M.mx_amt[r] = amt; strncpy(M.mx_txt[r], sbuf, 15); M.mx_txt[r][15] = 0; } }
API void scr_mx_count(int n, int focus) { M.mx_n = n; M.mx_focus = focus; }
API void scr_shift_state(int bits, int len) { M.sh_bits = (uint16_t)bits; M.sh_len = len; }
API int8_t *scr_shift_hist(void) { return M.sh_hist; }
API void scr_marbles_push(int t, int x1, int x3) { int i = M.mb_n & 31; M.mb_t[i] = (uint8_t)t; M.mb_x1[i] = (uint8_t)x1; M.mb_x3[i] = (uint8_t)x3; M.mb_n++; }
API void scr_tides_push(int a, int b, int c, int d) { int i = M.td_n & 127; M.td[0][i] = (int8_t)a; M.td[1][i] = (int8_t)b; M.td[2][i] = (int8_t)c; M.td[3][i] = (int8_t)d; M.td_n++; }
API void scr_beads_state(int time, int size, int dens, int pitch, int freeze, int in, int out)
{ M.b_time = time; M.b_size = size; M.b_dens = dens; M.b_pitch = pitch; M.b_freeze = freeze; M.b_in = in; M.b_out = out; }
/* envelope i: attack, decay, release in ms, sustain in % */
API void scr_envs(int i, int a, int d, int s, int r) { if (i < 0 || i > 2) return; M.e_a[i] = a; M.e_d[i] = d; M.e_s[i] = s; M.e_r[i] = r; }
API void scr_env_mode(int i, int ppg) { if (i >= 0 && i < 3) M.e_ppg[i] = ppg; }
API void scr_env_live(int i, int lv) { if (i >= 0 && i < 3) M.e_lv[i] = lv; }
API void scr_env_focus(int i) { M.e_focus = i < 0 ? 0 : i % 3; }
API int scr_n_themes(void) { return N_THEMES; }
API const char *scr_theme_name(int i) { return THEMES[(unsigned)i % N_THEMES]->name; }

/* string from sbuf: 0 family, 1 variant, 2 knob name, 3 knob value, 4 knob unit, 5 wave preset, 6 wave osc mode, 7 wave info */
API void scr_str(int which)
{
    char *d = which == 0 ? M.fam : which == 1 ? M.var : which == 2 ? M.kname : which == 3 ? M.kval : which == 4 ? M.kunit
            : which == 5 ? M.w_name : which == 6 ? M.w_mode : which == 8 ? M.l_shape : which == 9 ? M.p_group : which == 10 ? M.e_sub : M.w_info;
    size_t n = which <= 1 ? sizeof M.fam : which == 4 ? sizeof M.kunit : which == 8 ? sizeof M.l_shape : which == 9 ? sizeof M.p_group : which == 10 ? sizeof M.e_sub : which >= 5 ? sizeof M.w_name : sizeof M.kname;
    strncpy(d, sbuf, n - 1); d[n - 1] = 0;
}

enum { S_SCREEN, S_THEME, S_PLAYING, S_BPM, S_FILTER, S_NFILT, S_STEP, S_BAR, S_BARSPER, S_SWEEP,
       S_RES, S_CUT, S_ENV, S_DRV, S_FC, S_SEL, S_POP_KNOB, S_POP_MODEL, S_BOOT,
       S_V_ON, S_V_MODE, S_V_COMP, S_V_NOISE, S_V_WOW, S_V_IN, S_V_GR,
       S_W_OSC, S_W_POS, S_W_LVL, S_W_CUT, S_W_RES, S_W_HELD, S_W_WT,
       S_E_AMT_LAD, S_E_AMT_SVF, S_E_GATE, S_L_PH, S_L_RATE, S_L_DLY, S_F_SVF, S_F_LAD, S_F_RES, S_F_DRV, S_F_PAR,
       S_F_SVFN, S_F_LADN, S_E_LV0, S_E_LV1, S_P_MODE, S_P_OCT, S_P_VOL, S_P_ON };

API void scr_set(int k, int v, uint32_t now)
{
    switch (k) {
    case S_SCREEN: M.screen = (v % SCR_FL_N + SCR_FL_N) % SCR_FL_N; break;
    case S_THEME: M.theme = (v % N_THEMES + N_THEMES) % N_THEMES; break;
    case S_PLAYING: M.playing = v; break;
    case S_BPM: M.bpm = v; break;
    case S_FILTER: M.filter = v; break;
    case S_NFILT: M.nfilt = v; break;
    case S_STEP: M.step = v & 15; break;
    case S_BAR: M.bar = v; break;
    case S_BARSPER: M.bars_per = v; break;
    case S_SWEEP: M.sweep = v; break;
    case S_RES: M.res = v; break;
    case S_CUT: M.cut = v; break;
    case S_ENV: M.env = v; break;
    case S_DRV: M.drv = v; break;
    case S_FC: M.fc = v; break;
    case S_SEL: M.sel = (v % LANES + LANES) % LANES; break;
    case S_POP_KNOB:                                 /* v = position 0..4096; kname/kval/kunit set before */
    case S_POP_MODEL: {
        int kind = k == S_POP_KNOB ? POP_FL_KNOB : POP_FL_MODEL;
        if (kind == POP_FL_KNOB) M.kpos = v;
        if (M.pop == kind) M.pop_last = (int)now;
        else { if (kind == POP_FL_KNOB) disp = -1; M.pop = kind; M.pop_t0 = M.pop_last = (int)now; }
        break;
    }
    case S_BOOT: M.boot_on = 1; M.boot_t0 = now; break;
    case S_V_ON: M.v_on = v; break;
    case S_V_MODE: M.v_mode = v; break;
    case S_V_COMP: M.v_comp = v; break;
    case S_V_NOISE: M.v_noise = v; break;
    case S_V_WOW: M.v_wow = v; break;
    case S_V_IN: M.v_in = v; break;
    case S_V_GR: M.v_gr = v; break;
    case S_W_OSC: M.w_osc = v; break;
    case S_W_POS: M.w_pos = v; break;
    case S_W_LVL: M.w_lvl = v; break;
    case S_W_CUT: M.w_cut = v; break;
    case S_W_RES: M.w_res = v; break;
    case S_W_HELD: M.w_held = v; break;
    case S_W_WT: M.w_wt = v; break;
    case S_E_AMT_LAD: M.e_amt_lad = v; break;
    case S_E_AMT_SVF: M.e_amt_svf = v; break;
    case S_E_GATE: M.e_gate10 = v; break;
    case S_L_PH: M.l_ph = v; break;
    case S_L_RATE: M.l_rate = v; break;
    case S_L_DLY: M.l_dly = v; break;
    case S_F_SVF: M.f_svf_hz = v; break;
    case S_F_LAD: M.f_lad_hz = v; break;
    case S_F_RES: M.f_res = v; break;
    case S_F_DRV: M.f_drv10 = v; break;
    case S_F_PAR: M.f_par = v; break;
    case S_F_SVFN: M.f_svf_note = v; break;
    case S_F_LADN: M.f_lad_note = v; break;
    case S_E_LV0: M.e_lv[0] = v; break;
    case S_E_LV1: M.e_lv[1] = v; break;
    case S_P_MODE: M.p_mode = v; break;
    case S_P_OCT: M.p_oct = v; break;
    case S_P_VOL: M.p_vol = v; break;
    case S_P_ON: M.p_on = v; break;
    }
}

/* lane l: steps, hits, rotate, mute */
API void scr_lane(int l, int n, int k, int r, int mute)
{
    if (l < 0 || l >= LANES) return;
    M.steps[l] = n; M.hits[l] = k; M.rot[l] = r; M.mute[l] = mute;
}
API void scr_trig(int l, uint32_t now) { if (l >= 0 && l < LANES) M.trig[l] = now; }

API int scr_frame(uint32_t now)
{
    static uint32_t last; static bool started;
    int32_t dt = started ? (int32_t)(now - last) : 0; started = true; last = now;
    if (M.theme != M.theme_drawn) {
        M.theme_drawn = M.theme; T = THEMES[M.theme];
        memset(glow, 0, sizeof glow);
    }
    if (T->fps && M.drawn_any && (now - M.drawn) * T->fps < 1000) return 0;
    M.drawn_any = 1; M.drawn = now;

    gfx_begin(fb);
    gfx_ghost(T->ghost, T->line);
    uint32_t bt = now - M.boot_t0;
    bool boot = M.boot_on && bt < TERM_MS;
    if (M.boot_on && !boot) M.boot_on = 0;
    if (boot) scr_term(bt);
    else {
        gfx_rect(0, 0, W, H, T->bg);
        switch (M.screen) {
        case SCR_FL_HOME: scr_home(now); break;
        case SCR_FL_FILTER: scr_filter(); break;
        case SCR_FL_EUCLID: scr_euclid(); break;
        case SCR_FL_VINYL: scr_vinyl(now); break;
        case SCR_FL_ENV: scr_env(); break;
        case SCR_FL_LFO: scr_tides(); break;
        case SCR_FL_WFLT: scr_wflt(); break;
        case SCR_FL_PLAY: scr_play(now); break;
        case SCR_FL_SHIFT: scr_shift(); break;
        case SCR_FL_MARBLES: scr_marbles(); break;
        case SCR_FL_BEADS: scr_beads(now); break;
        case SCR_FL_MATRIX: scr_matrix(); break;
        default: scr_wave(now); break;
        }
        status(now);
        overlay(now, dt);
    }
    if (T->glow) afterglow();
    return 1;
}
