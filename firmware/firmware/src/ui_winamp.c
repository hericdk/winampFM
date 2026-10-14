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
static const char *const WF_VIEW[5] = {"PLAYLIST", "EFFECTS", "VISUALIZER", "TAPES", "VISUALIZER"};
#define WV_N 7
static const char *const WF_VMODE[WV_N] = {"SCOPE", "BARS", "MILK", "RAYS", "SHIP", "RACER", "DRIVE"};
static uint8_t wf_grp;                       /* the group KNOB 1..4 move */
static uint8_t wf_view = 1;                  /* the lower window: 0 playlist, 1 effects (the first, as Winamp's EQ), 2 visualizer, 3 tapes, 4 WARP (the whole screen) */
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
#define WVF_N 80                         /* the whole screen: 80 x 80 cells of 3 x 3 */
static uint8_t wv_mem[7200] __attribute__((section(".pool")));   /* (the window 115 x 52, the whole screen 80 x 80, a video 120 x 120 at 4 bits) */   /* (pool: RAM is tight under SLOOP 2.6) */
/* the cells being drawn: the window's 115 x 52 (2 x 2 px) or the whole screen's 80 x 80 (3 x 3); the two share the buffer and each
 * clears it when it takes it. A cell: bits 0-3 its light (0 off, it fades: the trails), bit 4 the picture's second colour */
static int32_t wv_w = VZ_CW, wv_h = VZ_CH;
#define WVC(x, y) wv_mem[(y) * wv_w + (x)]
static uint8_t wvf_on;                       /* the buffer holds the whole screen's picture */
static uint32_t wv_ph;                        /* its own clock */
static const int8_t WV_SIN[64] = {0, 12, 25, 37, 49, 60, 71, 81, 90, 98, 106, 112, 117, 122, 125, 126, 127, 126, 125, 122, 117, 112, 106,
    98, 90, 81, 71, 60, 49, 37, 25, 12, 0, -12, -25, -37, -49, -60, -71, -81, -90, -98, -106, -112, -117, -122, -125, -126, -127, -126,
    -125, -122, -117, -112, -106, -98, -90, -81, -71, -60, -49, -37, -25, -12};
static UI_NOINLINE int32_t wv_sin4(uint32_t a) { uint32_t i = (a >> 2) & 63u, f = a & 3u; return (WV_SIN[i] * (int32_t)(4u - f) + WV_SIN[(i + 1u) & 63u] * (int32_t)f) / 4; }   /* a in 1/256 turns */
static uint8_t wv_force;                     /* 1: a line paints over (a dark needle on a light face), else the brighter cell wins */
static UI_NOINLINE void wv_put(int32_t x, int32_t y, uint32_t v) { if ((uint32_t)x < (uint32_t)wv_w && (uint32_t)y < (uint32_t)wv_h && (wv_force || (WVC(x, y) & 15u) < (v & 15u))) WVC(x, y) = (uint8_t)v; }
static UI_NOINLINE void wv_line(int32_t x0, int32_t y0, int32_t x1, int32_t y1, uint32_t v)
{
    int32_t dx = x1 > x0 ? x1 - x0 : x0 - x1, dy = -(y1 > y0 ? y1 - y0 : y0 - y1), sx = x0 < x1 ? 1 : -1, sy = y0 < y1 ? 1 : -1, e = dx + dy, n = 0;
    for (;;) {
        wv_put(x0, y0, v);
        int32_t e2 = 2 * e;
        if ((x0 == x1 && y0 == y1) || ++n > 400) break;
        if (e2 >= dy) { e += dy; x0 += sx; }
        if (e2 <= dx) { e += dx; y0 += sy; }
    }
}
static UI_NOINLINE void wv_fade(uint32_t d) { uint32_t n, N = (uint32_t)(wv_w * wv_h); for (n = 0; n < N; n++) { uint32_t v = wv_mem[n]; wv_mem[n] = (uint8_t)((v & 15u) > d ? v - d : 0u); } }
static UI_NOINLINE int32_t wv_bass(void) { return (wf_spec[0] + wf_spec[1] + wf_spec[2] + wf_spec[3]) / 4; }
static UI_NOINLINE int32_t wv_all(void) { int32_t i, a = 0; for (i = 0; i < 19; i++) a += wf_spec[i]; return a; }
/* the window's five pictures, drawn on the cells in use (the window's wide strip or the whole screen: the shapes scale with it) */
/* SHIP (the window's 5th picture, and the whole screen's): a little ship of our own (13 x 13, nose up) flies through streaks of light;
 * the streaks run faster with the sound, the ship sways with the melody and its engines flare with the bass. In the wide window it flies
 * to the right. The streaks are cells; the ship is drawn over them (wv_ship_draw) */
static const char *const WSHIP[13] = {"......1......", ".....111.....", ".....141.....", ".....141.....", ".....111.....", "....11311....",
                                      "2...13331...2", "2..1113111..2", "2111113111112", "2311111111132", "2..22.1.22..2", "...22...22...",
                                      "...55...55..."};
static const uint16_t WSHIP_C[6] = {0, RGB(0xd8, 0xdc, 0xe8), RGB(0x5a, 0x60, 0x74), RGB(0xf0, 0x78, 0x18), RGB(0x40, 0xd0, 0xf0), RGB(0xff, 0x60, 0xc0)};
static uint8_t wsh_s[24][3];                 /* the streaks: along, across, length */
static int32_t ws_x, ws_y, ws_fl;            /* the ship (cells) and its flame */
static UI_NOINLINE uint32_t wvf_rnd(void);
static UI_NOINLINE void wv_ship_step(int32_t W, int32_t H)
{
    int32_t i, all = wv_all(), hi = 0, lo = 0, wide = W > H, L = wide ? W : H, X = wide ? H : W, sp = 1 + all / 40;
    wv_fade(5);
    for (i = 0; i < 9; i++) lo += wf_spec[i];
    for (i = 10; i < 19; i++) hi += wf_spec[i];
    for (i = 0; i < 24; i++) {
        uint8_t *q = wsh_s[i];
        if (!q[2]) { q[0] = (uint8_t)(wvf_rnd() % (uint32_t)L), q[1] = (uint8_t)(wvf_rnd() % (uint32_t)X), q[2] = (uint8_t)(2u + wvf_rnd() % 9u); }
        q[0] = (uint8_t)(q[0] + (uint32_t)(sp * q[2] / 4 + 1));
        if (q[0] >= (uint32_t)L + q[2]) { q[0] = 0, q[1] = (uint8_t)(wvf_rnd() % (uint32_t)X), q[2] = (uint8_t)(2u + wvf_rnd() % 9u); }
        if (wide) wv_line(W - (int32_t)q[0], q[1], W - (int32_t)q[0] + q[2], q[1], 4u + q[2]);
        else wv_line(q[1], (int32_t)q[0] - q[2], q[1], q[0], 4u + q[2]);
    }
    i = WV_SIN[(wv_ph * 2u) & 63u] * 3 / 127 + clamp((hi - lo) / 12, -8, 8);   /* the sway */
    if (wide) ws_x = W / 4 + WV_SIN[(wv_ph + 16u) & 63u] * 2 / 127, ws_y = H / 2 + i;
    else ws_x = W / 2 + i, ws_y = H * 2 / 3 + WV_SIN[(wv_ph + 16u) & 63u] * 2 / 127;
    ws_fl = 2 + wv_bass() / 4 + (int32_t)(wv_ph & 1u);
}
static UI_NOINLINE void wv_ship_draw(int32_t cx, int32_t cy, int32_t z, int wide)   /* the ship at pixel cx, cy, z px a dot; wide: nose right */
{
    int32_t r, c, k;
    for (r = 0; r < 13; r++)
        for (c = 0; c < 13; c++) {
            uint32_t v = (uint32_t)(WSHIP[r][c] - '0');
            if (WSHIP[r][c] == '.') continue;
            if (wide) WFR(cx + (6 - r) * z, cy + (c - 6) * z, z, z, WSHIP_C[v]);
            else WFR(cx + (c - 6) * z, cy + (r - 6) * z, z, z, WSHIP_C[v]);
        }
    for (k = 0; k < ws_fl; k++) {                                      /* the flames: pink, white at the nozzle */
        uint16_t col = k < 1 ? WC_WHT : k < 3 ? RGB(0xff, 0x60, 0xc0) : RGB(0x90, 0x20, 0x70);
        int32_t d = 7 + k, w = k > ws_fl - 2 ? 1 : 2;
        if (wide) { WFR(cx - d * z, cy - 3 * z, z, w * z / 2 + 1, col), WFR(cx - d * z, cy + 2 * z, z, w * z / 2 + 1, col); }
        else { WFR(cx - 3 * z, cy + d * z, w * z / 2 + 1, z, col), WFR(cx + 2 * z, cy + d * z, w * z / 2 + 1, z, col); }
    }
}
static UI_NOINLINE void wv_draw(uint32_t mode)
{
    int32_t x, y, i, W = wv_w, H = wv_h, cx = W / 2, cy = H / 2, ax = W >= 2 * H ? 2 : 1;   /* (ax: the window is wide, the shapes twice as wide) */
    uint32_t dm = mode, w = scope_w;
    int32_t bass = wv_bass(), all = wv_all();
    if (mode == 2u) { static const uint8_t SEQ[5] = {2, 4, 3, 2, 1}; dm = SEQ[(wv_ph / 200u) % 5u]; }   /* MILK: the old-school one, every scene in turn by itself */
    if (mode == 2u) {                                                  /* MILK: the picture flies out of the middle (MilkDrop / Geiss feedback): */
        int32_t q;                                                     /* each cell takes the one nearer the centre, in place, outer cells first */
        for (q = 0; q < 4; q++) {
            int32_t sx = q & 1 ? 1 : -1, sy = q & 2 ? 1 : -1, dy, dx;
            for (dy = cy; dy >= 0; dy--)
                for (dx = cx; dx >= 0; dx--) {
                    int32_t tx = cx + sx * dx, ty = cy + sy * dy, fx = cx + sx * (dx * 7 / 8), fy = cy + sy * (dy * 7 / 8);
                    uint32_t v;
                    if ((uint32_t)tx >= (uint32_t)W || (uint32_t)ty >= (uint32_t)H) continue;
                    v = WVC(fx, fy) & 15u;
                    WVC(tx, ty) = (uint8_t)(v > 1u ? v - 1u : 0u);
                }
        }
    } else
        wv_fade(3);
    wv_ph++;
    if (dm == 0u) {                                                /* SCOPE: the output as a line, Winamp's oscilloscope */
        int32_t py = cy;
        for (x = 0; x < W; x++) {
            int32_t s = scope_buf[(w - 2u * (uint32_t)W + 2u * (uint32_t)x) & (SCOPE_N - 1u)];
            y = clamp(cy - s * cy / 12000, 0, H - 1);
            if (x) wv_line(x - 1, py, x, y, 15);
            py = y;
        }
    } else if (dm == 1u) {                                         /* BARS: the spectrum mirrored round the middle line */
        for (i = 0; i < 19; i++) {
            int32_t bx = 3 + i * (W - 8) / 18, h = wf_spec[i] * (cy - 1) / 24;
            wv_line(bx, cy - h, bx, cy + h, 15);
            wv_line(bx + 1, cy - h, bx + 1, cy + h, 11);
            if (W > 100) wv_put(bx, cy - 1 - wf_peak[i] * (cy - 1) / 24, 15);
            else wv_line(bx + 2, cy - h, bx + 2, cy + h, 7);
        }
    } else if (dm == 2u) {                                         /* MILK: the feedback flies it out; the scene changes by itself every ~8 s, */
        uint32_t sc = (wv_ph / 160u) % 3u;                             /* as MilkDrop's presets do: the wave, rings, a turning beam */
        int32_t py = cy, a, px = 0;
        if (sc == 0u) {
            for (x = W / 7; x < W - W / 7; x++) {
                int32_t sm = scope_buf[(w - 2u * (uint32_t)W + 2u * (uint32_t)x) & (SCOPE_N - 1u)];
                y = clamp(cy - sm * H / 62400, 0, H - 1);
                if (x > W / 7) wv_line(x - 1, py, x, y, 15);
                py = y;
            }
        } else if (sc == 1u) {                                         /* a ring each beat of the bass: they spread as rings in water */
            int32_t r = (3 + bass / 4) * H / 52;
            for (a = 0; a <= 32; a++) {
                int32_t qx = cx + r * ax * WV_SIN[((uint32_t)a * 2u + 16u) & 63u] / 127, qy = cy + r * WV_SIN[((uint32_t)a * 2u) & 63u] / 127;
                if (a) wv_line(px, py, qx, qy, bass > 6 ? 15u : 9u);
                px = qx, py = qy;
            }
        } else {                                                       /* a beam turning round the middle, wider with the sound */
            uint32_t an = wv_ph * 2u;
            for (a = -1 - all / 80; a <= 1 + all / 80; a++) {
                uint32_t q = (an + (uint32_t)(a + 64)) & 63u;
                wv_line(cx, cy, cx + (cx + 2) * WV_SIN[(q + 16u) & 63u] / 127, cy + (cy + 4) * WV_SIN[q] / 127, 15);
                wv_line(cx, cy, cx - (cx + 2) * WV_SIN[(q + 16u) & 63u] / 127, cy - (cy + 4) * WV_SIN[q] / 127, 11);
            }
        }
    } else if (dm == 3u) {                                         /* RAYS: one line per band from the middle, turning */
        for (i = 0; i < 19; i++) {
            uint32_t a = (wv_ph / 2u + (uint32_t)i * 64u / 19u) & 63u;
            int32_t l = (4 + wf_spec[i] * 2 + all / 40) * H / 52;
            wv_line(cx, cy, cx + l * WV_SIN[(a + 16u) & 63u] / 127, cy + l * WV_SIN[a] / (127 * ax), 15);
        }
    } else wv_ship_step(W, H);                                         /* SHIP: a little ship flying through streaks (it was the TUNNEL) */
}
static UI_NOINLINE void nk_step(void);
static UI_NOINLINE void nc_spec(void);
static UI_NOINLINE void wvf_hits(void);
static UI_NOINLINE void sh_step(void);
static UI_NOINLINE void rd_step(void);
static UI_NOINLINE void sh_draw(int32_t rx, int32_t ry, int32_t rw, int32_t rh, int32_t z, int big);
static UI_NOINLINE void v2_racer(void);
static UI_NOINLINE void v2_drive(void);
static UI_NOINLINE void wv_step(void)                               /* the window's picture, 20 fps */
{
    wv_w = VZ_CW, wv_h = VZ_CH;
    if (wvf_on) { wvf_on = 0; memset(wv_mem, 0, sizeof wv_mem); }      /* (back from the whole screen: its cells are not ours) */
    if (wv_mode >= 4u) {                                               /* the scenes (SHIP, RACER, DRIVE): the whole screen's, in the window */
        nk_step(), nc_spec(), wv_ph++, wvf_hits();
        if (wv_mode == 4u) sh_step(); else rd_step();
        return;
    }
    wv_draw(wv_mode);
}
static UI_NOINLINE void wv_scene(void)                              /* draw the scene into the window, clipped to it (RACER, DRIVE: a band of the big picture) */
{
    int32_t ox = cv_ox, oy = cv_oy, ry = wf_ry, sy = wv_mode == 5u ? 92 : 90;
    if (wv_mode == 4u) {
        cv_cx0 = VZ_X, cv_cy0 = VZ_Y, cv_cx1 = VZ_X + VZ_CW * 2, cv_cy1 = VZ_Y + VZ_CH * 2;
        sh_draw(VZ_X, VZ_Y, VZ_CW * 2, VZ_CH * 2, 2, 0);
    } else {
        cv_oy = oy + VZ_Y - sy, wf_ry = ry - (VZ_Y - sy);              /* (the picture's rows sy .. sy + 104 in the window) */
        cv_cx0 = VZ_X, cv_cy0 = sy, cv_cx1 = VZ_X + VZ_CW * 2, cv_cy1 = sy + VZ_CH * 2;
        if (wv_mode == 5u) v2_racer(); else v2_drive();
    }
    cv_ox = ox, cv_oy = oy, wf_ry = ry;
    cv_cx0 = -32768, cv_cy0 = -32768, cv_cx1 = 32767, cv_cy1 = 32767;
}
static const uint16_t WVF_PAL[5][4] = {      /* the second colours (bit 4) and the own pictures' first */
    {RGB(0x18, 0x20, 0x48), RGB(0x40, 0x58, 0xa0), RGB(0x98, 0xb0, 0xf0), RGB(0xf0, 0xf4, 0xff)},   /* WARP stars */
    {RGB(0x50, 0x20, 0x04), RGB(0xa0, 0x48, 0x0c), RGB(0xf0, 0x78, 0x18), RGB(0xff, 0xc8, 0x70)},   /* the bolt's orange */
    {RGB(0x50, 0x46, 0x30), RGB(0x98, 0x88, 0x60), RGB(0xe8, 0xd8, 0xa8), RGB(0xff, 0xf8, 0xe0)},   /* VU: cream */
    {RGB(0x50, 0x08, 0x08), RGB(0xa0, 0x18, 0x10), RGB(0xff, 0x34, 0x22), RGB(0xff, 0xa0, 0x90)},   /* red */
    {RGB(0x28, 0x08, 0x40), RGB(0x60, 0x10, 0x88), RGB(0xd0, 0x30, 0xc8), RGB(0xff, 0x90, 0xf8)}};  /* SYNTH: magenta */
static const uint16_t WVF_PLASMA[16] = {
    RGB(0x10, 0x04, 0x28), RGB(0x24, 0x08, 0x50), RGB(0x40, 0x0c, 0x78), RGB(0x68, 0x10, 0x90), RGB(0x98, 0x14, 0x98), RGB(0xc8, 0x20, 0x88),
    RGB(0xf0, 0x38, 0x68), RGB(0xff, 0x60, 0x40), RGB(0xff, 0x90, 0x20), RGB(0xff, 0xc0, 0x30), RGB(0xff, 0xe8, 0x80), RGB(0xc0, 0xf0, 0xa0),
    RGB(0x60, 0xd0, 0xc0), RGB(0x30, 0x90, 0xd0), RGB(0x20, 0x50, 0xb0), RGB(0x18, 0x18, 0x70)};
static const uint16_t WV_PAL[6][4] = {                /* per picture: dim .. bright */
    {RGB(0x06, 0x30, 0x06), RGB(0x10, 0x78, 0x10), RGB(0x1c, 0xc8, 0x1c), RGB(0xb0, 0xff, 0xa0)},
    {RGB(0x30, 0x24, 0x08), RGB(0x80, 0x68, 0x18), RGB(0xe0, 0xc0, 0x30), RGB(0xff, 0xf0, 0xb0)},
    {RGB(0x08, 0x20, 0x40), RGB(0x18, 0x60, 0xa8), RGB(0x40, 0xb0, 0xf0), RGB(0xd8, 0xf0, 0xff)},
    {RGB(0x38, 0x10, 0x08), RGB(0x98, 0x30, 0x18), RGB(0xf0, 0x70, 0x20), RGB(0xff, 0xd8, 0x90)},
    {RGB(0x20, 0x08, 0x38), RGB(0x60, 0x18, 0xa0), RGB(0xc0, 0x40, 0xf0), RGB(0xf8, 0xc8, 0xff)},
    {RGB(0x08, 0x28, 0x30), RGB(0x10, 0x80, 0x90), RGB(0x30, 0xe0, 0xd0), RGB(0xd0, 0xff, 0xf8)}};
static UI_NOINLINE uint32_t wv_pal_of(uint32_t mode) { return mode == 2u ? (wv_ph / 40u) % 6u : mode == 4u ? 5u : mode; }
static UI_NOINLINE void wf_visual(void)
{
    int32_t x, y;
    if (wf_vis(VZ_X - 1, VZ_Y - 1, VZ_CW * 2 + 2, VZ_CH * 2 + 2)) {
        wf_inset(VZ_X, VZ_Y, VZ_CW * 2, VZ_CH * 2, WC_LCD);
        if (wv_mode >= 4u) wv_scene();
        else for (y = 0; y < VZ_CH && !wvf_on; y++) {                 /* (growing into the whole screen: the buffer is the big one's) */
            int32_t sy = VZ_Y + y * 2;
            if (sy + 2 <= wf_ry || sy >= wf_ry + wf_rh) continue;
            for (x = 0; x < VZ_CW; x++) {
                uint32_t v = wv_mem[y * VZ_CW + x] & 15u;
                if (v) WFR(VZ_X + x * 2, sy, 2, 2, wv_mode == 4u ? WVF_PAL[0][(v - 1u) >> 2] : WV_PAL[wv_pal_of(wv_mode)][(v - 1u) >> 2]);
            }
        }
    }
    if (wf_vis(4, VZ_Y + VZ_CH * 2 + 1, 232, 240 - (VZ_Y + VZ_CH * 2 + 1))) {   /* the foot: the picture's name, its knob */
        int32_t fy = VZ_Y + VZ_CH * 2 + 2;   /* (rows 230..236) */
        int32_t i2, fx = 9;
        for (i2 = 0; i2 < WV_N; i2++) fx = wf_text(fx, fy + 1, 0, WF_VMODE[i2], i2 == (int32_t)wv_mode ? WC_GRN : WC_LOFF) + 6;
        {                                                              /* SELECT changes the picture: "SEL" and a little knob, its pointer on the picture */
            static const int8_t PT[WV_N][2] = {{-3, 2}, {-4, 0}, {-3, -2}, {0, -4}, {3, -2}, {4, 0}, {3, 2}};   /* 7 o'clock .. 5 o'clock */
            int32_t kx = 228, ky = fy + 3, yy, xx;
            wf_textr(kx - 7, fy, 1, "SEL", WC_LAB);
            for (yy = -4; yy <= 4; yy++) for (xx = -4; xx <= 4; xx++) {
                int32_t d2 = xx * xx + yy * yy;
                if (d2 <= 16) cv_pset(kx + xx, ky + yy, d2 >= 10 ? WC_BSH : WC_BTN);
            }
            for (yy = 0; yy <= 3; yy++) cv_pset(kx + PT[wv_mode % WV_N][0] * yy / 3, ky + PT[wv_mode % WV_N][1] * yy / 3, WC_AMB);
            (void)xx;
        }
    }
}

/* ---------------------------------------------------------------- the visualizer on the whole screen (HOME after the VISUALIZER, before TAPES):
 * it comes in through the VHS tape, as every window. The window's five pictures (the same choice: SELECT here or there), then its own:
 * the space ones in a row (the ship, inside it, the jump, the black hole, the crystal planet), then the rest. The names at the foot for
 * 4 s after a turn of SELECT. Its own pictures leave the window as it was. */
static uint8_t sp_s[19];                       /* the spectrum, smoothed (the whole screen's pictures) */
static uint32_t nk_mask, nk_prev, nk_on;          /* the keys sounding (played or by the tracks), the ones that just started */
static uint32_t nk_hand, nk_hprev, nk_hon;        /* the keys under the fingers only, the ones just pressed */
static uint16_t dr_hits;                         /* the drum lanes hit this step (lane 3: the clap) */
static int32_t ev_lvl, ev_rate;                  /* the level, smoothed (0..~300); onsets a second, smoothed (x16) */
#define GV_N 28
static const char *const GV_NAME[GV_N] = {"SCOPE", "BARS", "MILK", "RAYS", "SHIP", "RACER", "DRIVE", "SYNTH", "WATERFALL", "CURVES", "FLOW", "TARGET",
                                          "HOUND", "BOXER", "CHORDS", "ORB", "DANCE", "BLUEPRINT", "PATCH", "TAPE", "NEO", "SMOKE", "DOOMSCROLL", "ROOM",
                                          "FLOAT", "DUEL", "LOFI STUDY", "RACCOON"};
enum { GV_SHIP = 4, GV_RACER, GV_DRIVE, GV_SYNTH, GV_WATER, GV_CURVES, GV_FLOW, GV_TARGET, GV_HOUND, GV_BOXER, GV_CHORDS, GV_ORB, GV_DANCE, GV_BLUEPRINT,
       GV_PATCH, GV_TAPE, GV_NEO, GV_SMOKE, GV_DOOM, GV_ROOM, GV_FLOAT, GV_DUEL, GV_STUDY, GV_RACCOON };
/* the order: the four first pictures of the window, the cars, the lines, the tape, the cities and the moods, the drawn ones last. Everything
 * after the SHIP is drawn in pixels; the last three are videos turned into pixels (wfm_vidpix.h, in this order) */
#define GV_CLIP(m) ((m) >= GV_STUDY ? (int32_t)((m) - GV_STUDY) : -1)
#define GV_PIX(m) ((m) >= GV_SHIP)
#define GV_WIN(g) ((g) <= (uint32_t)GV_DRIVE ? (int32_t)(g) : -1)   /* the window's picture this one is (-1 none): the first seven are the window's */
#define WV_GV(w) ((uint32_t)(w))
#define GV_HT(m) 0
static uint8_t gv_mode;                       /* the whole screen's picture: 0..4 the window's, 5.. its own */
static uint32_t wvf_sel_t;                    /* the names show until 4 s after this */
#define WVF_STARS 56
static int16_t wvf_st[WVF_STARS][3];         /* the stars: x, y (-120..120), depth 8..256 */
static uint32_t wvf_seed = 0x2545F491u;
static UI_NOINLINE uint32_t wvf_rnd(void) { wvf_seed = wvf_seed * 1664525u + 1013904223u; return wvf_seed >> 9; }
static UI_NOINLINE void wvf_new(int32_t i, int32_t far)
{
    wvf_st[i][0] = (int16_t)((int32_t)(wvf_rnd() % 241u) - 120), wvf_st[i][1] = (int16_t)((int32_t)(wvf_rnd() % 241u) - 120);
    wvf_st[i][2] = (int16_t)(far ? 256 : 24 + (int32_t)(wvf_rnd() % 232u));
}
static UI_NOINLINE void wvf_warp(void)                              /* WARP: stars rush out as streaks; the spectrum a ring of rays, turning */
{
    int32_t i, all = wv_all(), sp = 3 + all / 22, c = WVF_N / 2, r0 = 7 + wv_bass() / 3;
    wv_fade(2);
    for (i = 0; i < WVF_STARS; i++) {
        int32_t z0 = wvf_st[i][2], z1 = z0 - sp, x1, y1;
        if (z1 < 8) { wvf_new(i, 1); continue; }
        x1 = c + wvf_st[i][0] * 18 / z1, y1 = c + wvf_st[i][1] * 18 / z1;
        if ((uint32_t)x1 >= WVF_N || (uint32_t)y1 >= WVF_N) { wvf_new(i, 1); continue; }
        wv_line(c + wvf_st[i][0] * 18 / z0, c + wvf_st[i][1] * 18 / z0, x1, y1, (uint32_t)clamp(15 - z1 / 20, 3, 15));
        wvf_st[i][2] = (int16_t)z1;
    }
    for (i = 0; i < 38; i++) {
        uint32_t a = (wv_ph / 3u + (uint32_t)i * 64u / 38u) & 63u;
        int32_t l = r0 + 2 + wf_spec[i % 19] * 2 / 3, ca = WV_SIN[(a + 16u) & 63u], sa = WV_SIN[a];
        wv_line(c + r0 * ca / 127, c + r0 * sa / 127, c + l * ca / 127, c + l * sa / 127, 16u + (uint32_t)clamp(8 + wf_spec[i % 19] / 3, 8, 15));
    }
}
static UI_NOINLINE void wvf_plasma(void)                            /* PLASMA: the demoscene's, four waves summed; the bass pushes it */
{
    static uint32_t t1, t2, t3;
    int32_t x, y, bass = wv_bass();
    t1 += 1u + (uint32_t)bass / 5u, t2 += 2u, t3 += 1u + (uint32_t)wv_all() / 70u;
    for (y = 0; y < WVF_N; y++)
        for (x = 0; x < WVF_N; x++) {
            int32_t d = (x > 40 ? x - 40 : 40 - x) + (y > 40 ? y - 40 : 40 - y);
            int32_t s = WV_SIN[((uint32_t)x * 2u + t1) & 63u] + WV_SIN[((uint32_t)y * 3u + t2) & 63u] + WV_SIN[((uint32_t)(x + y) * 2u + t3) & 63u] +
                        WV_SIN[((uint32_t)d * 3u - t1 * 2u) & 63u];
            wv_mem[y * WVF_N + x] = (uint8_t)(((s + 512) * 16 / 1024) & 15);
        }
}
static int32_t wvf_vu_n[2];                  /* VU: the needles, 1/16 of the scale (the ballistics) */
static UI_NOINLINE void wvf_vu(void)                                /* VU: two analogue meters, LEFT and RIGHT: a cream face, the needle swings, a red peak lamp */
{
    int32_t m, t;
    memset(wv_mem, 0, WVF_N * WVF_N);
    for (m = 0; m < 2; m++) {
        int32_t py = m ? 74 : 35, px = 40, R = 31, lv = 0, i, x, y, tgt;
        const int16_t *b = m ? scope_bufr : scope_buf;
        for (i = 0; i < 256; i++) { int32_t s = b[(scope_w - 256u + (uint32_t)i) & (SCOPE_N - 1u)]; s = s < 0 ? -s : s; if (s > lv) lv = s; }
        tgt = clamp(lv * 256 / 11000, 0, 256);                         /* (the peak of the last 6 ms; 256 = full) */
        wvf_vu_n[m] += (tgt - wvf_vu_n[m]) / (tgt > wvf_vu_n[m] ? 2 : 6);   /* quick up, slow down: as a real needle */
        for (y = py - R; y <= py; y++)                                 /* the face: the quarter turn above the pivot */
            for (x = px - R; x <= px + R; x++) {
                int32_t dx = x - px, dy = y - py;
                if ((dx < 0 ? -dx : dx) <= -dy && dx * dx + dy * dy <= R * R) wv_put(x, y, dx * dx + dy * dy > (R - 9) * (R - 9) ? 14u : 10u);
            }
        wv_force = 1;
        for (t = 0; t <= 8; t++) {                                     /* the scale: ticks, the last three red */
            uint32_t a0 = 168u + (uint32_t)t * 6u;                        /* 168 .. 216 (1/256 turns) */
            int32_t ca = wv_sin4(a0 + 64u), sa = wv_sin4(a0);
            wv_line(px + (R - 7) * ca / 127, py + (R - 7) * sa / 127, px + (R - 2) * ca / 127, py + (R - 2) * sa / 127, t >= 6 ? 16u | 14u : 1u);
        }
        {
            uint32_t a = 168u + (uint32_t)(wvf_vu_n[m] * 48 / 256);    /* the needle: dark, on the face */
            wv_line(px, py, px + (R - 1) * wv_sin4(a + 64u) / 127, py + (R - 1) * wv_sin4(a) / 127, 1);
        }
        wv_line(px - 2, py, px + 2, py, 1), wv_line(px - 1, py - 1, px + 1, py - 1, 1);
        wv_force = 0;
        for (i = 0; i < 3; i++) wv_line(72, py - 30 + i, 74, py - 30 + i, tgt > 230 ? 16u | 15u : 16u | 3u);   /* the peak lamp */
    }
}
static UI_NOINLINE void wvf_marquee(void)                           /* MARQUEE: the spectrum behind (the letters: wvf_paint) */
{
    int32_t i;
    wv_fade(2);
    for (i = 0; i < 19; i++) {
        int32_t bx = 2 + i * 4, h = wf_spec[i] * 70 / 24;
        wv_line(bx, 79 - h, bx, 79, 5), wv_line(bx + 1, 79 - h, bx + 1, 79, 5), wv_line(bx + 2, 79 - h, bx + 2, 79, 4);
        wv_line(bx, 78 - wf_peak[i] * 70 / 24, bx + 2, 78 - wf_peak[i] * 70 / 24, 8);
    }
}
static UI_NOINLINE void wvf_synth(void)                             /* SYNTH: a striped sun on the horizon, pulsing with the bass, a grid floor rushing in */
{
    static uint32_t tz;
    int32_t x, y, i, hy = 46, R = 17 + wv_bass() / 6, sx = 40, sy = 31;
    memset(wv_mem, 0, WVF_N * WVF_N);
    tz += 2u + (uint32_t)wv_all() / 40u;
    for (i = 0; i < 14; i++)                                           /* stars: a few, twinkling */
        wv_put((int32_t)((i * 37u + 11u) % 80u), (int32_t)((i * 13u + 3u) % 24u), ((wv_ph / 3u + (uint32_t)i * 5u) % 9u) < 2u ? 13u : 4u);
    for (y = sy - R; y < hy; y++) {                                    /* the sun: rows of the disc, stripes cut out of its lower half */
        int32_t dy = y - sy, hw = 0, gap = dy > 0 ? dy * 4 / R : 0;
        while ((hw + 1) * (hw + 1) + dy * dy <= R * R) hw++;
        if (dy > 0 && ((uint32_t)y + wv_ph / 2u) % 5u < (uint32_t)gap) continue;
        if (!hw) continue;
        wv_line(sx - hw, y, sx + hw, y, 16u | (uint32_t)clamp(15 - (y - (sy - R)) * 12 / (2 * R), 3, 15));
    }
    wv_line(0, hy, 79, hy, 15);                                        /* the horizon */
    for (i = 0; i < 8; i++) {                                          /* the floor: lines rushing in, nearer = further apart */
        int32_t z = (int32_t)(((uint32_t)i * 32u + tz) % 256u);
        wv_line(0, hy + 1 + z * z / 1700, 79, hy + 1 + z * z / 1700, (uint32_t)(5 + z / 30));
    }
    for (i = -9; i <= 9; i++) wv_line(40 + i * 2, hy + 1, 40 + i * 16, 79, 8);   /* and the lines to the vanishing point */
    for (x = 0; x < 80; x++) {                                         /* mountains: a dark ridge on the horizon */
        int32_t h = 3 + (WV_SIN[((uint32_t)x * 5u) & 63u] + WV_SIN[((uint32_t)x * 13u + 7u) & 63u]) / 60;
        if (x < sx - R - 2 || x > sx + R + 2) wv_line(x, hy - h, x, hy - 1, 6);
    }
}
static int32_t wvf_lastb, wvf_hit_n;          /* a hit of the bass (the lasers, the lightning, a shooting star): frames left */
static UI_NOINLINE void wvf_hits(void) { int32_t b = wv_bass(); if (b > wvf_lastb + 4 && b > 8) wvf_hit_n = 5; else if (wvf_hit_n) wvf_hit_n--; wvf_lastb = b; }
static UI_NOINLINE void wvf_twinkle(int32_t n, int32_t ymax, uint32_t v)   /* stars that twinkle, a few lit by the bands */
{
    int32_t i;
    for (i = 0; i < n; i++)
        wv_put((int32_t)(((uint32_t)i * 37u + 11u) % WVF_N), (int32_t)(((uint32_t)i * 53u + 7u) % (uint32_t)ymax),
               wf_spec[i % 19] > 9 || ((wv_ph / 4u + (uint32_t)i * 7u) % 11u) == 0u ? v : v > 6u ? v - 6u : 2u);
}
static UI_NOINLINE void wv_disc(int32_t cx, int32_t cy, int32_t r, uint32_t v)   /* a filled circle of cells */
{
    int32_t y, hw;
    for (y = -r; y <= r; y++) { hw = 0; while ((hw + 1) * (hw + 1) + y * y <= r * r) hw++; wv_line(cx - hw, cy + y, cx + hw, cy + y, v); }
}
/* COCKPIT: inside a ship (after the pixel-art cockpit reference): space through the canopy, stars drifting, a planet passing, lasers on
 * the kick; the dashboard's screens show the music (the spectrum, a radar of the notes, the volume, the wave, the sound's name) */
static UI_NOINLINE void wvf_cockpit(void)
{
    int32_t i, px, sp = 1 + wv_all() / 60, c = WVF_N / 2;
    wv_fade(4);
    wvf_hits();
    for (i = 0; i < 30; i++) {                                         /* the stars, slowly towards us */
        int32_t z0 = wvf_st[i][2], z1 = z0 - sp, x1, y1;
        if (z1 < 8) { wvf_new(i, 1); continue; }
        x1 = c + wvf_st[i][0] * 18 / z1, y1 = 16 + wvf_st[i][1] * 10 / z1;
        if ((uint32_t)x1 >= WVF_N || (uint32_t)y1 >= 34u) { wvf_new(i, 1); continue; }
        wv_line(c + wvf_st[i][0] * 18 / z0, 16 + wvf_st[i][1] * 10 / z0, x1, y1, (uint32_t)clamp(14 - z1 / 20, 3, 14));
        wvf_st[i][2] = (int16_t)z1;
    }
    px = 100 - (int32_t)((wv_ph / 2u) % 220u);                         /* a planet crossing, lit from the left: dithered bands */
    if (px > -30 && px < 110) {
        int32_t y, x;
        for (y = -16; y <= 16; y++)
            for (x = -16; x <= 16; x++) {
                int32_t d = x * x + y * y, l;
                if (d > 256 || (uint32_t)(px + x) >= WVF_N || 18 + y < 0) continue;
                l = 15 - (x + 16) * 12 / 32 - ((((px + x) ^ (18 + y)) & 1) ? 1 : 0);
                wv_mem[(18 + y) * WVF_N + px + x] = (uint8_t)(16u | (uint32_t)clamp(l, 2, 15));
            }
    }
}
static UI_NOINLINE void wvf_disk(int32_t cx, int32_t cy, int32_t r, uint16_t col)   /* pixels: a filled circle */
{
    int32_t y, hw;
    for (y = -r; y <= r; y++) { hw = 0; while ((hw + 1) * (hw + 1) + y * y <= r * r) hw++; WFR(cx - hw, cy + y, 2 * hw + 1, 1, col); }
}
static UI_NOINLINE void wvf_pline(int32_t x0, int32_t y0, int32_t x1, int32_t y1, uint16_t col)   /* pixels: a line */
{
    int32_t dx = x1 > x0 ? x1 - x0 : x0 - x1, dy = -(y1 > y0 ? y1 - y0 : y0 - y1), sx = x0 < x1 ? 1 : -1, sy = y0 < y1 ? 1 : -1, e = dx + dy, n = 0;
    for (;;) {
        cv_pset(x0, y0, col);
        if ((x0 == x1 && y0 == y1) || ++n > 500) break;
        { int32_t e2 = 2 * e; if (e2 >= dy) { e += dy; x0 += sx; } if (e2 <= dx) { e += dx; y0 += sy; } }
    }
}
#define CK_B0 RGB(0x24, 0x21, 0x30)
#define CK_B1 RGB(0x34, 0x30, 0x44)
#define CK_B2 RGB(0x46, 0x41, 0x5a)
#define CK_B3 RGB(0x5e, 0x58, 0x76)
#define CK_SCR RGB(0x1a, 0x1c, 0x2a)
#define CK_INK RGB(0x9c, 0xe8, 0xd0)
#define CK_LED RGB(0xff, 0x5c, 0x5c)
#define CK_LDIM RGB(0x5a, 0x24, 0x2c)
static UI_NOINLINE void wvf_cockpit_over(void)                     /* the dashboard and the canopy over the space */
{
    int32_t y, i, k, x;
    for (y = 0; y < 132; y++) {                                        /* the canopy's struts */
        int32_t w = 14 + y * 26 / 132;
        WFR(0, y, w, 1, CK_B0), WFR(240 - w, y, w, 1, CK_B0);
        if (y < 6) WFR(0, y, 240, 1, CK_B0);
    }
    wvf_pline(14, 0, 40, 132, CK_B2), wvf_pline(225, 0, 199, 132, CK_B2);
    for (y = 96; y < 240; y++) {                                       /* the dashboard: a hump in the middle */
        int32_t x0 = y < 108 ? 92 - (y - 96) * 4 : y < 120 ? 44 - (y - 108) * 2 : 20, x1 = 240 - x0;
        if (y >= 96) WFR(x0 < 0 ? 0 : x0, y, (x1 > 240 ? 240 : x1) - (x0 < 0 ? 0 : x0), 1, y < 98 ? CK_B3 : CK_B1);
    }
    WFR(0, 120, 20, 120, CK_B0), WFR(220, 120, 20, 120, CK_B0);
    for (k = 0; k < 2; k++) {                                          /* the two screens */
        int32_t sx = k ? 140 : 32;
        WFR(sx - 3, 120, 74, 58, CK_B2), WFR(sx, 123, 68, 52, CK_SCR);
        for (i = 0; i < 4; i++) WFR(sx - 6, 126 + i * 12, 2, 4, (wv_ph / 6u + (uint32_t)i + (uint32_t)k) % 5u ? CK_LDIM : CK_LED), WFR(sx + 72, 126 + i * 12, 2, 4, CK_LDIM);
        if (!k) {                                                      /* left: the spectrum */
            for (i = 0; i < 19; i++) { int32_t h = wf_spec[i] * 44 / 24; WFR(sx + 3 + i * 3 + i / 4, 172 - h, 2, h, CK_INK); }
        } else {                                                       /* right: a radar, the notes as blips */
            int32_t rx = sx + 34, ry = 149;
            uint32_t a = wv_ph * 3u;
            for (i = 0; i < 64; i += 2) cv_pset(rx + 22 * WV_SIN[(i + 16) & 63] / 127, ry + 22 * WV_SIN[i] / 127, CK_B3);
            wvf_pline(rx - 24, ry, rx + 24, ry, CK_B2), wvf_pline(rx, ry - 24, rx, ry + 24, CK_B2);
            wvf_pline(rx, ry, rx + 22 * wv_sin4(a + 64u) / 127, ry + 22 * wv_sin4(a) / 127, CK_INK);
            for (i = 0; i < 19; i++) if (wf_spec[i] > 7) WFR(rx + (8 + wf_spec[i] / 2) * WV_SIN[((uint32_t)i * 7u + 16u) & 63u] / 127, ry + (8 + wf_spec[i] / 2) * WV_SIN[((uint32_t)i * 7u) & 63u] / 127, 2, 2, CK_LED);
        }
    }
    WFR(108, 102, 24, 13, CK_SCR);                                     /* the middle: the tempo, a gauge of the volume */
    { char b[6]; fmt_int(b, song.g[G_BPM]); wf_textc(120, 105, 0, b, CK_INK); }
    wvf_disk(120, 152, 13, CK_B2), wvf_disk(120, 152, 11, CK_SCR);
    { uint32_t a = 160u + (uint32_t)clamp(wv_all() / 3, 0, 64); wvf_pline(120, 152, 120 + 10 * wv_sin4(a + 64u) / 127, 152 + 10 * wv_sin4(a) / 127, CK_LED); }
    WFR(78, 186, 84, 54, CK_B2), WFR(81, 189, 78, 51, CK_SCR);         /* the big screen below: the wave, the sound */
    {
        int32_t py = 212, s;
        for (x = 0; x < 74; x++) {
            s = scope_buf[(scope_w - 148u + 2u * (uint32_t)x) & (SCOPE_N - 1u)];
            y = clamp(212 - s * 12 / 12000, 196, 228);
            if (x) wvf_pline(82 + x, py, 83 + x, y, CK_INK);
            py = y;
        }
        if (song.sel < NPART) { char nm[14]; trk_short_name(song.sel, nm); wf_textc(120, 231, 0, nm, CK_INK); }
    }
    for (i = 0; i < 18; i++) {                                         /* the red keys: lit by the bands */
        int32_t bx = 24 + (i % 6) * 8, by = 196 + (i / 6) * 8;
        WFR(bx, by, 6, 5, wf_spec[i] > 8 ? CK_LED : CK_LDIM), WFR(240 - bx - 6, by, 6, 5, wf_spec[18 - i] > 8 ? CK_LED : CK_LDIM);
    }
    for (x = 0; x < 3; x++) WFR(28 + x * 16, 226, 10, 4, CK_B3), WFR(170 + x * 16, 226, 10, 4, CK_B3);
    if (wvf_hit_n) {                                                   /* the lasers */
        uint16_t lc = wvf_hit_n > 3 ? WC_WHT : RGB(0xff, 0x50, 0x40);
        wvf_pline(40, 100, 120 - wvf_hit_n * 4, 46, lc), wvf_pline(200, 100, 120 + wvf_hit_n * 4, 46, lc);
        wvf_pline(41, 100, 121 - wvf_hit_n * 4, 46, lc), wvf_pline(199, 100, 119 + wvf_hit_n * 4, 46, lc);
    }
    WFR(117, 52, 7, 1, CK_LED), WFR(120, 49, 1, 7, CK_LED);             /* the sight */
}
/* HOLE: a black hole (after the rainbow one): the disc turns round it, the near half crosses in front, a ring of light over it, rays of
 * the spectrum fan out below in the rainbow; cells 1..12 the rainbow, 13..14 white, 15 a dim star */
static const uint16_t WVF_HOLE[16] = {0, RGB(0xff, 0x30, 0x40), RGB(0xff, 0x70, 0x20), RGB(0xff, 0xb0, 0x20), RGB(0xf0, 0xe8, 0x40), RGB(0x90, 0xf0, 0x40),
    RGB(0x30, 0xe0, 0x80), RGB(0x30, 0xe0, 0xe0), RGB(0x40, 0x90, 0xff), RGB(0x70, 0x60, 0xff), RGB(0xb0, 0x50, 0xf0), RGB(0xf0, 0x50, 0xd0),
    RGB(0xff, 0x60, 0x90), RGB(0xff, 0xe8, 0xc8), RGB(0xff, 0xff, 0xff), RGB(0x50, 0x58, 0x80)};
static UI_NOINLINE void wvf_hole(void)
{
    int32_t i, cx = 40, cy = 34, all = wv_all();
    uint32_t a;
    memset(wv_mem, 0, WVF_N * WVF_N);
    wvf_twinkle(26, 80, 15);
    for (i = 0; i < 19; i++) {                                         /* the rays below */
        uint32_t an = 72u + (uint32_t)i * 112u / 18u;                    /* (1/256 turn: from down-right round to down-left) */
        int32_t l = 16 + wf_spec[i] * 2;
        wv_line(cx + 14 * wv_sin4(an + 64u) / 127, cy + 6 * wv_sin4(an) / 127, cx + l * 2 * wv_sin4(an + 64u) / 127, cy + l * wv_sin4(an) / 127, 1u + (uint32_t)i * 12u / 19u);
    }
    for (a = 0; a < 256u; a += 2u) {                                    /* the disc, the far half */
        int32_t s = wv_sin4(a), r;
        if (s > 0) continue;
        for (r = 18; r <= 34; r += 4) wv_put(cx + r * wv_sin4(a + 64u) / 127, cy + r * s / 127 / 4, 1u + ((a + wv_ph * (2u + (uint32_t)all / 40u) + (uint32_t)r * 5u) / 21u) % 12u);
    }
    wv_force = 1;
    wv_disc(cx, cy, 12, 0);                                            /* the hole */
    wv_force = 0;
    for (a = 128u; a < 256u; a += 2u) wv_put(cx + 14 * wv_sin4(a + 64u) / 127, cy + 14 * wv_sin4(a) / 127, 13u + (a / 2u + wv_ph) % 2u);   /* the ring of light */
    for (a = 0; a < 128u; a += 2u) {                                   /* the near half, in front */
        int32_t r;
        for (r = 18; r <= 34; r += 4) wv_put(cx + r * wv_sin4(a + 64u) / 127, cy + r * wv_sin4(a) / 127 / 4, 1u + ((a + wv_ph * (2u + (uint32_t)all / 40u) + (uint32_t)r * 5u) / 21u) % 12u);
    }
    wv_force = 1;
    for (i = -12; i <= 12; i++) wv_put(cx + i, cy, (i & 1) ? 14u : 13u);   /* (its edge, bright) */
    wv_force = 0;
}
/* ORB: a crystal planet (after the cyan one): a sphere with a glowing rim, lightning inside, purple spikes out of it, one per band */
static UI_NOINLINE void wvf_orb(void)
{
    int32_t i, x, y, cx = 40, cy = 38, R = 17 + wv_bass() / 8;
    wv_fade(4);
    wvf_twinkle(30, 80, 16u | 12u);
    for (y = -R; y <= R; y++)                                          /* the inside: the plasma's waves, bright where they cross */
        for (x = -R; x <= R; x++) {
            int32_t d = x * x + y * y, s;
            if (d > R * R) continue;
            s = WV_SIN[((uint32_t)(x * 5) + wv_ph * 3u) & 63u] + WV_SIN[((uint32_t)(y * 4) + wv_ph * 2u) & 63u] + WV_SIN[((uint32_t)((x + y) * 3) + wv_ph * 5u) & 63u];
            wv_put(cx + x, cy + y, (uint32_t)clamp(6 + (s < 0 ? -s : s) / 40 - d * 3 / (R * R), 3, 15));
        }
    for (i = 0; i < 64; i++) wv_put(cx + (R + 1) * WV_SIN[(i + 16) & 63] / 127, cy + (R + 1) * WV_SIN[i] / 127, 15);   /* the rim */
    for (i = 0; i < 24; i++) {                                         /* the spikes */
        uint32_t an = (uint32_t)i * 256u / 24u + wv_ph / 2u;
        int32_t l = R + 2 + wf_spec[i % 19] * 3 / 4, cs = wv_sin4(an + 64u), sn = wv_sin4(an), k;
        for (k = -1; k <= 1; k++) {
            uint32_t a2 = an + (uint32_t)(k * 3);
            wv_line(cx + (R - 2) * wv_sin4(a2 + 64u) / 127, cy + (R - 2) * wv_sin4(a2) / 127, cx + (l - (k ? 3 : 0)) * cs / 127, cy + (l - (k ? 3 : 0)) * sn / 127, 16u | (k ? 10u : 14u));
        }
    }
}
/* FIRE: a ball of blue fire on a grid floor (after the magic one): flames lick up round it, lightning on the kick */
static UI_NOINLINE void wvf_fire(void)
{
    int32_t i, cx = 40, by = 61, all = wv_all();
    wv_fade(3);
    wvf_hits();
    for (i = 0; i < 9; i++) wv_line(0, 62 + i * i / 3, 79, 62 + i * i / 3, 3);   /* the floor */
    for (i = -8; i <= 8; i++) wv_line(40 + i * 3, 62, 40 + i * 12, 79, 3);
    for (i = 0; i < 34; i++) {                                         /* the flames: tongues that sway as they rise */
        int32_t x0 = cx - 17 + i + WV_SIN[((uint32_t)i * 9u + wv_ph * 3u) & 63u] / 60, h = 14 + wf_spec[i % 19] * 3 / 2 + all / 16 - (i > 17 ? i - 17 : 17 - i) * 2 / 3
                                                                         + WV_SIN[((uint32_t)i * 13u + wv_ph * 5u) & 63u] / 25, k;
        for (k = 0; k < h; k++) wv_put(x0 + WV_SIN[((uint32_t)(k * 6 + i * 11) + wv_ph * 4u) & 63u] * k / 300, by - k, (k > h * 2 / 3 ? 16u : 0u) | (uint32_t)clamp(15 - k * 10 / (h + 1), 5, 15));
    }
    wv_disc(cx, by - 4, 5, 16u | 15u);                                  /* the core */
    if (wvf_hit_n > 2) {                                               /* lightning */
        int32_t x = cx, y = by - 4, k;
        for (k = 0; k < 8; k++) { int32_t nx = x + (int32_t)(wvf_rnd() % 9u) - 4, ny = y - 4; wv_line(x, y, nx, ny, 16u | 14u); x = nx, y = ny; }
    }
}
/* SKY: a scene, as a visual novel's (after the clouds one): a sky fading from lilac to pink, clouds drifting with the tempo, a moon, stars
 * that light with the notes, a shooting star on the kick, and on the bridge someone and a cat, looking at it. Cells 1..4 the clouds
 * (shadow .. light), 5 a star, 6 the moon, 7 the bridge, 8 its rail, 9 a shooting star, 15 a dim star */
static const uint16_t WVF_SKY[16] = {0, RGB(0x98, 0x88, 0xd8), RGB(0xc8, 0x98, 0xd0), RGB(0xf0, 0xb0, 0xc8), RGB(0xff, 0xe0, 0xe0),
    RGB(0xff, 0xff, 0xf0), RGB(0xff, 0xf4, 0xc0), RGB(0x2c, 0x2a, 0x48), RGB(0x58, 0x58, 0x80), RGB(0xff, 0xff, 0xff), 0, 0, 0, 0, 0, RGB(0xc0, 0xc8, 0xff)};
static int32_t wvf_shoot[3];                  /* the shooting star: x, y, frames left */
static UI_NOINLINE void wvf_sky(void)
{
    static const int8_t CL[5][3] = {{0, 0, 7}, {7, -3, 8}, {15, 1, 6}, {-7, 2, 5}, {21, 3, 4}};   /* a cloud: puffs (dx, dy, r) */
    uint32_t drift = wv_ph * (uint32_t)clamp(song.g[G_BPM], 40, 240) / 240u;
    int32_t i, k, y, x;
    memset(wv_mem, 0, WVF_N * WVF_N);
    wvf_hits();
    for (i = 0; i < 22; i++)                                           /* the stars */
        wv_put((int32_t)(((uint32_t)i * 37u + 5u) % WVF_N), (int32_t)(((uint32_t)i * 23u + 3u) % 30u), wf_spec[i % 19] > 8 ? 5u : 15u);
    wv_disc(20, 9, 4, 6);                                              /* the moon: a crescent */
    wv_force = 1; wv_disc(22, 8, 4, 0); wv_force = 0;
    for (k = 0; k < 3; k++) {                                          /* the clouds, the far ones first */
        int32_t bx = (int32_t)((drift / (uint32_t)(3 - k) + (uint32_t)k * 47u) % 130u) - 30, byy = 44 + k * 4;
        for (i = 0; i < 5; i++) {
            int32_t r = CL[i][2] + k, cx = bx + CL[i][0] * (k + 3) / 3, cy = byy + CL[i][1];
            for (y = -r; y <= r; y++) for (x = -r; x <= r; x++)
                if (x * x + y * y <= r * r && (uint32_t)(cx + x) < WVF_N && (uint32_t)(cy + y) < 58u)
                    wv_mem[(cy + y) * WVF_N + cx + x] = (uint8_t)clamp(4 - (y + r) * 4 / (2 * r + 1) + (k == 2), 1, 4);
        }
    }
    if (wvf_hit_n == 5 && !wvf_shoot[2]) wvf_shoot[0] = 50 + (int32_t)(wvf_rnd() % 28u), wvf_shoot[1] = 2, wvf_shoot[2] = 9;
    if (wvf_shoot[2]) { wvf_shoot[2]--; wv_line(wvf_shoot[0], wvf_shoot[1], wvf_shoot[0] + 5, wvf_shoot[1] - 3, 9); wvf_shoot[0] -= 3, wvf_shoot[1] += 2; }
    for (y = 60; y < 80; y++) wv_line(0, y, 79, y, 7);                  /* the bridge */
    wv_line(0, 54, 79, 54, 8);
    for (x = 2; x < 80; x += 8) wv_line(x, 54, x, 59, 8);
}
static UI_NOINLINE void wvf_sky_back(int32_t y0, int32_t hh)        /* the sky's colour, under the cells: lilac above, pink at the horizon */
{
    int32_t y;
    for (y = y0; y < y0 + hh && y < 180; y++) {
        int32_t t = y * 255 / 180, r = 0x70 + (0xf0 - 0x70) * t / 255, g = 0x88 + (0xb0 - 0x88) * t / 255, b = 0xe8 - (0xe8 - 0xd0) * t / 255;
        WFR(0, y, 240, 1, RGB(r, g, b));
    }
}
static const char *const WSKY_MAN[16] = {"..11..", ".1111.", ".1111.", "..11..", ".1111.", "111111", "111111", "111111", ".1111.", ".1111.",
                                         ".1..1.", ".1..1.", ".1..1.", ".1..1.", ".1..1.", "11..11"};
static const char *const WSKY_CAT[6] = {"1.1...", "111...", "111...", ".111..", ".1111.", ".11111"};
static UI_NOINLINE void wvf_sky_over(void)                         /* someone and a cat on the bridge, as silhouettes */
{
    int32_t r, c;
    for (r = 0; r < 16; r++) for (c = 0; c < 6; c++) if (WSKY_MAN[r][c] == '1') WFR(170 + c * 2, 148 + r * 2, 2, 2, RGB(0x1c, 0x1a, 0x30));
    for (r = 0; r < 6; r++) for (c = 0; c < 6; c++) if (WSKY_CAT[r][c] == '1') WFR(188 + c * 2, 168 + r * 2, 2, 2, RGB(0x1c, 0x1a, 0x30));
}
static UI_NOINLINE void wvf_led(int32_t x, int32_t y, const char *s, uint16_t on)   /* MARQUEE: the 5 x 7 face as LEDs of 5 px */
{
    for (; *s; s++) {
        uint32_t c = (uint8_t)*s, w, r, b;
        if (c >= 'a' && c <= 'z') c -= 32;
        if (c < 32 || c > 95) c = '?';
        w = WFM_W[c - 32];
        if (x > -40 && x < 240)
            for (r = 0; r < WFM_H; r++)
                for (b = 0; b < w; b++)
                    if ((WFM_ROWS[c - 32][r] >> (7 - b)) & 1u) WFR(x + (int32_t)b * 5, y + (int32_t)r * 5, 4, 4, on);
        x += ((int32_t)w + 1) * 5;
    }
}
static UI_NOINLINE void wvf_foot(void)                              /* the names at the foot, the SEL knob (as the window's) */
{
    int32_t i, x = 0, xs = 0, tot, off, kx = 228, ky = 233, yy, xx;
    uint32_t a = 96u + (uint32_t)gv_mode * 192u / (GV_N - 1u);           /* 7 o'clock .. 5 o'clock */
    WFR(0, 226, 240, 14, WC_LCD);
    for (i = 0; i < GV_N; i++) { if (i == gv_mode) xs = x; x += wf_tw(1, GV_NAME[i]) + 5; }
    tot = x;
    off = clamp(xs - 80, 0, tot > 190 ? tot - 190 : 0);
    wf_clx0 = 4, wf_clx1 = 198;
    for (i = 0, x = 6 - off; i < GV_N; i++) x = wf_text(x, 230, 1, GV_NAME[i], i == gv_mode ? WC_GRN : WC_LOFF) + 5;
    wf_clx0 = 0, wf_clx1 = 240;
    wf_textr(kx - 7, 230, 1, "SEL", WC_LAB);
    for (yy = -4; yy <= 4; yy++) for (xx = -4; xx <= 4; xx++) { int32_t d2 = xx * xx + yy * yy; if (d2 <= 16) cv_pset(kx + xx, ky + yy, d2 >= 10 ? WC_BSH : WC_BTN); }
    for (yy = 0; yy <= 3; yy++) cv_pset(kx + wv_sin4(a + 64u) * yy / 127, ky + wv_sin4(a) * yy / 127, WC_AMB);
}
/* ---- the neon ones (after the line-art references): drawn in pixels, thin bright strokes, the device's own values in big digits.
 * The history they keep (WATERFALL, TERRAIN, CURVES) lives in wv_mem: these pictures have no cells */
#define NE_PUR RGB(0xb0, 0x70, 0xe8)
#define NE_GRN RGB(0x30, 0xe0, 0x80)
#define NE_BLU RGB(0x60, 0x90, 0xff)
#define NE_RED RGB(0xff, 0x40, 0x60)
#define NE_PNK RGB(0xff, 0x98, 0xa8)
#define NE_WHT RGB(0xe8, 0xe8, 0xf8)
#define NE_DIM RGB(0x40, 0x30, 0x60)
#define NE_BRN RGB(0xc8, 0x84, 0x5c)
#define NE_ORG RGB(0xff, 0x90, 0x30)
static UI_NOINLINE void ne_line(int32_t x0, int32_t y0, int32_t x1, int32_t y1, uint16_t c)   /* a 2 px stroke */
{
    int32_t dx = x1 > x0 ? x1 - x0 : x0 - x1, dy = y1 > y0 ? y1 - y0 : y0 - y1;
    wvf_pline(x0, y0, x1, y1, c);
    if (dx > dy) wvf_pline(x0, y0 + 1, x1, y1 + 1, c); else wvf_pline(x0 + 1, y0, x1 + 1, y1, c);
}
static UI_NOINLINE void ne_line3(int32_t x0, int32_t y0, int32_t x1, int32_t y1, uint16_t c)   /* a 3 px stroke (the little people) */
{
    int32_t dx = x1 > x0 ? x1 - x0 : x0 - x1, dy = y1 > y0 ? y1 - y0 : y0 - y1;
    ne_line(x0, y0, x1, y1, c);
    if (dx > dy) wvf_pline(x0, y0 - 1, x1, y1 - 1, c); else wvf_pline(x0 - 1, y0, x1 - 1, y1, c);
}
/* the figure every scene shares (FLOAT's two): a flat silhouette, a round head, a block of a body, arms and legs as strokes. x, y: its feet;
 * s: its size in eighths (8 = 64 px tall; below 0: upside down, a reflection); dir: which way it faces; p: the pose, the hands from the
 * shoulders and the feet from the hips (dx, dy at size 8) */
static UI_NOINLINE void fg_man(int32_t x, int32_t y, int32_t s, int32_t dir, const int8_t *p, uint16_t c)
{
    int32_t a = s < 0 ? -s : s, sh = y - 48 * s / 8, hp = y - 27 * s / 8, r = 6 * a / 8, k;
    wvf_disk(x, y - 58 * s / 8, r < 2 ? 2 : r, c);
    WFR(x - r, s < 0 ? y + 27 * a / 8 : y - 51 * a / 8, 2 * r, 24 * a / 8, c);
    for (k = 0; k < 4; k++) {
        int32_t bx = x + (k & 1 ? 1 : -1) * (k < 2 ? r - 1 : r / 2), by = k < 2 ? sh : hp, ex = x + dir * p[k * 2] * a / 8, ey = by + p[k * 2 + 1] * s / 8;
        if (a > 8) ne_line3(bx, by, ex, ey, c); else ne_line(bx, by, ex, ey, c);
    }
}
static UI_NOINLINE void ne_poly(const int16_t *p, int32_t n, int32_t ox, int32_t oy, int32_t sc, uint16_t c)   /* a polyline, points x, y in 1/16 px (sc 16 = 1:1) */
{
    int32_t i;
    for (i = 1; i < n; i++) ne_line(ox + p[2 * i - 2] * sc / 16, oy + p[2 * i - 1] * sc / 16, ox + p[2 * i] * sc / 16, oy + p[2 * i + 1] * sc / 16, c);
}
static UI_NOINLINE void ne_ring(int32_t cx, int32_t cy, int32_t r, uint16_t c)
{
    int32_t i, px = cx + r, py = cy;
    for (i = 1; i <= 32; i++) { int32_t x = cx + r * WV_SIN[((uint32_t)i * 2u + 16u) & 63u] / 127, y = cy + r * WV_SIN[((uint32_t)i * 2u) & 63u] / 127; ne_line(px, py, x, y, c); px = x, py = y; }
}
static UI_NOINLINE void ne_num(int32_t x, int32_t y, int32_t h, int32_t v, uint16_t c)   /* thin digits, a seven-segment cut, h tall */
{
    static const uint8_t SEG[10] = {0x3F, 0x06, 0x5B, 0x4F, 0x66, 0x6D, 0x7D, 0x07, 0x7F, 0x6F};
    char b[8];
    int32_t w = h / 2, m = h / 2, i;
    fmt_int(b, clamp(v, 0, 999));
    for (i = 0; b[i]; i++, x += w + h / 4) {
        uint32_t s = SEG[(b[i] - '0') % 10];
        if (s & 1u) ne_line(x, y, x + w, y, c);
        if (s & 2u) ne_line(x + w, y, x + w, y + m, c);
        if (s & 4u) ne_line(x + w, y + m, x + w, y + h, c);
        if (s & 8u) ne_line(x, y + h, x + w, y + h, c);
        if (s & 16u) ne_line(x, y + m, x, y + h, c);
        if (s & 32u) ne_line(x, y, x, y + m, c);
        if (s & 64u) ne_line(x, y + m, x + w, y + m, c);
    }
}
static UI_NOINLINE void ne_gauge(int32_t cx, int32_t cy, int32_t f, uint16_t c)   /* a little dial: ticks round, a needle at f 0..1024 */
{
    uint32_t a = 160u + (uint32_t)f * 64u / 1024u, t;
    for (t = 0; t <= 4u; t++) { uint32_t at = 160u + t * 16u; ne_line(cx + 11 * wv_sin4(at + 64u) / 127, cy + 11 * wv_sin4(at) / 127, cx + 14 * wv_sin4(at + 64u) / 127, cy + 14 * wv_sin4(at) / 127, NE_DIM); }
    ne_line(cx, cy, cx + 12 * wv_sin4(a + 64u) / 127, cy + 12 * wv_sin4(a) / 127, c);
    WFR(cx - 1, cy - 1, 3, 3, c);
}
static UI_NOINLINE int32_t ne_frac(int32_t v, int32_t lo, int32_t hi) { return hi > lo ? clamp((v - lo) * 1024 / (hi - lo), 0, 1024) : 0; }
static UI_NOINLINE void ne_hist(uint32_t rows, uint32_t cols)       /* the spectrum's history: row 0 the newest (wv_mem, cols bands) */
{
    uint32_t r, i;
    for (r = rows - 1u; r > 0u; r--) for (i = 0; i < cols; i++) wv_mem[r * cols + i] = wv_mem[(r - 1u) * cols + i];
    for (i = 0; i < cols; i++) wv_mem[i] = sp_s[i * 19u / cols];   /* (smoothed: it rolls, it does not spike) */
}
/* WATERFALL: the spectrum's last 16 moments as coloured lines in perspective over a grid; the level as a MIN / MAX ladder; the tempo, big */
static UI_NOINLINE void ne_waterfall(void)
{
    static const uint16_t RC[16] = {RGB(0x40, 0x70, 0xff), RGB(0x30, 0x90, 0xff), RGB(0x20, 0xb0, 0xe0), RGB(0x20, 0xc8, 0xa0), RGB(0x30, 0xe0, 0x80), RGB(0x60, 0xe8, 0x80),
        RGB(0xa0, 0xa8, 0xb8), RGB(0xc0, 0xc4, 0xd0), RGB(0xff, 0xff, 0xff), RGB(0xe0, 0xe0, 0xe8), RGB(0xff, 0x50, 0x60), RGB(0xff, 0x40, 0x58), RGB(0xf0, 0x38, 0x60),
        RGB(0xc0, 0x40, 0x90), RGB(0x90, 0x40, 0xb0), RGB(0x70, 0x40, 0xa0)};
    int32_t k, i, lv = clamp(wv_all() / 14, 0, 16);
    for (i = 0; i <= 6; i++) ne_line(40 + i * 24, 196 - i * 0, 100 + i * 24, 166, NE_DIM);   /* the floor grid */
    for (i = 0; i <= 3; i++) ne_line(40 + i * 20, 196 - i * 10, 184 + i * 20, 196 - i * 10, NE_DIM);
    for (k = 15; k >= 0; k--) {                                        /* the lines, the oldest behind */
        int32_t px = 0, py = 0;
        for (i = 0; i < 19; i++) {
            int32_t x = 52 + i * 6 + k * 6, y = 192 - k * 3 - clamp(wv_mem[(uint32_t)k * 19u + (uint32_t)i], 0, 20) * 4;
            if (i) wvf_pline(px, py, x, y, RC[15 - k]);
            px = x, py = y;
        }
    }
    wf_text(8, 40, 1, "MAX", NE_BLU), wf_text(8, 186, 1, "MIN", NE_BLU);
    for (i = 0; i < 16; i++) WFR(30, 186 - i * 9, i < 4 ? 10 : 7, 2, i < lv ? NE_GRN : NE_DIM);
    ne_num(168, 20, 34, song.g[G_BPM], NE_WHT);
    for (i = 0; i < 6; i++) WFR(10 + i * 14 + (int32_t)((wv_ph + (uint32_t)i * 3u) % 4u), 214 + (i & 1) * 3, 10, 2, NE_BLU);
}
/* TERRAIN: the spectrum's history as an orange wire landscape on a dark floor, crosses for stars behind */
static UI_NOINLINE void ne_terrain(void)
{
    int32_t k, i, x, y;
    for (i = 0; i < 26; i++) { x = (int32_t)(((uint32_t)i * 47u + 13u) % 230u) + 5, y = (int32_t)(((uint32_t)i * 29u + 7u) % 110u) + 4;
        WFR(x - 2, y, 5, 1, NE_WHT), WFR(x, y - 2, 1, 5, NE_WHT); }
    for (y = 0; y < 70; y++) {                                         /* the floor: a dark orange plane, seen at an angle */
        int32_t xa = 4 + y * 2, xb = 236 - (70 - y) * 1;
        if (xb > xa) WFR(xa, 150 + y, xb - xa, 1, (y & 1) ? RGB(0x50, 0x20, 0x08) : RGB(0x60, 0x28, 0x0c));
    }
    for (k = 15; k >= 0; k--) {                                        /* the wires: rows of the history, the newest in front */
        int32_t px = 0, py = 0;
        for (i = 0; i < 19; i++) {
            x = 10 + i * 10 + k * 6, y = 214 - k * 5 - i * 2 - wv_mem[(uint32_t)k * 19u + (uint32_t)i] * 4;
            if (i) ne_line(px, py, x, y, k < 4 ? NE_ORG : RGB(0xe0, 0x70, 0x20));
            px = x, py = y;
        }
    }
}
/* CURVES: six smooth lines drifting left, each a part of the spectrum; above, the four knobs of the effects as dials */
static UI_NOINLINE void ne_curves(void)
{
    static const uint16_t CC[6] = {RGB(0x40, 0xc8, 0xf0), RGB(0x50, 0x90, 0xf0), RGB(0x80, 0x50, 0x90), RGB(0xff, 0x90, 0x98), RGB(0xa0, 0x60, 0xe0), RGB(0xff, 0x40, 0x60)};
    static const uint16_t KC4[4] = {RGB(0x60, 0x90, 0xff), RGB(0x30, 0xe0, 0x80), RGB(0xe0, 0xe0, 0xf8), RGB(0xff, 0x40, 0x60)};
    int32_t c, x, k;
    for (k = 0; k < 4; k++) {                                          /* the knobs: KNOB 1..4 of the effects group */
        int16_t *vp;
        const param_desc_t *d = wf_sl((uint32_t)wf_grp % 3u * 4u + (uint32_t)k, &vp);
        int32_t cx = 36 + k * 56, cy = 64, f = d ? wf_frac(d, *vp) : 0;
        uint32_t a = 96u + (uint32_t)f * 192u / 1024u;
        WFR(cx - 20, cy, 6, 2, NE_DIM), WFR(cx + 14, cy, 6, 2, NE_DIM), WFR(cx, cy - 22, 2, 6, NE_DIM), WFR(cx, cy + 16, 2, 6, NE_DIM);
        ne_line(cx, cy, cx + 16 * wv_sin4(a + 64u) / 127, cy + 16 * wv_sin4(a) / 127, KC4[k]);
        wvf_disk(cx + 1, cy + 1, 4, KC4[k]);
    }
    for (c = 0; c < 6; c++) {                                          /* the lines: their history, smoothed */
        int32_t px = 0, py = 0;
        for (x = 0; x < 40; x++) {
            int32_t v = (wv_mem[(uint32_t)x * 6u + (uint32_t)c] * 2 + wv_mem[(uint32_t)(x ? x - 1 : 0) * 6u + (uint32_t)c] + wv_mem[(uint32_t)(x < 39 ? x + 1 : 39) * 6u + (uint32_t)c]) / 4;
            int32_t sx = 226 - x * 5, sy = 132 + c * 10 + 26 - v * 3;
            if (x) wvf_pline(px, py, sx, sy, CC[c]);
            px = sx, py = sy;
        }
    }
}
/* FLOW: the sound's path, a diagram: the LFO moves a wave, the effects, the tape, the four tracks' knobs into the EQ, the drive, the
 * speaker. Signals run along the lines; the warnings blink when it is loud */
static UI_NOINLINE void ne_box(int32_t x, int32_t y, int32_t w, int32_t h, const char *t, uint16_t c)
{
    ne_line(x, y, x + w, y, c), ne_line(x + w, y, x + w, y + h, c), ne_line(x + w, y + h, x, y + h, c), ne_line(x, y + h, x, y, c);
    if (t) wf_textc(x + w / 2 + 1, y + h / 2 - 3, 1, t, c);
}
static UI_NOINLINE void ne_warn(int32_t x, int32_t y, int on)
{
    uint16_t c = on ? NE_RED : NE_DIM;
    ne_line(x, y + 12, x + 7, y, c), ne_line(x + 7, y, x + 14, y + 12, c), ne_line(x + 14, y + 12, x, y + 12, c);
    WFR(x + 7, y + 4, 1, 4, c), WFR(x + 7, y + 9, 1, 1, c);
}
static UI_NOINLINE void ne_flow(void)
{
    int32_t i, k, all = wv_all(), loud = all > 150, blink = (int32_t)((wv_ph / 4u) & 1u);
    uint32_t sp = wv_ph * 2u;
    ne_box(60, 44, 34, 16, "LFO", NE_GRN);
    ne_line(64, 60, 64, 70, NE_DIM), ne_line(64, 52, 44, 52, NE_DIM), ne_line(44, 52, 44, 74, NE_DIM), ne_line(86, 60, 86, 70, NE_DIM);
    for (i = 0; i < 24; i++) cv_pset(26 + i, 82 + WV_SIN[((uint32_t)i * 8u + sp) & 63u] * 6 / 127, NE_BLU), cv_pset(26 + i, 83 + WV_SIN[((uint32_t)i * 8u + sp) & 63u] * 6 / 127, NE_BLU);
    ne_box(66, 72, 34, 18, "FX", NE_WHT);
    ne_line(104, 81, 120, 81, NE_WHT), ne_line(116, 77, 120, 81, NE_WHT), ne_line(116, 85, 120, 81, NE_WHT);
    ne_box(124, 72, 18, 18, "I", NE_RED);
    ne_warn(148, 74, loud && blink);
    for (k = 0; k < 2; k++) {                                          /* the tape: two reels turning while it plays */
        int32_t cx = 36 + k * 30, cy = 122;
        ne_ring(cx, cy, 11, NE_WHT);
        for (i = 0; i < 3; i++) { uint32_t a = (uint32_t)i * 85u + (song.playing ? wv_ph * 8u : 0u); ne_line(cx, cy, cx + 8 * wv_sin4(a + 64u) / 127, cy + 8 * wv_sin4(a) / 127, NE_WHT); }
    }
    ne_line(84, 112, 84, 132, NE_RED);
    ne_line(92, 122, 116, 122, NE_WHT), ne_line(112, 118, 116, 122, NE_WHT), ne_line(112, 126, 116, 122, NE_WHT);
    ne_box(120, 113, 32, 18, "EQ", NE_BLU);
    ne_warn(160, 115, all > 110 && blink);
    for (k = 0; k < 4; k++) {                                          /* the four tracks: their levels as knobs, the signal coming in */
        int32_t cx = 140 + k * 20, cy = 52, f = (int32_t)trk_level((uint32_t)k) * 1024 / 127;
        uint32_t a = 160u + (uint32_t)f * 64u / 1024u;
        ne_line(cx, 20 + (int32_t)((wv_ph + (uint32_t)k * 2u) % 8u), cx, 34, NE_WHT);
        ne_line(cx - 7, cy, cx - 5, cy - 5, NE_DIM), ne_line(cx + 7, cy, cx + 5, cy - 5, NE_DIM);
        ne_line(cx, cy, cx + 8 * wv_sin4(a + 64u) / 127, cy + 8 * wv_sin4(a) / 127, wf_spec[k * 5] > 6 ? NE_WHT : NE_DIM);
        ne_line(cx, cy + 4, cx, 66, NE_DIM);
    }
    ne_line(140, 66, 200, 66, NE_DIM), ne_line(170, 66, 170, 100, NE_DIM), ne_line(170, 100, 136, 100, NE_DIM), ne_line(136, 100, 136, 112, NE_DIM);
    ne_warn(214, 44, wf_spec[18] > 10 && blink);
    ne_line(136, 132, 136, 140, NE_DIM);
    ne_box(120, 140, 32, 18, "FX", NE_GRN);
    ne_box(158, 140, 46, 18, "DRIVE", NE_RED);
    ne_line(153, 149, 157, 149, NE_WHT);
    ne_warn(172, 166, wvf_hit_n > 0);
    ne_line(204, 149, 214, 149, NE_WHT), ne_line(214, 149, 214, 110, NE_WHT), ne_line(217, 149, 217, 113, NE_WHT);
    {                                                                  /* the speaker: it pushes with the bass */
        int32_t b = wv_bass() / 4;
        ne_box(208, 96, 8, 16, 0, NE_RED);
        ne_line(216, 96, 228 + b, 86, NE_RED), ne_line(216, 112, 228 + b, 122, NE_RED), ne_line(228 + b, 86, 228 + b, 122, NE_RED);
    }
    ne_box(28, 160, 34, 18, 0, NE_PUR);
    WFR(36, 166, 4, 4, NE_PUR), WFR(46, 166, 3, 3, NE_PUR), WFR(52, 166, 3, 3, NE_PUR);
    {                                                                  /* the FM-1's keys: the ones sounding light up */
        static const uint8_t BL[12] = {0, 1, 0, 1, 0, 1, 0, 0, 1, 0, 1, 0};   /* from F: which are black */
        int32_t k, wx = 0;
        ne_box(70, 156, 52, 26, 0, NE_PUR);
        for (k = 0; k < 27; k++) if (!BL[k % 12]) { WFR(73 + wx * 3, 159, 2, 20, (nk_mask >> k) & 1u ? RGB(0xff, 0xe0, 0x60) : NE_WHT); wx++; }
        for (k = 0, wx = 0; k < 27; k++) { if (!BL[k % 12]) { wx++; continue; } WFR(72 + wx * 3, 159, 2, 12, (nk_mask >> k) & 1u ? RGB(0xff, 0x60, 0xc0) : RGB(0x20, 0x10, 0x30)); }
    }
    ne_line(78, 169, 66, 169, NE_WHT), ne_line(45, 158, 45, 136, NE_WHT);
    for (i = 0; i < 4; i++) ne_ring(186 + i * 12, 22, 3, trk[i].p[P_MUTE] ? NE_DIM : NE_RED);
}
/* COW: a cow in neon lines; inside it the sound goes round (in at the mouth: a ring, a mixer, the spectrum, a square wave out), the
 * device's FREQ (the DJ filter), DELAY and FEEDBACK at its side */
static const int16_t NE_COW[] = {48, 132, 40, 118, 44, 100, 58, 86, 54, 74, 66, 82, 78, 78, 90, 86, 112, 76, 148, 72, 176, 74, 192, 80, 200, 96, 199, 118,
    194, 146, 196, 172, 191, 180, 183, 180, 184, 158, 178, 146, 150, 150, 120, 149, 116, 172, 118, 180, 110, 180, 108, 160, 104, 146, 92, 138,
    82, 128, 70, 128, 58, 132, 48, 132};
static UI_NOINLINE void ne_cow(void)
{
    int32_t i, f = ne_frac(song.g[G_FILT], GP[G_FILT].min, GP[G_FILT].max);
    ne_poly(NE_COW, (int32_t)(sizeof NE_COW / sizeof NE_COW[0] / 2), 30, 20, 16, NE_BRN);
    ne_line(196, 92, 204, 120, NE_BRN), ne_line(204, 120, 202, 146, NE_BRN), ne_line(198, 146, 206, 152, NE_BRN);   /* the tail */
    ne_line(160, 158, 162, 166, NE_BRN), ne_line(162, 166, 170, 166, NE_BRN), ne_line(170, 166, 172, 158, NE_BRN);   /* the udder */
    WFR(84, 112, 3, 3, NE_BLU);                                        /* the eye */
    for (i = 0; i < 10; i++) {                                         /* the sound going in, dashes running */
        int32_t x = 92 + i * 6, y = 128 + (i * i) / 6 - i;
        if (((uint32_t)i + wv_ph / 2u) % 3u) WFR(x, y, 3, 2, NE_BLU);
    }
    ne_ring(158, 120, 9, NE_BLU), ne_ring(158, 120, 5, NE_WHT);
    ne_line(158, 129, 158, 136, NE_BLU);
    ne_ring(158, 146, 9, NE_GRN);
    WFR(155, 146, 2, 2, NE_PNK), WFR(160, 143, 2, 2, NE_GRN), WFR(160, 149, 2, 2, NE_RED);
    for (i = 0; i < 8; i++) {                                          /* the spectrum in its belly */
        int32_t h = 3 + wf_spec[i * 2] / 3;
        WFR(176 + i * 5, 132 - h, 2, h, i & 1 ? NE_RED : NE_BLU);
        WFR(176 + i * 5, 136, 2, 2 + wf_spec[i * 2 + 1] / 4, NE_GRN);
    }
    ne_ring(196, 152, 8, NE_RED);
    for (i = 0; i < 4; i++) WFR(190, 148 + i * 3, 12, 1, NE_RED);
    for (i = 0; i < 4; i++) {                                          /* a square wave out, its height the bass */
        int32_t x = 182 + i * 8, h = 6 + wv_bass() / 3;
        ne_line(x, 112, x, 112 - h, NE_RED), ne_line(x, 112 - h, x + 4, 112 - h, NE_RED), ne_line(x + 4, 112 - h, x + 4, 112, NE_RED), ne_line(x + 4, 112, x + 8, 112, NE_RED);
    }
    wf_text(10, 20, 1, "FREQ", NE_BLU);
    ne_gauge(24, 52, f, NE_BLU);
    wf_text(10, 70, 1, "DELAY", NE_PUR);
    ne_num(12, 84, 24, TSEL->p[P_DLY], NE_GRN);
    wf_text(10, 120, 1, "FDBACK", NE_BRN);
    ne_num(12, 134, 30, song.g[G_DFDBK], NE_WHT);
    ne_box(170, 14, 62, 14, "SIDEBAND", wvf_hit_n ? NE_RED : NE_DIM);
    for (i = 0; i < 9; i++) WFR(212, 52 + i * 6, 12, 2, i < 9 - clamp(wv_all() / 25, 0, 9) ? NE_DIM : i < 4 ? NE_RED : NE_PUR);
    ne_line(32, 198, 236, 198, NE_GRN);                                /* the grass */
    for (i = 0; i < 18; i++) WFR(36 + i * 11, 194 - (wf_spec[i] > 10 ? 4 : 0), 2, 4 + (wf_spec[i] > 10 ? 4 : 0), NE_GRN);
}
/* BOXER: a boxer in neon lines, the right glove punches on the kick; FREQ, PUNCH (the bass), ROUNDS (the bars), POWER (the level) */
static const int16_t NE_BOXT[] = {110, 84, 92, 94, 86, 112, 82, 128, 92, 150, 140, 150, 148, 124, 152, 104, 144, 94, 126, 84};
static int32_t ne_rounds;
static UI_NOINLINE void ne_boxer(void)
{
    int32_t i, f = ne_frac(song.g[G_FILT], GP[G_FILT].min, GP[G_FILT].max), pu = wvf_hit_n * 3, hx = 120, hy = 62;
    if (wvf_hit_n == 5) ne_rounds = (ne_rounds + 1) % 100;
    for (i = 0; i <= 32; i++) {                                        /* the head */
        int32_t x0 = hx + 12 * WV_SIN[((uint32_t)i * 2u + 16u) & 63u] / 127, y0 = hy + 15 * WV_SIN[((uint32_t)i * 2u) & 63u] / 127;
        int32_t x1 = hx + 12 * WV_SIN[((uint32_t)i * 2u + 18u) & 63u] / 127, y1 = hy + 15 * WV_SIN[((uint32_t)i * 2u + 2u) & 63u] / 127;
        ne_line(x0, y0, x1, y1, NE_GRN);
    }
    ne_line(114, 60, 117, 60, NE_GRN), ne_line(123, 60, 126, 60, NE_GRN), ne_line(116, 69, 124, 69, NE_GRN);
    ne_poly(NE_BOXT, (int32_t)(sizeof NE_BOXT / sizeof NE_BOXT[0] / 2), 0, 0, 16, NE_GRN);
    ne_line(112, 76, 110, 84, NE_GRN), ne_line(128, 76, 130, 84, NE_GRN);
    ne_line(104, 104, 118, 112, NE_GRN), ne_line(136, 104, 122, 112, NE_GRN), ne_line(110, 124, 130, 124, NE_GRN), ne_line(110, 136, 130, 136, NE_GRN);
    ne_line(86, 112, 78, 132, NE_GRN), ne_line(78, 132, 92, 120, NE_GRN);   /* the guard arm */
    ne_ring(98, 112, 11, NE_RED), ne_line(94, 108, 100, 104, NE_RED);
    ne_line(150, 108, 162 + pu, 124 - pu / 2, NE_GRN);                 /* the punching arm */
    ne_ring(170 + pu, 128 - pu / 2, 13 + pu / 3, NE_RED), ne_line(162 + pu, 124 - pu / 2, 170 + pu, 118 - pu / 2, NE_RED);
    if (wvf_hit_n) for (i = 0; i < 5; i++) { uint32_t a = 176u + (uint32_t)i * 10u; ne_line(hx + 22 * wv_sin4(a + 64u) / 127, hy - 6 + 22 * wv_sin4(a) / 127, hx + 30 * wv_sin4(a + 64u) / 127, hy - 6 + 30 * wv_sin4(a) / 127, NE_GRN); }
    for (i = 0; i < 10; i++) WFR(146, 40 + i * 5, 8, 2, i < 10 - clamp(wv_all() / 22, 0, 10) ? NE_DIM : NE_GRN);
    ne_box(184, 168, 22, 14, "FX", NE_PUR);
    wf_text(10, 22, 1, "FREQ", NE_PUR), ne_gauge(26, 56, f, NE_GRN);
    ne_line(40, 32, 92, 32, NE_PUR), ne_line(92, 32, 108, 44, NE_PUR);
    wf_text(10, 96, 1, "PUNCH", NE_PUR), ne_num(12, 112, 34, clamp(wv_bass() * 4, 0, 99), NE_GRN);
    ne_line(52, 150, 104, 150, NE_PUR), ne_line(104, 150, 112, 140, NE_PUR);
    wf_text(178, 22, 1, "ROUNDS", NE_GRN), ne_num(184, 38, 34, ne_rounds, NE_WHT);
    wf_text(184, 96, 1, "POWER", NE_RED), ne_num(184, 112, 34, clamp(wv_all() / 4, 0, 99), NE_RED);
}

/* HIGHWAY: a road at night in a long exposure (after the light-trails reference): streaks of light come from the horizon and rush past,
 * faster and more of them with the sound; red tail lights going away */
static uint8_t hw_p[30][2];                   /* a light: its lane, how near (0 far .. 255 past) */
static UI_NOINLINE void hw_at(uint32_t lane, int32_t d, int32_t *x, int32_t *y)
{
    int32_t tx = -40 + (int32_t)lane * 18, ty = 96 + (lane > 5u ? 10 : 0), f = d * d / 255;
    *x = 52 + (tx - 52) * f / 255, *y = 22 + (ty - 22) * f / 255;
}
static UI_NOINLINE void sc_highway(void)
{
    int32_t i, all = wv_all(), x0, y0, x1, y1;
    wv_fade(2);
    wv_line(52, 22, -10, 90, 3), wv_line(52, 22, 90, 60, 3);
    for (i = 0; i < 30; i++) {
        uint8_t *q = hw_p[i];
        int32_t d0 = q[1], d1 = d0 + 3 + d0 / 14 + all / 30, back = i >= 24;
        if (i >= 12 + all / 12 && !back) { q[1] = 0; continue; }   /* (more lights with more sound) */
        if (back) d1 = d0 - 2 - d0 / 30;
        if (d1 > 255 || d1 < 0 || !q[0]) { q[0] = (uint8_t)(1u + wvf_rnd() % 7u), q[1] = (uint8_t)(back ? 120u + wvf_rnd() % 100u : wvf_rnd() % 30u); continue; }
        hw_at(q[0], d0, &x0, &y0), hw_at(q[0], d1, &x1, &y1);
        wv_line(x0, y0, x1, y1, back ? 16u | 9u : (uint32_t)clamp(6 + d1 / 28, 6, 15));
        q[1] = (uint8_t)d1;
    }
}
static UI_NOINLINE void sc_rect(int32_t x, int32_t y, int32_t w, int32_t h, uint32_t v);
/* VORTEX: rings of points flowing out of a centre off to one side (after the dotted reference), wine near it, navy, green, blue far off
 * where they turn into little crosses; a yellow square by the centre jumps on the kick. Cells: 1..4 wine, 5..8 navy, 9..12 green, 13..15 blue */
static const uint16_t SC_VORTEX[16] = {0, RGB(0x80, 0x28, 0x50), RGB(0x80, 0x28, 0x50), RGB(0x80, 0x28, 0x50), RGB(0x80, 0x28, 0x50),
    RGB(0x26, 0x24, 0x60), RGB(0x26, 0x24, 0x60), RGB(0x26, 0x24, 0x60), RGB(0x26, 0x24, 0x60), RGB(0x30, 0x90, 0x50), RGB(0x30, 0x90, 0x50),
    RGB(0x30, 0x90, 0x50), RGB(0x30, 0x90, 0x50), RGB(0x60, 0xb0, 0xff), RGB(0x60, 0xb0, 0xff), RGB(0xff, 0xf0, 0x10)};
static UI_NOINLINE void sc_vortex(void)
{
    static uint32_t flow;
    int32_t r, k, cx = 26, cy = 28;
    memset(wv_mem, 0, WVF_N * WVF_N);
    wvf_hits();
    flow += 1u + (uint32_t)wv_all() / 50u;
    for (r = 0; r < 14; r++) {
        int32_t rad = 4 + r * 5 + (int32_t)(flow % 5u), n = 10 + rad / 2;
        uint32_t v = rad < 16 ? 2u : rad < 30 ? 6u : rad < 48 ? 10u : 14u;
        for (k = 0; k < n; k++) {
            uint32_t a = (uint32_t)k * 256u / (uint32_t)n + (uint32_t)rad + flow / 2u;
            int32_t x = cx + rad * wv_sin4(a + 64u) / 127, y = cy + rad * wv_sin4(a) / 127;
            if (wf_spec[k % 19] < 3 && rad > 40) continue;
            wv_put(x, y, v);
            if (rad >= 30) wv_put(x - 1, y, v), wv_put(x + 1, y, v), wv_put(x, y - 1, v), wv_put(x, y + 1, v);
        }
    }
    sc_rect(34, 38 - (wvf_hit_n > 2 ? 3 : 0), 8, 8, 15);
}
static UI_NOINLINE void wf_big(int32_t x, int32_t y, int32_t z, const char *s, uint16_t col);
/* NEO: a neon city at night (after the anime ones): towers in two layers with windows the bands light, searchlights sweeping, green street
 * lamps; on the kick a red motorbike tears across leaving a long red light trail */
static int32_t neo_bike = -1000;
static UI_NOINLINE void ne_neo(void)
{
    int32_t i, r, c;
    for (i = 0; i < 46; i++) WFR(0, i * 4, 240, 4, RGB(0x06 + i / 3, 0x18 + i / 2, 0x26 + i / 2));
    for (i = 0; i < 3; i++) { int32_t bx = 50 + i * 70; wvf_pline(bx, 170, bx + WV_SIN[(wv_ph + (uint32_t)i * 20u) & 63u] * 60 / 127, 0, RGB(0x70, 0x80, 0x48)); }
    for (i = 0; i < 12; i++) {                                         /* the far towers */
        int32_t x = i * 21 - 4, h = 70 + (int32_t)(((uint32_t)i * 37u) % 60u), t = 170 - h;
        WFR(x, t, 19, h, RGB(0x10, 0x2c, 0x44));
        for (r = 0; r < h / 6 - 1; r++) for (c = 0; c < 4; c++)
            if (wf_spec[(i + r + c) % 19] > 7 && ((i * 7 + r * 3 + c) % 5) < 3) WFR(x + 2 + c * 4, t + 4 + r * 6, 2, 3, (i + r) % 3 == 0 ? RGB(0xff, 0x60, 0xd0) : (i + r) % 3 == 1 ? RGB(0x60, 0xf0, 0xff) : RGB(0xff, 0xe0, 0x80));
    }
    for (i = 0; i < 6; i++) {                                          /* the near ones */
        int32_t x = i * 44 - 10, h = 50 + (int32_t)(((uint32_t)i * 53u) % 40u), t = 182 - h;
        WFR(x, t, 34, h, RGB(0x06, 0x12, 0x1e));
        for (r = 0; r < h / 7 - 1; r++) for (c = 0; c < 6; c++) if (((i * 11 + r * 5 + c * 3 + (int32_t)(wv_ph / 20u)) % 7) < 2) WFR(x + 3 + c * 5, t + 4 + r * 7, 3, 3, RGB(0xff, 0xd8, 0x70));
    }
    WFR(0, 182, 240, 58, RGB(0x08, 0x0a, 0x10));
    for (i = 0; i < 8; i++) { wvf_disk(14 + i * 30, 178, 5, RGB(0x14, 0x40, 0x28)); wvf_disk(14 + i * 30, 178, 2, RGB(0x80, 0xff, 0xa0)); }
    if (neo_bike > -1000) {                                            /* the bike and its trail */
        int32_t x = neo_bike, y = 206;
        for (i = 0; i < 3; i++) WFR(x - 140, y - 3 + i * 2, 140, 2, i == 1 ? RGB(0xff, 0x70, 0x40) : RGB(0xd0, 0x20, 0x18));
        for (i = 0; i < 40; i++) cv_pset(x - 140 - i * 3, y - 1, RGB(0x80, 0x10, 0x10));
        wvf_disk(x, y, 9, RGB(0xd0, 0x18, 0x18)), wvf_disk(x - 12, y + 4, 6, RGB(0x20, 0x08, 0x08)), wvf_disk(x + 10, y + 4, 6, RGB(0x20, 0x08, 0x08));
        wvf_disk(x + 12, y - 4, 3, RGB(0xff, 0xf0, 0x80));
    }
}
/* TARGET: a sight in hexagons (after the "global target" one): yellow and blue frames, a red one whose edge is the spectrum, a magenta box
 * turning round a blue geodesic ball, three red arms; the tempo in the corner */
static UI_NOINLINE void ne_hexagon(int32_t cx, int32_t cy, int32_t r, uint16_t col, int dash, int spec)
{
    int32_t k, j;
    for (k = 0; k < 6; k++) {
        uint32_t a0 = (uint32_t)k * 256u / 6u, a1 = (uint32_t)(k + 1) * 256u / 6u;
        int32_t x0 = cx + r * wv_sin4(a0 + 64u) / 127, y0 = cy + r * wv_sin4(a0) / 127, x1 = cx + r * wv_sin4(a1 + 64u) / 127, y1 = cy + r * wv_sin4(a1) / 127;
        if (!dash && !spec) { ne_line(x0, y0, x1, y1, col); continue; }
        for (j = 0; j < 20; j++) {
            int32_t ax = x0 + (x1 - x0) * j / 20, ay = y0 + (y1 - y0) * j / 20, bx = x0 + (x1 - x0) * (j + 1) / 20, by = y0 + (y1 - y0) * (j + 1) / 20;
            if (dash && !(j & 1)) wvf_pline(ax, ay, bx, by, col);
            if (spec) { int32_t l = 2 + wf_spec[(k * 20 + j) % 19] / 2; wvf_pline(ax, ay, ax + (ax - cx) * l / r, ay + (ay - cy) * l / r, col); }
        }
    }
}
static UI_NOINLINE void ne_target(void)
{
    static const int8_t V[8][3] = {{-1, -1, -1}, {1, -1, -1}, {1, 1, -1}, {-1, 1, -1}, {-1, -1, 1}, {1, -1, 1}, {1, 1, 1}, {-1, 1, 1}};
    static const uint8_t E[12][2] = {{0, 1}, {1, 2}, {2, 3}, {3, 0}, {4, 5}, {5, 6}, {6, 7}, {7, 4}, {0, 4}, {1, 5}, {2, 6}, {3, 7}};
    int32_t i, px[8], py[8], cx = 120, cy = 124;
    uint32_t a = wv_ph * 2u;
    int32_t c = wv_sin4(a + 64u), sn = wv_sin4(a);
    char b[20];
    WFR(0, 0, 240, 240, RGB(0x1c, 0x10, 0x18));
    ne_hexagon(cx + 6, cy + 8, 100, RGB(0x40, 0x40, 0xd0), 0, 0);
    ne_hexagon(cx, cy, 98, RGB(0xf0, 0xc8, 0x30), 1, 0);
    ne_hexagon(cx - 2, cy, 86, RGB(0xff, 0x30, 0x20), 0, 1);
    for (i = 0; i < 8; i++) {
        int32_t x = V[i][0] * c - V[i][2] * sn, z = V[i][0] * sn + V[i][2] * c, y = V[i][1] * 127, y2 = (y * 110 - z * 60) / 127, z2 = (y * 60 + z * 110) / 127;
        px[i] = cx + x * 52 / (127 + z2 / 5), py[i] = cy + y2 * 52 / (127 + z2 / 5);
    }
    for (i = 0; i < 12; i++) wvf_pline(px[E[i][0]], py[E[i][0]], px[E[i][1]], py[E[i][1]], RGB(0xe0, 0x40, 0xd0));
    ne_ring(cx, cy, 30 + wv_bass() / 8, RGB(0x40, 0x50, 0xf0));
    for (i = 0; i < 8; i++) {                                          /* the ball's triangles */
        uint32_t a0 = (uint32_t)i * 32u + a / 2u;
        wvf_pline(cx + 30 * wv_sin4(a0 + 64u) / 127, cy + 30 * wv_sin4(a0) / 127, cx + 14 * wv_sin4(a0 + 80u) / 127, cy + 14 * wv_sin4(a0 + 16u) / 127, RGB(0x40, 0x50, 0xf0));
        wvf_pline(cx + 14 * wv_sin4(a0 + 80u) / 127, cy + 14 * wv_sin4(a0 + 16u) / 127, cx + 30 * wv_sin4(a0 + 96u) / 127, cy + 30 * wv_sin4(a0 + 32u) / 127, RGB(0x40, 0x50, 0xf0));
    }
    for (i = 0; i < 3; i++) {                                          /* the three arms */
        uint32_t an = a + (uint32_t)i * 85u;
        int32_t ex = cx + 34 * wv_sin4(an + 64u) / 127, ey = cy + 34 * wv_sin4(an) / 127;
        ne_line(cx, cy, ex, ey, RGB(0xff, 0x30, 0x20)), ne_line(cx + 3, cy + 2, ex + 3, ey + 2, RGB(0xc0, 0x20, 0x10));
    }
    wf_textc(120, 8, 1, "I GLOBAL TARGET I", RGB(0xf0, 0xc8, 0x30));
    str_cpy(b, "X ", 20), fmt_int(b + 2, song.g[G_BPM]), str_cpy(b + str_len(b), ".20272", 20 - str_len(b));
    wf_text(70, 224, 1, b, RGB(0xf0, 0xc8, 0x30));
    wf_text(196, 60, 1, "WFM", RGB(0xf0, 0xc8, 0x30)), wf_text(196, 70, 1, "V0.12", RGB(0xf0, 0xc8, 0x30));
}
/* the DANCE's strokes: the few signs its people are made of, drawn (the 5 x 7 face has no brackets nor backslash) */
static UI_NOINLINE void dn_char(int32_t x, int32_t y, int32_t z, char ch, uint16_t col)   /* z: 4 = 8 x 12 */
{
#define DQ(v) ((v) * z / 4)
    switch (ch) {
    case 'O': wvf_disk(x + DQ(4), y + DQ(6), DQ(5), col); wvf_disk(x + DQ(4), y + DQ(6), DQ(5) - 3, C_BLACK); break;
    case '/': ne_line3(x + DQ(7), y, x, y + DQ(12), col); break;
    case '\\': ne_line3(x, y, x + DQ(7), y + DQ(12), col); break;
    case 'I': ne_line3(x + DQ(4), y, x + DQ(4), y + DQ(12), col); break;
    case '(': ne_line3(x + DQ(6), y, x + DQ(2), y + DQ(6), col), ne_line3(x + DQ(2), y + DQ(6), x + DQ(6), y + DQ(12), col); break;
    case ')': ne_line3(x + DQ(2), y, x + DQ(6), y + DQ(6), col), ne_line3(x + DQ(6), y + DQ(6), x + DQ(2), y + DQ(12), col); break;
    case '<': ne_line3(x + DQ(7), y + DQ(2), x + DQ(1), y + DQ(6), col), ne_line3(x + DQ(1), y + DQ(6), x + DQ(7), y + DQ(10), col); break;
    case '>': ne_line3(x + DQ(1), y + DQ(2), x + DQ(7), y + DQ(6), col), ne_line3(x + DQ(7), y + DQ(6), x + DQ(1), y + DQ(10), col); break;
    case '-': ne_line3(x, y + DQ(6), x + DQ(8), y + DQ(6), col); break;
    case 'V': ne_line3(x, y, x + DQ(4), y + DQ(12), col), ne_line3(x + DQ(4), y + DQ(12), x + DQ(8), y, col); break;
    case 'X': ne_line3(x, y, x + DQ(8), y + DQ(12), col), ne_line3(x + DQ(8), y, x, y + DQ(12), col); break;
    case '7': ne_line3(x, y, x + DQ(7), y, col), ne_line3(x + DQ(7), y, x + DQ(3), y + DQ(12), col); break;
    case '*': ne_line3(x + DQ(4), y + DQ(1), x + DQ(4), y + DQ(11), col), ne_line3(x, y + DQ(6), x + DQ(8), y + DQ(6), col),
              ne_line(x + DQ(1), y + DQ(3), x + DQ(7), y + DQ(9), col), ne_line(x + DQ(7), y + DQ(3), x + DQ(1), y + DQ(9), col); break;   /* (a clap) */
    default: break;
    }
#undef DQ
}
/* RACER: an 8-bit night drive (after the racing-game ones): the road in pseudo-3D, its dashes running at the tempo, bending with the
 * melody; a city of dotted windows and a full moon ahead; posts going by; our red car from behind, its tail lights on the kick; and the
 * game's purple dashboard with the device's numbers (TIME the play time, KM/H the tempo, SCORE the notes, the rev bar the level, S..G the loop) */
static int32_t rc_curve, rc_phase, rc_score, rc_rival = -1;
static const char *const RC_CAR[12] = {"....111111111111....", "...12222222222221...", "..1222222222222221..", ".111111111111111111.",
                                       "11333311111111333311", "13444431111113444431", "13444431111113444431", "11333311111111333311",
                                       "11111111111111111111", ".55.............55..", ".55.............55..", "...................."};
static UI_NOINLINE void rc_step(void)
{
    int32_t hi = 0, lo = 0, i;
    for (i = 0; i < 9; i++) lo += wf_spec[i];
    for (i = 10; i < 19; i++) hi += wf_spec[i];
    rc_curve += (clamp((hi - lo) / 3, -40, 40) - rc_curve) / 8;
    rc_phase += 2 + clamp(song.g[G_BPM], 40, 240) / 30;
    rc_score += wv_all() / 20;
    wvf_hits();
    if (wvf_hit_n == 5 && rc_rival < 0) rc_rival = 0;
    if (rc_rival >= 0 && ++rc_rival > 60) rc_rival = -1;
}
static UI_NOINLINE void ne_racer(void)
{
    int32_t y, i, yh = 124, cxh = 120 + rc_curve;
    char b[16];
    wvf_disk(150, 54, 15, RGB(0xe8, 0xe8, 0xe8));                     /* the moon */
    for (i = 0; i < 9; i++) cv_pset(142 + (int32_t)(((uint32_t)i * 7u) % 18u), 46 + (int32_t)(((uint32_t)i * 5u) % 16u), RGB(0xb0, 0xb0, 0xb0)), cv_pset(143 + (int32_t)(((uint32_t)i * 7u) % 18u), 47 + (int32_t)(((uint32_t)i * 5u) % 16u), RGB(0xb0, 0xb0, 0xb0));
    for (i = 0; i < 30; i++) {                                         /* the city: towers of dotted windows */
        int32_t x = i * 8 + (i * 13) % 5 - 4 + rc_curve / 4, h = 12 + (int32_t)(((uint32_t)i * 53u) % 30u) + (i == 7 ? 26 : 0), r, c;
        if (i == 13 || i == 14 || i == 15) continue;
        for (r = 0; r < h; r += 3) for (c = 0; c < 6; c += 2)
            if (((i * 7 + r + c * 3 + (int32_t)(wv_ph / 8u)) % 5) < 3 || wf_spec[(i + r) % 19] > 10)
                WFR(x + c, yh - 4 - h + r, 1, 2, (r / 3 + i) % 4 == 0 ? RGB(0xf8, 0xd8, 0x90) : (r / 3 + i) % 4 == 1 ? RGB(0xf0, 0xf0, 0xf0) : RGB(0x40, 0x40, 0xd8));
    }
    for (i = 0; i < 30; i++) WFR(i * 8 + 2, yh - 3, 3, 2, (i % 3) ? RGB(0x40, 0x40, 0xd8) : RGB(0xf8, 0xd0, 0x80));
    for (y = yh; y < 192; y++) {                                       /* the road, row by row */
        int32_t d = y - yh, z = 4000 / (d + 2), w = d * 7 / 4 + 4, cx = cxh - rc_curve * d * d / 4600, dash = ((z + rc_phase) / 18) & 1;
        if (dash) {
            int32_t dw = 1 + d / 14;
            WFR(cx - dw / 2, y, dw, 1, RGB(0xf8, 0xd8, 0x60));
            WFR(cx - w * 2 / 3 - dw / 2, y, dw, 1, RGB(0xf8, 0x90, 0x30)), WFR(cx + w * 2 / 3 - dw / 2, y, dw, 1, RGB(0xf8, 0x90, 0x30));
            WFR(cx - w - 2 - d / 4, y, 2 + d / 6, 1, RGB(0xf8, 0x90, 0x30)), WFR(cx + w + 2, y, 2 + d / 6, 1, RGB(0xf8, 0x90, 0x30));
        }
        if (((z + rc_phase) % 220) < 4 && d > 6) {                     /* a post going by */
            int32_t px = cx - w - 14 - d / 3, ph = d / 2 + 6;
            WFR(px, y - ph, 2, ph, RGB(0x60, 0xe8, 0xc0)), WFR(px, y - ph, 6 + d / 10, 2, RGB(0x60, 0xe8, 0xc0));
        }
    }
    if (rc_rival >= 0) {                                               /* a car ahead, coming nearer then gone */
        int32_t d = 4 + rc_rival, y = yh + d, cx = cxh - rc_curve * d * d / 4600 + d / 2, s = 2 + d / 8;
        WFR(cx - s * 2, y - s, s * 4, s, RGB(0x30, 0xd0, 0x60)), WFR(cx - s * 2, y, s, s / 2 + 1, RGB(0xf0, 0xf0, 0xf0)), WFR(cx + s, y, s, s / 2 + 1, RGB(0xf0, 0xf0, 0xf0));
    }
    {                                                                  /* our car */
        int32_t r, c, x0 = 100 - rc_curve / 3, y0 = 162 + (int32_t)((wv_ph / 2u) & 1u);
        for (r = 0; r < 12; r++) for (c = 0; c < 20; c++) {
            char ch = RC_CAR[r][c];
            uint16_t col = ch == '1' ? RGB(0xd8, 0x20, 0x20) : ch == '2' ? RGB(0x20, 0x08, 0x08) : ch == '3' ? RGB(0x60, 0x08, 0x08) : ch == '4' ? (wvf_hit_n ? RGB(0xff, 0xff, 0xc0) : RGB(0xff, 0x60, 0x40)) : ch == '5' ? RGB(0x18, 0x18, 0x18) : 0;
            if (ch != '.') WFR(x0 + c * 2, y0 + r * 2, 2, 2, col);
        }
    }
    WFR(0, 192, 240, 48, RGB(0x50, 0x18, 0xc0));                       /* the dashboard */
    WFR(0, 192, 240, 2, RGB(0x90, 0x60, 0xf0)), WFR(0, 236, 240, 4, RGB(0x30, 0x0c, 0x80));
    for (i = 0; i < 16; i++) WFR(72 + i * 6, 196, 5, 4, i < clamp(wv_all() / 14, 0, 16) ? (i < 10 ? RGB(0x40, 0xe0, 0x40) : i < 13 ? RGB(0xf0, 0xf0, 0xf0) : RGB(0xf0, 0x30, 0x30)) : RGB(0x30, 0x10, 0x70));
    WFR(8, 204, 44, 12, C_BLACK), wf_textc(30, 207, 1, "TIME", RGB(0x60, 0xf0, 0x90));
    WFR(80, 204, 80, 12, C_BLACK), fmt_int(b, song.g[G_BPM] * 2), str_cpy(b + str_len(b), " KM/H", 16 - str_len(b)), wf_textc(120, 207, 1, b, RGB(0x60, 0xf0, 0x90));
    WFR(184, 204, 50, 12, C_BLACK), wf_textc(209, 207, 1, "SCORE", RGB(0x60, 0xf0, 0x90));
    WFR(8, 220, 44, 12, C_BLACK), fmt_int(b, (int32_t)(wf_secs() % 1000u)), wf_textc(30, 223, 1, b, RGB(0x60, 0xf0, 0x90));
    WFR(64, 220, 112, 12, C_BLACK), wf_text(70, 223, 1, "S", RGB(0x60, 0xf0, 0x90)), wf_text(166, 223, 1, "G", RGB(0x60, 0xf0, 0x90));
    {
        int32_t n = song.playing ? (int32_t)(TSEL->seq_idx % trk_len(TSEL)) * 12 / (int32_t)trk_len(TSEL) + 1 : 0;
        for (i = 0; i < 12; i++) { int32_t ax = 80 + i * 7; uint16_t cc = i < n ? RGB(0x60, 0xf0, 0x90) : RGB(0x30, 0x10, 0x70); WFR(ax, 224, 2, 5, cc), WFR(ax + 2, 225, 2, 3, cc), WFR(ax + 4, 226, 1, 1, cc); }
    }
    WFR(184, 220, 50, 12, C_BLACK), fmt_int(b, rc_score % 100000), wf_textc(209, 223, 1, b, RGB(0x60, 0xf0, 0x90));
}
/* ROOM: someone standing in a dark room whose walls are data (after the installation reference): white strips and dashes run along the
 * back wall, each row at its speed, denser and brighter where its band sounds; the floor mirrors them in perspective */
static UI_NOINLINE void ne_room(void)
{
    int32_t r, k;
    for (r = 0; r < 34; r++) {                                         /* the wall */
        int32_t y = 30 + r * 3, band = r % 19, n = 2 + wf_spec[band] / 3, sp = 1 + (r * 7) % 5;
        uint16_t col = wf_spec[band] > 10 ? WC_WHT : RGB(0x90, 0x90, 0x98);
        for (k = 0; k < n; k++) {
            int32_t len = 2 + (int32_t)(((uint32_t)(r * 31 + k * 17)) % 24u), x = (int32_t)(((uint32_t)(r * 53 + k * 89) + wv_ph * (uint32_t)sp) % 260u) - 10;
            WFR(x, y, len, 1 + (k & 1), col);
        }
    }
    for (r = 0; r < 26; r++) {                                         /* the floor: the same, squeezed, darker */
        int32_t y = 134 + r * r / 6 + r, band = (33 - r) % 19, n = 1 + wf_spec[band] / 4;
        for (k = 0; k < n; k++) {
            int32_t len = (2 + (int32_t)(((uint32_t)(r * 29 + k * 13)) % 30u)) * (r + 4) / 10, x = (int32_t)(((uint32_t)(r * 61 + k * 97) + wv_ph * (uint32_t)(2 + r / 4)) % 280u) - 20;
            WFR(x, y, len, 1 + r / 9, wf_spec[band] > 10 ? RGB(0xc0, 0xc0, 0xc8) : RGB(0x50, 0x50, 0x58));
        }
    }
    WFR(0, 133, 240, 1, RGB(0x30, 0x30, 0x34));
    {                                                                  /* a light under the feet, bent by what runs past */
        int32_t r, w2 = 16 + (int32_t)sp_s[3] / 2 + WV_SIN[(wv_ph * 2u) & 63u] / 40, l;
        for (r = 0; r < 6; r++) { l = 0x50 - r * 10 + (int32_t)sp_s[(uint32_t)r * 3u] * 2; WFR(120 - w2 + r * 2 + WV_SIN[(wv_ph * 3u + (uint32_t)r * 9u) & 63u] / 50, 140 + r, 2 * (w2 - r * 2), 1, RGB(l, l, l + 8)); }
    }
    wvf_disk(120, 98, 6, RGB(0x46, 0x46, 0x52)), WFR(112, 103, 16, 24, RGB(0x46, 0x46, 0x52)), WFR(114, 125, 12, 18, RGB(0x46, 0x46, 0x52));   /* (a faint rim: the figure reads on the dark) */
    WFR(110, 105, 20, 18, RGB(0x46, 0x46, 0x52));
    wvf_disk(120, 98, 5, RGB(0x06, 0x06, 0x08));
    WFR(113, 104, 14, 22, RGB(0x06, 0x06, 0x08)), WFR(115, 126, 4, 16, RGB(0x06, 0x06, 0x08)), WFR(121, 126, 4, 16, RGB(0x06, 0x06, 0x08));
    WFR(111, 106, 3, 16, RGB(0x06, 0x06, 0x08)), WFR(126, 106, 3, 16, RGB(0x06, 0x06, 0x08));
    WFR(112, 142, 18, 8, RGB(0x10, 0x10, 0x14));
}
/* SUNSET: RACER's second road: a striped sunset, purple mountains, pines going by, the purple tarmac, and the green dashboard (the loops
 * as LAP times, MPH the tempo, POWER the level) */
static UI_NOINLINE void ne_sunset(void)
{
    static const uint16_t SKY[8] = {RGB(0x30, 0x10, 0x60), RGB(0x50, 0x18, 0x80), RGB(0x80, 0x20, 0x90), RGB(0xb0, 0x30, 0x90), RGB(0xe0, 0x50, 0x80),
                                    RGB(0xf0, 0x70, 0x70), RGB(0xf8, 0x90, 0x60), RGB(0xf8, 0xa8, 0x70)};
    int32_t y, i, yh = 120, cxh = 120 + rc_curve;
    char b[16];
    for (y = 0; y < 100; y++) WFR(0, y, 240, 1, ((y / 3) & 1) && y % 12 < 9 ? SKY[y * 8 / 100] : SKY[y * 8 / 100 > 0 ? y * 8 / 100 - 1 : 0]);
    for (i = 0; i < 240; i += 2) {                                     /* mountains */
        int32_t h = 10 + (WV_SIN[((uint32_t)i / 3u + 5u) & 63u] + WV_SIN[((uint32_t)i + 20u) & 63u]) / 20 + (i % 37 < 4 ? 3 : 0);
        WFR(i, 100 - h + 4, 2, h + 2, RGB(0x20, 0x08, 0x40)), WFR(i, 100 - h + 4, 2, 2, RGB(0xa0, 0x40, 0xd0));
    }
    WFR(0, 104, 240, 16, RGB(0x30, 0x20, 0x90));
    for (i = 0; i < 6; i++) WFR(0, 106 + i * 2, 240, 1, RGB(0x50, 0x40, 0xc0));
    for (y = yh; y < 186; y++) {
        int32_t d = y - yh, z = 4000 / (d + 2), w = d * 2 + 6, cx = cxh - rc_curve * d * d / 4600;
        WFR(cx - w, y, 2 * w, 1, RGB(0x70, 0x30, 0xe0));
        if (((z + rc_phase) / 18) & 1) { int32_t dw = 1 + d / 12; WFR(cx - w / 3 - dw / 2, y, dw, 1, WC_WHT), WFR(cx + w / 3 - dw / 2, y, dw, 1, WC_WHT); }
        if (((z + rc_phase) % 260) < 4 && d > 6) {                     /* a pine going by */
            int32_t px = cx - w - 10 - d / 4, ph = d / 2 + 10, k;
            WFR(px, y - ph / 4, 2, ph / 4, RGB(0x40, 0x20, 0x10));
            for (k = 0; k < ph; k += 2) WFR(px + 1 - k / 4, y - ph / 4 - ph + k, k / 2 + 1, 2, RGB(0x10, 0x40, 0x20));
        }
    }
    {
        int32_t r, c, x0 = 100 - rc_curve / 3, y0 = 156 + (int32_t)((wv_ph / 2u) & 1u);
        for (r = 0; r < 12; r++) for (c = 0; c < 20; c++) {
            char ch = RC_CAR[r][c];
            uint16_t col = ch == '1' ? RGB(0xc8, 0x40, 0x20) : ch == '2' ? RGB(0x20, 0x08, 0x08) : ch == '3' ? RGB(0x60, 0x18, 0x08) : ch == '4' ? (wvf_hit_n ? RGB(0xff, 0xff, 0xc0) : RGB(0xff, 0x70, 0x40)) : ch == '5' ? RGB(0x18, 0x18, 0x18) : 0;
            if (ch != '.') WFR(x0 + c * 2, y0 + r * 2, 2, 2, col);
        }
        if (wvf_hit_n) { wvf_disk(x0 + 6, y0 + 26, 3, RGB(0xf0, 0xb0, 0xc0)), wvf_disk(x0 + 32, y0 + 26, 3, RGB(0xf0, 0xb0, 0xc0)); }
    }
    WFR(0, 186, 240, 54, RGB(0x08, 0x14, 0x10)), ne_line(0, 188, 240, 188, RGB(0x40, 0xc0, 0x80));
    for (i = 0; i < 3; i++) {                                          /* LAP 1..3: the loop's time in turns */
        uint32_t t = song.playing ? (fm1_ms - wf_t0) / 100u + (uint32_t)i * 37u : 0u;
        str_cpy(b, "LAP", 16), fmt_int(b + 3, i + 1), str_cpy(b + str_len(b), " ", 16 - str_len(b)), fmt_int(b + str_len(b), (int32_t)(t / 600u % 10u));
        str_cpy(b + str_len(b), "'", 16 - str_len(b)), fmt_int(b + str_len(b), (int32_t)(t / 10u % 60u));
        wf_text(10, 196 + i * 12, 1, b, RGB(0xb0, 0xf0, 0xb0));
    }
    fmt_int(b, song.g[G_BPM] * 2), wf_text(150, 198, 1, b, RGB(0x60, 0xf0, 0x90)), wf_text(178, 198, 1, "MPH", RGB(0x60, 0xf0, 0x90));
    ne_line(128, 206, 128, 196, RGB(0x60, 0xf0, 0x90)), ne_line(124, 200, 128, 196, RGB(0x60, 0xf0, 0x90)), ne_line(132, 200, 128, 196, RGB(0x60, 0xf0, 0x90));
    for (i = 0; i < 14; i++) WFR(118 + i * 6, 214, 4, 3, i < clamp(wv_all() / 16, 0, 14) ? RGB(0x60, 0xf0, 0x90) : RGB(0x18, 0x40, 0x28));
    for (i = 0; i < 8; i++) WFR(214, 228 - i * 5, 18, 3, i < clamp(wv_all() / 30, 0, 8) ? RGB(0x60, 0xf0, 0x90) : RGB(0x18, 0x40, 0x28));
    wf_text(208, 232, 0, "POWER", RGB(0x60, 0xf0, 0x90));
}
/* FALL: someone falling, tumbling, between towers in a pink haze (after the falling reference): three layers of towers rush up past
 * (faster with the sound), the near ones dark, the far ones lost in the haze */
static uint32_t fall_y;
static UI_NOINLINE void ne_fall(void)
{
    int32_t i, y, L;
    uint32_t a = fall_y * 2u;
    for (y = 0; y < 240; y += 4) WFR(0, y, 240, 4, RGB(0xf0 - y / 3, 0x70 - y / 6, 0x60 + y / 8));
    for (L = 0; L < 3; L++) {
        static const uint16_t TC[3] = {RGB(0xc0, 0x60, 0x90), RGB(0x80, 0x30, 0x80), RGB(0x30, 0x10, 0x40)};
        static const uint16_t WC2[3] = {RGB(0xe0, 0x90, 0xb0), RGB(0xb0, 0x70, 0xd0), RGB(0x70, 0x60, 0xd0)};
        int32_t n = L == 2 ? 2 : 3, sp = 1 + L * 2;
        for (i = 0; i < n; i++) {
            int32_t w = 14 + L * 14, x = L == 2 ? (i ? 240 - w + 6 : -6) : (i * 240 / n + L * 37 + 20) % 240 - 10, r;
            WFR(x, 0, w, 240, TC[L]);
            for (r = 0; r < 30; r++) {
                int32_t wy = (r * 10 - (int32_t)((fall_y * (uint32_t)sp) % 10u) - 0);
                int32_t c;
                for (c = 0; c < w / 5; c++) if (((r + c + i + (int32_t)((fall_y * (uint32_t)sp) / 10u)) % 4) && wf_spec[(r + c) % 19] > 4) WFR(x + 2 + c * 5, wy, 3, 4, WC2[L]);
            }
        }
    }
    {                                                                  /* the one falling, turning */
        int32_t cx = 120 + WV_SIN[(wv_ph) & 63u] * 14 / 127, cy = 104 + WV_SIN[(wv_ph * 2u + 9u) & 63u] * 6 / 127;
        int32_t c = wv_sin4(a + 64u), s2 = wv_sin4(a);
        uint16_t k = RGB(0x20, 0x10, 0x40);
        ne_line3(cx - 8 * c / 127, cy - 8 * s2 / 127, cx + 8 * c / 127, cy + 8 * s2 / 127, k);
        wvf_disk(cx - 12 * c / 127, cy - 12 * s2 / 127, 4, k);
        ne_line3(cx + 8 * c / 127, cy + 8 * s2 / 127, cx + 16 * c / 127 - 6 * s2 / 127, cy + 16 * s2 / 127 + 6 * c / 127, k), ne_line3(cx + 8 * c / 127, cy + 8 * s2 / 127, cx + 16 * c / 127 + 5 * s2 / 127, cy + 16 * s2 / 127 - 5 * c / 127, k);
        ne_line3(cx - 4 * c / 127, cy - 4 * s2 / 127, cx - 4 * c / 127 + 10 * s2 / 127, cy - 4 * s2 / 127 - 10 * c / 127, k), ne_line3(cx - 4 * c / 127, cy - 4 * s2 / 127, cx - 4 * c / 127 - 9 * s2 / 127, cy - 4 * s2 / 127 + 9 * c / 127, k);
    }
}
/* FLOAT: two people hand in hand, floating in a burst of blue light that rushes out from behind them (after the dreamlike one) */
static UI_NOINLINE void ne_float(void)
{
    static int32_t fph, frot;
    int32_t i, k;
    fph += 2 + clamp(ev_lvl / 50, 0, 5), frot += 1;                   /* (a phase that only ever creeps forward: no jumps) */
    WFR(0, 0, 240, 240, RGB(0x08, 0x10, 0x30));
    wvf_disk(120, 120, 90, RGB(0x10, 0x20, 0x58)), wvf_disk(120, 120, 56, RGB(0x20, 0x38, 0x90)), wvf_disk(120, 120, 28, RGB(0x50, 0x70, 0xd0));
    for (i = 0; i < 90; i++) {                                         /* the burst: specks rushing out */
        uint32_t an = (uint32_t)i * 97u + (uint32_t)(frot / 3);
        int32_t d0 = (int32_t)(((uint32_t)i * 53u + (uint32_t)fph) % 130u), d1 = d0 + 4 + d0 / 8;
        uint16_t col = i % 3 == 0 ? WC_WHT : i % 3 == 1 ? RGB(0x90, 0xb0, 0xff) : RGB(0x40, 0x70, 0xf0);
        wvf_pline(120 + d0 * wv_sin4(an + 64u) / 127, 120 + d0 * wv_sin4(an) / 127, 120 + d1 * wv_sin4(an + 64u) / 127, 120 + d1 * wv_sin4(an) / 127, col);
    }
    for (k = 0; k < 2; k++) {                                          /* the two, hand in hand */
        int32_t sw = 20 + clamp(ev_lvl / 12, 0, 14), x = 120 + (k ? sw : -sw) * WV_SIN[((uint32_t)frot / 2u + 16u) & 63u] / 127, y = 110 + WV_SIN[(wv_ph + (uint32_t)k * 20u) & 63u] * 6 / 127 + (k ? -1 : 1) * WV_SIN[(wv_ph / 2u) & 63u] / 20;
        uint16_t c = RGB(0x08, 0x0c, 0x24);
        wvf_disk(x, y - 28, 6, c);
        WFR(x - 6, y - 21, 12, 24, c);
        ne_line(x - 3, y + 3, x - 6 + k * 2, y + 30, c), ne_line(x + 3, y + 3, x + 6 + k * 2, y + 28, c);
        ne_line(k ? x - 6 : x + 6, y - 18, 120, y - 6, c);
        ne_line(k ? x + 6 : x - 6, y - 18, k ? x + 16 : x - 16, y - 2, c);
        if (!k) WFR(x - 8, y - 4, 16, 10, c);
    }
}
/* NIGHTRUN: behind a big car on a wet road at night (after the neon drive): a red sun on the horizon, towers glittering both sides,
 * reflections on the road, rain; the tail lights flare on the kick */
static UI_NOINLINE void ne_nightrun(void)
{
    int32_t i, y, x, r;
    for (y = 0; y < 120; y += 4) WFR(0, y, 240, 4, RGB(0x30 + y / 4, 0x50 + y / 4, 0x80 + y / 5));
    wvf_disk(150, 112, 22, RGB(0xd0, 0x30, 0x40)), wvf_disk(150, 112, 18, RGB(0xff, 0x40, 0x30));
    for (i = 0; i < 2; i++) for (r = 0; r < 4; r++) {                  /* the towers, both sides */
        int32_t bx = i ? 160 + r * 20 : r * 22, bw = 18, top = 10 + (r * 23) % 40;
        WFR(bx, top, bw, 120 - top, RGB(0x40, 0x40, 0x80));
        for (y = top + 2; y < 118; y += 3) for (x = bx + 1; x < bx + bw - 1; x += 3)
            if (((x * 7 + y * 13 + (int32_t)wv_ph) % 5) < 2 || wf_spec[(x + y) % 19] > 11) WFR(x, y, 2, 2, ((x + y) % 3) ? RGB(0xd0, 0xd8, 0xff) : RGB(0xff, 0x80, 0xc0));
    }
    WFR(0, 120, 240, 120, RGB(0x18, 0x20, 0x40));                    /* the wet road: the sun and the windows mirrored */
    for (y = 122; y < 240; y += 2) WFR(140 + (int32_t)(((uint32_t)y * 7u + wv_ph * 3u) % 9u) - 4, y, 20 - (y - 120) / 10, 1, RGB(0xc0, 0x30, 0x30));
    for (i = 0; i < 40; i++) WFR((int32_t)(((uint32_t)i * 37u) % 240u), 124 + (int32_t)(((uint32_t)i * 53u + wv_ph) % 100u), 3, 1, RGB(0x80, 0x90, 0xd0));
    for (i = 0; i < 30; i++) { x = (int32_t)(((uint32_t)i * 41u + wv_ph * 5u) % 260u) - 10; y = (int32_t)(((uint32_t)i * 67u + wv_ph * 9u) % 240u); wvf_pline(x, y, x - 3, y + 8, RGB(0x90, 0xa0, 0xd0)); }
    {                                                                  /* the car */
        int32_t gl = wvf_hit_n ? 1 : 0;
        WFR(48, 150, 144, 60, RGB(0x10, 0x10, 0x18)), WFR(56, 140, 128, 12, RGB(0x14, 0x14, 0x1c)), WFR(40, 136, 160, 5, RGB(0x08, 0x08, 0x0c));
        WFR(40, 136, 4, 16, RGB(0x08, 0x08, 0x0c)), WFR(196, 136, 4, 16, RGB(0x08, 0x08, 0x0c));
        for (i = 0; i < 2; i++) {
            int32_t lx = i ? 160 : 80;
            wvf_disk(lx - 10, 170, 8, gl ? RGB(0xff, 0xe0, 0x80) : RGB(0xff, 0x70, 0x30)), wvf_disk(lx - 10, 170, 4, RGB(0x60, 0x10, 0x08));
            wvf_disk(lx + 8, 170, 8, gl ? RGB(0xff, 0x80, 0x60) : RGB(0xe0, 0x20, 0x20)), wvf_disk(lx + 8, 170, 4, RGB(0x60, 0x10, 0x08));
        }
        WFR(108, 196, 8, 6, RGB(0x30, 0x30, 0x38)), WFR(124, 196, 8, 6, RGB(0x30, 0x30, 0x38));
        WFR(52, 210, 26, 12, RGB(0x06, 0x06, 0x08)), WFR(162, 210, 26, 12, RGB(0x06, 0x06, 0x08));
    }
}
/* ORBIT: driving a highway with a planet filling the horizon (after the space-road one): the road bends, green signs go overhead on the
 * kick, and the dashboard's four dials are the level, the tempo, the loop, the bass; gloved hands on the wheel */
static int32_t orb_sign = -1;
static UI_NOINLINE void ne_orbit(void)
{
    int32_t i, y, yh = 120, cxh = 140 + rc_curve;
    WFR(0, 0, 240, 240, RGB(0x02, 0x04, 0x0c));
    for (i = 0; i < 30; i++) cv_pset((int32_t)(((uint32_t)i * 71u) % 240u), (int32_t)(((uint32_t)i * 37u) % 90u), RGB(0xc0, 0xc8, 0xe0));
    wvf_disk(60, 150, 96, RGB(0x10, 0x50, 0x90)), wvf_disk(56, 146, 90, RGB(0x30, 0x90, 0xd0));
    for (i = 0; i < 40; i++) WFR(10 + (int32_t)(((uint32_t)i * 29u + wv_ph / 4u) % 100u), 70 + (int32_t)(((uint32_t)i * 43u) % 50u), 10, 3, RGB(0xd8, 0xf0, 0xff));
    for (y = yh; y < 176; y++) {
        int32_t d = y - yh, z = 4000 / (d + 2), w = d * 2 + 4, cx = cxh - rc_curve * d * d / 4600;
        WFR(cx - w, y, 2 * w, 1, RGB(0x48, 0x48, 0x50));
        WFR(cx - w - 4, y, 4, 1, RGB(0xc0, 0xc0, 0xc8)), WFR(cx + w, y, 4, 1, RGB(0xc0, 0xc0, 0xc8));
        if (((z + rc_phase) / 16) & 1) WFR(cx - 1 - d / 20, y, 2 + d / 10, 1, RGB(0xf0, 0xc0, 0x30));
    }
    if (orb_sign >= 0) {                                               /* a sign overhead, coming at us */
        int32_t s = 6 + orb_sign * 2, sy = 96 - orb_sign;
        WFR(120 - s * 3, sy, s * 6, s, RGB(0x10, 0x80, 0x40)), WFR(120 - s * 3 + 3, sy + 3, s * 2, 1, WC_WHT), WFR(120 + 3, sy + 3, s * 2, 1, WC_WHT);
        WFR(120 - s * 3, sy, 2, 120 - sy, RGB(0x80, 0x80, 0x88)), WFR(120 + s * 3, sy, 2, 120 - sy, RGB(0x80, 0x80, 0x88));
    }
    WFR(0, 176, 240, 64, RGB(0x10, 0x10, 0x14));                       /* the dashboard */
    for (i = 0; i < 4; i++) {
        static const uint8_t SRC[4] = {0, 1, 2, 3};
        int32_t gx = 34 + i * 58, f = SRC[i] == 0 ? clamp(wv_all() * 3, 0, 1024) : SRC[i] == 1 ? ne_frac(song.g[G_BPM], 40, 240) :
                                       SRC[i] == 2 ? (song.playing ? (int32_t)(TSEL->seq_idx % trk_len(TSEL)) * 1024 / (int32_t)trk_len(TSEL) : 0) : clamp(wv_bass() * 40, 0, 1024);
        uint32_t a = 96u + (uint32_t)f * 192u / 1024u;
        wvf_disk(gx, 200, 17, RGB(0x30, 0x30, 0x38)), wvf_disk(gx, 200, 15, RGB(0x06, 0x06, 0x08));
        ne_line(gx, 200, gx + 13 * wv_sin4(a + 64u) / 127, 200 + 13 * wv_sin4(a) / 127, RGB(0xff, 0x40, 0x20));
    }
    {                                                                  /* the wheel and the gloves */
        int32_t t = -rc_curve / 4;
        ne_ring(120, 250, 56, RGB(0x20, 0x20, 0x24)), ne_ring(120, 250, 54, RGB(0x20, 0x20, 0x24));
        wvf_disk(70 + t, 212 + t / 3, 13, RGB(0xd8, 0xd8, 0xd0)), wvf_disk(170 + t, 212 - t / 3, 13, RGB(0xd8, 0xd8, 0xd0));
        wvf_disk(70 + t, 216 + t / 3, 8, RGB(0xa0, 0xa0, 0x98)), wvf_disk(170 + t, 216 - t / 3, 8, RGB(0xa0, 0xa0, 0x98));
    }
}
/* DUSK: a city against a huge setting sun (after the last reference): pink sky, the sun swelling with the bass behind the towers, their
 * windows lit by the bands, two street lamps, a railing in front and the sun's light shivering on the water below it */
static UI_NOINLINE void ne_dusk(void)
{
    int32_t i, y, r, c, R = 34 + wv_bass() / 3;
    for (y = 0; y < 200; y += 4) WFR(0, y, 240, 4, RGB(0x40 + y * 3 / 4 > 0xf0 ? 0xf0 : 0x40 + y * 3 / 4, 0x20 + y / 4, 0x50 + y / 6));
    for (i = 0; i < 8; i++) WFR((int32_t)(((uint32_t)i * 61u + wv_ph / 3u) % 280u) - 40, 14 + i * 5, 40 + i * 6, 2, RGB(0x60, 0x30, 0x60));
    wvf_disk(110, 96, R + 8, RGB(0xff, 0x90, 0x90)), wvf_disk(110, 96, R, RGB(0xff, 0xf0, 0xe0));
    for (i = 0; i < 11; i++) {                                         /* the towers */
        int32_t x = i * 23 - 6 + (i == 3 ? 6 : 0), h = 60 + (int32_t)(((uint32_t)i * 47u) % 90u) + (i == 1 || i == 6 ? 50 : 0), t = 196 - h;
        if (i == 4) { h -= 70; t += 70; }
        WFR(x, t, 20, h, i & 1 ? RGB(0x50, 0x20, 0x48) : RGB(0x38, 0x14, 0x38));
        for (r = 0; r < h / 6 - 1; r++) for (c = 0; c < 3; c++) if (((i * 5 + r * 3 + c) % 4) == 0 || wf_spec[(i + r) % 19] > 10) WFR(x + 3 + c * 6, t + 4 + r * 6, 4, 2, RGB(0xff, 0xd0, 0x90));
    }
    for (i = 0; i < 8; i++) if (wf_spec[i * 2 + 2] > 7) {                /* birds: one for each band that sounds, crossing the sky */
        int32_t bx = (int32_t)(((uint32_t)i * 71u + wv_ph * (2u + (uint32_t)i % 3u)) % 280u) - 20, by = 30 + i * 9 + WV_SIN[((uint32_t)i * 9u + wv_ph * 4u) & 63u] / 40, f = (int32_t)((wv_ph / 2u + (uint32_t)i) & 1u);
        wvf_pline(bx - 4, by - 2 + f * 3, bx, by, RGB(0x20, 0x08, 0x20)), wvf_pline(bx, by, bx + 4, by - 2 + f * 3, RGB(0x20, 0x08, 0x20));
    }
    WFR(0, 196, 240, 44, RGB(0x30, 0x14, 0x30));                       /* the water and its light */
    for (y = 200; y < 240; y += 2) WFR(96 + (int32_t)(((uint32_t)y * 13u + wv_ph * 3u) % 9u) - 4, y, 28 - (y - 200) / 3, 1, RGB(0xff, 0xd0, 0xb0));
    WFR(0, 200, 240, 2, RGB(0x10, 0x08, 0x14)), WFR(0, 224, 240, 3, RGB(0x10, 0x08, 0x14));
    for (i = 0; i < 24; i++) WFR(i * 10, 200, 2, 26, RGB(0x10, 0x08, 0x14));
    for (i = 0; i < 2; i++) {                                          /* the lamps */
        int32_t lx = i ? 228 : 14;
        WFR(lx, 70, 3, 160, RGB(0x10, 0x08, 0x14)), WFR(lx - 10, 70, 23, 3, RGB(0x10, 0x08, 0x14));
        wvf_disk(lx - 8, 78, 6, RGB(0xff, 0xe8, 0xb0)), wvf_disk(lx + 12, 78, 6, RGB(0xff, 0xe8, 0xb0));
    }
}
/* MOTO: riding into a cyberpunk city in the rain (after the motorbike reference): a magenta sky, the city in layers with windows the bands
 * light and neon signs (one says WFM), two street lamps with halos mirrored on the wet road, the rider from behind with the bolt on the
 * back, the red tail light pulsing on the kick, leaning with the melody */
static UI_NOINLINE void ne_moto(void)
{
    int32_t i, y, x, r, c, lean = rc_curve / 8;
    for (y = 0; y < 120; y += 3) WFR(0, y, 240, 3, RGB(0x60 + y / 3, 0x20 + y / 6, 0x70 + y / 4));
    for (i = 0; i < 6; i++) { int32_t cy = 10 + i * 12; for (x = 0; x < 240; x += 2) if (((x / 2 + i * 7 + (int32_t)(wv_ph / 6u)) % 9) < 4 && ((x + cy) & 2)) WFR(x, cy + WV_SIN[((uint32_t)x / 4u + (uint32_t)i * 11u) & 63u] / 40, 2, 2, RGB(0xc0, 0x50, 0xb0)); }
    for (i = 0; i < 16; i++) {                                         /* the far city: spires */
        int32_t bx = i * 15 + (i * 7) % 6, h = 40 + (int32_t)(((uint32_t)i * 41u) % 50u) + (i == 6 ? 40 : 0);
        WFR(bx, 140 - h, 12, h, RGB(0x50, 0x40, 0x78));
        if (i == 6) WFR(bx + 5, 140 - h - 16, 2, 16, RGB(0x50, 0x40, 0x78));
        for (r = 0; r < h / 5; r++) for (c = 0; c < 3; c++) if (((i * 3 + r * 7 + c) % 6) == 0 || wf_spec[(i + r) % 19] > 11) WFR(bx + 2 + c * 3, 142 - h + r * 5, 2, 2, RGB(0xe0, 0xd0, 0xf0));
    }
    for (i = 0; i < 6; i++) {                                          /* the near blocks, lit windows */
        int32_t bx = i * 42 - 8, h = 34 + (int32_t)(((uint32_t)i * 29u) % 30u), t = 150 - h;
        WFR(bx, t, 36, h, RGB(0x34, 0x2c, 0x58)), WFR(bx, t, 36, 2, RGB(0x70, 0x60, 0xa0));
        for (r = 0; r < h / 7; r++) for (c = 0; c < 5; c++) if (((i * 5 + r * 3 + c * 2 + (int32_t)(wv_ph / 24u)) % 4) == 0 || wf_spec[(i * 3 + r + c) % 19] > 10) WFR(bx + 3 + c * 7, t + 5 + r * 7, 5, 3, RGB(0xff, 0xe8, 0xa0));
    }
    WFR(54, 104, 26, 10, C_BLACK), wf_text(57, 106, 1, "WFM", RGB(0xff, 0x90, 0x20));   /* the neon signs */
    WFR(226, 70, 3, 18, RGB(0x40, 0xe0, 0xff)), WFR(14, 82, 3, 12, RGB(0x40, 0xe0, 0xff)), WFR(118, 92, 10, 3, (wv_ph / 10u) & 1u ? RGB(0xff, 0x50, 0xc0) : RGB(0x60, 0x20, 0x50));
    WFR(0, 150, 240, 90, RGB(0x1c, 0x14, 0x30));                       /* the land, the road */
    for (y = 150; y < 240; y++) { int32_t d = y - 150, w = 10 + d * 13 / 10; WFR(120 - w + lean * d / 90, y, 2 * w, 1, d & 1 ? RGB(0x58, 0x50, 0x70) : RGB(0x5e, 0x56, 0x78)); }
    for (i = 0; i < 2; i++) {                                          /* the lamps: halos and their light on the road */
        int32_t lx = i ? 140 : 100, ly = 150;
        WFR(lx + (i ? 6 : -6), 120, 2, 30, RGB(0x20, 0x18, 0x30));
        wvf_disk(lx, ly - 30, 12, RGB(0x70, 0x60, 0x98)), wvf_disk(lx, ly - 30, 7, RGB(0xb8, 0xb0, 0xe0)), wvf_disk(lx, ly - 30, 3, WC_WHT);
        for (y = 0; y < 40; y += 2) WFR(lx - 3 + (i ? y / 4 : -y / 4), 156 + y * 2, 6 - y / 8, 2, RGB(0x90, 0x88, 0xb8));
    }
    for (i = 0; i < 46; i++) { x = (int32_t)(((uint32_t)i * 47u + wv_ph * 4u) % 260u) - 10; y = (int32_t)(((uint32_t)i * 71u + wv_ph * 11u) % 240u); wvf_pline(x, y, x - 2, y + 7, RGB(0xa0, 0x90, 0xc8)); }
    {                                                                  /* the rider */
        int32_t bx = 120 + lean, by = 196, k = wvf_hit_n;
        uint16_t dk = RGB(0x10, 0x0c, 0x18), dk2 = RGB(0x1c, 0x16, 0x26);
        wvf_disk(bx, by - 46, 9, dk2), WFR(bx - 9, by - 46, 18, 4, RGB(0x30, 0x28, 0x40));   /* helmet */
        WFR(bx - 16, by - 36, 32, 22, dk), wvf_disk(bx - 15, by - 30, 6, dk), wvf_disk(bx + 15, by - 30, 6, dk);
        WFR(bx - 4, by - 33, 3, 3, RGB(0xff, 0x90, 0x20)), WFR(bx - 1, by - 30, 3, 3, RGB(0xff, 0x90, 0x20)), WFR(bx + 2, by - 27, 3, 3, RGB(0xff, 0x90, 0x20));   /* the bolt */
        WFR(bx - 12, by - 14, 24, 14, dk), WFR(bx - 6, by, 12, 30, dk2);
        wvf_disk(bx, by - 8, 6 + k, k ? RGB(0xff, 0x60, 0x80) : RGB(0xc0, 0x20, 0x40)), wvf_disk(bx, by - 8, 2 + k / 2, RGB(0xff, 0xc0, 0xd0));
        WFR(bx - 26, by - 9, 52, 1, RGB(0xd0, 0x20, 0x40));
        for (y = 0; y < 20; y += 2) WFR(bx - 2, by + 30 + y, 4, 1, RGB(0x80, 0x18, 0x30));
    }
}
/* DRIVE: inside a car at night (after the pixel-art one): the teal sky dithered through the windscreen, clouds, the moon, the mirror and the
 * pillar, the road in the headlights, the dashboard with its gauges (speed the tempo, revs the level), the tracks as warning lights, the
 * clock the play time, a hand on the wheel; and the lights going by (after the train one): the city's along the horizon, lamps sweeping
 * over the glass and washing the dashboard, a car coming the other way on the kick */
static int32_t dr_lamp, dr_car = -1;
static UI_NOINLINE void ne_drive(void)
{
    int32_t x, y, i, wash = dr_lamp > 70 ? (dr_lamp - 70) * 3 : 0;
    uint16_t teal = RGB(0x20 + wash, 0xa8, 0xc0), cyan = RGB(0x30 + wash, 0xc8, 0xe0);
    char b[8];
    WFR(0, 0, 240, 240, RGB(0x1c, 0x18, 0x1c));
    for (y = 36; y < 133; y++) {                                       /* the sky: dithered, darker at the top */
        int32_t xl = y * 50 / 130 + 2;
        for (x = xl; x < 236; x += 2) cv_pset(x + (y & 1), y, y < 46 ? RGB(0x1a, 0x30, 0x36) : RGB(0x24, 0x44, 0x4c));
        WFR(xl, y, 236 - xl, 1, y < 46 ? RGB(0x16, 0x28, 0x2e) : RGB(0x22, 0x40, 0x48));
        for (x = xl + (y & 1); x < 236; x += 2) cv_pset(x, y, y < 46 ? RGB(0x1a, 0x30, 0x36) : RGB(0x26, 0x46, 0x50));
    }
    for (i = 0; i < 7; i++) {                                          /* clouds, drifting */
        int32_t cx = (int32_t)(((uint32_t)i * 67u + wv_ph / 8u) % 280u) - 20, cy = 44 + i * 11, w = 18 + i * 3;
        WFR(cx, cy, w, 2, RGB(0x14, 0x22, 0x26)), WFR(cx + 4, cy - 2, w / 2, 2, RGB(0x14, 0x22, 0x26)), WFR(cx - 3, cy + 2, w + 6, 1, RGB(0x14, 0x22, 0x26));
    }
    for (i = 0; i < 24; i++) cv_pset(30 + (int32_t)(((uint32_t)i * 71u) % 200u), 50 + (int32_t)(((uint32_t)i * 37u) % 75u), wf_spec[i % 19] > 10 ? WC_WHT : RGB(0x60, 0x90, 0x98));
    wvf_disk(181, 51, 4, RGB(0xe8, 0xe8, 0xe0));
    for (i = 0; i < 30; i++) {                                         /* the city's lights along the horizon, going by */
        x = (int32_t)(((uint32_t)i * 37u + 300u - (wv_ph * 2u) % 300u) % 300u) - 30;
        if (x > 52) WFR(x, 131 + (i % 3), 1 + (i & 1), 1, i % 4 ? RGB(0xff, 0xb0, 0x60) : RGB(0xe0, 0xe8, 0xff));
    }
    WFR(52, 133, 184, 22, RGB(0x20, 0x1c, 0x22));                      /* the road in the headlights */
    for (y = 136; y < 156; y++) { int32_t d = y - 133, w = d * 5; WFR(150 - w, y, 2 * w, 1, d > 14 ? RGB(0x3c, 0x60, 0x72) : RGB(0x2c, 0x48, 0x56)); }
    for (i = 0; i < 3; i++) { int32_t d = (int32_t)((wv_ph * 2u + (uint32_t)i * 8u) % 24u); WFR(150 + d / 2, 135 + d, 1 + d / 8, 1 + d / 10, RGB(0xa0, 0xd8, 0xf0)); }
    if (dr_car >= 0) { int32_t s = 1 + dr_car / 6; WFR(132 - dr_car, 134 + dr_car / 3, s, s, WC_WHT), WFR(138 - dr_car + s * 2, 134 + dr_car / 3, s, s, WC_WHT); }
    if (dr_lamp > 0) {                                                 /* a lamp coming at us, out over the glass */
        int32_t p = dr_lamp, lx = 160 + p * p / 60, ly = 132 - p * p / 90, r = 1 + p / 18;
        if (ly > 30) { wvf_disk(lx, ly, r + 3, RGB(0x60, 0x50, 0x30)), wvf_disk(lx, ly, r, RGB(0xff, 0xd0, 0x80)); WFR(lx - 1, ly, 2, 133 - ly, RGB(0x30, 0x2c, 0x30)); }
    }
    WFR(0, 0, 240, 36, RGB(0x1c, 0x18, 0x1c));                          /* the roof, the visors, the mirror */
    WFR(26, 57, 10, 6, RGB(0x10, 0x0e, 0x12)), WFR(23, 68, 15, 3, RGB(0x10, 0x0e, 0x12)), wvf_disk(57, 82, 7, RGB(0x10, 0x0e, 0x12));
    WFR(204, 44, 32, 24, cyan), WFR(206, 46, 30, 21, RGB(0x10, 0x0e, 0x12));
    for (y = 35; y < 166; y++) { int32_t xp = y * 50 / 130; WFR(0, y, xp, 1, RGB(0x1c, 0x18, 0x1c)); WFR(xp, y, 2, 1, cyan); WFR(xp - 2, y, 2, 1, teal); }
    for (y = 155; y < 200; y++) {                                      /* the dashboard */
        int32_t x0 = 56 - (y - 155) / 3;
        WFR(x0, y, 240 - x0, 1, y < 159 ? cyan : y < 172 ? RGB(0x18 + wash / 2, 0x68, 0x80) : RGB(0x10, 0x40, 0x50));
    }
    WFR(0, 200, 240, 40, RGB(0x14, 0x12, 0x16));
    for (i = 0; i < 48; i++) {                                         /* the wheel: a bright arc, the cluster's orange rim */
        uint32_t a = 128u + (uint32_t)i * 128u / 48u;
        int32_t px = 100 + 70 * wv_sin4(a + 64u) / 127, py = 222 + 70 * wv_sin4(a) / 127;
        WFR(px - 2, py - 2, 4, 4, cyan);
        cv_pset(100 + 56 * wv_sin4(a + 64u) / 127, 222 + 56 * wv_sin4(a) / 127, RGB(0xb0, 0x60, 0x10)), cv_pset(100 + 55 * wv_sin4(a + 64u) / 127, 222 + 55 * wv_sin4(a) / 127, RGB(0xb0, 0x60, 0x10));
    }
    WFR(48, 172, 104, 34, RGB(0x14, 0x12, 0x16));
    for (i = 0; i < 2; i++) {                                          /* the gauges: speed the tempo, revs the level */
        int32_t gx = i ? 114 : 85, gy = 187, f = i ? ne_frac(song.g[G_BPM], 40, 240) : clamp(wv_all() * 3, 0, 1024), t;
        uint32_t a = 96u + (uint32_t)f * 192u / 1024u;
        for (t = 0; t < 10; t++) { uint32_t at = 96u + (uint32_t)t * 21u; cv_pset(gx + 11 * wv_sin4(at + 64u) / 127, gy + 11 * wv_sin4(at) / 127, RGB(0xd0, 0x70, 0x20)); }
        wvf_pline(gx, gy, gx + 9 * wv_sin4(a + 64u) / 127, gy + 9 * wv_sin4(a) / 127, RGB(0xe0, 0x80, 0x20)), wvf_pline(gx + 1, gy, gx + 1 + 9 * wv_sin4(a + 64u) / 127, gy + 9 * wv_sin4(a) / 127, RGB(0xe0, 0x80, 0x20));
    }
    ne_ring(68, 194, 4, RGB(0x60, 0x30, 0x10)), ne_ring(132, 194, 4, RGB(0x60, 0x30, 0x10));
    for (i = 0; i < 4; i++) WFR(78 + i * 11, 201, i == 1 ? 6 : 4, 2, wf_spec[i * 5] > 7 ? (i == 1 ? RGB(0x40, 0xe0, 0x40) : i == 3 ? RGB(0xe0, 0xc0, 0x20) : RGB(0xe0, 0x30, 0x30)) : RGB(0x30, 0x10, 0x10));
    WFR(74, 205, 46, 6, teal), ne_ring(93, 225, 5, RGB(0x20, 0x70, 0x80));
    WFR(168, 183, 58, 12, RGB(0x10, 0x0e, 0x12));                      /* the console: vents, the radio, the clock */
    for (i = 0; i < 5; i++) WFR(170, 185 + i * 2, 24, 1, RGB(0x24, 0x22, 0x28)), WFR(198, 185 + i * 2, 24, 1, RGB(0x24, 0x22, 0x28));
    WFR(168, 200, 58, 36, RGB(0x18, 0x16, 0x1a)), WFR(206, 202, 16, 10, RGB(0x10, 0x0e, 0x12));
    fmt_int(b, (int32_t)(wf_secs() / 60u % 100u)), str_cpy(b + str_len(b), ".", 8 - str_len(b)), fmt_int(b + str_len(b), (int32_t)(wf_secs() % 60u));
    wf_text(207, 204, 0, b, RGB(0xe0, 0x70, 0x20));
    WFR(184, 214, 20, 2, RGB(0xc0, 0x60, 0x20)), ne_ring(176, 228, 4, RGB(0x30, 0x40, 0x50)), ne_ring(215, 228, 4, RGB(0x30, 0x40, 0x50));
    wvf_disk(52, 186, 8, RGB(0x50, 0x54, 0x5a)), wvf_disk(56, 192, 7, RGB(0x44, 0x48, 0x50));   /* the hand on the wheel */
    for (y = 196; y < 240; y++) WFR(40 + (y - 196) / 6, y, 16, 1, RGB(0x34, 0x38, 0x40));
}
/* the videos as pixel art (tools/vidpix_import.py, gen_vidpix.py): a whole frame, then each one's changes, decoded into wv_mem; two frames
 * a beat while it plays, calmer when stopped. Painted in runs of one colour */
#include "wfm_vidpix.h"
static uint8_t vp_on = 0xFF, vp_f;
static uint32_t vp_t;
#define VP_AT(n) ((uint32_t)(wv_mem[(n) >> 1] >> (((n) & 1u) << 2)) & 15u)   /* the frame: 4 bits a pixel */
static UI_NOINLINE void vp_decode(const vp_clip_t *c, uint32_t f)
{
    const uint8_t *d = c->data + c->off[f];
    uint32_t n = 0, N = (uint32_t)c->w * c->h;
    while (n < N) {
        uint32_t b = *d++, idx = b >> 4, run = (b & 15u) + 1u, k;
        if (run == 16u) run = 16u + *d++;
        if (n + run > N) run = N - n;
        if (idx != 15u) for (k = n; k < n + run; k++) wv_mem[k >> 1] = (uint8_t)(k & 1u ? (wv_mem[k >> 1] & 0x0Fu) | idx << 4 : (wv_mem[k >> 1] & 0xF0u) | idx);
        n += run;
    }
}
static UI_NOINLINE void vp_step(uint32_t clip)
{
    const vp_clip_t *c;
    uint32_t per;
    if (clip >= (uint32_t)VP_N) return;
    c = &VP_CLIPS[clip];
    per = 1000u * 120u / ((uint32_t)c->fps * (uint32_t)(song.playing ? clamp(song.g[G_BPM], 40, 240) : 90));   /* its own speed, with the tempo */
    if (vp_on != clip) { vp_on = (uint8_t)clip, vp_f = 0, vp_t = fm1_ms; vp_decode(c, 0); return; }
    if (c->nf > 1u && fm1_ms - vp_t >= per) { vp_t = fm1_ms; vp_f = (uint8_t)((vp_f + 1u) % c->nf); vp_decode(c, vp_f); }
}
static UI_NOINLINE void vp_paint(uint32_t clip, int32_t y0, int32_t hh)
{
    const vp_clip_t *c;
    int32_t z, x, y;
    if (clip >= (uint32_t)VP_N) return;
    c = &VP_CLIPS[clip], z = 240 / c->w;
    for (y = y0 / z; y < c->h && y * z < y0 + hh; y++)
        for (x = 0; x < c->w;) {
            uint32_t v = VP_AT((uint32_t)(y * c->w + x));
            int32_t x1 = x + 1;
            while (x1 < c->w && VP_AT((uint32_t)(y * c->w + x1)) == v) x1++;
            WFR(x * z, y * z, (x1 - x) * z, z, c->pal[v]);
            x = x1;
        }
}
/* ---- the city (after the pixel-art cities): three layers of towers in 2 px dots, the same wherever a scene has a city. Each tower is
 * fixed by its number (a hash), so the skyline stays as it scrolls; its windows (grids, strips, big panes) are lit at random and by the
 * bands; railings, antennas with a red light, a tower with a ball; on the near layer neon signs (BAR, INN, ATM, a BANK board). With
 * pc_mir set it draws itself again upside down under pc_base, darker and wavering: its reflection in water */
typedef struct { uint16_t far, mid, near, warm, cool, neon, edge, glow; } pc_pal_t;
static int32_t pc_base, pc_mir;
static UI_NOINLINE uint32_t pc_h(uint32_t a) { a ^= a >> 7; a *= 2654435761u; return a ^ (a >> 13); }
static UI_NOINLINE uint16_t pc_dim(uint16_t c) { return (uint16_t)((c >> 1) & 0x7BEFu); }
static UI_NOINLINE void pc_rect(int32_t x, int32_t y, int32_t w, int32_t h, uint16_t c)
{
    if (pc_mir) {
        int32_t r;
        for (r = 0; r < h; r++) {
            int32_t my = 2 * pc_base - (y + r) - 1;
            if (my < pc_base) continue;
            WFR(x + WV_SIN[((uint32_t)my * 5u + wv_ph * 3u) & 63u] / 50, my, w, 1, (my & 2) ? pc_dim(c) : pc_dim(pc_dim(c)) | (c & 0x001Fu));
        }
    } else WFR(x, y, w, h, c);
}
static UI_NOINLINE void pc_city(int32_t base, int32_t scroll, const pc_pal_t *pl, int signs)
{
    static const char *const SG[5] = {"BAR", "INN", "ATM", "BANK", "WFM"};
    int32_t L;
    pc_base = base;
    for (L = 0; L < 3; L++) {
        int32_t slot = L == 0 ? 18 : L == 1 ? 24 : 34, sc = scroll * (L + 1) / 3, k0 = sc / slot, k;
        uint16_t col = L == 0 ? pl->far : L == 1 ? pl->mid : pl->near;
        for (k = k0 - 1; k <= k0 + 240 / slot + 1; k++) {
            uint32_t h = pc_h((uint32_t)k * 3u + (uint32_t)L * 977u);
            int32_t x = k * slot - sc + (int32_t)(h % 5u), bw = slot - 2 - (int32_t)(h >> 8) % 6, bh = (L == 0 ? 70 : L == 1 ? 46 : 26) + (int32_t)((h >> 12) % (L == 0 ? 60u : L == 1 ? 50u : 40u)), top = base - bh, r, c;
            uint32_t pat = (h >> 4) % 3u;
            pc_rect(x, top, bw, bh, col);
            pc_rect(x, top, 1, bh, pl->edge);
            if (h & 1u) for (c = 0; c < bw; c += 2) pc_rect(x + c, top - 2, 1, 2, pl->edge);   /* a railing */
            if (h & 2u) { pc_rect(x + bw / 2, top - 8 - (int32_t)(h % 8u), 1, 8 + (int32_t)(h % 8u), pl->edge); if (((wv_ph / 10u) + (uint32_t)k) & 1u) pc_rect(x + bw / 2, top - 9 - (int32_t)(h % 8u), 1, 1, pl->neon); }
            if (L == 0 && h % 7u == 0u) { pc_rect(x + bw / 2, top - 22, 2, 22, col); pc_rect(x + bw / 2 - 4, top - 28, 10, 7, col); pc_rect(x + bw / 2 - 2, top - 32, 6, 4, col); }
            for (r = 0; r < bh / 6 - 1; r++)
                for (c = 0; c < (bw - 2) / 4; c++) {
                    uint32_t q = pc_h((uint32_t)k * 131u + (uint32_t)r * 7u + (uint32_t)c + (uint32_t)L * 31u);
                    int lit = q % 5u < 2u || wf_spec[(uint32_t)(k + r) % 19u] > 11;
                    uint16_t wc = L == 0 ? pc_dim(pl->cool) : (q >> 3) & 1u ? pl->warm : pl->cool;
                    if (!lit) { if (L) wc = pc_dim(col) | pl->edge; else continue; }
                    if (pat == 0u) pc_rect(x + 2 + c * 4, top + 3 + r * 6, 2, 2, wc);
                    else if (pat == 1u) { if (!c) pc_rect(x + 2, top + 3 + r * 6, bw - 4, 1, wc); }
                    else if (c % 2 == 0) pc_rect(x + 2 + c * 4, top + 3 + r * 6, 5, 3, wc);
                }
            if (signs && L == 2 && h % 5u == 0u) {                     /* a neon sign down its side */
                const char *t = SG[(h >> 9) % 5u];
                int32_t i, n = (int32_t)str_len(t);
                for (i = 0; i < n; i++) { char ch[2] = {t[i], 0}; pc_rect(x + bw - 2, top + 6 + i * 10, 9, 9, pl->glow); if (!pc_mir) wf_text(x + bw, top + 7 + i * 10, 1, ch, pl->neon); }
            }
            if (signs && L == 1 && h % 9u == 4u) { pc_rect(x, top - 14, 30, 11, pl->glow); if (!pc_mir) wf_text(x + 3, top - 12, 1, SG[(h >> 5) % 5u], pl->neon); }   /* a board on the roof */
        }
    }
}
static UI_NOINLINE void pc_bokeh(int32_t n, int32_t y0, int32_t y1, uint16_t a, uint16_t b)   /* soft lights out of focus: dithered discs */
{
    int32_t i, x, y;
    for (i = 0; i < n; i++) {
        int32_t cx = (int32_t)((pc_h((uint32_t)i) % 260u + wv_ph / (uint32_t)(2 + i % 3)) % 260u) - 10, cy = y0 + (int32_t)(pc_h((uint32_t)i * 7u) % (uint32_t)(y1 - y0)), r = 4 + (int32_t)(pc_h((uint32_t)i * 3u) % 6u);
        for (y = -r; y <= r; y++) for (x = -r; x <= r; x++) if (x * x + y * y <= r * r && ((x + y) & 1)) cv_pset(cx + x, cy + y, i & 1 ? a : b);
    }
}
static const pc_pal_t PC_PURPLE = {RGB(0x6a, 0x58, 0xd0), RGB(0x50, 0x3c, 0xa8), RGB(0x34, 0x26, 0x70), RGB(0xff, 0xe8, 0x98), RGB(0xb0, 0xe0, 0xff),
                                   RGB(0xff, 0x90, 0xe0), RGB(0x80, 0x70, 0xd8), RGB(0x80, 0x40, 0xa0)};
static const pc_pal_t PC_TEAL = {RGB(0x2c, 0x50, 0x50), RGB(0x6a, 0x3c, 0x30), RGB(0x2a, 0x1e, 0x1c), RGB(0xf0, 0xe0, 0xc0), RGB(0xc8, 0xb8, 0xa8),
                                 RGB(0xf0, 0x80, 0x30), RGB(0x90, 0x60, 0x40), RGB(0x80, 0x40, 0x20)};
static UI_NOINLINE void pc_sky(uint16_t top, uint16_t bot, int32_t h)   /* a sky in bands, dithered where they meet */
{
    int32_t y, x;
    for (y = 0; y < h; y += 2) {
        int32_t t = y * 255 / h, r = (((top >> 11) & 31) * (255 - t) + ((bot >> 11) & 31) * t) / 255, g = (((top >> 5) & 63) * (255 - t) + ((bot >> 5) & 63) * t) / 255, b = ((top & 31) * (255 - t) + (bot & 31) * t) / 255;
        uint16_t c = (uint16_t)(r << 11 | g << 5 | b);
        WFR(0, y, 240, 2, c);
        if ((y / 2) % 6 == 5 && y < h / 3) for (x = (y / 2) & 2; x < 240; x += 4) WFR(x, y, 2, 2, (uint16_t)(c + 0x0841u));   /* (the lines only up top) */
    }
}
/* SHIP: our ship in the player's colours on the left, facing right; invaders, saucers and asteroids drift in from the right and wander
 * there. It fires only when a note is played: one shot per note, at the nearest one, coloured by the note; a hit bursts it. Without notes
 * they just hover facing each other. The same game in the window (the stars) and on the whole screen (the night over the roofs) */
typedef struct { int16_t x, y, vx, vy; uint8_t kind, boom, wait, tx; } sh_en_t;   /* x, y: 1/1024 of the picture */
static sh_en_t sh_e[6];
static int16_t sh_b[8][5];                     /* shots: x, y, dx, dy (1/1024 a step), colour; dx == 0: none */
static int32_t sh_y = 512, sh_warp;              /* the stars' speed: up with the notes, easing back */
static const char *const SH_INV[4][8] = {
    {"..X.....X..", "...X...X...", "..XXXXXXX..", ".XX.X.X.XX.", ".XXXXXXXXX.", "..X.XXX.X..", ".X.......X.", "X.........X"},
    {"...XXXXX...", ".XXXXXXXXX.", "XX..XXX..XX", "XXXXXXXXXXX", "..XX...XX..", ".XX.XXX.XX.", "XX.......XX", "..........."},
    {".....X.....", "...XXXXX...", ".XXX...XXX.", "XXX..X..XXX", ".XXX...XXX.", "...XXXXX...", "..X.X.X.X..", ".X.......X."},
    {"....XXXXX....", "..XXXXXXXXX..", "XXX.XX.XX.XXX", "..XXXXXXXXX..", "...X..X..X...", ".............", ".............", "............."}};
static const char *const SH_ME[9] = {"BB............", "DBBB..........", ".DBBBBB.......", ".DDBBBGGBB....", "..DBBBGGGBBBO.", ".DDBBBGGBB....", ".DBBBBB.......",
                                     "DBBB..........", "BB............"};
static UI_NOINLINE void sh_spawn(sh_en_t *e)
{
    uint32_t h = pc_h(wv_ph * 7u + (uint32_t)(e - sh_e) * 31u);
    e->kind = (uint8_t)(h % 6u), e->x = 1100, e->y = (int16_t)(120 + (int32_t)((h >> 3) % 760u)), e->vx = (int16_t)-(4 + (int32_t)((h >> 9) % 5u)), e->vy = 0;
    e->boom = 0, e->wait = 0, e->tx = (uint8_t)(120 + (h >> 13) % 100u);   /* where it stops coming in (x / 4) */
}
static UI_NOINLINE void sh_step(void)
{
    int32_t i, k, best = -1, bd = 99999;
    for (i = 0; i < 6; i++) {
        sh_en_t *e = &sh_e[i];
        if (e->boom) { if (--e->boom == 0) e->wait = (uint8_t)(10 + i * 3); continue; }
        if (e->wait) { if (--e->wait == 0) sh_spawn(e); continue; }
        if (!e->x) { sh_spawn(e); continue; }
        if (e->kind == 5u) { e->x = (int16_t)(e->x + e->vx); if (e->x < -60) sh_spawn(e); }          /* an asteroid: it just drifts across */
        else if (e->x > e->tx * 4) e->x = (int16_t)(e->x + e->vx);                                    /* coming in */
        else {                                                                                       /* wandering on the right */
            if (!(wv_ph % 12u)) { uint32_t h = pc_h(wv_ph + (uint32_t)i * 77u); e->vx = (int16_t)((int32_t)(h % 9u) - 4); e->vy = (int16_t)((int32_t)((h >> 4) % 13u) - 6); }
            e->x = (int16_t)clamp(e->x + e->vx, 480, 980), e->y = (int16_t)clamp(e->y + e->vy, 80, 920);
        }
        { int32_t d = e->y - sh_y; d = d < 0 ? -d : d; d += (e->x - 200) / 3; if (e->x < 1000 && d < bd) bd = d, best = i; }
    }
    sh_warp += nk_on ? 6 : 0, sh_warp = clamp(sh_warp, 0, 40), sh_warp -= sh_warp > 0 ? 1 : 0;
    if (best >= 0) sh_y += (sh_e[best].y - sh_y) / (ev_rate > 40 ? 2 : 4);   /* it turns to face the nearest: quick, quicker when you play */
    for (k = 0; k < 27; k++) if ((nk_on >> k) & 1u && best >= 0) {       /* a note: a shot */
        for (i = 0; i < 8; i++) if (!sh_b[i][2]) {
            int32_t dx = sh_e[best].x - 150, dy = sh_e[best].y - sh_y, n = (dx < 0 ? -dx : dx) / (130 + sh_warp * 4) + 1;
            sh_b[i][0] = 150, sh_b[i][1] = (int16_t)sh_y, sh_b[i][2] = (int16_t)(dx / n ? dx / n : 1), sh_b[i][3] = (int16_t)(dy / n), sh_b[i][4] = (int16_t)k;
            break;
        }
    }
    for (i = 0; i < 8; i++) if (sh_b[i][2]) {
        sh_b[i][0] = (int16_t)(sh_b[i][0] + sh_b[i][2]), sh_b[i][1] = (int16_t)(sh_b[i][1] + sh_b[i][3]);
        for (k = 0; k < 6; k++) {
            sh_en_t *e = &sh_e[k];
            int32_t dx = e->x - sh_b[i][0], dy = e->y - sh_b[i][1];
            if (!e->boom && !e->wait && e->x < 1024 && dx < 60 && dx > -60 && dy < 70 && dy > -70) { e->boom = 10; sh_b[i][2] = 0; break; }
        }
        if (sh_b[i][0] > 1100 || sh_b[i][0] < -50) sh_b[i][2] = 0;
    }
}
static UI_NOINLINE uint16_t sh_note_col(int32_t k)
{
    static const uint16_t C[6] = {RGB(0xff, 0x50, 0x40), RGB(0xff, 0xa0, 0x30), RGB(0xf0, 0xf0, 0x60), RGB(0x60, 0xf0, 0x90), RGB(0x60, 0xc0, 0xff), RGB(0xd0, 0x80, 0xff)};
    return C[(k * 6) / 27];
}
static UI_NOINLINE void sh_draw(int32_t rx, int32_t ry, int32_t rw, int32_t rh, int32_t z, int big)
{
    int32_t i, r, c;
    uint16_t en = RGB(0xe0, 0xe8, 0xd0), star = RGB(0xc8, 0xd8, 0xc0);
#define SX(v) (rx + (v) * rw / 1024)
#define SY(v) (ry + (v) * rh / 1024)
    WFR(rx, ry, rw, rh, RGB(0x10, 0x0c, 0x1e));                         /* the stars: dots, streaking when you play */
    for (i = 0; i < (big ? 70 : 34); i++) {
        uint32_t sp = 1u + (uint32_t)(i % 3) + (uint32_t)sh_warp / 3u;
        int32_t x = rx + (int32_t)((pc_h((uint32_t)i * 13u) % (uint32_t)rw + (uint32_t)rw * 64u - (wv_ph * sp) % ((uint32_t)rw * 64u)) % (uint32_t)rw), y = ry + (int32_t)(pc_h((uint32_t)i * 29u) % (uint32_t)rh), l = sh_warp * (1 + i % 3) / 3;
        WFR(x, y, 1 + l, 1, l > 6 ? (i & 1 ? RGB(0x80, 0xc0, 0xff) : WC_WHT) : star);
        if (i % 11 == 0 && !l) WFR(x - 1, y, 3, 1, star), WFR(x, y - 1, 1, 3, star);
    }
    for (i = 0; i < 6; i++) {                                          /* them */
        sh_en_t *e = &sh_e[i];
        int32_t x = SX(e->x), y = SY(e->y);
        if (e->wait || !e->x) continue;
        if (e->boom) {                                                 /* a burst */
            int32_t rr = (10 - e->boom) * z;
            for (r = 0; r < 8; r++) { uint32_t an = (uint32_t)r * 32u; WFR(x + rr * wv_sin4(an + 64u) / 127, y + rr * wv_sin4(an) / 127, z, z, e->boom & 1 ? WC_WHT : RGB(0xff, 0xa0, 0x30)); }
            continue;
        }
        if (e->kind == 5u) {                                           /* an asteroid */
            wvf_disk(x, y, 4 * z, RGB(0x8a, 0x84, 0x90)), WFR(x - z, y - 2 * z, 2 * z, z, RGB(0x60, 0x5a, 0x68)), WFR(x + z, y + z, z, z, RGB(0x60, 0x5a, 0x68));
            continue;
        }
        {
            const char *const *sp = SH_INV[e->kind < 4u ? e->kind : 3u];
            int32_t w = (int32_t)str_len(sp[0]), fl = (int32_t)((wv_ph / 6u + (uint32_t)i) & 1u);
            for (r = 0; r < 8; r++) for (c = 0; c < w; c++) if (sp[r][c] == 'X' && !(fl && r == 7)) WFR(x - w * z / 2 + c * z, y - 4 * z + r * z, z, z, en);
        }
    }
    {                                                                  /* us */
        int32_t x = SX(90), y = SY(sh_y);
        for (r = 0; r < 9; r++) for (c = 0; c < 14; c++) {
            char ch = SH_ME[r][c];
            if (ch != '.') WFR(x + c * z, y - 9 * z / 2 + r * z, z, z, ch == 'B' ? WC_BTN : ch == 'D' ? WC_FR2 : ch == 'G' ? WC_GRN : WC_AMB);
        }
        for (i = 0; i < 2 + (int32_t)(wv_ph & 1u) + wv_bass() / 6; i++) WFR(x - (i + 1) * z, y - z / 2, z, z, i < 1 ? WC_WHT : RGB(0xff, 0x80, 0x30));
    }
    for (i = 0; i < 8; i++) if (sh_b[i][2]) { int32_t x = SX(sh_b[i][0]), y = SY(sh_b[i][1]); WFR(x - 3 * z, y, 4 * z, z, sh_note_col(sh_b[i][4])); }
#undef SX
#undef SY
}
/* ==== v2: the chosen ones ====================================================================================================== */
/* the keys that sound (pressed, or played by the tracks) and the ones that just started; an energy that follows the level softly */
static UI_NOINLINE uint32_t keys_sounding(const track_t *t);
static UI_NOINLINE void nk_step(void)
{
    uint32_t m = fm1_in.notes, i, n = 0, d;
    for (i = 0; i < NTRK; i++) m |= keys_sounding(&trk[i]);
    nk_on = m & ~nk_prev, nk_prev = m, nk_mask = m;
    nk_hand = fm1_in.notes, nk_hon = nk_hand & ~nk_hprev, nk_hprev = nk_hand;
    dr_hits = drum_vis_hits, drum_vis_hits = 0;
    for (d = nk_on; d; d >>= 1) n += d & 1u;
    ev_rate += ((int32_t)n * 20 * 16 - ev_rate) / 24;                 /* (20 steps a second) */
    ev_lvl += (wv_all() - ev_lvl) / 4;
}
static UI_NOINLINE int32_t nk_low(uint32_t m) { return (m & 0x1FFFu) != 0u; }      /* a key on the left half (F3..) */
static UI_NOINLINE int32_t nk_high(uint32_t m) { return (m & 0x7FFE000u) != 0u; }  /* on the right half */
static UI_NOINLINE int32_t nk_top(uint32_t m) { int32_t k = -1; while (m) { k++; m >>= 1; } return k; }   /* the highest key, -1 none */

/* ---- the road (RACER, DRIVE): a pseudo-3D road as the old tunnel, rows from the horizon down, the 2D pictures over it. Three lanes; the
 * other cars come back at us, ours changes lane before it reaches one; the speed starts low and climbs with the beat and the tempo, the
 * curves grow with it */
typedef struct { uint16_t road, dash, edge, side, post, glow; } rd_pal_t;
static int32_t rd_pos, rd_spd = 40, rd_cur, rd_curt, rd_px, rd_lane = 1, rd_kmh = 90 * 16, rd_ovt;
static int32_t rd_car[4][3] = {{900, 1, 0}, {1600, 0, 1}, {2300, 2, 2}, {3200, 1, 3}};   /* distance ahead, lane, colour */
static UI_NOINLINE void rd_step(void)
{
    int32_t i, tgt = 90 + clamp((clamp(song.g[G_BPM], 40, 240) - 70) / 2 + ev_rate / 5 + ev_lvl / 3, 0, 120);   /* the km/h it wants: 90 cruising, up to 210 with the tempo, the notes, the level */
    if (!song.playing && ev_lvl < 10) tgt = 90;
    if (rd_ovt) rd_ovt--, tgt = tgt + 30 > 210 ? 210 : tgt + 30;      /* an overtake: a push of ~30 */
    tgt *= 16, rd_kmh += (tgt - rd_kmh) / (tgt > rd_kmh ? 40 : 16);    /* (it climbs slowly, slows a little faster) */
    rd_spd = (rd_kmh / 16 - 30) * 2 / 3;                               /* the road runs at the speed the dial says */
    rd_pos += rd_spd;
    if (!(wv_ph % 40u)) rd_curt = (int32_t)(pc_h(wv_ph) % 200u) - 100;   /* a new bend now and then, sharper when fast */
    rd_cur += (rd_curt * clamp(rd_spd - 30, 0, 160) / 160 - rd_cur) / 20;
    for (i = 0; i < 4; i++) {
        int32_t *c = rd_car[i];
        c[0] -= rd_spd - 34;
        if (c[0] < -120) c[0] = 2400 + (int32_t)(pc_h((uint32_t)i + wv_ph) % 1200u), c[1] = (int32_t)(pc_h(wv_ph * 3u + (uint32_t)i) % 3u);
        if (c[0] > 0 && c[0] < 520 && c[1] == rd_lane) {                /* one ahead in our lane: the free lane next to it */
            int32_t a = rd_lane == 0 ? 1 : rd_lane == 2 ? 1 : (pc_h(wv_ph) & 1u) ? 0 : 2, j, ok = 1;
            for (j = 0; j < 4; j++) if (rd_car[j][1] == a && rd_car[j][0] > -60 && rd_car[j][0] < 700) ok = 0;
            if (ok) rd_lane = a, rd_ovt = 90;
            else for (j = 0; j < 3; j++) { int32_t k2, ok2 = j != rd_lane; for (k2 = 0; k2 < 4; k2++) if (rd_car[k2][1] == j && rd_car[k2][0] > -60 && rd_car[k2][0] < 700) ok2 = 0; if (ok2) { rd_lane = j, rd_ovt = 90; break; } }
        }
    }
    rd_px += ((rd_lane - 1) * 170 - rd_px) / 5;                        /* (the lane change: quick, then settling) */
}
static UI_NOINLINE int32_t rd_cx(int32_t y, int32_t yh, int32_t yb) { int32_t d = y - yh, w = d * 2 + 4, f = yb - y; return 120 - rd_px * w / 256 + rd_cur * f * f / 900; }
static UI_NOINLINE int32_t rd_w(int32_t y, int32_t yh) { return (y - yh) * 2 + 4; }
static UI_NOINLINE void rd_draw(int32_t yh, int32_t yb, const rd_pal_t *p)
{
    int32_t y, i;
    for (y = yh; y < yb; y++) {
        int32_t d = y - yh + 1, z = 6000 / d, w = rd_w(y, yh), cx = rd_cx(y, yh, yb), on = ((z + rd_pos / 8) / 40) & 1, dw = 1 + d / 16;
        WFR(cx - w, y, 2 * w, 1, p->road);
        WFR(cx - w - 1 - d / 20, y, 1 + d / 20, 1, p->edge), WFR(cx + w, y, 1 + d / 20, 1, p->edge);
        if (on) { WFR(cx - w / 3 - dw / 2, y, dw, 1, p->dash), WFR(cx + w / 3 - dw / 2, y, dw, 1, p->dash); }
        if (((z + rd_pos / 8) % 260) < 3 && d > 4) {                   /* the posts with their lamps, both sides */
            int32_t ph = d / 2 + 4, l;
            for (l = -1; l <= 1; l += 2) {
                int32_t px = cx + l * (w + 8 + d / 3);
                WFR(px, y - ph, 1 + d / 30, ph, p->post), WFR(l < 0 ? px : px - 2 - d / 12, y - ph, 3 + d / 12, 1 + d / 40, p->post);
                wvf_disk(l < 0 ? px + 2 + d / 12 : px - 2 - d / 12, y - ph + 1, 1 + d / 30, p->glow);
            }
        }
    }
    for (i = 3; i >= 0; i--) {                                         /* the others, the farthest first */
        int32_t dz = rd_car[i][0], yy, s, lx, w;
        static const uint16_t CC[4] = {RGB(0xe0, 0xe0, 0xe8), RGB(0x40, 0xc0, 0x60), RGB(0x40, 0x80, 0xe0), RGB(0xf0, 0xc0, 0x30)};
        if (dz <= 20 || dz > 2600) continue;
        yy = yh + 6000 * 8 / (dz * 40 / 40 + 60) / 8;
        if (yy >= yb || yy <= yh) continue;
        w = rd_w(yy, yh), s = 2 + w / 9, lx = rd_cx(yy, yh, yb) + (rd_car[i][1] - 1) * w * 2 / 3;
        WFR(lx - s, yy - s * 2 / 3, s * 2, s * 2 / 3, CC[rd_car[i][2]]), WFR(lx - s + s / 4, yy - s, s * 3 / 2, s / 3 + 1, CC[rd_car[i][2]]);
        WFR(lx - s, yy - s / 3, s / 3 + 1, s / 4 + 1, RGB(0xff, 0x30, 0x30)), WFR(lx + s - s / 3 - 1, yy - s / 3, s / 3 + 1, s / 4 + 1, RGB(0xff, 0x30, 0x30));
        WFR(lx - s, yy, s / 2, 1 + s / 6, C_BLACK), WFR(lx + s / 2, yy, s / 2, 1 + s / 6, C_BLACK);
    }
}
/* the strip city (after the night-drive one): towers drawn in dotted lines, lit dots, spires, a wheel turning, signs and round bulbs along
 * the base; it slides with the bends */
static UI_NOINLINE void rd_city(int32_t base, int32_t scroll, uint16_t a, uint16_t b, uint16_t c)
{
    int32_t k, x, y;
    for (k = -1; k < 15; k++) {
        uint32_t h = pc_h((uint32_t)(k + 40 + scroll / 20) * 7u);
        int32_t bx = k * 20 - ((scroll % 20) + 20) % 20, bw = 10 + (int32_t)(h % 8u), bh = 14 + (int32_t)((h >> 5) % 34u), t = base - bh, kind = (int32_t)((h >> 9) % 6u);
        for (y = t; y < base; y += 2) WFR(bx, y, 1, 1, a), WFR(bx + bw, y, 1, 1, a);
        for (x = bx; x <= bx + bw; x += 2) WFR(x, t, 1, 1, a);
        for (y = t + 3; y < base - 2; y += 4) for (x = bx + 2; x < bx + bw - 1; x += 3) if (((uint32_t)(x * 5 + y * 3) + h) % 4u == 0u || wf_spec[(uint32_t)(x + y) % 19u] > 11) WFR(x, y, 1, 1, b);
        if (kind == 0) for (y = 0; y < 14; y += 2) WFR(bx + bw / 2, t - 2 - y, 1, 1, a);                    /* a spire */
        else if (kind == 1) { int32_t cxw = bx + bw / 2, cyw = t - 12, r = 10; uint32_t q; for (q = 0; q < 16u; q++) { uint32_t an = q * 16u + wv_ph * 2u; WFR(cxw + r * wv_sin4(an + 64u) / 127, cyw + r * wv_sin4(an) / 127, 1, 1, (q & 1u) ? b : a); }
            for (q = 0; q < 4u; q++) { uint32_t an = q * 64u + wv_ph * 2u; wvf_pline(cxw, cyw, cxw + r * wv_sin4(an + 64u) / 127, cyw + r * wv_sin4(an) / 127, a); } }   /* a wheel */
        else if (kind == 2) { int32_t sw2 = 26; for (x = 0; x < sw2; x += 2) WFR(bx + x, t - 12, 1, 1, ((uint32_t)x / 2u + wv_ph / 3u) & 1u ? b : a), WFR(bx + x, t - 3, 1, 1, ((uint32_t)x / 2u + wv_ph / 3u) & 1u ? a : b);
            wf_text(bx + 4, t - 10, 0, (h >> 12) & 1u ? "WFM" : "BAR", a); }                              /* a sign */
        if ((h >> 3) % 3u == 0u) wvf_disk(bx + bw / 2, base - 3, 2 + (int32_t)((h >> 14) % 3u), c);       /* a bulb */
    }
}
static const rd_pal_t RD_VEGAS = {RGB(0x04, 0x04, 0x04), RGB(0xf8, 0xa0, 0x20), RGB(0xf8, 0xa0, 0x20), 0, RGB(0x30, 0xb0, 0xc0), RGB(0xa0, 0xf0, 0xff)};
static const rd_pal_t RD_SYNTH = {RGB(0x18, 0x0c, 0x30), RGB(0xff, 0x60, 0xd0), RGB(0x60, 0xe0, 0xff), 0, RGB(0x60, 0x40, 0xb0), RGB(0xff, 0xc0, 0xf0)};
static UI_NOINLINE void rd_player(int32_t x0, int32_t y0, int brake)  /* our red car from behind (2 px), its tail lights; a puff on a hard bend */
{
    int32_t r, c;
    for (r = 0; r < 12; r++) for (c = 0; c < 20; c++) {
        char ch = RC_CAR[r][c];
        uint16_t col = ch == '1' ? RGB(0xd8, 0x20, 0x20) : ch == '2' ? RGB(0x20, 0x08, 0x08) : ch == '3' ? RGB(0x60, 0x08, 0x08) : ch == '4' ? (brake ? RGB(0xff, 0xff, 0xc0) : RGB(0xff, 0x60, 0x40)) : ch == '5' ? RGB(0x18, 0x18, 0x18) : 0;
        if (ch != '.') WFR(x0 + c * 2, y0 + r * 2, 2, 2, col);
    }
}
/* RACER: the night drive through the strip (after the orange-lit one), the dashboard of a car below */
static UI_NOINLINE void v2_racer(void)
{
    int32_t i, kmh = rd_kmh / 16;
    char b[12];
    rd_city(110, rd_cur / 2 + (int32_t)(rd_pos / 400), RGB(0xf8, 0xa0, 0x20), RGB(0xff, 0xe0, 0x90), RGB(0xff, 0xb0, 0x40));
    for (i = 0; i < 3; i++) WFR((int32_t)(pc_h((uint32_t)i * 9u) % 200u) + 20, 50 + i * 9, 1, 30 - i * 8, RGB(0xf8, 0xa0, 0x20));   /* (searchlight poles) */
    WFR(0, 110, 240, 2, RGB(0x60, 0x30, 0x08));
    rd_draw(112, 192, &RD_VEGAS);
    rd_player(100 + (rd_cur / 10), 160 + (int32_t)((wv_ph / 2u) & 1u), wvf_hit_n > 2);
    for (i = 0; i < 52; i++) { int32_t x = 4 + i * 4 + i / 13 * 2, top = 192 + (i - 26) * (i - 26) / 120; WFR(x, top, 4, 240 - top, RGB(0x1a, 0x1a, 0x22)); }   /* the dashboard: a curved hood */
    for (i = 0; i < 240; i += 2) { int32_t top = 192 + (i / 4 - 26) * (i / 4 - 26) / 120; WFR(i, top, 2, 1, RGB(0x50, 0x50, 0x68)); }
    for (i = 0; i < 2; i++) {                                          /* two dials: the speed, the level */
        int32_t gx = i ? 196 : 44, gy = 222, f = i ? clamp(ev_lvl * 4, 0, 1024) : clamp(kmh * 1024 / 240, 0, 1024), t;
        uint32_t a = 96u + (uint32_t)f * 192u / 1024u;
        wvf_disk(gx, gy, 17, RGB(0x30, 0x30, 0x40)), wvf_disk(gx, gy, 15, RGB(0x06, 0x06, 0x0a));
        for (t = 0; t <= 8; t++) { uint32_t at = 96u + (uint32_t)t * 24u; WFR(gx + 12 * wv_sin4(at + 64u) / 127, gy + 12 * wv_sin4(at) / 127, 1, 1, t >= 6 ? RGB(0xff, 0x40, 0x30) : RGB(0xf8, 0xa0, 0x20)); }
        ne_line(gx, gy, gx + 11 * wv_sin4(a + 64u) / 127, gy + 11 * wv_sin4(a) / 127, RGB(0xff, 0x40, 0x30));
    }
    WFR(80, 206, 80, 30, RGB(0x06, 0x0c, 0x08)), WFR(80, 206, 80, 1, RGB(0x40, 0xc0, 0x80));   /* the LCD: speed, rev bar, time */
    fmt_int(b, kmh), wf_textr(140, 210, 1, b, RGB(0x60, 0xf0, 0x90)), wf_text(144, 210, 0, "KM/H", RGB(0x60, 0xf0, 0x90));
    for (i = 0; i < 14; i++) WFR(84 + i * 5, 222, 4, 4, i < clamp(ev_lvl / 12, 0, 14) ? (i < 10 ? RGB(0x60, 0xf0, 0x90) : RGB(0xff, 0x50, 0x40)) : RGB(0x14, 0x30, 0x20));
    fmt_int(b, (int32_t)(wf_secs() / 60u)), str_cpy(b + str_len(b), "'", 12 - str_len(b)), fmt_int(b + str_len(b), (int32_t)(wf_secs() % 60u)), wf_text(86, 229, 0, b, RGB(0x60, 0xf0, 0x90));
}
/* DRIVE: the same road from inside (after the purple car and the night-drive one): the windscreen, the mirror and a little charm swinging,
 * the wheel turning with the bends (no one at it), the dashboard glowing with the music, the radio on:
 * its equaliser the spectrum, the sound's name running on its display */
static UI_NOINLINE void v2_drive(void)
{
    int32_t i, y, x, gl = clamp(ev_lvl / 3, 0, 60), tilt = -rd_cur / 8;
    uint16_t frame = RGB(0x2a, 0x1c, 0x4c), dash = RGB(0x50 + gl / 2, 0x38, 0x98 + gl), rim = RGB(0x90 + gl, 0x70, 0xe0);
    char nm[14];
    pc_sky(RGB(0x10, 0x08, 0x38), RGB(0x90, 0x30, 0x90), 96);
    for (i = 0; i < 26; i++) WFR((int32_t)(pc_h((uint32_t)i) % 240u), (int32_t)(pc_h((uint32_t)i * 5u) % 60u), 1, 1, wf_spec[i % 19] > 10 ? WC_WHT : RGB(0xc0, 0xa0, 0xf0));
    rd_city(96, rd_cur / 2 + (int32_t)(rd_pos / 400), RGB(0xff, 0x70, 0xd0), RGB(0x80, 0xf0, 0xff), RGB(0xff, 0xa0, 0xe0));
    rd_draw(96, 150, &RD_SYNTH);
    WFR(0, 0, 240, 22, frame);                                         /* the top of the windscreen (no pillars: the street stays open) */
    WFR(0, 22, 240, 1, rim);
    WFR(80, 24, 80, 26, RGB(0x10, 0x08, 0x20));                       /* the mirror: what is behind, the picture flipped */
    if (wf_ry == 0 && cv_oy == 0 && cv_w == 240u) {
        int32_t my, mx;
        for (my = 0; my < 22; my++) { const uint16_t *src = cv_px + (uint32_t)(52 + my * 2) * 240u; uint16_t *dst = cv_px + (uint32_t)(26 + my) * 240u; for (mx = 0; mx < 76; mx++) dst[82 + mx] = src[196 - mx * 2]; }
    }
    WFR(80, 24, 80, 2, rim), WFR(80, 48, 80, 2, rim), WFR(80, 24, 2, 26, rim), WFR(158, 24, 2, 26, rim);
    { int32_t sw = WV_SIN[(wv_ph * 4u) & 63u] * (3 + wv_bass() / 3) / 127; wvf_pline(120, 50, 120 + sw, 60, RGB(0x80, 0x70, 0xa0)); wvf_disk(120 + sw, 63, 3, RGB(0xff, 0x80, 0xc0)); }
    for (y = 150; y < 240; y++) WFR(0, y, 240, 1, y < 153 ? rim : y < 176 ? dash : RGB(0x30, 0x20, 0x60));   /* the dashboard */
    for (x = 0; x < 240; x += 4) WFR(x, 153, 2, 1, (wf_spec[(uint32_t)x / 13u % 19u] > 8) ? RGB(0xff, 0xb0, 0xff) : dash);   /* (a light strip that the bands run along) */
    WFR(124, 160, 80, 44, RGB(0x14, 0x0c, 0x28)), WFR(124, 160, 80, 1, rim);                    /* the radio */
    WFR(128, 164, 72, 12, RGB(0x06, 0x10, 0x10));
    if (song.sel < NPART) trk_short_name(song.sel, nm); else str_cpy(nm, "DRUMS", 14);
    wf_clx0 = 129, wf_clx1 = 199;
    wf_text(200 - (int32_t)((wv_ph * 2u) % (uint32_t)(wf_tw(1, nm) + 72)), 166, 1, nm, RGB(0x60, 0xff, 0xd0));
    wf_clx0 = 0, wf_clx1 = 240;
    for (i = 0; i < 19; i++) { int32_t h = clamp(wf_spec[i], 0, 18); WFR(129 + i * 4 - i / 5, 198 - h, 3, h, i < 6 ? RGB(0x40, 0xe0, 0xff) : i < 13 ? RGB(0xff, 0x70, 0xd0) : RGB(0xff, 0xe0, 0x60)); }
    wvf_disk(212, 172, 5, RGB(0x30, 0x24, 0x50)), wvf_disk(212, 190, 5, RGB(0x30, 0x24, 0x50));
    for (i = 0; i < 2; i++) { uint32_t an = 96u + (uint32_t)clamp(i ? ev_lvl * 2 : song.g[G_BPM], 0, 192); wvf_pline(212, 172 + i * 18, 212 + 4 * wv_sin4(an + 64u) / 127, 172 + i * 18 + 4 * wv_sin4(an) / 127, RGB(0xff, 0xb0, 0xff)); }
    {                                                                  /* behind the wheel: the cluster (its two dials turning, the marks, the lights) */
        int32_t g, t;
        WFR(26, 160, 86, 50, RGB(0x10, 0x0a, 0x1c));
        for (t = 0; t < 40; t++) { uint32_t an = 128u + (uint32_t)t * 128u / 40u; WFR(68 + 42 * wv_sin4(an + 64u) / 127, 204 + 42 * wv_sin4(an) / 127, 2, 2, RGB(0xb0, 0x60, 0x10)); }
        for (g = 0; g < 2; g++) {
            int32_t gx = g ? 86 : 50, gy = 184, f = g ? clamp(rd_kmh / 16 * 1024 / 240, 0, 1024) : clamp(ev_lvl * 3, 0, 1024);
            uint32_t a = 96u + (uint32_t)f * 192u / 1024u;
            for (t = 0; t < 10; t++) { uint32_t at = 96u + (uint32_t)t * 21u; WFR(gx + 12 * wv_sin4(at + 64u) / 127, gy + 12 * wv_sin4(at) / 127, 1, 1, RGB(0xd0, 0x70, 0x20)); }
            ne_line(gx, gy, gx + 10 * wv_sin4(a + 64u) / 127, gy + 10 * wv_sin4(a) / 127, RGB(0xff, 0x90, 0x30));
        }
        ne_ring(36, 196, 4, RGB(0x60, 0x30, 0x10)), ne_ring(100, 196, 4, RGB(0x60, 0x30, 0x10));
        for (t = 0; t < 4; t++) WFR(54 + t * 8, 202, 5, 2, (int32_t)trk_level((uint32_t)t) && sp_s[t * 4] > 6 ? (t == 1 ? RGB(0x40, 0xe0, 0x40) : t == 3 ? RGB(0xe0, 0xc0, 0x20) : RGB(0xe0, 0x30, 0x30)) : RGB(0x30, 0x10, 0x10));
    }
    {                                                                  /* the wheel: a ring, three spokes, turned by the bend */
        int32_t cx = 68, cy = 196, R = 44;
        uint32_t q;
        for (q = 0; q < 64u; q++) { uint32_t an = q * 4u; WFR(cx + R * wv_sin4(an + 64u) / 127 - 2, cy + R * wv_sin4(an) / 127 - 2, 5, 5, RGB(0x34, 0x22, 0x66)); }
        for (q = 0; q < 3u; q++) { uint32_t an = (uint32_t)(64 + (int32_t)q * 85 + tilt * 4); ne_line3(cx, cy, cx + (R - 2) * wv_sin4(an + 64u) / 127, cy + (R - 2) * wv_sin4(an) / 127, RGB(0x20, 0x14, 0x40)); }
        wvf_disk(cx, cy, 10, RGB(0x24, 0x18, 0x44)), wvf_disk(cx, cy, 3, RGB(0xff, 0x80, 0xc0));
    }
}
/* SYNTH: the synthwave land (after the grid ones): a striped sun behind a city in neon outlines, mountains, the grid running at the tempo, and
 * a little car seen from above the road, its cyan trail snaking behind it with the melody */
static int32_t sy_path[24], sy_x;
static uint8_t sy_col[24];
static UI_NOINLINE void v2_synth(void)
{
    int32_t i, y, x, hy = 128, sp = (int32_t)(wv_ph * (uint32_t)(2 + clamp(song.g[G_BPM], 40, 240) / 40));
    pc_sky(RGB(0x08, 0x04, 0x20), RGB(0x40, 0x10, 0x60), hy);
    for (i = 0; i < 30; i++) WFR((int32_t)(pc_h((uint32_t)i * 3u) % 240u), (int32_t)(pc_h((uint32_t)i * 11u) % 90u), 1 + (int32_t)(i % 7 == 0), 1 + (int32_t)(i % 7 == 0), RGB(0x60, 0xf0, 0xff));
    {                                                                  /* the sun: yellow to pink, its stripes running down, wider with the bass */
        int32_t R = 46 + wv_bass() / 6, cy = 92;
        for (y = cy - R; y < hy; y++) {
            int32_t dy = y - cy, hw = 0, t = (y - (cy - R)) * 255 / (R * 2), gap = dy > -6 ? (dy + 6) / 6 + wv_bass() / 8 : 0;
            while ((hw + 1) * (hw + 1) + dy * dy <= R * R) hw++;
            if (gap && ((uint32_t)(y - (int32_t)(wv_ph / 2u)) % 9u) < (uint32_t)gap) continue;
            WFR(120 - hw, y, 2 * hw, 1, RGB(0xff, 0xe8 - t * 3 / 4 > 0x20 ? 0xe8 - t * 3 / 4 : 0x20, 0x20 + t * 3 / 4 > 0xff ? 0xff : 0x20 + t * 3 / 4));
        }
    }
    for (i = 0; i < 13; i++) {                                         /* the city: neon outlines in front of the sun */
        uint32_t h = pc_h((uint32_t)i * 13u + 5u);
        int32_t bx = 64 + i * 9, bw = 7 + (int32_t)(h % 5u), bh = 18 + (int32_t)((h >> 4) % 36u) + (wf_spec[(uint32_t)i * 19u / 13u] > 12 ? 3 : 0), t = hy - 8 - bh;
        WFR(bx, t, bw, bh, RGB(0x14, 0x06, 0x30));
        WFR(bx, t, 1, bh, RGB(0x40, 0xe0, 0xff)), WFR(bx + bw, t, 1, bh, RGB(0x40, 0xe0, 0xff)), WFR(bx, t, bw, 1, RGB(0x40, 0xe0, 0xff));
        if (h & 1u) WFR(bx + bw / 2, t - 6, 1, 6, RGB(0x40, 0xe0, 0xff));
        for (y = t + 4; y < hy - 10; y += 5) {
            int32_t lit = (hy - 10 - y) < (int32_t)sp_s[(uint32_t)i * 19u / 13u] * 4, nt = (nk_mask >> ((uint32_t)i * 2u)) & 3u;
            uint16_t wc = punch.cur >= 0 ? sh_note_col((int32_t)((wv_ph / 2u + (uint32_t)y) % 27u)) : nt ? WC_WHT : lit ? RGB(0xff, 0x70, 0xe0) : RGB(0x30, 0x60, 0x90);
            if (((uint32_t)y + h) % 3u != 1u) WFR(bx + 2, y, bw - 3, 1, wc);
        }
    }
    for (x = 0; x < 240; x++) {                                        /* the mountains: a zigzag in pink */
        int32_t m = 10 - ((x * 3) % 40 < 20 ? (x * 3) % 40 : 40 - (x * 3) % 40) / 2 - (x > 90 && x < 150 ? 0 : 4);
        WFR(x, hy - m, 1, m, RGB(0x10, 0x04, 0x20)), WFR(x, hy - m, 1, 1, RGB(0xff, 0x40, 0xd0));
    }
    WFR(0, hy, 240, 240 - hy, RGB(0x08, 0x02, 0x14)), WFR(0, hy, 240, 2, RGB(0xff, 0x60, 0xe0));
    for (i = 0; i < 12; i++) {                                         /* the grid: lines coming at us, rays from the middle */
        int32_t z = 255 - (int32_t)(((uint32_t)i * 22u + (uint32_t)sp) % 256u), yy = hy + 2 + 2600 / (z + 24) - 10;
        if (yy > hy && yy < 240) WFR(0, yy, 240, 1, wvf_hit_n > 2 ? RGB(0xff, 0x90, 0xff) : RGB(0xd0, 0x20, 0xc0));
    }
    for (i = -12; i <= 12; i++) wvf_pline(120 + i * 4, hy + 2, 120 + i * 40, 240, RGB(0xa0, 0x10, 0xa0));
    {                                                                  /* the car, steered by the notes as along a line only it can see; its two trails */
        int32_t tx, k = nk_top(nk_mask), cy = 166;
        tx = k >= 0 ? clamp((k - 13) * 7, -80, 80) : sy_x * 7 / 8;
        sy_x += (tx - sy_x) / 4;
        for (i = 23; i > 0; i--) sy_path[i] = sy_path[i - 1], sy_col[i] = sy_col[i - 1];
        sy_path[0] = sy_x, sy_col[0] = (uint8_t)(k < 0 ? sy_col[1] : k < 13 ? 0 : 1);
        for (i = 1; i < 24; i++) {                                     /* (each step back is nearer us: lower and wider) */
            int32_t y0 = cy + 6 + (i - 1) * 3, y1 = cy + 6 + i * 3, s0 = 240 + (i - 1) * 14, s1 = 240 + i * 14, w0 = 7 + (i - 1) / 2, w1 = 7 + i / 2, l;
            uint16_t c = sy_col[i] ? RGB(0x40, 0xa0, 0xff) : RGB(0xff, 0x30, 0x50), c2 = sy_col[i] ? RGB(0xb0, 0xe8, 0xff) : RGB(0xff, 0xb0, 0xb8);
            for (l = -1; l <= 1; l += 2) {
                wvf_pline(120 + sy_path[i - 1] * s0 / 240 + l * w0, y0, 120 + sy_path[i] * s1 / 240 + l * w1, y1, c2);
                wvf_pline(121 + sy_path[i - 1] * s0 / 240 + l * w0, y0, 121 + sy_path[i] * s1 / 240 + l * w1, y1, c);
            }
        }
        x = 120 + sy_x;
        WFR(x - 10, cy - 4, 21, 10, RGB(0x10, 0x10, 0x30)), WFR(x - 10, cy - 4, 21, 1, RGB(0x60, 0xf0, 0xff)), WFR(x - 10, cy - 4, 1, 10, RGB(0x60, 0xf0, 0xff)), WFR(x + 10, cy - 4, 1, 10, RGB(0x60, 0xf0, 0xff));
        WFR(x - 7, cy - 10, 15, 6, RGB(0x60, 0xf0, 0xff)), WFR(x - 5, cy - 9, 11, 4, RGB(0x10, 0x10, 0x30));
        WFR(x - 9, cy + 3, 4, 3, sy_col[0] ? RGB(0x60, 0xc0, 0xff) : RGB(0xff, 0x40, 0x50)), WFR(x + 6, cy + 3, 4, 3, sy_col[0] ? RGB(0x60, 0xc0, 0xff) : RGB(0xff, 0x40, 0x50));
    }
}
/* ---- the lit city (NEO's): a sky, searchlights behind, far towers whose windows the bands light in colours, near towers in two faces with
 * a ledge, antennas and water tanks, whose yellow windows rise in each column as bars of the spectrum (smoothed: it breathes, it does not
 * flicker). Every scene with a city uses it */
typedef struct { uint16_t sky0, sky1, beam, far, farw0, farw1, near, nearl, win, wind; } nc_pal_t;
static const nc_pal_t NC_NEO = {RGB(0x06, 0x18, 0x26), RGB(0x14, 0x40, 0x56), RGB(0x58, 0x70, 0x48), RGB(0x10, 0x2c, 0x44), RGB(0xff, 0x60, 0xd0), RGB(0x60, 0xf0, 0xff),
                                RGB(0x06, 0x12, 0x1e), RGB(0x10, 0x24, 0x34), RGB(0xff, 0xd8, 0x70), RGB(0x14, 0x1c, 0x24)};
static const nc_pal_t NC_BLUE = {RGB(0x02, 0x02, 0x10), RGB(0x08, 0x10, 0x60), RGB(0x30, 0x40, 0x90), RGB(0x20, 0x3c, 0x60), RGB(0xff, 0xf0, 0xa0), RGB(0xc0, 0xd8, 0xff),
                                 RGB(0x24, 0x48, 0x60), RGB(0x40, 0x70, 0x88), RGB(0xff, 0xe0, 0x40), RGB(0x10, 0x20, 0x30)};
static const nc_pal_t NC_PURPLE = {RGB(0x28, 0x18, 0x70), RGB(0x80, 0x40, 0xb0), RGB(0x90, 0x60, 0xd0), RGB(0x50, 0x38, 0xa0), RGB(0xff, 0xa0, 0xe0), RGB(0xa0, 0xe0, 0xff),
                                   RGB(0x30, 0x1c, 0x60), RGB(0x48, 0x2c, 0x84), RGB(0xd0, 0x90, 0xff), RGB(0x28, 0x18, 0x50)};
static UI_NOINLINE void nc_spec(void) { uint32_t i; for (i = 0; i < 19u; i++) sp_s[i] = (uint8_t)(sp_s[i] + ((int32_t)wf_spec[i] - (int32_t)sp_s[i]) / 3); }
static UI_NOINLINE void nc_tower(int32_t x, int32_t t, int32_t w, int32_t base, const nc_pal_t *p, uint32_t h, int32_t band)
{
    int32_t r, c, rows = (base - t - 6) / 6, lit = clamp((int32_t)sp_s[band % 19] * rows / 18 + (int32_t)(h % 3u), 0, rows);
    WFR(x, t, w, base - t, p->near), WFR(x, t, w / 3, base - t, p->nearl);           /* two faces */
    WFR(x - 1, t, w + 2, 2, p->nearl);                                                /* the ledge */
    if (h & 1u) WFR(x + w / 2, t - 10, 1, 10, p->nearl), WFR(x + w / 2 - 1, t - 11, 3, 1, ((wv_ph / 8u + h) & 1u) ? RGB(0xff, 0x40, 0x40) : p->near);
    if (h & 2u) WFR(x + w - 9, t - 6, 6, 6, p->nearl), WFR(x + w - 8, t - 8, 4, 2, p->nearl);
    for (r = 0; r < rows; r++)
        for (c = 0; c < (w - 4) / 5; c++) {
            int on = rows - 1 - r < lit && ((uint32_t)(r * 3 + c) + h) % 5u != 0u;
            WFR(x + 3 + c * 5, t + 5 + r * 6, 3, 3, on ? (c == 0 ? RGB(0xff, 0xf4, 0xc0) : p->win) : p->wind);
        }
}
static UI_NOINLINE void nc_city(int32_t base, int32_t scroll, const nc_pal_t *p, int beams)
{
    int32_t i, r, c;
    for (i = 0; i < base / 4 + 1; i++) {
        int32_t t = i * 4 * 255 / base;
        WFR(0, i * 4, 240, 4, RGB((((p->sky0 >> 11) & 31) * (255 - t) + ((p->sky1 >> 11) & 31) * t) / 255 << 3, (((p->sky0 >> 5) & 63) * (255 - t) + ((p->sky1 >> 5) & 63) * t) / 255 << 2,
                                  ((p->sky0 & 31) * (255 - t) + (p->sky1 & 31) * t) / 255 << 3));
    }
    for (i = 0; i < 20; i++) cv_pset((int32_t)(pc_h((uint32_t)i * 17u) % 240u), (int32_t)(pc_h((uint32_t)i * 29u) % (uint32_t)(base / 2)), RGB(0xd0, 0xd8, 0xf0));
    if (beams) for (i = 0; i < 3; i++) {                               /* searchlights: they sweep, faster with the beat */
        int32_t bx = 50 + i * 70, sw = WV_SIN[(wv_ph * (1u + (uint32_t)(ev_rate > 40)) + (uint32_t)i * 20u) & 63u] * 70 / 127;
        wvf_pline(bx, base, bx + sw, 0, p->beam), wvf_pline(bx + 1, base, bx + sw + 3, 0, p->beam);
    }
    for (i = -1; i < 13; i++) {                                        /* the far towers */
        uint32_t h = pc_h((uint32_t)(i + scroll / 21 + 100) * 7u);
        int32_t x = i * 21 - ((scroll % 21) + 21) % 21, hh = base / 2 + (int32_t)(h % (uint32_t)(base / 2)), t = base - 12 - hh / 2 - 30;
        WFR(x, t, 19, base - t, p->far);
        if (h % 5u == 0u) WFR(x + 9, t - 14, 1, 14, p->far);
        for (r = 0; r < (base - t) / 6 - 1; r++) for (c = 0; c < 4; c++)
            if (sp_s[(uint32_t)(i + r + c + 19) % 19u] > 6 && ((uint32_t)(r * 3 + c) + h) % 5u < 3u) WFR(x + 2 + c * 4, t + 4 + r * 6, 2, 3, (h + (uint32_t)r) % 3u == 0u ? p->farw0 : p->farw1);
    }
    for (i = -1; i < 7; i++) {                                         /* the near ones */
        uint32_t h = pc_h((uint32_t)(i + scroll / 44 + 300) * 13u);
        int32_t x = i * 44 - 10 - ((scroll % 44) + 44) % 44 + 44, hh = base / 4 + (int32_t)(h % (uint32_t)(base / 4));
        nc_tower(x, base - hh, 36, base, p, h, i * 3 + 19);
    }
}
static UI_NOINLINE void v2_neo(void)                                     /* NEO: the lit city, its street lamps */
{
    int32_t i;
    nc_city(182, 0, &NC_NEO, 1);
    WFR(0, 182, 240, 58, RGB(0x08, 0x0a, 0x10));
    for (i = 0; i < 8; i++) { wvf_disk(14 + i * 30, 178, 5, RGB(0x14, 0x40, 0x28)); wvf_disk(14 + i * 30, 178, 2, sp_s[i * 2] > 9 ? WC_WHT : RGB(0x80, 0xff, 0xa0)); }
    for (i = 0; i < 8; i++) WFR(10 + i * 30, 188 + (int32_t)(i & 1) * 18, 20, 2, RGB(0x30, 0x40, 0x30));   /* (the street's reflections) */
}
/* SMOKE: over someone's shoulder, the lit city ahead; the arm brings a cigarette up, its tip glowing, and the smoke rises, swaying with the
 * music and puffing on the beat (black pixels, as the person on the bridge) */
static int16_t sm_p[24][3];                    /* smoke: x, y (1/4 px), age */
static UI_NOINLINE void v2_smoke(void)
{
    static const char *const SM_BODY[] = {"......XXXXX.......", ".....XXXXXXX......", "....XXXXXXXXX.....", "....XXXXXXXXX.....", "....XXXXXXXXX.....",
        ".....XXXXXXX......", "......XXXXX.......", ".......XXX........", "....XXXXXXXXXX....", "..XXXXXXXXXXXXXX..", ".XXXXXXXXXXXXXXXXX", "XXXXXXXXXXXXXXXXXX",
        "XXXXXXXXXXXXXXXXXX", "XXXXXXXXXXXXXXXXXX", "XXXXXXXXXXXXXXXXXX", "XXXXXXXXXXXXXXXXXX", "XXXXXXXXXXXXXXXXXX", "XXXXXXXXXXXXXXXXXX"};
    uint32_t cyc = wv_ph % 120u;                                       /* the arm: down, up to the mouth (a drag), down */
    int32_t i, r, c, up = cyc < 20u ? (int32_t)cyc : cyc < 50u ? 20 : cyc < 70u ? 70 - (int32_t)cyc : 0, hx = 136, hy = 118;
    int32_t tipx, tipy;
    nc_city(196, 0, &NC_BLUE, 1);
    WFR(0, 196, 240, 44, RGB(0x04, 0x06, 0x10));
    for (i = 0; i < 12; i++) WFR(i * 20 + 4, 200 + (int32_t)(i & 1) * 6, 12, 1, RGB(0x60, 0x58, 0x30));
    for (r = 0; r < 18; r++) for (c = 0; c < 18; c++) if (SM_BODY[r][c] == 'X') WFR(hx + c * 6, hy + r * 6 + (int32_t)((wv_ph / 30u) & 1u), 6, 6, RGB(0x02, 0x02, 0x06));
    {                                                                  /* the arm: from the shoulder, the elbow out, the hand from the hip up to the mouth */
        int32_t hxr = hx + 4, hyr = hy + 110, hxm = hx + 22, hym = hy + 34, ex = hx + 2 - up / 2, ey = hy + 96 - up;
        tipx = hxr + (hxm - hxr) * up / 20, tipy = hyr + (hym - hyr) * up / 20;
        for (i = 0; i < 5; i++) ne_line3(hx + 24 + i, hy + 66, ex + i, ey, RGB(0x02, 0x02, 0x06));
        for (i = 0; i < 4; i++) ne_line3(ex + i, ey, tipx + 4 + i, tipy + 2, RGB(0x02, 0x02, 0x06));
        wvf_disk(tipx + 6, tipy + 2, 6, RGB(0x02, 0x02, 0x06));
        tipx -= 2;
    }
    WFR(tipx - 4, tipy, 8, 2, RGB(0xe0, 0xe0, 0xe0)), WFR(tipx - 6, tipy, 2, 2, cyc >= 20u && cyc < 50u ? RGB(0xff, 0xa0, 0x30) : RGB(0xd0, 0x40, 0x20));
    for (i = 0; i < 24; i++) {                                         /* the smoke */
        int16_t *q = sm_p[i];
        if (!q[2]) {
            if ((wv_ph + (uint32_t)i) % 6u == 0u || (wvf_hit_n == 5 && i % 3 == 0)) { q[0] = (int16_t)((tipx - 6) * 4), q[1] = (int16_t)(tipy * 4), q[2] = 1; }
            else if (cyc >= 52u && cyc < 64u && i % 2 == 0) { q[0] = (int16_t)((hx + 14 - (int32_t)(cyc - 52u) * 2) * 4), q[1] = (int16_t)((hy + 36) * 4), q[2] = 8; }   /* the breath out, from the mouth */
            continue;
        }
        q[2]++, q[1] = (int16_t)(q[1] - 3 - (int16_t)(ev_lvl / 60)), q[0] = (int16_t)(q[0] + WV_SIN[((uint32_t)q[2] * 4u + (uint32_t)i * 9u + wv_ph) & 63u] * (2 + wv_bass() / 4) / 127 - 1);
        if (q[2] > 60 || q[1] < 0) { q[2] = 0; continue; }
        {
            int32_t rr = 1 + q[2] / 12, x = q[0] / 4, y = q[1] / 4, xx, yy;
            for (yy = -rr; yy <= rr; yy++) for (xx = -rr; xx <= rr; xx++) if (xx * xx + yy * yy <= rr * rr && ((x + xx + y + yy) & 1)) cv_pset(x + xx, y + yy, q[2] < 30 ? RGB(0xb0, 0xb8, 0xc8) : RGB(0x70, 0x78, 0x90));
        }
    }
}
/* SKYLINE: a city in neon lines against a huge sun with bands, on a floor of tiles that mirrors it; someone stands there looking (after the
 * synthwave city with the figure) */
static UI_NOINLINE void v2_skyline(void)
{
    int32_t i, y, R = 70 + wv_bass() / 4, hy = 156;
    for (y = 0; y < hy; y += 2) WFR(0, y, 240, 2, RGB(0x10 + y / 10, 0x08, 0x28 + y / 8));
    for (i = 0; i < 60; i++) cv_pset((int32_t)(pc_h((uint32_t)i * 31u) % 240u), (int32_t)(pc_h((uint32_t)i * 13u) % (uint32_t)hy), sp_s[i % 19] > 9 ? WC_WHT : RGB(0x80, 0x70, 0xa0));
    for (y = 96 - R; y < hy; y++) {                                    /* the sun */
        int32_t dy = y - 96, hw = 0, t = (y - (96 - R)) * 255 / (2 * R);
        while ((hw + 1) * (hw + 1) + dy * dy <= R * R) hw++;
        if (dy > 20 && ((uint32_t)(y - (int32_t)(wv_ph / 3u)) % 10u) < (uint32_t)(dy / 14)) continue;
        WFR(120 - hw, y, 2 * hw, 1, RGB(0xff, 0x40 + t / 2 > 0xd0 ? 0xd0 : 0x40 + t / 2, 0xa0 - t / 3 > 0x40 ? 0xa0 - t / 3 : 0x40));
    }
    for (i = 0; i < 16; i++) {                                         /* the towers in lines */
        uint32_t h = pc_h((uint32_t)i * 7u + 3u);
        int32_t bw = 12 + (int32_t)(h % 10u), x = 26 + i * 12 - bw / 3, bh = 30 + (int32_t)((h >> 4) % 70u) * (i > 3 && i < 12 ? 1 : 1) / (i > 3 && i < 12 ? 1 : 2), t = hy - bh, r;
        uint16_t ln = RGB(0x80, 0xe8, 0xff);
        WFR(x, t, bw, bh, RGB(0x18, 0x10, 0x38));
        WFR(x, t, 1, bh, ln), WFR(x + bw - 1, t, 1, bh, ln), WFR(x, t, bw, 1, ln);
        for (r = t + 4; r < hy - 2; r += 4) if (((uint32_t)r + h) % 3u != 0u) {
            int32_t ht = hy - r, on = (int32_t)sp_s[(uint32_t)i * 19u / 16u] * 6 > ht / 2;
            WFR(x + 2, r, bw - 4, 1, !on ? RGB(0x40, 0x60, 0x90) : ht < 30 ? RGB(0x60, 0xe8, 0xff) : ht < 60 ? RGB(0xff, 0x60, 0xd0) : ht < 85 ? RGB(0xff, 0xa0, 0x60) : RGB(0xff, 0xf0, 0x90));
        }
        if (h & 1u) WFR(x + bw / 2, t - 8, 1, 8, ln);
    }
    for (y = hy; y < 240; y++) {                                       /* the floor: the sun and the towers mirrored, lines of tiles */
        int32_t d = y - hy, my = hy - d * 2 / 3 - 1;
        uint16_t *row = cv_px + (uint32_t)(y - wf_ry) * 240u, *src = cv_px + (uint32_t)(my - wf_ry) * 240u;
        int32_t x;
        if (y < wf_ry || y >= wf_ry + wf_rh) continue;
        if (my >= wf_ry && my < wf_ry + wf_rh) { for (x = 0; x < 240; x++) row[x] = swap16((swap16(src[(x + WV_SIN[((uint32_t)y * 6u + wv_ph * 2u) & 63u] / 60 + 240) % 240]) >> 1) & 0x7BEFu); }
        else WFR(0, y, 240, 1, RGB(0x30, 0x20, 0x50));
        if (d % ((d / 8) + 3) == 0) WFR(0, y, 240, 1, RGB(0x80, 0x40, 0x90));
    }
    for (i = -6; i <= 6; i++) wvf_pline(120 + i * 14, hy, 120 + i * 60, 240, RGB(0x80, 0x40, 0x90));
    WFR(116, 196, 8, 28, RGB(0x06, 0x04, 0x0c)), wvf_disk(120, 192, 5, RGB(0x06, 0x04, 0x0c)), WFR(114, 200, 12, 14, RGB(0x06, 0x04, 0x0c));   /* someone */
    WFR(116, 224, 3, 8, RGB(0x06, 0x04, 0x0c)), WFR(121, 224, 3, 8, RGB(0x06, 0x04, 0x0c));
    for (i = 0; i < 8; i++) WFR(114 + (i & 1), 232 + i, 12, 1, RGB(0x10, 0x08, 0x18));
}
/* FALL: falling between lit towers (the city's windows, ledges, the searchlights behind), the haze pink below */
static UI_NOINLINE void v2_fall(void)
{
    int32_t i, L, y, r, c;
    uint32_t a = fall_y * 2u;
    for (y = 0; y < 240; y += 4) WFR(0, y, 240, 4, RGB(0x30 + y / 2 > 0xf0 ? 0xf0 : 0x30 + y / 2, 0x18 + y / 6, 0x60 + y / 8));
    for (i = 0; i < 3; i++) { int32_t bx = 70 + i * 50, sw = WV_SIN[(wv_ph + (uint32_t)i * 21u) & 63u] * 50 / 127; wvf_pline(bx, 240, bx + sw, 0, RGB(0xd0, 0x90, 0xd0)), wvf_pline(bx + 1, 240, bx + sw + 2, 0, RGB(0xd0, 0x90, 0xd0)); }
    for (L = 0; L < 3; L++) {                                          /* towers: far and pale to near and dark; their windows rush up */
        static const uint16_t TC[3] = {RGB(0xa0, 0x60, 0xa8), RGB(0x60, 0x30, 0x80), RGB(0x24, 0x10, 0x40)};
        static const uint16_t TL[3] = {RGB(0xc0, 0x80, 0xc0), RGB(0x80, 0x48, 0xa0), RGB(0x40, 0x24, 0x60)};
        int32_t sp = 1 + L * 2, w = 16 + L * 14, n = L == 2 ? 2 : 3;
        for (i = 0; i < n; i++) {
            int32_t x = L == 2 ? (i ? 240 - w + 4 : -4) : (i * 240 / n + L * 41 + 18) % 240 - 8, sc = (int32_t)((fall_y * (uint32_t)sp) % 12u);
            if (L < 2 && x > 90 && x < 150) x += x < 120 ? -50 : 50;
            WFR(x, 0, w, 240, TC[L]), WFR(x, 0, w / 3, 240, TL[L]);
            for (r = -1; r < 21; r++) {
                int32_t wy = r * 12 - sc, band = (r + i * 5 + L * 3 + 40) % 19;
                WFR(x - 1, wy, w + 2, 1, TL[L]);                                  /* (a floor's ledge) */
                for (c = 0; c < (w - 4) / 5; c++) if (((uint32_t)(r + c + i) + fall_y * (uint32_t)sp / 12u) % 4u != 0u)
                    WFR(x + 3 + c * 5, wy + 4, 3, 4, (int32_t)sp_s[band] > 4 + c * 2 ? RGB(0xff, 0xe0, 0x90) : L == 2 ? RGB(0x50, 0x34, 0x70) : RGB(0xe0, 0xa8, 0xe0));
            }
        }
    }
    {                                                                  /* the one falling, turning */
        int32_t cx = 120 + WV_SIN[(wv_ph) & 63u] * 14 / 127, cy = 104 + WV_SIN[(wv_ph * 2u + 9u) & 63u] * 6 / 127;
        int32_t C = wv_sin4(a + 64u), S2 = wv_sin4(a);
        uint16_t k = RGB(0x20, 0x10, 0x40);
        ne_line3(cx - 8 * C / 127, cy - 8 * S2 / 127, cx + 8 * C / 127, cy + 8 * S2 / 127, k);
        wvf_disk(cx - 12 * C / 127, cy - 12 * S2 / 127, 4, k);
        ne_line3(cx + 8 * C / 127, cy + 8 * S2 / 127, cx + 16 * C / 127 - 6 * S2 / 127, cy + 16 * S2 / 127 + 6 * C / 127, k), ne_line3(cx + 8 * C / 127, cy + 8 * S2 / 127, cx + 16 * C / 127 + 5 * S2 / 127, cy + 16 * S2 / 127 - 5 * C / 127, k);
        ne_line3(cx - 4 * C / 127, cy - 4 * S2 / 127, cx - 4 * C / 127 + 10 * S2 / 127, cy - 4 * S2 / 127 - 10 * C / 127, k), ne_line3(cx - 4 * C / 127, cy - 4 * S2 / 127, cx - 4 * C / 127 - 9 * S2 / 127, cy - 4 * S2 / 127 + 9 * C / 127, k);
    }
}
/* DOOMSCROLL: someone on the bridge, phone in hand, its screen lighting the face in a new colour with every note; the sky above: clouds that
 * come in from the sides with the notes (the left keys from the left), birds crossing at the height of their note, a shooting star on the kick */
static int16_t dm_b[10][3], dm_c[8][3];        /* birds: x, y, dir (0 off); clouds: x, y, life */
static uint8_t dm_col;
static UI_NOINLINE void dm_cloud(int32_t cx, int32_t cy, int32_t s)
{
    static const int8_t PF[6][3] = {{0, 0, 9}, {10, -4, 10}, {20, 0, 8}, {-9, 2, 7}, {28, 3, 6}, {6, 4, 9}};
    int32_t i;
    for (i = 0; i < 6; i++) wvf_disk(cx + PF[i][0] * s / 8, cy + PF[i][1] * s / 8 + 2, PF[i][2] * s / 8, RGB(0xd0, 0x98, 0xc0));
    for (i = 0; i < 6; i++) wvf_disk(cx + PF[i][0] * s / 8 - 1, cy + PF[i][1] * s / 8, PF[i][2] * s / 8 - 1, RGB(0xf8, 0xc8, 0xd8));
    for (i = 0; i < 6; i++) wvf_disk(cx + PF[i][0] * s / 8 - 3, cy + PF[i][1] * s / 8 - 3, PF[i][2] * s / 8 / 2, RGB(0xff, 0xe8, 0xf0));
}
static UI_NOINLINE void dm_word(int32_t x, int32_t y);
/* CHORDS: the chord you hold as the ratio of its frequencies (after the "chords as polygons" one): A minor is 10 : 12 : 15, so a 10-gon, a
 * 12-gon and a 15-gon one inside the other; a light runs round each, as many corners a turn as its number, so they all meet at the top once a
 * bar; each corner it passes rings. Two notes: their interval (a fifth, 2 : 3); one: the note and its octave. The name and the ratio above,
 * the floor mirroring the lights. Let go: the last chord stays, dim, turning slowly */
typedef struct { uint16_t mask; uint8_t n, r[4]; const char *name; } ch_tpl_t;
static const ch_tpl_t CH_TPL[] = {
    {0x489, 4, {10, 12, 15, 18}, "MINOR 7"}, {0x891, 4, {8, 10, 12, 15}, "MAJOR 7"}, {0x491, 4, {4, 5, 6, 7}, "7"},
    {0x89, 3, {10, 12, 15}, "MINOR"}, {0x91, 3, {4, 5, 6}, "MAJOR"}, {0x49, 3, {5, 6, 7}, "DIM"}, {0x111, 3, {16, 20, 25}, "AUG"},
    {0xA1, 3, {6, 8, 9}, "SUS4"}, {0x85, 3, {8, 9, 12}, "SUS2"},
    {0x81, 2, {2, 3}, "FIFTH"}, {0x21, 2, {3, 4}, "FOURTH"}, {0x11, 2, {4, 5}, "THIRD"}, {0x09, 2, {5, 6}, "MINOR 3RD"}, {0x201, 2, {3, 5}, "SIXTH"},
    {0x101, 2, {5, 8}, "MINOR 6TH"}, {0x05, 2, {8, 9}, "SECOND"}, {0x401, 2, {9, 16}, "MINOR 7TH"}, {0x801, 2, {8, 15}, "SEVENTH"}, {0x41, 2, {5, 7}, "TRITONE"},
    {0x03, 2, {15, 16}, "HALF STEP"}, {0x01, 2, {1, 2}, "OCTAVE"}};
static const char *const CH_NOTE[12] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
static int32_t ch_tpl = 3, ch_root = 9, ch_rings[10][4];
static uint32_t ch_ph, ch_seg[4];
static UI_NOINLINE void ch_find(uint32_t keys)
{
    uint32_t pcs = 0, k, best = 0;
    int32_t t, root, bt = -1, br = 0;
    for (k = 0; k < 27u; k++) if ((keys >> k) & 1u) pcs |= 1u << ((k + 5u) % 12u);   /* (key 0 is the F) */
    if (!pcs) return;
    for (root = 0; root < 12; root++) {
        uint32_t rel;
        if (!((pcs >> root) & 1u)) continue;
        rel = ((pcs >> root) | (pcs << (12 - root))) & 0xFFFu;
        for (t = 0; t < (int32_t)(sizeof CH_TPL / sizeof CH_TPL[0]); t++) {
            uint32_t m = CH_TPL[t].mask, sc = (rel & m) == m ? (m == rel ? 100u : 10u) + CH_TPL[t].n : 0u;
            if (sc > best) best = sc, bt = t, br = root;
        }
    }
    if (bt >= 0) ch_tpl = bt, ch_root = br;
}
static UI_NOINLINE void v5_chords(void)
{
    static const uint16_t VC[4] = {RGB(0xc8, 0x98, 0xff), RGB(0xf0, 0xc0, 0x60), RGB(0x70, 0xe0, 0xff), RGB(0xff, 0x80, 0xc0)};
    const ch_tpl_t *t;
    int32_t i, v, lit = nk_mask != 0u, cx = 120, cy = 112, minor;
    char b[24];
    if (lit) ch_find(nk_mask);
    t = &CH_TPL[ch_tpl], minor = t->mask == 0x89 || t->mask == 0x489 || t->mask == 0x49;
    ch_ph += song.playing || lit ? (uint32_t)clamp(song.g[G_BPM], 40, 240) * 14u : 260u;   /* (a turn a bar when it plays; a slow drift when not) */
    WFR(0, 0, 240, 240, C_BLACK);
    wvf_disk(cx, cy, 104, minor ? RGB(0x0c, 0x06, 0x1c) : RGB(0x1c, 0x0e, 0x06)), wvf_disk(cx, cy, 70, minor ? RGB(0x14, 0x0a, 0x2c) : RGB(0x2a, 0x16, 0x08));
    ne_ring(cx, 204, 3, RGB(0x20, 0x20, 0x30));
    for (i = 0; i < 48; i++) { uint32_t an = (uint32_t)i * 16u / 3u; WFR(cx + 96 * wv_sin4(an + 64u) / 127, 204 + 12 * wv_sin4(an) / 127, 1, 1, RGB(0x40, 0x40, 0x58)); }   /* (the floor) */
    for (v = 0; v < t->n; v++) {
        int32_t n = t->r[v], R = 40 + v * (t->n == 4 ? 20 : t->n == 3 ? 26 : 34), k, px = 0, py = 0, x0 = 0, y0 = 0;
        uint32_t f = (ch_ph & 0xFFFFu) * (uint32_t)n, seg = f >> 16, fr = f & 0xFFFFu;
        uint16_t c = lit ? VC[v] : RGB(0x60, 0x58, 0x78);
        if (n <= 1) { ne_ring(cx, cy, R, c); x0 = cx, y0 = cy - R; }   /* (one: a circle) */
        else for (k = 0; k <= n; k++) {                                /* the polygon (two sides: a line across) */
            uint32_t an = 192u + (uint32_t)(k % n) * 256u / (uint32_t)n;
            int32_t x = cx + R * wv_sin4(an + 64u) / 127, y = cy + R * wv_sin4(an) / 127;
            if (k) wvf_pline(px, py, x, y, c);
            if (k < n) WFR(x - 1, y - 1, 2, 2, lit ? WC_WHT : c);
            px = x, py = y;
        }
        if (n > 1) {                                                   /* its light, between the corners seg and seg + 1 */
            uint32_t a0 = 192u + seg * 256u / (uint32_t)n, a1 = 192u + (seg + 1u) * 256u / (uint32_t)n;
            int32_t xa = cx + R * wv_sin4(a0 + 64u) / 127, ya = cy + R * wv_sin4(a0) / 127, xb = cx + R * wv_sin4(a1 + 64u) / 127, yb = cy + R * wv_sin4(a1) / 127;
            x0 = xa + (xb - xa) * (int32_t)(fr >> 8) / 256, y0 = ya + (yb - ya) * (int32_t)(fr >> 8) / 256;
            if (seg != ch_seg[v]) {                                    /* a corner passed: it rings */
                for (k = 0; k < 10; k++) if (!ch_rings[k][2]) { ch_rings[k][0] = xa, ch_rings[k][1] = ya, ch_rings[k][2] = 8, ch_rings[k][3] = v; break; }
                ch_seg[v] = seg;
            }
        }
        wvf_disk(x0, y0, lit ? 4 : 2, c), wvf_disk(x0, y0, lit ? 2 : 1, WC_WHT);
        WFR(x0 - 1, 204 + (204 - y0) / 8, 3, 2, c);                    /* (its glint on the floor) */
    }
    for (i = 0; i < 10; i++) if (ch_rings[i][2]) { ne_ring(ch_rings[i][0], ch_rings[i][1], 3 + (8 - ch_rings[i][2]) * 2, lit ? VC[ch_rings[i][3] & 3] : RGB(0x50, 0x48, 0x68)); ch_rings[i][2]--; }
    str_cpy(b, CH_NOTE[ch_root], 24);
    str_cpy(b + str_len(b), " ", 24 - str_len(b)), str_cpy(b + str_len(b), t->name, 24 - str_len(b));
    wf_textc(120, 8, 1, b, lit ? (minor ? RGB(0xd0, 0xb0, 0xff) : RGB(0xff, 0xd0, 0x90)) : RGB(0x70, 0x68, 0x88));
    for (v = 0, b[0] = 0; v < t->n; v++) { if (v) str_cpy(b + str_len(b), " : ", 24 - str_len(b)); fmt_int(b + str_len(b), t->r[v]); }
    wf_textc(120, 24, 0, b, RGB(0xa0, 0x98, 0xb8));
}
/* ORB: a ring on a string (after the "plasma in a ring" one, laid flat): a line across the screen, shaking with the sound, through a circle
 * that fills from its rim inwards while the music plays (and empties again in the quiet) with water in a few blues and an orange glint.
 * The notes drop into it on the rim (each key its place), the kick runs a wave along the string, the loop's beat pulses from the rim, an
 * effect stirs a whirl. 60 x 60 cells of 4 px; the water's two heights in the picture memory (8 bits each) */
#define OB_G 60
#define OB_R 22
static int32_t ob_fill = OB_R * 16, ob_sw, ob_flip;
static uint32_t ob_lb;
static UI_NOINLINE int32_t ob_d2(int32_t x, int32_t y) { return (x - OB_G / 2) * (x - OB_G / 2) + (y - OB_G / 2) * (y - OB_G / 2); }
static UI_NOINLINE void ob_drop(int32_t x, int32_t y, int32_t v)
{
    int8_t *A = (int8_t *)wv_mem + (ob_flip ? OB_G * OB_G : 0);
    int32_t i, j;
    for (j = -1; j <= 1; j++) for (i = -1; i <= 1; i++)                /* (a round drop: the middle, and half around it) */
        if (x + i > 0 && y + j > 0 && x + i < OB_G - 1 && y + j < OB_G - 1) { int32_t n = A[(y + j) * OB_G + x + i] + (i || j ? v / 2 : v); A[(y + j) * OB_G + x + i] = (int8_t)clamp(n, -127, 127); }
}
static UI_NOINLINE void orb_step(void)
{
    int8_t *A, *B;
    int32_t x, y, k, it, r0, play = song.playing || ev_lvl > 12 || nk_mask;
    uint32_t bt = wv_ph * (uint32_t)clamp(song.g[G_BPM], 40, 240) / 600u;
    ob_fill += play ? -3 : 2;                                          /* (it fills while it plays, empties in the quiet) */
    ob_fill = clamp(ob_fill, 0, OB_R * 16);
    r0 = ob_fill / 16;
    for (k = 0; k < 27; k++) if ((nk_on >> k) & 1u) {                 /* a note: a drop on the rim, at its key's place */
        uint32_t an = (uint32_t)k * 256u / 27u + 192u;
        ob_drop(OB_G / 2 + (OB_R - 2) * wv_sin4(an + 64u) / 127, OB_G / 2 + (OB_R - 2) * wv_sin4(an) / 127, 110);
    }
    if (wvf_hit_n == 5) for (x = OB_G / 2 - OB_R; x <= OB_G / 2 + OB_R; x += 2) ob_drop(x, OB_G / 2, (x & 2) ? 70 : -70);   /* the kick: along the string */
    if (song.playing && bt != ob_lb) for (k = 0; k < 32; k++) { uint32_t an = (uint32_t)k * 8u; ob_drop(OB_G / 2 + (OB_R - 1) * wv_sin4(an + 64u) / 127, OB_G / 2 + (OB_R - 1) * wv_sin4(an) / 127, 36); }
    ob_lb = bt;
    if (punch.cur >= 0 || thr_req) { ob_sw += 14; ob_drop(OB_G / 2 + (OB_R * 2 / 3) * wv_sin4((uint32_t)ob_sw + 64u) / 127, OB_G / 2 + (OB_R * 2 / 3) * wv_sin4((uint32_t)ob_sw) / 127, 90); }
    if (play && !(wv_ph % 11u)) ob_drop(OB_G / 2 - OB_R + (int32_t)(pc_h(wv_ph) % (uint32_t)(OB_R * 2)), OB_G / 2 - OB_R + (int32_t)(pc_h(wv_ph * 7u) % (uint32_t)(OB_R * 2)), 40);
    for (it = 0; it < 2; it++) {                                       /* the water: each cell the mean of its four, less where it was, a little damped */
        A = (int8_t *)wv_mem + (ob_flip ? OB_G * OB_G : 0), B = (int8_t *)wv_mem + (ob_flip ? 0 : OB_G * OB_G);
        for (y = 1; y < OB_G - 1; y++) for (x = 1; x < OB_G - 1; x++) {
            int32_t i = y * OB_G + x, d2 = ob_d2(x, y), n;
            if (d2 > OB_R * OB_R || d2 < r0 * r0) { B[i] = 0; continue; }
            n = ((A[i - 1] + A[i + 1] + A[i - OB_G] + A[i + OB_G]) >> 1) - B[i];
            n -= n >> 5;
            B[i] = (int8_t)clamp(n, -127, 127);
        }
        ob_flip ^= 1;
    }
}
static UI_NOINLINE void orb_paint(void)
{
    const int8_t *A = (const int8_t *)wv_mem + (ob_flip ? OB_G * OB_G : 0);
    int32_t x, y, r0 = ob_fill / 16, lv = clamp(ev_lvl / 4, 0, 40), tw = WV_SIN[(wv_ph / 2u) & 63u] / 3 + (int32_t)(wv_ph * 2u % 256u) / 8;
    uint32_t q;
    for (y = OB_G / 2 - OB_R; y <= OB_G / 2 + OB_R; y++) for (x = OB_G / 2 - OB_R; x <= OB_G / 2 + OB_R; x++) {
        int32_t d2 = ob_d2(x, y), v, dx = x - OB_G / 2, dy = y - OB_G / 2, rr = 0, sx, sy, i;
        uint32_t an;
        uint16_t c;
        if (d2 > OB_R * OB_R || d2 < r0 * r0) continue;
        while ((rr + 1) * (rr + 1) <= d2) rr++;
        an = (uint32_t)(tw * (OB_R - rr) / OB_R);                      /* (the whirl: the middle turned further than the rim) */
        sx = OB_G / 2 + (dx * wv_sin4(an + 64u) - dy * wv_sin4(an)) / 127, sy = OB_G / 2 + (dx * wv_sin4(an) + dy * wv_sin4(an + 64u)) / 127;
        sx = clamp(sx, 1, OB_G - 2), sy = clamp(sy, 1, OB_G - 2), i = sy * OB_G + sx;
        v = (A[i] * 2 + A[i - 1] + A[i + 1]) / 4 + (A[i] - A[i - OB_G - 1]) / 2;   /* (the height and the slope: the light catching the waves) */
        c = v < -8 ? RGB(0x06, 0x08, 0x34) : v < 12 ? RGB(0x14, 0x1c, 0x84) : v < 30 ? RGB(0x28, 0x40, 0xd0) : v < 50 ? RGB(0x70, 0x90, 0xff) : v < 72 ? RGB(0xff, 0xb0, 0x70) : WC_WHT;
        if (r0 && d2 < (r0 + 1) * (r0 + 1)) c = (A[i] & 4) ? RGB(0xff, 0xb0, 0x70) : RGB(0x90, 0xb0, 0xff);   /* (the front of the water, lit) */
        WFR(x * 4, y * 4, 4, 4, c);
    }
    for (q = 0; q < 160u; q++) {                                       /* the rim */
        uint32_t an = q * 256u / 160u;
        WFR(120 + 89 * wv_sin4(an + 64u) / 127, 120 + 89 * wv_sin4(an) / 127, 2, 2, RGB(0x40 + lv * 2, 0x60 + lv * 2, 0xff));
    }
    for (x = 0; x < 240; x += 2) {                                     /* the string, shaking with the sound */
        int32_t sv = scope_buf[(scope_w - 240u + (uint32_t)x) & (SCOPE_N - 1u)], y1 = 120 + sv * (6 + lv / 4) / 12000;
        WFR(x, y1 - 1, 2, 1, RGB(0x30, 0x40, 0xa0)), WFR(x, y1, 2, 1, RGB(0xa0, 0xb8, 0xff)), WFR(x, y1 + 1, 2, 1, RGB(0x30, 0x40, 0xa0));
    }
}
/* DOOMSCROLL's phone: a feed of posts in a handful of pixels, scrolling for ever (each post fixed by its number: a sunset, mountains, a
 * selfie, a quote, a cat, the city at night, a coffee; every third an AD), a double tap on a key likes the one in the middle; or the player */
static int32_t dm_feed, dm_v, dm_hand = 100, dm_loop, dm_heart, dm_hpost = -1, dm_cpost;
static int32_t dm_liked[6] = {-1, -1, -1, -1, -1, -1};
static uint32_t dm_tap[27];
static UI_NOINLINE void dm_photo(int32_t x, int32_t y, uint32_t h)
{
    static const uint16_t SK[4] = {RGB(0xff, 0x90, 0x60), RGB(0x70, 0xb0, 0xf0), RGB(0xe0, 0x80, 0xc0), RGB(0x60, 0xd0, 0xc0)};
    uint16_t a = SK[h % 4u], d = RGB(0x24, 0x20, 0x38);
    int32_t i, kind = (int32_t)((h >> 3) % 7u);
    switch (kind) {
    case 0: WFR(x, y, 80, 12, RGB(0xff, 0xc0, 0x60)), WFR(x, y + 12, 80, 10, a), wvf_disk(x + 40, y + 22, 8, RGB(0xff, 0xf0, 0xa0)), WFR(x, y + 22, 80, 12, RGB(0x30, 0x40, 0x80));
        for (i = 0; i < 4; i++) WFR(x + 32 + (i & 1) * 4, y + 24 + i * 3, 16 - i * 3, 1, RGB(0xff, 0xe0, 0x90)); break;   /* a sunset on the sea */
    case 1: WFR(x, y, 80, 34, a);
        for (i = 0; i < 24; i++) WFR(x + 26 - i, y + 10 + i, i * 2 + 1, 1, i < 6 ? WC_WHT : d), WFR(x + 58 - i / 2, y + 16 + i * 3 / 4, i + 1, 1, RGB(0x40, 0x50, 0x70)); break;   /* mountains */
    case 2: WFR(x, y, 80, 34, a), wvf_disk(x + 40, y + 14, 8, RGB(0xf0, 0xc0, 0xa0)), WFR(x + 32, y + 5, 16, 5, d), WFR(x + 28, y + 24, 24, 10, d),
        WFR(x + 37, y + 13, 2, 2, d), WFR(x + 42, y + 13, 2, 2, d), WFR(x + 38, y + 18, 5, 1, RGB(0xc0, 0x50, 0x50)); break;   /* someone, a selfie */
    case 3: WFR(x, y, 80, 34, d), WFR(x + 6, y + 4, 3, 4, a), WFR(x + 11, y + 4, 3, 4, a);
        for (i = 0; i < 3; i++) WFR(x + 14 + (i == 2 ? 10 : 0), y + 12 + i * 6, i == 2 ? 32 : 52, 2, WC_WHT); break;   /* a quote */
    case 4: WFR(x, y, 80, 34, RGB(0xf0, 0xd8, 0xb0)), wvf_disk(x + 40, y + 20, 10, d);
        for (i = 0; i < 6; i++) WFR(x + 31 + i / 2, y + 6 + i, 3, 1, d), WFR(x + 46 - i / 2, y + 6 + i, 3, 1, d);
        WFR(x + 35, y + 18, 3, 3, RGB(0xc0, 0xf0, 0x60)), WFR(x + 43, y + 18, 3, 3, RGB(0xc0, 0xf0, 0x60)); break;   /* a cat */
    case 5: WFR(x, y, 80, 34, RGB(0x10, 0x10, 0x28));
        for (i = 0; i < 8; i++) { int32_t bh = 10 + (int32_t)(pc_h(h + (uint32_t)i) % 20u), k; WFR(x + i * 10, y + 34 - bh, 9, bh, RGB(0x30, 0x30, 0x58)); for (k = 0; k < bh - 3; k += 4) WFR(x + i * 10 + 2 + (k & 4), y + 36 - bh + k, 2, 2, (pc_h(h + (uint32_t)(i * 9 + k)) & 1u) ? RGB(0xff, 0xd0, 0x60) : RGB(0x30, 0x30, 0x58)); }
        break;                                                         /* the city at night */
    default: WFR(x, y, 80, 34, RGB(0xc8, 0x90, 0x60)), wvf_disk(x + 40, y + 18, 11, WC_WHT), wvf_disk(x + 40, y + 18, 8, RGB(0x70, 0x40, 0x20)),
        wvf_disk(x + 40, y + 18, 3, RGB(0xf0, 0xd0, 0xa0)), WFR(x + 51, y + 15, 5, 2, WC_WHT); break;   /* a coffee */
    }
}
static UI_NOINLINE void dm_feedscr(int32_t fx, int32_t fy, int32_t fw, int32_t fh)
{
    static const char HT[6][8] = {".XX.XX.", "XXXXXXX", "XXXXXXX", ".XXXXX.", "..XXX..", "...X..."};
    int32_t q, PH = 58;
    WFR(fx, fy, fw, fh, RGB(0xf4, 0xf4, 0xf6));
    for (q = -1; q < 3; q++) {
        int32_t pidx = dm_feed / 3 / PH + q, y0 = fy + 10 + q * PH - (dm_feed / 3) % PH, k, liked;
        uint32_t h = (pc_h((uint32_t)pidx * 2654435761u) & ~(7u << 3)) | (uint32_t)((((pidx * 3) % 7) + 7) % 7) << 3;   /* (the pictures take turns) */
        if (y0 <= fy + fh / 2 && fy + fh / 2 < y0 + PH) dm_cpost = pidx;   /* (the one in the middle: a double tap likes it) */
        for (k = 0, liked = 0; k < 6; k++) if (dm_liked[k] == pidx) liked = 1;
        wvf_disk(fx + 6, y0 + 4, 3, (h & 2u) ? RGB(0xe0, 0x50, 0x90) : RGB(0x60, 0x90, 0xf0)), WFR(fx + 12, y0 + 3, 18 + (int32_t)(h % 14u), 2, RGB(0x40, 0x40, 0x48));
        WFR(fx + 74, y0 + 4, 1, 1, RGB(0x40, 0x40, 0x48)), WFR(fx + 76, y0 + 4, 1, 1, RGB(0x40, 0x40, 0x48)), WFR(fx + 78, y0 + 4, 1, 1, RGB(0x40, 0x40, 0x48));
        dm_photo(fx + 2, y0 + 9, h);
        if (((pidx % 3) + 3) % 3 == 2) {                               /* every third post: an AD, a white box in the picture's corner */
            WFR(fx + 2 + 80 - 4 - 15, y0 + 9 + 34 - 4 - 10, 15, 10, WC_WHT);
            wf_text(fx + 2 + 80 - 4 - 13, y0 + 9 + 34 - 4 - 9, 0, "AD", RGB(0x20, 0x20, 0x28));
        }
        if (pidx == dm_hpost && dm_heart) {                            /* the double tap: a red heart pops over the picture */
            int32_t z = dm_heart > 10 ? 2 + (14 - dm_heart) / 2 : 4, r, c, hx = fx + 42 - z * 7 / 2, hy = y0 + 26 - z * 3;
            for (r = 0; r < 6; r++) for (c = 0; c < 7; c++) if (HT[r][c] == 'X') WFR(hx + c * z, hy + r * z, z, z, (dm_heart & 2) && dm_heart < 6 ? RGB(0xff, 0x90, 0xa0) : RGB(0xf0, 0x30, 0x50));
        }
        {                                                              /* the heart, the bubble, the arrow */
            uint16_t hc = liked ? RGB(0xf0, 0x30, 0x50) : RGB(0x40, 0x40, 0x48);
            int32_t hx = fx + 4, hy = y0 + 46;
            WFR(hx, hy, 2, 2, hc), WFR(hx + 3, hy, 2, 2, hc), WFR(hx, hy + 1, 5, 2, hc), WFR(hx + 1, hy + 3, 3, 1, hc), WFR(hx + 2, hy + 4, 1, 1, hc);
            ne_ring(fx + 13, hy + 2, 2, RGB(0x40, 0x40, 0x48)), wvf_pline(fx + 19, hy + 4, fx + 23, hy, RGB(0x40, 0x40, 0x48)), wvf_pline(fx + 19, hy, fx + 23, hy, RGB(0x40, 0x40, 0x48));
            WFR(fx + 76, hy, 4, 5, RGB(0x40, 0x40, 0x48)), WFR(fx + 77, hy + 1, 2, 3, RGB(0xf4, 0xf4, 0xf6));
        }
        WFR(fx + 4, y0 + 53, 20 + (int32_t)((h >> 5) % 20u), 2, RGB(0x60, 0x60, 0x68));
    }
    WFR(fx, fy, fw, 9, WC_WHT), WFR(fx, fy + 9, fw, 1, RGB(0xd0, 0xd0, 0xd8));   /* the bar on top */
    wf_text(fx + 3, fy + 1, 0, "feed", RGB(0x20, 0x20, 0x28));
    WFR(fx + 66, fy + 3, 2, 2, RGB(0xf0, 0x30, 0x50)), WFR(fx + 69, fy + 3, 2, 2, RGB(0xf0, 0x30, 0x50)), WFR(fx + 66, fy + 4, 5, 2, RGB(0xf0, 0x30, 0x50)), WFR(fx + 67, fy + 6, 3, 1, RGB(0xf0, 0x30, 0x50));
    wvf_pline(fx + 74, fy + 6, fx + 80, fy + 2, RGB(0x20, 0x20, 0x28)), wvf_pline(fx + 74, fy + 2, fx + 80, fy + 2, RGB(0x20, 0x20, 0x28));
}
static UI_NOINLINE void dm_player(int32_t fx, int32_t fy, int32_t fw, int32_t fh)
{
    char b[16];
    int32_t i;
    uint32_t sec = wf_secs();
    WFR(fx, fy, fw, fh, RGB(0x20, 0x24, 0x30));
    for (i = 0; i < 4; i++) WFR(fx + 2, fy + 2 + i * 2, fw - 4, 1, RGB(0xc0, 0xa0, 0x30));   /* the title bar, its stripes */
    WFR(fx + 22, fy + 1, 40, 9, RGB(0x20, 0x24, 0x30)), wf_textc(fx + fw / 2, fy + 2, 0, "WINAMPFM", RGB(0xf0, 0xe0, 0x90));
    WFR(fx + 4, fy + 14, fw - 8, 40, C_BLACK), WFR(fx + 4, fy + 14, fw - 8, 1, RGB(0x60, 0x68, 0x80));
    fmt_int(b, (int32_t)(sec / 60u)), str_cpy(b + str_len(b), ":", 16 - str_len(b));
    if (sec % 60u < 10u) str_cpy(b + str_len(b), "0", 16 - str_len(b));
    fmt_int(b + str_len(b), (int32_t)(sec % 60u)), wf_text(fx + 8, fy + 17, 1, b, RGB(0x40, 0xf0, 0x60));
    wf_text(fx + 8, fy + 30, 0, song.playing ? ">" : "II", RGB(0x40, 0xf0, 0x60));
    for (i = 0; i < 12; i++) {                                         /* the little bars: the music's; nothing playing, a song of their own */
        int32_t real = song.playing || ev_lvl > 10, hh = clamp(real ? (int32_t)sp_s[(uint32_t)i * 19u / 12u] : 8 + (WV_SIN[(wv_ph * (uint32_t)(3 + i % 4) + (uint32_t)i * 11u) & 63u] + WV_SIN[(wv_ph * 5u + (uint32_t)i * 23u) & 63u]) / 32, 0, 18);
        WFR(fx + 44 + i * 3, fy + 50 - hh, 2, hh, hh > 13 ? RGB(0xf0, 0x40, 0x30) : hh > 8 ? RGB(0xf0, 0xd0, 0x40) : RGB(0x40, 0xf0, 0x60));
    }
    if (song.sel < NPART) trk_short_name(song.sel, b); else str_cpy(b, "DRUMS", 16);
    wf_clx0 = fx + 5, wf_clx1 = fx + fw - 5;
    wf_text(fx + fw - (int32_t)((wv_ph * 1u) % (uint32_t)(wf_tw(0, b) + fw)), fy + 58, 0, b, RGB(0x40, 0xf0, 0x60));
    wf_clx0 = 0, wf_clx1 = 240;
    WFR(fx + 4, fy + 70, fw - 8, 3, RGB(0x40, 0x44, 0x58)), WFR(fx + 4 + (int32_t)(sec % 60u) * (fw - 14) / 60, fy + 69, 6, 5, RGB(0xc0, 0xa0, 0x30));   /* the seek bar */
    for (i = 0; i < 5; i++) {                                          /* prev, play, pause, stop, next */
        uint16_t c = (i == 1 && song.playing) || (i == 2 && !song.playing) ? RGB(0x40, 0xf0, 0x60) : RGB(0xa0, 0xa8, 0xb8);
        WFR(fx + 5 + i * 15, fy + 80, 13, 10, RGB(0x50, 0x54, 0x68)), WFR(fx + 9 + i * 15, fy + 83, 5, 4, c);
    }
    for (i = 0; i < 10; i++) WFR(fx + 5 + i * 7, fy + 100, 5, 4, i < clamp(ev_lvl / 20, 0, 10) ? RGB(0x40, 0xf0, 0x60) : RGB(0x30, 0x34, 0x44));   /* the volume, the level */
}
static UI_NOINLINE void v2_doom(void)
{
    int32_t i, y, k;
    for (y = 0; y < 180; y += 3) { int32_t t = y * 255 / 180; WFR(0, y, 240, 3, RGB(0x70 + (0xf0 - 0x70) * t / 255, 0x88 + (0xb0 - 0x88) * t / 255, 0xe8 - (0xe8 - 0xd0) * t / 255)); }
    for (i = 0; i < 22; i++) WFR((int32_t)(pc_h((uint32_t)i * 37u) % 240u), (int32_t)(pc_h((uint32_t)i * 23u) % 90u), 2, 2, (nk_mask >> (i % 27)) & 1u ? WC_WHT : RGB(0xc8, 0xc8, 0xff));
    wvf_disk(212, 44, 10, RGB(0xff, 0xf4, 0xc0)), wvf_disk(217, 40, 9, RGB(0x86, 0x9c, 0xe8));
    dm_cloud(40 + (int32_t)((wv_ph / 3u) % 300u) - 60, 132, 10), dm_cloud(190 - (int32_t)((wv_ph / 4u) % 300u) + 60, 120, 8);
    for (k = 0; k < 27; k++) if ((nk_on >> k) & 1u) {                 /* a note: a bird at its height, and maybe a cloud from its side */
        for (i = 0; i < 10; i++) if (!dm_b[i][2]) { dm_b[i][2] = (int16_t)(k < 13 ? 1 : -1); dm_b[i][0] = (int16_t)(k < 13 ? -10 : 250); dm_b[i][1] = (int16_t)(150 - k * 4); break; }
        dm_col = (uint8_t)((dm_col + 1u) % 6u);
        if ((pc_h(wv_ph + (uint32_t)k) & 3u) == 0u) for (i = 0; i < 8; i++) if (!dm_c[i][2]) { dm_c[i][0] = (int16_t)(k < 13 ? -40 : 280); dm_c[i][1] = (int16_t)(40 + (int32_t)(pc_h((uint32_t)k + wv_ph) % 80u)); dm_c[i][2] = (int16_t)(k < 13 ? 1 : -1); break; }
    }
    for (i = 0; i < 8; i++) if (dm_c[i][2]) {                          /* the clouds the notes sent drift across and out */
        dm_c[i][0] = (int16_t)(dm_c[i][0] + dm_c[i][2]);
        if (dm_c[i][0] < -60 || dm_c[i][0] > 300) { dm_c[i][2] = 0; continue; }
        dm_cloud(dm_c[i][0], dm_c[i][1], 6);
    }
    for (i = 0; i < 10; i++) if (dm_b[i][2]) {                         /* the birds: two strokes, flapping */
        int32_t x = dm_b[i][0], yy = dm_b[i][1], f = (int32_t)((wv_ph / 2u + (uint32_t)i) & 1u);
        dm_b[i][0] = (int16_t)(x + dm_b[i][2] * 4);
        if (x < -20 || x > 260) { dm_b[i][2] = 0; continue; }
        wvf_pline(x - 4, yy - 2 + f * 4, x, yy, RGB(0x30, 0x28, 0x50)), wvf_pline(x, yy, x + 4, yy - 2 + f * 4, RGB(0x30, 0x28, 0x50));
    }
    if (wvf_hit_n) { int32_t sx = 200 - wvf_hit_n * 12; wvf_pline(sx, 30 + (5 - wvf_hit_n) * 5, sx + 10, 24 + (5 - wvf_hit_n) * 5, WC_WHT); }
    dm_word(32, 8);
    WFR(0, 180, 240, 60, RGB(0x2c, 0x2a, 0x48));                       /* the bridge, its rail */
    WFR(0, 166, 240, 2, RGB(0x58, 0x58, 0x80));
    for (i = 4; i < 240; i += 22) WFR(i, 166, 2, 14, RGB(0x58, 0x58, 0x80));
    {                                                                  /* someone looking at the phone, a cat beside */
        int32_t px = 168, py = 116, k, scroll, swipe;
        uint16_t k2 = RGB(0x1c, 0x1a, 0x30), sc;
        static int32_t was;
        /* when it scrolls: while the hands play (and a moment after), and for two seconds after the loop starts; else the player */
        if (nk_hand || nk_hon) dm_hand = 0; else if (dm_hand < 1000) dm_hand++;
        if (song.playing && !was) dm_loop = 40;
        was = song.playing;
        if (dm_loop) dm_loop--;
        scroll = dm_hand < 16 || dm_loop > 0;
        if (nk_hon) dm_v = 9;
        if (scroll) dm_feed += 3 + dm_v; else dm_v = 0;
        if (dm_v > 0) dm_v--;
        for (k = 0; k < 27; k++) if ((nk_hon >> k) & 1u) {             /* two quick taps on one key: the post in the middle is liked */
            if (dm_tap[k] && fm1_ms - dm_tap[k] < 320u && fm1_ms - dm_tap[k] > 60u) {
                dm_heart = 14, dm_hpost = dm_cpost, dm_tap[k] = 0;
                for (swipe = 5; swipe > 0; swipe--) dm_liked[swipe] = dm_liked[swipe - 1];
                dm_liked[0] = dm_cpost;
            } else dm_tap[k] = fm1_ms;
        }
        if (dm_heart) dm_heart--;
        sc = scroll ? RGB(0xf0, 0xf0, 0xf4) : RGB(0x40, 0xe0, 0x60);
        { static const int8_t PS[8] = {-4, 18, 2, 20, -4, 27, 4, 27}; fg_man(px, py + 58, 8, 1, PS, k2); }   /* (the upper arms down: elbows bent) */
        swipe = scroll ? (int32_t)((uint32_t)(dm_feed / 6) % 10u) : 4;  /* the other thumb, flicking up */
        ne_line(164, 144, 151, 129, k2);                               /* the forearm up to the phone, close to the face */
        {                                                              /* the phone in the hand, small, over the arm; beside, its screen big, lines between them as a zoom */
            int32_t fx = 22, fy = 40, fw = 84, fh = 112, sx = 146, sy = 121, o0 = cv_cx0, o1 = cv_cy0, o2 = cv_cx1, o3 = cv_cy1, q;
            WFR(sx - 1, sy - 1, 8, 12, k2);
            if (scroll) for (q = 0; q < 4; q++) {                      /* its screen: only the colours going up */
                static const uint16_t PC[4] = {RGB(0xff, 0x90, 0x60), RGB(0x70, 0xb0, 0xf0), RGB(0xe0, 0x80, 0xc0), RGB(0x60, 0xd0, 0xc0)};
                int32_t yy = sy + q * 4 - (dm_feed / 3) % 4;
                WFR(sx, yy < sy ? sy : yy, 6, yy < sy ? 4 - (sy - yy) : (yy + 4 > sy + 10 ? sy + 10 - yy : 4), (q + dm_feed / 12) & 1 ? RGB(0xf4, 0xf4, 0xf6) : PC[(uint32_t)(q + dm_feed / 12) % 4u]);
            } else { WFR(sx, sy, 6, 10, RGB(0x20, 0x24, 0x30)); for (q = 0; q < 3; q++) WFR(sx + 1 + q * 2, sy + 9 - (int32_t)sp_s[(uint32_t)q * 6u] / 4 - 1, 1, 1 + (int32_t)sp_s[(uint32_t)q * 6u] / 4, RGB(0x40, 0xe0, 0x60)); }
            ne_line(170, 146, 152, 134 - swipe, k2);                   /* the other hand on it */
            wvf_pline(sx, sy - 1, fx + fw, fy, WC_WHT), wvf_pline(sx, sy + 10, fx + fw, fy + fh, WC_WHT);
            WFR(fx - 3, fy - 3, fw + 6, fh + 6, k2);
            cv_cx0 = fx, cv_cy0 = fy, cv_cx1 = fx + fw, cv_cy1 = fy + fh;
            if (scroll) dm_feedscr(fx, fy, fw, fh); else dm_player(fx, fy, fw, fh);
            cv_cx0 = o0, cv_cy0 = o1, cv_cx1 = o2, cv_cy1 = o3;
        }
        wvf_disk(px - 4, py, 3, sc);                                   /* its light on the face */
        WFR(px + 18, py + 52, 12, 8, k2), WFR(px + 18, py + 48, 4, 4, k2), WFR(px + 26, py + 48, 2, 3, k2), WFR(px + 29, py + 56, 6, 2, k2);
    }
}
/* DANCE: little people written in lines, as text and symbols; each takes a new pose on the beat when its band sounds; a clap (the drums'
 * CLAP, from the keys or the loop): all of them clap once with it */
static const char *const DN_POSE[13][3] = {{"\\O ", " I\\", "< \\"}, {" O)", "(I ", " ))"}, {" O ", "/I\\", "< >"}, {" O/", "<X ", "/ \\"}, {"  O", "///", "// "},
    {"-O-", " / ", " 77"}, {" O/", "/I ", "/ >"}, {"(O ", " I)", "(( "}, {" O ", " V ", " X "}, {"\\O ", " X>", "/ \\"}, {"\\O/", " I ", "/ \\"},
    {" O ", ">*<", "/ \\"}, {" O ", "< >", "/ \\"}};
static uint8_t dn_pose[12], dn_last[12];
/* DANCE: twelve little people, each one a band of the spectrum: a new pose when its band hits, a jump as high as it sounds, brighter
 * with it; on the kick all of them throw their arms up; a light under each on the floor */
static UI_NOINLINE void ne_dance(void)
{
    static const uint16_t DC[6] = {RGB(0xf0, 0xa0, 0x80), RGB(0xf8, 0xb0, 0x50), RGB(0xd8, 0xb8, 0x90), RGB(0xb8, 0x80, 0x40), RGB(0xe0, 0x98, 0x60), RGB(0xf0, 0x90, 0x78)};
    static uint32_t lastbeat;
    static int32_t clap;
    uint32_t beat = wv_ph * (uint32_t)clamp(song.g[G_BPM], 40, 240) / 600u, i, r;
    int up = wvf_hit_n > 2;
    if (dr_hits & (1u << 3)) clap = 6;                                 /* (lane 3: the CLAP) */
    else if (clap) clap--;
    for (i = 0; i < 12u; i++) {
        uint32_t lv = wf_spec[i * 19u / 12u];
        if ((lv > 8u && dn_last[i] <= 8u) || (beat != lastbeat && lv > 5u)) dn_pose[i] = (uint8_t)((dn_pose[i] + 1u + i % 3u) % 10u);
        dn_last[i] = (uint8_t)lv;
    }
    lastbeat = beat;
    for (i = 0; i < 12u; i++) {
        uint32_t lv = wf_spec[i * 19u / 12u];
        int32_t x = 12 + (int32_t)(i % 4u) * 58, y = 14 + (int32_t)(i / 4u) * 74 - (int32_t)lv / 2;
        uint16_t c = DC[(i * 7u) % 6u];
        if (lv > 12u) c = RGB(0xff, 0xf0, 0xd0);
        for (r = 0; r < 3u; r++) {
            const char *p2 = DN_POSE[clap > 2 ? 11 : clap ? 12 : up ? 10 : dn_pose[i]][r];
            int32_t c2;
            for (c2 = 0; p2[c2]; c2++) dn_char(x + c2 * 14, y + (int32_t)r * 19, 5, p2[c2], c);
        }
        WFR(x + 6, 14 + (int32_t)(i / 4u) * 74 + 60, 30, 2, RGB(0x30, 0x20, 0x18)), WFR(x + 6, 14 + (int32_t)(i / 4u) * 74 + 60, (int32_t)lv * 30 / 24, 2, c);
    }
}
/* DUEL: two little sword fighters of our own on a grey stage with a mirror floor (after the pixel duel reference): on guard to the
 * tempo; on each kick one dashes through the other with a white slash, sparks where it lands, dust at its feet */
static int32_t du_x[2] = {64, 176}, du_att = -1, du_t, du_who, du_clash, du_var;
static UI_NOINLINE void du_man(int32_t x, int32_t fy, int32_t dir, int32_t pose, uint16_t hair, int32_t refl)   /* the shared figure, 72 px tall */
{
    static const int8_t DP[4][8] = {{-6, 18, 12, 6, -8, 27, 8, 27}, {-12, 4, 20, 2, -12, 24, 16, 24}, {-10, 10, 22, 0, -10, 26, 10, 26}, {-10, -4, 6, -10, -6, 27, 6, 27}};
    static const int8_t SW[4][2] = {{22, -40}, {32, -8}, {44, -4}, {10, -30}};   /* the blade, from the front hand */
    int32_t s = refl ? -9 : 9, hx = x + dir * DP[pose][2] * 9 / 8, hy = fy - 48 * s / 8 + DP[pose][3] * s / 8;
    uint16_t body = refl ? RGB(0x60, 0x60, 0x68) : RGB(0x14, 0x18, 0x24), sw = refl ? RGB(0x90, 0x90, 0x98) : WC_WHT;
    fg_man(x, refl ? fy + 1 : fy, s, dir, DP[pose], body);
    if (!refl) WFR(x - 7, fy - 62, 14, 3, hair);                       /* (a headband: who is who) */
    ne_line3(hx, hy, hx + dir * SW[pose][0], hy + SW[pose][1] * s / 9, sw);
}
static UI_NOINLINE void ne_duel(void)
{
    int32_t i, k, fy = 170, bob = (int32_t)((wv_ph * (uint32_t)clamp(song.g[G_BPM], 40, 240) / 600u) & 1u);
    WFR(0, 0, 240, fy, RGB(0x7c, 0x7c, 0x80)), WFR(0, fy, 240, 90, RGB(0x4c, 0x4c, 0x50));
    for (i = 0; i < 5; i++) WFR(30 + i * 48, 0, 1, fy, RGB(0x68, 0x68, 0x6c));
    WFR(0, fy, 240, 1, RGB(0x40, 0xa0, 0x90));
    {   /* who strikes: the left half of the keys moves the one on the left, the right half the one on the right; both together: they clash */
        static int32_t lt = 99, rt = 99;
        int32_t lf = du_x[0] < du_x[1] ? 0 : 1;
        lt = nk_low(nk_on) ? 0 : lt + 1, rt = nk_high(nk_on) ? 0 : rt + 1;
        if (!du_clash && du_att < 0) {
            if (lt < 4 && rt < 4) du_clash = 1, lt = rt = 99;
            else if (lt == 0) du_att = lf, du_t = 0, du_var = (int32_t)(pc_h(wv_ph) % 4u);
            else if (rt == 0) du_att = lf ^ 1, du_t = 0, du_var = (int32_t)(pc_h(wv_ph * 3u) % 4u);
            else if (wvf_hit_n == 5) { du_att = du_who; du_who ^= 1; du_t = 0; }
        }
    }
    if (du_clash) {                                                    /* the clash: both rush in, the blades cross, sparks, both thrown back */
        int32_t lf = du_x[0] < du_x[1] ? 0 : 1, c = du_clash++;
        if (c < 4) du_x[lf] += (96 - du_x[lf]) / 2, du_x[lf ^ 1] += (144 - du_x[lf ^ 1]) / 2;
        else if (c < 10) {
            int32_t cy = fy - 60, r = 6 + (c - 4) * 6;
            for (k = 0; k < 10; k++) { uint32_t an = (uint32_t)k * 26u + (uint32_t)c * 7u; ne_line(120, cy, 120 + r * wv_sin4(an + 64u) / 127, cy + r * wv_sin4(an) / 127, k & 1 ? RGB(0xff, 0xe0, 0x80) : WC_WHT); }
            ne_ring(120, cy, r / 2 + 4, WC_WHT);
        } else du_x[lf] += (64 - du_x[lf]) / 2, du_x[lf ^ 1] += (176 - du_x[lf ^ 1]) / 2;
        if (c > 14) du_clash = 0, du_x[lf] = 64, du_x[lf ^ 1] = 176;
    }
    if (du_att >= 0) {                                                 /* the dash: through the other, a slash where they cross */
        int32_t a = du_att, o = a ^ 1, dir = du_x[o] > du_x[a] ? 1 : -1;
        du_t++;
        if (du_t < 4) du_x[a] += dir * (du_var == 1 ? 26 : du_var == 3 ? 40 : 34);
        if (du_t == 3) du_x[o] += dir * 14;
        if (du_t >= 2 && du_t < 7) {
            int32_t cx = (du_x[a] + du_x[o]) / 2, cy = fy - (du_var == 2 ? 14 : du_var == 3 ? 70 : 44), r = 24 + du_t * 5;
            uint32_t base = du_var == 1 ? 140u : du_var == 2 ? (dir > 0 ? 200u : 40u) : (uint32_t)(dir > 0 ? 180 : 0);   /* (the cut's arc: across, down from above, low and flat, or from the air) */
            for (k = 0; k < 12; k++) { uint32_t an = base + (uint32_t)k * 6u; ne_line(cx + r * wv_sin4(an + 64u) / 127, cy + (du_var == 1 ? r : r / 2) * wv_sin4(an) / 127, cx + r * wv_sin4(an + 70u) / 127, cy + (du_var == 1 ? r : r / 2) * wv_sin4(an + 6u) / 127, du_var == 3 ? RGB(0xff, 0xe0, 0x90) : WC_WHT); }
            for (k = 0; k < 6; k++) { uint32_t an = (uint32_t)k * 43u + du_t * 9u; wvf_pline(cx, cy, cx + (8 + du_t * 3) * wv_sin4(an + 64u) / 127, cy + (8 + du_t * 3) * wv_sin4(an) / 127, WC_WHT); }
            for (k = 0; k < 4; k++) wvf_disk(du_x[a] - dir * (6 + k * 7), fy - 2 - k, 3 - k / 2, RGB(0xe8, 0xe8, 0xe8));
        }
        if (du_t > 10) du_att = -1;
        du_x[0] = clamp(du_x[0], 30, 210), du_x[1] = clamp(du_x[1], 30, 210);
        if (du_t > 10 && (du_x[1] - du_x[0] < 60 && du_x[0] - du_x[1] < 60)) { du_x[0] = du_x[0] < du_x[1] ? 64 : 176; du_x[1] = du_x[0] == 64 ? 176 : 64; }
    }
    for (k = 0; k < 2; k++) {
        int32_t dir = du_x[k ^ 1] > du_x[k] ? 1 : -1, pose = du_clash ? (du_clash < 10 ? 1 : 3) : du_att == k ? (du_t < 4 ? 1 : 2) : du_att == (k ^ 1) && du_t >= 3 && du_t < 7 ? 3 : 0;
        int32_t lift = du_att == k && du_var == 3 && du_t < 6 ? 30 - (du_t - 3) * (du_t - 3) * 3 : du_att == k && du_var == 2 ? -10 : 0;   /* (the leap; the crouch) */
        if (du_att == k && du_var == 1 && pose == 2) pose = 1;          /* (the overhead cut keeps the blade up) */
        du_man(du_x[k], fy - (pose == 0 ? bob : 0) - (lift > 0 ? lift : 0) + (lift < 0 ? 4 : 0), dir, pose, k ? RGB(0xe8, 0xe8, 0xf0) : RGB(0xf0, 0x50, 0x90), 0);
        du_man(du_x[k], fy, dir, pose, 0, 1);
    }
}
/* BLUEPRINT: a robot's schematic, green lines on a blue grid (after the reference): every part lights with its band of the spectrum,
 * cyan sights turn, a scan line runs down, red labels point at the parts with the device's numbers */
static const int16_t BP_HALF[] = {120, 20, 132, 24, 138, 40, 132, 56, 120, 60, -1, -1, 120, 62, 150, 66, 160, 80, 156, 110, 140, 124, 120, 128, -1, -1,
    164, 50, 192, 52, 198, 62, 196, 86, 186, 92, 166, 90, 160, 80, 160, 58, 164, 50, -1, -1, 186, 92, 198, 130, 206, 160, 196, 168, 184, 150, 172, 110,
    -1, -1, 128, 128, 140, 140, 136, 156, 120, 160, -1, -1, 140, 150, 170, 170, 190, 220, 176, 238, 150, 238, 142, 200, 130, 170, 140, 150, -1, -1,
    120, 40, 130, 38, 132, 46, 120, 48, -1, -1, 126, 80, 146, 80, 146, 96, 126, 96, 126, 80};
static const uint8_t BP_BAND[9] = {17, 2, 6, 10, 0, 13, 18, 3, 1};   /* which band lights each part */
static UI_NOINLINE void bp_label(int32_t x, int32_t y, int32_t tx, int32_t ty, const char *t, int32_t v)
{
    char b[16];
    wvf_pline(x, y, tx, ty, RGB(0xff, 0xb0, 0x60));
    WFR(tx, ty - 9, 44, 1, RGB(0xff, 0xb0, 0x60));
    str_cpy(b, t, 16);
    if (v >= 0) fmt_int(b + str_len(b), v);
    wf_text(tx, ty - 17, 0, b, RGB(0xff, 0x30, 0x30));
    WFR(tx, ty - 6, 20, 1, WC_LAB), WFR(tx, ty - 3, 30, 1, WC_LAB);
}
static UI_NOINLINE void ne_blueprint(void)
{
    int32_t i, part = 0, k;
    uint32_t a = wv_ph * 3u;
    WFR(0, 0, 240, 240, RGB(0x04, 0x08, 0x10));
    for (i = 0; i < 12; i++) WFR(i * 20, 0, 1, 240, RGB(0x18, 0x34, 0x48)), WFR(0, i * 20, 240, 1, RGB(0x18, 0x34, 0x48));
    WFR(0, (int32_t)((wv_ph * 4u) % 240u), 240, 1, RGB(0x30, 0x90, 0xb0));   /* the scan */
    for (i = 0; i + 3 < (int32_t)(sizeof BP_HALF / sizeof BP_HALF[0]); i += 2) {
        uint16_t c;
        if (BP_HALF[i] < 0) { part++; continue; }
        if (BP_HALF[i + 2] < 0) continue;
        c = wf_spec[BP_BAND[part % 9]] > 9 ? RGB(0xc8, 0xff, 0x90) : RGB(0x40, 0xe0, 0x20);
        ne_line(BP_HALF[i], BP_HALF[i + 1], BP_HALF[i + 2], BP_HALF[i + 3], c);
        ne_line(240 - BP_HALF[i], BP_HALF[i + 1], 240 - BP_HALF[i + 2], BP_HALF[i + 3], c);
    }
    for (k = 0; k < 2; k++) { int32_t cx = k ? 80 : 160, cy = 196; ne_ring(cx, cy, 8, RGB(0x40, 0xe0, 0x20)); wvf_pline(cx - 5, cy + 5, cx + 5, cy - 5, RGB(0x40, 0xe0, 0x20)); }
    {                                                                  /* the sights */
        int32_t sx = 56, sy = 72;
        ne_ring(sx, sy, 22, RGB(0x30, 0xc0, 0xf0)), ne_ring(sx, sy, 12 + wv_bass() / 4, RGB(0x30, 0xc0, 0xf0));
        wvf_pline(sx - 30, sy, sx + 30, sy, RGB(0x30, 0xc0, 0xf0)), wvf_pline(sx, sy - 30, sx, sy + 30, RGB(0x30, 0xc0, 0xf0));
        for (k = 0; k < 24; k++) { uint32_t an = a + (uint32_t)k * 4u; cv_pset(120 + 76 * wv_sin4(an + 64u) / 127, 120 + 76 * wv_sin4(an) / 127, RGB(0x30, 0xc0, 0xf0)); }
        wvf_pline(120, 120, 120 + 76 * wv_sin4(a + 64u) / 127, 120 + 76 * wv_sin4(a) / 127, RGB(0x30, 0xc0, 0xf0));
        wvf_pline(120, 70, 120, 100, RGB(0x30, 0xc0, 0xf0)), wvf_pline(114, 96, 126, 96, RGB(0x30, 0xc0, 0xf0));
    }
    bp_label(120, 30, 150, 14, "HEAD ", -1);
    bp_label(136, 88, 176, 120, "CORE ", song.g[G_BPM]);
    bp_label(60, 120, 8, 104, "ARM ", (int32_t)song.sel + 1);
    bp_label(100, 210, 30, 232, "PWR ", clamp(wv_all() / 3, 0, 99));
    if ((wv_ph / 8u) & 1u) WFR(6, 6, 10, 3, RGB(0x30, 0xc0, 0xf0));
    for (k = 0; k < 4; k++) WFR(6, 12 + k * 4, 6 + (int32_t)(((uint32_t)k * 7u + wv_ph / 4u) % 14u), 1, RGB(0x30, 0xc0, 0xf0));
}
/* ---- the neon diagrams (after the wireframe references) */
/* DATA: numbers on the left (they change with the bands), green dashes flying through the middle, an orange net on the right that the
 * spectrum ripples; three white slabs underneath */
static UI_NOINLINE void ne_data(void)
{
    int32_t i, j, x, y;
    for (i = 0; i < 48; i++) {
        char d[2] = {(char)('0' + (int32_t)((wv_ph / (2u + (uint32_t)(i % 3)) + (uint32_t)i * 7u + (uint32_t)wf_spec[i % 19]) % 10u)), 0};
        x = 8 + (i % 6) * 13 + (i / 6) * 4, y = 70 + (i / 6) * 17 + (i % 6) * 3;
        wf_text(x, y, 1, d, wf_spec[i % 19] > 10 ? NE_WHT : RGB(0x98, 0x98, 0xa8));
    }
    for (i = 0; i < 40; i++) {
        x = 80 + (int32_t)(((uint32_t)i * 37u + wv_ph * 2u) % 80u), y = 50 + (int32_t)(((uint32_t)i * 23u) % 140u) + (x - 80) / 4;
        WFR(x, y, 5, 1, NE_GRN), WFR(x + 2, y - 1, 3, 1, NE_GRN);
    }
    for (j = 0; j < 9; j++)                                            /* the net: 9 x 9 points, pushed by the bands */
        for (i = 0; i < 9; i++) {
            int32_t px = 140 + i * 10 + j * 2, py = 40 + j * 16 - i * 4 + (wf_spec[(i + j) % 19] - 8) / 2 + WV_SIN[((uint32_t)(i * 7 + j * 5) + wv_ph * 3u) & 63u] / 40;
            int32_t qx = 140 + (i + 1) * 10 + j * 2, qy = 40 + j * 16 - (i + 1) * 4 + (wf_spec[(i + 1 + j) % 19] - 8) / 2 + WV_SIN[((uint32_t)((i + 1) * 7 + j * 5) + wv_ph * 3u) & 63u] / 40;
            int32_t rx = 140 + i * 10 + (j + 1) * 2, ry = 40 + (j + 1) * 16 - i * 4 + (wf_spec[(i + j + 1) % 19] - 8) / 2 + WV_SIN[((uint32_t)(i * 7 + (j + 1) * 5) + wv_ph * 3u) & 63u] / 40;
            if (i < 8) wvf_pline(px, py, qx, qy, NE_ORG);
            if (j < 8) wvf_pline(px, py, rx, ry, NE_ORG);
        }
    for (i = 0; i < 3; i++) { x = 40 + i * 64, y = 214 - i * 26; ne_line(x, y, x + 40, y + 16, NE_WHT); ne_line(x, y, x + 6, y - 4, NE_WHT); ne_line(x + 6, y - 4, x + 46, y + 12, NE_WHT); ne_line(x + 40, y + 16, x + 46, y + 12, NE_WHT); }
}
/* LAB: a blue wire landscape (the spectrum's history) under an orange board: the wave, the left channel against the right (a Lissajous),
 * a spring that bounces with the bass, the four tracks as bars, dotted numbers */
static UI_NOINLINE void ne_lab(void)
{
    int32_t i, k, x, y, b = wv_bass();
    for (k = 0; k < 12; k++) {                                         /* the board */
        WFR(110 + k * 10, 20, 1, 120, RGB(0x80, 0x30, 0x10));
        WFR(110, 20 + k * 10, 110, 1, RGB(0x80, 0x30, 0x10));
    }
    {
        int32_t py = 60, s;
        for (x = 0; x < 110; x++) { s = scope_buf[(scope_w - 220u + 2u * (uint32_t)x) & (SCOPE_N - 1u)]; y = 60 - s * 30 / 12000; if (x) wvf_pline(109 + x, py, 110 + x, y, NE_ORG); py = y; }
    }
    for (i = 0; i < 128; i++) {                                        /* the Lissajous */
        int32_t l = scope_buf[(scope_w - 128u + (uint32_t)i) & (SCOPE_N - 1u)], r = scope_bufr[(scope_w - 128u + (uint32_t)i) & (SCOPE_N - 1u)];
        WFR(100 + l * 26 / 12000, 140 + r * 26 / 12000, 2, 2, NE_ORG);
    }
    for (k = 0; k < 6; k++) {                                          /* the spring */
        int32_t cy = 150 + k * (6 + b / 6);
        for (i = 0; i < 16; i++) cv_pset(164 + 9 * WV_SIN[((uint32_t)i * 4u + 16u) & 63u] / 127, cy + 3 * WV_SIN[(uint32_t)i * 4u & 63u] / 127, NE_ORG);
    }
    for (k = 0; k < 4; k++) { int32_t h = (int32_t)trk_level((uint32_t)k) * 34 / 127; ne_box(182 + k * 9, 132 - h, 6, h, 0, NE_ORG); }
    for (i = 0; i < 4; i++) for (k = 0; k < 9; k++) if (((uint32_t)k * 5u + (uint32_t)i * 3u + wv_ph / 6u) % 4u) WFR(76 + (k % 3) * 3 + (k / 3) * 1, 30 + i * 18 + (k / 3) * 3, 2, 2, NE_ORG);
    for (k = 0; k < 12; k++) {                                         /* the landscape: rows of the history, isometric */
        int32_t px = 0, py = 0;
        for (i = 0; i < 19; i++) {
            x = 10 + i * 9 + k * 5, y = 236 - k * 5 - i * 3 - wv_mem[(uint32_t)k * 19u + (uint32_t)i] * 2;
            if (i) wvf_pline(px, py, x, y, RGB(0x40, 0x70, 0xf0));
            if (k) wvf_pline(x, y, x - 5, y + 5 + (wv_mem[(uint32_t)k * 19u + (uint32_t)i] - wv_mem[(uint32_t)(k - 1) * 19u + (uint32_t)i]) * 2, RGB(0x30, 0x50, 0xc0));
            px = x, py = y;
        }
    }
}
/* TESLA: a red wire sphere over a magenta grid; green lightning runs from it down to the floor, more with the sound */
static UI_NOINLINE void ne_tesla(void)
{
    int32_t i, k, cx = 120, cy = 60, all = wv_all();
    WFR(0, 0, 240, 240, RGB(0x24, 0x14, 0x20));
    for (i = 0; i <= 10; i++) {                                        /* the floor */
        wvf_pline(30 + i * 16, 120 + i * 4, 70 + i * 16, 210 + i * 4 - 40, RGB(0xc0, 0x40, 0xd0));
        wvf_pline(30 + i * 4, 120 + i * 9, 190 + i * 4, 160 + i * 9 - 40, RGB(0xc0, 0x40, 0xd0));
    }
    ne_line(20, 136, 20, 204, NE_ORG), ne_line(20, 204, 60, 230, NE_ORG), ne_line(176, 96, 214, 120, NE_ORG), ne_line(214, 120, 214, 170, NE_ORG);
    for (k = 0; k < 2 + all / 30 && k < 12; k++) {                     /* the lightning */
        int32_t x = cx + (k - 5) * 3, y = cy + 14, tx = 50 + (int32_t)(((uint32_t)k * 53u + wv_ph * 7u) % 150u), ty = 140 + (int32_t)(((uint32_t)k * 31u) % 60u), n;
        for (n = 1; n <= 8; n++) {
            int32_t nx = x + (tx - x) / (9 - n) + (int32_t)(wvf_rnd() % 7u) - 3, ny = y + (ty - y) / (9 - n);
            wvf_pline(x, y, nx, ny, RGB(0x90, 0xf0, 0x40));
            x = nx, y = ny;
        }
    }
    ne_ring(cx, cy, 18 + wv_bass() / 6, NE_RED);                       /* the sphere: rings turning */
    for (k = 0; k < 4; k++) {
        int32_t rx = (18 + wv_bass() / 6) * WV_SIN[((uint32_t)k * 16u + wv_ph * 2u) & 63u] / 127, py = cy - 18, j;
        for (j = 1; j <= 16; j++) { int32_t y = cy - (18 + wv_bass() / 6) * WV_SIN[((uint32_t)j * 2u + 16u) & 63u] / 127, x = cx + rx * WV_SIN[(uint32_t)j * 2u & 63u] / 127;
            int32_t ox = cx + rx * WV_SIN[(uint32_t)(j - 1) * 2u & 63u] / 127; wvf_pline(ox, py, x, y, NE_RED); py = y; }
    }
    wvf_pline(cx - 18, cy, cx + 18, cy, NE_RED);
    for (k = 0; k < 6; k++) ne_box(14, 96 + k * 7 - (k < all / 40 ? 2 : 0), 18, 5, 0, NE_RED);
    ne_box(60, 24, 34, 12, 0, NE_RED);
}
/* ---- the scenes (after the pixel-art ones): cells of 3 px from a palette of 16, each picture its own */
static const uint16_t SC_MECHA[16] = {0, RGB(0x50, 0x80, 0x90), RGB(0x80, 0xa8, 0xa0), RGB(0xd8, 0xd8, 0xb8), RGB(0xa0, 0xa8, 0x98), RGB(0x68, 0x78, 0x74),
    RGB(0x48, 0x58, 0x56), RGB(0xf0, 0xe8, 0xa0), RGB(0x30, 0x3c, 0x46), RGB(0x58, 0x68, 0x78), RGB(0xc8, 0xcc, 0xc0), RGB(0xe8, 0xc0, 0x40),
    RGB(0xc0, 0x30, 0x30), RGB(0x60, 0xff, 0x90), RGB(0x40, 0x70, 0x38), RGB(0x10, 0x14, 0x18)};
static const uint16_t SC_HANGAR[16] = {0, RGB(0x0c, 0x12, 0x1e), RGB(0x16, 0x20, 0x32), RGB(0x2a, 0x3a, 0x50), RGB(0x50, 0xd8, 0xff), RGB(0x1c, 0x50, 0x70),
    RGB(0x44, 0x44, 0x1c), RGB(0x74, 0x72, 0x2c), RGB(0xa8, 0xa0, 0x50), RGB(0xff, 0x90, 0x20), RGB(0xff, 0x38, 0x10), RGB(0xe8, 0xc0, 0x30),
    RGB(0x08, 0x08, 0x08), RGB(0x24, 0x22, 0x30), RGB(0x8a, 0x4a, 0x28), RGB(0xe8, 0xe8, 0xf0)};
static const uint16_t SC_STATION[16] = {0, RGB(0x30, 0x60, 0xc0), RGB(0x50, 0x80, 0xd8), RGB(0x88, 0xb0, 0xf0), RGB(0xf0, 0xf4, 0xff), RGB(0x60, 0x68, 0x78),
    RGB(0x38, 0x3c, 0x48), RGB(0xc8, 0xb0, 0x90), RGB(0x88, 0x70, 0x58), RGB(0x2a, 0x2a, 0x30), RGB(0xff, 0xf0, 0xa0), RGB(0x88, 0x80, 0x70),
    RGB(0xe0, 0xc0, 0x40), RGB(0x40, 0x2c, 0x20), RGB(0xe8, 0x60, 0x20), RGB(0x20, 0x20, 0x30)};
static UI_NOINLINE void sc_rect(int32_t x, int32_t y, int32_t w, int32_t h, uint32_t v) { int32_t i; wv_force = 1; for (i = 0; i < h; i++) wv_line(x, y + i, x + w - 1, y + i, v); wv_force = 0; }
static UI_NOINLINE void sc_sky(uint32_t a, uint32_t b, uint32_t c, int32_t h)   /* three bands, dithered where they meet */
{
    int32_t x, y;
    for (y = 0; y < h; y++) for (x = 0; x < WVF_N; x++) { int32_t t = y * 3 * 4 / h, k = t / 4, f = t % 4; uint32_t v = (k + ((x + y) % 4 < f ? 1 : 0)) == 0 ? a : (k + ((x + y) % 4 < f ? 1 : 0)) == 1 ? b : c; wv_mem[y * WVF_N + x] = (uint8_t)v; }
}
/* MECHA: a giant robot of our own among ruined towers, seen from below; its eyes light with the bass, the towers' windows with the bands */
static UI_NOINLINE void sc_mecha(void)
{
    int32_t i, k, b = wv_bass(), hi = 0, lo = 0, dy, hx;
    uint32_t beat = wv_ph * (uint32_t)clamp(song.g[G_BPM], 40, 240) / 600u;
    for (i = 0; i < 9; i++) lo += wf_spec[i];
    for (i = 10; i < 19; i++) hi += wf_spec[i];
    dy = (int32_t)(beat & 1u) + b / 10, hx = clamp((hi - lo) / 30, -3, 3);   /* it sways on the beat, turns its head with the melody */
    wvf_hits();
    sc_sky(1, 2, 3, 60);
    for (k = 0; k < 4; k++) { int32_t cx = (int32_t)((wv_ph * (1u + (uint32_t)wv_all() / 60u) / 3u + (uint32_t)k * 31u) % 110u) - 15, cy = 8 + k * 9; wv_disc(cx, cy, 5, 3); wv_disc(cx + 6, cy + 1, 4, 4); wv_disc(cx - 5, cy + 2, 3, 4); }
    for (k = 0; k < 3; k++) {                                          /* the towers */
        int32_t x = 44 + k * 13, top = 18 + k * 7, w = 11;
        sc_rect(x, top, w, 80 - top, k == 1 ? 5 : 6);
        for (i = 0; i < (80 - top) / 4; i++) { int32_t j; for (j = 0; j < 3; j++) sc_rect(x + 2 + j * 3, top + 3 + i * 4, 2, 2, wf_spec[(k * 6 + i + j) % 19] > 9 ? 7 : 15); }
    }
    sc_rect(0, 30 + dy, 30, 50, 8);                                     /* the robot: shoulder, head, crest */
    sc_rect(2, 32 + dy, 14, 16, 10), sc_rect(4, 50 + dy, 22, 4, 9);
    sc_rect(18 + hx, 14 + dy, 18, 20, 9), sc_rect(20 + hx, 16 + dy, 14, 10, 10), sc_rect(22 + hx, 26 + dy, 10, 6, 8);
    wv_force = 1;
    wv_line(27 + hx, 14 + dy, 18 + hx, 4 + dy, 11), wv_line(28 + hx, 14 + dy, 38 + hx, 4 + dy, 11), wv_line(27 + hx, 13 + dy, 19 + hx, 4 + dy, 11), wv_line(28 + hx, 13 + dy, 37 + hx, 4 + dy, 11);
    wv_line(22 + hx, 22 + dy, 32 + hx, 22 + dy, b > 5 || ((wv_ph / 3u) & 1u) ? 13u : 12u), wv_line(22 + hx, 23 + dy, 32 + hx, 23 + dy, 12);
    if (wvf_hit_n) {                                                   /* the beam: from its eyes into a tower, sparks */
        int32_t tx = 50 + (int32_t)((wv_ph * 7u) % 24u), ty = 30 + (int32_t)((wv_ph * 3u) % 20u);
        wv_line(32 + hx, 22 + dy, tx, ty, 13), wv_line(32 + hx, 23 + dy, tx, ty + 1, 3);
        for (i = 0; i < 6; i++) wv_put(tx + (int32_t)(wvf_rnd() % 7u) - 3, ty + (int32_t)(wvf_rnd() % 7u) - 3, 7);
    }
    wv_force = 0;
    sc_rect(28 + hx, 30 + dy, 4, 4, 12);
    for (i = 0; i < 4; i++) sc_rect(6, 38 + i * 3 + dy, 8, 1, wf_spec[12 + i * 2] > 8 ? 13 : 11);   /* the shoulder's lights: the treble */
    for (i = 0; i < 6; i++) wv_disc(64 + i * 4, 76 - (i & 1) * 2, 4, 14);
}
/* HANGAR: a robot's head in a dark hangar: girders with cyan lamps that the notes light, a crane's hook swinging in time, the visor glows */
static UI_NOINLINE void sc_hangar(void)
{
    int32_t i, b = wv_bass(), hx = 58 + WV_SIN[(wv_ph * 2u) & 63u] * 4 / 127, hd;
    static int32_t drop;
    wvf_hits();
    if (wvf_hit_n == 5) drop = 14;                                      /* the hook drops on the kick, then winds back up */
    else if (drop) drop--;
    hd = 22 + drop;
    sc_rect(0, 0, 80, 80, 1);
    sc_rect(0, 8, 80, 50, 2);
    for (i = 0; i < 8; i++) { wv_force = 1; wv_line(i * 10, 4, i * 10 + 10, 10, 3); wv_line(i * 10 + 10, 4, i * 10, 10, 3); wv_force = 0; }
    sc_rect(0, 3, 80, 1, 3), sc_rect(0, 10, 80, 1, 3);
    for (i = 0; i < 19; i++) sc_rect(2 + i * 4, 12 + (i & 1) * 30, 1, 1, wf_spec[i] > 8 || (int32_t)((wv_ph / 2u) % 19u) == i ? 4 : 5);   /* the lamps: the bands, a chase */
    wv_force = 1; wv_line(58, 10, hx, hd, 3); wv_force = 0;
    sc_rect(hx - 4, hd, 9, 5, 7); for (i = 0; i < 4; i++) sc_rect(hx - 4 + i * 2, hd + 3, 1, 2, 11);
    wv_disc(30, 46, 16, 7);                                            /* the head */
    sc_rect(14, 46, 32, 16, 6), sc_rect(18, 40, 26, 4, 8);
    sc_rect(20, 48, 22, 4, 12), sc_rect(22, 49, 18, 2, b > 8 ? 10u : 9u), sc_rect(22 + (int32_t)((wv_ph * 2u) % 16u), 49, 3, 2, 15);   /* the visor, a scan running */
    if (wf_spec[16] > 8) for (i = 0; i < 5; i++) wv_put(50 + (int32_t)(wvf_rnd() % 6u), 50 + (int32_t)(wvf_rnd() % 6u), i & 1 ? 9u : 15u);   /* welding sparks on the treble */
    wv_force = 1; wv_line(26, 32, 18, 18, 8), wv_line(27, 32, 20, 18, 8); wv_line(4, 40, 20, 44, 6), wv_line(4, 41, 20, 45, 6); wv_force = 0;
    sc_rect(4, 38, 4, 4, 14);
    sc_rect(48, 54, 32, 26, 6), sc_rect(48, 54, 32, 2, 8);
    for (i = 0; i < 8; i++) sc_rect(i * 4, 66, 1, 14, 3);
    sc_rect(0, 66, 30, 1, 3);
    sc_rect(64, 46, 2, 8, 13), sc_rect(64, 44, 2, 2, 14);
    for (i = 0; i < 5; i++) sc_rect(70 + (i & 1), 20 + i * 6, 1, 1, ((wv_ph / 5u + (uint32_t)i) % 3u) ? 5u : 4u);
}
/* STATION: a train station's platform under an arched roof (as the fighting games' stages): the lamps glow on the beat, the people on
 * the platform nod to the tempo, a train pulls in on the kick */
static int32_t sc_train = -1;
static UI_NOINLINE void sc_station(void)
{
    int32_t i, k, beat = (int32_t)((wv_ph * (uint32_t)clamp(song.g[G_BPM], 40, 240) / 600u) & 1u);
    sc_sky(3, 2, 1, 46);
    for (k = 0; k < 2; k++) { int32_t cx = 50 + k * 18 - (int32_t)((wv_ph / 6u) % 30u); wv_disc(cx, 12 + k * 6, 3, 4); wv_disc(cx + 4, 13 + k * 6, 2, 4); }
    sc_rect(0, 30, 52, 22, 7);                                          /* the building, its arched windows */
    for (i = 0; i < 4; i++) { wv_disc(6 + i * 12, 38, 4, 8); sc_rect(2 + i * 12, 38, 9, 8, 8); }
    for (k = 0; k < 5; k++) {                                          /* the roof: arches */
        int32_t j, px = -1, py = 0;
        for (j = 0; j <= 16; j++) { int32_t x = j * 4 - 8 + k * 6, y = 4 + k * 3 + (j - 8) * (j - 8) / 4; if (j) { wv_force = 1; wv_line(px, py, x, y, 5); wv_force = 0; } px = x, py = y; }
    }
    sc_rect(0, 2, 80, 2, 6);
    for (i = 0; i < 5; i++) sc_rect(4 + i * 16, 20, 1, 32, 6);
    sc_rect(0, 52, 80, 10, 11), sc_rect(0, 52, 80, 1, 12);
    sc_rect(0, 62, 80, 18, 13);
    for (i = 0; i < 3; i++) { sc_rect(14 + i * 26, 40, 1, 12, 9); sc_rect(13 + i * 26, 38, 3, 2, beat || wf_spec[i * 6] > 9 ? 10u : 9u); }
    for (i = 0; i < 4; i++) { int32_t x = 20 + i * 13, y = 46 + (((uint32_t)beat + (uint32_t)i) & 1u) - wf_spec[i * 5] / 5; sc_rect(x, y, 2, 6, 15), sc_rect(x, y - 2, 2, 2, 15);
        if (wf_spec[i * 5] > 10) sc_rect(x - 1, y - 1, 1, 2, 15), sc_rect(x + 2, y - 1, 1, 2, 15); }   /* (arms up when it is loud) */
    if (wvf_hit_n == 5 && sc_train < 0) sc_train = 80;
    if (sc_train >= 0) {
        sc_rect(sc_train, 56, 60, 14, 14), sc_rect(sc_train, 58, 60, 2, 12);
        for (i = 0; i < 6; i++) sc_rect(sc_train + 4 + i * 9, 61, 5, 3, 3);
        sc_train -= 3;
        if (sc_train < -70) sc_train = -1;
    }
}
/* BLOCKS: a 1-bit estate of tower blocks; the windows light with the bands, the lamp post flickers on the kick, someone runs past */
static UI_NOINLINE void sc_blocks(void)
{
    int32_t i, j, k, run = (int32_t)((wv_ph * 2u) % 120u) - 20;
    memset(wv_mem, 0, WVF_N * WVF_N);
    for (k = 0; k < 3; k++) {
        static const int8_t BX[3][4] = {{0, 0, 26, 70}, {32, 12, 22, 46}, {60, 0, 20, 64}};   /* x, top, w, bottom */
        int32_t x = BX[k][0], t = BX[k][1], w = BX[k][2], bt = BX[k][3];
        for (j = t; j < bt; j++) wv_line(x, j, x + w - 1, j, 15);
        for (j = t + 2; j < bt - 2; j += 4) for (i = x + 2; i < x + w - 2; i += 4) {
            uint32_t on = wf_spec[(uint32_t)(i + j * 3 + k * 5) % 19u] > 9;
            wv_force = 1; wv_line(i, j, i + 1, j, on ? 15u : 1u); wv_line(i, j + 1, i + 1, j + 1, on ? 15u : 1u); wv_force = 0;
        }
    }
    for (j = 70; j < 80; j++) for (i = 0; i < 80; i++) if (((i * 7 + j * 13 + (int32_t)(wv_ph / 3u)) % 5) < 2) wv_put(i, j, 15);   /* the ground, dithered */
    wv_force = 1;
    wv_line(56, 28, 56, 76, 15), wv_line(56, 28, 62, 26, 15);
    if (wvf_hit_n || ((wv_ph / 3u) % 7u)) wv_line(60, 27, 63, 27, 15);
    wv_force = 0;
    if (run > 0 && run < 80) { wv_line(run, 66, run, 72, 15); wv_line(run, 72, run - 2 + (int32_t)(wv_ph & 2u), 76, 15); wv_line(run, 72, run + 2 - (int32_t)(wv_ph & 2u), 76, 15); wv_put(run, 65, 15); }
}
/* ---- the half-tone ones (after the printed references): a picture of 0..15 per cell, printed as dots that grow with it */
static UI_NOINLINE void ht_blob(int32_t cx, int32_t cy, int32_t r, int32_t lit)   /* a puff, lit from the top left */
{
    int32_t x, y;
    for (y = -r; y <= r; y++) for (x = -r; x <= r; x++) {
        int32_t d = x * x + y * y;
        if (d > r * r || (uint32_t)(cx + x) >= WVF_N || (uint32_t)(cy + y) >= WVF_N) continue;
        wv_put(cx + x, cy + y, (uint32_t)clamp(lit - (y + r) * 9 / (2 * r + 1) - (x + r) * 3 / (2 * r + 1) + d * 2 / (r * r + 1), 2, 15));
    }
}
/* CLOUDS: a tower of cloud printed in cream dots on night blue, swelling with the bands; a moon, four-pointed stars that flash on the notes */
static UI_NOINLINE void ht_clouds(void)
{
    static const int8_t C[11][3] = {{40, 72, 20}, {20, 66, 15}, {60, 64, 16}, {42, 52, 15}, {28, 44, 12}, {54, 42, 12}, {40, 32, 11}, {48, 22, 9},
                                    {32, 24, 8}, {8, 76, 12}, {72, 76, 12}};
    int32_t i, s = WV_SIN[(wv_ph) & 63u] / 60;
    memset(wv_mem, 0, WVF_N * WVF_N);
    for (i = 0; i < 11; i++) ht_blob(C[i][0], C[i][1] + s, C[i][2] + wf_spec[(i * 2) % 19] / 6, 15);
    wv_disc(66, 9, 4, 14); wv_force = 1; wv_disc(68, 8, 4, 0); wv_force = 0;
    for (i = 0; i < 9; i++) {
        int32_t x = (int32_t)(((uint32_t)i * 29u + 5u) % 78u), y = (int32_t)(((uint32_t)i * 11u + 2u) % 20u), on = wf_spec[i * 2] > 9;
        wv_put(x, y, 15); if (on) { wv_put(x - 1, y, 10); wv_put(x + 1, y, 10); wv_put(x, y - 1, 10); wv_put(x, y + 1, 10); }
    }
}
/* HANDS: two hands reaching, printed in dots; between their fingers a light that grows with the volume and throws sparks on the kick */
static UI_NOINLINE void ht_finger(int32_t x0, int32_t y0, int32_t x1, int32_t y1, int32_t r) { int32_t k; for (k = 0; k <= 8; k++) ht_blob(x0 + (x1 - x0) * k / 8, y0 + (y1 - y0) * k / 8, r, 11); }
static UI_NOINLINE void ht_hands(void)
{
    int32_t i, x, y, lr = 6 + wv_all() / 30, cx = 38, cy = 44;
    memset(wv_mem, 0, WVF_N * WVF_N);
    ht_finger(84, 6, 62, 24, 6), ht_blob(58, 28, 7, 12);                 /* the hand from above */
    ht_finger(54, 30, 43, 38, 2), ht_finger(57, 33, 48, 42, 2), ht_finger(60, 34, 53, 43, 2), ht_finger(62, 33, 58, 41, 2);
    ht_finger(-4, 76, 18, 62, 6), ht_blob(22, 58, 7, 12);               /* the hand from below */
    ht_finger(24, 54, 31, 45, 2), ht_finger(27, 56, 35, 48, 2), ht_finger(29, 58, 38, 52, 2), ht_finger(17, 56, 19, 47, 2);
    for (y = -lr; y <= lr; y++) for (x = -lr; x <= lr; x++) { int32_t d = x * x + y * y; if (d <= lr * lr) wv_put(cx + x, cy + y, (uint32_t)(15 - d * 12 / (lr * lr + 1))); }
    if (wvf_hit_n) for (i = 0; i < 8; i++) wv_put(cx + (int32_t)(wvf_rnd() % 25u) - 12, cy + (int32_t)(wvf_rnd() % 25u) - 12, 15);
}
/* INDIGO: white dots on blue: a great wave curling round, thicker with the bass, its spray flying on the kick */
static UI_NOINLINE void ht_indigo(void)
{
    int32_t i, a, b = wv_bass();
    memset(wv_mem, 0, WVF_N * WVF_N);
    for (a = 0; a < 200; a++) {                                        /* the curl: a spiral arm, thick at its foot */
        uint32_t an = (uint32_t)a + wv_ph;
        int32_t r = 34 - a / 8, th = 2 + (200 - a) / 30 + b / 6;
        int32_t x = 36 + r * wv_sin4(an + 64u) / 127, y = 44 + r * wv_sin4(an) / 127;
        for (i = 0; i < th; i++) wv_put(x + i * wv_sin4(an + 64u) / 127, y + i * wv_sin4(an) / 127, (uint32_t)clamp(15 - i * 10 / th, 3, 15));
    }
    for (i = 0; i < 80; i++) wv_put(i, 70 + WV_SIN[((uint32_t)i * 3u + wv_ph * 2u) & 63u] / 40, 12), wv_put(i, 74, 7), wv_put(i, 78, 9);
    if (wvf_hit_n) for (i = 0; i < 14; i++) wv_put(50 + (int32_t)(wvf_rnd() % 26u), 20 + (int32_t)(wvf_rnd() % 30u), 15);
}

static const char *const KJ_SPR[31] = {".................................", "...............P.................", "..............PPP...MMMMMM.......", ".............PPMP..MMLLLLMM......", "............PPMMP.MMLLLLLLMM.....", "...........PPMMMPMMLLWKLLLLMM....", "..........PPMMMMMMMLLWKLLLLLLM...", ".........PPMMMMMMMDMMLLLLLLLLLM..", "........PPMMMMMMMDDDMMMMMLLLLLMM.", "........PMMMMMMMDDDMMMMMMMMTTTTT.", ".......PPMMMMMMDDDMMMMMMRRRRRRR..", ".......PMMMMMMDDDMMMMMMRRRRR.....", "......PPMMMMMDDDMMMMMMMRRRRRRR...", "......PMMMMMDDDMMMMMMMMMTTTTTTT..", ".....PPMMMMDDDMMMMMMMMMMMMMMMM...", ".....PMMMMDDDMMMMMMMMMMMMMMM.....", "....PPMMMDDDMMMMMLLLLMMMMMM......", "....PMMMDDDMMMMMLLLLLLMMMMMMM....", "...PPMMDDDMMMMMLLLLLLLLMMMMMMM...", "...PMMDDDMMMMMMLLLLLLLLMMDDMMMM..", "..PPMDDDMMMMMMMLLLLLLLLMMDMMKMM..", "..PMMDDMMMMMMMMMLLLLLLMMMDMMMM...", ".PPMDDMMMMMMMMMMMLLLLMMMMMDKK....", ".PMMDDMMMMMMMMMMMMMMMMMMMMM......", "PPMDDMMMMMMMMMMMMMMMMMMMMMM......", "PMMDDMMMMMMMMMMMMMMMMMMMMMM......", "PMDDMMMMMMMMMMMMMMMMMMMMMMM......", "MMDDMMMMMMMMMMMMMMMMMMMMMMM......", "MDDMMMMMMMMMMMMMMMMMMMMMMMM......", "MDDMMMMMMMMMMMMMMMMMMMMMMMM......", "DDMMMMMMMMMMMMMMMMMMMMMMMMM......"};
static const char *const BX_G[30] = {"..........HHHH..........", ".........HHHHHH.........", ".........HSSSSS.........", ".........SSSSSS.........", ".........SSSSSSs........", "..........SSSSs.........", "..........ssss..........", "........SSSSSSSS........", ".......SSSSSSSSSSGGG....", "......SSSSSSSSSSGGGGG...", "......SSSSSSSSSSGGGGG...", ".....sSSSSSSSSGGGGGG....", ".....sSSSSSSSSSGGGG.....", ".....sSSSSSSSSSSs.......", "....GGGsSSSSSSSs........", "...GGGGGsSSSSSs.........", "...GGGGGsSSSSS..........", "....GGG.sSSSSS..........", "........BBBBBBB.........", "........BwBBBBB.........", "........BBBBBBBB........", "........BBBBBBBB........", "........SSS..SSS........", ".......SSS....SSS.......", ".......SSS....SSS.......", "......SSS......SSS......", "......sSS......SSs......", "......sS........Ss......", "......NNN.......NNN.....", ".....NNNN.......NNNN...."};
static const char *const BX_J[30] = {"..........HHHH............", ".........HHHHHH...........", ".........HSSSSS...........", ".........SSSSSS...........", ".........SSSSSSs..........", "..........SSSSs...........", "..........ssss............", "........SSSSSSSSSSSSSGGGG.", ".......SSSSSSSSSSSSSGGGGGG", "......SSSSSSSSSS....GGGGGG", "......SSSSSSSSSS.....GGGG.", ".....sSSSSSSSSs...........", ".....sSSSSSSSSs...........", ".....sSSSSSSSSSs..........", "....GGGsSSSSSSSs..........", "...GGGGGsSSSSSs...........", "...GGGGGsSSSSS............", "....GGG.sSSSSS............", "........BBBBBBB...........", "........BwBBBBB...........", "........BBBBBBBB..........", ".......BBBBBBBBB..........", ".......SSS...SSS..........", "......SSS.....SSS.........", "......SSS......SSS........", ".....SSS.......SSS........", ".....sSS........SSs.......", ".....sS.........SSs.......", ".....NNN.........NNN......", "....NNNN.........NNNN....."};
/* ---- the drawn ones */
static UI_NOINLINE void spr_draw(const char *const *rows, int32_t nr, int32_t x, int32_t y, int32_t z, const char *keys, const uint16_t *cols)   /* a sprite: each letter of keys a colour */
{
    int32_t r, c;
    for (r = 0; r < nr; r++)
        for (c = 0; rows[r][c]; c++) {
            const char *k = keys;
            int32_t i = 0;
            while (*k && *k != rows[r][c]) k++, i++;
            if (*k) WFR(x + c * z, y + r * z, z, z, cols[i]);
        }
}
/* KAIJU: a monster over the city (after the pixel one with the needle tower): teal sky, brown and orange blocks with cream windows, the tower
 * with its orange saucer; the monster breathes with the beat, its plates glow as the bass builds, and on the kick it breathes a blue ray
 * into the towers: they flash and shed bricks */
static int32_t kj_ray;
static UI_NOINLINE void v2_kaiju(void)
{
    static const uint16_t KC[8] = {RGB(0x96, 0xdc, 0xc8), RGB(0x34, 0x40, 0x3a), RGB(0x22, 0x2a, 0x26), RGB(0x54, 0x64, 0x5c), RGB(0xf0, 0xf0, 0xe6), RGB(0x0a, 0x0a, 0x0a), RGB(0xe8, 0xe0, 0xc8), RGB(0xc8, 0x3c, 0x28)};
    static const uint16_t BC[5] = {RGB(0x5a, 0x34, 0x2c), RGB(0x2a, 0x2a, 0x2c), RGB(0xc8, 0x70, 0x40), RGB(0x80, 0x5c, 0x50), RGB(0x30, 0x48, 0x48)};
    int32_t i, r, c, bob = (int32_t)((wv_ph * (uint32_t)clamp(song.g[G_BPM], 40, 240) / 600u) & 1u) * 2, glow = clamp(wv_bass() * 6, 0, 60);
    uint16_t pc = RGB(0x96 + glow, 0xdc, 0xc8 + glow / 2 > 0xff ? 0xff : 0xc8 + glow / 2);
    WFR(0, 0, 240, 240, RGB(0x00, 0x96, 0x96));
    if (wvf_hit_n == 5) kj_ray = 12;
    {                                                                  /* the needle tower */
        int32_t tx = 150;
        WFR(tx - 2, 60, 4, 160, RGB(0x80, 0xc8, 0xc0)), WFR(tx - 1, 40, 2, 20, RGB(0x20, 0x20, 0x20)), WFR(tx, 34, 1, 6, RGB(0xc0, 0x30, 0x20));
        for (r = 0; r < 8; r++) WFR(tx - 22 + r, 60 + r, 44 - 2 * r, 1, r < 3 ? RGB(0xe8, 0x80, 0x30) : RGB(0xf0, 0xe0, 0xc0));
        WFR(tx - 8, 120, 16, 2, RGB(0x80, 0xc8, 0xc0)), wvf_pline(tx - 6, 122, tx - 12, 220, RGB(0x80, 0xc8, 0xc0)), wvf_pline(tx + 6, 122, tx + 12, 220, RGB(0x80, 0xc8, 0xc0));
    }
    for (i = 0; i < 12; i++) {                                         /* the blocks */
        uint32_t h = pc_h((uint32_t)i * 19u + 3u);
        int32_t bx = 70 + i * 15 - (i > 4 ? 0 : 0), bw = 14 + (int32_t)(h % 10u), bh = 40 + (int32_t)((h >> 4) % 70u), t = 240 - bh, hit = kj_ray > 0 && i >= 6 && i <= 9;
        uint16_t col = hit && (kj_ray & 2) ? RGB(0xf0, 0xf0, 0xff) : BC[h % 5u];
        WFR(bx, t, bw, bh, col), WFR(bx, t, bw, 2, RGB(0x20, 0x20, 0x20));
        for (r = 0; r < bh / 6 - 1; r++) for (c = 0; c < (bw - 2) / 4; c++) if (((uint32_t)(r + c) + h) % 4u != 0u) WFR(bx + 2 + c * 4, t + 4 + r * 6, 2, 2, (int32_t)sp_s[(uint32_t)(r + i) % 19u] > 9 ? RGB(0xff, 0xff, 0xe0) : RGB(0xf0, 0xe0, 0xc0));
        if (hit && kj_ray > 6) for (c = 0; c < 4; c++) WFR(bx + (int32_t)(pc_h(wv_ph + (uint32_t)c) % (uint32_t)bw), t - (12 - kj_ray) * 3 - c * 4, 3, 3, BC[h % 5u]);
    }
    for (r = 0; r < 31; r++) for (c = 0; KJ_SPR[r][c]; c++) {          /* the monster: 3 px, its plates glowing */
        char ch = KJ_SPR[r][c];
        uint16_t col;
        if (ch == '.') continue;
        col = ch == 'P' ? pc : ch == 'M' ? KC[1] : ch == 'D' ? KC[2] : ch == 'L' ? KC[3] : ch == 'W' ? KC[4] : ch == 'K' ? KC[5] : ch == 'T' ? KC[6] : KC[7];
        if (ch == 'R' && kj_ray) col = RGB(0x60, 0xd0, 0xff);
        WFR(c * 4 - 6, 116 + r * 4 + bob, 4, 4, col);
    }
    if (kj_ray) {                                                      /* the ray */
        int32_t x0 = 112, y0 = 158 + bob, w;
        for (w = 0; w < 6; w++) wvf_pline(x0, y0 + w - 3, 190, 150 + w * 3 - 6, w == 2 || w == 3 ? RGB(0xe0, 0xf8, 0xff) : RGB(0x40, 0xa0, 0xf0));
        for (i = 0; i < 10; i++) WFR(x0 + 10 + (int32_t)(pc_h(wv_ph * 3u + (uint32_t)i) % 80u), y0 - 10 + (int32_t)(pc_h(wv_ph + (uint32_t)i * 5u) % 24u), 2, 2, RGB(0x90, 0xe0, 0xff));
        kj_ray--;
    }
}
/* BOXER: a boxer in a ring (after the pixel boxing one): guard up, bouncing on the beat; a note on the left half throws the jab, on the right
 * half the hook; the heavy bag swings when it lands; the crowd's flashes go off with the bands */
static int32_t bx_t, bx_bag;
static UI_NOINLINE void v2_boxer(void)
{
    static const uint16_t BCOL[9] = {RGB(0x28, 0x18, 0x10), RGB(0xc8, 0x88, 0x5c), RGB(0x9c, 0x64, 0x40), RGB(0x10, 0x08, 0x08), RGB(0xd8, 0x28, 0x28), RGB(0x30, 0x50, 0xc0), RGB(0xf8, 0xf8, 0xf8), RGB(0x18, 0x18, 0x20), RGB(0xd8, 0x28, 0x28)};
    int32_t i, bob = (int32_t)((wv_ph * (uint32_t)clamp(song.g[G_BPM], 40, 240) / 600u) & 1u) * 3, sw;
    char b[8];
    WFR(0, 0, 240, 240, RGB(0x14, 0x12, 0x18));
    for (i = 0; i < 90; i++) {                                         /* the crowd, its flashes */
        int32_t x = (int32_t)(pc_h((uint32_t)i * 7u) % 240u), y = 10 + (int32_t)(pc_h((uint32_t)i * 13u) % 80u);
        wvf_disk(x, y, 4, RGB(0x2a, 0x26, 0x30));
        if (sp_s[i % 19] > 12 && (pc_h((uint32_t)i + wv_ph / 2u) & 7u) == 0u) WFR(x - 1, y - 1, 3, 3, WC_WHT);
    }
    WFR(0, 150, 240, 90, RGB(0xd0, 0xd0, 0xd8)), WFR(0, 150, 240, 3, RGB(0xa0, 0xa0, 0xb0));   /* the canvas, the ropes, the posts */
    for (i = 0; i < 3; i++) WFR(0, 96 + i * 18, 240, 3, i == 0 ? RGB(0xd8, 0x30, 0x30) : i == 1 ? RGB(0xf0, 0xf0, 0xf0) : RGB(0x30, 0x50, 0xd0));
    WFR(10, 90, 8, 64, RGB(0xd8, 0x30, 0x30)), WFR(222, 90, 8, 64, RGB(0x30, 0x50, 0xd0));
    if (nk_low(nk_on)) bx_t = 1; else if (nk_high(nk_on)) bx_t = 101; else if (wvf_hit_n == 5 && !bx_t) bx_t = 1;
    {                                                                  /* the bag: it swings when a punch lands */
        int32_t bx0 = 182;
        if (bx_t == 4 || bx_t == 104) bx_bag = 14;
        sw = bx_bag * WV_SIN[(wv_ph * 6u) & 63u] / 127;
        if (bx_bag) bx_bag--;
        wvf_pline(bx0, 0, bx0 + sw / 3, 70, RGB(0x60, 0x60, 0x60));
        WFR(bx0 - 14 + sw, 70, 28, 64, RGB(0x90, 0x24, 0x20)), WFR(bx0 - 14 + sw, 70, 6, 64, RGB(0xb0, 0x34, 0x30)), WFR(bx0 - 14 + sw, 84, 28, 2, RGB(0x50, 0x10, 0x10));
    }
    {                                                                  /* the boxer: guard, the jab or the hook */
        int32_t jab = bx_t > 0 && bx_t < 8 && bx_t < 100, hook = bx_t > 100 && bx_t < 108;
        spr_draw(jab || hook ? BX_J : BX_G, 30, 54 + (jab || hook ? 10 : 0), 84 + bob, 4, "HSsKGBwN", (const uint16_t[]){BCOL[0], BCOL[1], BCOL[2], BCOL[3], BCOL[4], BCOL[5], BCOL[6], BCOL[7]});
        if (bx_t) { bx_t++; if (bx_t == 9 || bx_t == 109) bx_t = 0; }
    }
    WFR(6, 214, 64, 20, RGB(0x10, 0x10, 0x14)), wf_text(10, 218, 1, "ROUND", RGB(0xf0, 0xc0, 0x30)), fmt_int(b, (int32_t)(wf_secs() / 30u % 12u) + 1), wf_text(52, 218, 1, b, WC_WHT);
    WFR(150, 214, 84, 20, RGB(0x10, 0x10, 0x14)), wf_text(154, 218, 1, "POWER", RGB(0xf0, 0xc0, 0x30));
    for (i = 0; i < 8; i++) WFR(194 + i * 5, 218, 4, 10, i < clamp(ev_lvl / 25, 0, 8) ? RGB(0xff, 0x50, 0x40) : RGB(0x40, 0x20, 0x20));
}
/* HOUND: the dog in neon lines (it was the cow): when the music stops it puts its head down to its bowl and eats; when it plays it lifts it
 * and nods to the beat. The sound goes in at its mouth and comes out at its tail as notes; inside it four things, each its own: a ring
 * turning with the tempo, the four tracks as meters, the spectrum, a square wave as tall as the bass */
static int32_t hd_head;                         /* 0 up .. 32 down at the bowl */
static UI_NOINLINE void v2_hound(void)
{
    int32_t i, quiet = !nk_mask && ev_lvl < 12, nod = quiet ? 0 : (int32_t)((wv_ph * (uint32_t)clamp(song.g[G_BPM], 40, 240) / 600u) & 1u) * 4;
    int32_t hx, hy, tail = WV_SIN[(wv_ph * (quiet ? 2u : 8u)) & 63u] * (quiet ? 4 : 10) / 127, chew = (int32_t)((wv_ph / 3u) & 1u);
    hd_head += quiet ? (hd_head < 32 ? 2 : 0) : (hd_head > 0 ? -4 : 0);
    if (hd_head < 0) hd_head = 0;
    hx = 74 - hd_head / 3, hy = 92 + hd_head * 2 + nod;
    ne_line(80, 104, 176, 104, NE_BRN), ne_line(176, 104, 186, 114, NE_BRN), ne_line(186, 114, 186, 140, NE_BRN), ne_line(186, 140, 176, 150, NE_BRN);   /* the body */
    ne_line(176, 150, 80, 150, NE_BRN), ne_line(80, 150, 72, 140, NE_BRN), ne_line(72, 140, 72, 114, NE_BRN), ne_line(72, 114, 80, 104, NE_BRN);
    for (i = 0; i < 4; i++) { static const int16_t LX[4] = {84, 104, 156, 174}; int32_t st = !quiet && ((wv_ph / 4u + (uint32_t)i) & 1u) ? 2 : 0; ne_line(LX[i], 150, LX[i] + st, 186, NE_BRN), ne_line(LX[i] + st, 186, LX[i] + st + 6, 186, NE_BRN); }
    ne_line(184, 108, 198 + tail, 88 - (tail < 0 ? -tail : tail) / 2, NE_BRN);   /* the tail, wagging */
    ne_line(80, 110, hx + 4, hy + 18, NE_BRN), ne_line(80, 124, hx + 6, hy + 24, NE_BRN);   /* the neck */
    ne_line(hx, hy, hx - 24, hy + 4, NE_BRN), ne_line(hx - 24, hy + 4, hx - 32, hy + 14, NE_BRN), ne_line(hx - 32, hy + 14, hx - 24, hy + 20 + (quiet ? chew * 3 : 0), NE_BRN);   /* the head */
    ne_line(hx - 24, hy + 20 + (quiet ? chew * 3 : 0), hx + 2, hy + 24, NE_BRN), ne_line(hx + 2, hy + 24, hx + 8, hy + 10, NE_BRN), ne_line(hx + 8, hy + 10, hx, hy, NE_BRN);
    ne_line(hx - 2, hy + 2, hx + 4, hy + 22 + nod / 2, NE_PNK), ne_line(hx + 4, hy + 22 + nod / 2, hx + 10, hy + 18, NE_PNK);   /* the ear */
    WFR(hx - 14, hy + 8, 3, 3, quiet ? NE_DIM : NE_BLU), WFR(hx - 33, hy + 12, 3, 3, NE_RED);
    ne_line(26, 192, 64, 192, NE_GRN), ne_line(26, 192, 30, 200, NE_GRN), ne_line(64, 192, 60, 200, NE_GRN), ne_line(30, 200, 60, 200, NE_GRN);   /* the bowl */
    for (i = 0; i < 6; i++) WFR(32 + i * 5, 188 - (i & 1) * 2 + (quiet && chew && i == 3 ? -3 : 0), 3, 3, NE_ORG);
    ne_line(0, 202, 240, 202, NE_GRN);
    if (!quiet) for (i = 0; i < 8; i++) { int32_t x = 4 + ((int32_t)(wv_ph * 3u) + i * 9) % 72; if (x < hx - 34) WFR(x, hy + 12 + WV_SIN[((uint32_t)x * 4u) & 63u] / 40, 3, 2, NE_BLU); }   /* the sound going in */
    if (!quiet) for (i = 0; i < 5; i++) {                              /* and coming out at the tail, as notes */
        int32_t d = (int32_t)((wv_ph * 2u + (uint32_t)i * 13u) % 60u), x = 200 + d, y = 84 - d / 2 + WV_SIN[((uint32_t)d * 6u + (uint32_t)i * 11u) & 63u] / 30;
        WFR(x, y, 3, 3, i & 1 ? NE_PNK : NE_GRN), WFR(x + 2, y - 6, 1, 6, i & 1 ? NE_PNK : NE_GRN);
    }
    {                                                                  /* inside: four things, side by side */
        uint32_t a = wv_ph * (uint32_t)(1 + clamp(song.g[G_BPM], 40, 240) / 60), q;
        int32_t r = 6 + wv_bass() / 4, k;
        ne_ring(96, 127, r, NE_BLU);
        for (q = 0; q < 3u; q++) { uint32_t an = a + q * 85u; WFR(96 + r * wv_sin4(an + 64u) / 127 - 1, 127 + r * wv_sin4(an) / 127 - 1, 3, 3, WC_WHT); }
        for (k = 0; k < 4; k++) { int32_t h = (int32_t)trk_level((uint32_t)k) * 30 / 127 * (trk[k].p[P_MUTE] ? 0 : 1) * (int32_t)(sp_s[k * 4] + 4) / 20; WFR(114 + k * 6, 142 - clamp(h, 1, 30), 4, clamp(h, 1, 30), k == 3 ? NE_RED : NE_PUR); }
        for (k = 0; k < 8; k++) { int32_t h = clamp((int32_t)sp_s[k * 2 + 1], 1, 30); WFR(142 + k * 3, 142 - h, 2, h, k & 1 ? NE_RED : NE_GRN); }
        for (k = 0; k < 3; k++) { int32_t xs = 168 + k * 5, hh = 3 + wv_bass() / 2; ne_line(xs, 132, xs, 132 - hh, NE_ORG), ne_line(xs, 132 - hh, xs + 3, 132 - hh, NE_ORG), ne_line(xs + 3, 132 - hh, xs + 3, 132, NE_ORG); }
    }
    { char b[8]; wf_text(10, 14, 1, "FREQ", NE_BLU), ne_gauge(24, 44, ne_frac(song.g[G_FILT], GP[G_FILT].min, GP[G_FILT].max), NE_BLU);
      wf_text(196, 14, 1, "BPM", NE_PUR), fmt_int(b, song.g[G_BPM]), ne_num(186, 28, 22, song.g[G_BPM], NE_GRN); (void)b; }
}
static UI_NOINLINE void wt_make(uint32_t n, int back, uint32_t spin);
static UI_NOINLINE void wt_quad(int32_t cx, int32_t cy, int32_t w0, int32_t h0, uint32_t a, int32_t shade, int32_t bs);
/* TAPE: the tape of the sound on the track, big, floating in the TAPES room: it sways, the notes nudge it round (left keys one way, right
 * keys the other) but it turns at a calm pace whatever comes in, its reels spin while it plays, it rises and settles with the bass */
static int32_t tp_ang, tp_tgt, tp_bob;
static UI_NOINLINE void v2_tape_step(void)
{
    uint32_t tot, a;
    if (nk_on) tp_tgt = 14;                                            /* a note: a pulse */
    else if (tp_tgt) tp_tgt--;
    tp_ang++;                                                          /* round and round, as on a select screen: a turn every ~3 s */
    tp_bob += (wv_bass() * 2 - tp_bob) / 6;
    a = (uint32_t)tp_ang & 63u;
    wt_make(preset_pos(&tot), ((a + 16u) & 63u) >= 32u, song.playing ? fm1_ms / 55u : 0u);
}
static UI_NOINLINE void v2_tape(void)
{
    static const uint8_t BY[16] = {0, 8, 2, 10, 12, 4, 14, 6, 3, 11, 1, 9, 15, 7, 13, 5};
    static const uint16_t G[5] = {RGB(0x7a, 0x70, 0x90), RGB(0x5c, 0x48, 0x82), RGB(0x40, 0x28, 0x6c), RGB(0x2a, 0x14, 0x4c), RGB(0x16, 0x08, 0x2a)};
    int32_t x, y, i;
    char nm[14];
    for (y = wf_ry & ~1; y < wf_ry + wf_rh; y += 2) {                  /* the room: a soft light above, a dithered purple, the grid floor */
        int32_t f = y * 4 * 16 / 240, band = f / 16, fr = f % 16;
        for (x = 0; x < 240; x += 2) WFR(x, y, 2, 2, G[clamp(band + (BY[((y >> 1) & 3) * 4 + ((x >> 1) & 3)] < (uint32_t)fr ? 1 : 0), 0, 4)]);
    }
    for (i = 0; i < 8; i++) WFR(0, 180 + i * i, 240, 1, RGB(0x60, 0x40, 0x98));
    for (i = -8; i <= 8; i++) cv_line(120 + i * 12, 180, 120 + i * 40, 240, RGB(0x60, 0x40, 0x98));
    wt_quad(120, 104 - tp_bob / 2 + WV_SIN[(wv_ph * 2u) & 63u] / 40, 180 + tp_tgt * 2, 113 + tp_tgt, (uint32_t)tp_ang & 63u, 0, 2);

    if (song.sel < NPART) trk_short_name(song.sel, nm); else str_cpy(nm, "DRUMS", 14);
    wf_textc(120, 214, 1, nm, WC_WHT);
}
/* TARGET, BLUEPRINT: more of them moves with the music */
static uint8_t tg_fl[6];
static UI_NOINLINE void v2_target(void)
{
    int32_t k, r = 92 + wv_bass();
    ne_target();
    for (k = 0; k < 27; k++) if ((nk_on >> k) & 1u) tg_fl[k % 6] = 8;
    for (k = 0; k < 6; k++) {                                          /* the corners flash with the notes */
        uint32_t an = (uint32_t)k * 256u / 6u;
        int32_t x = 120 + r * wv_sin4(an + 64u) / 127, y = 124 + r * wv_sin4(an) / 127, s = tg_fl[k];
        if (s) { ne_line(x - s, y, x, y - s, RGB(0xff, 0xf0, 0x60)), ne_line(x, y - s, x + s, y, RGB(0xff, 0xf0, 0x60)), ne_line(x + s, y, x, y + s, RGB(0xff, 0xf0, 0x60)), ne_line(x, y + s, x - s, y, RGB(0xff, 0xf0, 0x60)); tg_fl[k]--; }
    }
    for (k = 0; k < 3; k++) { uint32_t an = wv_ph * (uint32_t)(1 + clamp(song.g[G_BPM], 40, 240) / 60) + (uint32_t)k * 85u; wvf_disk(120 + 74 * wv_sin4(an + 64u) / 127, 124 + 74 * wv_sin4(an) / 127, 2, RGB(0x60, 0xe0, 0xff)); }
    if (wvf_hit_n > 2) { WFR(96, 196, 48, 12, RGB(0xff, 0x30, 0x20)), wf_textc(120, 199, 1, "LOCK", WC_WHT); }
}
static UI_NOINLINE void v2_blueprint(void)
{
    int32_t k, tx = nk_top(nk_mask);
    ne_blueprint();
    ne_ring(120, 88, 6 + wv_bass() / 2, RGB(0x60, 0xff, 0x60));          /* the core */
    for (k = 0; k < 2; k++) WFR(110 + k * 14, 72, 6, 3, (nk_mask >> (k * 13)) & 1u || ((wv_ph / 6u) & 1u) ? RGB(0xff, 0x40, 0x40) : RGB(0x60, 0x10, 0x10));   /* the eyes */
    if (tx >= 0) { int32_t sx = 20 + tx * 7; ne_ring(sx, 210, 7, RGB(0x30, 0xc0, 0xf0)), wvf_pline(sx, 200, sx, 220, RGB(0x30, 0xc0, 0xf0)); }   /* a sight on the note */
    if (wvf_hit_n > 1) { WFR(176, 6, 58, 14, RGB(0xff, 0x30, 0x30)), wf_textc(205, 9, 1, "ALERT", WC_WHT); }
}
/* the dancing dog's clip: still while nothing plays (it plays when the music does), its size following the notes */
static int32_t dg_sz = 220;
static UI_NOINLINE void dog_paint(uint32_t clip, int32_t y0, int32_t hh)
{
    const vp_clip_t *c;
    int32_t y, x, sz = dg_sz;
    if (clip >= (uint32_t)VP_N) return;
    c = &VP_CLIPS[clip];
    for (y = y0; y < y0 + hh; y++) {
        uint16_t *row = cv_px + (uint32_t)(y - y0) * 240u;
        int32_t v = c->h / 2 + (y - 130) * c->h / sz;
        if ((uint32_t)v >= c->h) continue;
        for (x = 0; x < 240; x++) { int32_t u = c->w / 2 + (x - 120) * c->w / sz; if ((uint32_t)u < c->w) row[x] = swap16(c->pal[VP_AT((uint32_t)(v * c->w + u))]); }
    }
}
/* ---- v3 */
/* FLOW's effect: while a performance effect (or a throw) is on, the FX block glows and blinks and a new block hangs off it, showing what the
 * effect does: echoes for the echo, shrinking for the fade, a blink at the loop's length, a turning arrow backwards for reverse and rewind,
 * bars with a band cut for the filters and the kills, steps for crush, chops for the gate, a sway for the wobbles, ice for freeze */
static UI_NOINLINE void fl_fx(void)
{
    int32_t fx = punch.cur, t = (int32_t)wv_ph, i, x = 150, y = 196, w = 70, h = 26;
    const char *nm = fx >= 0 ? PUNCH_NAME[fx] : (thr_req & 2u) ? "ECHO THROW" : (thr_req & 1u) ? "REV THROW" : 0;
    uint16_t c = RGB(0xff, 0x90, 0x30);
    if (!nm) return;
    if ((t / 3) & 1) WFR(67, 73, 33, 17, RGB(0x80, 0x40, 0x10)), WFR(121, 141, 31, 17, RGB(0x80, 0x40, 0x10));   /* the FX blocks blink */
    ne_box(66, 72, 34, 18, "FX", c), ne_box(120, 140, 32, 18, "FX", c);
    ne_line(136, 158, 136, 180, c), ne_line(136, 180, x + w / 2, 180, c), ne_line(x + w / 2, 180, x + w / 2, y, c);   /* the wire to it */
    for (i = 0; i < 5; i++) WFR(136 + ((t * 3 + i * 8) % 40 < 20 ? 0 : 0), 158 + (t * 2 + i * 6) % 22, 2, 2, WC_WHT);
    if (fx < 0 ? (thr_req & 2u) != 0u : fx == PX_ECHO) {               /* echoes moving out, fainter */
        for (i = 3; i >= 1; i--) { int32_t o = i * 10 + (t % 10); ne_box(x + o, y - o / 3, w, h, 0, i == 1 ? RGB(0xb0, 0x60, 0x20) : i == 2 ? RGB(0x70, 0x40, 0x18) : RGB(0x40, 0x24, 0x10)); }
    } else if (fx < 0) {                                               /* the reverb throw: rings opening */
        for (i = 0; i < 3; i++) ne_ring(x + w / 2, y + h / 2, 8 + ((t * 2 + i * 12) % 36), RGB(0x60, 0xc0, 0xff));
    } else if (fx == PX_FADE) {                                        /* fading: it shrinks and dims, again and again */
        int32_t k = (t % 30), ww = w * (30 - k) / 30, hh2 = h * (30 - k) / 30;
        x += (w - ww) / 2, y += (h - hh2) / 2, w = ww, h = hh2 > 6 ? hh2 : 6, c = RGB(0xff - k * 6, 0x90 - k * 4, 0x30);
    } else if (fx <= PX_LOOP32 || fx == PX_STUT || fx == PX_LOOP8T) { if ((t / (fx == PX_STUT ? 1 : 2 + fx)) & 1) c = WC_WHT; }
    else if (fx == PX_REV || fx == PX_RWD) { uint32_t an = (uint32_t)(1000 - t * 12); ne_line(x + w / 2, y + h / 2, x + w / 2 + 10 * wv_sin4(an + 64u) / 127, y + h / 2 + 10 * wv_sin4(an) / 127, WC_WHT); }
    else if (fx == PX_FREEZE) c = RGB(0xa0, 0xf0, 0xff);
    else if (fx == PX_OCT) y -= (t / 2) & 1 ? 8 : 0;
    else if (fx == PX_WOBBLE || fx == PX_FLANGE || fx == PX_PAN) x += WV_SIN[((uint32_t)t * 6u) & 63u] * 12 / 127;
    else if (fx == PX_GATE) { if ((t / 2) & 1) return; }
    ne_box(x, y, w, h, 0, c);
    if (fx == PX_CRUSH || fx == PX_DOWN) for (i = 0; i < 8; i++) WFR(x + 4 + i * 8, y + h - 4 - (i % 4) * 4, 8, 2, c);
    else if ((fx >= PX_LPF && fx <= PX_TEL) || (fx >= PX_KLOW && fx <= PX_KHIGH)) {
        int32_t cut = fx == PX_KLOW || fx == PX_HPF ? 0 : fx == PX_KMID || fx == PX_TEL ? 1 : 2;
        for (i = 0; i < 9; i++) { int32_t band = i * 3 / 9, hh2 = band == cut ? 2 : 4 + (int32_t)sp_s[i * 2] / 2; WFR(x + 6 + i * 7, y + h - 3 - clamp(hh2, 2, h - 6), 4, clamp(hh2, 2, h - 6), band == cut ? RGB(0x40, 0x20, 0x10) : c); }
    } else if (fx == PX_FREEZE) { ne_line(x + w / 2 - 6, y + h / 2, x + w / 2 + 6, y + h / 2, c), ne_line(x + w / 2, y + 6, x + w / 2, y + h - 6, c), ne_line(x + w / 2 - 4, y + 9, x + w / 2 + 4, y + h - 9, c); }
    wf_textc(x + w / 2, y - 10, 0, nm, c);
}
static UI_NOINLINE void v3_flow(void) { ne_flow(); fl_fx(); }
/* TARGET in 3D: the hexagons turn round their upright, the box and the ball inside them; the ring is the spectrum as bars that flash white
 * when a note of their range sounds */
static int32_t tg_yaw;
static UI_NOINLINE void tg_proj(int32_t x, int32_t y, int32_t *ox, int32_t *oy)
{
    int32_t c = wv_sin4((uint32_t)tg_yaw + 64u), s2 = wv_sin4((uint32_t)tg_yaw), X = x * c / 127, Z = x * s2 / 127;
    *ox = 120 + X * 220 / (220 + Z / 2), *oy = 124 + y * 220 / (220 + Z / 2);
}
static UI_NOINLINE void tg_hex(int32_t r, uint16_t col, int dash, int spec)
{
    int32_t k, j;
    for (k = 0; k < 6; k++) {
        uint32_t a0 = (uint32_t)k * 256u / 6u + 21u, a1 = (uint32_t)(k + 1) * 256u / 6u + 21u;
        int32_t x0 = r * wv_sin4(a0 + 64u) / 127, y0 = r * wv_sin4(a0) / 127, x1 = r * wv_sin4(a1 + 64u) / 127, y1 = r * wv_sin4(a1) / 127;
        for (j = 0; j < 12; j++) {
            int32_t ax, ay, bx, by, px = x0 + (x1 - x0) * j / 12, py = y0 + (y1 - y0) * j / 12, qx = x0 + (x1 - x0) * (j + 1) / 12, qy = y0 + (y1 - y0) * (j + 1) / 12;
            if (dash && (j & 1)) continue;
            tg_proj(px, py, &ax, &ay), tg_proj(qx, qy, &bx, &by);
            ne_line(ax, ay, bx, by, col);
            if (spec) { int32_t l = 100 + (int32_t)sp_s[(uint32_t)(k * 12 + j) % 19u] * 3, ex, ey; tg_proj(px * l / 100, py * l / 100, &ex, &ey); wvf_pline(ax, ay, ex, ey, col); }
        }
    }
}
static UI_NOINLINE void v3_target(void)
{
    static const int8_t V[8][3] = {{-1, -1, -1}, {1, -1, -1}, {1, 1, -1}, {-1, 1, -1}, {-1, -1, 1}, {1, -1, 1}, {1, 1, 1}, {-1, 1, 1}};
    static const uint8_t E[12][2] = {{0, 1}, {1, 2}, {2, 3}, {3, 0}, {4, 5}, {5, 6}, {6, 7}, {7, 4}, {0, 4}, {1, 5}, {2, 6}, {3, 7}};
    int32_t i, px[8], py[8], b;
    uint32_t a = wv_ph * 2u;
    char t[20];
    tg_yaw += 1 + clamp(song.g[G_BPM], 40, 240) / 80;
    WFR(0, 0, 240, 240, RGB(0x1c, 0x10, 0x18));
    tg_hex(102, RGB(0x40, 0x40, 0xd0), 0, 0), tg_hex(96, RGB(0xf0, 0xc8, 0x30), 1, 0), tg_hex(86, RGB(0xff, 0x30, 0x20), 0, 1);
    for (i = 0; i < 8; i++) {                                          /* the box, turning its own way */
        int32_t c = wv_sin4(a + 64u), s2 = wv_sin4(a), x = V[i][0] * c - V[i][2] * s2, z = V[i][0] * s2 + V[i][2] * c, y = V[i][1] * 127, y2 = (y * 110 - z * 60) / 127, z2 = (y * 60 + z * 110) / 127;
        px[i] = 120 + x * 48 / (127 + z2 / 5), py[i] = 124 + y2 * 48 / (127 + z2 / 5);
    }
    for (i = 0; i < 12; i++) wvf_pline(px[E[i][0]], py[E[i][0]], px[E[i][1]], py[E[i][1]], RGB(0xe0, 0x40, 0xd0));
    for (b = 0; b < 38; b++) {                                         /* the ring: bars of the spectrum, white when their keys sound */
        uint32_t an = (uint32_t)b * 256u / 38u;
        int32_t band = b % 19, k0 = band * 27 / 19, k1 = (band + 1) * 27 / 19, lit = (int32_t)((nk_mask >> k0) & ((1u << (k1 - k0 + 1)) - 1u)) != 0, l = 24 + (int32_t)sp_s[band] * 2 / 3, x0, y0, x1, y1;
        tg_proj(24 * wv_sin4(an + 64u) / 127, 24 * wv_sin4(an) / 127, &x0, &y0), tg_proj(l * wv_sin4(an + 64u) / 127, l * wv_sin4(an) / 127, &x1, &y1);
        ne_line(x0, y0, x1, y1, lit ? WC_WHT : RGB(0x40, 0x50, 0xf0));
    }
    for (i = 0; i < 3; i++) { uint32_t an = a + (uint32_t)i * 85u; int32_t ex, ey; tg_proj(30 * wv_sin4(an + 64u) / 127, 30 * wv_sin4(an) / 127, &ex, &ey); ne_line3(120, 124, ex, ey, RGB(0xff, 0x30, 0x20)); }
    wf_textc(120, 8, 1, "I GLOBAL TARGET I", RGB(0xf0, 0xc8, 0x30));
    str_cpy(t, "X ", 20), fmt_int(t + 2, song.g[G_BPM]), str_cpy(t + str_len(t), ".20272", 20 - str_len(t)), wf_text(70, 226, 1, t, RGB(0xf0, 0xc8, 0x30));
}
/* BLUEPRINT as a fight: each part of the robot is a range of keys; a note hits it (it flashes white, a number flies off in the key's colour),
 * many hits turn it orange then red; two seconds without hits and it mends, back to green. The parts swell on the beat. With an effect on,
 * the blue sight moves on the beat to a part and fires at it */
static uint8_t bp_dmg[9], bp_fl[9], bp_quiet[9];
static int16_t bp_num[6][4];                   /* x, y, life, key */
static int32_t bp_sx = 56, bp_sy = 72, bp_tgt;
static UI_NOINLINE void v3_blueprint(void)
{
    int32_t i, part = 0, k, cx[9] = {0}, cy[9] = {0}, cn[9] = {0}, pulse = wvf_hit_n > 2 ? 4 : 0;
    char b[8];
    for (k = 0; k < 27; k++) if ((nk_on >> k) & 1u) {                  /* a hit */
        int32_t p = k * 9 / 27;
        if (bp_dmg[p] < 9) bp_dmg[p]++;
        bp_fl[p] = 4, bp_quiet[p] = 0;
        for (i = 0; i < 6; i++) if (!bp_num[i][2]) { bp_num[i][2] = 16, bp_num[i][3] = (int16_t)k, bp_num[i][0] = (int16_t)p; break; }
    }
    for (i = 0; i < 9; i++) { if (bp_fl[i]) bp_fl[i]--; if (++bp_quiet[i] > 20 && bp_dmg[i] && !(wv_ph % 2u)) bp_dmg[i]--;   /* (a second untouched, then it mends fast) */ if (bp_quiet[i] > 200) bp_quiet[i] = 200; }
    WFR(0, 0, 240, 240, RGB(0x04, 0x08, 0x10));
    for (i = 0; i < 12; i++) WFR(i * 20, 0, 1, 240, RGB(0x18, 0x34, 0x48)), WFR(0, i * 20, 240, 1, RGB(0x18, 0x34, 0x48));
    WFR(0, (int32_t)((wv_ph * 4u) % 240u), 240, 1, RGB(0x30, 0x90, 0xb0));
    for (i = 0; i + 1 < (int32_t)(sizeof BP_HALF / sizeof BP_HALF[0]); i += 2) { if (BP_HALF[i] < 0) { part++; continue; } cx[part % 9] += BP_HALF[i] - 120, cy[part % 9] += BP_HALF[i + 1], cn[part % 9]++; }
    for (i = 0; i < 9; i++) if (cn[i]) cx[i] /= cn[i], cy[i] /= cn[i];
    part = 0;
    for (i = 0; i + 3 < (int32_t)(sizeof BP_HALF / sizeof BP_HALF[0]); i += 2) {
        int32_t p, d, sc;
        uint16_t c;
        if (BP_HALF[i] < 0) { part++; continue; }
        if (BP_HALF[i + 2] < 0) continue;
        p = part % 9, d = bp_dmg[p], sc = 100 + pulse + (int32_t)sp_s[BP_BAND[p]] / 3;
        c = bp_fl[p] ? WC_WHT : d >= 6 ? RGB(0xff, 0x30, 0x30) : d >= 3 ? RGB(0xff, 0x90, 0x20) : RGB(0x40, 0xe0, 0x20);
#define BX(v) (120 + cx[p] + ((v) - 120 - cx[p]) * sc / 100)
#define BY2(v) (cy[p] + ((v) - cy[p]) * sc / 100)
        ne_line(BX(BP_HALF[i]), BY2(BP_HALF[i + 1]), BX(BP_HALF[i + 2]), BY2(BP_HALF[i + 3]), c);
        ne_line(240 - BX(BP_HALF[i]), BY2(BP_HALF[i + 1]), 240 - BX(BP_HALF[i + 2]), BY2(BP_HALF[i + 3]), c);
#undef BX
#undef BY2
    }
    if (punch.cur >= 0 || thr_req) {                                   /* an effect on: the sight hunts on the beat and fires */
        if (wvf_hit_n == 5 || !(wv_ph % 10u)) bp_tgt = (int32_t)(pc_h(wv_ph) % 9u);
        bp_sx += (120 + cx[bp_tgt] - bp_sx) / 3, bp_sy += (cy[bp_tgt] - bp_sy) / 3;
        if ((wv_ph % 10u) < 3u) { wvf_pline(bp_sx, bp_sy, 120 + cx[bp_tgt], cy[bp_tgt], RGB(0x80, 0xe0, 0xff)); if ((wv_ph % 10u) == 2u && bp_dmg[bp_tgt] < 9) bp_dmg[bp_tgt]++, bp_fl[bp_tgt] = 3, bp_quiet[bp_tgt] = 0; }
    } else bp_sx += (56 - bp_sx) / 6, bp_sy += (72 - bp_sy) / 6;
    ne_ring(bp_sx, bp_sy, 20, RGB(0x30, 0xc0, 0xf0)), ne_ring(bp_sx, bp_sy, 10 + wv_bass() / 2, RGB(0x30, 0xc0, 0xf0));
    wvf_pline(bp_sx - 28, bp_sy, bp_sx + 28, bp_sy, RGB(0x30, 0xc0, 0xf0)), wvf_pline(bp_sx, bp_sy - 28, bp_sx, bp_sy + 28, RGB(0x30, 0xc0, 0xf0));
    for (i = 0; i < 6; i++) if (bp_num[i][2]) {                        /* the numbers flying off */
        int32_t p = bp_num[i][0], kk = bp_num[i][3];
        fmt_int(b, 3 + kk % 7 + (int32_t)bp_dmg[p] * 2);
        wf_textc(120 + cx[p] + (kk & 1 ? 14 : -14), cy[p] - (16 - bp_num[i][2]) * 2, 1, b, sh_note_col(kk));
        bp_num[i][2]--;
    }
    { int32_t hp = 0; for (i = 0; i < 9; i++) hp += 9 - bp_dmg[i]; WFR(8, 226, 82, 8, RGB(0x10, 0x20, 0x10)), WFR(9, 227, hp, 6, hp > 50 ? RGB(0x40, 0xe0, 0x20) : hp > 25 ? RGB(0xff, 0x90, 0x20) : RGB(0xff, 0x30, 0x30)); wf_text(94, 226, 0, "HP", WC_LAB); }
    for (i = 0; i < 9; i++) if (bp_dmg[i] >= 6) { if ((wv_ph / 4u) & 1u) WFR(176, 6, 58, 14, RGB(0xff, 0x30, 0x30)), wf_textc(205, 9, 1, "ALERT", WC_WHT); break; }
}
/* HOUND, bigger: the neon dog. In a pause it lowers its head to the bowl and eats (the kibble goes down, it chews); with music it lifts it and
 * nods on the beat, its tail wags, the sound goes in at its mouth and out at its tail as notes. Inside it, each thing its own: gears that turn
 * with the tempo and spin up with the bass, rings that open with the bands, the four tracks as meters, the spectrum, a square wave */
static int32_t hd2_head, hd2_food = 8;
static UI_NOINLINE void hd_gear(int32_t cx, int32_t cy, int32_t r, uint32_t a, uint16_t c)
{
    uint32_t q;
    ne_ring(cx, cy, r, c), ne_ring(cx, cy, r / 3, c);
    for (q = 0; q < 8u; q++) { uint32_t an = a + q * 32u; ne_line(cx + r * wv_sin4(an + 64u) / 127, cy + r * wv_sin4(an) / 127, cx + (r + 4) * wv_sin4(an + 64u) / 127, cy + (r + 4) * wv_sin4(an) / 127, c); }
}
static UI_NOINLINE void v3_hound(void)
{
    int32_t i, quiet = !nk_mask && ev_lvl < 12, nod = quiet ? 0 : (int32_t)((wv_ph * (uint32_t)clamp(song.g[G_BPM], 40, 240) / 600u) & 1u) * 5;
    int32_t hx, hy, tail = WV_SIN[(wv_ph * (quiet ? 2u : 9u)) & 63u] * (quiet ? 5 : 14) / 127, chew = (int32_t)((wv_ph / 3u) & 1u);
    static uint32_t gear;
    gear += 2u + (uint32_t)clamp(song.g[G_BPM], 40, 240) / 30u + (uint32_t)wv_bass();
    hd2_head += quiet ? (hd2_head < 40 ? 3 : 0) : (hd2_head > 0 ? -5 : 0);
    if (hd2_head < 0) hd2_head = 0;
    if (quiet && hd2_head >= 40 && !(wv_ph % 14u) && hd2_food > 0) hd2_food--;   /* eating */
    if (!quiet && hd2_food < 8 && !(wv_ph % 30u)) hd2_food++;          /* (the bowl fills up again while it plays) */
    hx = 64 - hd2_head / 3, hy = 62 + hd2_head * 2 + nod;
    ne_line(72, 80, 196, 80, NE_BRN), ne_line(196, 80, 210, 94, NE_BRN), ne_line(210, 94, 210, 150, NE_BRN), ne_line(210, 150, 196, 164, NE_BRN);   /* the body */
    ne_line(196, 164, 72, 164, NE_BRN), ne_line(72, 164, 60, 150, NE_BRN), ne_line(60, 150, 60, 94, NE_BRN), ne_line(60, 94, 72, 80, NE_BRN);
    for (i = 0; i < 4; i++) { static const int16_t LX[4] = {76, 102, 168, 194}; int32_t st = !quiet && ((wv_ph / 4u + (uint32_t)i) & 1u) ? 3 : 0; ne_line(LX[i], 164, LX[i] + st, 206, NE_BRN), ne_line(LX[i] + st, 206, LX[i] + st - 8, 206, NE_BRN); }
    ne_line(206, 86, 224 + tail, 58 - (tail < 0 ? -tail : tail) / 2, NE_BRN);
    ne_line(70, 86, hx + 6, hy + 24, NE_BRN), ne_line(66, 104, hx + 8, hy + 32, NE_BRN);   /* the neck */
    ne_line(hx, hy, hx - 30, hy + 5, NE_BRN), ne_line(hx - 30, hy + 5, hx - 40, hy + 18, NE_BRN), ne_line(hx - 40, hy + 18, hx - 30, hy + 26 + (quiet ? chew * 4 : 0), NE_BRN);
    ne_line(hx - 30, hy + 26 + (quiet ? chew * 4 : 0), hx + 3, hy + 30, NE_BRN), ne_line(hx + 3, hy + 30, hx + 10, hy + 12, NE_BRN), ne_line(hx + 10, hy + 12, hx, hy, NE_BRN);
    ne_line(hx - 2, hy + 2, hx + 5, hy + 28 + nod / 2, NE_PNK), ne_line(hx + 5, hy + 28 + nod / 2, hx + 13, hy + 22, NE_PNK);
    WFR(hx - 18, hy + 10, 4, 4, quiet ? NE_DIM : NE_BLU), WFR(hx - 42, hy + 15, 4, 4, NE_RED);
    ne_line(10, 212, 58, 212, NE_GRN), ne_line(10, 212, 16, 222, NE_GRN), ne_line(58, 212, 52, 222, NE_GRN), ne_line(16, 222, 52, 222, NE_GRN);   /* the bowl */
    for (i = 0; i < hd2_food; i++) WFR(16 + i * 5, 207 - (i & 1) * 3, 4, 4, NE_ORG);
    if (quiet && hd2_head >= 40 && chew) for (i = 0; i < 3; i++) WFR(hx - 36 + i * 6, hy + 34 - i * 3, 2, 2, NE_ORG);   /* crumbs */
    ne_line(0, 224, 240, 224, NE_GRN);
    if (!quiet) for (i = 0; i < 8; i++) { int32_t x = 2 + ((int32_t)(wv_ph * 3u) + i * 9) % 60; if (x < hx - 42) WFR(x, hy + 18 + WV_SIN[((uint32_t)x * 4u) & 63u] / 40, 3, 2, NE_BLU); }
    if (!quiet) for (i = 0; i < 5; i++) { int32_t d = (int32_t)((wv_ph * 2u + (uint32_t)i * 13u) % 50u), x = 222 + d / 2, y = 54 - d + WV_SIN[((uint32_t)d * 6u + (uint32_t)i * 11u) & 63u] / 30; WFR(x, y, 3, 3, i & 1 ? NE_PNK : NE_GRN), WFR(x + 2, y - 6, 1, 6, i & 1 ? NE_PNK : NE_GRN); }
    {                                                                  /* inside, side by side: no one over another. Only the part that answers lights white:
                                                                        * low notes the big gear, middle ones the scope, high ones the dial and the small gear,
                                                                        * the loop's beat the track bars, an effect the right-hand gears */
        static int32_t fl[5], say;
        static uint32_t lb;
        int32_t k;
        uint32_t bt = wv_ph * (uint32_t)clamp(song.g[G_BPM], 40, 240) / 600u;
        uint16_t c1, c2, c3, c4, c5;
        for (k = 0; k < 5; k++) if (fl[k]) fl[k]--;
        if (nk_on & 0x1FFu) fl[0] = 4;
        if (nk_on & 0x3FE00u) fl[1] = 4;
        if (nk_on & 0x7FC0000u) fl[2] = 4;
        if (song.playing && bt != lb) fl[3] = 3;
        lb = bt;
        if (punch.cur >= 0 || thr_req) fl[4] = (wv_ph & 2u) ? 2 : fl[4];
        c1 = fl[0] ? WC_WHT : NE_ORG, c2 = fl[2] ? WC_WHT : NE_PNK, c3 = fl[1] ? WC_WHT : NE_BLU, c4 = fl[3] ? WC_WHT : NE_PUR, c5 = fl[4] ? WC_WHT : NE_GRN;
        hd_gear(84, 102, 11, gear, c1), hd_gear(102, 118, 7, (uint32_t)(-(int32_t)gear * 3 / 2), c2);
        for (k = 0; k < 3; k++) ne_ring(82, 144, 3 + (int32_t)sp_s[k * 6] / 2 + k * 2, k == 0 ? c3 : k == 1 ? c4 : c5);
        for (k = 0; k < 4; k++) { int32_t h = clamp((int32_t)trk_level((uint32_t)k) * 24 / 127 * (trk[k].p[P_MUTE] ? 0 : 1) * ((int32_t)sp_s[k * 4] + 4) / 20, 1, 24); ne_box(116 + k * 8, 156 - h, 5, h, 0, k == 3 ? NE_RED : c4); }
        for (k = 0; k < 8; k++) { int32_t h = clamp((int32_t)sp_s[k * 2 + 1], 1, 24); WFR(156 + k * 4, 156 - h, 2, h, k & 1 ? NE_RED : NE_GRN); }
        { int32_t px = 118, py = 100, x; for (x = 0; x < 34; x++) { int32_t y = 100 + scope_buf[(scope_w - 68u + 2u * (uint32_t)x) & (SCOPE_N - 1u)] * 8 / 12000; if (x) wvf_pline(px, py, 118 + x, y, c3); px = 118 + x, py = y; } }   /* a scope */
        for (k = 0; k < 3; k++) { int32_t xs = 160 + k * 9, hh = 3 + wv_bass(); ne_line(xs, 112, xs, 112 - hh, NE_ORG), ne_line(xs, 112 - hh, xs + 4, 112 - hh, NE_ORG), ne_line(xs + 4, 112 - hh, xs + 4, 112, NE_ORG); }
        hd_gear(192, 104, 6, gear * 2u, c5), hd_gear(194, 140, 5, (uint32_t)(-(int32_t)gear * 2), c5);
        for (k = 0; k < 5; k++) WFR(146 + k * 6, 92, 4, 4, (nk_mask >> (k * 5)) & 1u ? WC_WHT : (k & 1 ? NE_PNK : NE_BLU));   /* little lamps: the keys */
        { uint32_t an = gear / 2u; ne_line(138, 132, 138 + 6 * wv_sin4(an + 64u) / 127, 132 + 6 * wv_sin4(an) / 127, c2), ne_ring(138, 132, 7, c2); }   /* a dial */
        if (nk_on & 0x7E00000u) say = 30;                              /* the high notes: someone says it */
        if (say) { say--; ne_box(16, 40, 112, 16, 0, NE_PNK); wf_text(20, 44, 1, "WHO'S A GOOD BOY?", WC_WHT); }
    }
    wf_text(10, 6, 1, "FREQ", NE_BLU), ne_gauge(24, 34, ne_frac(song.g[G_FILT], GP[G_FILT].min, GP[G_FILT].max), NE_BLU);
    wf_text(200, 6, 1, "BPM", NE_PUR), ne_num(190, 18, 20, song.g[G_BPM], NE_GRN);
}
/* BOXER in outlines only (as the line pictures): the ring, the crowd, the bag and the boxer drawn as their contours, in their colours */
static UI_NOINLINE void spr_outline(const char *const *rows, int32_t nr, int32_t x, int32_t y, int32_t z, const char *keys, const uint16_t *cols)   /* the contour of each colour, 2 px */
{
    int32_t r, c;
    for (r = 0; r < nr; r++)
        for (c = 0; rows[r][c]; c++) {
            char ch = rows[r][c];
            const char *k = keys;
            int32_t i = 0, X = x + c * z, Y = y + r * z;
            while (*k && *k != ch) k++, i++;
            if (!*k) continue;
            if (r == 0 || rows[r - 1][c] != ch) WFR(X, Y, z, 2, cols[i]);
            if (r == nr - 1 || rows[r + 1][c] != ch) WFR(X, Y + z - 2, z, 2, cols[i]);
            if (c == 0 || rows[r][c - 1] != ch) WFR(X, Y, 2, z, cols[i]);
            if (!rows[r][c + 1] || rows[r][c + 1] != ch) WFR(X + z - 2, Y, 2, z, cols[i]);
        }
}
static UI_NOINLINE void v3_boxer(void)
{
    static const uint16_t BC[8] = {RGB(0x80, 0x50, 0x30), RGB(0xe8, 0xa8, 0x7c), RGB(0xc0, 0x84, 0x60), RGB(0xf0, 0xf0, 0xf0), RGB(0xff, 0x40, 0x40), RGB(0x50, 0x80, 0xff), RGB(0xf8, 0xf8, 0xf8), RGB(0x90, 0x90, 0xa8)};
    int32_t i, bob = (int32_t)((wv_ph * (uint32_t)clamp(song.g[G_BPM], 40, 240) / 600u) & 1u) * 3, sw;
    char b[8];
    WFR(0, 0, 240, 240, RGB(0x0c, 0x0a, 0x12));
    for (i = 0; i < 60; i++) { int32_t x = (int32_t)(pc_h((uint32_t)i * 7u) % 240u), y = 10 + (int32_t)(pc_h((uint32_t)i * 13u) % 70u); ne_ring(x, y, 4, sp_s[i % 19] > 12 && (pc_h((uint32_t)i + wv_ph / 2u) & 7u) == 0u ? WC_WHT : RGB(0x40, 0x3c, 0x50)); }
    ne_line(0, 150, 240, 150, RGB(0xa0, 0xa0, 0xb0));
    for (i = 0; i < 3; i++) wvf_pline(0, 96 + i * 18, 240, 96 + i * 18, i == 0 ? RGB(0xff, 0x40, 0x40) : i == 1 ? RGB(0xf0, 0xf0, 0xf0) : RGB(0x50, 0x80, 0xff));
    ne_box(10, 90, 8, 64, 0, RGB(0xff, 0x40, 0x40)), ne_box(222, 90, 8, 64, 0, RGB(0x50, 0x80, 0xff));
    for (i = 0; i < 6; i++) wvf_pline(0, 170 + i * 12, 240, 170 + i * 12, RGB(0x30, 0x30, 0x40));
    if (nk_low(nk_on)) bx_t = 1; else if (nk_high(nk_on)) bx_t = 101; else if (wvf_hit_n == 5 && !bx_t) bx_t = 1;
    if (bx_t == 4 || bx_t == 104) bx_bag = 14;
    sw = bx_bag * WV_SIN[(wv_ph * 6u) & 63u] / 127;
    if (bx_bag) bx_bag--;
    wvf_pline(182, 0, 182 + sw / 3, 70, RGB(0x80, 0x80, 0x80));
    ne_box(168 + sw, 70, 28, 64, 0, RGB(0xd0, 0x40, 0x30)), ne_line(168 + sw, 84, 196 + sw, 84, RGB(0xd0, 0x40, 0x30));
    {
        int32_t hit = (bx_t > 0 && bx_t < 8) || (bx_t > 100 && bx_t < 108);
        spr_outline(hit ? BX_J : BX_G, 30, 54 + (hit ? 10 : 0), 84 + bob, 4, "HSsKGBwN", BC);
        if (bx_t) { bx_t++; if (bx_t == 9 || bx_t == 109) bx_t = 0; }
        if (hit && (bx_t == 3 || bx_t == 103)) for (i = 0; i < 6; i++) { uint32_t an = (uint32_t)i * 43u; ne_line(166 + sw, 98, 166 + sw + 10 * wv_sin4(an + 64u) / 127, 98 + 10 * wv_sin4(an) / 127, RGB(0xff, 0xe0, 0x60)); }
    }
    wf_text(10, 218, 1, "ROUND", RGB(0xf0, 0xc0, 0x30)), fmt_int(b, (int32_t)(wf_secs() / 30u % 12u) + 1), wf_text(52, 218, 1, b, WC_WHT);
    {                                                                  /* POWER: how many keys at once (three fill it), green to red */
        static int32_t pw;
        int32_t n = 0; uint32_t m;
        for (m = nk_mask; m; m >>= 1) n += (int32_t)(m & 1u);
        pw += (clamp(n * 34, 0, 100) - pw) / (n * 34 > pw ? 2 : 6);
        wf_text(150, 218, 1, "POWER", RGB(0xf0, 0xc0, 0x30));
        for (i = 0; i < 10; i++) if (i * 10 < pw) WFR(192 + i * 4, 226 - i, 3, 4 + i, i < 4 ? RGB(0x60, 0xf0, 0x60) : i < 7 ? RGB(0xff, 0xd0, 0x40) : RGB(0xff, 0x40, 0x30)); else ne_line(192 + i * 4, 230, 194 + i * 4, 230, RGB(0x50, 0x40, 0x40));
    }
}
/* PATCH: a modular in lines (after the patched one): modules, jacks, cables. Each effect has its module and its cable: the DJ filter,
 * the delay (its send and feedback), the reverb, the drive, and the performance effect. The cable's sag and swing are the effect's amount, it
 * glows while it is on; the performance effect's cable pulses at its pace; the LEDs blink with it */
static UI_NOINLINE void pt_cable(int32_t x0, int32_t y0, int32_t x1, int32_t y1, int32_t sag, int32_t sway, uint16_t c)
{
    int32_t i, px = x0, py = y0;
    for (i = 1; i <= 16; i++) {
        int32_t t = i * 64 / 16, x = x0 + (x1 - x0) * i / 16 + sway * WV_SIN[(uint32_t)t & 63u] / 127, y = y0 + (y1 - y0) * i / 16 + sag * WV_SIN[(uint32_t)(i * 2) & 63u] / 127;
        ne_line(px, py, x, y, c); px = x, py = y;
    }
    ne_ring(x0, y0, 3, c), ne_ring(x1, y1, 3, c);
}
static UI_NOINLINE void v3_patch(void)
{
    static const char *const MN[6] = {"FILTER", "DELAY", "FDBK", "REVERB", "DRIVE", "FX"};
    static const uint16_t MC[6] = {RGB(0x60, 0xd0, 0xff), RGB(0xff, 0xc8, 0x40), RGB(0xff, 0x90, 0x30), RGB(0xb0, 0x80, 0xff), RGB(0xff, 0x50, 0x50), RGB(0x60, 0xff, 0x90)};
    int32_t v[6], i, k;
    uint32_t t = wv_ph;
    v[0] = ne_frac(song.g[G_FILT], GP[G_FILT].min, GP[G_FILT].max), v[1] = clamp(TSEL->p[P_DLY] * 8, 0, 1024), v[2] = ne_frac(song.g[G_DFDBK], GP[G_DFDBK].min, GP[G_DFDBK].max);
    v[3] = clamp(TSEL->p[P_REV] * 8, 0, 1024), v[4] = clamp(TSEL->p[P_DIST] * 8, 0, 1024), v[5] = punch.cur >= 0 || thr_req ? 1024 : 0;
    WFR(0, 0, 240, 240, RGB(0x10, 0x0c, 0x14));
    ne_box(8, 10, 224, 74, 0, RGB(0xff, 0x60, 0x40)), ne_box(8, 92, 224, 82, 0, RGB(0x40, 0xc0, 0xc0));   /* the two rows of modules */
    for (i = 0; i < 6; i++) {
        int32_t mx = 14 + i * 37, my = 16, on = v[i] > 40, k2;
        uint32_t a = 96u + (uint32_t)v[i] * 192u / 1024u;
        ne_box(mx, my, 32, 62, 0, on ? MC[i] : RGB(0x50, 0x40, 0x48));
        wf_textc(mx + 16, my + 3, 0, MN[i], on ? MC[i] : RGB(0x70, 0x60, 0x68));
        ne_ring(mx + 16, my + 26, 9, MC[i]), ne_line(mx + 16, my + 26, mx + 16 + 8 * wv_sin4(a + 64u) / 127, my + 26 + 8 * wv_sin4(a) / 127, WC_WHT);   /* its knob */
        for (k2 = 0; k2 < 3; k2++) WFR(mx + 7 + k2 * 8, my + 42, 4, 3, on && ((t / (uint32_t)(2 + i) + (uint32_t)k2) % 3u == 0u) ? MC[i] : RGB(0x30, 0x24, 0x2c));   /* its LEDs */
        ne_ring(mx + 16, my + 54, 4, RGB(0xc0, 0xb0, 0x90));          /* its jack */
    }
    for (i = 0; i < 12; i++) ne_ring(18 + i * 18, 108, 4, RGB(0xc0, 0xb0, 0x90)), ne_ring(18 + i * 18, 128, 6, sp_s[(uint32_t)i * 19u / 12u] > 10 ? RGB(0xff, 0x80, 0x90) : RGB(0x80, 0x40, 0x50));
    for (i = 0; i < 12; i++) WFR(14 + i * 18, 146, 9, 2 + (int32_t)sp_s[(uint32_t)i * 19u / 12u] / 2, RGB(0x40, 0xc0, 0xc0));
    for (i = 0; i < 6; i++) {                                          /* the cables: down to the voices' jacks, hanging and swinging with the amount */
        int32_t x0 = 30 + i * 37, x1 = 18 + ((i * 5 + 2) % 12) * 18, sag = 6 + v[i] / 40, sway = v[i] > 40 ? WV_SIN[(t * (uint32_t)(2 + i)) & 63u] * v[i] / 1024 / 6 : 0;
        uint16_t c = v[i] > 40 ? MC[i] : RGB(0x60, 0x50, 0x30);
        if (i == 5 && v[5] && ((t / 2u) & 1u)) c = WC_WHT;
        pt_cable(x0, 70, x1, 108, sag + (i == 5 && v[5] ? (int32_t)((t * 3u) % 12u) : 0), sway, c);
    }
    for (k = 0; k < 8; k++) ne_box(14 + k * 28, 186, 24, 24, 0, (nk_mask >> (k * 3)) & 7u ? RGB(0xff, 0xe0, 0x60) : (k & 1 ? RGB(0xff, 0x60, 0x40) : RGB(0x40, 0xc0, 0xc0)));   /* the pads: the keys */
    ne_ring(30, 222, 8, RGB(0x60, 0x60, 0x70)), ne_ring(210, 222, 8, RGB(0x60, 0x60, 0x70)), ne_ring(30, 222, 3 + wv_bass() / 3, WC_WHT), ne_ring(210, 222, 3 + wv_bass() / 3, WC_WHT);   /* speakers */
    if (punch.cur >= 0) wf_textc(120, 224, 1, PUNCH_NAME[punch.cur], RGB(0x60, 0xff, 0x90));
}
/* TARGET as a tesseract: the 4D cube turning in two planes at once (its inner cube passing through the outer), projected in lines; it swells
 * with the bass, turns faster with the tempo, its edges flash white with their notes; the ring of the spectrum round it, the title */
static int32_t ts_a, ts_b;
static UI_NOINLINE void v4_tesseract(void)
{
    int32_t px[16], py[16], i, j, b;
    char t[20];
    ts_a += 1 + clamp(song.g[G_BPM], 40, 240) / 60, ts_b += 1 + ev_rate / 64;
    WFR(0, 0, 240, 240, RGB(0x1c, 0x10, 0x18));
    for (i = 0; i < 16; i++) {
        int32_t x = (i & 1) ? 100 : -100, y = (i & 2) ? 100 : -100, z = (i & 4) ? 100 : -100, w = (i & 8) ? 100 : -100;
        int32_t ca = wv_sin4((uint32_t)ts_a + 64u), sa = wv_sin4((uint32_t)ts_a), cb = wv_sin4((uint32_t)ts_b + 64u), sb = wv_sin4((uint32_t)ts_b);
        int32_t x2 = (x * ca - w * sa) / 127, w2 = (x * sa + w * ca) / 127, y2 = (y * cb - z * sb) / 127, z2 = (y * sb + z * cb) / 127;   /* the XW and YZ turns */
        int32_t k4 = 300 * 127 / (300 - w2), x3 = x2 * k4 / 127, y3 = y2 * k4 / 127, z3 = z2 * k4 / 127, s = 60 + wv_bass();   /* 4D to 3D */
        int32_t x4 = (x3 * 120 - z3 * 40) / 127, z4 = (x3 * 40 + z3 * 120) / 127;   /* a slight turn to see it in depth */
        px[i] = 120 + x4 * s / (200 + z4 / 2), py[i] = 124 + y3 * s / (200 + z4 / 2);
    }
    for (i = 0; i < 16; i++) for (b = 0; b < 4; b++) {               /* an edge: two corners one bit apart */
        j = i ^ (1 << b);
        if (j < i) continue;
        { int32_t e = i * 4 + b, lit = (nk_mask >> (e % 27)) & 1u; ne_line(px[i], py[i], px[j], py[j], lit ? WC_WHT : b == 3 ? RGB(0xff, 0x30, 0x20) : (i & 8) ? RGB(0x40, 0x60, 0xf0) : RGB(0xe0, 0x40, 0xd0)); }
    }
    for (b = 0; b < 38; b++) {                                         /* the ring: bars of the spectrum, white when their keys sound */
        uint32_t an = (uint32_t)b * 256u / 38u + (uint32_t)ts_a / 2u;
        int32_t band = b % 19, k0 = band * 27 / 19, k1 = (band + 1) * 27 / 19, lit = (int32_t)((nk_mask >> k0) & ((1u << (k1 - k0 + 1)) - 1u)) != 0, l = 92 + (int32_t)sp_s[band] * 2 / 3;
        ne_line(120 + 92 * wv_sin4(an + 64u) / 127, 124 + 92 * wv_sin4(an) / 127, 120 + l * wv_sin4(an + 64u) / 127, 124 + l * wv_sin4(an) / 127, lit ? WC_WHT : RGB(0xf0, 0xc8, 0x30));
    }
    wf_textc(120, 8, 1, "I GLOBAL TARGET I", RGB(0xf0, 0xc8, 0x30));
    str_cpy(t, "X ", 20), fmt_int(t + 2, song.g[G_BPM]), str_cpy(t + str_len(t), ".20272", 20 - str_len(t)), wf_text(70, 226, 1, t, RGB(0xf0, 0xc8, 0x30));
}
/* PATCH, busier: two rows of modules whose knobs turn with their effects, many cables between them that sag with the amounts and shiver
 * when notes are played; below, two keyboards whose keys go down as the notes sound */
static UI_NOINLINE void v4_patch(void)
{
    static const uint16_t MC[5] = {RGB(0x60, 0xd0, 0xff), RGB(0xff, 0xc8, 0x40), RGB(0xb0, 0x80, 0xff), RGB(0xff, 0x50, 0x50), RGB(0x60, 0xff, 0x90)};
    static const uint8_t BL[12] = {0, 1, 0, 1, 0, 1, 0, 0, 1, 0, 1, 0};
    uint32_t g = (uint32_t)wf_grp % 3u;
    int32_t v[5], i, k, shake = nk_on ? 5 : ev_rate > 30 ? 2 : 0;
    uint32_t t = wv_ph;
    WFR(0, 0, 240, 240, RGB(0x10, 0x0c, 0x14));
    for (i = 0; i < 5; i++) {                                          /* the top row: KNOB 1..4 (the group they move now, named as they are), the FX */
        int32_t mx = 4 + i * 47, my = 6, on, hot = 0, k2;
        uint32_t a;
        const char *nm = "FX";
        if (i < 4) {
            int16_t *vp;
            const param_desc_t *d = wf_sl(g * 4u + (uint32_t)i, &vp);
            v[i] = d ? wf_frac(d, *vp) : 0, nm = WF_SL[g * 4u + (uint32_t)i].shrt, hot = wf_hot == (int32_t)(g * 4u + (uint32_t)i) && wf_saying();
        } else v[i] = punch.cur >= 0 || thr_req ? 1024 : 0, nm = punch.cur >= 0 ? PUNCH_NAME[punch.cur] : "FX";
        on = v[i] > 40 || hot;
        a = 96u + (uint32_t)v[i] * 192u / 1024u;                      /* (the knob points where yours is: 7 o'clock .. 5 o'clock) */
        ne_box(mx, my, 43, 66, 0, hot ? WC_WHT : on ? MC[i] : RGB(0x50, 0x40, 0x48));
        wf_clx0 = mx + 1, wf_clx1 = mx + 42;
        wf_textc(mx + 21, my + 3, 0, nm, hot ? WC_WHT : on ? MC[i] : RGB(0x70, 0x60, 0x68));
        wf_clx0 = 0, wf_clx1 = 240;
        ne_ring(mx + 21, my + 25, 11, hot ? WC_WHT : MC[i]), ne_line3(mx + 21, my + 25, mx + 21 + 10 * wv_sin4(a + 64u) / 127, my + 25 + 10 * wv_sin4(a) / 127, WC_WHT);
        for (k2 = 0; k2 < 3; k2++) WFR(mx + 10 + k2 * 9, my + 42, 4, 3, on && ((t / (uint32_t)(2 + i) + (uint32_t)k2) % 3u == 0u) ? MC[i] : RGB(0x30, 0x24, 0x2c));
        ne_ring(mx + 12, my + 56, 3, RGB(0xc0, 0xb0, 0x90)), ne_ring(mx + 30, my + 56, 3, RGB(0xc0, 0xb0, 0x90));
    }
    for (i = 0; i < 8; i++) {                                          /* the middle row: the voices, a knob each turned by its band */
        int32_t mx = 6 + i * 29, my = 84, f = (int32_t)sp_s[(uint32_t)i * 19u / 8u] * 40;
        uint32_t a = 96u + (uint32_t)clamp(f, 0, 1024) * 192u / 1024u;
        ne_box(mx, my, 26, 52, 0, RGB(0x40, 0xc0, 0xc0));
        ne_ring(mx + 13, my + 14, 7, RGB(0xff, 0x80, 0x90)), ne_line(mx + 13, my + 14, mx + 13 + 6 * wv_sin4(a + 64u) / 127, my + 14 + 6 * wv_sin4(a) / 127, WC_WHT);
        WFR(mx + 5, my + 30, 16, 3, sp_s[(uint32_t)i * 2u] > 9 ? RGB(0xff, 0xe0, 0x60) : RGB(0x30, 0x40, 0x40));
        ne_ring(mx + 7, my + 43, 3, RGB(0xc0, 0xb0, 0x90)), ne_ring(mx + 19, my + 43, 3, RGB(0xc0, 0xb0, 0x90));
    }
    for (i = 0; i < 12; i++) {                                         /* the cables: from the effects down to the voices and across, shivering with the notes */
        int32_t s0 = i % 5, x0 = 16 + s0 * 47 + (i >= 5 ? 18 : 0), x1 = 13 + ((i * 5 + 3) % 8) * 29 + (i & 1) * 12, amt = v[s0], sag = 8 + amt / 40 + (i % 3) * 4;
        int32_t sway = (amt > 40 ? WV_SIN[(t * (uint32_t)(2 + s0)) & 63u] * amt / 1024 / 6 : 0) + (shake ? (int32_t)(pc_h(t + (uint32_t)i) % (uint32_t)(shake * 2 + 1)) - shake : 0);
        uint16_t c = amt > 40 ? MC[s0] : i & 1 ? RGB(0xc8, 0xa0, 0x30) : RGB(0x50, 0x70, 0x70);
        if (s0 == 4 && v[4] && ((t / 2u) & 1u)) c = WC_WHT;
        pt_cable(x0, 62, x1, 127, sag, sway, c);
    }
    for (k = 0; k < 2; k++) {                                          /* two keyboards: the left and the right halves of the keys */
        int32_t kx = 6 + k * 118, ky = 150, wx = 0, n;
        ne_box(kx, ky, 110, 52, 0, RGB(0xff, 0x60, 0x40));
        for (n = k * 13; n < (k ? 27 : 13); n++) {
            int on = (nk_mask >> n) & 1u;
            if (BL[n % 12]) continue;
            ne_box(kx + 4 + wx * 13, ky + 6 + (on ? 3 : 0), 11, 40, 0, on ? RGB(0xff, 0xe0, 0x60) : RGB(0xe0, 0xe0, 0xe8)), wx++;
        }
        for (n = k * 13, wx = 0; n < (k ? 27 : 13); n++) { int on = (nk_mask >> n) & 1u; if (!BL[n % 12]) { wx++; continue; } WFR(kx + 4 + wx * 13 - 4, ky + 6 + (on ? 3 : 0), 7, 22, on ? RGB(0xff, 0x60, 0xc0) : RGB(0x60, 0x50, 0x70)); }
    }
    ne_ring(24, 222, 9, RGB(0x60, 0x60, 0x70)), ne_ring(216, 222, 9, RGB(0x60, 0x60, 0x70)), ne_ring(24, 222, 3 + wv_bass() / 3, WC_WHT), ne_ring(216, 222, 3 + wv_bass() / 3, WC_WHT);
    if (punch.cur >= 0) wf_textc(120, 220, 1, PUNCH_NAME[punch.cur], RGB(0x60, 0xff, 0x90));
}
/* DOOMSCROLL's lettering: the word drawn in dots, each dot lit by the level of its note, in the visualizer's colours */
static UI_NOINLINE void dm_word(int32_t x, int32_t y)
{
    const char *s = "DOOMSCROLL";
    int32_t ci = 0;
    for (; *s; s++, ci++) {
        uint32_t c = (uint8_t)*s, w = WFM_W[c - 32], r, b;
        for (r = 0; r < WFM_H; r++)
            for (b = 0; b < w; b++)
                if ((WFM_ROWS[c - 32][r] >> (7 - b)) & 1u) {
                    int32_t band = (ci * 2 + (int32_t)b / 3) % 19, lv = (int32_t)sp_s[band], on = lv > (int32_t)(WFM_H - r) * 2 || ((nk_mask >> (ci * 2 + 1)) & 1u);
                    uint16_t col = !on ? RGB(0x50, 0x50, 0x80) : r < 2 ? RGB(0xff, 0xf0, 0x90) : r < 4 ? RGB(0xff, 0x70, 0xd0) : RGB(0x60, 0xe8, 0xff);
                    WFR(x + (int32_t)b * 3, y + (int32_t)r * 3, 2, 2, col);
                }
        x += ((int32_t)w + 1) * 3;
    }
}
/* LOFI STUDY: she writes while the music plays (frames 0..7 in a loop, at the tempo); a second after it stops (and with the loop alone, every 4 s) she looks up at the cat (8..11), stays two
 * seconds, and goes back to her page (frame 0, still, till the music comes back). The frames are changes over the one before: a jump that is
 * not the next frame decodes from frame 0 */
static int32_t lf_cur = -1, lf_hold;
static UI_NOINLINE void lf_goto(const vp_clip_t *c, int32_t f)
{
    int32_t i;
    if (f == lf_cur) return;
    if (f == lf_cur + 1) vp_decode(c, (uint32_t)f);
    else for (i = 0; i <= f; i++) vp_decode(c, (uint32_t)i);
    lf_cur = f;
}
static UI_NOINLINE void lf_step(void)
{
    static int32_t quiet, lp, look, once;
    const vp_clip_t *c = &VP_CLIPS[0];
    int32_t play = nk_mask || song.playing;                            /* (a key held, or the loop running) */
    uint32_t per = 1000u * 120u / (6u * (uint32_t)(song.playing ? clamp(song.g[G_BPM], 40, 240) : 120));
    if (vp_on != 0u) vp_on = 0, lf_cur = -1, lf_goto(c, 0), vp_t = fm1_ms, look = 0;
    quiet = play ? 0 : quiet + 1;
    if (play) once = 0;
    lp = song.playing && !nk_hand && !look ? lp + 1 : 0;
    if (!look && ((lp >= 80) || (quiet >= 20 && !once))) look = 1, lf_hold = 0, once = !play, lp = 0;   /* the loop alone: every 4 s; the quiet: once, after 1 s */
    if (look && nk_hand) look = 0;                                     /* (you play: back to the page) */
    if (look) {                                                        /* she looks up at the cat (8..10), stays, turns back (11) */
        if (lf_cur < 8) { lf_goto(c, 8); vp_t = fm1_ms; return; }
        if (lf_cur < 10) { if (fm1_ms - vp_t >= 160u) { vp_t = fm1_ms; lf_goto(c, lf_cur + 1); } return; }
        if (++lf_hold == (play ? 20 : 40)) lf_goto(c, 11);
        else if (lf_hold > (play ? 24 : 44)) look = 0, lf_goto(c, 0), vp_t = fm1_ms;
        return;
    }
    if (!play) return;                                                 /* (the quiet, after looking: still at her page) */
    if (lf_cur >= 8) lf_goto(c, 0);
    if (fm1_ms - vp_t >= per) { vp_t = fm1_ms; lf_goto(c, (lf_cur + 1) % 8); }   /* writing, at the tempo */
}
/* RACCOON: in its round frame. Quiet: the still of it looking at you (frame 0), turning slowly. Playing: the clip of it dancing with its arms
 * (frames 1..8, the clip turns by itself), faster with the tempo; it swells on the beat */
static int32_t rc2_cur = -1, rc2_ang;
static UI_NOINLINE void rc2_goto(const vp_clip_t *c, int32_t f)
{
    int32_t i;
    if (f == rc2_cur) return;
    if (f == rc2_cur + 1) vp_decode(c, (uint32_t)f); else for (i = 0; i <= f; i++) vp_decode(c, (uint32_t)i);
    rc2_cur = f;
}
static UI_NOINLINE void rc2_paint(int32_t y0, int32_t hh)   /* the round frame turned: each screen pixel looks up its place, 3 x 3 */
{
    const vp_clip_t *c = &VP_CLIPS[1];
    int32_t y, x, R = 119, C = wv_sin4((uint32_t)rc2_ang + 64u), S = wv_sin4((uint32_t)rc2_ang), half = c->w / 2;
    for (y = y0; y < y0 + hh; y++) {
        int32_t dy = y - 120, hw = 0, u, v, du, dv;
        uint16_t *row = cv_px + (uint32_t)(y - y0) * 240u;
        while ((hw + 1) * (hw + 1) + dy * dy <= R * R) hw++;
        if (!hw) continue;
        u = (half << 16) + ((-hw) * C + dy * S) * 172, v = (half << 16) + (hw * S + dy * C) * 172;   /* (16.16, from the row's left end) */
        du = C * 172, dv = -S * 172;
        for (x = 120 - hw; x <= 120 + hw; x++, u += du, v += dv) {
            uint32_t uu = (uint32_t)(u >> 16), vv = (uint32_t)(v >> 16);
            if (uu < c->w && vv < c->h) row[x] = swap16(c->pal[VP_AT(vv * c->w + uu)]);
        }
    }
}
static UI_NOINLINE void rc2_step(void)
{
    const vp_clip_t *c = &VP_CLIPS[1];
    int32_t play = nk_mask || ev_lvl > 12 || song.playing;
    uint32_t per = 1000u * 120u / ((uint32_t)c->fps * (uint32_t)(song.playing ? clamp(song.g[G_BPM], 40, 240) : 120));
    if (vp_on != 1u) vp_on = 1, rc2_cur = -1, rc2_goto(c, 0), vp_t = fm1_ms;
    rc2_ang -= play ? 4 + wv_bass() / 6 : 2;                           /* (it turns for ever: the clip's frames were set upright, the turn is ours) */
    if (!play) { rc2_goto(c, 0); return; }   /* still: looking at the screen */
    if (fm1_ms - vp_t >= per) { vp_t = fm1_ms; rc2_goto(c, rc2_cur >= 8 || rc2_cur < 1 ? 1 : rc2_cur + 1); }
}
static UI_NOINLINE void wvf_step(void)                              /* the whole screen's picture, 20 fps */
{
    static uint8_t last = 0xFF;
    int32_t i;
    wv_w = WVF_N, wv_h = WVF_N;
    if (!wvf_on || last != gv_mode) {
        wvf_on = 1, last = gv_mode, vp_on = 0xFF;
        memset(wv_mem, 0, sizeof wv_mem);
        wvf_vu_n[0] = wvf_vu_n[1] = 0;
        for (i = 0; i < WVF_STARS; i++) wvf_new(i, 0);
    }
    nk_step(), nc_spec();
    if (gv_mode < 4u) { wv_draw(gv_mode); return; }
    wv_ph++;
    wvf_hits();
    switch (gv_mode) {
    case GV_RACER: case GV_DRIVE: rd_step(); break;
    case GV_SHIP: sh_step(); break;
    case GV_WATER: ne_hist(16, 19); break;
    case GV_STUDY: lf_step(); break;
    case GV_RACCOON: rc2_step(); break;
    case GV_ORB: orb_step(); break;
    case GV_TAPE: v2_tape_step(); break;
    case GV_CURVES: {                                                  /* six parts of the spectrum, 40 moments, the newest at 0 */
        uint32_t x, c;
        for (x = 39; x > 0u; x--) for (c = 0; c < 6u; c++) wv_mem[x * 6u + c] = wv_mem[(x - 1u) * 6u + c];
        for (c = 0; c < 6u; c++) wv_mem[c] = (uint8_t)((wf_spec[c * 3u] + wf_spec[c * 3u + 1u] + wf_spec[c * 3u + 2u]) / 3u);
        break;
    }
    default: break;
    }
}
static UI_NOINLINE uint16_t wvf_col(uint32_t v)                     /* the colour of a lit cell of the whole screen */
{
    uint32_t l = ((v & 15u) - 1u) >> 2;
    if (gv_mode == GV_SHIP) return WVF_PAL[0][l];
    return WV_PAL[wv_pal_of(gv_mode)][l];
}
static UI_NOINLINE void wvf_cells(int32_t y0, int32_t hh)          /* the cells of rows y0 .. y0 + hh of the screen */
{
    int32_t x, y;
    for (y = y0 / 3; y < WVF_N && y * 3 < y0 + hh; y++)
        for (x = 0; x < WVF_N; x++) {
            uint32_t v = wv_mem[y * WVF_N + x];
            if (v & 15u) WFR(x * 3, y * 3, 3, 3, wvf_col(v));
        }
}
static UI_NOINLINE void wvf_paint(void)
{
    int32_t y0 = 0;
    while (y0 < 240) {
        int32_t hh = 240 - y0 < 123 ? 240 - y0 : 123;
        cv_begin(240, (uint32_t)hh, C_BLACK);
        cv_ox = 0, cv_oy = -y0;
        wf_rx = 0, wf_ry = y0, wf_rw = 240, wf_rh = hh;
        if (!GV_PIX(gv_mode)) wvf_cells(y0, hh);
        else switch (gv_mode) {
        case GV_SHIP: sh_draw(0, 0, 240, 240, 3, 1); break;
        case GV_RACER: v2_racer(); break;
        case GV_DRIVE: v2_drive(); break;
        case GV_SYNTH: v2_synth(); break;
        case GV_WATER: ne_waterfall(); break;
        case GV_CURVES: ne_curves(); break;
        case GV_FLOW: v3_flow(); break;
        case GV_PATCH: v4_patch(); break;
        case GV_TARGET: v4_tesseract(); break;
        case GV_BLUEPRINT: v3_blueprint(); break;
        case GV_NEO: v2_neo(); break;
        case GV_SMOKE: v2_smoke(); break;
        case GV_DOOM: v2_doom(); break;
        case GV_ROOM: ne_room(); break;
        case GV_FLOAT: ne_float(); break;
        case GV_HOUND: v3_hound(); break;
        case GV_BOXER: v3_boxer(); break;
        case GV_CHORDS: v5_chords(); break;
        case GV_DANCE: ne_dance(); break;
        case GV_DUEL: ne_duel(); break;
        case GV_STUDY: vp_paint((uint32_t)GV_CLIP(gv_mode), y0, hh); break;
        case GV_RACCOON: rc2_paint(y0, hh); break;
        case GV_ORB: orb_paint(); break;
        case GV_TAPE: v2_tape(); break;
        default: break;
        }
        if (fm1_ms - wvf_sel_t < 4000u) wvf_foot();
        cv_ox = 0, cv_oy = 0;
        cv_blit(0, (uint32_t)y0);
        y0 += hh;
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

static int wfm_vis_open(void) { return !ui.menu && (wf_view == 2u || wf_view == 4u) && !ui.home && cur_page()->scope == SC_TRK; }   /* a visualizer is on screen */
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
    if (!wf_pg && wf_view == 4u && !wf_recmode()) {                    /* the visualizer on the whole screen */
        int step = now - wf_t_vz >= 50u;                               /* 20 fps */
        if (now - wf_t_lcd >= 40u) { wf_t_lcd = now; wf_spectrum(); }  /* (the spectrum goes on: the player is not drawn) */
        if (step) { wf_t_vz = now; wvf_step(); }
        if (step || ui.force) wvf_paint();
        wf_was_play = song.playing;
        return;                                                        /* (wf_frame untouched: the windows paint all again after it) */
    }
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
    wf_view = wf_view == 1u ? 0u : wf_view == 0u ? 2u : wf_view == 2u ? 4u : wf_view == 4u ? 3u : 1u;   /* EFFECTS -> PLAYLIST -> VISUALIZER -> WARP -> TAPES */
    if (wf_view == 3u) wfm_tapes_open();
    if (wf_view == 4u) wvf_sel_t = fm1_ms | 1u;                        /* (the names, as it comes in) */
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
            gv_mode = (uint8_t)WV_GV(wv_mode);                         /* (the whole screen shows the same) */
            wf_say(14);
        } else if (wf_view == 4u) {                                    /* the whole screen: its ten pictures; the window's five are the window's too */
            gv_mode = (uint8_t)((gv_mode + (s > 0 ? 1u : GV_N - 1u)) % GV_N);
            if (GV_WIN(gv_mode) >= 0) wv_mode = (uint8_t)GV_WIN(gv_mode);
            wvf_sel_t = fm1_ms | 1u;
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
        wf_textc(120, 210, 0, "WINAMPFM 0.12  BASED ON SLOOP 2.6  GPL-3.0", WC_LOFF);
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
