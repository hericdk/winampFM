/* SPDX-License-Identifier: GPL-3.0-only
 * SMK-37 port: Copyright (C) 2026 M. Geisler (genkigenki/makkuro) */
/* The FADER BANK screen: the four banks, the active one lit, what faders 1..4 set (track colours).
 * FADER BANK opens it and then steps the bank, as < > and K2 SELECT do; PATCH closes it, or it closes by itself.
 * Also the pad picker's screen (ui_pick.c): both drawn as winampFM's windows. */
static const char *const FB_NAME[4] = {"LEVEL", "FILTER", "REVERB", "DELAY"};
#define FB_SHOW_MS 4000u                           /* up this long after the last change */
static uint8_t fb_open;
static uint32_t fb_t, fb_frame;                    /* last action (ms); the frame drawn last */

static int fb_shown(void)
{
    return fb_open && !ui.menu && ui.layer == LY_PLAY && !ui.hold_kind;
}
static void fb_set(uint32_t b)                     /* bank b, the screen up */
{
    smk_fbank = (uint8_t)(b % 4u);
    smk_fbank_new = 1;
    fb_open = 1;
    fb_t = fm1_ms;
    ui.force = 1;
}
static void fb_close(void) { fb_open = 0; }
static void fb_expire(void)                        /* up FB_SHOW_MS after the last change */
{
    if (fb_open && fm1_ms - fb_t > FB_SHOW_MS)
        fb_open = 0, ui.force = 1;
}
static int fb_left(void)                           /* drawn last frame, not now: the page needs a redraw */
{
    int l = fb_frame && fb_frame + 1u == ui.frame;
    if (l)
        fb_frame = 0;
    return l;
}

/* what fader i sets in bank b (main.c smk_faders) */
static const param_desc_t *fb_param(uint32_t b, uint32_t i, int16_t **vp)
{
    static const uint8_t TID[4] = {P_LEVEL, P_TFLT, P_REV, P_DLY}, GID[4] = {G_DRLVL, 0, G_DRREV, G_DRDLY};
    if (i == TRK_DRUM && b != 1u) {
        *vp = &song.g[GID[b & 3u]];
        return &GP[GID[b & 3u]];
    }
    *vp = &trk[i].p[TID[b & 3u]];
    return &TP[TID[b & 3u]];
}

/* the SMK-37's own screens as Winamp windows (ui_winamp.c's skin), over the whole screen while they are up */
static UI_NOINLINE void smk_win_band(uint32_t band, const char *title)
{
    int32_t y0 = (int32_t)band * 120;
    cv_begin(240, 120, WC_FR2);
    cv_ox = 0, cv_oy = -y0;
    wf_rx = 0, wf_ry = y0, wf_rw = 240, wf_rh = 120;
    wf_window(0, 240, title);
}
static UI_NOINLINE void smk_win_blit(uint32_t band)
{
    cv_ox = 0, cv_oy = 0;
    cv_blit(0, band * 120u);
}

/* FADER BANK: the four banks as Winamp's buttons (the one in use pressed, its LED lit), then faders 1..4 as four big EQ
 * sliders in the tracks' colours (the colour of a track's notes on the player and its pads), each value under it */
static void fb_draw(void)
{
    static uint32_t shown;
    uint32_t b = smk_fbank & 3u, i, band, sig = b * 7919u;
    int16_t *vp;
    if (fb_frame + 1u != ui.frame)                 /* another screen was up: all of it */
        ui.force = 1;
    fb_frame = ui.frame;
    for (i = 0; i < 4u; i++) {
        fb_param(b, i, &vp);
        sig = sig * 31u + (uint32_t)(*vp + 200);
    }
    if (!ui.force && sig == shown)
        return;
    shown = sig;
    for (band = 0; band < 2u; band++) {
        smk_win_band(band, "WINAMPFM FADER BANK");
        for (i = 0; i < 4u; i++) {                 /* the banks */
            int32_t bx = 5 + (int32_t)i * 58, on = i == b;
            char l[10] = {(char)('A' + i), ' ', 0};
            str_cpy(l + 2, FB_NAME[i], sizeof l - 2);
            wf_button(bx, 16, 56, 15, on);
            WFR(bx + 3, 21, 3, 4, on ? WC_GRN : WC_GDIM);
            wf_textc(bx + 31, 21, 0, l, on ? WC_WHT : WC_GLY);
        }
        wf_inset(5, 37, 230, 166, WC_LCD);
        for (i = 0; i < 4u; i++) {                 /* the faders: the track, the slider, the value */
            int32_t x = 33 + (int32_t)i * 58;
            uint16_t tc = WF_KNOB[(i + 3u) & 3u];
            char lab[8] = "TRACK 1", val[12], v2[20];
            const char *unit = "";
            const param_desc_t *d = fb_param(b, i, &vp);
            if (i == TRK_DRUM)
                str_cpy(lab, "DRUMS", sizeof lab);
            else
                lab[6] = (char)('1' + i);
            wf_textc(x + 1, 42, 1, lab, tc);
            WFR(x - 10, 52, 21, 2, tc);
            wf_vslider2(x, 60, 112, wf_frac(d, *vp), 1, 0);
            param_format(d, *vp, val, &unit);
            str_cpy(v2, val, sizeof v2);
            str_cpy(v2 + str_len(v2), unit, sizeof v2 - str_len(v2));
            wf_textc(x + 1, 180, 1, v2, WC_GRN);
            {
                char f[3] = {'F', (char)('1' + i), 0};
                wf_textc(x + 1, 192, 0, f, WC_LAB);
            }
        }
        wf_inset(5, 209, 230, 26, WC_LCD);
        wf_text(10, 213, 1, "< > K2 FADER BANK", WC_GRN), wf_text(124, 213, 1, "NEXT BANK", WC_LAB);
        wf_text(10, 225, 1, "PATCH", WC_GRN), wf_text(124, 225, 1, "CLOSE", WC_LAB);
        smk_win_blit(band);
    }
}

/* the pad picker (ui_pick.c): the sound loaded in the display, the page's sounds as 2 x 8 Winamp buttons in the pads'
 * order (the top row first), each with its kind's colour (the pads' colour; the sound loaded white), the kind and page below */
static void pick_draw(void)
{
    static uint32_t shown;
    pick_pg_t pg;
    uint32_t np = pick_info(&pg), cur = pick_cur(), band, w, sig;
    uint16_t kc = pick_col(pg.kind, 0);
    char nm[13], l1[4], l2[4], foot[16];
    if (pick_frame + 1u != ui.frame)               /* another screen was up: all of it */
        ui.force = 1;
    pick_frame = ui.frame;
    sig = (np ? pg.first * 977u + pg.kind * 31u : 0u) + cur * 7919u + song.sel * 131u + pick_total() * 3u;
    if (!ui.force && sig == shown)
        return;
    shown = sig;
    str_cpy(foot, PICK_KIND[pg.kind % 10u], sizeof foot);
    if (pg.nsub > 1u) {
        uint32_t n = str_len(foot);
        foot[n++] = ' ';
        fmt_int(foot + n, pg.sub + 1);
        n = str_len(foot);
        foot[n++] = '/';
        fmt_int(foot + n, pg.nsub);
    }
    for (band = 0; band < 2u; band++) {
        smk_win_band(band, "WINAMPFM PAD SOUNDS");
        wf_inset(5, 16, 230, 22, WC_LCD);
        {
            char tn[8] = "TRACK 1";
            pick_name(cur, nm);
            if (is_drum(TSEL))
                str_cpy(tn, "DRUMS", sizeof tn);
            else
                tn[6] = (char)('1' + song.sel % NTRK);
            wf_text(10, 23, 1, nm, WC_GRN);
            wf_textr(230, 23, 1, tn, WF_KNOB[(song.sel + 3u) & 3u]);
        }
        for (w = 0; np && w < 16u && w < pg.len; w++) {
            int32_t x = 6 + (int32_t)(w & 7u) * 29, y = w < 8u ? 46 : 120, sel = pg.first + w == cur;
            pick_name(pg.first + w, nm);
            pick_abbr(nm, l1, l2);
            wf_button(x, y, 27, 68, sel);
            WFR(x + 3, y + 4, 21, 4, sel ? WC_WHT : kc);   /* the pad's colour */
            wf_textc(x + 14, y + 22, 1, l1, sel ? WC_WHT : WC_GLY);
            wf_textc(x + 14, y + 38, 1, l2, sel ? WC_WHT : WC_GLY);
        }
        wf_inset(5, 196, 230, 39, WC_LCD);
        wf_text(10, 200, 1, "<", WC_GRN), wf_textr(230, 200, 1, ">", WC_GRN);
        wf_textc(120, 200, 1, foot, kc);
        wf_text(10, 213, 1, "PAD", WC_GRN), wf_text(34, 213, 1, "LOADS AND PLAYS IT", WC_LAB);
        wf_text(10, 225, 1, "KNOB BANK", WC_GRN), wf_text(72, 225, 1, "CLOSE", WC_LAB);
        smk_win_blit(band);
    }
}
