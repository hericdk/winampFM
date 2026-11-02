/* SPDX-License-Identifier: GPL-3.0-only */
/* SLOOP boot splash: the logo (tools/gen_logo.py -> sloop_logo.h: 16 colours, RLE runs of
 * (run - 1) << 4 | colour), drawn into the canvas in bands of at most 120 rows. */
#ifndef UI_SMALL                 /* (2.7) compiled for size: not the audio path (gfx.c UI_SMALL); the host tests: nothing */
#define UI_SMALL
#endif
#ifndef FELUCCA_LOGO_TEXT       /* (2.7) 1: the boot logo as text, "S" and "LOOP" in the four track colours (4.6 KB of flash
                                 * less: tools/build.py builds so by itself when the flash is full) */
#define FELUCCA_LOGO_TEXT 0
#endif
#if FELUCCA_LOGO_TEXT
UI_SMALL static void sloop_logo_draw(uint32_t y0)
{
    static const char L[5] = {'S', 'L', 'O', 'O', 'P'};
    uint32_t i, w = text_w(&FONT_L, "SLOOP"), k = w * 3u <= 232u ? 3u : 2u, h = 32u, off, y, x;
    int32_t px = 0;
    char c[2] = {0, 0};
    cv_begin(240, h * k, C_BLACK);
    for (i = 0; i < 5u; i++) {                          /* small, at the top left: S white, L O O P the tracks' colours */
        c[0] = L[i];
        px = cv_text(px, 0, &FONT_L, c, i ? TE_COL_DEF[i - 1u] : C_WHITE);
    }
    off = (240u - w * k) / 2u;
    for (y = h * k; y-- > 0;)                           /* then k times as big, in place (from the end: nothing read is
                                                         * overwritten before) */
        for (x = w * k; x-- > 0;)
            cv_px[y * 240u + off + x] = cv_px[(y / k) * 240u + x / k];
    for (y = 0; y < h * k; y++)                         /* (the small one's left-over columns) */
        for (x = 0; x < off; x++)
            cv_px[y * 240u + x] = swap16(C_BLACK);
    cv_blit(0, y0 + 60u);
}
#else
#include "sloop_logo.h"
UI_SMALL static void sloop_logo_draw(uint32_t y0)
{
    uint32_t r0;
    for (r0 = 0; r0 < SLOOP_SPLASH_H; r0 += 120u) {
        uint32_t rows = SLOOP_SPLASH_H - r0 > 120u ? 120u : SLOOP_SPLASH_H - r0;
        uint32_t lo = r0 * SLOOP_SPLASH_W, hi = (r0 + rows) * SLOOP_SPLASH_W, pos = 0, i;
        cv_begin(SLOOP_SPLASH_W, rows, C_BLACK);
        for (i = 0; i < sizeof SLOOP_SPLASH_RLE && pos < hi; i++) {
            uint32_t run = (SLOOP_SPLASH_RLE[i] >> 4) + 1u, c = SLOOP_SPLASH_RLE[i] & 15u, p;
            if (c && pos + run > lo)
                for (p = pos < lo ? lo : pos; p < pos + run && p < hi; p++)
                    cv_px[p - lo] = swap16(SLOOP_SPLASH_PAL[c]);
            pos += run;
        }
        cv_blit((240u - SLOOP_SPLASH_W) / 2u, y0 + r0);
    }
}
#endif

/* FM1 Quest title (README: TITLE): the illustration reduced to 240 x 240 in the scene palette, a dark gradient at the top and bottom,
 * the logo (~170 px, a 1-bit mask drawn cream with a dark outline), a "press play" panel and "based on Sloop". Drawn in two bands of
 * 120 rows with cv_oy so that everything is in screen coordinates. */
static uint16_t q_dim565(uint16_t c, uint32_t f)           /* f / 256 of the brightness */
{
    uint32_t r = (c >> 11) * f / 256u, g = ((c >> 5) & 63u) * f / 256u, b = (c & 31u) * f / 256u;
    return (uint16_t)((r << 11) | (g << 5) | b);
}
static void wfm_splash(void);                                /* winampFM: its own boot screen (ui_winamp.c) */
static void sloop_splash(void)                               /* winampFM: the Winamp boot window (ui_winamp.c wfm_splash) */
{
    wfm_splash();
}
