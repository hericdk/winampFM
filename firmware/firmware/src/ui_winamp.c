/* SPDX-License-Identifier: GPL-3.0-only */
/* winampFM: the main screen, drawn like Winamp 2.x (the classic skin) on the FM-1's 240 x 240 LCD.
 *
 * Winamp's windows, enlarged so the small screen stays readable (text 5 x 7, time digits 11 x 22):
 *   PLAYER (top, always)  the time from PLAY, a real spectrum of the output, the selected track's sound in the
 *                         marquee, BPM and key, the loop position as the seek bar, and the four tracks as buttons
 *                         with an LED (green = a loop, red = recording, amber = muted) and the sound each one plays.
 *   one window below, as Winamp's EQ / PL buttons open them (HOME switches; the knob you touch opens its window):
 *     PLAYLIST    the sound bank (the drum kits on the drum track): PRESETS scrolls it and loads the sound.
 *     EFFECTS     the equalizer re-used for the effects: 12 sliders in 3 groups of 4 (SEND, SPACE, MASTER).
 *                 KNOB 1..4 move the group's four sliders (their colours under the sliders), SELECT picks the group.
 *     VISUALIZER  lines only, pixel art (half resolution, trails): SCOPE, BARS, MILK (old school: every scene in turn by itself, MilkDrop-like trails and colours), RAYS, TUNNEL; SELECT picks one.
 *
 * Painted in small windows of the screen (the cv_* canvas with absolute coordinates through cv_ox / cv_oy): a part is
 * repainted only when what it shows changed, or while it animates (spectrum, marquee, curve, visualizer). */
#include "wfm_font.h"

/* ---------------------------------------------------------------- colours (measured on the reference skin) */
#define WC_FR0 RGB(0x0c, 0x0c, 0x15)     /* outline */
#define WC_FR1 RGB(0x18, 0x18, 0x28)     /* frame, dark bevel */
#define WC_FR2 RGB(0x2a, 0x2c, 0x45)     /* body */
#define WC_FR3 RGB(0x34, 0x37, 0x56)     /* body hatch */
#define WC_FR4 RGB(0x58, 0x5c, 0x86)     /* bevel light */
#define WC_FR5 RGB(0x84, 0x88, 0xb4)     /* bevel highlight */
#define WC_TBAR RGB(0x22, 0x20, 0x3a)    /* title bar */
#define WC_GLD1 RGB(0xe8, 0xd8, 0x98)    /* title stripes */
#define WC_GLD2 RGB(0x8c, 0x7a, 0x48)
#define WC_TTXT RGB(0xee, 0xee, 0xfa)
#define WC_LCD RGB(0x00, 0x00, 0x00)
#define WC_GRN RGB(0x1c, 0xe6, 0x1c)     /* display text */
#define WC_GRN2 RGB(0x10, 0xd0, 0x10)    /* playlist text */
#define WC_GDIM RGB(0x0c, 0x3a, 0x0c)    /* an LED off */
#define WC_WHT RGB(0xff, 0xff, 0xff)
#define WC_SEL RGB(0x00, 0x00, 0xc6)     /* playlist: the selected row */
#define WC_LAB RGB(0xb4, 0xb6, 0xca)     /* labels on the body */
#define WC_LOFF RGB(0x5c, 0x5e, 0x74)    /* a lamp off, the dashes */
#define WC_RED RGB(0xff, 0x34, 0x22)
#define WC_RED2 RGB(0x60, 0x10, 0x10)
#define WC_AMB RGB(0xf0, 0xa8, 0x20)
#define WC_BTN RGB(0xc6, 0xd0, 0xdc)     /* buttons and slider knobs */
#define WC_BHI RGB(0xf4, 0xfa, 0xff)
#define WC_BSH RGB(0x6c, 0x78, 0x88)
#define WC_BDK RGB(0x28, 0x2e, 0x38)
#define WC_GLY RGB(0x34, 0x3a, 0x48)     /* text on buttons */
#define WC_KG1 RGB(0xd8, 0xb8, 0x60)     /* the gold knob (seek bar, scrollbar) */
#define WC_KG2 RGB(0xf6, 0xe6, 0xa6)
#define WC_KG3 RGB(0x6a, 0x52, 0x14)
#define WC_PEAK RGB(0xa8, 0xa8, 0xb0)
#define WC_DB RGB(0xc8, 0xa0, 0x40)      /* the EQ's gold labels */
static const uint16_t WF_KNOB[4] = {RGB(0x5a, 0x8a, 0xd0), RGB(0x7a, 0xb0, 0x4e), RGB(0xf0, 0xc8, 0x60), RGB(0xe0, 0x8a, 0x3a)};   /* KNOB 1..4 */
static const uint16_t WF_VISC[12] = {     /* the spectrum, bottom to top: green, yellow, orange, red */
    RGB(0x18, 0xa0, 0x18), RGB(0x28, 0xb8, 0x18), RGB(0x40, 0xcc, 0x1c), RGB(0x68, 0xd8, 0x20), RGB(0x98, 0xe0, 0x20), RGB(0xc4, 0xe0, 0x24),
    RGB(0xe0, 0xd4, 0x28), RGB(0xec, 0xb8, 0x28), RGB(0xec, 0x98, 0x24), RGB(0xe8, 0x74, 0x20), RGB(0xe0, 0x50, 0x1c), RGB(0xd4, 0x28, 0x14)};

/* ---------------------------------------------------------------- layout (screen pixels) */
#define WP_H 110                 /* PLAYER window: rows 0 .. 109 */
#define WL_Y 110                 /* the lower window: rows 110 .. 239 */
#define WL_H 130
#define WT_H 11                  /* title bars */
#define LCD_X 5                  /* the time + spectrum display */
#define LCD_Y 15
#define LCD_W 104
#define LCD_H 54
#define MQ_X 114                 /* the marquee */
#define MQ_Y 15
#define MQ_W 121
#define MQ_H 13
#define INF_Y 32                 /* BPM, key */
#define VB_Y 49                  /* the view buttons PL / FX / VIS, REC lamp */
#define POS_Y 72                 /* the seek bar: the loop of the selected track, its notes drawn in it */
#define POS_H 12
#define TK_Y 86                  /* the four tracks */
#define TK_H 20
#define LO_Y (WL_Y + WT_H + 3)   /* the lower window's content starts */
#define PL_X 5                   /* the list */
#define PL_W 221
#define PL_H 90
#define PL_ROWS 8
#define PL_RH 11
#define FT_Y 221                 /* the foot of the lower window */
#define EQ_TOP 169               /* the sliders' rails */
#define EQ_RAIL 36
#define CRV_Y 140                /* the line above the sliders (Winamp's EQ graph, here also a small visualizer) */
#define CRV_H 25
#define VZ_X 5                   /* the visualizer: 115 x 52 cells of 2 x 2 */
#define VZ_Y LO_Y
#define VZ_CW 115
#define VZ_CH 52

/* ---------------------------------------------------------------- the 12 effect sliders */
static const struct {
    uint8_t glob, id;
    char lab[4], shrt[6];
    const char *name;
} WF_SL[12] = {
    {0, P_DIST, "DST", "DIST", "DIST SEND"}, {0, P_CHOR, "CHO", "CHORS", "CHORUS SEND"}, {0, P_DLY, "DLY", "DELAY", "DELAY SEND"}, {0, P_REV, "REV", "VERB", "REVERB SEND"},
    {1, G_DTIME, "TIM", "TIME", "DELAY TIME"}, {1, G_DFDBK, "FBK", "FDBK", "FEEDBACK"}, {1, G_RSIZE, "SIZ", "SIZE", "REVERB SIZE"}, {1, G_RDAMP, "DMP", "DAMP", "REVERB DAMP"},
    {1, G_FILT, "FLT", "FILT", "DJ FILTER"}, {1, G_DUST, "LOF", "LOFI", "LO-FI DUST"}, {1, G_DUCK, "DCK", "DUCK", "KICK DUCK"}, {1, G_BPM, "BPM", "BPM", "TEMPO"},
};
static const char *const WF_GRP[5] = {"SEND", "SPACE", "MASTER", "PARTY", "TRACK"};
static const char *const WF_VIEW[4] = {"PLAYLIST", "EFFECTS", "VISUALIZER", "TAPES"};
#define WV_N 5
static const char *const WF_VMODE[WV_N] = {"SCOPE", "BARS", "MILK", "RAYS", "TUNNEL"};
static uint8_t wf_grp;                       /* the group KNOB 1..4 move */
static uint8_t wf_view = 1;                  /* the lower window: 0 playlist, 1 effects (the first, as Winamp's EQ), 2 visualizer */
static uint8_t wf_pg;                        /* the lower window: 0 the player's own, 1 a page, 2 a layer, 3 SONG, 4 STEP, 5 a hold, 6 PRESETS, 7 drums, 8 USER */
static struct {                              /* a layer's keys and knobs (ui_layers.c), or the STEP page's */
    uint32_t layer;
    char sub[24], lab[4][8], val[4][10];
    tile_t tl[16];
} wl;
/* TAPES (the 4th home, the Quest's sound select redrawn): the copy animation */
static uint32_t wft_t0;                      /* fm1_ms when SEL copied, 0 = none */
static char wft_old[16], wft_new[16];        /* the track's sound before and after (the elevator) */
static uint8_t wft_spec0[19];                /* the spectrum when it was copied (the bars that explode) */
#define WFT_MS 2000u
static UI_NOINLINE uint32_t wft_age(void) { return wft_t0 ? fm1_ms - wft_t0 : 0xFFFFFFFFu; }
static UI_NOINLINE int wfm_tape_busy(void) { return wft_t0 && fm1_ms - wft_t0 < WFT_MS; }
static UI_NOINLINE int32_t wf_back(int32_t k)                         /* easeOutBack (the swing), k and the result 0..1024 (it goes past 1024) */
{
    int32_t d = clamp(k, 0, 1024) - 1024, d2 = d * d / 1024, d3 = d2 * d / 1024;
    return 1024 + d3 * 2766 / 1024 + d2 * 1742 / 1024;
}
static uint8_t wv_mode;                      /* the visualizer's picture */
static int8_t wf_hot = -1;                   /* what the marquee says for a moment: 0..11 a slider, 12 the group, 13 the view, 14 the picture */
static uint32_t wf_hot_t;

static UI_NOINLINE const param_desc_t *wf_sl(uint32_t i, int16_t **vp)
{
    if (WF_SL[i].glob) {
        *vp = &song.g[WF_SL[i].id];
        return &GP[WF_SL[i].id];
    }
    *vp = &TSEL->p[WF_SL[i].id];
    return &TP[WF_SL[i].id];
}
static UI_NOINLINE int32_t wf_frac(const param_desc_t *d, int32_t v)   /* 0..1024 */
{
    int32_t r = d->max - d->min;
    return r > 0 ? clamp((v - d->min) * 1024 / r, 0, 1024) : 0;
}
/* PARTY (the 4th group, the RPG's word): who is on stage. KNOB 1 FORM: SOLO one track, DUEL two, TRIAD three, PARTY all; KNOB 2 LEAD: the first
 * of them; KNOB 3 TURN: every 1 / 2 / 4 / 8 bars the stage turns (OFF: it stays); KNOB 4 STYLE: ROLL (the next tracks step in, 12 -> 23 -> 3D ..)
 * or TRADE (the chosen ones take turns, one at a time: a duel). It works through the solo mask (the others fade out). */
static uint8_t wp_form = 3, wp_lead, wp_turn, wp_style;          /* (AUTOTURN starts at OFF) */
static uint32_t wp_hint_t;                    /* KNOB 3 / 4 last turned on PARTY (the arrow), 0 = not */
#define WP_HINT_MS 1500u
static uint32_t wp_p0, wp_el;                 /* the turns count from PLAY (a new LEAD steps into the running turns, in time); wp_el: how far into this turn, /1024 (the NEXT bar) */
static uint8_t wp_mask = 0xFF;               /* the stage last set (0xFF: not ours) */
static const char *const WP_FORM[4] = {"SOLO", "DUEL", "TRIAD", "PARTY"};
static const char *const WP_TURN[5] = {"OFF", "1 BAR", "2 BARS", "4 BARS", "8 BARS"};
static const char *const WP_STYLE[2] = {"KEEP", "YOYO"};
static const char *const WP_KN[4] = {"FORM", "LEAD", "AUTOTURN", "STYLE"};
static const char *const WP_TRK[4] = {"1", "2", "3", "D"};
static void tracks_edit(uint32_t slot, int32_t steps);
static const char *const WT_KN[4] = {"LEVEL", "STEPS", "PAN", "SWING"};
static const uint8_t WT_SLOT[4] = {1, 2, 3, 0};                       /* (tracks_edit's slots: 0 SWING 1 LEVEL 2 LEN 3 PAN) */
static UI_NOINLINE const param_desc_t *wt_par(uint32_t k, int16_t **vp)
{
    if (k == 0u) { if (is_drum(TSEL)) { *vp = &song.g[G_DRLVL]; return &GP[G_DRLVL]; } *vp = &TSEL->p[P_LEVEL]; return &TP[P_LEVEL]; }
    if (k == 1u) { *vp = &TSEL->p[P_SLEN]; return &TP[P_SLEN]; }
    if (k == 2u) { *vp = &TSEL->p[P_PAN]; return &TP[P_PAN]; }
    *vp = &song.g[G_SWING]; return &GP[G_SWING];
}
static UI_NOINLINE uint32_t wp_bar(uint32_t *frac)                   /* bars since PLAY, a little early (a note on the beat finds its track open); frac: into it, /1024 */
{
    uint32_t la = 2205u * (uint32_t)song.g[G_BPM], pos = clk_pos + la, beat = clk_beat + pos / BEAT_U;
    pos %= BEAT_U;
    if (frac) *frac = ((beat & 3u) * 256u) + pos / (BEAT_U / 256u);
    return beat / 4u;
}
static UI_NOINLINE uint32_t wp_stage(uint32_t *left)                 /* the tracks on stage now (a mask of 4); left: bars to the next turn */
{   /* KEEP: the LEAD stays on stage, the other places turn through the rest. YOYO: the stage walks on and comes back (0 1 2 3 2 1 ..) */
    static const uint8_t BARS[5] = {0, 1, 2, 4, 8}, YO[6] = {0, 1, 2, 3, 2, 1};
    uint32_t n = wp_form + 1u, b = BARS[wp_turn % 5u], st = 0, j, m = 0, fr = 0, p = song.playing ? wp_bar(&fr) * 1024u + fr : 0u, el = p > wp_p0 ? p - wp_p0 : 0u;
    if (b) st = el / (b * 1024u);
    if (left) *left = b ? b - (el / 1024u) % b : 0u;
    wp_el = b ? el % (b * 1024u) * 1024u / (b * 1024u) : 0u;
    if (wp_style) {                                                    /* YOYO */
        uint32_t o = b ? YO[st % 6u] : 0u;
        for (j = 0; j < n; j++) m |= 1u << ((wp_lead + o + j) & 3u);
        return m;
    }
    m = 1u << (wp_lead & 3u);                                          /* KEEP: the lead, and n - 1 of the other three, turning */
    for (j = 0; j + 1u < n; j++) m |= 1u << ((wp_lead + 1u + (st + j) % 3u) & 3u);
    return m;
}
static uint32_t wfm_view_key(void) { return wf_view; }
static UI_NOINLINE void wfm_party_tick(void)                          /* ui_draw, every frame: the stage turns on the bar */
{
    uint32_t m;
    if (!song.playing) wp_p0 = 0;                                      /* (a new PLAY counts from its first bar) */
    if (ui.menu == 9) return;                                          /* (TAPES previews through the solo: leave it) */
    m = wp_stage(0);
    if (m == 15u) m = 0u;                                              /* everyone: no solo */
    if (m == wp_mask) return;
    if (wp_mask == 0xFFu && !m) return;                                /* PARTY and nothing of ours to undo */
    song.solo = (uint8_t)m;
    wp_mask = m ? (uint8_t)m : 0xFFu;
}
static UI_NOINLINE void wf_say(int32_t what) { wf_hot = (int8_t)what, wf_hot_t = fm1_ms; }
static UI_NOINLINE int wf_saying(void) { return wf_hot >= 0 && fm1_ms - wf_hot_t < 1600u; }

/* ---------------------------------------------------------------- the region being painted */
static int32_t wf_rx, wf_ry, wf_rw, wf_rh;    /* screen rectangle of the canvas */
static int32_t wf_clx0 = 0, wf_clx1 = 240;    /* text clip (the marquee, the list) */
static int32_t wf_cly0 = -1000, wf_cly1 = 1000; /* (and vertically: the elevator of a track's name) */
static UI_NOINLINE int wf_vis(int32_t x, int32_t y, int32_t w, int32_t h)
{
    return x < wf_rx + wf_rw && x + w > wf_rx && y < wf_ry + wf_rh && y + h > wf_ry;
}
#define WFR(x, y, w, h, c) cv_rect((x), (y), (w), (h), (c))
static UI_NOINLINE void wf_bevel(int32_t x, int32_t y, int32_t w, int32_t h, uint16_t tl, uint16_t br)
{
    WFR(x, y, w, 1, tl);
    WFR(x, y, 1, h, tl);
    WFR(x, y + h - 1, w, 1, br);
    WFR(x + w - 1, y, 1, h, br);
}
static UI_NOINLINE void wf_inset(int32_t x, int32_t y, int32_t w, int32_t h, uint16_t bg)   /* a display sunk into the body */
{
    if (!wf_vis(x - 1, y - 1, w + 2, h + 2)) return;
    wf_bevel(x - 1, y - 1, w + 2, h + 2, WC_FR1, WC_FR5);
    WFR(x, y, w, h, bg);
}
static UI_NOINLINE void wf_body(int32_t x, int32_t y, int32_t w, int32_t h)   /* the metal, with its fine diagonal hatch */
{
    int32_t y0 = y > wf_ry ? y : wf_ry, y1 = y + h < wf_ry + wf_rh ? y + h : wf_ry + wf_rh;
    int32_t x0 = x > wf_rx ? x : wf_rx, x1 = x + w < wf_rx + wf_rw ? x + w : wf_rx + wf_rw, i, j;
    for (j = y0; j < y1; j++)
        for (i = x0; i < x1; i++)
            cv_pset(i, j, ((i + j) & 3) ? WC_FR2 : WC_FR3);
}

/* ---------------------------------------------------------------- text: big = the 5 x 7 face, else the 5 px one */
static UI_NOINLINE int32_t wf_tw(int big, const char *s)
{
    int32_t w = 0;
    for (; *s; s++) {
        uint32_t c = (uint8_t)*s;
        if (c >= 'a' && c <= 'z') c -= 32;
        if (c < 32 || c > 95) c = '?';
        w += (big ? WFM_W[c - 32] : WFS_W[c - 32]) + 1;
    }
    return w ? w - 1 : 0;
}
static UI_NOINLINE int32_t wf_text(int32_t x, int32_t y, int big, const char *s, uint16_t col)
{
    const int32_t h = big ? WFM_H : WFS_H;
    if (y >= wf_ry + wf_rh || y + h <= wf_ry) return x + wf_tw(big, s) + 1;
    for (; *s; s++) {
        uint32_t c = (uint8_t)*s, w, r, b;
        const uint8_t *rows;
        if (c >= 'a' && c <= 'z') c -= 32;
        if (c < 32 || c > 95) c = '?';
        w = big ? WFM_W[c - 32] : WFS_W[c - 32];
        rows = big ? WFM_ROWS[c - 32] : WFS_ROWS[c - 32];
        if (x + (int32_t)w > wf_clx0 && x < wf_clx1)
            for (r = 0; r < (uint32_t)h; r++)
                for (b = 0; b < w; b++)
                    if ((rows[r] >> (7 - b)) & 1u && x + (int32_t)b >= wf_clx0 && x + (int32_t)b < wf_clx1 && y + (int32_t)r >= wf_cly0 && y + (int32_t)r < wf_cly1)
                        cv_pset(x + (int32_t)b, y + (int32_t)r, col);
        x += (int32_t)w + 1;
    }
    return x;
}
static UI_NOINLINE void wf_textc(int32_t cx, int32_t y, int big, const char *s, uint16_t col) { wf_text(cx - wf_tw(big, s) / 2, y, big, s, col); }
static UI_NOINLINE void wf_textr(int32_t rx, int32_t y, int big, const char *s, uint16_t col) { wf_text(rx - wf_tw(big, s), y, big, s, col); }

/* the time digits: a thin LCD seven-segment, 11 x 22 */
static UI_NOINLINE void wf_digit(int32_t x, int32_t y, uint32_t d, uint16_t c)
{
    static const uint8_t SEG[10] = {0x3F, 0x06, 0x5B, 0x4F, 0x66, 0x6D, 0x7D, 0x07, 0x7F, 0x6F};   /* gfedcba */
    uint32_t m = SEG[d % 10u];
    if (m & 0x01u) WFR(x + 2, y, 7, 2, c);
    if (m & 0x02u) WFR(x + 9, y + 2, 2, 8, c);
    if (m & 0x04u) WFR(x + 9, y + 12, 2, 8, c);
    if (m & 0x08u) WFR(x + 2, y + 20, 7, 2, c);
    if (m & 0x10u) WFR(x, y + 12, 2, 8, c);
    if (m & 0x20u) WFR(x, y + 2, 2, 8, c);
    if (m & 0x40u) WFR(x + 2, y + 10, 7, 2, c);
}

/* ---------------------------------------------------------------- pieces of skin */
static UI_NOINLINE void wf_titlebar(int32_t y, const char *label)
{
    int32_t tw, tx, x;
    if (!wf_vis(0, y, 240, WT_H)) return;
    WFR(0, y, 240, WT_H, WC_TBAR);
    WFR(0, y, 240, 1, WC_FR4);
    tw = wf_tw(1, label);
    tx = 120 - tw / 2;
    for (x = 0; x < 2; x++) {                                      /* the two gold stripes left and right of the title */
        int32_t sy = y + 4 + x * 2;
        WFR(15, sy, tx - 20, 1, x ? WC_GLD2 : WC_GLD1);
        WFR(tx + tw + 5, sy, 240 - 30 - (tx + tw + 5), 1, x ? WC_GLD2 : WC_GLD1);
    }
    wf_text(tx, y + 2, 1, label, WC_TTXT);
    WFR(4, y + 3, 7, 5, WC_GLD2);                                   /* the menu stamp, left */
    WFR(5, y + 4, 5, 3, WC_GLD1);
    for (x = 0; x < 2; x++) {                                      /* shade / close, right */
        int32_t bx = 220 + x * 8;
        WFR(bx, y + 3, 6, 5, WC_FR4);
        WFR(bx + 1, y + 4, 4, 3, x ? WC_GLD2 : WC_FR2);
    }
}
static UI_NOINLINE void wf_window(int32_t y, int32_t h, const char *label)
{
    if (!wf_vis(0, y, 240, h)) return;
    wf_body(0, y, 240, h);
    wf_bevel(0, y, 240, h, WC_FR4, WC_FR0);
    wf_bevel(1, y + 1, 238, h - 2, WC_FR3, WC_FR1);
    wf_titlebar(y, label);
}
static UI_NOINLINE void wf_button(int32_t x, int32_t y, int32_t w, int32_t h, int down)
{
    if (!wf_vis(x, y, w, h)) return;
    WFR(x, y, w, h, down ? WC_BSH : WC_BTN);
    wf_bevel(x, y, w, h, down ? WC_BDK : WC_BHI, down ? WC_BHI : WC_BDK);
    if (!down) {
        WFR(x + 1, y + h - 2, w - 2, 1, WC_BSH);
        WFR(x + w - 2, y + 1, 1, h - 2, WC_BSH);
    }
}
static UI_NOINLINE void wf_led(int32_t x, int32_t y, uint16_t c)
{
    WFR(x, y, 4, 4, c);
    if (c != WC_GDIM && c != WC_RED2) cv_pset(x, y, WC_WHT);
}

/* ---------------------------------------------------------------- what the screen shows */
static uint32_t wf_t0;                       /* fm1_ms at PLAY */
static uint8_t wf_led_on;                    /* the bolt became the recording LED */
static const int8_t WV_SIN[64];
static uint16_t wf_dim(uint16_t c, uint32_t k);
static uint32_t wf_led_t;                    /* when it changed */
static uint8_t wf_was_play;
static uint8_t wf_spec[19], wf_peak[19], wf_pkt[19];
static int32_t wf_curve_j;                   /* the curve's tremble, from the output level */
static uint32_t wf_mq_off;                   /* the marquee's scroll, in pixels */
static char wf_mq[72];

static UI_NOINLINE uint32_t wf_isqrt(uint32_t v) { uint32_t r = 0, b = 1u << 30; while (b > v) b >>= 2; while (b) { if (v >= r + b) { v -= r + b; r = (r >> 1) + b; } else r >>= 1; b >>= 2; } return r; }
static UI_NOINLINE int wf_recording(void) { return ft_on || (song.rec && song.playing); }
static UI_NOINLINE uint32_t wf_secs(void) { return song.playing ? (fm1_ms - wf_t0) / 1000u : 0u; }

/* the spectrum: 19 Goertzel filters, 60 Hz .. 10 kHz (log), over the last 256 samples of the output (scope_buf, 22 kHz) */
static const int16_t WF_GC[19] = {32763, 32760, 32753, 32742, 32721, 32686, 32623, 32512, 32317, 31973, 31369, 30311,
                                  28473, 25314, 20003, 11424, -1415, -17786, -31380};
static UI_NOINLINE void wf_spectrum(void)
{
    uint32_t w = scope_w, b, n, lv = 0;
    for (b = 0; b < 19u; b++) {
        int32_t s1 = 0, s2 = 0, c = WF_GC[b], re, im, m, lg = 0, h;
        for (n = 0; n < 256u; n++) {
            int32_t x = scope_buf[(w - 256u + n) & (SCOPE_N - 1u)] >> 7, s0 = x + ((c * s1) >> 14) - s2;
            s2 = s1;
            s1 = s0;
        }
        re = s1 - ((c * s2) >> 15);                                   /* |s1 - s2 e^-jw|, sin from cos */
        im = (int32_t)wf_isqrt((uint32_t)(16384 * 16384 - (c / 2) * (c / 2))) * (s2 < 0 ? -s2 : s2) >> 14;
        re = re < 0 ? -re : re;
        m = re > im ? re + im / 2 : im + re / 2;
        while ((m >> lg) > 1) lg++;
        h = m < 2 ? 0 : clamp((lg * 8 + (((m << 3) >> lg) & 7) - 40) * 24 / 72, 0, 24);   /* 0..24 rows */
        wf_spec[b] = (uint8_t)(h > (int32_t)wf_spec[b] ? h : wf_spec[b] > 1u ? wf_spec[b] - 2u : 0u);   /* rises at once, falls 2 rows a frame */
        if (wf_spec[b] >= wf_peak[b]) wf_peak[b] = wf_spec[b], wf_pkt[b] = 10;
        else if (wf_pkt[b]) wf_pkt[b]--;
        else if (wf_peak[b]) wf_peak[b]--;
        lv += wf_spec[b];
    }
    wf_curve_j = (int32_t)(lv / 60u);
}

static UI_NOINLINE uint32_t wf_pos_of(const track_t *t)   /* where a part's sound sits in the bank list */
{
    uint32_t i, cur = 0;
    if (!bank_ready) bank_resolve();
    for (i = 0; i < NBANK; i++)
        if (BANK[i].e == t->eng_req && bank_pi[i] == t->preset) cur = i;
    if (user_of(t) < UP_SLOTS) cur = NBANK + up_rank(user_of(t));
    return cur;
}
static UI_NOINLINE uint32_t wf_list(uint32_t *total)    /* the list of the selected track and its current row */
{
    if (is_drum(TSEL)) {
        *total = DRUM_KITS;
        return (uint32_t)clamp(TDRUM->p[P_E0], 0, DRUM_KITS - 1);
    }
    return preset_pos(total);
}
static UI_NOINLINE void wf_item(uint32_t n, char *name, const char **kind)
{
    if (is_drum(TSEL)) {
        str_cpy(name, DRUM_KIT_NAMES[n % DRUM_KITS], 14);
        *kind = DRUM_KIT_STYLES[n % DRUM_KITS];
    } else if (n < NBANK) {
        str_cpy(name, BANK[n].name, 14);
        *kind = BANK_KIND[BANK[n].kind];
    } else {
        up_name(up_nth(n - NBANK), name);
        *kind = "USER";
    }
}
static UI_NOINLINE void wf_cat(const char *s) { str_cpy(wf_mq + str_len(wf_mq), s, sizeof wf_mq - str_len(wf_mq)); }
static UI_NOINLINE void wf_cati(int32_t v) { char b[12]; fmt_int(b, v); wf_cat(b); }

static UI_NOINLINE void wf_marquee_text(void)
{
    char v[8], nm[14];
    const char *unit;
    wf_mq[0] = 0;
    if (ci_on) { wf_cat("COUNT IN "); wf_cati(4 - (int32_t)ci_beat); return; }
    if (wf_recording()) { if (song.sel == TRK_DRUM) wf_cat("RECORDING DRUMS"); else { wf_cat("RECORDING TRACK "); wf_cati((int32_t)song.sel + 1); } wf_cat("  ***  IT REALLY WHIPS THE LLAMA'S ASS  1997  ***"); return; }   /* (Winamp 2) */
    if (rec_wait) {
        wf_cat("REC READY: PLAY A NOTE  *  ");
        wf_cati(TSEL->p[P_SLEN]);
        wf_cat(rec_count ? " STEPS  *  COUNT ON" : " STEPS  *  COUNT OFF");
        return;
    }
    if (ui.msg_t) { wf_cat(ui.msg); return; }
    if (wf_pg == 9u && sx_open) {
        char nm[16];
        sx_name(nm, sx_i, sizeof nm);
        if (sx_on) { wf_cat("LISTENING: "); wf_cat(nm); return; }
        wf_cat("TAPE "); wf_cati((int32_t)sx_i + 1); wf_cat("/"); wf_cati((int32_t)sx_n); wf_cat(": "); wf_cat(nm);
        wf_cat("  *  SEL: COPY TO TRACK "); wf_cat(is_drum(TSEL) ? "D" : song.sel == 0u ? "1" : song.sel == 1u ? "2" : "3");
        return;
    }
    if (wf_pg == 3u) { wf_cat("REC: STORE  *  SAVE: CHAIN  *  OCT-: LOOP / SONG  *  OCT+ TWICE: LOAD"); return; }
    if (wf_pg == 5u) { wf_cat(ui.hold_kind == 1u ? "HOLD REC TO CLEAR THE TRACK" : "HOLD SAVE TO SAVE"); return; }
    if (wf_pg == 4u && ui.hot_t) { wf_cat(wl.lab[ui.hot_col & 3u]); wf_cat(" "); wf_cat(wl.val[ui.hot_col & 3u]); return; }
    if (wf_pg == 1u && ui.hot_t) {                                           /* a page: the parameter just turned, "ATK 120MS" */
        int16_t *vp;
        const param_desc_t *d = page_desc(cur_page(), ui.hot_col & 3u, &vp);
        if (d && vp) { param_format(d, *vp, v, &unit); wf_cat(d->label); wf_cat(" "); wf_cat(v); wf_cat(unit); return; }
    }
    if (wf_saying()) {
        if (wf_hot == 12) { wf_cat("EFFECTS: "); wf_cat(WF_GRP[wf_grp]); }
        else if (wf_hot == 13) wf_cat(WF_VIEW[wf_view]);
        else if (wf_hot == 14) { wf_cat("VIS: "); wf_cat(WF_VMODE[wv_mode]); }
        else if (wf_hot >= 19) {                                          /* TRACK: "TRACK 2 LEVEL -3DB" */
            int16_t *vp;
            uint32_t q = (uint32_t)wf_hot - 19u;
            const param_desc_t *d = wt_par(q, &vp);
            wf_cat(is_drum(TSEL) ? "DRUMS " : "TRACK "); if (!is_drum(TSEL)) { wf_cati((int32_t)song.sel + 1); wf_cat(" "); }
            wf_cat(WT_KN[q]); wf_cat(" ");
            if (q == 1u) { wf_cati(*vp); wf_cat(" STEPS"); } else { param_format(d, *vp, v, &unit); wf_cat(v); wf_cat(unit); }
        }
        else if (wf_hot >= 15) {                                          /* PARTY: "FORM DUEL", "LEAD 3", "TURN 2 BARS", "STYLE TRADE" */
            uint32_t q = (uint32_t)wf_hot - 15u;
            wf_cat("PARTY "); wf_cat(WP_KN[q]); wf_cat(" ");
            wf_cat(q == 0u ? WP_FORM[wp_form] : q == 1u ? WP_TRK[wp_lead] : q == 2u ? WP_TURN[wp_turn] : WP_STYLE[wp_style]);
        }
        else {
            int16_t *vp;
            const param_desc_t *d = wf_sl((uint32_t)wf_hot, &vp);
            param_format(d, *vp, v, &unit);
            wf_cat(WF_SL[wf_hot].name);
            wf_cat(" ");
            wf_cat(v);
            wf_cat(unit);
        }
        return;
    }
    wf_cati((int32_t)song.sel + 1);                                    /* "2. DX RHODES - KEYS (16 STEPS)", as Winamp's "4. Artist - Title (3:50)" */
    wf_cat(". ");
    if (is_drum(TSEL)) {
        wf_cat("DRUMS - ");
        wf_cat(DRUM_KIT_NAMES[clamp(TDRUM->p[P_E0], 0, DRUM_KITS - 1)]);
    } else {
        uint32_t tot, cur = preset_pos(&tot);
        trk_short_name(song.sel, nm);
        wf_cat(nm);
        wf_cat(" - ");
        wf_cat(preset_kind(cur));
    }
    if (track_empty(TSEL)) wf_cat(" (EMPTY)");
    else { wf_cat(" ("); wf_cati((int32_t)trk_len(TSEL)); wf_cat(" STEPS)"); }
}


/* the notes of the selected track in the seek bar. cap: remember where they are (the copy blows them up) instead of drawing; k: their height, 0..1024+ */
static int16_t wft_rp[64][2];
static uint8_t wft_rn;
static UI_NOINLINE void wf_rnote(int cap, int32_t k, int32_t x, int32_t y, int32_t w, uint16_t c)
{
    int32_t bot = POS_Y + POS_H - 2;
    if (cap) { if (wft_rn < 64u) wft_rp[wft_rn][0] = (int16_t)x, wft_rp[wft_rn][1] = (int16_t)y, wft_rn++; return; }
    y = bot - (bot - y) * k / 1024;
    WFR(x, clamp(y, POS_Y, bot), w, 1, c);
}
static UI_NOINLINE void wf_roll(int cap, int32_t k)
{
    const track_t *t = TSEL;
    uint32_t len = trk_len(t), j, q;
    int32_t lo = 127, hi = 0, span, cw = 228 * 16 / (int32_t)len, rows = POS_H - 2;
    uint16_t nc = WF_KNOB[(song.sel + 3u) & 3u];
    if (cap) wft_rn = 0;
    if (!is_drum(t)) {
        for (j = 0; j < len; j++) { const step_t *st = &t->step[j]; if (st->time == ST_NOTE) for (q = 0; q < st->n; q++) { if (st->note[q] < lo) lo = st->note[q]; if (st->note[q] > hi) hi = st->note[q]; } }
        span = hi - lo < rows - 1 ? rows - 1 : hi - lo;                /* one row a semitone when it fits */
        if (hi >= lo) {
            int32_t base = lo - (span - (hi - lo)) / 2, r;
            if (!cap) for (r = 0; r < rows; r++) if (((uint32_t)(base + r * span / (rows - 1)) + 12u - (uint32_t)t->p[P_ROOT] % 12u) % 12u == 0u)
                WFR(6, POS_Y + POS_H - 2 - r, 228, 1, RGB(0x22, 0x22, 0x3a));   /* the key's root, a faint line */
            for (j = 0; j < len; j++) {
                const step_t *st = &t->step[j];
                uint32_t e = j + 1u;
                if (st->time != ST_NOTE) continue;
                while (e < len && t->step[e].time == ST_TIE) e++;            /* a held note: as long as its ties */
                for (q = 0; q < st->n; q++)
                    wf_rnote(cap, k, 6 + (int32_t)(j * 228u / len), POS_Y + POS_H - 2 - (st->note[q] - base) * (rows - 1) / span,
                             (int32_t)((e - j) * 228u / len) - 1 > 1 ? (int32_t)((e - j) * 228u / len) - 1 : 1, nc);
            }
        }
    } else {
        for (j = 0; j < len; j++) for (q = 0; q < DRUM_LANES; q++)
            if (dstep_has(&t->dstep[j], q)) wf_rnote(cap, k, 6 + (int32_t)(j * 228u / len), POS_Y + POS_H - 2 - (int32_t)q * (rows - 1) / 15, cw / 16 > 2 ? cw / 16 - 1 : 2, nc);
    }
}


/* a track's loop in its button, three pixels high: a yellow line per note, its height the pitch (the drums: the sound); a light dot where the loop is */
static UI_NOINLINE void wf_miniroll(uint32_t ti, int32_t x0, int32_t y0, int32_t w)
{
    const track_t *t = &trk[ti];
    uint32_t len = trk_len(t), j, q, any = 0;
    int32_t lo = 127, hi = 0, span;
    uint16_t c = t->p[P_MUTE] ? RGB(0x80, 0x60, 0x20) : RGB(0xf0, 0xc0, 0x30);
    WFR(x0, y0, w, 3, RGB(0x30, 0x34, 0x44));
    if (!is_drum(t)) {
        for (j = 0; j < len; j++) { const step_t *st = &t->step[j]; if (st->time == ST_NOTE) for (q = 0; q < st->n; q++) { lo = st->note[q] < lo ? st->note[q] : lo; hi = st->note[q] > hi ? st->note[q] : hi; } }
        span = hi - lo > 0 ? hi - lo : 1;
        for (j = 0; j < len; j++) {
            const step_t *st = &t->step[j];
            uint32_t e = j + 1u;
            if (st->time != ST_NOTE) continue;
            while (e < len && t->step[e].time == ST_TIE) e++;
            for (q = 0; q < st->n; q++) {
                int32_t xa = x0 + (int32_t)(j * (uint32_t)w / len), xb = x0 + (int32_t)(e * (uint32_t)w / len);
                WFR(xa, y0 + 2 - (hi > lo ? (st->note[q] - lo) * 2 / span : 1), xb - xa > 1 ? xb - xa - 1 : 1, 1, c);
                any = 1;
            }
        }
    } else {
        for (j = 0; j < len; j++) for (q = 0; q < DRUM_LANES; q++)
            if (dstep_has(&t->dstep[j], q)) { WFR(x0 + (int32_t)(j * (uint32_t)w / len), y0 + 2 - (int32_t)q * 2 / 15, 1, 1, c); any = 1; }
    }
    if (any && song.playing) WFR(x0 + (int32_t)((t->seq_idx % len) * (uint32_t)w / len), y0, 1, 3, RGB(0xff, 0xf0, 0xc0));
}

/* the REC LEDs (5 x 5, pixel art): off a little grey stone with a highlight, on an orange light, white at its heart on the beat */
static UI_NOINLINE void wf_recled(int32_t x, int32_t y, int on, int tick)
{
    static const char OFF[5][6] = {".eee.", "ehbbe", "ebbse", "essbe", ".eee."}, ON[5][6] = {".ror.", "royor", "oywyo", "royor", ".ror."};
    int32_t r, c;
    for (r = 0; r < 5; r++) for (c = 0; c < 5; c++) {
        char k = (on ? ON : OFF)[r][c];
        uint16_t col;
        if (k == '.') continue;
        if (!on) col = k == 'e' ? RGB(0x2a, 0x24, 0x22) : k == 'h' ? RGB(0xb8, 0x90, 0x8c) : k == 's' ? RGB(0x74, 0x78, 0x82) : RGB(0x52, 0x4a, 0x44);
        else if (k == 'r') col = RGB(0xa0, 0x20, 0x0c);
        else if (k == 'o') col = tick ? RGB(0xff, 0xa8, 0x30) : RGB(0xf0, 0x78, 0x18);
        else if (k == 'y') col = tick ? RGB(0xff, 0xf0, 0xb0) : RGB(0xff, 0xc4, 0x40);
        else col = RGB(0xff, 0xfc, 0xec);
        cv_pset(x + c, y + r, col);
    }
}

/* ---------------------------------------------------------------- PLAYER window */
static UI_NOINLINE uint16_t wf_trk_led(uint32_t i)
{
    const track_t *t = &trk[i];
    if (i == song.sel && wf_recording()) return (fm1_ms / 250u) & 1u ? WC_RED : WC_RED2;
    if (i == song.sel && rec_wait) return (fm1_ms / 400u) & 1u ? WC_RED : WC_RED2;
    if (t->p[P_MUTE] || (song.solo && !((song.solo >> i) & 1u))) return WC_AMB;
    return track_empty(t) ? WC_GDIM : WC_GRN;
}
static UI_NOINLINE void wf_player(void)
{
    char b[16];
    int32_t i, x;
    const track_t *t = TSEL;
    wf_window(0, WP_H, "WINAMPFM");
    /* the display: state, time, spectrum */
    if (wf_vis(LCD_X - 1, LCD_Y - 1, LCD_W + 2, LCD_H + 2)) {
        uint32_t s = wf_secs(), m = s / 60u;
        wf_inset(LCD_X, LCD_Y, LCD_W, LCD_H, WC_LCD);
        if (song.playing) {                                                                /* play: the green arrow */
            for (i = 0; i < 6; i++) WFR(LCD_X + 6 + i, LCD_Y + 6 + i, 1, 11 - 2 * i, WC_GRN);
        } else {                                                                           /* stop: the square */
            WFR(LCD_X + 6, LCD_Y + 7, 9, 9, WC_GRN);
        }
        if (wf_recording() || (rec_wait && (fm1_ms / 400u) & 1u))                          /* recording: a red dot under it */
            WFR(LCD_X + 7, LCD_Y + 20, 6, 6, WC_RED);
        x = LCD_X + 33;
        wf_digit(x, LCD_Y + 4, (m / 10u) % 10u, WC_GRN);
        wf_digit(x + 14, LCD_Y + 4, m % 10u, WC_GRN);
        WFR(x + 28, LCD_Y + 9, 2, 3, WC_GRN);
        WFR(x + 28, LCD_Y + 17, 2, 3, WC_GRN);
        wf_digit(x + 33, LCD_Y + 4, (s % 60u) / 10u, WC_GRN);
        wf_digit(x + 47, LCD_Y + 4, s % 10u, WC_GRN);
        {
            uint32_t ag = wft_age();                                                       /* a tape copied: the bars blow up, new ones rise with a swing */
            int32_t gone = ag >= 650u && ag < 1050u, k = ag >= 1050u && ag < 1650u ? wf_back((int32_t)(ag - 1050u) * 1024 / 600) : 1024;
            for (i = 0; i < 19 && !gone; i++) {                                            /* the bars and their falling peaks */
                int32_t bx = LCD_X + 5 + i * 5, r, hgt = clamp((int32_t)wf_spec[i] * k / 1024, 0, 26);
                for (r = 0; r < hgt; r++) WFR(bx, LCD_Y + LCD_H - 3 - r, 4, 1, WF_VISC[clamp(r, 0, 23) / 2]);
                if (wf_peak[i] && k == 1024) WFR(bx, LCD_Y + LCD_H - 3 - wf_peak[i], 4, 1, WC_PEAK);
            }
        }
    }
    /* the marquee */
    if (wf_vis(MQ_X - 1, MQ_Y - 1, MQ_W + 2, MQ_H + 2)) {
        int32_t tw = wf_tw(1, wf_mq);
        uint16_t c = wf_recording() || rec_wait || ci_on ? WC_RED : WC_GRN;
        wf_inset(MQ_X, MQ_Y, MQ_W, MQ_H, WC_LCD);
        wf_clx0 = MQ_X + 2, wf_clx1 = MQ_X + MQ_W - 2;
        if (tw <= MQ_W - 6) wf_text(MQ_X + 3, MQ_Y + 3, 1, wf_mq, c);
        else {                                                         /* longer than the box: it scrolls round, " *** " between */
            int32_t per = tw + wf_tw(1, " *** ") + 2, o = (int32_t)(wf_mq_off % (uint32_t)per);
            int32_t x0 = MQ_X + 3 - o;
            x0 = wf_text(x0, MQ_Y + 3, 1, wf_mq, c);
            x0 = wf_text(x0, MQ_Y + 3, 1, " *** ", c);
            wf_text(x0 + 1, MQ_Y + 3, 1, wf_mq, c);
        }
        wf_clx0 = 0, wf_clx1 = 240;
    }
    /* "120 BPM   C MIN" (Winamp: 128 kbps 48 kHz) */
    if (wf_vis(MQ_X - 1, INF_Y - 1, MQ_W + 2, 13)) {
        const param_desc_t *sd = &TP[P_SCALE];
        wf_inset(MQ_X, INF_Y, 23, 11, WC_LCD);
        fmt_int(b, song.g[G_BPM]);
        wf_textr(MQ_X + 21, INF_Y + 2, 1, b, WC_GRN);
        wf_text(MQ_X + 27, INF_Y + 2, 1, "BPM", WC_LAB);
        wf_inset(MQ_X + 52, INF_Y, 17, 11, WC_LCD);
        wf_textc(MQ_X + 61, INF_Y + 2, 1, N_NOTE[t->p[P_ROOT] % 12], WC_GRN);
        str_cpy(b, sd->names[clamp(t->p[P_SCALE], sd->min, sd->max)], 5);
        wf_text(MQ_X + 73, INF_Y + 2, 1, b, WC_LAB);
        {                                                              /* the winampFM bolt, orange, beside it; recording: a red LED that glows on the panel */
            static const int8_t BL[][2] = {{4, 0}, {8, 0}, {6, 4}, {9, 4}, {3, 11}, {4, 6}, {1, 6}};
            static const uint8_t BY4[16] = {0, 8, 2, 10, 12, 4, 14, 6, 3, 11, 1, 9, 15, 7, 13, 5};
            int32_t cx = MQ_X + 104, cy = INF_Y + 6, yy, xx, nb = (int32_t)(sizeof BL / sizeof BL[0]), q, jj;
            uint32_t ag = fm1_ms - wf_led_t;                           /* since the state changed */
            int32_t armed = rec_wait && !wf_led_on;
            if (wf_led_on || armed || (ag < 260u && wf_led_t)) {           /* REC: four small LEDs, 2 over 2, lit one per beat of the bar (the tick); armed or just stopped: off */
                uint32_t lit = 0, tick = 0;
                if (wf_led_on) {
                    uint32_t bms = 60000u / (uint32_t)clamp(song.g[G_BPM], 20, 300), ph;
                    if (song.playing) lit = (clk_beat & 3u) + 1u, ph = clk_pos / (BEAT_U / bms);
                    else { uint32_t e2 = fm1_ms - wf_led_t; lit = (e2 / bms) % 4u + 1u, ph = e2 % bms; }
                    tick = ph < 110u;
                }
                for (q = 0; q < 4; q++) wf_recled(cx - 5 + (q & 1) * 6, cy - 5 + (q >> 1) * 6, (uint32_t)q < lit, (uint32_t)q + 1u == lit && tick);
            } else {                                                   /* the bolt (back with a swing) */
                int32_t k = wf_led_t && ag < 760u ? wf_back((int32_t)(ag - 260u) * 1024 / 500) : 1024;
                for (yy = 0; yy < 12; yy++) for (xx = 0; xx < 10; xx++) {
                    int in = 0;
                    for (q = 0, jj = nb - 1; q < nb; jj = q++)
                        if ((BL[q][1] > yy) != (BL[jj][1] > yy) && xx < (BL[jj][0] - BL[q][0]) * (yy - BL[q][1]) / (BL[jj][1] - BL[q][1]) + BL[q][0]) in = !in;
                    if (in) cv_pset(cx + (xx - 5) * k / 1024, cy + (yy - 6) * k / 1024, yy < 5 ? RGB(0xff, 0xb0, 0x50) : RGB(0xf0, 0x78, 0x18));
                }
            }
        }
        b[0] = 'O', b[1] = 'C', b[2] = 'T', b[3] = song.octave < 0 ? '-' : '+';             /* the octave, only when moved */
        fmt_int(b + 4, song.octave < 0 ? -song.octave : song.octave);
        if (song.octave) wf_textr(MQ_X + MQ_W, INF_Y + 2, 0, b, WC_AMB);
    }
    /* the view buttons (Winamp's EQ / PL) and the REC lamp */
    if (wf_vis(MQ_X - 1, VB_Y - 1, MQ_W + 2, 15)) {
        static const char *const VL[3] = {"FX", "PL", "VIS"};          /* FX first, as Winamp's EQ comes before PL; the 4th: TAPES (a tape) */
        static const uint8_t VV[4] = {1, 0, 2, 3};
        for (i = 0; i < 4; i++) {
            int on = (!wf_pg && wf_view == VV[i]) || (wf_pg == 9u && i == 3);
            int32_t bx = MQ_X + i * 30 + (i == 3 ? 1 : 0);
            uint16_t c = on ? WC_WHT : WC_GLY;
            wf_button(bx, VB_Y, 29, 13, on);
            wf_led(bx + 3, VB_Y + 4, on ? WC_GRN : WC_GDIM);
            if (i < 3) wf_textc(bx + 8 + (29 - 8) / 2, VB_Y + 3, 1, VL[i], c);
            else {                                                     /* a cassette: body, two reels */
                wf_bevel(bx + 10, VB_Y + 3, 15, 8, c, c);
                WFR(bx + 13, VB_Y + 5, 2, 2, c), WFR(bx + 20, VB_Y + 5, 2, 2, c);
                WFR(bx + 13, VB_Y + 9, 9, 1, c);
            }
        }
    }
    /* the seek bar: the loop of the selected track as a small roll (height = pitch; on the drums, the sound), the gold play head */
    if (wf_vis(4, POS_Y - 1, 232, POS_H + 2)) {
        uint32_t len = trk_len(t), p = t->seq_idx % len, j, ag = wft_age();
        wf_inset(5, POS_Y, 230, POS_H, RGB(0x0c, 0x0c, 0x18));
        for (j = 0; j < len; j += 4u) WFR(5 + (int32_t)(j * 230u / len), POS_Y, 1, POS_H, RGB(0x1c, 0x1c, 0x30));   /* the beats */
        if (!(ag >= 650u && ag < 1050u))                               /* (copied: gone while they blow up, then they rise with a swing) */
            wf_roll(0, ag >= 1050u && ag < 1650u ? wf_back((int32_t)(ag - 1050u) * 1024 / 600) : 1024);
        if (song.playing) {
            int32_t hx = 6 + (int32_t)(p * 228u / len);
            WFR(hx - 1, POS_Y, 3, POS_H, WC_KG1);
            WFR(hx, POS_Y, 1, POS_H, WC_KG2);
        }
    }
    /* the four tracks: LED, number, the sound (ALGORITHM picks) */
    if (wf_vis(4, TK_Y - 1, 232, TK_H + 2)) {
        for (i = 0; i < NTRK; i++) {
            int32_t bx = 5 + i * 58;
            int sel = (uint32_t)i == song.sel;
            char nm[14];
            wf_button(bx, TK_Y, 56, TK_H, sel);
            wf_led(bx + 4, TK_Y + 4, wf_trk_led((uint32_t)i));
            b[0] = (char)(i < 3 ? '1' + i : 'D'), b[1] = 0;
            wf_text(bx + 11, TK_Y + 2, 0, i < 3 ? "TRACK" : "DRUMS", sel ? WC_BTN : WC_BSH);
            wf_textr(bx + 52, TK_Y + 2, 0, b, sel ? WC_WHT : WC_GLY);
            if (i == TRK_DRUM) str_cpy(nm, DRUM_KIT_NAMES[clamp(TDRUM->p[P_E0], 0, DRUM_KITS - 1)], 9);
            else { trk_short_name((uint32_t)i, nm); nm[8] = 0; }
            if (ui.menu == 9u && sx_open && sel) { sx_name(nm, sx_com, 14); nm[8] = 0; }    /* TAPES: what the track keeps, not the tape being heard */
            if (sel && wft_age() < 650u) { str_cpy(nm, wft_old, 9); }   /* (copied: the old name until the tape hits) */
            wf_clx0 = bx + 2, wf_clx1 = bx + 54;
            if (sel && wft_age() >= 650u && wft_age() < 1150u) {      /* copied: the names ride an elevator, the old one up, the new one in */
                int32_t k = (int32_t)(wft_age() - 650u) * 1024 / 500, e = k < 512 ? 2 * k * k / 1024 : 1024 - 2 * (1024 - k) * (1024 - k) / 1024;
                char o[16], n2[16];
                str_cpy(o, wft_old, 9), str_cpy(n2, wft_new, 9);
                wf_cly0 = TK_Y + 8, wf_cly1 = TK_Y + 17;
                if (wft_age() < 760u) WFR(bx + 1, TK_Y + 1, 54, TK_H - 2, WC_WHT);      /* the hit: a flash */
                wf_text(bx + 4, TK_Y + 9 - e * 10 / 1024, 1, o, WC_WHT);
                wf_text(bx + 4, TK_Y + 19 - e * 10 / 1024, 1, n2, WC_GRN);
                wf_cly0 = -1000, wf_cly1 = 1000;
            } else
                wf_text(bx + 4, TK_Y + 9, 1, nm, sel ? WC_WHT : WC_GLY);
            wf_clx0 = 0, wf_clx1 = 240;
            if (!(sel && wft_age() >= 650u && wft_age() < 1050u))      /* what the track holds: a small yellow roll (copied: it blows up, then comes back) */
                wf_miniroll((uint32_t)i, bx + 4, TK_Y + 16, 48);
        }
    }
}

/* ---------------------------------------------------------------- lower window: PLAYLIST (the sounds) */
static UI_NOINLINE void wf_playlist(void)
{
    char nm[16], b[16];
    const char *kind;
    uint32_t total, cur = wf_list(&total), top, r, i;
    top = cur > 3u ? cur - 3u : 0u;
    if (total > PL_ROWS && top > total - PL_ROWS) top = total - PL_ROWS;
    if (wf_vis(PL_X - 1, LO_Y - 1, PL_W + 2, PL_H + 2)) {
        uint32_t used[NPART];
        for (i = 0; i < NPART; i++) used[i] = wf_pos_of(&trk[i]);
        wf_inset(PL_X, LO_Y, PL_W, PL_H, WC_LCD);
        wf_clx0 = PL_X + 1, wf_clx1 = PL_X + PL_W - 1;
        for (r = 0; r < PL_ROWS && top + r < total; r++) {
            uint32_t n = top + r;
            int32_t y = LO_Y + 1 + (int32_t)r * PL_RH, x;
            uint16_t c = n == cur ? WC_WHT : WC_GRN2;
            if (n == cur) WFR(PL_X + 1, y, PL_W - 2, PL_RH - 1, WC_SEL);
            wf_item(n, nm, &kind);
            fmt_int(b, (int32_t)n + 1);
            str_cpy(b + str_len(b), ".", 4);
            x = wf_text(PL_X + 3, y + 2, 1, b, c);
            wf_text(x + 3, y + 2, 1, nm, c);
            x = PL_X + PL_W - 4 - wf_tw(1, kind);
            wf_text(x, y + 2, 1, kind, c);
            if (!is_drum(TSEL))                                        /* the other tracks playing this sound: their number in a chip */
                for (i = 0; i < NPART; i++)
                    if (i != song.sel && used[i] == n) {
                        x -= 11;
                        WFR(x, y + 1, 9, 9, c);
                        b[0] = (char)('1' + i), b[1] = 0;
                        wf_text(x + 3, y + 2, 1, b, n == cur ? WC_SEL : WC_LCD);
                    }
        }
        wf_clx0 = 0, wf_clx1 = 240;
    }
    if (wf_vis(228, LO_Y - 1, 9, PL_H + 2)) {                          /* the scrollbar, its gold thumb */
        int32_t ty = LO_Y + (total > 1u ? (int32_t)(cur * (uint32_t)(PL_H - 18) / (total - 1u)) : 0);
        WFR(229, LO_Y, 6, PL_H, WC_FR1);
        WFR(229, ty, 6, 18, WC_KG1);
        wf_bevel(229, ty, 6, 18, WC_KG2, WC_KG3);
        WFR(231, ty + 7, 2, 4, WC_KG3);
    }
    if (wf_vis(4, FT_Y - 1, 232, 15)) {                                /* the foot: which knob, where in the list */
        wf_inset(5, FT_Y, 230, 13, WC_LCD);
        wf_text(9, FT_Y + 3, 1, "PRESETS", WC_LAB);
        fmt_int(b, (int32_t)cur + 1);
        str_cpy(b + str_len(b), "/", 16 - str_len(b));
        fmt_int(b + str_len(b), (int32_t)total);
        wf_text(56, FT_Y + 3, 1, b, WC_GRN);
        wf_textr(231, FT_Y + 3, 1, is_drum(TSEL) ? "DRUM KITS" : "SOUNDS", WC_LAB);
    }
}

/* ---------------------------------------------------------------- lower window: EFFECTS (the equalizer) */
static UI_NOINLINE int32_t wf_slx(uint32_t i) { return 55 + (int32_t)i * 14 + (int32_t)(i / 4u) * 5; }   /* (the preamp sits at x 14) */
static UI_NOINLINE uint16_t wf_railc(int32_t f)            /* the bar's colour follows its value, as Winamp's: green low, yellow middle, red high */
{
    static const uint16_t C[8] = {RGB(0x20, 0xb0, 0x20), RGB(0x58, 0xc8, 0x20), RGB(0x9c, 0xd8, 0x24), RGB(0xd8, 0xd8, 0x28), RGB(0xec, 0xd6, 0x33),
                                  RGB(0xec, 0xb4, 0x2c), RGB(0xe8, 0x84, 0x24), RGB(0xe0, 0x48, 0x1c)};
    return C[clamp(f, 0, 1023) * 8 / 1024];
}
static UI_NOINLINE uint16_t wf_dim(uint16_t c, uint32_t k)  /* k / 256 of the brightness */
{
    uint32_t r = (c >> 11) * k / 256u, g = ((c >> 5) & 63u) * k / 256u, b = (c & 31u) * k / 256u;
    return (uint16_t)((r << 11) | (g << 5) | b);
}
/* a Winamp EQ slider: the groove, a bar whose colour is its value (dark edge, bright core, a pale right edge), the dashes at top / middle /
 * bottom, the square grey knob with its dark centre. wide: the page sliders (a wider bar and knob) */
static UI_NOINLINE void wf_vslider2(int32_t cx, int32_t top, int32_t h, int32_t f, int wide, int hot)
{
    int32_t kh = wide ? 9 : 7, kw = wide ? 15 : 9, ky = top + h - kh - f * (h - kh) / 1024, d, bw = wide ? 3 : 1;
    uint16_t c = wf_railc(f);
    for (d = 0; d < 3; d++) {                                          /* the dashes */
        int32_t ty = top + 1 + d * (h - 3) / 2;
        WFR(cx - kw / 2 - 4, ty, 3, 1, WC_LOFF);
        WFR(cx + kw / 2 + 2, ty, 3, 1, WC_LOFF);
    }
    WFR(cx - bw - 2, top, 1, h, RGB(0x1a, 0x12, 0x46));               /* the groove's dark line */
    WFR(cx - bw - 1, top, 1, h, wf_dim(c, 90));                        /* the bar */
    WFR(cx - bw, top, 2 * bw + 1, h, c);
    WFR(cx - bw + 1, top, 1, h, wf_dim(c, 236));
    WFR(cx + bw + 1, top, 1, h, RGB(0x9c, 0x98, 0x88));
    WFR(cx + bw + 2, top, 1, h, RGB(0x50, 0x50, 0x78));
    WFR(cx - kw / 2, ky, kw, kh, hot ? WC_WHT : RGB(0xc4, 0xd2, 0xdf));   /* the knob */
    wf_bevel(cx - kw / 2, ky, kw, kh, WC_BHI, RGB(0x4c, 0x56, 0x60));
    WFR(cx - kw / 2 + 1, ky + kh - 2, kw - 2, 1, RGB(0x90, 0x9a, 0xab));
    WFR(cx - 1 - (wide ? 2 : 0), ky + 2, 3 + (wide ? 4 : 0), kh - 4, RGB(0x50, 0x5c, 0x6c));
}
/* the line above the sliders: the curve through their values (Winamp's EQ graph, coloured by height like its own) and, on top, the sound
 * itself bending it: a small line visualizer. Its own little preamp line (MASTER) as Winamp draws it. detail: the EFFECTS one (taller, the
 * pure curve drawn faintly under the moving one) */
static UI_NOINLINE void wf_gline(int32_t x0, int32_t y0, int32_t w, int32_t h, int detail, uint32_t back, int32_t xo, uint32_t keep)
{                                    /* one pass of the line; back: samples earlier (an echo), xo: drawn shifted right, keep: its brightness /256 */
    static const int8_t S16[16] = {0, 49, 90, 117, 127, 117, 90, 49, 0, -49, -90, -117, -127, -117, -90, -49};
    uint32_t sw = scope_w - back, t = fm1_ms / 70u + back / 9u;       /* (an echo: its own phase of the slow wave, so they cross) */
    int32_t x, py = -1, mid = y0 + h / 2, amp = detail ? 3 : 2;
    for (x = x0 + 1 + xo; x < x0 + w - 1; x++) {
        int32_t k = x - x0 - xo, s = scope_buf[(sw - 2u * (uint32_t)w + 2u * (uint32_t)k) & (SCOPE_N - 1u)], y, j;
        int32_t wv = S16[(uint32_t)(k / 7 + t) & 15u] + S16[(uint32_t)(k / 13 + 16u - t / 2u) & 15u] / 2;   /* -190..190 */
        y = clamp(mid + wv * (detail ? 2 : 1) * (back ? 3 : 1) / 190 + clamp(s * amp / 12000, -amp, amp), y0 + 1, y0 + h - 2);
        if (py < 0) py = y;
        for (j = py < y ? py : y; j <= (py < y ? y : py); j++) cv_pset(x, j, wf_dim(j == mid ? WC_GLD1 : RGB(0xd8, 0xc0, 0x50), keep));
        py = y;
    }
}
static UI_NOINLINE void wf_graph(int32_t x0, int32_t y0, int32_t w, int32_t h, uint32_t n, const int32_t *xs, const int32_t *fr, int32_t pre, int detail)
{                                    /* the line: straight in the middle, alive (slow small waves; the sound moves it a little); with DELAY, its echoes behind it */
    uint32_t i, dly = (uint32_t)clamp(TSEL->p[P_DLY], 0, 127), fb = (uint32_t)clamp(song.g[G_DFDBK], 0, 120), ne = dly > 3u ? 1u + fb / 40u : 0u, e;
    (void)fr, (void)pre;
    wf_inset(x0, y0, w, h, RGB(0x10, 0x10, 0x20));
    for (i = 0; i < n; i++) WFR(xs[i], y0, 1, h, RGB(0x26, 0x28, 0x40));      /* the bands' lines */
    for (e = ne; e >= 1u && e <= 4u; e--)                                       /* the echoes: later in the sound, further right, fainter */
        wf_gline(x0, y0, w, h, detail, e * 46u, (int32_t)e * (detail ? 9 : 6), 90u + (dly * 140u / 127u) / e);
    wf_gline(x0, y0, w, h, detail, 0, 0, 256);
}
static UI_NOINLINE int32_t wf_master_f(void) { return clamp((int32_t)wf_isqrt(song.master_q12 << 8) * 1024 / 1022, 0, 1024); }   /* the MASTER knob's own travel */
/* TRACK (the 5th group): the selected track's own mix, the SLOOP TRACKS page as sliders. KNOB 1 LEVEL (the drums: DRUMS LEVEL), 2 STEPS (its loop:
 * the cuts of the recorder are 16 of them each), 3 PAN, 4 SWING (every track). ui_input.c tracks_edit does the turning. */
static UI_NOINLINE void wf_trackgrp(void)                             /* the TRACK group: four big sliders, their values, the loop's cuts */
{
    uint32_t i;
    char v[16];
    const char *unit;
    if (wf_vis(4, CRV_Y - 1, 232, CRV_H + 2)) {
        int32_t xs[1] = {0}, fr[1] = {0};
        wf_graph(46, CRV_Y, 189, CRV_H, 0, xs, fr, wf_master_f(), 1);
    }
    if (wf_vis(4, EQ_TOP - 4, 232, EQ_RAIL + 16)) {
        int16_t *vp;
        wf_vslider2(14, EQ_TOP, EQ_RAIL, wf_master_f(), 0, 0);
        wf_textc(15, EQ_TOP + EQ_RAIL + 3, 0, "MAST", WC_LAB);
        for (i = 0; i < 4u; i++) {
            const param_desc_t *d = wt_par(i, &vp);
            int32_t x = 52 + (int32_t)i * 50;
            wf_vslider2(x, EQ_TOP, EQ_RAIL, wf_frac(d, *vp), 1, wf_hot == (int32_t)(19u + i) && wf_saying());
            WFR(x - 7, EQ_TOP + EQ_RAIL + 10, 15, 2, WF_KNOB[i]);
            param_format(d, *vp, v, &unit);
            if (i == 1u) { fmt_int(v, *vp); unit = ""; }                   /* STEPS: the count */
            str_cpy(v + str_len(v), unit, sizeof v - str_len(v));
            wf_text(x + 11, EQ_TOP + 6, 1, v, WC_GRN);
        }
        {                                                              /* the loop's cuts: 16 steps each, as the recorder shows them */
            uint32_t len = (uint32_t)clamp(TSEL->p[P_SLEN], 1, 64), n = (len + 15u) / 16u, c;
            for (c = 0; c < 4u; c++) WFR(52 + 50 + 11 + (int32_t)c * 7, EQ_TOP + 18, 5, 3, c < n ? WC_AMB : RGB(0x30, 0x24, 0x10));
        }
    }
    if (wf_vis(4, FT_Y - 1, 232, 15)) {
        wf_inset(5, FT_Y, 230, 13, WC_LCD);
        for (i = 0; i < 4u; i++) {
            int32_t fx = 8 + (int32_t)i * 57;
            WFR(fx, FT_Y + 3, 5, 7, WF_KNOB[i]);
            wf_text(fx + 8, FT_Y + 3, 1, WT_KN[i], WF_KNOB[i]);
        }
    }
}
static UI_NOINLINE void wf_party(void)                                /* the PARTY group: its 4 sliders, and the stage (who plays now, who is next) */
{
    static const uint8_t WPN[4] = {3, 3, 4, 1};                       /* the knobs' last values */
    uint8_t val[4];
    uint32_t i, left, m = wp_stage(&left), frac = 0;
    char b[16];
    val[0] = wp_form, val[1] = wp_lead, val[2] = wp_turn, val[3] = wp_style;
    if (wf_vis(4, CRV_Y - 1, 232, CRV_H + 2)) {                        /* the line stays alive above */
        int32_t xs[1] = {0}, fr[1] = {0};
        wf_graph(46, CRV_Y, 189, CRV_H, 0, xs, fr, wf_master_f(), 1);
    }
    if (wf_vis(4, EQ_TOP - 4, 232, EQ_RAIL + 16)) {
        wf_vslider2(14, EQ_TOP, EQ_RAIL, wf_master_f(), 0, 0);
        wf_textc(15, EQ_TOP + EQ_RAIL + 3, 0, "MAST", WC_LAB);
        for (i = 0; i < 4u; i++) {
            int32_t x = 38 + (int32_t)i * 17;
            wf_vslider2(x, EQ_TOP, EQ_RAIL, (int32_t)val[i] * 1024 / WPN[i], 0, wf_hot == (int32_t)(15u + i) && wf_saying());
            WFR(x - 5, EQ_TOP + EQ_RAIL + 10, 11, 2, WF_KNOB[i]);
        }
        wf_inset(110, EQ_TOP - 3, 125, EQ_RAIL + 15, WC_LCD);           /* the stage */
        wf_text(114, EQ_TOP, 1, WP_FORM[wp_form], WC_GRN);
        if (wp_turn && wp_form < 3u) {
            str_cpy(b, WP_STYLE[wp_style], sizeof b);
            str_cpy(b + str_len(b), " ", sizeof b - str_len(b));
            str_cpy(b + str_len(b), WP_TURN[wp_turn], sizeof b - str_len(b));
        } else str_cpy(b, wp_form < 3u ? "STAYS" : "ALL PLAY", sizeof b);
        wf_textr(232, EQ_TOP + 1, 0, b, WC_DB);
        if (wp_hint_t && fm1_ms - wp_hint_t < WP_HINT_MS) {            /* KNOB 3 / 4 turned: a yellow arrow taps the text that changed */
            uint32_t e = fm1_ms - wp_hint_t, ph = e % 250u;
            int32_t c, bx = 232 - wf_tw(0, b) - 9 + (int32_t)(ph < 90u ? ph * 3u / 90u : ph < 140u ? 3u : (250u - ph) * 3u / 110u);   /* in, a beat, back */
            for (c = 0; c < 4; c++) WFR(bx + c, EQ_TOP + 1 + c, 1, 7 - 2 * c, RGB(0xff, 0xd8, 0x30));
        }
        for (i = 0; i < 4u; i++) {                                     /* 1 2 3 D: lit on stage, dim off it */
            int32_t x = 114 + (int32_t)i * 30, y = EQ_TOP + 11;
            int on = (m >> i) & 1u;
            char nm[14];
            WFR(x, y, 27, 20, on ? RGB(0x10, 0x3a, 0x10) : RGB(0x10, 0x10, 0x1a));
            WFR(x, y, 27, 1, on ? WC_GRN : WC_LOFF);
            wf_textc(x + 14, y + 3, 1, WP_TRK[i], on ? WC_WHT : WC_LOFF);
            if (i < NPART) trk_short_name(i, nm); else str_cpy(nm, "DRUMS", sizeof nm);
            nm[6] = 0;
            wf_textc(x + 14, y + 12, 0, nm, on ? WC_GRN : WC_LOFF);
            if (i == wp_lead) WFR(x + 11, y + 21, 6, 2, WF_KNOB[1]);   /* the LEAD */
        }
        if (song.playing && wp_turn && wp_form < 3u) {                 /* bars to the next turn: the last one fills */
            wp_bar(&frac);
            str_cpy(b, "NEXT ", sizeof b), fmt_int(b + 5, (int32_t)left);
            wf_text(114, EQ_TOP + EQ_RAIL + 2, 0, b, WC_LAB);
            WFR(150, EQ_TOP + EQ_RAIL + 4, 80, 2, RGB(0x30, 0x24, 0x10));
            WFR(150, EQ_TOP + EQ_RAIL + 4, (int32_t)wp_el * 80 / 1024, 2, WC_AMB);   /* (the time to the next turn: empties on a new LEAD) */
        }
    }
    if (wf_vis(4, FT_Y - 1, 232, 15)) {
        wf_inset(5, FT_Y, 230, 13, WC_LCD);
        for (i = 0; i < 4u; i++) {
            static const uint8_t PX[4] = {8, 62, 116, 182};             /* (AUTOTURN is the long one) */
            int32_t fx = PX[i];
            WFR(fx, FT_Y + 3, 5, 7, WF_KNOB[i]);
            wf_text(fx + 8, FT_Y + 3, 1, WP_KN[i], WF_KNOB[i]);
        }
    }
}
static UI_NOINLINE void wf_effects(void)
{
    int32_t i, xs[12], fr[12];
    int16_t *vp;
    if (wf_vis(4, LO_Y - 1, 232, 14)) {                                /* the group buttons (Winamp's ON / AUTO / PRESETS): LED left, name centred */
        for (i = 0; i < 5; i++) {
            int on = wf_grp == (uint32_t)i, bx = 5 + i * 46;
            wf_button(bx, LO_Y, 45, 13, on);
            WFR(bx + 3, LO_Y + 5, 2, 3, on ? WC_GRN : WC_GDIM);          /* (a slim LED: five buttons share the row) */
            wf_textc(bx + 6 + (45 - 6) / 2, LO_Y + 3, 1, WF_GRP[i], on ? WC_WHT : WC_GLY);
        }
    }
    if (wf_grp == 3u) { wf_party(); return; }
    if (wf_grp == 4u) { wf_trackgrp(); return; }
    for (i = 0; i < 12; i++) { const param_desc_t *d = wf_sl((uint32_t)i, &vp); xs[i] = wf_slx((uint32_t)i); fr[i] = wf_frac(d, *vp); }
    if (wf_vis(4, CRV_Y - 1, 232, CRV_H + 2)) {                        /* the line: the 12 values joined, bent by the sound */
        wf_graph(46, CRV_Y, 189, CRV_H, 12, xs, fr, wf_master_f(), 1);
    }
    if (wf_vis(4, EQ_TOP - 4, 232, EQ_RAIL + 16)) {
        wf_vslider2(14, EQ_TOP, EQ_RAIL, wf_master_f(), 0, 0);        /* the preamp: MASTER */
        wf_textc(15, EQ_TOP + EQ_RAIL + 3, 0, "MAST", WC_LAB);
        wf_textr(41, EQ_TOP - 1, 0, "MAX", WC_DB);                     /* (Winamp: +12 db, +0 db, -12 db) */
        wf_textr(41, EQ_TOP + EQ_RAIL / 2 - 3, 0, "MID", WC_DB);
        wf_textr(41, EQ_TOP + EQ_RAIL - 5, 0, "MIN", WC_DB);
        for (i = 0; i < 12; i++) {
            int act = (uint32_t)i / 4u == wf_grp;
            wf_vslider2(xs[i], EQ_TOP, EQ_RAIL, fr[i], 0, act && wf_hot == i && wf_saying());
            wf_textc(xs[i] + 1, EQ_TOP + EQ_RAIL + 3, 0, WF_SL[i].lab, act ? WC_WHT : WC_LOFF);
            if (act) WFR(xs[i] - 5, EQ_TOP + EQ_RAIL + 10, 11, 2, WF_KNOB[i & 3]);   /* the knob that moves it */
        }
    }
    if (wf_vis(4, FT_Y - 1, 232, 15)) {                                /* the foot: KNOB 1..4 -> what they move, in the knobs' colours */
        wf_inset(5, FT_Y, 230, 13, WC_LCD);
        for (i = 0; i < 4; i++) {
            int32_t fx = 8 + i * 57;
            WFR(fx, FT_Y + 3, 5, 7, WF_KNOB[i]);
            wf_text(fx + 8, FT_Y + 3, 1, WF_SL[wf_grp * 4u + (uint32_t)i].shrt, WF_KNOB[i]);
        }
    }
}

/* ---------------------------------------------------------------- lower window: VISUALIZER (lines only, pixel art) */
static uint8_t wv_buf[VZ_CH][VZ_CW] __attribute__((section(".pool")));   /* intensity 0..15 of each 2 x 2 cell; it fades: the trails (pool: RAM is tight under SLOOP 2.6) */
static uint32_t wv_ph;                        /* its own clock */
static const int8_t WV_SIN[64] = {0, 12, 25, 37, 49, 60, 71, 81, 90, 98, 106, 112, 117, 122, 125, 126, 127, 126, 125, 122, 117, 112, 106,
    98, 90, 81, 71, 60, 49, 37, 25, 12, 0, -12, -25, -37, -49, -60, -71, -81, -90, -98, -106, -112, -117, -122, -125, -126, -127, -126,
    -125, -122, -117, -112, -106, -98, -90, -81, -71, -60, -49, -37, -25, -12};
static UI_NOINLINE void wv_put(int32_t x, int32_t y, uint32_t v) { if ((uint32_t)x < VZ_CW && (uint32_t)y < VZ_CH && wv_buf[y][x] < v) wv_buf[y][x] = (uint8_t)v; }
static UI_NOINLINE void wv_line(int32_t x0, int32_t y0, int32_t x1, int32_t y1, uint32_t v)
{
    int32_t dx = x1 > x0 ? x1 - x0 : x0 - x1, dy = y1 > y0 ? y0 - y1 : y1 - y0, sx = x0 < x1 ? 1 : -1, sy = y0 < y1 ? 1 : -1, e = dx + dy, n = 0;
    for (;;) {
        wv_put(x0, y0, v);
        int32_t e2 = 2 * e;
        if ((x0 == x1 && y0 == y1) || ++n > 400) break;
        if (e2 >= dy) { e += dy; x0 += sx; }
        if (e2 <= dx) { e += dx; y0 += sy; }
    }
}
static UI_NOINLINE void wv_step(void)
{
    int32_t x, y, i;
    uint32_t dm = wv_mode;                                             /* the picture drawn: OLDSCHOOL takes them in turn by itself */
    uint32_t w = scope_w, bass = (uint32_t)(wf_spec[0] + wf_spec[1] + wf_spec[2] + wf_spec[3]) / 4u, all = 0;
    for (i = 0; i < 19; i++) all += wf_spec[i];
    if (wv_mode == 2u) { static const uint8_t SEQ[5] = {2, 4, 3, 2, 1}; dm = SEQ[(wv_ph / 200u) % 5u]; }   /* MILK: the old-school one, every scene in turn by itself */
    if (wv_mode == 2u) {                                               /* MILK: the picture flies out of the middle (MilkDrop / Geiss feedback): */
        int32_t q;                                                     /* each cell takes the one nearer the centre, in place, outer cells first */
        for (q = 0; q < 4; q++) {
            int32_t sx = q & 1 ? 1 : -1, sy = q & 2 ? 1 : -1, cx = VZ_CW / 2, cy = VZ_CH / 2, dy, dx;
            for (dy = VZ_CH / 2; dy >= 0; dy--)
                for (dx = VZ_CW / 2; dx >= 0; dx--) {
                    int32_t tx = cx + sx * dx, ty = cy + sy * dy, fx = cx + sx * (dx * 7 / 8), fy = cy + sy * (dy * 7 / 8);
                    uint32_t v;
                    if ((uint32_t)tx >= VZ_CW || (uint32_t)ty >= VZ_CH) continue;
                    v = wv_buf[fy][fx];
                    wv_buf[ty][tx] = (uint8_t)(v > 1u ? v - 1u : 0u);
                }
        }
    } else
        for (y = 0; y < VZ_CH; y++)                                    /* the fade */
            for (x = 0; x < VZ_CW; x++) {
                uint32_t v = wv_buf[y][x];
                wv_buf[y][x] = (uint8_t)(v > 3u ? v - 3u : 0u);
            }
    wv_ph++;
    if (dm == 0u) {                                                /* SCOPE: the output as a line, Winamp's oscilloscope */
        int32_t py = VZ_CH / 2;
        for (x = 0; x < VZ_CW; x++) {
            int32_t s = scope_buf[(w - 2u * VZ_CW + 2u * (uint32_t)x) & (SCOPE_N - 1u)];
            y = clamp(VZ_CH / 2 - s * (VZ_CH / 2) / 12000, 0, VZ_CH - 1);
            if (x) wv_line(x - 1, py, x, y, 15);
            py = y;
        }
    } else if (dm == 1u) {                                         /* BARS: the spectrum mirrored round the middle line */
        for (i = 0; i < 19; i++) {
            int32_t cx = 3 + i * 6, h = wf_spec[i] * (VZ_CH / 2 - 1) / 24;
            wv_line(cx, VZ_CH / 2 - h, cx, VZ_CH / 2 + h, 15);
            wv_line(cx + 1, VZ_CH / 2 - h, cx + 1, VZ_CH / 2 + h, 11);
            wv_put(cx, VZ_CH / 2 - 1 - wf_peak[i] * (VZ_CH / 2 - 1) / 24, 15);
        }
    } else if (dm == 2u) {                                         /* MILK: the feedback flies it out; the scene changes by itself every ~8 s, */
        uint32_t sc = (wv_ph / 160u) % 3u;                             /* as MilkDrop's presets do: the wave, rings, a turning beam */
        int32_t py = VZ_CH / 2, a, px = 0;
        if (sc == 0u) {
            for (x = 16; x < VZ_CW - 16; x++) {
                int32_t sm = scope_buf[(w - 2u * VZ_CW + 2u * (uint32_t)x) & (SCOPE_N - 1u)];
                y = clamp(VZ_CH / 2 - sm * 10 / 12000, 0, VZ_CH - 1);
                if (x > 16) wv_line(x - 1, py, x, y, 15);
                py = y;
            }
        } else if (sc == 1u) {                                         /* a ring each beat of the bass: they spread as rings in water */
            int32_t r = 3 + (int32_t)bass / 4;
            for (a = 0; a <= 32; a++) {
                int32_t ax = VZ_CW / 2 + r * 2 * WV_SIN[((uint32_t)a * 2u + 16u) & 63u] / 127, ay = VZ_CH / 2 + r * WV_SIN[((uint32_t)a * 2u) & 63u] / 127;
                if (a) wv_line(px, py, ax, ay, bass > 6u ? 15u : 9u);
                px = ax, py = ay;
            }
        } else {                                                       /* a beam turning round the middle, wider with the sound */
            uint32_t an = wv_ph * 2u;
            for (a = -1 - (int32_t)all / 80; a <= 1 + (int32_t)all / 80; a++) {
                uint32_t q = (an + (uint32_t)(a + 64)) & 63u;
                wv_line(VZ_CW / 2, VZ_CH / 2, VZ_CW / 2 + 60 * WV_SIN[(q + 16u) & 63u] / 127, VZ_CH / 2 + 30 * WV_SIN[q] / 127, 15);
                wv_line(VZ_CW / 2, VZ_CH / 2, VZ_CW / 2 - 60 * WV_SIN[(q + 16u) & 63u] / 127, VZ_CH / 2 - 30 * WV_SIN[q] / 127, 11);
            }
        }
    } else if (dm == 3u) {                                         /* RAYS: one line per band from the middle, turning */
        for (i = 0; i < 19; i++) {
            uint32_t a = (wv_ph / 2u + (uint32_t)i * 64u / 19u) & 63u;
            int32_t l = 4 + wf_spec[i] * 2 + (int32_t)all / 40;
            wv_line(VZ_CW / 2, VZ_CH / 2, VZ_CW / 2 + l * WV_SIN[(a + 16u) & 63u] / 127, VZ_CH / 2 + l * WV_SIN[a] / 254, 15);
        }
    } else {                                                           /* TUNNEL: frames rushing out of the middle, as the old video players and screen savers */
        static uint32_t tz;
        int32_t k2;
        tz += 2u + all / 30u;                                          /* faster with more sound */
        for (k2 = 0; k2 < 6; k2++) {
            int32_t z = (int32_t)((tz + (uint32_t)k2 * 43u) % 256u), r = z * z / 1100 + 1, hw = r * 2, hh = r;
            int32_t ox = WV_SIN[(wv_ph + (uint32_t)k2 * 9u + 16u) & 63u] * (8 - z / 40) / 127, oy = WV_SIN[(wv_ph * 2u + (uint32_t)k2 * 9u) & 63u] * (4 - z / 80) / 127;
            int32_t cx = VZ_CW / 2 + ox, cy = VZ_CH / 2 + oy, v = 4 + z / 24;
            hh += (int32_t)bass * z / 600;
            wv_line(cx - hw, cy - hh, cx + hw, cy - hh, (uint32_t)v), wv_line(cx + hw, cy - hh, cx + hw, cy + hh, (uint32_t)v);
            wv_line(cx + hw, cy + hh, cx - hw, cy + hh, (uint32_t)v), wv_line(cx - hw, cy + hh, cx - hw, cy - hh, (uint32_t)v);
        }
    }
}
static const uint16_t WV_PAL[6][4] = {                /* per picture: dim .. bright */
    {RGB(0x06, 0x30, 0x06), RGB(0x10, 0x78, 0x10), RGB(0x1c, 0xc8, 0x1c), RGB(0xb0, 0xff, 0xa0)},
    {RGB(0x30, 0x24, 0x08), RGB(0x80, 0x68, 0x18), RGB(0xe0, 0xc0, 0x30), RGB(0xff, 0xf0, 0xb0)},
    {RGB(0x08, 0x20, 0x40), RGB(0x18, 0x60, 0xa8), RGB(0x40, 0xb0, 0xf0), RGB(0xd8, 0xf0, 0xff)},
    {RGB(0x38, 0x10, 0x08), RGB(0x98, 0x30, 0x18), RGB(0xf0, 0x70, 0x20), RGB(0xff, 0xd8, 0x90)},
    {RGB(0x20, 0x08, 0x38), RGB(0x60, 0x18, 0xa0), RGB(0xc0, 0x40, 0xf0), RGB(0xf8, 0xc8, 0xff)},
    {RGB(0x08, 0x28, 0x30), RGB(0x10, 0x80, 0x90), RGB(0x30, 0xe0, 0xd0), RGB(0xd0, 0xff, 0xf8)}};
static UI_NOINLINE void wf_visual(void)
{
    int32_t x, y;
    if (wf_vis(VZ_X - 1, VZ_Y - 1, VZ_CW * 2 + 2, VZ_CH * 2 + 2)) {
        wf_inset(VZ_X, VZ_Y, VZ_CW * 2, VZ_CH * 2, WC_LCD);
        for (y = 0; y < VZ_CH; y++) {
            int32_t sy = VZ_Y + y * 2;
            if (sy + 2 <= wf_ry || sy >= wf_ry + wf_rh) continue;
            for (x = 0; x < VZ_CW; x++) {
                uint32_t v = wv_buf[y][x];
                if (v) WFR(VZ_X + x * 2, sy, 2, 2, WV_PAL[wv_mode == 2u ? (wv_ph / 40u) % 6u : wv_mode == 4u ? 5u : wv_mode][(v - 1u) >> 2]);
            }
        }
    }
    if (wf_vis(4, VZ_Y + VZ_CH * 2 + 1, 232, 240 - (VZ_Y + VZ_CH * 2 + 1))) {   /* the foot: the picture's name, its knob */
        int32_t fy = VZ_Y + VZ_CH * 2 + 2;   /* (rows 230..236) */
        int32_t i2, fx = 9;
        for (i2 = 0; i2 < WV_N; i2++) fx = wf_text(fx, fy, 1, WF_VMODE[i2], i2 == (int32_t)wv_mode ? WC_GRN : WC_LOFF) + 5;
        {                                                              /* SELECT changes the picture: "SEL" and a little knob, its pointer on the picture */
            static const int8_t PT[WV_N][2] = {{-3, 2}, {-3, -2}, {0, -4}, {3, -2}, {3, 2}};   /* 7 o'clock .. 5 o'clock */
            int32_t kx = 228, ky = fy + 3, yy, xx;
            wf_textr(kx - 7, fy, 1, "SEL", WC_LAB);
            for (yy = -4; yy <= 4; yy++) for (xx = -4; xx <= 4; xx++) {
                int32_t d2 = xx * xx + yy * yy;
                if (d2 <= 16) cv_pset(kx + xx, ky + yy, d2 >= 10 ? WC_BSH : WC_BTN);
            }
            for (yy = 0; yy <= 3; yy++) cv_pset(kx + PT[wv_mode % WV_N][0] * yy / 3, ky + PT[wv_mode % WV_N][1] * yy / 3, WC_AMB);
        }
    }
}


/* ---------------------------------------------------------------- lower window: a page of the firmware (ENV, LFO, FX, EDIT, GLO, ARP, SEQ...)
 * The Quest drew these as scenes; here every page is the same window: its four parameters as four big sliders, each in the colour of the
 * knob that turns it, its name above and its value below, the family's pages as tabs (tap the button again: the next one). */
static const char *const WF_FAMBTN[FAM_COUNT] = {"HOME", "ENV", "LFO", "FX", "SEL", "EDIT", "GLO", "SAVE", "ARP", "SEQ", "HOME"};
static UI_NOINLINE int wfm_page_ok(void)     /* a page this window can show (the special ones keep their own screen) */
{
    const page_t *pg = cur_page();
    if (ui.home || ui.menu) return 0;
    if (pg->scope == SC_TRK || pg->scope == SC_SONG || pg->scope == SC_DRUM) return 0;
    return 1;
}
static UI_NOINLINE void wf_pagewin(void)
{
    const page_t *pg = cur_page();
    char v[8], b[16];
    const char *unit;
    int32_t i, n = 0, x;
    uint32_t pi;
    if (wf_vis(4, LO_Y - 1, 232, 14)) {                                /* the tabs: the pages of this button */
        for (pi = 0; pi < NPAGES; pi++) if (PAGES[pi].fam == pg->fam) n++;
        x = 5;
        for (pi = 0; pi < NPAGES; pi++) {
            int32_t w;
            if (PAGES[pi].fam != pg->fam) continue;
            w = (230 - (n - 1) * 2) / n;
            wf_button(x, LO_Y, w, 13, pi == ui.page);
            if (n <= 4) {                                              /* few pages: LED and the name */
                wf_led(x + 4, LO_Y + 4, pi == ui.page ? WC_GRN : WC_GDIM);
                wf_clx0 = x + 9, wf_clx1 = x + w - 2;
                wf_textc(x + 8 + (w - 8) / 2, LO_Y + 3, 1, PAGES[pi].title, pi == ui.page ? WC_WHT : WC_GLY);
            } else {                                                   /* many (SLOOP 2.6: FX 7, GLO 8): a short name that fits, the title bar says it whole */
                char t[8];
                const char *src = PAGES[pi].title;
                uint32_t j = 0, k, two = str_len(src) > 2u && src[str_len(src) - 1u] >= '2' && src[str_len(src) - 1u] <= '9' && src[str_len(src) - 2u] == ' ';
                if (str_eq(src, "TEXTURE") || str_eq(src, "TEXTURE 2")) str_cpy(t, src[7] ? "TXTR2" : "TXTR", sizeof t);
                else if (str_eq(src, "FILT/MIX")) str_cpy(t, "FL/MX", sizeof t);   /* (not \"TEXT\") */
                else if (wf_tw(0, src) <= w - 4) str_cpy(t, src, sizeof t);
                else {
                    for (k = 0; src[k] && j < (two ? 3u : 4u); k++) if (src[k] != ' ' && src[k] != '/') t[j++] = src[k];
                    if (two) t[j++] = src[str_len(src) - 1u];
                    t[j] = 0;
                }
                wf_clx0 = x + 1, wf_clx1 = x + w - 1;
                wf_textc(x + w / 2, LO_Y + 4, 0, t, pi == ui.page ? WC_WHT : WC_GLY);
                if (pi == ui.page) WFR(x + 3, LO_Y + 10, w - 6, 1, WC_GRN);
            }
            wf_clx0 = 0, wf_clx1 = 240;
            x += w + 2;
        }
    }
    {                                                                  /* the line above the sliders (only the ones the page has) */
        int32_t xs[4], fr[4], m = 0;
        for (i = 0; i < 4; i++) {
            int16_t *vp;
            const param_desc_t *d = page_desc(pg, (uint32_t)i, &vp);
            if (d && vp) xs[m] = 33 + i * 58, fr[m] = wf_frac(d, *vp), m++;
        }
        if (m && wf_vis(4, LO_Y + 15, 232, 16)) wf_graph(5, LO_Y + 16, 230, 14, (uint32_t)m, xs, fr, -1, 0);
    }
    for (i = 0; i < 4; i++) {
        int16_t *vp;
        const param_desc_t *d = page_desc(pg, (uint32_t)i, &vp);
        int32_t cx = 33 + i * 58, hot = ui.hot_t && ui.hot_col == (uint32_t)i;
        if (!wf_vis(cx - 28, LO_Y + 32, 57, FT_Y - LO_Y - 33)) continue;
        if (!d || !vp) {
            if (i == 0 && is_drum(TSEL)) wf_text(10, LO_Y + 50, 1, "NOT ON THE DRUM TRACK", WC_LAB);
            continue;
        }
        wf_textc(cx + 1, LO_Y + 33, 1, d->label, WF_KNOB[i]);          /* the name, in the knob's colour */
        wf_vslider2(cx, LO_Y + 43, 36, wf_frac(d, *vp), 1, hot);         /* the slider, as Winamp's EQ */
        param_format(d, *vp, v, &unit);                                /* the value, in a small display */
        str_cpy(b, v, 12);
        str_cpy(b + str_len(b), unit, 16 - str_len(b));
        wf_inset(cx - 26, LO_Y + 81, 53, 12, WC_LCD);
        wf_clx0 = cx - 25, wf_clx1 = cx + 26;
        wf_textc(cx + 1, LO_Y + 84, 1, b, hot ? WC_WHT : WC_GRN);
        wf_clx0 = 0, wf_clx1 = 240;
    }
    if (wf_vis(4, FT_Y - 1, 232, 15)) {
        wf_inset(5, FT_Y, 230, 13, WC_LCD);
        str_cpy(b, "TAP ", 16);
        str_cpy(b + 4, WF_FAMBTN[pg->fam % FAM_COUNT], 12);
        x = wf_text(9, FT_Y + 3, 1, b, WC_GRN);
        x = wf_text(x + 4, FT_Y + 3, 1, "AGAIN: NEXT PAGE", WC_LAB);
        wf_textr(231, FT_Y + 3, 1, "HOME", WC_GRN);
    }
}


/* ---------------------------------------------------------------- lower window: a layer held (FX / EDIT / ARP / SEQ / SEL / GLO / SAVE + key)
 * ui_layers.c works out the 16 keys (label, lit, the band on top) and the knobs; here they are 16 Winamp buttons, one per white key, and the knobs'
 * row in their colours. */
static UI_NOINLINE const char *wl_name(uint32_t layer)
{
    switch (layer) {
    case LY_FX: return "PUNCH FX";
    case LY_ERASE: return "ERASE";
    case LY_ROLL: return "NOTE REPEAT";
    case LY_STEP: return "STEPS";
    case LY_SCALE: return "KEY + SCALE";
    case LY_MIX: return "MIX";
    case LY_SONG: return "SONG";
    default: return "KEYS";
    }
}
static UI_NOINLINE void wf_big(int32_t x, int32_t y, int32_t z, const char *s, uint16_t col);
static UI_NOINLINE int32_t wf_bigw(int32_t z, const char *s);
static UI_NOINLINE void wf_punchwin(void);
static UI_NOINLINE void wf_layerwin(void)
{
    int32_t i;
    if (wf_pg == 2u && wl.layer == LY_FX) { wf_punchwin(); return; }   /* (SLOOP 2.6: its own window, the 27 effects) */
    for (i = 0; i < 16 && !(wf_pg == 7u && !drum_page); i++) {        /* the keys: 4 x 4, left to right as on the keyboard */
        const tile_t *t = &wl.tl[i];
        int32_t x = 5 + (i & 3) * 58, y = LO_Y + (i >> 2) * 20, empty = t->bg == C_BLACK, lit = t->bg != TE_G1 && !empty;
        uint16_t face, txt;
        if (t->bg == TE_G2) lit = 0;                                   /* (the Quest's dark wood: a plain button, dimmed) */
        face = t->bg == C_WHITE ? WC_WHT : lit ? t->bg : t->bg == TE_G2 ? WC_BSH : WC_BTN;
        if (!wf_vis(x, y, 56, 18)) continue;
        if (empty) { wf_inset(x + 1, y + 1, 54, 16, WC_FR1); continue; }
        WFR(x, y, 56, 18, face);
        wf_bevel(x, y, 56, 18, lit ? WC_WHT : WC_BHI, WC_BDK);
        if (!lit) { WFR(x + 1, y + 16, 54, 1, WC_BSH); WFR(x + 54, y + 1, 1, 16, WC_BSH); }
        if (t->top) WFR(x + 2, y + 2, 52, 2, t->top == C_WHITE ? WC_WHT : t->top);
        txt = lit ? WC_BDK : t->fg == C_WHITE ? WC_BDK : WC_BSH;
        wf_clx0 = x + 2, wf_clx1 = x + 54;
        wf_textc(x + 28, y + 6, 1, t->lab, txt);
        wf_clx0 = 0, wf_clx1 = 240;
        if (t->marks) { int32_t m; for (m = 0; m <= (int32_t)t->marks; m++) WFR(x + 22 + m * 4, y + 14, 2, 2, WC_BDK); }
    }
    if (wf_vis(4, LO_Y + 79, 232, 17)) {                               /* the knobs */
        for (i = 0; i < 4; i++) {
            int32_t x = 5 + i * 58;
            if (!wl.lab[i][0]) continue;
            wf_inset(x, LO_Y + 81, 56, 13, WC_LCD);
            wf_text(x + 3, LO_Y + 85, 0, wl.lab[i], WF_KNOB[i]);
            wf_clx0 = x + 1, wf_clx1 = x + 55;
            wf_textr(x + 53, LO_Y + 84, 1, wl.val[i], WC_GRN);
            wf_clx0 = 0, wf_clx1 = 240;
        }
    }
    if (wf_vis(4, FT_Y - 1, 232, 15)) {
        wf_inset(5, FT_Y, 230, 13, WC_LCD);
        wf_text(9, FT_Y + 3, 1, wl.sub, WC_GRN);
        wf_textr(231, FT_Y + 3, 1, wf_pg == 4u || wf_pg == 7u ? "" : "LET GO: BACK", WC_LAB);
    }
}


/* ---------------------------------------------------------------- lower window: PUNCH FX (FX held), SLOOP 2.6's 27 effects
 * A small keyboard (16 white, 11 black keys) lights the key of the effect; under it a carousel: the effect's name large in the
 * middle, its neighbours on the keyboard at the sides; a new key slides the strip there (a soft swing). The knobs' row, and
 * REV / ECHO THROW (OCT- / OCT+ held) in the foot. */
static const uint8_t WPX_ORD[27] = {0, 16, 1, 17, 2, 18, 3, 4, 19, 5, 20, 6, 7, 21, 8, 22, 9, 23, 10, 11, 24, 12, 25, 13, 14, 26, 15};   /* the keys left to right: F3 F#3 G3 .. G5 */
static uint8_t wpx_last;                       /* the effect last held (its place in WPX_ORD) */
static int32_t wpx_from, wpx_to;               /* the strip: from where, to where (places, x256) */
static uint32_t wpx_t0;                        /* when it started to slide */
#define WPX_MS 320u
static UI_NOINLINE int32_t wpx_pos(void)       /* where the strip is now (x256): a fast start, a soft end, and a small swing past it (a quarter of a place at most) */
{
    uint32_t e = fm1_ms - wpx_t0;
    int32_t k, r, d = wpx_to - wpx_from, ov, sw;
    if (!wpx_t0 || e >= WPX_MS) return wpx_to;
    k = (int32_t)(e * 1024u / WPX_MS), r = 1024 - k;
    ov = (d < 0 ? -d : d) > 256 ? 64 : (d < 0 ? -d : d) / 4;           /* the swing: past by ov, then back */
    sw = k < 600 ? 0 : (k < 800 ? (k - 600) * 1024 / 200 : (1024 - k) * 1024 / 224);
    return wpx_from + d * (1024 - r * r / 1024 * r / 1024) / 1024 + (d < 0 ? -ov : ov) * sw / 1024;
}
static UI_NOINLINE void wpx_tick(void)         /* (every frame on the layer) a new key held: slide to it */
{
    uint32_t q;
    if (punch.req < 0) return;
    for (q = 0; q < 27u; q++) if (WPX_ORD[q] == (uint32_t)punch.req) break;
    if (q < 27u && q != wpx_last) {
        wpx_from = wpx_pos(), wpx_to = (int32_t)q * 256, wpx_t0 = fm1_ms | 1u, wpx_last = (uint8_t)q;
    }
}
static UI_NOINLINE int wpx_moving(void) { return wpx_t0 && fm1_ms - wpx_t0 < WPX_MS; }
static UI_NOINLINE void wf_punchwin(void)
{
    int32_t i, pos = wpx_pos();
    char v[16];
    if (wf_vis(4, LO_Y - 1, 232, 30)) {                                /* the keyboard: the key of the effect lit */
        wf_inset(5, LO_Y, 230, 28, WC_FR1);
        for (i = 0; i < 16; i++) {
            int32_t x = 6 + i * 14 + i / 2;
            int on = punch.req == i;
            WFR(x, LO_Y + 1, 13, 26, on ? WC_AMB : WC_BTN);
            WFR(x + 12, LO_Y + 1, 1, 26, WC_BSH);
        }
        for (i = 0; i < 11; i++) {                                     /* the black keys sit after white keys 0 1 2, 4 5, 7 8 9, 11 12, 14 */
            static const uint8_t AFT[11] = {0, 1, 2, 4, 5, 7, 8, 9, 11, 12, 14};
            int32_t w = AFT[i], x = 6 + w * 14 + w / 2 + 9;
            int on = punch.req == 16 + i;
            WFR(x, LO_Y + 1, 9, 16, on ? WC_AMB : WC_BDK);
            if (!on) WFR(x + 1, LO_Y + 2, 7, 1, WC_GLY);
        }
    }
    if (wf_vis(4, LO_Y + 30, 232, 46)) {                               /* the carousel */
        int32_t c = pos / 256, q;
        wf_inset(5, LO_Y + 31, 230, 44, WC_LCD);
        wf_clx0 = 6, wf_clx1 = 234;
        for (q = c - 3; q <= c + 3; q++) {
            int32_t d = q * 256 - pos, x, z, ad = d < 0 ? -d : d;
            const char *nm;
            uint16_t col;
            if (q < 0 || q >= 27) continue;
            nm = PUNCH_NAME[WPX_ORD[q]];
            x = 120 + d * 96 / 256;                                    /* 96 px a place: a margin between the names */
            z = ad < 128 ? 2 : 1;
            col = ad < 128 ? (punch.req == (int32_t)WPX_ORD[q] ? WC_AMB : WC_GRN) : wf_dim(WC_GRN, (uint32_t)clamp(200 - ad * 60 / 256, 50, 200));
            wf_big(x - wf_bigw(z, nm) / 2, LO_Y + (z == 2 ? 40 : 46), z, nm, col);
            if (ad < 128) {                                            /* (the key it is on) */
                static const char *const KN[27] = {"F3", "G3", "A3", "B3", "C4", "D4", "E4", "F4", "G4", "A4", "B4", "C5", "D5", "E5", "F5", "G5",
                                                   "F#3", "G#3", "A#3", "C#4", "D#4", "F#4", "G#4", "A#4", "C#5", "D#5", "F#5"};
                wf_textc(x, LO_Y + 62, 0, KN[WPX_ORD[q]], WC_LAB);
            }
        }
        wf_clx0 = 0, wf_clx1 = 240;
        if (punch.req < 0) wf_textc(120, LO_Y + 33, 0, "HOLD A KEY", WC_LOFF);
    }
    if (wf_vis(4, LO_Y + 78, 232, 17)) {                               /* the knobs (FILTER DUST DUCK TRK FILT) */
        for (i = 0; i < 4; i++) {
            int32_t x = 5 + i * 58;
            if (!wl.lab[i][0]) continue;
            wf_inset(x, LO_Y + 80, 56, 13, WC_LCD);
            wf_text(x + 3, LO_Y + 84, 0, wl.lab[i], WF_KNOB[i]);
            wf_clx0 = x + 1, wf_clx1 = x + 55;
            wf_textr(x + 53, LO_Y + 83, 1, wl.val[i], WC_GRN);
            wf_clx0 = 0, wf_clx1 = 240;
        }
    }
    if (wf_vis(4, FT_Y - 1, 232, 15)) {                                /* REV / ECHO THROW: lit while OCT- / OCT+ */
        wf_inset(5, FT_Y, 230, 13, WC_LCD);
        str_cpy(v, "OCT- REV THROW", sizeof v);
        wf_text(9, FT_Y + 3, 1, v, (thr_req & 1u) ? WC_AMB : WC_LOFF);
        str_cpy(v, "OCT+ ECHO THROW", sizeof v);
        wf_textr(231, FT_Y + 3, 1, v, (thr_req & 2u) ? WC_AMB : WC_LOFF);
    }
}

/* ---------------------------------------------------------------- lower window: SONG (the chain of sections), as a playlist */
static UI_NOINLINE void wf_songwin(void)
{
    uint32_t start, i;
    char b[16];
    song_sane();
    start = song_cursor > 3u ? song_cursor - 3u : 0u;
    if (wf_vis(PL_X - 1, LO_Y - 1, PL_W + 10, PL_H + 2)) {
        wf_inset(PL_X, LO_Y, PL_W + 8, PL_H, WC_LCD);
        for (i = 0; i < PL_ROWS && start + i < arrangement.count; i++) {
            uint32_t pos = start + i;
            const arr_entry_t *e = &arrangement.entry[pos];
            int sel = pos == song_cursor, used = project_used(e->scene), now = arrangement_clock.running && arrangement_clock.index == pos;
            int32_t y = LO_Y + 1 + (int32_t)i * PL_RH, x, w = (int32_t)e->bars * 90 / 64 + 4;
            uint16_t c = sel ? WC_WHT : used ? WC_GRN2 : WC_LOFF;
            if (sel) WFR(PL_X + 1, y, PL_W + 6, PL_RH - 1, WC_SEL);
            fmt_int(b, (int32_t)pos + 1);
            str_cpy(b + str_len(b), ".", 4);
            x = wf_text(PL_X + 3, y + 2, 1, b, c);
            WFR(x + 3, y + 1, 9, 9, used ? WF_KNOB[e->scene & 3u] : WC_FR2);  /* the section, in its colour */
            b[0] = (char)('A' + e->scene), b[1] = 0;
            wf_text(x + 5, y + 2, 1, b, used ? WC_BDK : WC_LOFF);
            WFR(x + 18, y + 3, w, 5, used ? (sel ? WC_WHT : WF_KNOB[e->scene & 3u]) : WC_FR2);
            if (now) WFR(x + 18, y + 8, (int32_t)(arrangement_clock.bar + 1u) * w / (e->bars ? (int32_t)e->bars : 1), 1, WC_WHT);
            fmt_int(b, e->bars);
            str_cpy(b + str_len(b), e->bars == 1 ? " BAR" : " BARS", 8);
            wf_textr(PL_X + PL_W + 4, y + 2, 1, used ? b : "EMPTY", c);
            if (now) for (x = 0; x < 4; x++) WFR(PL_X + PL_W - 52 + x, y + 2 + x, 1, 7 - 2 * x, WC_GRN);
        }
    }
    if (wf_vis(4, FT_Y - 1, 232, 15)) {
        static const char *const L[4] = {"ENTRY", "SECT", "BARS", "LEN"};
        wf_inset(5, FT_Y, 230, 13, WC_LCD);
        for (i = 0; i < 4u; i++) {
            int32_t fx = 8 + (int32_t)i * 57;
            WFR(fx, FT_Y + 3, 5, 7, WF_KNOB[i]);
            wf_text(fx + 8, FT_Y + 3, 1, L[i], WF_KNOB[i]);
        }
    }
}

/* ---------------------------------------------------------------- lower window: STEP (the selected step of the pattern), as 16 buttons */
static UI_NOINLINE void wf_step_tiles(void)
{
    const track_t *t = TSEL;
    uint32_t len = trk_len(t), page = ui.cursor / 16u, i;
    static const char *const TM[3] = {"NOTE", "TIE", "REST"};
    memset(wl.tl, 0, sizeof wl.tl);
    for (i = 0; i < 16u; i++) {
        uint32_t idx = page * 16u + i;
        tile_t *tl = &wl.tl[i];
        tl->bg = TE_G1;
        tl->fg = TE_G3;
        if (idx >= len) { tl->bg = C_BLACK; continue; }
        if (!is_drum(t)) {
            const step_t *st = &t->step[idx];
            if (step_on(st)) { note_name(tl->lab, st->note[0]); tl->fg = C_WHITE; }
            else str_cpy(tl->lab, st->time == ST_TIE ? "--" : ".", 8);
        }
        if (idx == ui.cursor) tl->bg = C_WHITE;
        if (song.playing && idx == t->seq_idx) tl->top = WC_GRN;
    }
    str_cpy(wl.lab[0], "STEP", 8), str_cpy(wl.lab[1], "NOTE", 8), str_cpy(wl.lab[2], "TIME", 8), str_cpy(wl.lab[3], "FLAG", 8);
    fmt_int(wl.val[0], (int32_t)ui.cursor + 1);
    if (!is_drum(t)) {
        const step_t *st = &t->step[ui.cursor % NSTEP];
        if (st->n) note_name(wl.val[1], st->note[0]); else str_cpy(wl.val[1], "-", 10);
        str_cpy(wl.val[2], TM[clamp(st->time, 0, 2)], 10);
        str_cpy(wl.val[3], (st->flags & SF_ACCENT) && (st->flags & SF_SLIDE) ? "A+S" : st->flags & SF_ACCENT ? "ACC" : st->flags & SF_SLIDE ? "SLD" : "-", 10);
        str_cpy(wl.sub, "KEYS: WRITE NOTES", sizeof wl.sub);
    } else {
        wl.val[1][0] = wl.val[2][0] = wl.val[3][0] = 0;
        str_cpy(wl.sub, "DRUMS: HOLD SEQ + KEY", sizeof wl.sub);
    }
    wl.layer = 0xFF;
}

/* ---------------------------------------------------------------- the HOLD (REC held: clear the track; SAVE held: save), over the player */
static UI_NOINLINE void wf_holdwin(void)
{
    uint32_t span = ui.hold_kind == 1u ? HOLD_CLEAR_MS : HOLD_SAVE_MS, el = fm1_ms - ui.hold_t0;
    int32_t r = el >= span ? 1024 : (int32_t)(el * 1024u / span);
    char b[24];
    uint16_t c = ui.hold_kind == 1u ? WC_RED : WC_GRN;
    if (ui.hold_kind == 1u) { str_cpy(b, "CLEAR TRACK ", 24); b[12] = (char)(ui.hold_trk < 3u ? '1' + ui.hold_trk : 'D'), b[13] = 0; }
    else str_cpy(b, "SAVE THE PROJECT", 24);
    wf_inset(5, LO_Y + 8, 230, 24, WC_LCD);
    wf_textc(120, LO_Y + 16, 1, b, c);
    wf_inset(5, LO_Y + 44, 230, 14, WC_FR1);                             /* the bar fills as the seek bar does */
    WFR(6, LO_Y + 45, (int32_t)(228 * r / 1024), 12, c);
    wf_textc(120, LO_Y + 66, 1, r >= 1024 ? "DONE" : ui.hold_kind == 1u ? "KEEP HOLDING REC" : "KEEP HOLDING SAVE", WC_LAB);
    wf_textc(120, LO_Y + 80, 1, "LET GO: CANCEL", WC_LOFF);
}


/* ---------------------------------------------------------------- lower window: the DRUM track's pages (GRID: a matrix of LEDs; KIT: 16 pads) */
static UI_NOINLINE void wf_drum_fill(void)                             /* the knobs (and the KIT pads) into wl, as a layer */
{
    uint32_t len = (uint32_t)clamp(TDRUM->p[P_SLEN], 1, 64), kit = drum_kit(), i;
    const dstep_t *st;
    if (drum_cursor >= len) drum_cursor = (uint8_t)(len - 1u);
    st = &TDRUM->dstep[drum_cursor];
    memset(wl.tl, 0, sizeof wl.tl);
    if (!drum_page) {
        str_cpy(wl.lab[0], "SOUND", 8), str_cpy(wl.lab[1], "STEP", 8), str_cpy(wl.lab[2], "HIT", 8), str_cpy(wl.lab[3], "LEVEL", 8);
        str_cpy(wl.val[0], lane_label(drum_lane), 10);
        fmt_int(wl.val[1], drum_cursor + 1);
        str_cpy(wl.val[2], dstep_has(st, drum_lane) ? "ON" : "-", 10);
        str_cpy(wl.val[3], dstep_has(st, drum_lane) ? LV_NAME[dstep_lvl(st, drum_lane)] : "-", 10);
    } else {
        str_cpy(wl.lab[0], "KIT", 8), str_cpy(wl.lab[1], "LEVEL", 8), str_cpy(wl.lab[2], "REVERB", 8), str_cpy(wl.lab[3], "PAN", 8);
        fmt_int(wl.val[0], (int32_t)kit + 1);
        fmt_int(wl.val[1], song.g[G_DRLVL] * 100 / 127);
        fmt_int(wl.val[2], song.g[G_DRREV] * 100 / 127);
        fmt_int(wl.val[3], TDRUM->p[P_PAN]);
        for (i = 0; i < DRUM_LANES; i++) {
            str_cpy(wl.tl[i].lab, lane_label(i), 8);
            wl.tl[i].bg = pad_lit[i] ? C_WHITE : TE_G1;
            wl.tl[i].fg = i == drum_lane ? C_WHITE : TE_G3;
            wl.tl[i].top = i == drum_lane ? WC_AMB : 0;
        }
    }
    str_cpy(wl.sub, DRUM_KIT_NAMES[kit], sizeof wl.sub);
    str_cpy(wl.sub + str_len(wl.sub), " - ", sizeof wl.sub - str_len(wl.sub));
    str_cpy(wl.sub + str_len(wl.sub), DRUM_KIT_STYLES[kit], sizeof wl.sub - str_len(wl.sub));
    wl.layer = 0xFE;
}
static UI_NOINLINE void wf_drumgrid(void)
{
    uint32_t len = (uint32_t)clamp(TDRUM->p[P_SLEN], 1, 64), bank = drum_cursor / 16u, base = drum_lane >= 8u ? 8u : 0u, r, j;
    if (!wf_vis(4, LO_Y - 1, 232, 79)) return;
    wf_inset(5, LO_Y, 230, 76, WC_LCD);
    for (r = 0; r < 8u; r++) {
        uint32_t lane = base + r;
        int32_t y = LO_Y + 2 + (int32_t)r * 9;
        wf_text(8, y + 1, 0, lane_label(lane), pad_lit[lane] ? WC_WHT : lane == drum_lane ? WC_AMB : WC_LOFF);
        for (j = 0; j < 16u; j++) {
            uint32_t p = bank * 16u + j;
            int32_t x = 40 + (int32_t)j * 12;
            uint16_t c;
            const dstep_t *st = &TDRUM->dstep[p < NSTEP ? p : 0];
            if (p >= len) continue;
            if (dstep_has(st, lane)) {
                uint32_t lv = dstep_lvl(st, lane);
                c = lv == LV_HARD ? WC_WHT : lv == LV_GHOST ? RGB(0x0c, 0x50, 0x0c) : lv == LV_SOFT ? RGB(0x14, 0x98, 0x14) : WC_GRN;
            } else c = song.playing && p == TDRUM->seq_idx ? RGB(0x30, 0x30, 0x48) : (j & 3u) == 0u ? RGB(0x1a, 0x1a, 0x2a) : RGB(0x10, 0x10, 0x1a);
            WFR(x, y, 10, 7, c);
            if (song.playing && p == TDRUM->seq_idx) WFR(x, y + 7, 10, 1, WC_GRN);
            if (p == drum_cursor && lane == drum_lane) wf_bevel(x - 1, y - 1, 12, 9, WC_RED, WC_RED);
        }
    }
}


/* ---------------------------------------------------------------- lower window: USER (the user presets), as a playlist */
static UI_NOINLINE void wf_userwin(void)
{
    int32_t first = clamp((int32_t)ui.uslot - 3, 0, (int32_t)UP_SLOTS - (int32_t)PL_ROWS), i;
    char tag[8], nm[16];
    if (wf_vis(PL_X - 1, LO_Y - 1, PL_W + 10, PL_H + 2)) {
        wf_inset(PL_X, LO_Y, PL_W + 8, PL_H, WC_LCD);
        for (i = 0; i < (int32_t)PL_ROWS; i++) {
            uint32_t k = (uint32_t)(first + i);
            int32_t y = LO_Y + 1 + i * PL_RH, sel = k == ui.uslot, used = up_used(k);
            uint16_t c = sel ? WC_WHT : used ? WC_GRN2 : WC_LOFF;
            if (sel) WFR(PL_X + 1, y, PL_W + 6, PL_RH - 1, WC_SEL);
            up_slot_label(tag, k);
            if (used) up_name(k, nm); else str_cpy(nm, "EMPTY", sizeof nm);
            wf_text(PL_X + 3, y + 2, 1, tag, c);
            wf_text(PL_X + 40, y + 2, 1, nm, c);
        }
    }
    if (wf_vis(4, FT_Y - 1, 232, 15)) {
        static const char *const L[4] = {"SLOT", "LOAD", "ERASE", "SAVE"};
        wf_inset(5, FT_Y, 230, 13, WC_LCD);
        for (i = 0; i < 4; i++) {
            int32_t fx = 8 + i * 57;
            WFR(fx, FT_Y + 3, 5, 7, WF_KNOB[i]);
            wf_text(fx + 8, FT_Y + 3, 1, L[i], WF_KNOB[i]);
        }
    }
}


/* ---------------------------------------------------------------- the RECORDER: while REC is armed or a take runs, the window below is a reel to reel in
 * lines (as the norns tape): paused while armed; recording, the reels turn (the supply one empties into the other one) and the tape between them sways a little,
 * never past the reels nor the head */
static UI_NOINLINE int wf_recmode(void) { return !wf_pg && (rec_wait || ft_on || ci_on || wf_recording()); }
static UI_NOINLINE void wf_circ(int32_t cx, int32_t cy, int32_t r, uint16_t c)
{
    int32_t a, px = 0, py = 0;
    for (a = 0; a <= 32; a++) {
        int32_t x = cx + r * WV_SIN[((uint32_t)a * 2u + 16u) & 63u] / 127, y = cy + r * WV_SIN[((uint32_t)a * 2u) & 63u] / 127;
        if (a) cv_line(px, py, x, y, c);
        px = x, py = y;
    }
}
static UI_NOINLINE void wf_reel2(int32_t cx, int32_t cy, int32_t r, uint32_t a, uint16_t c, uint16_t d)
{
    uint32_t k;
    wf_circ(cx, cy, r, c);
    wf_circ(cx, cy, r - 2, d);
    wf_bevel(cx - 3, cy - 3, 7, 7, c, c);                               /* the hub */
    WFR(cx, cy, 1, 1, c);
    for (k = 0; k < 3u; k++) {                                          /* three spokes */
        uint32_t q = (a + k * 21u) & 63u;
        cv_line(cx + 5 * WV_SIN[(q + 16u) & 63u] / 127, cy + 5 * WV_SIN[q] / 127, cx + (r - 5) * WV_SIN[(q + 16u) & 63u] / 127, cy + (r - 5) * WV_SIN[q] / 127, c);
    }
}
static UI_NOINLINE void wf_tape(int32_t x0, int32_t y0, int32_t x1, int32_t y1, int32_t wob, uint16_t c)   /* a tape span: a straight line that sways in its middle */
{
    int32_t k, px = x0, py = y0;
    for (k = 1; k <= 8; k++) {
        int32_t x = x0 + (x1 - x0) * k / 8, y = y0 + (y1 - y0) * k / 8;
        if (k < 8) y += wob * WV_SIN[(uint32_t)(k * 8) & 63u] / 127;        /* (0 at both ends: it never leaves the reel or the head) */
        cv_line(px, py, x, y, c);
        px = x, py = y;
    }
}
static UI_NOINLINE void wf_recorder(void)
{
    const uint16_t c = RGB(0xf0, 0xc0, 0x40), d = RGB(0x70, 0x54, 0x18);
    int run = wf_recording() && !ci_on;
    uint32_t len = trk_len(TSEL), p = run ? (ft_on ? ((fm1_ms - wf_led_t) % 8000u) * 1024u / 8000u : (TSEL->seq_idx % len) * 1024u / len) : 0u, i;   /* (a free take: the time it runs) */
    int32_t rl = 26 - (int32_t)p * 8 / 1024, rr = 17 + (int32_t)p * 8 / 1024, lx = 86, ly = LO_Y + 44, rx = 174, ry = LO_Y + 46, hx = 130, hy = LO_Y + 78;
    int32_t wob = run ? (int32_t)(WV_SIN[(fm1_ms / 40u) & 63u] / 64) : 0, wob2 = run ? (int32_t)(WV_SIN[(fm1_ms / 55u + 20u) & 63u] / 96) : 0;
    uint32_t a = run ? fm1_ms / 30u : 0u, a2 = run ? fm1_ms * 3u / 70u : 0u;
    if (wf_vis(4, LO_Y - 1, 232, FT_Y - LO_Y)) {
        static uint32_t pk_v, pk_t;                                     /* the peak hold */
        uint32_t sw = scope_w, pmax = 0, lg = 0, v, notes = 0, j, q, bars = len >= 16u ? len / 16u : 1u;
        int32_t vh, ph;
        char b[24];
        const track_t *t = TSEL;
        for (i = 0; i < 256u; i++) { int32_t sm = scope_buf[(sw - 256u + i) & (SCOPE_N - 1u)]; uint32_t m = (uint32_t)(sm < 0 ? -sm : sm); if (m > pmax) pmax = m; }
        while ((pmax >> lg) > 1u) lg++;
        v = pmax < 2u ? 0u : lg * 8u + (((pmax << 3) >> lg) & 7u);   /* 8 log2: 120 = full scale; 6 dB = 8 */
        vh = clamp(((int32_t)v - 72) * 64 / 48, 0, 64);              /* -36 .. 0 dB */
        if ((uint32_t)vh >= pk_v) pk_v = (uint32_t)vh, pk_t = fm1_ms;
        else if (fm1_ms - pk_t > 900u && pk_v) pk_v--;
        ph = (int32_t)pk_v;
        WFR(4, LO_Y - 1, 232, FT_Y - LO_Y - 2, WC_LCD);
        /* the input meter: dB, green to red, the peak held a moment, CLIP */
        WFR(8, LO_Y + 12, 6, 65, RGB(0x14, 0x14, 0x20));
        for (i = 0; i < (uint32_t)vh; i += 2u) WFR(8, LO_Y + 75 - (int32_t)i, 6, 1, WF_VISC[clamp((int32_t)i * 12 / 64, 0, 11)]);
        if (ph) WFR(8, LO_Y + 75 - ph, 6, 1, WC_WHT);
        {
            static const int8_t DB[5] = {0, -6, -12, -24, -36};
            static const char *const DL[5] = {"0", "-6", "-12", "-24", "-36"};
            for (i = 0; i < 5u; i++) { int32_t y = LO_Y + 76 - (DB[i] + 36) * 64 / 36; WFR(15, y, 2, 1, d); wf_text(18, y - 2, 0, DL[i], d); }
        }
        if (pmax > 30000u) wf_text(6, LO_Y + 3, 0, "CLIP", WC_RED);
        else wf_text(6, LO_Y + 3, 0, "IN", d);
        /* the tape counter (the take's time), notes, the pass */
        {
            uint32_t ms = run ? fm1_ms - wf_led_t : 0u, sec = ms / 1000u;
            b[0] = (char)('0' + sec / 600u % 10u), b[1] = (char)('0' + sec / 60u % 10u), b[2] = ':', b[3] = (char)('0' + sec % 60u / 10u), b[4] = (char)('0' + sec % 10u), b[5] = '.', b[6] = (char)('0' + ms / 100u % 10u), b[7] = 0;
            if (run || (fm1_ms / 400u) & 1u) wf_text(36, LO_Y + 2, 1, b, run ? c : d);
        }
        if (is_drum(t)) { for (j = 0; j < len; j++) for (q = 0; q < DRUM_LANES; q++) notes += dstep_has(&t->dstep[j], q); }
        else for (j = 0; j < len; j++) if (t->step[j].time == ST_NOTE) notes += t->step[j].n;
        str_cpy(b, "NOTES ", 24), fmt_int(b + 6, (int32_t)notes), wf_text(36, LO_Y + 12, 0, b, d);
        str_cpy(b, "PASS ", 24), fmt_int(b + 5, (int32_t)t->pass), wf_text(78, LO_Y + 12, 0, b, d);
        str_cpy(b, "TRACK ", 24), b[6] = (char)(song.sel < 3u ? '1' + song.sel : 'D'), b[7] = 0, wf_textr(220, LO_Y + 3, 0, b, c);
        fmt_int(b, song.g[G_BPM]), str_cpy(b + str_len(b), " BPM", 24 - str_len(b)), wf_textr(220, LO_Y + 10, 0, b, d);
        if (run || (rec_wait && (fm1_ms / 400u) & 1u)) WFR(225, LO_Y + 3, 6, 6, WC_RED);
        /* the reels, the tape, the head */
        wf_reel2(lx, ly, rl, a, c, d);
        wf_reel2(rx, ry, rr, a2, c, d);
        wf_tape(lx, ly - rl, rx, ry - rr, wob2, c);                        /* over the top */
        wf_tape(lx - rl * 3 / 5, ly + rl * 4 / 5, hx - 6, hy, wob, c);      /* down to the head ... */
        wf_tape(hx + 6, hy, rx + rr * 3 / 5, ry + rr * 4 / 5, -wob, c);     /* ... and up to the other reel */
        WFR(hx - 6, hy - 2, 13, 5, c), WFR(hx - 4, hy, 9, 1, WC_LCD);       /* the head */
        /* the loop's bars, the one being written, the step */
        {
            int32_t x0 = 36, wtot = 150, gap = 3, sw2 = (wtot - gap * ((int32_t)bars - 1)) / (int32_t)bars;
            uint32_t step = run && !ft_on ? t->seq_idx % len : 0u;
            for (i = 0; i < bars && i < 4u; i++) {
                int32_t sx = x0 + (int32_t)i * (sw2 + gap), f = run ? (ft_on ? clamp((int32_t)p * 4 - (int32_t)i * 1024, 0, 1024) : clamp(((int32_t)step - (int32_t)i * 16) * 1024 / 16, 0, 1024)) : 0;
                wf_bevel(sx, LO_Y + 86, sw2, 5, d, d);
                WFR(sx + 1, LO_Y + 87, (sw2 - 2) * f / 1024, 3, c);
                b[0] = (char)('1' + i), b[1] = 0, wf_text(sx + 1, LO_Y + 92, 0, b, d);
            }
            if (ft_on) wf_text(192, LO_Y + 86, 0, "FREE", c);
            else { str_cpy(b, "STEP ", 24), fmt_int(b + 5, (int32_t)step + 1), str_cpy(b + str_len(b), "/", 24 - str_len(b)), fmt_int(b + str_len(b), (int32_t)len); wf_text(192, LO_Y + 86, 0, b, c); }
        }
    }
    if (wf_vis(4, FT_Y - 1, 232, 15)) {
        char b[24];
        wf_inset(5, FT_Y, 230, 13, WC_LCD);
        if (ci_on) { str_cpy(b, "COUNT IN ", 24); fmt_int(b + 9, 4 - (int32_t)ci_beat); }
        else str_cpy(b, run ? "RECORDING" : "PLAY A NOTE", 24);
        wf_text(9, FT_Y + 3, 1, b, run ? WC_RED : c);
        wf_textr(231, FT_Y + 3, 1, run ? "REC: CLOSE LOOP" : "REC: CANCEL", WC_LAB);
    }
}

/* ---------------------------------------------------------------- painting */
static void wf_tapeswin(void), wf_tape_overlay(void);
static UI_NOINLINE void wf_paint(int32_t x, int32_t y, int32_t w, int32_t h)
{
    while (h > 0) {                                                    /* (the canvas holds 240 x 124 pixels) */
        int32_t hh = (int32_t)(CV_MAX / (uint32_t)w);
        if (hh > h) hh = h;
        cv_begin((uint32_t)w, (uint32_t)hh, WC_FR2);
        cv_ox = -x, cv_oy = -y;
        wf_rx = x, wf_ry = y, wf_rw = w, wf_rh = hh;
        if (wf_vis(0, 0, 240, WP_H)) wf_player();
        if (wf_vis(0, WL_Y, 240, WL_H) && wf_pg == 9u) {
            wf_window(WL_Y, WL_H, "WINAMPFM TAPES");
            wf_tapeswin();
        } else if (wf_vis(0, WL_Y, 240, WL_H) && wf_pg == 3u) {
            wf_window(WL_Y, WL_H, arrangement_enabled ? "WINAMPFM SONG MODE" : "WINAMPFM SONG");
            wf_songwin();
        } else if (wf_vis(0, WL_Y, 240, WL_H) && wf_pg == 5u) {
            wf_window(WL_Y, WL_H, ui.hold_kind == 1u ? "WINAMPFM CLEAR" : "WINAMPFM SAVE");
            wf_holdwin();
        } else if (wf_vis(0, WL_Y, 240, WL_H) && wf_pg == 8u) {
            wf_window(WL_Y, WL_H, "WINAMPFM USER PRESETS");
            wf_userwin();
        } else if (wf_vis(0, WL_Y, 240, WL_H) && wf_pg == 6u) {
            wf_window(WL_Y, WL_H, "WINAMPFM PRESETS");
            wf_playlist();
        } else if (wf_vis(0, WL_Y, 240, WL_H) && wf_pg == 7u && !drum_page) {
            wf_window(WL_Y, WL_H, "WINAMPFM DRUM GRID");
            wf_drumgrid();
            wf_layerwin();                                             /* (its knob row and foot; no keys: the grid is drawn above) */
        } else if (wf_vis(0, WL_Y, 240, WL_H) && (wf_pg == 2u || wf_pg == 4u || wf_pg == 7u)) {
            char t[24];
            str_cpy(t, "WINAMPFM ", 24);
            str_cpy(t + 9, wf_pg == 4u ? "STEP" : wf_pg == 7u ? "DRUM KIT" : wl_name(wl.layer), 15);
            wf_window(WL_Y, WL_H, t);
            wf_layerwin();
        } else if (wf_vis(0, WL_Y, 240, WL_H) && wf_pg) {
            char t[24];
            str_cpy(t, "WINAMPFM ", 24);
            str_cpy(t + 9, cur_page()->title, 15);
            wf_window(WL_Y, WL_H, t);
            wf_pagewin();
        } else if (wf_vis(0, WL_Y, 240, WL_H) && wf_recmode()) {
            wf_window(WL_Y, WL_H, "WINAMPFM RECORDER");
            wf_recorder();
        } else if (wf_vis(0, WL_Y, 240, WL_H)) {
            wf_window(WL_Y, WL_H, wf_view == 0u ? "WINAMPFM PLAYLIST" : wf_view == 1u ? "WINAMPFM EFFECTS" : "WINAMPFM VISUALIZER");
            if (wf_view == 0u) wf_playlist();
            else if (wf_view == 1u) wf_effects();
            else wf_visual();
        }
        if (wfm_tape_busy()) wf_tape_overlay();                         /* the flying tape and the bars blowing up, over everything */
        cv_ox = 0, cv_oy = 0;
        cv_blit((uint32_t)x, (uint32_t)y);
        y += hh, h -= hh;
    }
}

static UI_NOINLINE uint32_t wf_mix(uint32_t h, uint32_t v) { return (h ^ v) * 16777619u; }

static uint32_t wf_frame;                    /* ui.frame of the last paint: a gap = another screen came between, all again */
static uint32_t wf_k_lcd, wf_k_mq, wf_k_info, wf_k_pos, wf_k_trk, wf_k_lo, wf_k_crv;
static uint32_t wf_t_lcd, wf_t_mq, wf_t_vz, wf_lastcur = 0xFFFFFFFFu;
static uint8_t wf_lastsel = 0xFF;
static uint32_t wf_lastview = 0xFFFFFFFFu;

static UI_NOINLINE int wfm_here(void) { return !ui.menu && (rec_wait || ft_on || (!ui.home && cur_page()->scope == SC_TRK)); }

static void wfm_draw_(void);
static UI_NOINLINE void wfm_draw(void) { wf_pg = 0; wfm_draw_(); }        /* the player with its own windows */
static UI_NOINLINE void wfm_page_draw(void)                         /* the player with the page below */
{
    const page_t *pg = cur_page();
    wf_pg = pg->scope == SC_STEP ? 4u : pg->graph == GR_BROWSE ? 6u : pg->graph == GR_USER ? 8u : 1u;
    wfm_draw_();
}
static UI_NOINLINE void wfm_song_draw(void) { wf_pg = 3; wfm_draw_(); }   /* SONG: the chain as a playlist */
static UI_NOINLINE void wfm_drum_draw(void) { wf_pg = 7; wfm_draw_(); }   /* the drum track's GRID / KIT */
static UI_NOINLINE void wfm_hold_draw(void) { wf_pg = 5; wfm_draw_(); }   /* REC / SAVE held: the bar fills */
static void wfm_layer_draw(uint32_t layer, const char *sub, const tile_t *tl, const char *const lab[4], const char *const val[4], const int32_t ratio[4])
{                                                                      /* the player with the layer's keys below (ui_layers.c) */
    uint32_t i;
    (void)ratio;
    wl.layer = layer;
    str_cpy(wl.sub, sub, sizeof wl.sub);
    for (i = 0; i < 16u; i++) wl.tl[i] = tl[i];
    for (i = 0; i < 4u; i++) { str_cpy(wl.lab[i], lab[i] ? lab[i] : "", 8); str_cpy(wl.val[i], val[i] ? val[i] : "", 10); }
    wf_pg = 2;
    wfm_draw_();
}
static UI_NOINLINE void wfm_draw_(void)
{
    uint32_t now = fm1_ms, k, i, full, tot, cur;
    int16_t *vp;
    if (song.playing && !wf_was_play) wf_t0 = now;
    wf_was_play = song.playing;
    full = ui.force || ui.frame != wf_frame + 1u;
    wf_frame = ui.frame;
    if (wf_pg == 4u) wf_step_tiles();                                  /* (before any paint) */
    if (wfm_tape_busy() || (wft_t0 && fm1_ms - wft_t0 < WFT_MS + 100u)) full = 1;   /* copying: the whole screen moves */
    if (wf_pg == 7u) wf_drum_fill();
    cur = wf_list(&tot);
    if (cur != wf_lastcur && song.sel == wf_lastsel && wf_lastcur != 0xFFFFFFFFu && wf_view == 1u && !wf_pg) wf_view = 0;   /* PRESETS turned: the playlist */
    wf_lastcur = cur, wf_lastsel = song.sel;
    if (((uint32_t)wf_view | (uint32_t)wf_pg << 2 | (uint32_t)ui.page << 4 | (wf_pg == 2u ? wl.layer << 12 : 0u) | (uint32_t)ui.hold_kind << 20 | (uint32_t)drum_page << 24 | (uint32_t)wf_recmode() << 28) != wf_lastview) { wf_lastview = (uint32_t)wf_view | (uint32_t)wf_pg << 2 | (uint32_t)ui.page << 4 | (wf_pg == 2u ? wl.layer << 12 : 0u) | (uint32_t)ui.hold_kind << 20 | (uint32_t)drum_page << 24 | (uint32_t)wf_recmode() << 28; full = 1; }
    if (now - wf_t_lcd >= 40u) {                                       /* the spectrum: ~25 fps */
        wf_t_lcd = now;
        wf_spectrum();
    }
    wf_marquee_text();
    if (now - wf_t_mq >= 60u) {
        wf_t_mq = now;
        if (wf_tw(1, wf_mq) > MQ_W - 6) wf_mq_off++;
        else wf_mq_off = 0;
    }
    if (full) {
        wf_paint(0, 0, 240, 240);
        wf_k_lcd = wf_k_mq = wf_k_info = wf_k_pos = wf_k_trk = wf_k_lo = wf_k_crv = 0;
    }
    /* each part: a key of what it shows; repainted when the key changes */
    k = wf_mix(wf_secs(), song.playing | (rec_wait << 1) | ((uint32_t)wf_recording() << 2) | (((now / 400u) & 1u) << 3));
    for (i = 0; i < 19u; i++) k = wf_mix(k, wf_spec[i] | (uint32_t)wf_peak[i] << 5);
    if (k != wf_k_lcd) { wf_k_lcd = k; if (!full) wf_paint(LCD_X - 1, LCD_Y - 1, LCD_W + 2, LCD_H + 2); }
    k = wf_mix(str_hash(2166136261u, wf_mq), wf_mq_off ^ ((uint32_t)(wf_recording() || rec_wait || ci_on) << 20));
    if (k != wf_k_mq) { wf_k_mq = k; if (!full) wf_paint(MQ_X - 1, MQ_Y - 1, MQ_W + 2, MQ_H + 2); }
    k = wf_mix(wf_mix((uint32_t)song.g[G_BPM], (uint32_t)TSEL->p[P_ROOT] << 8 | (uint32_t)TSEL->p[P_SCALE] | (uint32_t)(song.octave & 15) << 16),
               song.playing | ((uint32_t)wf_recording() << 1) | (rec_wait << 2) | (uint32_t)wf_view << 3 | (rec_wait ? ((now / 400u) & 1u) << 6 : 0u));
    if ((uint8_t)wf_recording() != wf_led_on) wf_led_on = (uint8_t)wf_recording(), wf_led_t = now | 1u;
    if (wf_led_on || now - wf_led_t < 800u) k = wf_mix(k, now / 40u);      /* (the LEDs tick; the bolt comes back) */
    k = wf_mix(k, rec_wait);
    if (k != wf_k_info) { wf_k_info = k; if (!full) wf_paint(MQ_X - 1, INF_Y - 1, MQ_W + 2, VB_Y + 15 - INF_Y); }
    k = song.playing ? TSEL->seq_idx % trk_len(TSEL) + 1u : 0u;
    k = wf_mix(wf_mix(k, song.sel), (uint32_t)TSEL->p[P_SLEN]);
    for (i = 0; i < trk_len(TSEL) && i < NSTEP; i++) { const uint8_t *sb = (const uint8_t *)&TSEL->step[i]; uint32_t q; for (q = 0; q < 10u; q++) k = k * 31u + sb[q]; }
    if (k != wf_k_pos) { wf_k_pos = k; if (!full) wf_paint(4, POS_Y - 1, 232, POS_H + 2); }
    k = song.sel;
    for (i = 0; i < NTRK; i++) {
        char nm[14];
        k = wf_mix(k, wf_trk_led(i) ^ trk_level(i) << 16 ^ (uint32_t)(trk[i].p[P_MUTE] != 0) << 24);
        if (i < NPART) { trk_short_name(i, nm); k = wf_mix(k, str_hash(0, nm)); }
    }
    k = wf_mix(k, (uint32_t)TDRUM->p[P_E0]);
    for (i = 0; i < NTRK; i++) {                                       /* (their little rolls: the steps, the playing position) */
        const uint8_t *sb = (const uint8_t *)trk[i].step;
        uint32_t q, n = trk_len(&trk[i]) * 10u;
        for (q = 0; q < n && q < sizeof trk[i].step; q += 3u) k = k * 31u + sb[q];
        if (song.playing) k = wf_mix(k, trk[i].seq_idx);
    }
    if (k != wf_k_trk) { wf_k_trk = k; if (!full) wf_paint(4, TK_Y - 1, 232, TK_H + 2); }
    /* the lower window */
    if (wf_recmode()) {
        k = wf_recording() || rec_wait || ci_on ? now / 40u : 0u;       /* the reels turn, the tape sways, the dot blinks */
        if (k != wf_k_lo) { wf_k_lo = k; if (!full) wf_paint(4, LO_Y - 1, 232, 240 - LO_Y - 1); }
    } else if (wf_pg == 9u) {
        k = now / 45u;                                                 /* the tapes always move (the middle one turns): ~22 fps */
        if (k != wf_k_lo) { wf_k_lo = k; if (!full) wf_paint(4, LO_Y - 1, 232, 240 - LO_Y - 1); }
    } else if (wf_pg == 3u) {
        k = song_cursor + 17u * arrangement_enabled + 71u * arrangement_clock.index + 127u * arrangement_clock.bar + 257u * arrangement.count + 509u * arrangement_clock.running;
        for (i = 0; i < arrangement.count; i++) k = k * 31u + arrangement.entry[i].scene + 7u * arrangement.entry[i].bars;
        for (i = 0; i < ARR_SCENES; i++) k = k * 3u + (uint32_t)project_used(i);
        if (k != wf_k_lo) { wf_k_lo = k; if (!full) wf_paint(4, LO_Y - 1, 232, 240 - LO_Y - 1); }
    } else if (wf_pg == 5u) {
        k = (fm1_ms - ui.hold_t0) / 40u + ui.hold_kind * 100000u;
        if (k != wf_k_lo) { wf_k_lo = k; if (!full) wf_paint(4, LO_Y - 1, 232, 240 - LO_Y - 1); }
    } else if (wf_pg == 8u) {
        k = ui.uslot * 977u + up_gen;
        if (k != wf_k_lo) { wf_k_lo = k; if (!full) wf_paint(4, LO_Y - 1, 232, 240 - LO_Y - 1); }
    } else if (wf_pg == 6u) {
        uint32_t t2, c2 = wf_list(&t2);
        k = wf_mix(wf_mix(c2, t2), song.sel);
        if (k != wf_k_lo) { wf_k_lo = k; if (!full) wf_paint(4, LO_Y - 1, 232, 240 - LO_Y - 1); }
    } else if (wf_pg == 7u) {
        uint32_t len = (uint32_t)clamp(TDRUM->p[P_SLEN], 1, 64);
        k = wf_mix(drum_page + drum_lane * 7u + drum_cursor * 101u + len * 3u, song.playing ? TDRUM->seq_idx + 1u : 0u);
        for (i = 0; i < DRUM_LANES; i++) k = wf_mix(k, pad_lit[i] ? 1u : 0u);
        for (i = 0; i < len; i++) { const dstep_t *d = &TDRUM->dstep[i]; k = wf_mix(k, dstep_mask(d) + d->lvl[0] + d->lvl[1] * 7u + d->lvl[2] * 49u + d->lvl[3] * 343u); }
        for (i = 0; i < 4u; i++) k = str_hash(k, wl.val[i]);
        if (k != wf_k_lo) { wf_k_lo = k; if (!full) wf_paint(4, LO_Y - 1, 232, 240 - LO_Y - 1); }
    } else if (wf_pg == 2u || wf_pg == 4u) {
        k = wf_mix(str_hash(wl.layer, wl.sub), 0);
        for (i = 0; i < 16u; i++) k = wf_mix(wf_mix(str_hash(k, wl.tl[i].lab), wl.tl[i].bg | (uint32_t)wl.tl[i].top << 16), wl.tl[i].fg ^ (uint32_t)wl.tl[i].marks << 20);
        for (i = 0; i < 4u; i++) k = str_hash(str_hash(k, wl.lab[i]), wl.val[i]);
        if (wf_pg == 2u && wl.layer == LY_FX) { wpx_tick(); k = wf_mix(wf_mix(k, (uint32_t)(punch.req + 1) | thr_req << 8), wpx_moving() ? now / 20u : (uint32_t)wpx_to); }   /* (the carousel slides) */
        if (k != wf_k_lo) { wf_k_lo = k; if (!full) wf_paint(4, LO_Y - 1, 232, 240 - LO_Y - 1); }
    } else if (wf_pg) {
        k = wf_mix(ui.page, ui.hot_t ? 1u + ui.hot_col : 0u);
        for (i = 0; i < 4u; i++) { const param_desc_t *d = page_desc(cur_page(), i, &vp); k = wf_mix(k, d && vp ? (uint32_t)*vp : 0xFFFFu); }
        k = wf_mix(k, song.sel);
        if (k != wf_k_lo) { wf_k_lo = k; wf_k_crv = 0; if (!full) wf_paint(4, LO_Y - 1, 232, 240 - LO_Y - 1); }
        k = now / 70u;                                                     /* the line above the sliders is alive */
        if (wf_pg == 1u && k != wf_k_crv) { wf_k_crv = k; if (!full) wf_paint(4, LO_Y + 15, 232, 16); }
    } else if (wf_view == 2u) {
        if (now - wf_t_vz >= 50u) {                                    /* the visualizer: 20 fps */
            wf_t_vz = now;
            wv_step();
            if (!full) wf_paint(VZ_X, VZ_Y, VZ_CW * 2, VZ_CH * 2);
        }
        k = wf_mix(wv_mode, 0x55u);
        if (k != wf_k_lo) { wf_k_lo = k; if (!full) wf_paint(4, VZ_Y + VZ_CH * 2 + 1, 232, 240 - (VZ_Y + VZ_CH * 2 + 1) - 2); }
    } else if (wf_view == 1u) {
        k = wf_grp | (uint32_t)(wf_hot + 1) << 4 | (wf_saying() ? 1u << 12 : 0u);
        for (i = 0; i < 12u; i++) { const param_desc_t *d = wf_sl(i, &vp); k = wf_mix(k, (uint32_t)wf_frac(d, *vp)); }
        k = wf_mix(k, (uint32_t)wf_master_f() / 8u);                    /* the MASTER pot moves the preamp slider */
        if (wf_grp == 4u) { int16_t *vp; for (i = 0; i < 4u; i++) { wt_par(i, &vp); k = wf_mix(k, (uint32_t)(uint16_t)*vp); } k = wf_mix(k, song.sel); }
        if (wf_grp == 3u && wp_hint_t && now - wp_hint_t < WP_HINT_MS + 60u) k = wf_mix(k, now / 30u);   /* (the arrow moves) */
        if (wf_grp == 3u) { uint32_t lf, fr = 0; k = wf_mix(wf_mix(k, wp_form | wp_lead << 2 | wp_turn << 4 | wp_style << 7 | wp_stage(&lf) << 8 | lf << 12), song.playing ? (wp_bar(&fr), fr / 32u) : 0u); for (i = 0; i < NPART; i++) { char nm[14]; trk_short_name(i, nm); k = str_hash(k, nm); } }
        if (k != wf_k_lo) { wf_k_lo = k; wf_k_crv = 0; if (!full) wf_paint(4, LO_Y - 1, 232, 240 - LO_Y - 1); }
        k = now / 70u;
        if (k != wf_k_crv) { wf_k_crv = k; if (!full) wf_paint(4, CRV_Y - 1, 232, CRV_H + 2); }
    } else {
        k = wf_mix(wf_mix(cur, tot), song.sel);
        for (i = 0; i < NPART; i++) k = wf_mix(k, wf_pos_of(&trk[i]));
        if (k != wf_k_lo) { wf_k_lo = k; if (!full) wf_paint(4, LO_Y - 1, 232, 240 - LO_Y - 1); }
    }
}

static UI_NOINLINE void wfm_show(uint32_t view)                      /* open a window of the player (from any page: back home) */
{
    if (!wfm_here()) go_home();
    wf_view = (uint8_t)(view % 3u);
    wf_say(13);
}

/* HOME tapped on this screen: the next window below (EFFECTS -> PLAYLIST -> VISUALIZER) */
static void wfm_tapes_open(void);
static UI_NOINLINE void wfm_home_tap(void)
{
    wf_view = wf_view == 1u ? 0u : wf_view == 0u ? 2u : wf_view == 2u ? 3u : 1u;   /* EFFECTS -> PLAYLIST -> VISUALIZER -> TAPES */
    if (wf_view == 3u) wfm_tapes_open();
    wf_say(13);
}

/* the knobs on this screen: KNOB 1..4 the effect sliders of the group, SELECT the group (on the visualizer: its picture).
 * Touching them opens the EFFECTS window (not over the visualizer). The tempo is the MASTER group's 4th slider. */
static UI_NOINLINE int wfm_knobs(void)
{
    int32_t s;
    uint32_t k;
    if (!wfm_here() || ui.layer != LY_PLAY || ui.hold_kind) return 0;
    if ((s = panel_enc(EN_SELECT)) != 0) {
        if (wf_view == 2u) {
            wv_mode = (uint8_t)((wv_mode + (s > 0 ? 1u : WV_N - 1u)) % WV_N);
            wf_say(14);
        } else {
            if (wf_view == 1u) wf_grp = (uint8_t)clamp((int32_t)wf_grp + (s > 0 ? 1 : -1), 0, 4);
            wf_view = 1;
            wf_say(12);
        }
    }
    if (wf_grp == 4u) {                                                /* TRACK: the TRACKS page's own turning */
        for (k = 0; k < 4u; k++) {
            if ((s = panel_enc(EN_K1 + k)) == 0) continue;
            tracks_edit(WT_SLOT[k], s);
            if (wf_view == 0u) wf_view = 1;
            wf_say((int32_t)(19u + k));
        }
        return 1;
    }
    if (wf_grp == 3u) {                                                /* PARTY: a step per detent */
        static const uint8_t TOP[4] = {3, 3, 4, 1};
        uint8_t *pv[4] = {&wp_form, &wp_lead, &wp_turn, &wp_style};
        for (k = 0; k < 4u; k++) {
            if ((s = panel_enc(EN_K1 + k)) == 0) continue;
            *pv[k] = (uint8_t)clamp((int32_t)*pv[k] + (s > 0 ? 1 : -1), 0, TOP[k]);
            if (wf_view == 0u) wf_view = 1;
            wf_say((int32_t)(15u + k));
            if (k >= 2u) wp_hint_t = fm1_ms | 1u;                     /* (AUTOTURN, STYLE: the arrow points at what changed) */
        }
        return 1;
    }
    for (k = 0; k < 4u; k++) {
        uint32_t i = wf_grp * 4u + k;
        int16_t *vp;
        const param_desc_t *d;
        if ((s = panel_enc(EN_K1 + k)) == 0) continue;
        d = wf_sl(i, &vp);
        *vp = (int16_t)clamp(*vp + accel(EN_K1 + k, s, d->max - d->min), d->min, d->max);
        if (WF_SL[i].id == G_BPM && WF_SL[i].glob) ui.bpm_t = 40;
        if (wf_view == 0u) wf_view = 1;
        wf_say((int32_t)i);
    }
    return 1;
}

/* ---------------------------------------------------------------- the boot screen (splash.c calls it): a Winamp window, the bolt, the name */
static UI_NOINLINE void wf_big(int32_t x, int32_t y, int32_t z, const char *s, uint16_t col)   /* the 5 x 7 face, z times */
{
    for (; *s; s++) {
        uint32_t c = (uint8_t)*s, w, r, b;
        if (c >= 'a' && c <= 'z') c -= 32;
        if (c < 32 || c > 95) c = '?';
        w = WFM_W[c - 32];
        for (r = 0; r < WFM_H; r++)
            for (b = 0; b < w; b++)
                if ((WFM_ROWS[c - 32][r] >> (7 - b)) & 1u) WFR(x + (int32_t)b * z, y + (int32_t)r * z, z, z, col);
        x += ((int32_t)w + 1) * z;
    }
}
static UI_NOINLINE int32_t wf_bigw(int32_t z, const char *s) { return wf_tw(1, s) * z; }
static void wfm_splash(void)
{
    static const int8_t BOLT[][2] = {{14, 0}, {26, 0}, {18, 16}, {28, 16}, {6, 44}, {12, 24}, {2, 24}};   /* the lightning bolt, 30 x 44 */
    uint32_t band;
    for (band = 0; band < 2u; band++) {
        int32_t y0 = (int32_t)band * 120, x, y, i, n = (int32_t)(sizeof BOLT / sizeof BOLT[0]);
        cv_begin(240, 120, WC_FR2);
        cv_ox = 0, cv_oy = -y0;
        wf_rx = 0, wf_ry = y0, wf_rw = 240, wf_rh = 120;
        wf_window(0, 240, "WINAMPFM");
        wf_inset(14, 24, 212, 150, WC_LCD);
        for (y = 0; y < 44 * 2; y++)                                   /* the bolt: scanline fill of the polygon, 2x, in the orange of the bolt beside CHR */
            for (x = 0; x < 30 * 2; x++) {
                int in = 0, j = n - 1;
                int32_t px = x / 2, py = y / 2;
                for (i = 0; i < n; j = i++)
                    if ((BOLT[i][1] > py) != (BOLT[j][1] > py) &&
                        px < (BOLT[j][0] - BOLT[i][0]) * (py - BOLT[i][1]) / (BOLT[j][1] - BOLT[i][1]) + BOLT[i][0]) in = !in;
                if (in) cv_pset(90 + x, 36 + y, (x + y) & 8 ? RGB(0xf0, 0x78, 0x18) : y < 30 ? RGB(0xff, 0xb0, 0x50) : RGB(0xf0, 0x78, 0x18));
            }
        x = 120 - wf_bigw(3, "WINAMPFM") / 2;                       /* WINAMP in the display green, FM in the bolt's orange */
        wf_big(x, 132, 3, "WINAMP", WC_GRN);
        wf_big(x + wf_bigw(3, "WINAMP") + 3, 132, 3, "FM", RGB(0xf0, 0x78, 0x18));
        wf_textc(120, 160, 1, "SYNTH FOR THE FM-1", WC_LAB);
        wf_inset(14, 186, 212, 13, WC_LCD);
        wf_textc(120, 189, 1, "*** LOADING ***", WC_GRN);
        wf_textc(120, 210, 0, "WINAMPFM 0.1  BASED ON SLOOP 2.6  GPL-3.0", WC_LOFF);
        cv_ox = 0, cv_oy = 0;
        cv_blit(0, (uint32_t)y0);
    }
    lcd_sync();
}

/* ---------------------------------------------------------------- HOME held: the menu, as Winamp's PREFERENCES window (ui_menu.c moves and sets it) */
static UI_NOINLINE void wfm_menu_draw(void)
{
    static uint32_t sig_was = 0xFFFFFFFFu, frame_was;
    uint32_t sig = ui.menu_sel * 131u + bl_lv * 3571u + TAPE_AUTO() * 7u + usb_serial * 13u + ml_count() * 17u + settings.lowcut * 7919u + usb_full * 49979687u + boot_mute * 104729u, band, i, n;
    if (sig == sig_was && ui.frame == frame_was + 1u && !ui.force) { frame_was = ui.frame; return; }
    sig_was = sig, frame_was = ui.frame;
    for (band = 0; band < 2u; band++) {
        int32_t y0 = (int32_t)band * 120;
        cv_begin(240, 120, WC_FR2);
        cv_ox = 0, cv_oy = -y0;
        wf_rx = 0, wf_ry = y0, wf_rw = 240, wf_rh = 120;
        wf_window(0, 240, "WINAMPFM PREFERENCES");
        wf_inset(5, 15, 230, 168, WC_LCD);
        for (i = 0, n = 0; i < MI_COUNT; i++) {
            const char *val = "";
            int32_t y = 17 + (int32_t)n * 15;
            int sel = i == ui.menu_sel;
            if ((WFM_MENU_HIDE >> i) & 1u) continue;
            if (i == MI_LOWCUT) val = settings.lowcut ? "ON" : "OFF";
            else if (i == MI_USB) val = usb_full ? "FULL" : "MASTER";
            else if (i == MI_BOOT) val = boot_mute ? "OFF" : "ON";
            else if (i == MI_TAPEAUTO) val = TAPE_AUTO() ? "ON" : "OFF";
            else if (i == MI_SERIAL) val = usb_serial ? "ON" : "OFF";                                  /* (SLOOP 2.6) */
            else if (i == MI_LCLEAR) { static char lv[8]; uint32_t nc = ml_count(); if (nc) { fmt_int(lv, (int32_t)nc); str_cpy(lv + str_len(lv), " CC", 4); val = lv; } else val = "NONE"; }
            else if (i == MI_BRIGHT) { static char bv[6]; fmt_int(bv, (int32_t)BL_LV() * 10); str_cpy(bv + str_len(bv), "%", 6 - str_len(bv)); val = bv; }
            else val = ">";
            if (sel) WFR(6, y, 228, 14, WC_SEL);
            wf_text(12, y + 4, 1, MI_NAME[i], sel ? WC_WHT : WC_GRN2);
            wf_textr(228, y + 4, 1, val, sel ? WC_WHT : WC_GRN2);
            n++;
        }
        wf_text(12, 17 + (int32_t)n * 15 + 4, 1, "SKINS", WC_LOFF);        /* next: HOLD HOME = the skins */
        wf_textr(228, 17 + (int32_t)n * 15 + 4, 1, "SOON", WC_LOFF);
        wf_inset(5, 190, 230, 42, WC_LCD);
        wf_text(10, 194, 1, "PRESETS", WC_GRN), wf_text(62, 194, 1, "MOVE", WC_LAB);
        wf_text(124, 194, 1, "KNOB 1", WF_KNOB[0]), wf_text(166, 194, 1, "SET", WC_LAB);
        wf_text(10, 207, 1, "OCT+", WC_GRN), wf_text(42, 207, 1, "OPEN / OK", WC_LAB);
        wf_text(124, 207, 1, "OCT-", WC_GRN), wf_text(156, 207, 1, "BACK", WC_LAB);
        wf_text(10, 220, 1, "HOLD HOME", WC_GRN), wf_text(72, 220, 1, "CLOSE", WC_LAB);
        cv_ox = 0, cv_oy = 0;
        cv_blit(0, (uint32_t)y0);
    }
}

/* ---------------------------------------------------------------- the menu's own screens as Winamp dialogs: KEYBOARD LED, IMPORTER, ABOUT */
static UI_NOINLINE void wf_dialog(const char *title, void (*body)(void), uint32_t sig)   /* the whole screen, repainted when sig changes */
{
    static uint32_t sig_was, frame_was;
    uint32_t band;
    if (sig == sig_was && ui.frame == frame_was + 1u && !ui.force) { frame_was = ui.frame; return; }
    sig_was = sig, frame_was = ui.frame;
    for (band = 0; band < 2u; band++) {
        int32_t y0 = (int32_t)band * 120;
        cv_begin(240, 120, WC_FR2);
        cv_ox = 0, cv_oy = -y0;
        wf_rx = 0, wf_ry = y0, wf_rw = 240, wf_rh = 120;
        wf_window(0, 240, title);
        body();
        cv_ox = 0, cv_oy = 0;
        cv_blit(0, (uint32_t)y0);
    }
}
static UI_NOINLINE void wf_foot2(const char *a, const char *b, const char *c, const char *d)   /* two hints: "KNOB  what" */
{
    wf_inset(5, 220, 230, 14, WC_LCD);
    if (a) wf_text(wf_text(10, 224, 1, a, WC_GRN) + 4, 224, 1, b, WC_LAB);
    if (c) wf_textr(230, 224, 1, d, WC_LAB), wf_textr(226 - wf_tw(1, d), 224, 1, c, WC_GRN);
}
static void wfb_led(void)
{
    static const char *const FXN[6] = {"FADE", "WAVES", "EXPLOSION", "RAIN", "COMET", "FIREWORKS"};
    static const char *const FXD[6] = {"THE LIGHT FADES OUT", "WAVES GO TO BOTH SIDES", "A SMALL BURST", "DROPS FALL ON THE DIAGONAL", "A LIGHT SHOOTS RIGHT", "A ROCKET CLIMBS AND BURSTS"};
    char v[8];
    uint32_t i;
    wf_inset(5, 15, 230, 98, WC_LCD);
    for (i = 0; i < 6u; i++) {
        int32_t y = 17 + (int32_t)i * 16, on = LED_ORDER[i] == led_fx;
        if (on) WFR(6, y, 228, 15, WC_SEL);
        WFR(12, y + 4, 7, 7, on ? WC_GRN : WC_FR2);                        /* the radio */
        wf_text(26, y + 4, 1, FXN[LED_ORDER[i]], on ? WC_WHT : WC_GRN2);
    }
    wf_inset(5, 120, 230, 14, WC_LCD);
    wf_textc(120, 124, 1, FXD[led_fx % 6u], WC_GRN);
    for (i = 0; i < 3u; i++) {                                         /* KNOB 1..3: speed, fade, once / hold */
        int32_t x = 5 + (int32_t)i * 77;
        wf_inset(x, 142, 74, 30, WC_LCD);
        wf_text(x + 4, 146, 1, i == 0u ? "SPEED" : i == 1u ? "FADE" : "MODE", WF_KNOB[i]);
        if (i < 2u) fmt_int(v, i ? (int32_t)led_fd : (int32_t)led_sp); else str_cpy(v, led_hold ? "HOLD" : "ONCE", 8);
        wf_textr(x + 70, 160, 1, v, WC_GRN);
        if (i < 2u) WFR(x + 4, 168, (int32_t)(i ? led_fd : led_sp) * 66 / 15, 2, WF_KNOB[i]);
    }
    wf_foot2("PRESETS", "EFFECT", "OCT-", "BACK");
}
static void wfb_imp(void)
{
    uint32_t r, i, kib = 0, prog = imp_prog(), top = imp_top();
    char b[24], nm[14], num[12];
    wf_inset(5, 15, 230, 178, WC_LCD);
    for (r = 0; r < IMP_ROWS; r++) {
        int32_t y = 16 + (int32_t)r * 16;
        int sel, used;
        uint16_t c;
        i = top + r;
        if (i >= IMP_N) break;
        sel = i == imp_sel, used = imp_used(i), c = sel ? WC_WHT : used ? WC_GRN2 : WC_LOFF;
        nm[0] = 0, b[0] = 0;
        if (sel) WFR(6, y, 228, 15, WC_SEL);
        if (i < SMP_USER_SLOTS) {
            const smp_user_hdr_t *u = (const smp_user_hdr_t *)smp_user_xip(i);
            str_cpy(num, "USR1", sizeof num);
            num[3] = (char)('1' + i);
            if (used) {
                imp_name(nm, u->name, 8u);
                fmt_int(b, (int32_t)((u->data_len + 1023u) / 1024u));
                str_cpy(b + str_len(b), "K", sizeof b - str_len(b));
            }
        }
        wf_text(10, y + 4, 1, num, c);
        wf_text(66, y + 4, 1, used ? nm : "-", c);
        if (used) wf_textr(230, y + 4, 1, b, c);
    }
    for (i = 0; i < SMP_USER_SLOTS; i++) if (usr_nz[i]) kib += (((const smp_user_hdr_t *)smp_user_xip(i))->data_len + 1023u) / 1024u;
    wf_inset(5, 200, 230, 14, WC_LCD);
    fmt_int(b, (int32_t)kib), str_cpy(b + str_len(b), "K / 320K SAMPLES", sizeof b - str_len(b));   /* (SLOOP 2.6: 4 slots of 80K) */
    wf_text(10, 204, 1, b, WC_GRN);
    if (imp_t0 && prog < 100u) { wf_inset(5, 220, 230, 14, WC_FR1); WFR(6, 221, (int32_t)(228u * prog / 100u), 12, WC_RED); }
    else wf_foot2("REC HELD", "ERASE", "OCT-", "BACK");
}
static void wfb_about(void)
{
    if (about_pg == 0u) {
        uint32_t fw = (uint32_t)((uintptr_t)_etext - FM1_APP_BASE) + (uint32_t)(_rt_end - _rt_start) * 4u + (uint32_t)(_data_end - _data_start) * 4u;
        uint32_t ram = (uint32_t)(_data_end - _data_start + _bss_end - _bss_start) * 4u, pool = (uint32_t)(_pool_end - _pool_start) * 4u, i, kib = 0;
        static const char *const N[4] = {"FIRMWARE", "RAM", "POOL", "SAMPLES"};
        uint32_t used[4], max[4] = {0x8DFBCu / 1024u, 96u, 0x54000u / 1024u, 192u};
        char b[24];
        for (i = 0; i < SMP_USER_SLOTS; i++) if (usr_nz[i]) kib += (((const smp_user_hdr_t *)smp_user_xip(i))->data_len + 1023u) / 1024u;
        used[0] = fw / 1024u, used[1] = ram / 1024u, used[2] = pool / 1024u, used[3] = kib;
        for (i = 0; i < 4u; i++) {                                     /* a bar per memory, as the seek bar */
            int32_t y = 18 + (int32_t)i * 44;
            uint32_t u = used[i] > max[i] ? max[i] : used[i];
            wf_text(8, y, 1, N[i], WF_KNOB[i]);
            fmt_int(b, (int32_t)u), str_cpy(b + str_len(b), "K / ", sizeof b - str_len(b));
            fmt_int(b + str_len(b), (int32_t)max[i]), str_cpy(b + str_len(b), "K", sizeof b - str_len(b));
            wf_textr(232, y, 1, b, WC_LAB);
            wf_inset(6, y + 12, 228, 12, WC_FR1);
            WFR(7, y + 13, (int32_t)(226u * u / (max[i] ? max[i] : 1u)), 10, WF_KNOB[i]);
        }
        wf_foot2("PRESETS", "CREDITS", "OCT-", "BACK");
    } else {
        static const char *const L[10] = {"WINAMPFM SYNTH FOR THE FM-1", "INTERFACE: HERICDK", "BASED ON SLOOP 2.6 (ISOD89)", "FELUCCA (KUROGEDELIC), FM1 QUEST",
                                          "R/MVAVEFM1 COMMUNITY", "SAMPLES: VERSILIAN, SONIC PI", "PHASE: CRISPYZEBRA (GPL)", "VOICE: KLATTSCH (MIT)",
                                          "LOOK INSPIRED BY WINAMP 2.X", "GPL-3.0, NO WARRANTY"};
        uint32_t i;
        wf_inset(5, 15, 230, 198, WC_LCD);
        for (i = 0; i < 10u; i++) wf_text(10, 20 + (int32_t)i * 19, 1, L[i], i == 0u ? WC_WHT : i < 3u ? WC_GRN : WC_GRN2);
        wf_foot2("PRESETS", "MEMORY", "OCT-", "BACK");
    }
}
static UI_NOINLINE void wfm_led_draw(void) { wf_dialog("WINAMPFM KEYBOARD LED", wfb_led, 1u + led_fx * 131u + led_sp * 17u + led_fd * 1009u + led_hold * 4001u); }
static UI_NOINLINE void wfm_importer_draw(void)
{
    uint32_t sig = 3u + imp_sel * 7u + imp_prog() * 977u + (imp_t0 != 0u) * 13u, i;
    for (i = 0; i < IMP_N; i++) sig = sig * 31u + (uint32_t)imp_used(i);
    wf_dialog("WINAMPFM IMPORTER", wfb_imp, sig);
}
static UI_NOINLINE void wfm_about_draw(void) { wf_dialog(about_pg ? "WINAMPFM ABOUT - CREDITS" : "WINAMPFM ABOUT - MEMORY", wfb_about, 5u + about_pg); }

/* FX held + REC: every effect of the selected track back to its default (the sends, the slicer). (REC held alone still clears the track) */
static UI_NOINLINE int wfm_fx_reset(uint32_t pressed)
{
    static const uint8_t ID[8] = {P_DIST, P_CHOR, P_DLY, P_REV, P_SLCR, P_SLPAT, P_SLRATE, P_SLDEPTH};
    uint32_t i;
    if (!((pressed >> panel.btn[B_REC]) & 1u) || !((fm1_in.buttons >> panel.btn[B_FX]) & 1u)) return 0;
    for (i = 0; i < 8u; i++) TSEL->p[ID[i]] = TP[ID[i]].def;
    ui_say("FX RESET: TRACK ", is_drum(TSEL) ? "D" : song.sel == 0u ? "1" : song.sel == 1u ? "2" : "3");
    return 1;
}

/* ---------------------------------------------------------------- the opening sound: in C, as the classic Winamp intro (a low C that swells, a bright
 * C-E-G at the end), here played by an FM bell (PLUGG BELL): the low C, a C-E-G-C arpeggio going up, a shining C major chord on top, a high E sparkle.
 * main.c's boot_keys plays it on the device; the simulator plays it at start (wfm_jingle_start / _tick, not blocking) */
static const struct { uint16_t on, off; uint8_t note; } WFM_JINGLE[] = {
    {0, 2700, 48}, {380, 900, 60}, {560, 1060, 64}, {740, 1220, 67}, {920, 1500, 72},
    {1250, 2950, 76}, {1250, 2950, 79}, {1250, 2950, 84}, {1650, 2950, 88},
};
#define WFM_JINGLE_N (sizeof WFM_JINGLE / sizeof WFM_JINGLE[0])
#define WFM_JINGLE_MS 3300u
static UI_NOINLINE uint32_t wfm_bell(void)                            /* the FM engine's PLUGG BELL (by name: the table may change order) */
{
    const engine_t *e = ENGINES[1];
    uint32_t k;
    for (k = 0; k < e->npresets; k++) if (str_eq(e->presets[k].name, "PLUGG BELL")) return k;
    return 0;
}
static track_t wj_save;
static int16_t wj_g[G_COUNT];
static uint32_t wj_t0, wj_on, wj_off;
static uint8_t wj_run;
static UI_NOINLINE void wfm_jingle_voice(void)                        /* track 1 becomes the bell (the caller keeps the old one) */
{
    uint32_t i;
    for (i = 0; i < (uint32_t)G_COUNT; i++) if (i != G_SLOT && i != G_DRCH && i != G_SYNC) song.g[i] = GP[i].def;
    song.g[G_BPM] = 80;
    set_engine_of(&trk[0], 1u);
    apply_preset_to(&trk[0], wfm_bell());
    trk[0].engine = trk[0].eng_req;
    trk[0].p[P_LEVEL] = TP[P_LEVEL].def, trk[0].p[P_PAN] = 0, trk[0].p[P_MUTE] = 0, trk[0].p[P_REV] = 40, trk[0].p[P_DLY] = 25;
}
static UI_NOINLINE void wfm_jingle_start(void)
{
    if (boot_mute) return;
    wj_save = trk[0];
    memcpy(wj_g, song.g, sizeof wj_g);
    wfm_jingle_voice();
    wj_t0 = fm1_ms | 1u, wj_on = wj_off = 0, wj_run = 1;
}
static UI_NOINLINE void wfm_jingle_tick(void)
{
    uint32_t t, i;
    if (!wj_run) return;
    t = fm1_ms - wj_t0;
    for (i = 0; i < WFM_JINGLE_N; i++) {
        if (!((wj_on >> i) & 1u) && t >= WFM_JINGLE[i].on) wj_on |= 1u << i, trk_note_on(&trk[0], WFM_JINGLE[i].note, 100);
        if (!((wj_off >> i) & 1u) && t >= WFM_JINGLE[i].off) wj_off |= 1u << i, trk_note_off(&trk[0], WFM_JINGLE[i].note);
    }
    if (t >= WFM_JINGLE_MS) {                                          /* the track and the globals as they were */
        trk[0] = wj_save;
        memcpy(song.g, wj_g, sizeof wj_g);
        wj_run = 0;
    }
}

/* ---------------------------------------------------------------- TAPES: the sounds as cassettes in a carousel, PS1 style (low resolution, affine
 * texture, dithered shading). ui_menu.c (ui.menu 9, the Quest's sound select) does the work: PRESETS rolls (the sound is put on the track and, when
 * the knob rests, it plays alone), PLAY plays it again, SEL keeps it (copy), ALGORITHM picks the track, HOME leaves. Here: the drawing and the copy. */
#define TT_W 80
#define TT_H 50
static uint8_t wt_tex[TT_H][TT_W];
static uint16_t wt_pal[21];
static int32_t wt_cur;                       /* the carousel, 1/256 tape, chasing sx_tgt */
static UI_NOINLINE uint32_t wt_style(uint32_t n)                      /* the cover: 0..7 the bank's kinds, 8 user, 9 drum kit, 10 the Brazilian ones */
{
    if (is_drum(TSEL)) return 9u;
    if (n >= sx_np) return 10u;
    return n < NBANK ? BANK[n].kind & 7u : 8u;
}
static UI_NOINLINE const char *wt_kind(uint32_t n)
{
    if (is_drum(TSEL)) return DRUM_KIT_STYLES[n % DRUM_KITS];
    if (n >= sx_np) return "BRASIL";
    return n < NBANK ? BANK_KIND[BANK[n].kind] : "USER";
}
static UI_NOINLINE void wt_tpx(int32_t x, int32_t y, uint8_t c) { if ((uint32_t)x < TT_W && (uint32_t)y < TT_H) wt_tex[y][x] = c; }
static UI_NOINLINE void wt_trect(int32_t x, int32_t y, int32_t w, int32_t h, uint8_t c) { int32_t i, j; for (j = y; j < y + h; j++) for (i = x; i < x + w; i++) wt_tpx(i, j, c); }
static UI_NOINLINE int32_t wt_ttextf(int32_t x, int32_t y, const char *s, uint8_t c, int32_t xmax, int big)   /* big: the 5 x 7 face */
{
    for (; *s && x < xmax; s++) {
        uint32_t ch = (uint8_t)*s, w, r, b, h = big ? WFM_H : WFS_H;
        const uint8_t *rows;
        if (ch >= 'a' && ch <= 'z') ch -= 32;
        if (ch < 32 || ch > 95) ch = '?';
        w = big ? WFM_W[ch - 32] : WFS_W[ch - 32];
        rows = big ? WFM_ROWS[ch - 32] : WFS_ROWS[ch - 32];
        for (r = 0; r < h; r++) for (b = 0; b < w; b++) if ((rows[r] >> (7 - b)) & 1u && x + (int32_t)b < xmax) wt_tpx(x + (int32_t)b, y + (int32_t)r, c);
        x += (int32_t)w + 1;
    }
    return x;
}
static UI_NOINLINE void wt_ttext(int32_t x, int32_t y, const char *s, uint8_t c, int32_t xmax) { wt_ttextf(x, y, s, c, xmax, 0); }
/* the tapes, after real ones (the user's references: 31 of them). Each kind of sound has up to four models and the list takes them in turn; on each
 * tape the art, the logos and the small print stay, and ONE big English line is the sound's name (handwriting and Japanese stay as they are, the
 * name goes beside them). palette: 0 shell 1 shell dark 2 shell light 3 label 4 small print 5 the name 6 second label colour 7 window frame
 * 8 reel 9 reel dark 10 tape 11..16 accents 17 screw 18 white 19 black 20 see-through (the shell of a clear tape: drawn every other pixel) */
enum { WM_CLASSIC, WM_ART, WM_COMPUTER, WM_AUDITION, WM_SONYB, WM_FUTURE, WM_TOLAKU, WM_DOODLE, WM_PINK, WM_STAR,
       WM_FISHER, WM_POLIMER, WM_AGFASC, WM_TEAC, WM_AGFAFC, WM_AGFALNS, WM_BASFR, WM_BASFO, WM_HAPPY, WM_BARBIE,
       WM_HOLO, WM_FEB, WM_BRITPOP, WM_EMO, WM_HORTEN, WM_EXCL, WM_SKC, WM_OIL, WM_FLORAL, WM_AGFAY, WM_CLASSICBR,
       WM_SCOTCH, WM_SEIKO, WM_TDKD, WM_KITTYP, WM_KITTYB, WM_SPANKP, WM_SPANKG, WM_AGFAC90, WM_APPLEPIE, WM_3M, WM_DEMAG, WM_N };
#define C3(r, g, b) RGB(0x##r, 0x##g, 0x##b)
static const struct { uint16_t shell, label, label2, ink, name, a[6]; } WT_M[WM_N] = {
    [WM_CLASSIC] = {C3(e6, dc, b8), C3(f6, f4, ee), C3(40, 46, 4a), C3(60, 60, 60), C3(e8, e8, ec), {C3(60, b8, 40), C3(f8, c0, 30), C3(f0, 80, 28), C3(e0, 40, 38), C3(90, 40, c0), C3(30, 70, d0)}},
    [WM_CLASSICBR] = {C3(e8, e0, a0), C3(f8, f8, f0), C3(10, 60, 30), C3(10, 60, 30), C3(f8, d8, 20), {C3(10, 90, 40), C3(10, 90, 40), C3(f8, d8, 20), C3(f8, d8, 20), C3(20, 50, b0), C3(20, 50, b0)}},
    [WM_ART] = {C3(c8, d0, d8), C3(f8, f8, f8), C3(10, 10, 14), C3(30, a0, 80), C3(20, 80, 60), {C3(40, 50, d8), C3(f8, e0, c8), C3(10, 10, 14), C3(d8, e8, f8), C3(f0, 90, 98), C3(20, 80, 60)}},
    [WM_COMPUTER] = {C3(f4, f2, ea), C3(f4, f2, ea), C3(20, 20, 20), C3(20, 20, 20), C3(20, 20, 20), {C3(60, b8, 40), C3(f8, c8, 30), C3(f0, 80, 28), C3(e0, 40, 38), C3(90, 40, a0), C3(30, 80, d0)}},
    [WM_AUDITION] = {C3(1c, 1c, 1c), C3(f0, f0, e8), C3(d8, 28, 20), C3(30, 30, 30), C3(f8, f8, f8), {C3(20, 40, a0), C3(d0, 30, 30), C3(b0, b0, b0), 0, 0, 0}},
    [WM_SONYB] = {C3(c0, c8, d0), C3(f0, ec, dc), C3(20, 90, 50), C3(30, 30, 30), C3(18, 18, 18), {C3(20, 90, 50), C3(30, 30, 30), 0, 0, 0, 0}},
    [WM_FUTURE] = {C3(b8, 18, 10), C3(f4, f2, ee), C3(b8, 18, 10), C3(20, 20, 20), C3(20, 20, 20), {C3(30, f0, 80), C3(d0, 30, 30), C3(e8, 50, 40), 0, 0, 0}},
    [WM_TOLAKU] = {C3(d8, 20, 20), C3(f4, f4, f0), C3(40, b8, e8), C3(20, 20, 20), C3(d8, 20, 30), {C3(20, 90, e0), C3(f8, f8, f8), C3(d8, 20, 30), 0, 0, 0}},
    [WM_DOODLE] = {C3(2c, 2c, 30), C3(f8, c8, 40), C3(90, c8, e8), C3(30, 30, 30), C3(e0, 48, 30), {C3(f8, f8, f8), C3(90, 70, c8), C3(f0, 70, 30), C3(e0, 48, 30), 0, 0}},
    [WM_PINK] = {C3(f4, c0, c8), C3(f8, f8, f8), C3(60, b8, b8), C3(20, 20, 20), C3(18, 18, 18), {C3(e0, 20, 20), C3(f8, f8, f8), C3(e0, a0, a8), 0, 0, 0}},
    [WM_STAR] = {C3(30, 30, 34), C3(c8, 18, 30), C3(c8, 18, 30), C3(f0, c8, 30), C3(f8, d0, 30), {C3(20, 30, 90), C3(10, 18, 50), C3(f0, c8, 30), C3(40, 60, d0), 0, 0}},
    [WM_FISHER] = {C3(f8, d8, 20), C3(f0, e8, d0), C3(e0, 30, 40), C3(30, 30, 30), C3(30, 28, 20), {C3(30, 70, c0), C3(40, a8, 50), C3(f0, 90, 30), C3(f8, f8, f8), 0, 0}},
    [WM_POLIMER] = {C3(f0, 78, 18), C3(30, b0, d0), C3(30, b0, d0), C3(f8, f8, f8), C3(10, 10, 20), {C3(f8, d0, 30), C3(f0, 80, 30), C3(e0, 40, 60), C3(90, 50, c0), 0, 0}},
    [WM_AGFASC] = {C3(30, 80, d0), C3(50, c8, d8), C3(50, c8, d8), C3(10, 20, 70), C3(10, 20, 70), {C3(10, 20, 70), C3(e0, 30, 30), C3(f0, e0, c0), C3(f8, f8, f8), 0, 0}},
    [WM_TEAC] = {C3(b8, d0, f0), C3(b8, d0, f0), C3(48, 20, 18), C3(30, 30, 40), C3(f8, f8, f8), {C3(48, 20, 18), C3(a0, 70, c0), 0, 0, 0, 0}},
    [WM_AGFAFC] = {C3(e0, 28, 28), C3(f0, f0, ea), C3(e0, 28, 28), C3(20, 20, 20), C3(20, 20, 20), {C3(10, 20, 70), C3(e0, 30, 30), C3(f8, f8, f8), C3(10, 10, 10), 0, 0}},
    [WM_AGFALNS] = {C3(f8, 70, 20), C3(f8, 70, 20), C3(f8, 70, 20), C3(20, 20, 20), C3(20, 20, 20), {C3(10, 20, 70), C3(e0, 30, 30), C3(f8, f8, f8), 0, 0, 0}},
    [WM_BASFR] = {C3(20, 20, 22), C3(f0, 40, 20), C3(10, 10, 10), C3(10, 10, 10), C3(f8, f8, f8), {C3(f0, 40, 20), C3(f8, f8, f8), 0, 0, 0, 0}},
    [WM_BASFO] = {C3(20, 20, 22), C3(f8, f8, f4), C3(f8, a0, 30), C3(20, 20, 20), C3(10, 10, 10), {C3(e0, 30, 30), C3(f8, f8, f8), C3(b0, b0, b8), 0, 0, 0}},
    [WM_HAPPY] = {C3(f0, 50, 18), C3(40, b8, d0), C3(40, b8, d0), C3(10, 10, 20), C3(f8, f8, f8), {C3(70, 30, a0), C3(f8, f8, f8), 0, 0, 0, 0}},
    [WM_BARBIE] = {C3(f0, 40, b0), C3(e0, f0, 40), C3(f0, 70, 80), C3(10, 10, 20), C3(e0, 20, 90), {C3(20, 30, 90), C3(f0, 70, 80), C3(a0, 90, b8), 0, 0, 0}},
    [WM_HOLO] = {C3(b0, 88, e8), C3(d8, e0, f0), C3(d8, e0, f0), C3(20, 20, 20), C3(10, 10, 10), {C3(f0, b0, d0), C3(a0, e0, f0), C3(f8, f0, a0), C3(c0, b0, f0), 0, 0}},
    [WM_FEB] = {C3(f4, b8, c4), C3(f8, d8, dc), C3(c8, 18, 30), C3(f8, d8, dc), C3(c8, 18, 30), {C3(d8, 20, 40), C3(f0, a0, b0), 0, 0, 0, 0}},
    [WM_BRITPOP] = {C3(90, b0, d0), C3(f4, f0, e8), C3(70, a8, d8), C3(20, 20, 20), C3(d0, 40, 40), {C3(d0, 40, 40), C3(20, 20, 20), C3(70, a8, d8), 0, 0, 0}},
    [WM_EMO] = {C3(20, 20, 20), C3(f0, ec, e4), C3(d0, 40, 40), C3(20, 20, 20), C3(20, 20, 20), {C3(d0, 40, 40), C3(f0, ec, e4), C3(20, 20, 20), 0, 0, 0}},
    [WM_HORTEN] = {C3(c8, cc, c4), C3(f0, 50, 90), C3(f0, 50, 90), C3(20, 20, 20), C3(f8, f8, f8), {C3(10, 10, 10), C3(30, 30, 90), 0, 0, 0, 0}},
    [WM_EXCL] = {C3(e8, 28, 20), C3(f8, f4, f0), C3(e8, 28, 20), C3(20, 20, 20), C3(f8, d8, 20), {C3(f8, d8, 20), C3(c0, 30, 30), 0, 0, 0, 0}},
    [WM_SKC] = {C3(d0, d8, e0), C3(d0, d8, e0), C3(20, 20, 28), C3(20, 20, 30), C3(f8, f8, f8), {C3(f0, 30, 70), C3(f8, c0, 30), C3(30, a0, b0), C3(20, 20, 28), C3(40, 60, 90), 0}},
    [WM_OIL] = {C3(d8, d8, e0), C3(f0, b0, c0), C3(f0, b0, c0), C3(30, 30, 30), C3(f8, f8, f8), {C3(f0, a0, 60), C3(a0, d0, f0), C3(c0, 90, d8), C3(f8, f0, 90), C3(60, 50, 70), 0}},
    [WM_FLORAL] = {C3(d8, d8, e0), C3(20, 28, 20), C3(20, 28, 20), C3(f8, f8, f8), C3(f8, f8, f8), {C3(e0, 70, 60), C3(f0, b0, a0), C3(40, 80, 40), C3(f8, f8, f0), 0, 0}},
    [WM_AGFAY] = {C3(f8, c0, 20), C3(f8, c0, 20), C3(f8, c0, 20), C3(20, 20, 20), C3(20, 20, 20), {C3(10, 20, 70), C3(e0, 30, 30), C3(f8, f8, f8), 0, 0, 0}},
    [WM_SCOTCH] = {C3(80, b8, f0), C3(a8, d0, d8), C3(70, 48, 30), C3(10, 10, 20), C3(10, 10, 20), {C3(70, 48, 30), C3(f0, e0, a0), 0, 0, 0, 0}},
    [WM_SEIKO] = {C3(c8, c8, c4), C3(f4, f4, f0), C3(30, a8, e0), C3(20, 20, 20), C3(f8, f8, f8), {C3(30, a8, e0), C3(10, 10, 20), 0, 0, 0, 0}},
    [WM_TDKD] = {C3(50, 48, d8), C3(50, 48, d8), C3(10, 10, 14), C3(f8, f8, f8), C3(e8, d8, 90), {C3(10, 10, 14), C3(70, 90, f0), C3(d8, c0, 60), C3(f8, f8, f8), 0, 0}},
    [WM_KITTYP] = {C3(f8, c8, d4), C3(f8, d8, e0), C3(30, 18, 20), C3(e0, 30, 70), C3(e0, 30, 70), {C3(e0, 30, 70), C3(30, 18, 20), C3(f8, f8, f8), 0, 0, 0}},
    [WM_KITTYB] = {C3(a8, c8, f0), C3(b8, d4, f8), C3(20, 30, 40), C3(30, 50, c0), C3(30, 50, c0), {C3(30, 50, c0), C3(20, 30, 40), C3(f8, f8, f8), 0, 0, 0}},
    [WM_SPANKP] = {C3(f8, b0, c0), C3(f8, b0, c0), C3(20, 18, 20), C3(e0, 50, 70), C3(e0, 50, 70), {C3(60, d0, b0), C3(f0, 60, 80), C3(60, a0, d0), C3(20, 18, 20), 0, 0}},
    [WM_SPANKG] = {C3(60, f0, 60), C3(60, f0, 60), C3(20, 18, 20), C3(e0, 40, 90), C3(e0, 40, 90), {C3(60, d0, b0), C3(f0, 50, 90), C3(60, a0, e0), C3(20, 18, 20), 0, 0}},
    [WM_AGFAC90] = {C3(f8, b0, 20), C3(f8, b0, 20), C3(f8, b0, 20), C3(20, 20, 20), C3(f8, f8, f8), {C3(10, 20, 70), C3(e0, 30, 30), C3(f8, f8, f8), 0, 0, 0}},
    [WM_APPLEPIE] = {C3(22, 22, 22), C3(f8, b8, 10), C3(f8, b8, 10), C3(20, 20, 20), C3(20, 20, 20), {C3(20, 20, 20), C3(a0, a0, a0), C3(f8, f8, f8), 0, 0, 0}},
    [WM_3M] = {C3(f8, 50, 20), C3(e0, 28, 28), C3(f8, f0, e8), C3(f8, f0, e8), C3(f8, f0, e8), {C3(f8, f0, e8), 0, 0, 0, 0, 0}},
    [WM_DEMAG] = {C3(d0, d8, e0), C3(e8, e4, d8), C3(30, 28, 28), C3(30, 28, 28), C3(20, 20, 20), {C3(b0, 78, 40), C3(30, 30, 30), C3(90, 80, c0), C3(c0, 60, 40), C3(c0, a0, 40), C3(e0, 40, 40)}},
};
static const uint8_t WT_CAT[11][6] = {                     /* the models of each kind of sound, taken in turn down the list (0 ends a shorter list) */
    {WM_STAR, WM_BASFR, WM_EMO, WM_AUDITION, WM_APPLEPIE, 0}, {WM_COMPUTER, WM_FISHER, WM_TEAC, WM_SEIKO, WM_SCOTCH, 0},
    {WM_BASFO, WM_AGFALNS, WM_HAPPY, WM_AGFAC90, 0, 0}, {WM_PINK, WM_HOLO, WM_OIL, WM_FLORAL, WM_KITTYP, WM_KITTYB},
    {WM_ART, WM_BRITPOP, WM_SKC, WM_TDKD, WM_SPANKG, 0}, {WM_SONYB, WM_POLIMER, WM_AGFAY, WM_SPANKP, 0, 0},
    {WM_TOLAKU, WM_EXCL, WM_HORTEN, WM_3M, 0, 0}, {WM_FUTURE, WM_AGFASC, WM_AGFAFC, WM_DEMAG, 0, 0}, {WM_CLASSIC, 0, 0, 0, 0, 0},
    {WM_DOODLE, WM_BARBIE, WM_FEB, WM_KITTYP, 0, 0}, {WM_CLASSICBR, 0, 0, 0, 0, 0}};
static const uint8_t WT_ORDER[WM_N] = {   /* every model once, shuffled: the tapes take them in turn down the whole list (no two the same side by side) */
    WM_TEAC, WM_3M, WM_COMPUTER, WM_AGFAY, WM_TDKD, WM_EXCL, WM_TOLAKU, WM_BASFR, WM_BASFO, WM_BARBIE, WM_HOLO, WM_POLIMER, WM_CLASSIC, WM_SEIKO, WM_SKC, WM_SPANKP, WM_AGFAC90, WM_KITTYB, WM_FISHER, WM_DOODLE, WM_SCOTCH, WM_HAPPY, WM_CLASSICBR, WM_BRITPOP, WM_ART, WM_FUTURE, WM_STAR, WM_EMO, WM_KITTYP, WM_OIL, WM_AGFASC, WM_AUDITION, WM_SPANKG, WM_DEMAG, WM_FLORAL, WM_FEB, WM_HORTEN, WM_APPLEPIE, WM_PINK, WM_AGFAFC, WM_SONYB, WM_AGFALNS};
static UI_NOINLINE uint32_t wt_model(uint32_t n) { return WT_ORDER[n % WM_N]; }   /* (the kind of sound still says itself in the little label on top) */
static UI_NOINLINE int wt_clear(uint32_t md)                           /* the see-through shells */
{
    return md == WM_FUTURE || md == WM_TEAC || md == WM_BRITPOP || md == WM_SKC || md == WM_OIL || md == WM_FLORAL || md == WM_SCOTCH ||
           md == WM_TDKD || md == WM_KITTYP || md == WM_KITTYB || md == WM_SPANKP || md == WM_SPANKG || md == WM_3M || md == WM_DEMAG || md == WM_SONYB || md == WM_ART;
}
static UI_NOINLINE void wt_tcirc(int32_t cx, int32_t cy, int32_t r, uint8_t c) { int32_t x, y; for (y = -r; y <= r; y++) for (x = -r; x <= r; x++) if (x * x + y * y <= r * r + r / 2) wt_tpx(cx + x, cy + y, c); }
static UI_NOINLINE void wt_tring(int32_t cx, int32_t cy, int32_t r, int32_t t, uint8_t c) { int32_t x, y; for (y = -r; y <= r; y++) for (x = -r; x <= r; x++) { int32_t d = x * x + y * y; if (d <= r * r + r / 2 && d >= (r - t) * (r - t)) wt_tpx(cx + x, cy + y, c); } }
static UI_NOINLINE void wt_screw(int32_t x, int32_t y) { wt_tcirc(x, y, 1, 17); wt_tpx(x, y, 1); }
static UI_NOINLINE void wt_reel(int32_t cx, int32_t cy, uint32_t spin)  /* a white reel, six teeth inside; spin turns it (12 steps a turn) */
{
    static const int8_t D[12][2] = {{0, -3}, {2, -3}, {3, -2}, {3, 0}, {3, 2}, {2, 3}, {0, 3}, {-2, 3}, {-3, 2}, {-3, 0}, {-3, -2}, {-2, -3}};
    uint32_t k;
    wt_tcirc(cx, cy, 5, 8);
    wt_tcirc(cx, cy, 3, 9);
    for (k = 0; k < 6u; k++) { uint32_t d = (spin + k * 2u) % 12u; wt_tpx(cx + D[d][0], cy + D[d][1], 8); wt_tpx(cx + D[d][0] * 2 / 3, cy + D[d][1] * 2 / 3, 8); }
    wt_tpx(cx, cy, 8);
}
static UI_NOINLINE int32_t wt_tw(const char *s, int big) { int32_t w = 0; for (; *s; s++) { uint32_t c = (uint8_t)*s; if (c >= 'a' && c <= 'z') c -= 32; if (c < 32 || c > 95) c = '?'; w += (big ? WFM_W[c - 32] : WFS_W[c - 32]) + 1; } return w ? w - 1 : 0; }
static UI_NOINLINE void wt_tc(int32_t cx, int32_t y, const char *s, uint8_t c, int big) { wt_ttextf(cx - wt_tw(s, big) / 2, y, s, c, TT_W - 2, big); }
static UI_NOINLINE void wt_agfa(int32_t x, int32_t y)                 /* the blue square, a red diamond, rings in a corner */
{
    int32_t i, j;
    wt_trect(x, y, 11, 11, 11);
    for (j = 0; j < 4; j++) for (i = -j; i <= j; i++) wt_tpx(x + 7 + i, y + 1 + j, 12), wt_tpx(x + 7 + i, y + 7 - j, 12);
    for (j = 2; j <= 8; j += 3) for (i = 0; i <= j; i++) wt_tpx(x + i, y + 10 - (int32_t)wf_isqrt((uint32_t)(j * j - i * i)), 18);
}
static UI_NOINLINE void wt_basf(int32_t x, int32_t y) { wt_trect(x, y, 7, 7, 18); wt_tring(x + 3, y + 3, 3, 1, 11); wt_tpx(x + 3, y + 3, 11); }
static UI_NOINLINE void wt_lines(int32_t x0, int32_t x1, int32_t y, int32_t n, int32_t gap, uint8_t c) { int32_t k; for (k = 0; k < n; k++) wt_trect(x0, y + k * gap, x1 - x0, 1, c); }
static UI_NOINLINE void wt_make(uint32_t n, int back, uint32_t spin)  /* the tape of item n into wt_tex / wt_pal (back: its other side) */
{
    uint32_t md = wt_model(n), i;
    int32_t x, y, clear = 0;                                           /* clear: a see-through shell (the reels' window has no frame of its own) */
    char nm[16];
    const char *kd = wt_kind(n);
    wt_pal[0] = WT_M[md].shell, wt_pal[1] = wf_dim(WT_M[md].shell, 150), wt_pal[2] = wf_dim(WT_M[md].shell, 255);
    wt_pal[3] = WT_M[md].label, wt_pal[4] = WT_M[md].ink, wt_pal[5] = WT_M[md].name, wt_pal[6] = WT_M[md].label2;
    wt_pal[7] = wf_dim(WT_M[md].shell, 200), wt_pal[8] = RGB(0xf8, 0xf8, 0xf4), wt_pal[9] = RGB(0x58, 0x50, 0x48), wt_pal[10] = RGB(0x40, 0x24, 0x18);
    for (i = 0; i < 6u; i++) wt_pal[11 + i] = WT_M[md].a[i];
    wt_pal[17] = RGB(0x90, 0x98, 0xa0), wt_pal[18] = RGB(0xf8, 0xf8, 0xf8), wt_pal[19] = RGB(0x10, 0x10, 0x10), wt_pal[20] = WT_M[md].shell;
    if (WT_M[md].shell < RGB(0x40, 0x40, 0x40)) wt_pal[2] = RGB(0x58, 0x58, 0x60), wt_pal[1] = RGB(0x0c, 0x0c, 0x0e);
    memset(wt_tex, wt_clear(md) ? 20 : 0, sizeof wt_tex);
    wt_trect(0, 0, TT_W, 1, 2), wt_trect(0, 0, 1, TT_H, 2), wt_trect(0, TT_H - 1, TT_W, 1, 1), wt_trect(TT_W - 1, 0, 1, TT_H, 1);
    wt_tpx(0, 0, 1), wt_tpx(TT_W - 1, 0, 1), wt_tpx(0, TT_H - 1, 1), wt_tpx(TT_W - 1, TT_H - 1, 1);
    for (y = 2; y < TT_H - 2; y += 3) wt_trect(1, y, 2, 1, 1), wt_trect(TT_W - 3, y, 2, 1, 1);
    sx_name(nm, n, sizeof nm);
    if (back) { wt_trect(6, 4, TT_W - 12, 30, 3); wt_ttext(9, 6, "B SIDE", 4, TT_W - 8); md = WM_N; }
    switch (md) {
    case WM_CLASSIC: case WM_CLASSICBR:                                /* A SIDE, "to my love" in pen, the rainbow; TRUE SOUND -> the name */
        wt_trect(6, 4, TT_W - 12, 30, 3);
        wt_ttext(8, 6, "A SIDE", 4, 30);
        wt_ttext(36, 5, "TO MY LOVE", 6, TT_W - 7), wt_trect(34, 11, TT_W - 41, 1, 7);
        for (i = 0; i < 6u; i++) wt_trect(6, 14 + (int32_t)i * 2, TT_W - 12, 2, (uint8_t)(11u + i));
        wt_trect(6, 26, TT_W - 12, 8, 6);
        wt_tc(40, 27, nm, 5, 1);
        break;
    case WM_ART:                                                       /* HF-S120 band; anime eyes round the reels; the big HF-S120 -> the name */
        wt_trect(5, 3, TT_W - 10, 7, 6);
        wt_ttext(7, 4, "HF-S120", 4, 40), wt_ttext(TT_W - 26, 4, "SONY", 18, TT_W - 6);
        wt_trect(5, 10, TT_W - 10, 18, 12);
        for (x = 5; x < TT_W - 5; x++) for (y = 10; y < 15 + ((x * 7) % 5); y++) wt_tpx(x, y, 11);
        wt_tring(29, 21, 8, 2, 13), wt_tring(50, 21, 8, 2, 13), wt_trect(37, 20, 6, 1, 13), wt_tcirc(40, 26, 1, 15);
        wt_trect(5, 29, TT_W - 10, 6, 3);
        wt_ttext(7, 30, "SONY", 19, 26);
        wt_ttextf(27, 29, nm, 5, TT_W - 6, 1);
        break;
    case WM_COMPUTER: {                                                /* the striped apple, Lemonade, 16K; "apple computer inc." -> the name */
        int32_t k;
        wt_trect(5, 3, TT_W - 10, 32, 3);
        for (k = 0; k < 6; k++) { int32_t yy = 9 + k * 3, w = k < 1 ? 7 : k < 5 ? 10 : 8; wt_trect(7 + (10 - w) / 2, yy, w, 3, (uint8_t)(11 + k)); }
        wt_tpx(12, 7, 11), wt_tpx(13, 6, 11), wt_tpx(13, 7, 11);
        wt_ttext(22, 4, "LEMONADE", 4, 60), wt_ttext(22, 9, "(C) 1979", 4, 60);
        wt_ttext(61, 17, "LOAD", 4, TT_W - 3), wt_ttext(62, 23, "16K", 4, TT_W - 3);
        wt_ttext(7, 29, "600-2026", 4, 30);
        wt_ttextf(33, 28, nm, 5, TT_W - 5, 1);
        break;
    }
    case WM_AUDITION:                                                  /* INDEX lines in pen, a red band, SIDE 1, 40 MIN; Audition -> the name */
        wt_trect(5, 3, TT_W - 10, 12, 3);
        wt_lines(5, TT_W - 5, 8, 2, 5, 13);
        wt_ttext(7, 4, "INDEX", 4, 30), wt_ttext(32, 4, "M*A*S*H", 11, TT_W - 6);
        wt_trect(5, 15, TT_W - 10, 20, 6);
        wt_trect(7, 18, 11, 9, 3), wt_ttext(8, 19, "SIDE", 4, 18), wt_ttext(11, 24, "1", 4, 18);
        wt_ttextf(61, 20, "40", 18, TT_W - 4, 1), wt_ttext(61, 16, "MIN", 18, TT_W - 4);
        wt_ttextf(7, 28, nm, 5, 52, 1), wt_ttext(52, 29, "KOREA", 18, TT_W - 5);
        break;
    case WM_SONYB:                                                     /* the green frame, B, "'80s rock" in marker, LOW-NOISE C90; SONY -> the name */
        wt_trect(5, 3, TT_W - 10, 32, 3);
        wt_trect(5, 3, 2, 32, 11), wt_trect(TT_W - 7, 3, 2, 32, 11), wt_trect(5, 33, TT_W - 10, 2, 11);
        wt_trect(8, 4, 8, 8, 11), wt_ttext(10, 6, "B", 3, 16);
        wt_ttextf(19, 5, "'80S ROCK", 19, TT_W - 8, 1), wt_trect(8, 13, TT_W - 16, 1, 11);
        wt_ttextf(8, 27, nm, 5, 50, 1);
        wt_ttext(52, 27, "LOW-", 4, TT_W - 8), wt_ttext(52, 32 - 3, "", 4, TT_W - 8), wt_ttext(TT_W - 20, 30, "C90", 11, TT_W - 8);
        break;
    case WM_FUTURE:                                                    /* red see-through, a price tag, Soundbite RECORD; FUTURE SOUNDS -> the name */
        wt_trect(7, 4, TT_W - 14, 9, 3);
        wt_ttextf(9, 5, nm, 5, TT_W - 9, 1);
        wt_trect(7, 28, TT_W - 14, 6, 3);
        wt_ttext(9, 29, "99", 4, 20), wt_ttext(34, 29, "SOUNDBITE REC", 4, TT_W - 9);
        wt_trect(60, 21, 13, 6, 11), wt_ttext(61, 22, "$9", 19, 73);
        for (y = 14; y < 27; y += 2) for (x = 6 + (y & 2); x < TT_W - 6; x += 7) wt_tpx(x, y, 2);
        clear = 1;
        break;
    case WM_TOLAKU:                                                    /* white label, a blue "kanji" block, a cyan band, SIDE 2; TOLAKU -> the name */
        wt_trect(5, 3, TT_W - 10, 13, 3);
        wt_ttextf(7, 4, nm, 5, 48, 1);
        wt_ttext(7, 11, "WISH YOU WERE HERE", 19, 48);
        for (x = 50; x < TT_W - 6; x += 6) wt_trect(x, 4, 5, 2, 11), wt_trect(x + 2, 4, 1, 10, 11), wt_trect(x, 9, 5, 1, 11), wt_trect(x, 13, 5, 1, 11);
        wt_trect(5, 16, TT_W - 10, 15, 6);
        wt_trect(7, 18, 5, 5, 13), wt_trect(12, 18, 5, 5, 11);
        wt_ttext(61, 17, "SIDE", 18, TT_W - 4), wt_ttextf(63, 23, "2", 18, TT_W - 4, 1);
        wt_trect(5, 31, TT_W - 10, 4, 3), wt_ttext(8, 31, "SUPERFERRO", 4, 50), wt_ttext(52, 31, "STEREO", 4, TT_W - 6);
        break;
    case WM_DOODLE:                                                    /* yellow over blue, INDEX, doodles, 2018; WE GO UP! -> the name */
        wt_trect(5, 3, TT_W - 10, 15, 3);
        wt_trect(5, 18, TT_W - 10, 17, 6);
        wt_ttext(7, 5, "INDEX", 4, 30);
        wt_ttextf(26, 4, nm, 5, TT_W - 7, 1), wt_trect(26, 12, TT_W - 33, 1, 14);
        wt_ttext(8, 29, "DREAM", 18, 34), wt_ttext(52, 29, "2018", 14, TT_W - 6);
        wt_tpx(68, 5, 11), wt_tpx(70, 7, 11), wt_tpx(69, 6, 11), wt_tpx(68, 7, 11), wt_tpx(70, 5, 11);
        wt_trect(8, 17, 1, 9, 4), wt_trect(9, 17, 6, 4, 12);
        wt_tcirc(66, 30, 3, 13);
        wt_ttext(62, 19, "A", 14, TT_W - 4);
        break;
    case WM_PINK: {                                                    /* the Japanese title in pen stays (as strokes); the name beside it; teal stripes, SIDE A */
        int32_t k;
        wt_trect(5, 3, TT_W - 10, 32, 3);
        for (k = 0; k < 6; k++) { x = 8 + k * 6; wt_trect(x, 5, 4, 1, 19), wt_trect(x + 1 + (k & 1), 5, 1, 6, 19), wt_trect(x, 9 - (k & 1), 4, 1, 19); }
        wt_ttextf(46, 5, nm, 5, TT_W - 6, 0);
        wt_ttextf(8, 12, nm, 5, TT_W - 8, 1);
        wt_trect(5, 20, TT_W - 10, 2, 6);
        wt_trect(5, 28, TT_W - 10, 7, 6);
        wt_ttext(61, 15, "SIDE", 4, TT_W - 4), wt_ttextf(64, 21, "A", 11, TT_W - 4, 1);
        wt_ttext(7, 29, "PONY CANYON MONO", 18, TT_W - 6);
        break;
    }
    case WM_STAR:                                                      /* red label, SIDE B, the track lists, XO, a blue figure; STARBOY -> the name */
        wt_trect(5, 3, TT_W - 10, 32, 3);
        wt_tc(40, 4, nm, 5, 1);
        wt_tc(40, 12, "SIDE B", 4, 0);
        wt_lines(7, 20, 5, 4, 2, 13), wt_lines(60, 73, 5, 4, 2, 13);
        wt_ttext(7, 20, "XO", 13, 20);
        for (y = 16; y < 35; y++) for (x = 62; x < TT_W - 5; x++) if ((x - 67) * (x - 67) + (y - 22) * (y - 22) < 22 || (y > 27 && x > 60 + (34 - y) / 2)) wt_tpx(x, y, 11);
        wt_lines(20, 58, 30, 3, 2, 13);
        break;
    case WM_FISHER:                                                    /* cream label, stripes, a red band with Fisher-Price; "Discover a World of Sounds" -> the name */
        wt_trect(5, 3, TT_W - 10, 32, 3);
        wt_tc(40, 4, nm, 5, 1);
        wt_ttext(16, 12, "(C)1987 FISHER-PRICE", 4, TT_W - 8);
        wt_trect(5, 17, TT_W - 10, 2, 11), wt_trect(5, 19, TT_W - 10, 2, 12), wt_trect(5, 21, TT_W - 10, 2, 13);
        wt_trect(5, 27, TT_W - 10, 8, 6);
        wt_ttext(8, 28, "SIDE", 19, 26), wt_tc(46, 28, "FISHER-PRICE", 18, 0);
        break;
    case WM_POLIMER:                                                   /* cyan label, the rainbow arc, 2x48, 90+6; POLIMER -> the name */
        wt_trect(5, 3, TT_W - 10, 32, 3);
        for (i = 0; i < 4u; i++) wt_tring(14, 15, 9 - (int32_t)i * 2, 2, (uint8_t)(11u + i));
        wt_trect(5, 15, 20, 10, 3);
        wt_ttextf(26, 4, nm, 5, TT_W - 5, 1);
        wt_trect(7, 17, 9, 9, 4), wt_ttext(8, 19, "2X48", 4, 17);
        wt_ttext(62, 17, "90+6", 4, TT_W - 3);
        wt_ttext(8, 29, "POLIMER", 4, 34), wt_ttext(34, 29, "LH-HIFI", 18, TT_W - 5);
        break;
    case WM_AGFASC:                                                    /* cyan label, the Agfa square, lines, a round sticker, 60, HIGH DYNAMIC; AGFA SUPER COLOR -> the name */
        wt_trect(5, 3, TT_W - 10, 32, 3);
        wt_agfa(7, 4);
        wt_lines(22, TT_W - 7, 7, 2, 5, 4);
        wt_tcirc(66, 21, 5, 13);
        wt_ttextf(7, 28, nm, 5, 52, 1);
        wt_ttext(52, 27, "HIGH", 4, 70), wt_ttext(52, 32 - 1, "DYN", 4, 70), wt_ttextf(64, 27, "60", 13, TT_W - 3, 1);
        break;
    case WM_TEAC:                                                      /* clear blue, a big brown spool, type I, a, mdx1, 100 min; TEAC -> the name */
        wt_tcirc(30, 21, 17, 11);
        wt_tc(40, 4, nm, 18, 1);
        wt_ttext(6, 21, "TYPE I", 4, 20), wt_ttextf(38, 28, "A", 18, 50, 1), wt_ttext(60, 21, "MDX1", 19, TT_W - 3);
        wt_ttext(8, 31, "SUPERFERRO", 18, 50), wt_ttext(56, 31, "100MIN", 4, TT_W - 4);
        clear = 1;
        break;
    case WM_AGFAFC:                                                    /* a white label with lines and a circle, the Agfa square, 60; AGFA FERRO COLOR -> the name */
        wt_trect(16, 4, TT_W - 22, 10, 3), wt_tring(10, 9, 5, 1, 19), wt_lines(17, TT_W - 7, 8, 2, 4, 11);
        wt_agfa(7, 16);
        wt_trect(62, 16, 11, 10, 18), wt_ttextf(63, 18, "60", 19, TT_W - 4, 1);
        wt_tc(40, 27, nm, 19, 1);
        wt_ttext(10, 35, "HIGH DYNAMIC", 19, TT_W - 8);
        break;
    case WM_AGFALNS:                                                   /* orange, the Agfa square, lines, a white dot, 60 2x30 MIN., LOW-NOISE SPECIAL; LNS 60 -> the name */
        wt_agfa(6, 4);
        wt_ttextf(20, 4, nm, 19, TT_W - 5, 1), wt_lines(20, TT_W - 7, 12, 1, 4, 18);
        wt_tcirc(66, 21, 5, 18);
        wt_ttext(7, 29, "60 2X30MIN", 19, 48), wt_ttext(48, 29, "LOW-NOISE", 19, TT_W - 4);
        wt_ttext(48, 34, "SPECIAL", 19, TT_W - 4);
        break;
    case WM_BASFR:                                                     /* red label with lines, Stereo / Mono / Dolby boxes, a black band, 90; BASF -> the name */
        wt_trect(5, 3, TT_W - 10, 32, 3);
        wt_lines(5, TT_W - 5, 7, 2, 4, 6), wt_ttext(7, 4, "1", 19, 12);
        wt_ttext(6, 16, "STEREO", 19, 22), wt_ttext(6, 21, "MONO", 19, 22), wt_ttext(6, 26 - 1, "DOLBY", 19, 22);
        wt_trect(5, 28, TT_W - 10, 7, 6);
        wt_basf(7, 28), wt_ttextf(16, 28, nm, 5, 60, 1), wt_ttextf(62, 28, "90", 11, TT_W - 4, 1);
        break;
    case WM_BASFO:                                                     /* white label (SIDE 2, STEREO, NO.), an orange band, 60, SM; BASF -> the name */
        wt_trect(5, 3, TT_W - 10, 12, 3);
        wt_ttext(7, 4, "SIDE 2  STEREO  NO.", 4, TT_W - 6), wt_lines(7, TT_W - 7, 10, 2, 2, 13);
        wt_trect(5, 15, TT_W - 10, 20, 6);
        wt_ttext(7, 20, "60", 19, 16), wt_trect(61, 17, 11, 9, 19), wt_ttext(62, 18, "SM", 6, 72);
        wt_basf(7, 28), wt_ttextf(16, 28, nm, 19, 60, 1), wt_ttext(62, 29, "LH SM", 19, TT_W - 3);
        break;
    case WM_HAPPY:                                                     /* cyan label, purple swirls, a flower, C90, 2x45 MIN. LOW-NOISE; Happy Tape -> the name */
        wt_trect(5, 3, TT_W - 10, 32, 3);
        for (x = 56; x < TT_W - 6; x++) for (y = 4; y < 34; y++) if (((x * 3 + y * 5) / 4) % 6 == 0) wt_tpx(x, y, 11);
        wt_tcirc(10, 7, 3, 11), wt_tpx(10, 7, 18);
        wt_ttext(16, 4, "MADE IN GERMANY", 18, 56);
        wt_ttextf(16, 9, nm, 5, 60, 1);
        wt_ttextf(7, 17, "C90", 18, 22, 0);
        wt_ttext(7, 29, "2X45 MIN LOW-NOISE", 18, 56);
        wt_tcirc(66, 21, 4, 18);
        break;
    case WM_BARBIE: {                                                  /* lime label, a pink diagonal band, a star, shapes, SIDE A; "Barbie AND THE BEAT" -> the name */
        wt_trect(5, 3, TT_W - 10, 32, 3);
        for (y = 3; y < 35; y++) for (x = 30 + y; x < 44 + y && x < TT_W - 5; x++) wt_tpx(x, y, 6);
        wt_ttext(16, 4, "THE BEAT", 19, 50), wt_ttext(55, 4, "SHINE", 19, TT_W - 6);
        wt_tcirc(9, 7, 2, 11), wt_trect(7, 19, 5, 5, 12), wt_trect(8, 27, 5, 4, 13);
        wt_ttext(62, 18, "SIDE", 13, TT_W - 4), wt_ttextf(64, 23, "A", 13, TT_W - 4, 1);
        wt_tc(40, 28, nm, 5, 1);
        break;
    }
    case WM_HOLO:                                                      /* a holographic label (bands of pastel), 7/13, Made in York, 14; the line in pen -> the name */
        wt_trect(5, 3, TT_W - 10, 32, 3);
        for (x = 5; x < TT_W - 5; x++) for (y = 3; y < 35; y++) wt_tpx(x, y, (uint8_t)(11u + (uint32_t)((x + y * 2 + (x * y) / 40) / 9) % 4u));
        wt_ttext(7, 4, "7/13", 19, 24);
        wt_ttextf(14, 5, nm, 19, TT_W - 6, 1), wt_trect(14, 12, TT_W - 22, 1, 19);
        wt_ttextf(62, 27, "14", 19, TT_W - 3, 1);
        wt_ttext(7, 29, "MADE IN YORK", 4, 50);
        break;
    case WM_FEB: {                                                     /* hearts round the title, a red band, V-21, MELOMANE, B SIDE; "Febuary Love" -> the name */
        static const uint8_t HT[5] = {0x6c, 0xfe, 0x7c, 0x38, 0x10};
        int32_t k;
        wt_trect(5, 3, TT_W - 10, 12, 3);
        for (k = 0; k < 5; k++) for (i = 0; i < 7u; i++) if ((HT[k] >> (7 - i)) & 1u) wt_tpx(7 + (int32_t)i, 5 + k, 11), wt_tpx(TT_W - 14 + (int32_t)i, 5 + k, 11);
        wt_tc(40, 5, nm, 5, 1);
        wt_trect(5, 15, TT_W - 10, 20, 6);
        wt_ttextf(7, 28, "V-21", 4, 30, 1), wt_tc(40, 30, "MELOMANE", 4, 0), wt_ttextf(64, 26, "B", 4, TT_W - 3, 1), wt_ttext(61, 33 - 1, "SIDE", 4, TT_W - 3);
        break;
    }
    case WM_BRITPOP:                                                   /* NOSTALGIA in a black box, A, blue and red curves, MADE IN GB; BRITPOP -> the name */
        wt_trect(5, 3, TT_W - 10, 32, 3);
        wt_trect(7, 5, 30, 7, 12), wt_ttext(9, 6, "NOSTALGIA", 18, 36);
        wt_ttextf(39, 5, nm, 5, TT_W - 6, 1);
        wt_ttextf(10, 18, "A", 13, 18, 1);
        wt_trect(5, 27, TT_W - 10, 3, 13), wt_trect(5, 31, TT_W - 30, 3, 11);
        wt_ttext(8, 22, "", 4, 20), wt_ttext(9, 33 - 1, "", 4, 20), wt_ttext(46, 31, "MADE IN GB", 4, TT_W - 6);
        clear = 1;
        break;
    case WM_EMO:                                                       /* NOSTALGIA, IN / OUT, a red and white chequer; EMO -> the name */
        wt_trect(5, 3, TT_W - 10, 12, 3);
        wt_trect(7, 5, 30, 7, 13), wt_ttext(9, 6, "NOSTALGIA", 12, 36);
        wt_ttextf(39, 5, nm, 5, TT_W - 14, 1);
        wt_ttext(TT_W - 13, 4, "IN", 4, TT_W - 5), wt_ttext(TT_W - 13, 9, "OUT", 4, TT_W - 3);
        wt_trect(5, 15, TT_W - 10, 20, 6);
        for (y = 27; y < 35; y++) for (x = 5; x < TT_W - 5; x++) if (((x / 3) + (y / 3)) & 1) wt_tpx(x, y, 12);
        break;
    case WM_HORTEN:                                                    /* a pink label, Horten, low noise, HI FI, Compact Cassette, C60; SUPERSOUND -> the name */
        wt_trect(5, 3, TT_W - 10, 32, 3);
        wt_ttext(9, 4, "HORTEN", 11, 36), wt_ttext(40, 4, "SHEILA-LOCOMOTION", 12, TT_W - 6);
        wt_tc(40, 9, nm, 5, 1);
        wt_ttext(6, 18, "LOW", 11, 20), wt_ttext(6, 23, "NOISE", 11, 22);
        wt_ttext(7, 29, "HI FI", 11, 26), wt_trect(28, 28, 26, 7, 11), wt_trect(29, 29, 24, 5, 3), wt_ttext(30, 29, "COMPACT", 11, 54), wt_ttextf(58, 28, "C60", 11, TT_W - 4, 1);
        break;
    case WM_EXCL:                                                      /* a white label with a line, a white dot, Made in Western Germany, C 60, hifi low noise; exclusiv -> the name */
        wt_ttextf(7, 4, nm, 5, 36, 1);
        wt_trect(38, 4, TT_W - 44, 10, 18), wt_trect(40, 9, TT_W - 48, 1, 12);
        wt_ttext(6, 17, "MADE IN", 18, 22), wt_ttext(6, 22, "W.GERMANY", 18, 22);
        wt_tcirc(66, 21, 5, 18);
        wt_trect(5, 28, TT_W - 10, 7, 18);
        wt_ttextf(7, 28, "C60", 19, 26, 1), wt_ttext(28, 29, "HIFI LOW NOISE", 12, TT_W - 6);
        break;
    case WM_SKC:                                                       /* clear: a black spool, a yellow triangle, teal dots, A, LX60, a pink band; SKC -> the name */
        wt_tcirc(52, 21, 16, 14);
        for (y = 3; y < 13; y++) wt_trect(44 + (y - 3), y, 20 - 2 * (y - 3), 1, 12);
        wt_tcirc(36, 13, 3, 13), wt_tcirc(70, 26, 3, 13), wt_tcirc(10, 22, 4, 12), wt_ttext(9, 20, "A", 19, 14);
        wt_trect(5, 28, 36, 6, 11), wt_ttextf(7, 28, nm, 18, 41, 1);
        wt_ttext(50, 29, "LX60", 18, TT_W - 6);
        clear = 1;
        break;
    case WM_OIL:                                                       /* a holographic oil label, swirls; the name on a strip */
        wt_trect(5, 3, TT_W - 10, 32, 3);
        for (x = 5; x < TT_W - 5; x++) for (y = 3; y < 35; y++) { uint32_t s2 = (uint32_t)(x * 2 + (WV_SIN[(uint32_t)(y * 3 + x) & 63u] + 128) / 16 + y); wt_tpx(x, y, (uint8_t)(11u + (s2 / 5u) % 5u)); }
        wt_trect(5, 28, TT_W - 10, 7, 15), wt_tc(40, 28, nm, 5, 1);
        clear = 1;
        break;
    case WM_FLORAL: {                                                  /* roses on black; the name on a strip */
        int32_t k;
        wt_trect(5, 3, TT_W - 10, 32, 3);
        for (k = 0; k < 14; k++) { int32_t fx = 8 + (k * 37) % 64, fy = 5 + (k * 23) % 28; wt_tcirc(fx, fy, 2 + k % 2, (uint8_t)(k % 3 == 0 ? 13 : k % 3 == 1 ? 11 : 12)); wt_tpx(fx, fy, 14); }
        wt_trect(5, 28, TT_W - 10, 7, 6), wt_tc(40, 28, nm, 5, 1);
        clear = 1;
        break;
    }
    case WM_AGFAY:                                                     /* yellow: lines, a white dot, the Agfa square, 60; AGFA LOW-NOISE -> the name */
        wt_tcirc(11, 8, 5, 18), wt_lines(18, TT_W - 7, 6, 2, 5, 12);
        wt_agfa(6, 16);
        wt_trect(62, 16, 11, 10, 18), wt_ttextf(63, 18, "60", 11, TT_W - 4, 1);
        wt_tc(40, 28, nm, 19, 1);
        break;

    case WM_SCOTCH:                                                    /* clear blue, brown spools, a pale label, 1, 60 min., 2x30 min., Scotch, 3M; DYNARANGE -> the name */
        wt_tcirc(28, 21, 15, 11), wt_tcirc(52, 21, 15, 11);
        wt_trect(5, 3, TT_W - 10, 32, 3);
        wt_ttextf(7, 4, "1", 4, 14, 1), wt_lines(14, TT_W - 8, 6, 2, 5, 4);
        wt_ttextf(7, 17, "60", 4, 20, 1), wt_ttext(7, 24, "MIN.", 4, 20), wt_ttext(61, 17, "2X30", 4, TT_W - 3), wt_ttext(62, 23, "MIN.", 4, TT_W - 3);
        wt_ttext(7, 29, "SCOTCH", 12, 30), wt_ttextf(31, 28, nm, 5, 64, 1), wt_ttext(65, 29, "3M", 4, TT_W - 4);
        break;
    case WM_SEIKO:                                                     /* white label with lines, INDEX, A, a blue middle band, C-60; SEIKO -> the name */
        wt_trect(5, 3, TT_W - 10, 32, 3);
        wt_lines(6, 22, 9, 6, 4, 4), wt_lines(58, TT_W - 6, 9, 6, 4, 4);
        wt_ttext(7, 4, "INDEX", 4, 22), wt_ttextf(64, 3, "A", 11, TT_W - 4, 1);
        wt_trect(22, 3, 36, 32, 6);
        wt_tc(40, 4, nm, 5, 1), wt_ttext(25, 11, "COMPACT", 18, 56);
        wt_tc(40, 28, "C-60", 12, 1);
        break;
    case WM_TDKD:                                                      /* clear violet, a black spool, TDK, SUPER D, 120, a cup, LOW NOISE...; SUPER D -> the name */
        wt_tcirc(28, 22, 17, 11);
        wt_ttext(8, 5, "TDK", 14, 22), wt_ttextf(26, 4, nm, 5, 66, 1), wt_ttextf(64, 9, "120", 13, TT_W - 2, 0);
        wt_tcirc(64, 5, 3, 14);
        wt_trect(5, 13, TT_W - 10, 1, 12);
        wt_tcirc(10, 30, 2, 13), wt_ttext(16, 29, "LOW NOISE SUPERIOR", 14, TT_W - 6);
        wt_trect(5, 34, TT_W - 10, 3, 12);
        break;
    case WM_KITTYP: case WM_KITTYB: {                                  /* see-through pink / blue, AXIA, Hello Kitty (her face, the title, flowers), A, 46; TYPE2 HIGH POSITION -> the name */
        int32_t fx = 18;
        wt_ttext(6, 4, "AXIA", 4, 24);
        wt_tring(fx, 9, 5, 1, 4), wt_tpx(fx - 6, 4, 4), wt_tpx(fx - 5, 5, 4), wt_tpx(fx + 6, 4, 4), wt_tpx(fx + 5, 5, 4);   /* her head, ears */
        wt_tpx(fx - 2, 9, 12), wt_tpx(fx + 2, 9, 12), wt_tpx(fx, 11, 13);
        wt_tcirc(fx + 4, 5, 1, 4);                                      /* the bow */
        wt_ttext(27, 4, "HELLO KITTY", 4, TT_W - 6), wt_ttext(30, 10, "(C)76,99 SANRIO", 4, TT_W - 6);
        wt_trect(14, 14, 52, 15, 6);
        wt_ttext(16, 19, "A", 18, 22), wt_ttext(58, 19, md == WM_KITTYP ? "46" : "74", 18, 66);
        wt_tc(40, 30, nm, 5, 1);
        break;
    }
    case WM_SPANKP: case WM_SPANKG: {                                  /* see-through pink / green, black spools, cocktail glasses, dots; Spanking New -> the name */
        int32_t k;
        wt_tcirc(29, 21, 13, 14), wt_tcirc(51, 21, 13, 14);
        for (k = 0; k < 8; k++) { int32_t cx = 8 + (k * 29) % 66, cy = 5 + (k * 17) % 30; wt_tring(cx, cy, 2, 1, (uint8_t)(k & 1 ? 13 : 11)); }
        for (k = 0; k < 3; k++) { int32_t gx = 10 + k * 26; wt_trect(gx, 30, 5, 1, 11), wt_trect(gx + 2, 26, 1, 4, 11), wt_trect(gx, 24, 5, 2, 12); }
        wt_ttextf(6, 3, nm, 5, TT_W - 6, 1);
        wt_ttext(44, 30, "TYPE I", 18, TT_W - 4);
        break;
    }
    case WM_AGFAC90:                                                   /* the Agfa square, lines, a big 1, Made in Germany, Compact Cassette C90 1 LOW-NOISE; C90 -> the name */
        wt_agfa(7, 4);
        wt_ttextf(20, 4, nm, 18, 60, 1), wt_lines(20, TT_W - 7, 12, 1, 3, 11);
        wt_ttextf(64, 17, "1", 18, TT_W - 4, 1);
        wt_ttext(7, 30, "COMPACT C90 1 LOW-NOISE", 19, TT_W - 5);
        break;
    case WM_APPLEPIE: {                                                /* yellow on black, the M logo, 1, C8, Midi Matilda, a black dot, the barcode, ACCENT 2004; APPLEPIE -> the name */
        int32_t k;
        wt_trect(5, 3, TT_W - 10, 32, 3);
        wt_trect(7, 4, 9, 6, 19), wt_ttext(9, 5, "MM", 3, 16), wt_ttext(18, 5, "1", 4, 22);
        for (k = 0; k < 4; k++) wt_trect(7 + k * 2, 17 - k, 2, 1 + k, (uint8_t)(k & 1 ? 12 : 11));
        wt_trect(25, 4, 9, 9, 18), wt_ttext(26, 6, "C8", 19, 34);
        wt_ttext(46, 4, "MIDI MATILDA", 4, TT_W - 5), wt_lines(36, TT_W - 7, 10, 2, 3, 4);
        wt_tcirc(66, 21, 4, 19);
        wt_ttextf(7, 28, nm, 5, 52, 1);
        for (k = 0; k < 8; k++) wt_trect(53 + k * 2, 28, 1, 6, 19);
        break;
    }
    case WM_3M:                                                        /* see-through orange, a red label, white stripes, Cassette 60/Side A, 3M; the name on the label */
        wt_trect(5, 3, TT_W - 10, 32, 3);
        for (y = 14; y < 35; y++) for (x = 5; x < TT_W - 5; x++) if (((x + y) / 7) % 2 == 0) wt_tpx(x, y, 6);
        wt_tc(40, 4, nm, 5, 1), wt_lines(7, TT_W - 7, 11, 1, 3, 6);
        wt_ttext(36, 30, "3M", 6, 50);
        wt_ttext(52, 30, "60/A", 18, TT_W - 6);
        break;
    case WM_DEMAG: {                                                   /* very clear, a circuit board inside (chips, capacitors, a battery), TDK, a label; HEAD DEMAGNETIZER -> the name */
        int32_t k;
        wt_trect(16, 4, TT_W - 24, 24, 11);                              /* the board */
        for (k = 0; k < 6; k++) wt_trect(20 + k * 9, 6 + (k & 1) * 5, 5, 4, 12);   /* chips */
        wt_tcirc(58, 8, 3, 14), wt_tcirc(66, 9, 3, 14), wt_trect(46, 5, 3, 6, 13), wt_trect(68, 4, 5, 6, 15);   /* capacitors */
        for (k = 0; k < 10; k++) wt_trect(18 + k * 6, 25, 3, 1, 16);    /* resistors */
        wt_tring(10, 9, 4, 1, 17), wt_tcirc(10, 9, 2, 17);               /* the battery */
        wt_trect(5, 28, TT_W - 10, 7, 3);
        wt_ttext(7, 29, "TDK", 19, 18), wt_ttextf(20, 28, nm, 5, TT_W - 6, 1);
        break;
    }
    default:
        break;
    }
    (void)kd;
    /* the window: a frame (a solid shell) or the shell seen through, the two reels, the tape between */
    if (wt_clear(md)) clear = 1;
    wt_trect(21, 15, 38, 13, clear ? 20 : 7);
    if (clear) wt_trect(21, 15, 38, 1, 2), wt_trect(21, 27, 38, 1, 2);
    wt_trect(33, 17, 14, 8, 10);
    wt_trect(35, 18, 10, 6, 1);
    wt_trect(37, 19, 6, 4, 10);
    wt_reel(29, 21, spin), wt_reel(50, 21, spin);
    for (y = 37; y < TT_H - 1; y++) {
        int32_t in = (y - 37) / 3;
        wt_trect(14 + in, y, TT_W - 28 - 2 * in, 1, clear ? ((y & 1) ? 20 : 2) : (y & 1) ? 7 : 2);
    }
    wt_tcirc(22, 45, 2, 9), wt_tcirc(57, 45, 2, 9);
    wt_trect(31, 43, 3, 3, 9), wt_trect(46, 43, 3, 3, 9);
    wt_screw(3, 3), wt_screw(TT_W - 4, 3), wt_screw(3, TT_H - 4), wt_screw(TT_W - 4, TT_H - 4), wt_screw(40, 40);
}
/* draw the current texture as a turning quad: centre cx, cy; full size w0 x h0; yaw a (64ths of a turn). Two-pixel blocks, the texture mapped
 * linearly across the face (no perspective correction: the PS1 wobble), the far side darker by a 4 x 4 dither */
static uint8_t wt_ghost;
static UI_NOINLINE void wt_quad(int32_t cx, int32_t cy, int32_t w0, int32_t h0, uint32_t a, int32_t shade, int32_t bs)
{
    int32_t c = WV_SIN[(a + 16u) & 63u], sn = WV_SIN[a & 63u], hw = w0 * c / 254, zl = -w0 * sn / 254, zr = -zl, hl, hr, x0, x1, sx;
    static const uint8_t BY[16] = {0, 8, 2, 10, 12, 4, 14, 6, 3, 11, 1, 9, 15, 7, 13, 5};
    int flip = hw < 0;
    int32_t T = w0 * 15 / 100, sw = T * (sn < 0 ? -sn : sn) / 127;      /* the tape's thickness: its side shows as it turns */
    if (flip) hw = -hw, zl = -zl, zr = -zr;
    hl = h0 * 220 / (220 + zl), hr = h0 * 220 / (220 + zr);
    {                                                                  /* the side: dark plastic, a groove along it, beside the near edge */
        int32_t near_l = zl < 0, hn = near_l ? hl : hr, ex = near_l ? cx - hw - sw : cx + hw, i;
        if (hw < 2) ex = cx - T / 2, sw = T, hn = h0;
        for (i = 0; i < sw; i += bs) {
            int32_t hh = hn - (near_l ? sw - i : i) * hn / (sw * 14 + 1), top = cy - hh / 2;
            WFR(ex + i, top, bs, hh, wt_pal[1]);
            WFR(ex + i, top, bs, 1, wt_pal[2]);
            WFR(ex + i, cy - 1, bs, 2, wf_dim(wt_pal[1], 120));
            WFR(ex + i, top + hh * 3 / 4, bs, 1, wf_dim(wt_pal[1], 140));
        }
    }
    if (hw < 2) return;
    x0 = bs > 1 ? (cx - hw) & ~1 : cx - hw, x1 = bs > 1 ? (cx + hw) & ~1 : cx + hw;
    for (sx = x0; sx < x1; sx += bs) {
        int32_t u = (sx - x0) * 1024 / (x1 - x0), h = hl + (hr - hl) * u / 1024, top = bs > 1 ? (cy - h / 2) & ~1 : cy - h / 2, sy, tu;
        int32_t dark = shade + (flip ? 2 : 0) + (u < 512 ? (zl > 0 ? zl / 18 : 0) : (zr > 0 ? zr / 18 : 0));
        if (sx + bs <= wf_rx || sx >= wf_rx + wf_rw) continue;
        tu = (flip ? 1023 - u : u) * TT_W / 1024;
        for (sy = top; sy < top + h; sy += bs) {
            int32_t v = (sy - top) * TT_H / (h ? h : 1);
            uint8_t ti = wt_tex[clamp(v, 0, TT_H - 1)][clamp(tu, 0, TT_W - 1)];
            uint16_t col = wt_pal[ti];
            if (ti == 20u) {                                           /* a clear shell: every other pixel, the back shows through */
                if (((sx / bs) + (sy / bs)) & 1) continue;
                col = wf_dim(col, 235);
            } else if (wt_ghost && dark > 0 && BY[((sy >> 1) & 3) * 4 + ((sx >> 1) & 3)] < (uint32_t)clamp(dark, 0, 15)) col = wf_dim(col, 150);   /* (only the far ones: the middle one keeps its colours) */
            if (wt_ghost) { if (((sx / bs) + (sy / bs)) & 1) continue; col = wf_dim(col, 110); }
            WFR(sx, sy, bs, bs, col);
        }
    }
}
static UI_NOINLINE void wt_floor(void)                                 /* the back: purple, greyer and lighter at the top (a soft light), in a coarse pixel dither; the grid floor */
{
    static const uint8_t BY[16] = {0, 8, 2, 10, 12, 4, 14, 6, 3, 11, 1, 9, 15, 7, 13, 5};
    static const uint16_t G[5] = {RGB(0x7a, 0x70, 0x90), RGB(0x5c, 0x48, 0x82), RGB(0x40, 0x28, 0x6c), RGB(0x2a, 0x14, 0x4c), RGB(0x16, 0x08, 0x2a)};
    int32_t y0 = LO_Y + 66, i, x, y, top = LO_Y - 1, h = FT_Y - LO_Y - 2;
    for (y = top; y < top + h; y += 2) {
        int32_t f = (y - top) * 4 * 16 / h, band = f / 16, frac = f % 16;
        if (y + 2 <= wf_ry || y >= wf_ry + wf_rh) continue;
        for (x = 4; x < 236; x += 2)
            WFR(x, y, 2, 2, G[clamp(band + (BY[((y >> 1) & 3) * 4 + ((x >> 1) & 3)] < (uint32_t)frac ? 1 : 0), 0, 4)]);
    }
    for (i = 0; i < 7; i++) WFR(4, y0 + i * i, 232, 1, RGB(0x60, 0x40, 0x98));
    for (i = -8; i <= 8; i++) {
        int32_t xb = 120 + i * 40, xt = 120 + i * 12;
        cv_line(xt, y0, xb, y0 + 36, RGB(0x60, 0x40, 0x98));
    }
}
static UI_NOINLINE void wf_tapeswin(void)
{
    uint32_t ag = wft_age(), o, n = sx_n ? sx_n : 1u;
    int32_t frac = wt_cur - sx_tgt, k, out = 0, in = 0;
    char nm[16];
    if (ag < 400u) out = (int32_t)ag * 1024 / 400;                        /* copy: the others fly out ... */
    else if (ag < 1500u) out = 1024;
    else if (ag < WFT_MS) in = 1024 - (int32_t)(ag - 1500u) * 1024 / 500, out = in * in / 1024;   /* ... and come back */
    if (wf_vis(4, LO_Y - 1, 232, FT_Y - LO_Y)) {
        wt_floor();
        for (o = 0; o < 5u; o++) {                                     /* far ones first: -2, 2, -1, 1, 0 */
            static const int8_t ORD[5] = {-2, 2, -1, 1, 0};
            int32_t d = ORD[o], p = d * 256 + frac, ap = p < 0 ? -p : p, x, cy, w0, h0;
            uint32_t idx = (uint32_t)((int32_t)sx_i + d + (int32_t)n * 4) % n, a;
            if (ap > 640) continue;
            if (d == 0 && ag >= 400u && ag < 1500u) continue;              /* (the middle one is flying: the overlay draws it) */
            x = 120 + p * 64 / 256 + (d ? (d < 0 ? -1 : 1) * out * 150 / 1024 : 0);
            w0 = ap < 256 ? 94 - ap * 42 / 256 : 52 - (ap - 256) * 10 / 256, h0 = w0 * 63 / 100;
            if (d == 0 && ag >= 1500u && ag < WFT_MS) w0 = w0 * (1024 - in) / 1024, h0 = h0 * (1024 - in) / 1024;
            cy = LO_Y + 44 - ap * 6 / 256;
            if (ap < 40 && (sx_on || song.playing)) a = 0;               /* playing: it stands still, facing you, and its reels turn */
            else if (ap < 40) {                                        /* in the middle, quiet: a slow sway showing its cover, and every 6 s a whole turn */
                uint32_t ph = fm1_ms % 6000u;
                a = ph < 900u ? (uint32_t)(ph * 64u / 900u) & 63u : (uint32_t)((int32_t)WV_SIN[((ph - 900u) / 80u) & 63u] * 5 / 127) & 63u;
            } else a = (uint32_t)((p > 0 ? 64 : 0) + (p > 0 ? -1 : 1) * clamp(ap * 4 / 256, 0, 6)) & 63u;
            {
                uint32_t rs = d == 0 && (sx_on || song.playing) ? fm1_ms / 55u : 0u;     /* the reels turn only while it plays */
                wt_make(idx, ((a + 16u) & 63u) >= 32u, rs);             /* (facing away: its back) */
            }
            wt_ghost = ap >= 40;                                       /* the ones behind: darker and see-through (every other block) */
            wt_quad(x, cy, w0, h0, a, ap / 160, ap < 40 ? 1 : 2);
            wt_ghost = 0;
            if (ap < 40 && !(ag >= 400u && ag < WFT_MS)) {             /* its kind, small, above it */
                const char *kd = wt_kind(idx);
                WFR(120 - wf_tw(0, kd) / 2 - 4, cy - h0 / 2 - 10, wf_tw(0, kd) + 8, 8, WC_FR1);
                WFR(120 - wf_tw(0, kd) / 2 - 4, cy - h0 / 2 - 10, 2, 8, WT_M[wt_model(idx)].shell);
                wf_textc(121, cy - h0 / 2 - 8, 0, kd, WC_WHT);
            }
        }
    }
    if (wf_vis(4, LO_Y + 78, 232, 12)) {                                /* the name, its kind, where in the list */
        char b[24];
        sx_name(nm, sx_i, sizeof nm);
        wf_textc(120, LO_Y + 80, 1, nm, WC_WHT);
        str_cpy(b, wt_kind(sx_i), 12);
        str_cpy(b + str_len(b), "  ", 24 - str_len(b));
        fmt_int(b + str_len(b), (int32_t)sx_i + 1);
        str_cpy(b + str_len(b), "/", 24 - str_len(b));
        fmt_int(b + str_len(b), (int32_t)n);
        wf_textr(232, LO_Y + 89, 0, b, WC_LAB);
        if (sx_on) wf_text(8, LO_Y + 89, 0, "LISTENING", WC_GRN);
        else if (sx_i == sx_com) wf_text(8, LO_Y + 89, 0, "ON THE TRACK", WC_AMB);
    }
    if (wf_vis(4, FT_Y - 1, 232, 15)) {
        wf_inset(5, FT_Y, 230, 13, WC_LCD);
        k = wf_text(9, FT_Y + 3, 1, "PRESETS", WC_GRN), k = wf_text(k + 3, FT_Y + 3, 1, "ROLL", WC_LAB);
        k = wf_text(k + 8, FT_Y + 3, 1, "PLAY", WC_GRN), k = wf_text(k + 3, FT_Y + 3, 1, "HEAR", WC_LAB);
        k = wf_text(k + 8, FT_Y + 3, 1, "SEL", WC_GRN), wf_text(k + 3, FT_Y + 3, 1, "COPY", WC_LAB);
    }
}
static UI_NOINLINE void wf_tape_overlay(void)                          /* the copy, over the whole screen */
{
    uint32_t ag = wft_age(), i;
    int32_t tx = 5 + (int32_t)song.sel * 58 + 28;
    if (ag >= 400u && ag < 760u) {                                      /* the tape flies up into the track and hits it hard */
        int32_t k = (int32_t)(ag - 400u) * 1024 / 300, e = clamp(k * k / 1024, 0, 1024);   /* ease-in: faster and faster */
        int32_t cx = 120 + (tx - 120) * e / 1024, cy = LO_Y + 44 + (TK_Y + 18 - (LO_Y + 44)) * e / 1024, w0 = 94 - 44 * e / 1024;
        if (ag >= 700u) w0 = w0 * (int32_t)(760u - ag) / 60, cy -= (int32_t)(ag - 700u) / 10;   /* squashed into it */
        wt_make(sx_com, 0, fm1_ms / 20u);                              /* (flying: the reels whirl) */
        wt_quad(cx, cy, w0, w0 * 63 / 100, (fm1_ms / 12u) & 63u, 0, 2);
    }
    if (ag >= 650u && ag < 900u) {                                      /* sparks off the hit */
        int32_t t = (int32_t)(ag - 650u);
        for (i = 0; i < 14u; i++) {
            int32_t vx = (int32_t)((i * 37u) % 15u) - 7, vy = -(int32_t)((i * 53u) % 9u) - 2;
            WFR(tx + vx * t / 40, TK_Y + 10 + vy * t / 40 + t * t / 9000, 2, 2, i & 1u ? WC_WHT : WC_KG2);
        }
    }
    if (ag >= 650u && ag < 1150u) {                                     /* the loop's notes blow up off the seek bar */
        int32_t t = (int32_t)(ag - 650u);
        for (i = 0; i < wft_rn; i++) {
            int32_t vx = (int32_t)((i * 29u) % 11u) - 5, vy = (int32_t)((i * 17u) % 6u) + 2;
            WFR(wft_rp[i][0] + vx * t / 50, wft_rp[i][1] - vy * t / 45 + t * t / 2600, 3, 1, WF_KNOB[(song.sel + 3u) & 3u]);
        }
    }
    if (ag >= 650u && ag < 1150u) {                                     /* the bars blow up out of the display */
        int32_t t = (int32_t)(ag - 650u);
        for (i = 0; i < 19u; i++) {
            int32_t r, bx = LCD_X + 5 + (int32_t)i * 5;
            for (r = 0; r < (int32_t)wft_spec0[i]; r += 2) {
                uint32_t h = i * 131u + (uint32_t)r * 71u;
                int32_t vx = (int32_t)(h % 13u) - 6 + ((int32_t)i - 9) / 2, vy = (int32_t)(h % 7u) + 3;
                WFR(bx + vx * t / 60, LCD_Y + LCD_H - 3 - r - vy * t / 50 + t * t / 2500, 3, 2, WF_VISC[clamp(r, 0, 23) / 2]);
            }
        }
    }
}
static UI_NOINLINE void wfm_tapes_draw(void)                         /* ui.menu 9: the player, the tapes below */
{
    int32_t d = (int32_t)sx_tgt - wt_cur;
    wt_cur += d / 4 + (d > 0 ? 1 : d < 0 ? -1 : 0);                     /* the carousel eases to the tape */
    if (d > 768 || d < -768) wt_cur = sx_tgt - (d > 0 ? 768 : -768);
    wf_pg = 9, wf_view = 3;
    wfm_draw_();
}
static UI_NOINLINE void wfm_tapes_open(void)                         /* (from the player: HOME's 4th window, or PLAY held) */
{
    if (ui.menu) return;
    ui.menu = 9;
    sx_enter();
    sx_on = 0;
    wt_cur = sx_tgt;
    ui.force = 1;
}
/* the phrase the tape played (ui_menu.c sx_tick: a bass line, a melody, held chords, comping, stabs, a beat) written into the track: 32 steps, the
 * drums 16, the old pattern replaced */
static UI_NOINLINE void wf_padd(track_t *t, uint32_t s, uint32_t note, uint32_t l)
{
    step_t *st = &t->step[s % NSTEP];
    uint32_t j;
    if (st->time != ST_NOTE) st->n = 0, st->time = ST_NOTE, st->flags = 0, st->vel = 100, st->lvl = 0, st->rat = 0;
    if (st->n < 4u) st->note[st->n++] = (uint8_t)note;
    for (j = 1; j < l && s + j < 32u; j++) { step_t *tt = &t->step[(s + j) % NSTEP]; if (tt->time == ST_REST && !tt->n) tt->time = ST_TIE; }
}
static UI_NOINLINE void wfm_copy_phrase(void)
{
    track_t *t = TSEL;
    uint32_t role = sx_role(), k, nev;
    const sxe_t *ev = role == 0u ? SX_BASS : role == 1u ? SX_MEL : role == 2u ? SX_HELD : role == 4u ? SX_STAB : SX_DRUM;
    nev = role == 0u ? sizeof SX_BASS / sizeof *SX_BASS : role == 1u ? sizeof SX_MEL / sizeof *SX_MEL : role == 2u ? sizeof SX_HELD / sizeof *SX_HELD :
          role == 4u ? sizeof SX_STAB / sizeof *SX_STAB : sizeof SX_DRUM / sizeof *SX_DRUM;
    steps_clear(t);
    if (is_drum(t)) {
        for (k = 0; k < nev; k++) dstep_set(&t->dstep[ev[k].s % 16u], lane_of_note(ev[k].n), LV_NORM, 0);
        t->p[P_SLEN] = 16;
    } else {
        if (role == 3u) for (k = 0; k < 10u; k++) wf_padd(t, SX_COMPS[k], 60, 2), wf_padd(t, SX_COMPS[k], 63, 2), wf_padd(t, SX_COMPS[k], 67, 2);
        else for (k = 0; k < nev; k++) wf_padd(t, ev[k].s, ev[k].n, ev[k].l);
        t->p[P_SLEN] = 32;
    }
    t->seq_active = 1;
}
static UI_NOINLINE void wfm_tape_copied(const char *was, const char *now)   /* SEL kept the tape: the show starts (input waits for it) */
{
    uint32_t i;
    str_cpy(wft_old, was, sizeof wft_old);
    str_cpy(wft_new, now, sizeof wft_new);
    for (i = 0; i < 19u; i++) wft_spec0[i] = wf_spec[i] > 4u ? wf_spec[i] : (uint8_t)(6u + i % 5u * 3u);
    wf_roll(1, 1024);                                                  /* (the old notes, to blow up) */
    sx_stop();                                                         /* (the preview ends: its solo and the transport back) */
    wfm_copy_phrase();                                                 /* the tape's phrase is the track's loop now */
    if (!song.playing || transport_req == 2u) transport_req = 1;       /* and it plays: the bars come back up */
    wft_t0 = fm1_ms | 1u;
    wft_back = 1;                                                      /* (when it ends: EFFECTS) */
}
