/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* Physical panel: which matrix button / encoder carries which printed label.
 * The default table can be overridden by HARDWARE CALIBRATION (hold OCT- and
 * OCT+ while powering on), which asks for each label in turn. The learned
 * table lives in .noinit and, with FELUCCA_FLASH, in flash with the settings
 * (project.c). */
/* (B_SCL: the button printed SEL, the key / scale button between FX and ENV) */
enum { B_FX, B_SCL, B_ENV, B_LFO, B_EDIT, B_GLO, B_HOME, B_SAVE, B_ARP, B_SEQ, B_PLAY, B_REC,
       B_OCTDN, B_OCTUP, NB };
enum { EN_SELECT, EN_ALGO, EN_PRESET, EN_K1, EN_K2, EN_K3, EN_K4, NE };
static const char *const B_NAME[NB] = {"FX", "SEL", "ENV", "LFO", "EDIT", "GLO", "HOME", "SAVE",
                                        "ARP", "SEQ", "PLAY", "REC", "OCT-", "OCT+"};
static const char *const E_NAME[NE] = {"SELECT", "ALGORITHM", "PRESETS", "KNOB 1", "KNOB 2",
                                        "KNOB 3", "KNOB 4"};
#define PANEL_MAGIC 0x50414E35u          /* "PAN5": bump when PANEL_DEFAULT changes */

typedef struct {
    uint32_t magic;
    uint8_t btn[NB];             /* matrix button id (0..13) per label */
    uint8_t enc[NE];             /* matrix encoder (0..6) per role */
    int8_t dir[NE];              /* +1 / -1 so that clockwise is + */
} panel_t;
panel_t panel __attribute__((section(".noinit")));

static const panel_t PANEL_DEFAULT = {
    PANEL_MAGIC,
    {2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 0, 1},   /* PLAY = 12, REC = 13 */
    {0, 1, 6, 2, 3, 4, 5},                           /* SELECT = enc 0, ALGORITHM = enc 1, PRESETS = enc 6 */
    {1, 1, 1, 1, 1, 1, 1},
};

static void panel_init(void)                     /* also after a flash load: ids are used as array indexes and shifts */
{
    uint32_t i, ok = panel.magic == PANEL_MAGIC;
    for (i = 0; ok && i < NB; i++)
        ok = panel.btn[i] < 14u;
    for (i = 0; ok && i < NE; i++)
        ok = panel.enc[i] < 7u && (panel.dir[i] == 1 || panel.dir[i] == -1);
    if (!ok)
        panel = PANEL_DEFAULT;
}

static uint32_t panel_btn_of(uint32_t matrix_id)        /* label of a matrix button, NB if none */
{
    uint32_t b;
    for (b = 0; b < NB; b++)
        if (panel.btn[b] == matrix_id)
            return b;
    return NB;
}

static void panel_led(uint32_t label, int on) { fm1_led_key(panel.btn[label], on); }

/* steps of a role, + = clockwise */
static uint32_t ui_input_ms;                    /* the last button, key or knob turn (ui_input.c; autosave) */
/* the knobs (roles, bit per EN_*) not to be read again in this UI pass (ui_input: a layer took them, #102 of
 * Felucca): panel_enc returns 0 and leaves their detents for the next pass */
static uint32_t enc_hold;
static int32_t panel_enc(uint32_t role)
{
    int32_t s;
    if ((enc_hold >> role) & 1u)
        return 0;
    s = fm1_enc_take(panel.enc[role]) * panel.dir[role];
    if (s)
        ui_input_ms = fm1_ms;                      /* a knob turning is not idle either: autosave waits */
    return s;
}

/* user settings that survive a reset */
#define SETTINGS_MAGIC 0x53455433u              /* "SET3" */
struct { uint32_t magic, palette, lowcut, zoom; } settings __attribute__((section(".noinit")));
/* the Quest options live in settings.zoom (the old large-readout flag is gone), stored as value XOR default so that a zero word is the defaults:
 *   bits 0-1 map enemies (0 few, 1 normal, 2 many), bit 2 damage numbers (1 on), bits 3-4 boss speed (0 slow, 1 normal, 2 fast) */
#define Q_OPT_DENS() ((uint32_t)((settings.zoom & 3u) ^ 1u))
#define Q_OPT_DMG()  ((uint32_t)(((settings.zoom >> 2) & 1u) ^ 1u))
#define Q_OPT_BSPD() ((uint32_t)(((settings.zoom >> 3) & 3u) ^ 1u))
/* winampFM: the screen's brightness 2..10 (20 .. 100 %), bits 8-11 of settings.zoom as 10 - level; main.c's TIMER5 (10 kHz) pulses the backlight
 * with it, 1 kHz: no flicker */
#define TAPE_AUTO() (!((settings.zoom >> 12) & 1u))           /* winampFM: the tapes play when the knob rests (menu TAPES AUTOPLAY), bit 12 set = off */
#define BL_LV() ((uint32_t)clamp(10 - (int32_t)((settings.zoom >> 8) & 15u), 2, 10))
static volatile uint8_t bl_lv = 10;
#define Q_OPT_SET(shift, mask, v) (settings.zoom = (settings.zoom & ~((uint32_t)(mask) << (shift))) | ((((uint32_t)(v)) ^ ((mask) == 1u ? 1u : 1u)) & (uint32_t)(mask)) << (shift), settings_later = 1)

/* the lights (menu LIGHTS / KEYS / NOTES), for playing in the dark: kept in flash with the settings
 * (project.c persist_t.lights), read at every boot; not in .noinit, so nothing there moves. The same
 * word keeps SYNC and the REC screen's MODE / START (seq.c rec_tempo, rec_count) */
enum { LIGHTS_OFF, LIGHTS_LOW, LIGHTS_MID, LIGHTS_HIGH, LIGHTS_N };
enum { KEYS_OFF, KEYS_C, KEYS_WHITE, KEYS_ALL, KEYS_N };   /* (ALL: SLOOP 2.4; an older SLOOP reads it as OFF) */
static uint8_t lights_lvl, lights_keys;
static uint8_t lights_notes;                   /* 1: on a synth track the sounding notes light their keys */
static uint8_t lights_sync;                    /* GLO > SYSTEM > SYNC (G_SYNC), kept here: 0 INT, 1 USB, 2 TRS */
static uint8_t lights_mout;                    /* GLO > SYSTEM > MIDI (G_MIDI): 1 = the sequencer goes to MIDI OUT too */
static uint8_t lights_min;                     /* GLO > SYSTEM > IN (G_ROUTE): 0 NOTES, 1 CLOCK only, 2..17 (2.6) CH 1..16 only */
/* 2.6: GLO > MIDI CH1 CH2 CH3, GLO > DRUMS > CH, GLO > MIDI > OTHER: settings of the FM-1 (kept with the MIDI LEARN map
 * after the SYN kits: project.c midi_part), mirrored in song.g (felucca_init) */
#define MIDI_SET_DEF {1, 2, 3, 10, 0, 2}            /* CH1 CH2 CH3, drums, OTHER, (2.6) BEND */
static uint8_t midi_set[6] = MIDI_SET_DEF;
static uint8_t boot_mute;                      /* the power-on sound is off (menu BOOT SOUND): bit 31 */
static uint8_t led_fx = 1, led_sp = 13, led_fd = 3, led_hold;   /* the default: waves, speed 13, fade 3, once per press (hold: it goes on while the key is held) */
static const uint8_t LED_ORDER[6] = {1, 0, 2, 3, 4, 5};   /* the order the menu lists them in: waves, fade, explosion, rain, comet, "fireworks" */  /* KEYBOARD LED: the effect a key press makes, its speed and its fade, 0..15 (bits 16-27) */
static const uint16_t LIGHTS_NS[LIGHTS_N] = {0u, 500u, 1000u, 2000u};   /* the backlight pulse a frame (ns): a lit
                                                * LED ~95 us, the glow (landmarks) 4 us (fm1_input.h) */
static uint8_t usb_serial;                      /* menu USB SERIAL: 1 = the serial console presented (usb.c) */
static uint8_t vis_style;                       /* the visualiser's style, 0..11 (ui_vis.c) */
static uint32_t lights_word(void)
{
    return (uint32_t)lights_lvl | (uint32_t)lights_keys << 4 | (uint32_t)(lights_notes != 0u) << 8 |
           (uint32_t)(rec_tempo != 0u) << 9 | (uint32_t)(rec_count != 0u) << 10 | (uint32_t)(usb_full != 0u) << 11 |
           (uint32_t)(lights_sync % 3u) << 12 | (uint32_t)(lights_mout != 0u) << 14 | (uint32_t)(lights_min == 1u) << 15 |
           (uint32_t)(usb_serial != 0u) << 16 | (uint32_t)(vis_style % 12u) << 17 |
           (uint32_t)(lights_min >= 2u && lights_min <= 17u ? lights_min - 1u : 0u) << 21;   /* 2.6: CH n (n 1..16; 0 none) */
}
static void lights_from_word(uint32_t w)
{
    lights_lvl = (uint8_t)((w & 15u) < LIGHTS_N ? (w & 15u) : LIGHTS_OFF);
    lights_keys = (uint8_t)(((w >> 4) & 15u) < KEYS_N ? ((w >> 4) & 15u) : KEYS_OFF);
    lights_notes = (uint8_t)((w >> 8) & 1u);
    rec_tempo = (uint8_t)((w >> 9) & 1u);       /* the REC screen: MODE, START (seq.c) */
    rec_count = (uint8_t)((w >> 10) & 1u);
    usb_full = (uint8_t)((w >> 11) & 1u);       /* menu USB AUDIO (fx.c) */
    lights_sync = (uint8_t)(((w >> 12) & 3u) % 3u);
    lights_mout = (uint8_t)((w >> 14) & 1u);    /* GLO > SYSTEM > MIDI (seq.c) */
    lights_min = (uint8_t)((w >> 15) & 1u);     /* GLO > SYSTEM > IN (seq.c) */
    if (((w >> 21) & 31u) >= 1u && ((w >> 21) & 31u) <= 16u)
        lights_min = (uint8_t)(((w >> 21) & 31u) + 1u);   /* 2.6: CH n (an older SLOOP reads NOTES) */
    usb_serial = (uint8_t)((w >> 16) & 1u);
    vis_style = (uint8_t)(((w >> 17) & 15u) % 12u);   /* the visualiser (ui_vis.c); 0 in 2.3 = OSCILLOSCOPE */     /* menu USB SERIAL (usb.c usb_cdc_on, at the next start); 0 in 2.3 = OFF */
}

static void settings_save(void);              /* project.c: flash copy (FELUCCA_FLASH) */
static uint8_t settings_later;                 /* changed while playing: saved once stopped (project.c) */

/* winampFM: its menu's options kept in settings.zoom (the lights word is SLOOP 2.6's, its MIDI settings): bits 8-11 the
 * brightness, 12 TAPES AUTOPLAY off, 13-15 the KEYBOARD LED effect, 16-19 its speed, 20-23 its fade, 24 hold, 25 BOOT SOUND off,
 * 26 the LED options are set (else the defaults) */
static void wfm_opts_load(void)
{
    uint32_t z = settings.zoom;
    boot_mute = (uint8_t)((z >> 25) & 1u);
    if ((z >> 26) & 1u) {
        led_fx = (uint8_t)((z >> 13) & 7u), led_sp = (uint8_t)((z >> 16) & 15u), led_fd = (uint8_t)((z >> 20) & 15u), led_hold = (uint8_t)((z >> 24) & 1u);
        if (led_fx > 5u) led_fx = 0;
    }
}
static void wfm_opts_save(void)                  /* (menu: a change; settings_later saves it) */
{
    settings.zoom = (settings.zoom & ~(0x7FFFu << 13)) | (uint32_t)(led_fx & 7u) << 13 | (uint32_t)(led_sp & 15u) << 16 | (uint32_t)(led_fd & 15u) << 20 |
                    (uint32_t)(led_hold != 0u) << 24 | (uint32_t)(boot_mute != 0u) << 25 | 1u << 26;
}
static void settings_init(void)
{
    if (settings.magic != SETTINGS_MAGIC || settings.palette >= NPALETTES) {
        settings.magic = SETTINGS_MAGIC;
        settings.palette = 4;                  /* MONO (default) */
        settings.lowcut = 0;
        settings.zoom = 0;                     /* large readout of the touched value: off */
    }
    palette_set(settings.palette);
    fx_lowcut = (uint8_t)(settings.lowcut != 0);
    bl_lv = (uint8_t)BL_LV();
    wfm_opts_load();                           /* winampFM: KEYBOARD LED, BOOT SOUND */
}
