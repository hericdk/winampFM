/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* Felucca UI drawing: status bar (top), columns + gauges, graphs, focus readout,
 * footer (steps + engine / preset / page). */
static void wfm_draw(void);                    /* winampFM: the player screen (ui_winamp.c) */
static int wfm_here(void);
static void wfm_page_draw(void);
static int wfm_page_ok(void);
static void wfm_menu_draw(void);
static void wfm_song_draw(void);
static void wfm_hold_draw(void);
static void wfm_drum_draw(void);
static void wfm_jingle_tick(void);
static void wfm_party_tick(void);
static uint32_t wfm_view_key(void);                /* winampFM: the player's lower window (a change of it is a screen change too) */
static void wfm_tapes_draw(void);
static void wfm_led_draw(void), wfm_importer_draw(void), wfm_about_draw(void);
/* FM1 Quest (ui_quest.c, included after ui_menu.c) */
static uint32_t str_hash(uint32_t h, const char *s);

/* --------------------------------------------------------- drawing --- */
static UI_NOINLINE int is_eng_name(const char *s)                 /* one of the ENGINES[]->name strings */
{
    uint32_t e;
    for (e = 0; e < NENGINES; e++)
        if (s == ENGINES[e]->name)
            return 1;
    return 0;
}

/* at most 5 characters, and no wider than maxw */
static UI_NOINLINE void fit(char *d, const char *src, const felucca_font_t *f, int32_t maxw)
{
    str_cpy(d, src, is_eng_name(src) ? 8 : 6);           /* engine names are kept whole */
    while (d[0] && text_w(f, d) > maxw)
        d[str_len(d) - 1u] = 0;
}

static UI_NOINLINE int32_t batt_level(void)                         /* ADC ch3 thresholds */
{
    return song.batt_raw >= 591 ? 3 : song.batt_raw >= 561 ? 2 : song.batt_raw >= 531 ? 1 : 0;
}
/* charging (a USB host powers us; there is no charger status line): bars fill 1, 2, 3
 * every 0.6 s, so the header is redrawn twice a second at most; else the level */
static UI_NOINLINE int32_t batt_shown(void)
{
    if (usb.config && !usb.suspended)
        return 1 + (int32_t)((fm1_ms / 600u) % 3u);
    return batt_level();
}




/* one column: [icon] LABEL / value unit / gauge, redrawn only when it changed.
 * ratio: 0..1000 for the gauge, -1 = no gauge. icon: ICON_* (icons.c), ICON_AUTO = by label */
#define LABEL_X (FELUCCA_ICONS ? ICON_CELL + ICON_GAP : 0)
static void q_knob(int32_t cx, int32_t cy, int32_t ratio, uint16_t col);   











static UI_NOINLINE uint32_t steps_hash(const track_t *t)
{
    uint32_t h = 2166136261u, i;
    for (i = 0; i < NSTEP; i++) {
        const step_t *st = &t->step[i];
        h = (h ^ (st->note[0] + st->n * 128u + st->time * 1024u + st->flags * 4096u + st->note[1] * 65536u)) *
            16777619u;
    }
    return h;
}

static UI_NOINLINE uint32_t str_hash(uint32_t h, const char *s)
{
    while (*s)
        h = (h ^ (uint8_t)*s++) * 16777619u;
    return h;
}








/* TRACKS page: four channel strips (mixer style), one under each column: number +
 * REC / ARM / MUTE, the sound's short name, then a level fader with the output meter
 * (note activity), the pattern over its LEN (time running down; bar width = notes in
 * the step) and the play head. The selected track is drawn bright. Every part is
 * its own small canvas with its own signature: while the transport runs only the
 * head markers and the meters move. (No ZOOM focus here.) */
#define TS_HEAD_Y (Y_GRAPH + 2)
#define TS_NAME_Y (Y_GRAPH + 20)
#define TS_Y (Y_GRAPH + 38)
#define TS_H 84                                      /* body: rows Y_GRAPH + 38 .. + 121 */
static struct {
    uint32_t head[NTRK], name[NTRK], fader[NTRK], ov[NTRK], mark[NTRK];
    uint8_t meter[NTRK];
} ts;

static UI_NOINLINE int32_t meter_px(int32_t a)                   /* |sample| (Q15) -> px: 6 dB = TS_H / 10 */
{
    int32_t lg = 0, v;
    if (a < 64)
        return 0;
    while ((a >> lg) > 1)
        lg++;
    v = lg * 8 + (((a << 3) >> lg) & 7);             /* 8 log2(a): 48 (-54 dB) .. 128 (+6 dB) */
    return clamp((v - 48) * (TS_H - 2) / 80, 0, TS_H - 2);
}

static UI_NOINLINE uint32_t trk_level(uint32_t c)                /* LEVEL 0..127 (the drum track: GLO > DRUMS LEVEL) */
{
    return (uint32_t)(c == TRK_DRUM ? song.g[G_DRLVL] : trk[c].p[P_LEVEL]) & 127u;
}

static UI_NOINLINE void trk_short_name(uint32_t c, char *b)      /* the track's sound, b holds 13 */
{
    const track_t *t = &trk[c];
    const engine_t *e = ENGINES[t->eng_req % NENGINES];
    if (c == TRK_DRUM)
        str_cpy(b, "DRUM", 13);
    else if (user_of(t) < UP_SLOTS)
        up_name(user_of(t), b);
    else if (e->npresets)
        str_cpy(b, e->presets[t->preset % e->npresets].name, 13);
    else
        str_cpy(b, e->name, 13);
}













/* the UI's timers, once a frame whatever the screen (a message, the BPM highlight, an armed GO, the
 * white value of the knob just turned) */
static UI_NOINLINE void ui_timers(void)
{
    if (ui.msg_t)
        ui.msg_t--;
    if (ui.bpm_t)
        ui.bpm_t--;
    if (ui.arm_t && !--ui.arm_t)
        ui.arm = 0;
    if (ui.hot_t)
        ui.hot_t--;
}

static int wfm_vis_open(void);
static UI_NOINLINE void ui_draw(void)
{
    ui.frame++;
    wfm_jingle_tick();                                  /* winampFM: the opening sound, while it plays (the simulator) */
    wfm_party_tick();                                   /* winampFM: PARTY, who is on stage */
    {   /* a screen change starts a transition: winampFM, the VHS tape (the rows tear, a tracking band rolls); the menu and power-on dither in */
        static uint32_t last_key = 0xFFFFFFFFu;
        uint32_t key = ui.menu ? 2000u + ui.menu : (!ui.home && cur_page()->scope == SC_TRK) ? 1000u + wfm_view_key() : 3000u + ui.page;
        if (ui.layer == LY_PLAY && !ui.hold_kind && key != last_key) {
            uint32_t is_menu = key >= 2000u && key < 3000u, was_menu = last_key >= 2000u && last_key < 3000u;
            if (key == 2009u || last_key == 2009u) tr_start(4, 560);         /* TAPES, in and out: the tape */
            else if (last_key == 0xFFFFFFFFu || is_menu || was_menu) tr_start(3, 420);          /* power-on and everything of the menu: the halftone fills in */
            else tr_start(4, 480);                                                       /* a window, a page, a home: the VHS tape */
            last_key = key;
        }
        if (tr_kind) {                                  /* every part of the screen goes through the mask; at the end one clean draw */
            if (fm1_ms - tr_t0 >= tr_dur) tr_kind = 0;
            ui.force = 1;
        }
    }
    pads_tick();
    if (rec_go) {                                       /* the take started: say so */
        rec_go = 0;
        ui_message("RECORDING");
    }
    if (ft_bars) {                                      /* a free take closed: the loop it made */
        char m[24];
        uint32_t n = ft_bars;
        ft_bars = 0;
        if (n == 0xFFu) {
            ui_message("TAKE DROPPED");
        } else {
            str_cpy(m, n == 1u ? "LOOP 1 BAR " : n == 2u ? "LOOP 2 BARS " : "LOOP 4 BARS ", sizeof m);
            fmt_int(m + str_len(m), song.g[G_BPM]);
            str_cpy(m + str_len(m), " BPM", sizeof m - str_len(m));
            ui_message(m);
        }
    }
    if (er_flash) {                                     /* EDIT + key took something out */
        er_flash = 0;
        if (!ui.msg_t)
            ui_message("ERASED");
    }
    if (!ui.menu && (ui.layer != LY_PLAY || ui.hold_kind) && !(ui.layer == LY_FX && !ui.hold_kind && wfm_vis_open())) {   /* a layer held / a hold to confirm
                                                         * (winampFM: FX held over the visualizer keeps the picture: the effects show in it) */
        if (ui.hold_kind) {
            layer_shown = 1;
            wfm_hold_draw();                           /* winampFM (was the Quest's ring) */
        }
        else
            layer_screen_draw();
        ui_timers();
        ui.force = 0;
        return;
    }
    if (layer_shown) {                                  /* back to the page */
        layer_shown = 0;
        lcd_fill(0, 0, 240, 240, C_BLACK);
        ui.force = 1;
    }
    if ((rec_wait || ft_on) && !ui.menu && !on_song_page()) {   /* armed / free take */
        wfm_draw();                                     /* winampFM: the player shows it (REC lamp, marquee, LEDs) */
        ui_timers();
        ui.force = 0;
        return;
    }
    if (rec_shown) {                                    /* back to the page */
        rec_shown = 0;
        lcd_fill(0, 0, 240, 240, C_BLACK);
        ui.force = 1;
    }
#if FELUCCA_ARRANGER
    if (!ui.menu && on_song_page()) {
        wfm_song_draw();                                /* winampFM (was the Quest's Chronicle) */
        ui_timers();
        ui.force = 0;
        return;
    }
#endif
    if (ui.menu) {
        if (ui.menu == 1) wfm_menu_draw();              /* winampFM: PREFERENCES (was the Quest's inn) */
        else if (ui.menu == 3) wfm_importer_draw();      /* winampFM */   /* the IMPORTER list */
        else if (ui.menu == 2) wfm_about_draw();         /* winampFM */      /* the memory and the credits */
        else if (ui.menu == 8) wfm_led_draw();           /* winampFM */        /* KEYBOARD LED */
        else if (ui.menu == 9) wfm_tapes_draw();        /* winampFM: TAPES (was the Quest's tape select) */

        ui_timers();
        ui.force = 0;
        return;
    }
    if (!ui.home && cur_page()->scope == SC_TRK) {
        wfm_draw();                                     /* winampFM: the player (was the Quest's PARTY / BOSS / DUEL) */
        ui_timers();
        ui.force = 0;
        return;
    }
    if (on_drum_page()) {
        wfm_drum_draw();                                /* winampFM: DRUM GRID / KIT (was the Quest's DUNGEON / KIT) */
        ui_timers();
        ui.force = 0;
        return;
    }
    if (wfm_page_ok()) {                               /* winampFM: every page with four knobs is the same window */
        wfm_page_draw();
        ui_timers();
        ui.force = 0;
        return;
    }
    cursor_fix();
    felucca_dbg.stage = 3;
    wfm_page_draw();                                   /* (any page left: the same window) */
    ui_timers();
    ui.force = 0;
}
