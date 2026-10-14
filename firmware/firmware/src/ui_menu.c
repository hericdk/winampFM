/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* SLOOP menu (HOME held): IMPORTER (the user samples and songs: see, erase), LOWCUT, ZOOM, LIGHTS, KEYS, NOTES, USB AUDIO, HARDWARE CALIBRATION, ABOUT. */
/* ------------------------------------------------------------ menu --- */
enum { MI_LED, MI_IMPORT, MI_LOWCUT, MI_USB, MI_BOOT, MI_BRIGHT, MI_TAPEAUTO, MI_SERIAL, MI_LCLEAR, MI_ABOUT, MI_COUNT };   /* winampFM (2.6: USB SERIAL, MIDI LEARN CLEAR) */
#define WFM_MENU_HIDE (1u << MI_BRIGHT)                     /* (BRIGHTNESS: the backlight pulse broke the knobs, main.c) */
static const char *const MI_NAME[MI_COUNT] = {"KEYBOARD LED", "IMPORTER", "LOWCUT", "USB AUDIO", "BOOT SOUND", "BRIGHTNESS", "TAPES AUTOPLAY", "USB SERIAL", "MIDI LEARN CLEAR", "ABOUT"};
/* SOUNDS: PRESETS browses every sound of the selected hero (the synth bank, the drum kits), OCT+ plays / pauses it with a short phrase, the other
 * tracks are soloed out while it plays; SEL keeps it on the hero's track, leaving puts back the sound, the solo and the transport */
static uint16_t sx_i, sx_n, sx_com, sx_np, sx_perc;                              /* the highlighted one, the count, the one the track has */
static uint8_t sx_on, sx_open, sx_resume, sx_eng, sx_pre, sx_usr, sx_note, sx_solo;
static uint32_t sx_t0, sx_st, sx_pend;                           /* sx_pend: when the knob last moved (the sound starts when it rests) */
static int32_t sx_cur, sx_tgt;                                     /* the tape strip, 1/256 of a tape: where it is and where it goes */
static int16_t sx_p[P_COUNT];
static uint8_t sx_hn[8], sx_ho[8];                              /* the notes sounding and the step they end on */
static uint32_t led_demo_t0;                                  /* KEYBOARD LED: a turn of a knob plays the effect once (ui_input.c ui_leds) */
#define LED_NFX 6u
#define MI_DY 18                                   /* rows between two menu lines */

static UI_NOINLINE void enc_drop(void)                             /* knob turns nobody takes */
{
    uint32_t k;
    for (k = 0; k < NE; k++)
        panel_enc(k);
}


/* ---- IMPORTER (menu -> IMPORTER): the user sample slots (USR1..USR4, SLOOP 2.6), loaded from the web editor.
 * PRESETS moves, REC held 1.3 s erases the one selected (the ring fills; let go before and nothing happens), OCT- back. */
static uint8_t about_pg;                                /* ABOUT: 0 = the memory, 1 = the credits (PRESETS flips) */
extern uint32_t _data_start[], _data_end[], _bss_start[], _bss_end[], _pool_start[], _pool_end[], _rt_start[], _rt_end[];
#define IMP_N SMP_USER_SLOTS
#define IMP_HOLD_MS 1300u
static uint8_t imp_sel, imp_done;
static uint32_t imp_t0;                                 /* REC pressed since (ms), 0 = not */
static uint8_t flash_ok;                                /* (felucca.c) */
static UI_NOINLINE int ed_smp_erase(uint32_t k, uint32_t all);      /* (editor.c) */
static UI_NOINLINE void imp_name(char *d, const char *src, uint32_t max)   /* a header's name: not 0-terminated when full */
{
    uint32_t j;
    for (j = 0; j < max && src[j] >= 32 && src[j] < 127; j++)
        d[j] = src[j];
    d[j] = 0;
}
#define IMP_ROWS (IMP_N < 11u ? IMP_N : 11u)
static UI_NOINLINE uint32_t imp_top(void) { return imp_sel < 5u || IMP_N <= IMP_ROWS ? 0u : imp_sel + 6u > IMP_N ? IMP_N - IMP_ROWS : imp_sel - 5u; }
static UI_NOINLINE uint32_t imp_prog(void)                          /* 0..100 */
{
    uint32_t d = imp_t0 ? fm1_ms - imp_t0 : 0u;
    return imp_done ? 100u : d >= IMP_HOLD_MS ? 100u : d * 100u / IMP_HOLD_MS;
}
static UI_NOINLINE int imp_used(uint32_t i)                         /* 1 = something is stored in the item */
{
    return i < SMP_USER_SLOTS && usr_nz[i] != 0u;
}
static UI_NOINLINE void imp_erase(uint32_t i)
{
    char b[8] = "USR1";
    if (!flash_ok || !imp_used(i)) {
        ui_message("NOTHING THERE");
        return;
    }
    b[3] = (char)('1' + i);
    ed_smp_erase(i, 1u);
    ui_say("ERASED ", b);
}
static UI_NOINLINE void importer_input(void)
{
    int32_t s = panel_enc(EN_PRESET);
    if (s)
        imp_sel = (uint8_t)((imp_sel + (s > 0 ? 1u : IMP_N - 1u)) % IMP_N);
    if (fm1_in.buttons & (1u << panel.btn[B_REC])) {
        if (!imp_t0)
            imp_t0 = fm1_ms | 1u, imp_done = 0;
        if (!imp_done && fm1_ms - imp_t0 >= IMP_HOLD_MS) {
            imp_done = 1;
            imp_erase(imp_sel);
        }
    } else {
        imp_t0 = 0;
        imp_done = 0;
    }
}

static UI_NOINLINE void sx_leave(void);
static int wfm_tape_busy(void);
static void wfm_tape_copied(const char *was, const char *now);
static uint8_t wft_back;                                         /* winampFM: a tape was copied: once its show ends, back to EFFECTS (the group it was on) */
static void wfm_show(uint32_t view);
static UI_NOINLINE void menu_close(void)
{
    if (ui.menu == 9) sx_leave();
    if (song.playing || transport_req)
        settings_later = 1;                            /* (a flash write stops the audio: once stopped) */
    else
        settings_save();                               /* palette / panel table, if changed */
    ui.menu = 0;
    ui.force = 1;
    go_home();
}

static UI_NOINLINE void sx_name(char *d, uint32_t i, uint32_t max)       /* a sound of the list (a tape): its name */
{
    const char *n;
    if (is_drum(TSEL)) n = DRUM_KIT_NAMES[i % DRUM_KITS];
    else if (i >= sx_np) n = BR_PAD[((i - sx_np) / 16u) & 1u][(i - sx_np) % 16u];     /* the Brazilian instruments */
    else if (i < NBANK) n = BANK[i].name;
    else n = "USER";
    str_cpy(d, n, max);
}
static UI_NOINLINE uint32_t sx_brkit(uint32_t which)           /* the index (in the synthesised kits) of BRASIL (0) or BRASIL 2 (1) */
{
    uint32_t i;
    for (i = DRUM_SAMPLED; i < DRUM_KITS; i++)
        if (str_eq(DRUM_KIT_NAMES[i], which ? "BRASIL 2" : "BRASIL")) return i - DRUM_SAMPLED;
    return 0;
}
static UI_NOINLINE void sx_enter(void)                          /* remember the track's sound as it is */
{
    track_t *t = TSEL;
    uint32_t i;
    for (i = 0; i < P_COUNT; i++) sx_p[i] = t->p[i];
    sx_eng = t->eng_req, sx_pre = t->preset, sx_usr = t->user;
    sx_perc = t->perc;
    if (is_drum(t)) sx_n = DRUM_KITS, sx_np = DRUM_KITS, sx_i = (uint16_t)drum_kit();
    else {                                                        /* the presets, and for the cleric / bard also the Brazilian instruments (the others keep the normal sounds) */
        uint32_t n;
        sx_i = (uint16_t)preset_pos(&n), sx_np = (uint16_t)n, sx_n = (uint16_t)(n + (song.sel == 2u ? 32u : 0u));
        if (song.sel != 2u) t->perc = 0, sx_perc = 0;
        if (t->perc) sx_i = (uint16_t)(n + ((t->perc - 1u) / 16u == sx_brkit(1) ? 16u : 0u) + (t->perc - 1u) % 16u);
    }
    sx_com = sx_i;
    sx_open = 1;
    sx_cur = sx_tgt = 0, sx_pend = 0;
}
static UI_NOINLINE void sx_stop(void)                           /* silence, the solo and the transport back */
{
    track_t *t = TSEL;
    if (!sx_on) return;
    sx_on = 0;
    { uint32_t k; for (k = 0; k < 8u; k++) sx_hn[k] = 0; }
    trk_all_off(t);
    song.solo = sx_solo;
    if (sx_resume) transport_req = 1, sx_resume = 0;
}
static UI_NOINLINE void sx_start(void)
{
    sx_solo = song.solo;
    song.solo = (uint8_t)(1u << song.sel);
    sx_resume = (uint8_t)(song.playing && !transport_req);
    if (sx_resume) transport_req = 2;
    sx_on = 1, sx_t0 = fm1_ms, sx_st = 0, sx_note = 0;
}
static UI_NOINLINE void sx_apply(void)                          /* the highlighted sound, on the track (until leaving) */
{
    track_t *t = TSEL;
    trk_all_off(t);
    if (is_drum(t)) TDRUM->p[P_E0] = (int16_t)sx_i;
    else if (sx_i >= sx_np) {                                      /* a Brazilian instrument: the hero's keys hit it */
        uint32_t k = sx_i - sx_np;
        t->perc = (uint16_t)(1u + sx_brkit(k / 16u) * 16u + k % 16u);
    } else {
        t->perc = 0;
        preset_go(sx_i);
    }
    sx_t0 = fm1_ms, sx_st = 0, sx_note = 0;
}
static UI_NOINLINE void sx_leave(void)
{
    track_t *t = TSEL;
    uint32_t i;
    sx_stop();
    if (sx_open && sx_i != sx_com) {                              /* heard but not kept: the old sound */
        for (i = 0; i < P_COUNT; i++) t->p[i] = sx_p[i];
        t->eng_req = sx_eng, t->preset = sx_pre, t->user = sx_usr, t->perc = sx_perc;
        sync_reload = 1;
        panic_req |= (uint8_t)(1u << song.sel);
    }
    sx_open = 0;
}
/* what each kind of sound plays (the sounds page's phrases): a bass line, a melody, held chords, comping chords, chord stabs, a beat for the kits; once, ~5 s */
typedef struct { uint8_t s, n, l; } sxe_t;
static const sxe_t SX_BASS[] = {{0, 36, 3}, {4, 36, 1}, {6, 43, 2}, {8, 41, 3}, {12, 39, 2}, {14, 38, 2}, {16, 36, 3}, {20, 36, 1}, {22, 43, 2}, {24, 46, 3}, {28, 43, 4}};
static const sxe_t SX_MEL[] = {{0, 72, 2}, {2, 75, 2}, {4, 77, 3}, {8, 79, 2}, {10, 77, 2}, {12, 75, 4}, {16, 72, 2}, {18, 70, 2}, {20, 72, 3}, {24, 67, 4}, {28, 70, 2}, {30, 72, 2}};
static const sxe_t SX_HELD[] = {{0, 60, 14}, {0, 63, 14}, {0, 67, 14}, {0, 70, 14}, {16, 58, 14}, {16, 62, 14}, {16, 65, 14}, {16, 68, 14}};
static const sxe_t SX_STAB[] = {{0, 60, 2}, {0, 63, 2}, {0, 67, 2}, {0, 70, 2}, {8, 60, 2}, {8, 63, 2}, {8, 67, 2}, {8, 70, 2}, {16, 58, 2}, {16, 62, 2}, {16, 65, 2}, {16, 68, 2}, {24, 58, 2}, {24, 62, 2}, {24, 65, 2}, {24, 68, 2}};
static const uint8_t SX_COMPS[10] = {0, 3, 6, 10, 12, 16, 19, 22, 26, 28};
static const sxe_t SX_DRUM[] = {{0, 36, 1}, {2, 42, 1}, {3, 70, 1}, {4, 38, 1}, {6, 42, 1}, {7, 63, 1}, {8, 36, 1}, {10, 36, 1}, {10, 42, 1}, {11, 56, 1}, {12, 38, 1}, {14, 46, 1}, {15, 70, 1}};
static UI_NOINLINE uint32_t sx_role(void)                       /* 0 bass, 1 melody, 2 held chords, 3 comping, 4 stabs, 5 beat */
{
    static const uint8_t BY_KIND[8] = {0, 3, 2, 2, 1, 1, 4, 1};   /* BASS KEYS ORGN PAD LEAD PLCK STAB FX */
    if (is_drum(TSEL)) return 5u;
    if (sx_i >= sx_np) return 0u;                               /* a percussion instrument: a groove of hits */
    return sx_i < NBANK ? BY_KIND[BANK[sx_i].kind & 7u] : 1u;
}
static UI_NOINLINE void sx_hit(track_t *t, uint32_t note, uint32_t step, uint32_t len)
{
    uint32_t k;
    if (is_drum(t)) { drum_on(note, 100); return; }
    trk_note_on(t, note, 100);
    for (k = 0; k < 8u; k++)
        if (!sx_hn[k]) { sx_hn[k] = (uint8_t)note, sx_ho[k] = (uint8_t)(step + len); return; }
}
static UI_NOINLINE void sx_tick(void)                           /* the phrase of this kind of sound, one time: 150 ms a step */
{
    track_t *t = TSEL;
    uint32_t st, k, role, nev;
    const sxe_t *ev;
    if (!sx_on) return;
    st = (fm1_ms - sx_t0) / 150u;
    role = sx_role();
    ev = role == 0u ? SX_BASS : role == 1u ? SX_MEL : role == 2u ? SX_HELD : role == 4u ? SX_STAB : SX_DRUM;
    nev = role == 0u ? sizeof SX_BASS / sizeof *SX_BASS : role == 1u ? sizeof SX_MEL / sizeof *SX_MEL : role == 2u ? sizeof SX_HELD / sizeof *SX_HELD :
          role == 4u ? sizeof SX_STAB / sizeof *SX_STAB : sizeof SX_DRUM / sizeof *SX_DRUM;
    for (; sx_st <= st && sx_st < 36u; sx_st++) {
        uint32_t s = sx_st;
        for (k = 0; k < 8u; k++)
            if (sx_hn[k] && sx_ho[k] <= s) trk_note_off(t, sx_hn[k]), sx_hn[k] = 0;
        if (role == 3u) {
            for (k = 0; k < 10u; k++)
                if (SX_COMPS[k] == s) sx_hit(t, 60, s, 2), sx_hit(t, 63, s, 2), sx_hit(t, 67, s, 2);
        } else {
            uint32_t sm = role == 5u ? s % 16u : s;
            for (k = 0; k < nev; k++)
                if (ev[k].s == sm) sx_hit(t, ev[k].n, s, ev[k].l);
        }
    }
    if (st >= 36u) sx_stop();                                   /* it plays once: PLAY plays it again */
}

/* menu: PRESETS moves, OCT+ confirms, OCT- cancels (ABOUT -> list -> close) */
static UI_NOINLINE void menu_input(uint32_t pressed)
{
    int32_t s;
    uint32_t ok = (pressed >> panel.btn[B_OCTUP]) & 1u, back = (pressed >> panel.btn[B_OCTDN]) & 1u, sel = (pressed >> panel.btn[B_SCL]) & 1u;
    if (ui.menu == 1) ok |= sel;                                       /* the menu list: SEL does what OCT+ does */
    if (back && ui.menu == 9) {                         /* reached by holding PLAY: back goes home */
        sx_leave();
        ui.menu = 0, ui.force = 1;
        go_home();
        return;
    }
    if (back) {
        if (ui.menu == 9) sx_leave();
        if (ui.menu >= 2)
            ui.menu = 1, ui.force = 1, imp_t0 = 0;
        else
            menu_close();
        return;
    }
    if (ui.menu == 2) {                                /* ABOUT: PRESETS flips between the memory and the credits */
        if (panel_enc(EN_PRESET)) {
            about_pg ^= 1u;
            ui.force = 1;
        }
        enc_drop();
        return;
    }
    if (ui.menu == 3) {                                /* IMPORTER */
        importer_input();
        enc_drop();
        return;
    }
    if (ui.menu == 8) {                                /* KEYBOARD LED: PRESETS picks the effect (one at a time), KNOB 1 its speed, KNOB 2 its fade; every change plays it on the keys */
        int32_t d = panel_enc(EN_PRESET) + panel_enc(EN_SELECT), k1 = panel_enc(EN_K1), k2 = panel_enc(EN_K2), k3 = panel_enc(EN_K3);
        if (k3) led_hold = (uint8_t)(k3 > 0);                                    /* KNOB 3: once per press / while held */
        if (d) {
            uint32_t pos = 0, q;
            for (q = 0; q < LED_NFX; q++) if (LED_ORDER[q] == led_fx) pos = q;
            led_fx = LED_ORDER[(pos + (d > 0 ? 1u : LED_NFX - 1u)) % LED_NFX];
        }
        if (k1) led_sp = (uint8_t)clamp((int32_t)led_sp + k1, 0, 15);
        if (k2) led_fd = (uint8_t)clamp((int32_t)led_fd + k2, 0, 15);
        if (d || k1 || k2 || k3) led_demo_t0 = fm1_ms | 1u, wfm_opts_save(), settings_later = 1;
        enc_drop();
        return;
    }
    if (ui.menu == 9 && wfm_tape_busy()) { sx_tick(); enc_drop(); return; }   /* winampFM: the copy is being shown */
    if (ui.menu == 9 && wft_back) {                    /* winampFM: then back home, to EFFECTS on the group that was open */
        wft_back = 0;
        sx_leave();
        ui.menu = 0, ui.force = 1;
        go_home();
        wfm_show(1);
        return;
    }
    if (ui.menu == 9) {                                /* SOUND SELECT (hold PLAY): PRESETS roll the tapes and, when the knob rests, the sound plays alone; SEL keeps it; ALGORITHM picks the hero */
        int32_t d = panel_enc(EN_PRESET) + panel_enc(EN_SELECT), a = panel_enc(EN_ALGO);
        uint32_t sel = (pressed >> panel.btn[B_SCL]) & 1u, pl = (pressed >> panel.btn[B_PLAY]) & 1u;
        if (pl) {                                      /* PLAY: hear it again */
            if (sx_on) sx_stop();
            sx_pend = 0;
            sx_start();
        }
        if (a) {
            sx_leave();
            song.sel = (uint8_t)((song.sel + (a > 0 ? 1u : NTRK - 1u)) % NTRK);
            sx_enter();
            ui.force = 1;
        }
        if (d && sx_n) {
            sx_i = (uint16_t)((sx_i + (d > 0 ? 1u : sx_n - 1u)) % sx_n);
            sx_tgt += d > 0 ? 256 : -256;
            if (sx_on) sx_stop();                                              /* quiet while it moves */
            sx_apply();
            sx_pend = fm1_ms | 1u;
            ui.force = 1;
        }
        if (sx_pend && !sx_on && TAPE_AUTO() && (int32_t)(fm1_ms - sx_pend) >= 450) {     /* the knob rested: now it plays (menu TAPES AUTOPLAY; off: PLAY plays it) */
            sx_pend = 0;
            sx_start();
            ui.force = 1;
        }
        {                                              /* (winampFM, unsaid: PLAY held 0.8 s copies too) */
            static uint32_t pl_t0;
            uint32_t held = (fm1_in.buttons >> panel.btn[B_PLAY]) & 1u;
            if (pl) pl_t0 = fm1_ms | 1u;
            if (!held) pl_t0 = 0;
            else if (pl_t0 && fm1_ms - pl_t0 >= 800u) pl_t0 = 0, sel = 1;
        }
        if (sel) {                                     /* copy: this is the track's sound now, clean: the hero's effects, chord and arpeggio off, as the preview played */
            if (!is_drum(TSEL)) {                      /* the sound's own parameters again, nothing of what was set before: the preset applied whole, the effects of the hero, chord, arpeggio, pan */
                track_t *t = TSEL;
                if (sx_i < sx_np) apply_preset(t->preset);
                fx_reset(song.sel);
                t->p[P_CHORD] = TP[P_CHORD].def;
                t->p[P_AMODE] = TP[P_AMODE].def;
                t->p[P_PAN] = TP[P_PAN].def;
            }
            {
                char nm[16], was[16], mm[28];
                sx_name(was, sx_com, sizeof was);       /* (winampFM: the old name, for the elevator) */
                sx_enter();
                sx_name(nm, sx_i, sizeof nm); str_cpy(mm, "COPIED: ", sizeof mm); str_cpy(mm + 8, nm, sizeof mm - 8); ui_message(mm);
                wfm_tape_copied(was, nm);               /* winampFM: the tape flies into the track; TAPES stays open */
            }
            ui.force = 1;
            return;
        }
        sx_tick();
        enc_drop();
        return;
    }
    if ((s = panel_enc(EN_PRESET)) != 0 && ui.menu == 1)
        do ui.menu_sel = (uint8_t)((ui.menu_sel + (s > 0 ? 1u : MI_COUNT - 1u)) % MI_COUNT);
        while ((WFM_MENU_HIDE >> ui.menu_sel) & 1u);
    s = panel_enc(EN_K1);
    if ((s != 0 || ok) && ui.menu == 1 && ui.menu_sel == MI_LOWCUT) {
        /* KNOB 1: right = ON, left = OFF; OCT+ toggles */
        uint32_t *v = &settings.lowcut;
        *v = s > 0 ? 1u : s < 0 ? 0u : !*v;
        fx_lowcut = (uint8_t)(settings.lowcut != 0);
        ok = 0;
    }
    if ((s != 0 || ok) && ui.menu == 1 && ui.menu_sel == MI_USB) {     /* right FULL, left MASTER; OCT+ toggles */
        usb_full = (uint8_t)(s > 0 ? 1u : s < 0 ? 0u : !usb_full);
        ok = 0;
    }
    if ((s != 0 || ok) && ui.menu == 1 && ui.menu_sel == MI_BOOT) {     /* right ON, left OFF; OCT+ toggles: the sound at power-on (the lights stay) */
        boot_mute = (uint8_t)(s > 0 ? 0 : s < 0 ? 1 : !boot_mute);
        wfm_opts_save(), settings_later = 1;
        ok = 0;
    }
    if ((s != 0 || ok) && ui.menu == 1 && ui.menu_sel == MI_SERIAL) {    /* SLOOP 2.6: USB SERIAL (at the next start), right ON, left OFF */
        usb_serial = (uint8_t)(s > 0 ? 1u : s < 0 ? 0u : !usb_serial);
        settings_later = 1;
        ok = 0;
    }
    if (ok && ui.menu == 1 && ui.menu_sel == MI_LCLEAR) {                /* SLOOP 2.6: OCT+ forgets every CC MIDI LEARN holds */
        ui_message(ml_clear_all() ? "LEARN CLEARED" : "NOTHING LEARNED");
        ok = 0;
    }
    if ((s != 0 || ok) && ui.menu == 1 && ui.menu_sel == MI_TAPEAUTO) {   /* winampFM: right ON, left OFF; OCT+ toggles */
        uint32_t on = s > 0 ? 1u : s < 0 ? 0u : !TAPE_AUTO();
        settings.zoom = (settings.zoom & ~(1u << 12)) | (on ? 0u : 1u << 12), settings_later = 1;
        ok = 0;
    }
    if ((s != 0 || ok) && ui.menu == 1 && ui.menu_sel == MI_BRIGHT) {  /* winampFM: KNOB 1 right brighter, left darker; OCT+ the next (wraps) */
        uint32_t v = BL_LV();
        v = s > 0 ? (v < 10u ? v + 1u : 10u) : s < 0 ? (v > 2u ? v - 1u : 2u) : v < 10u ? v + 1u : 2u;
        settings.zoom = (settings.zoom & ~(15u << 8)) | (10u - v) << 8, settings_later = 1;
        bl_lv = (uint8_t)v;
        ok = 0;
    }
    if (ok && ui.menu == 1) {
        switch (ui.menu_sel) {
        case MI_IMPORT:
            ui.menu = 3;
            imp_sel = 0;
            imp_t0 = 0;
            ui.force = 1;
            break;
        case MI_LED:
            ui.menu = 8;
            led_demo_t0 = fm1_ms | 1u;
            ui.force = 1;
            break;
        case MI_ABOUT:
            ui.menu = 2;
            about_pg = 0;
            ui.force = 1;
            break;
        default:
            menu_close();
            break;
        }
    }
    enc_drop();                                        /* swallow the rest while the menu is up */
}

