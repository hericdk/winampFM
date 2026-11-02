/* SPDX-License-Identifier: GPL-3.0-only
 *
 * SLOOP web emulator: the real SLOOP/FELUCCA firmware sources compiled to
 * wasm32, with a browser HAL in place of the FM-1 hardware (as tests/hostsim.c
 * does for the DSP alone, but with the whole UI: screen, layers, LEDs).
 *
 * The firmware is one compilation unit (firmware/src/felucca.c); this file
 * plays that role for the web: the HAL below replaces the fm1_*.h headers,
 * then the same sources are included in the same order. No firmware source is
 * modified.
 *
 *   audio   emu_render(n): mix_block into a float buffer (an AudioWorklet)
 *   screen  an ST7789 panel model behind lcd.c: 240x240 RGB565 framebuffer
 *   input   emu_input(notes, buttons) / emu_enc(e, steps) from JS events
 *   boot    emu_init (splash) then emu_start, emu_frame() ~60/s (ui loop)
 */
#include <stdint.h>
#define FM1_XIP(off) ((uintptr_t)(emu_flash + ((uint32_t)(off) - EMU_FLASH_BASE)))      /* the flash window the firmware reads songs and samples from: our RAM image */

#define WASM_EXPORT(n) __attribute__((export_name(n), used))

/* ------------------------------------------------------------- time HAL --- */
#define FM1_TICKS_PER_US 24u
static uint64_t emu_samples;                   /* total frames rendered */
static uint32_t fm1_ticks(void) { return (uint32_t)(emu_samples * 544u); } /* ~24 MHz */
static void fm1_delay_ms(uint32_t ms) { (void)ms; }   /* never busy-wait in a worklet */
static void fm1_wdt_feed(void) {}

/* -------------------------------------------------------------- irq HAL --- */
static uint32_t irq_save(void) { return 0; }
static void irq_restore(uint32_t f) { (void)f; }
static void fm1_irq_off(void) {}
static void fm1_irq_on(void) {}

/* ------------------------------------------------------------ input HAL --- */
/* as hal/fm1_input.h, without the matrix scan: JS writes the debounced state */
#define FM1_NCOL 11u
#define FM1_NKEY 41u              /* ids: 0..13 buttons, 14..40 note keys */
#define FM1_NENC 7u
enum { FM1_BTN_OCT_DOWN = 0, FM1_BTN_OCT_UP = 1 };
static volatile uint32_t fm1_remote_btn;   /* (SLOOP 2.7) GLO > MIDI 3 > REMOTE: buttons held over MIDI (ui_input.c remote_poll) */

static const int8_t FM1_KEYMAP[6][FM1_NCOL] = {
    {-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    { 5, 11,  4, 10,  3,  9,  2,  8, -1, -1, -1},
    {34, 35, 36, 37, 38, 40, 39, 13,  7,  6, 12},
    {23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33},
    { 0,  1, 15, 14, 17, 16, 19, 18, 20, 21, 22},
    {-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
};

static volatile struct {
    uint32_t notes;              /* bit n = note key n (0 = F3 .. 26 = G5) */
    uint32_t buttons;            /* bit i = matrix button i (0..13) */
    uint32_t pressed, released;  /* button edges since the last fm1_input_edges() */
    uint32_t notes_pressed;      /* note-key press edges */
    uint8_t raw[FM1_NCOL];
    uint8_t cnt[FM1_NKEY];
    uint8_t enc_prev[FM1_NENC], enc_last[FM1_NENC];
    uint8_t enc_rest[FM1_NENC];
    uint16_t enc_still[FM1_NENC];
    int8_t enc_sub[FM1_NENC];
    int16_t enc_steps[FM1_NENC];
    uint32_t frames;
} fm1_in;

static uint8_t fm1_led[FM1_NCOL];
static uint8_t fm1_led_dim[FM1_NCOL];
static uint8_t fm1_led_bg[FM1_NCOL];
static uint8_t fm1_led_lv[41];                 /* the brightness of each button / key LED (ui_input.c: effects, hints) */
char _etext[1]; uint32_t _data_start[1], _data_end[1], _bss_start[1], _bss_end[1], _pool_start[1], _pool_end[1], _rt_start[1], _rt_end[1];   /* (the linker's: the ABOUT page reads them) */
static volatile uint16_t fm1_led_bg_ns;

static void fm1_input_init(void) {}

static int32_t fm1_enc_take(uint32_t e)
{
    int32_t s = fm1_in.enc_steps[e];
    fm1_in.enc_steps[e] = 0;
    return s;
}

static uint32_t fm1_input_edges(uint32_t *released)
{
    uint32_t p = fm1_in.pressed;
    if (released)
        *released = fm1_in.released;
    fm1_in.pressed = fm1_in.released = 0;
    return p;
}

static uint32_t fm1_input_note_edges(void)
{
    uint32_t p = fm1_in.notes_pressed;
    fm1_in.notes_pressed = 0;
    return p;
}

static void fm1_led_key(uint32_t id, int on)
{
    uint32_t p, r;
    for (p = 0; p < FM1_NCOL; p++)
        for (r = 1; r < 5u; r++)
            if (FM1_KEYMAP[r][p] == (int8_t)id) {
                if (on)
                    fm1_led[p] |= (uint8_t)(1u << r);
                else
                    fm1_led[p] &= (uint8_t)~(1u << r);
            }
}

/* -------------------------------------------------------------- adc HAL --- */
enum { FM1_ADC_BATT = 3, FM1_ADC_MASTER = 4 };
static volatile int32_t emu_adc_v[8] = {0, 0, 0, 800, 1023, 0, 0, 0};
static int32_t fm1_adc_read(uint32_t ch) { return emu_adc_v[ch & 7u]; }
static void fm1_adc_init(void) {}

/* -------------------------------------------------- lcd HAL (ST7789 model) */
/* lcd.c speaks the real panel protocol; this consumes it into a framebuffer.
 * Only CASET (2A), RASET (2B) and RAMWR (2C) matter; init commands are
 * ignored. Pixels arrive RGB565 big-endian (gfx.c pre-swaps). */
static uint16_t emu_fb[240 * 240];
static uint32_t emu_lcd_px;
static volatile uint32_t emu_fb_gen;           /* bumped on every pixel write */
static uint32_t fm1_lcd_timeouts;
static uint8_t st_cmd;
static uint32_t st_argn;
static uint8_t st_arg[4];
static uint16_t st_x0, st_x1 = 239, st_y0, st_y1 = 239, st_cx, st_cy;

static void fm1_lcd_hw_init(void) {}
static void fm1_lcd_baud(uint32_t b) { (void)b; }
static void fm1_lcd_wait(void) {}
static void fm1_lcd_deselect(void) {}

static void fm1_lcd_send_cmd(uint8_t c)
{
    st_cmd = c;
    st_argn = 0;
    if (c == 0x2Cu) {                           /* RAMWR resets the cursor */
        st_cx = st_x0;
        st_cy = st_y0;
    }
}

static void fm1_lcd_send_data(const void *p, uint32_t n)
{
    const uint8_t *b = p;
    if (st_cmd == 0x2Au || st_cmd == 0x2Bu) {
        while (n-- && st_argn < 4u)
            st_arg[st_argn++] = *b++;
        if (st_argn == 4u) {
            uint16_t a = (uint16_t)(st_arg[0] << 8 | st_arg[1]);
            uint16_t e = (uint16_t)(st_arg[2] << 8 | st_arg[3]);
            if (st_cmd == 0x2Au) {
                st_x0 = a;
                st_x1 = e;
            } else {
                st_y0 = a;
                st_y1 = e;
            }
        }
        return;
    }
    if (st_cmd != 0x2Cu && st_cmd != 0x3Cu)
        return;
    for (; n >= 2u; n -= 2u, b += 2u) {
        if (st_cx < 240u && st_cy < 240u)
            emu_lcd_px++;   /* (tests: what goes to the LCD) */
            emu_fb[st_cy * 240u + st_cx] = (uint16_t)(b[0] << 8 | b[1]);
        if (++st_cx > st_x1) {
            st_cx = st_x0;
            if (++st_cy > st_y1)
                st_cy = st_y0;
        }
    }
    emu_fb_gen++;
}

/* ------------------------------------------------------------ audio HAL --- */
/* audio.c's ISR glue compiles against these but is never driven; the worklet
 * pulls mix_block through emu_render instead. */
#define FM1_AUDIO_HALF 0x80u
static void fm1_audio_init(int32_t *b, uint32_t n, void (*i)(void), uint32_t p)
{
    (void)b; (void)n; (void)i; (void)p;
}
static uint8_t fm1_audio_pending(void) { return 0; }
static void fm1_audio_ack_aux(uint8_t p) { (void)p; }
static uint32_t fm1_audio_free_half(void) { return 0; }
static void fm1_audio_ack_half(void) {}
static void fm1_audio_stop(void) {}
void isr_alnk0(void) {}

/* ----------------------------------------------------------- system HAL --- */
static void fm1_reboot(void) {}
static void fm1_enter_uboot(void) {}

/* ------------------------------------------------------ firmware sources --- */
#include "felucca_tables.h"
#include "libc.c"
#include "lcd.c"
#include "gfx.c"
#include "core.h"

/* the flash image (declared before engines.c: eng_sample.c reads the user
 * sample slots through SMP_USER_XIP, which on hardware is the memory-mapped
 * XIP window — here it points into the same RAM image the storage uses) */
#define EMU_FLASH_BASE 0x90000u
#define EMU_FLASH_SIZE 0x70000u
static uint8_t emu_flash[EMU_FLASH_SIZE];
static volatile uint32_t emu_flash_gen;
#define SMP_USER_XIP(k) (emu_flash + (SMP_USER_BASE - EMU_FLASH_BASE) + (k) * SMP_USER_SIZE)

#include "engines.c"
#include "drums.c"
#include "params.c"
#include "voice.c"
#include "slicer.c"
#include "cpu1.h"                        /* (SLOOP 2.7) no second core in the browser: every unit renders here (as host/preview.c) */
static int cpu1_run(cpu1_job_t fn, uint32_t arg) { (void)fn; (void)arg; return 0; }
static int cpu1_wait(void) { return 0; }
static int cpu1_on(void) { return 0; }
static uint32_t cpu1_ticks(void) { return 0; }
#include "fx.c"
#define FELUCCA_OTA 0
#define FELUCCA_CDC 0
#define FELUCCA_UAC 0
#include "usb.c"
#define FELUCCA_ARRANGER 1
#include "arranger.c"
#include "seq.c"
#include "audio.c"
#include "panel.c"
#include "ui.c"
#include "ui_song.c"
#include "ui_studio.c"
#include "icons.c"
#include "ui_draw.c"
#include "ui_layers.c"
#include "ui_menu.c"
#include "ui_winamp.c"       /* winampFM: the player screen */
#include "ui_input.c"
/* ---- flash: a RAM image of the FM-1's persisted areas ----
 * storage.c's A/B sector scheme runs unchanged on this array; JS loads a
 * saved image into it before boot and writes it back to IndexedDB whenever
 * emu_flash_gen moves, so projects, autosave, settings and user presets
 * survive a reload. Offsets are the real flash map (0x97000..0xFEFFF). */
#define FELUCCA_FLASH 1
static uint8_t flash_ok;
#define FL_FAR(f) f
static uint32_t fl_jedec_ram(void) { return 0x856014u; }   /* the expected part: flash_ok */
static void fl_plain_window_init(void) {}
static int emu_fl_ok(uint32_t off, uint32_t n)
{
    return off >= EMU_FLASH_BASE && n <= EMU_FLASH_SIZE && off - EMU_FLASH_BASE <= EMU_FLASH_SIZE - n;
}
static int st_read(uint32_t off, void *dst, uint32_t n)
{
    if (!emu_fl_ok(off, n))
        return -8;
    memcpy(dst, emu_flash + (off - EMU_FLASH_BASE), n);
    return 0;
}
static int st_erase(uint32_t off)
{
    if (!emu_fl_ok(off, 4096u))
        return -8;
    memset(emu_flash + (off - EMU_FLASH_BASE), 0xFF, 4096u);
    emu_flash_gen++;
    return 0;
}
static int st_prog(uint32_t off, const void *src, uint32_t n)
{
    if (!emu_fl_ok(off, n))
        return -8;
    memcpy(emu_flash + (off - EMU_FLASH_BASE), src, n);
    emu_flash_gen++;
    return 0;
}
#define ST_XIP_PTR(off) (emu_flash + ((uint32_t)(off) - EMU_FLASH_BASE))   /* storage.c st_xip: an object read in place */
#include "storage.c"
#include "upreset.c"
#include "project.c"
#include "splash.c"
static int ed_smp_erase(uint32_t k, uint32_t all) { (void)k; (void)all; return 0; }      /* (editor.c is not part of the emulator) */
static int ed_sng_erase(uint32_t sid) { (void)sid; return 0; }

/* ------------------------------------------------------------- web entry --- */
static float emu_audio_f[2u * 512u];
static int32_t emu_mix[2u * CTL];
static uint32_t emu_ms_acc;

/* power-on defaults, as main.c felucca_init() */
static void emu_felucca_init(void)
{
    uint32_t i;
    for (i = 0; i < G_COUNT; i++)
        song.g[i] = GP[i].def;
    for (i = 0; i < NTRK; i++) {
        track_t *t = &trk[i];
        track_defaults(t);
        if (i < NPART) {
            set_engine_of(t, TRK_DEF[i][0]);
            apply_preset_to(t, TRK_DEF[i][1]);
            t->engine = t->eng_req;
        }
        track_defaults_steps(t);
    }
    TDRUM->p[P_E0] = DRUM_DEFAULT_KIT;
    song.sel = 0;
    song.master_q12 = 2048;
    autosave_resume();
    song.g[G_SYNC] = (int16_t)lights_sync;
    layers_init();
    go_home();
    ui.force = 1;
}

WASM_EXPORT("emu_init") void emu_init(void)
{
    persist_boot();
    settings_init();
    lcd_init();
    sloop_splash();
    fm1_input_init();
    fm1_adc_init();
    panel_init();
    emu_felucca_init();
}

static uint8_t emu_running;                      /* the splash stays up until emu_start (as fm1_main shows it during init) */
WASM_EXPORT("emu_start") void emu_start(void)   /* after the splash has shown */
{
    emu_running = 1;
    wfm_jingle_start();                          /* winampFM: the opening sound (the device plays it in boot_keys) */
    lcd_fill(0, 0, 240, 240, C_BLACK);
    ui.force = 1;
}

WASM_EXPORT("emu_frame") void emu_frame(void)   /* the main loop's UI pass */
{
    {   /* master volume as the pot in fm1_main */
        uint32_t k10 = (uint32_t)fm1_adc_read(FM1_ADC_MASTER);
        song.master_q12 = (k10 * k10) >> 8;
    }
    {   /* battery: a steady healthy reading */
        int32_t b = fm1_adc_read(FM1_ADC_BATT);
        song.batt_raw = song.batt_raw ? song.batt_raw + (b - song.batt_raw) / 32 : b;
    }
    ui_input();
    ui_leds();
    if (emu_running)
        ui_draw();
    autosave_tick();
    sections_flush();
}

WASM_EXPORT("emu_render") void emu_render(uint32_t n)
{
    uint32_t k, i;
    if (n > 512u)
        n = 512u;
    for (k = 0; k + CTL <= n; k += CTL) {
        mix_block(emu_mix, CTL);
        for (i = 0; i < CTL; i++) {
            int32_t l = emu_mix[2u * i], r = emu_mix[2u * i + 1u];
            if (i & 1u)
                scope_buf[scope_w++ & (SCOPE_N - 1u)] = (int16_t)(l > 32767 ? 32767 : l < -32768 ? -32768 : l);
            emu_audio_f[2u * (k + i)] = (float)l * (1.0f / 32768.0f);
            emu_audio_f[2u * (k + i) + 1u] = (float)r * (1.0f / 32768.0f);
        }
    }
    emu_samples += n;
    emu_ms_acc += n * 1000u;
    while (emu_ms_acc >= FS) {
        emu_ms_acc -= FS;
        fm1_ms++;
    }
}

WASM_EXPORT("emu_input") void emu_input(uint32_t notes, uint32_t buttons)
{
    uint32_t pn = fm1_in.notes, pb = fm1_in.buttons;
    fm1_in.notes = notes & 0x7FFFFFFu;
    fm1_in.buttons = buttons & 0x3FFFu;
    fm1_in.notes_pressed |= notes & ~pn;
    fm1_in.pressed |= buttons & ~pb;
    fm1_in.released |= pb & ~buttons;
}

WASM_EXPORT("emu_enc") void emu_enc(uint32_t e, int32_t steps)
{
    if (e < FM1_NENC)
        fm1_in.enc_steps[e] = (int16_t)(fm1_in.enc_steps[e] + steps);
}

WASM_EXPORT("emu_adc_set") void emu_adc_set(uint32_t ch, int32_t v)
{
    emu_adc_v[ch & 7u] = v;
}

WASM_EXPORT("emu_midi") void emu_midi(uint32_t pkt)   /* a USB-MIDI event packet */
{
    midi_in_q[mi_w++ % MQ] = pkt;
}

WASM_EXPORT("emu_fb_ptr") uint32_t emu_fb_ptr(void) { return (uint32_t)(uintptr_t)emu_fb; }
WASM_EXPORT("emu_fb_gen") uint32_t emu_fb_gen_get(void) { return emu_fb_gen; }
WASM_EXPORT("emu_audio_ptr") uint32_t emu_audio_ptr(void) { return (uint32_t)(uintptr_t)emu_audio_f; }
WASM_EXPORT("emu_led_ptr") uint32_t emu_led_ptr(void) { return (uint32_t)(uintptr_t)fm1_led; }
WASM_EXPORT("emu_led_lv_ptr") uint32_t emu_led_lv_ptr(void) { return (uint32_t)(uintptr_t)fm1_led_lv; }
WASM_EXPORT("emu_led_dim_ptr") uint32_t emu_led_dim_ptr(void) { return (uint32_t)(uintptr_t)fm1_led_dim; }
WASM_EXPORT("emu_flash_ptr") uint32_t emu_flash_ptr(void) { return (uint32_t)(uintptr_t)emu_flash; }
WASM_EXPORT("emu_flash_size") uint32_t emu_flash_size(void) { return EMU_FLASH_SIZE; }
WASM_EXPORT("emu_flash_gen") uint32_t emu_flash_gen_get(void) { return emu_flash_gen; }

/* winampFM: what the simulator's status line shows: the selected track (0..3), the octave (+8), the row of its sound in the list */
WASM_EXPORT("emu_wfm_state") uint32_t emu_wfm_state(void)
{
    uint32_t tot, cur = wf_list(&tot);
    return (uint32_t)song.sel | ((uint32_t)(song.octave + 8) & 15u) << 4 | (cur & 0xFFFu) << 8 | ((uint32_t)wf_view & 3u) << 20 | (uint32_t)(ui.menu != 0) << 22 | (uint32_t)(song.playing != 0) << 23 | (uint32_t)(bl_lv & 15u) << 24;   /* (+ the brightness, 2..10: the simulator dims its screen) */   /* (+ the window, a menu up, playing: for the demos) */
}
/* the simulator's flat screen as a touch screen: what a click (kind 0), a drag starting (1), the wheel (2, d its sign) or a double click (3) at
 * x, y (screen pixels; d: +1 the wheel up, -1 down) means here, from the layout and the state; the page then presses the button or turns the knob, as a hand would.
 * Returns type << 24 | id << 16 | steps (16 bits, signed): 1 tap button id (0 OCT- .. 13 REC), 2 turn knob id (0 SELECT 1 ALGORITHM 2..5 KNOB 1..4
 * 6 PRESETS) by steps, 3 a drag that turns knob id, 4 HOME tapped steps times; 0 nothing */
#define TCH(t, id, n) ((uint32_t)(t) << 24 | (uint32_t)(id) << 16 | ((uint32_t)(int32_t)(n) & 0xFFFFu))
WASM_EXPORT("emu_touch") uint32_t emu_touch(int32_t x, int32_t y, uint32_t kind, int32_t d)
{
    static const uint8_t CYC[5] = {1, 0, 2, 4, 3};                   /* HOME's round: EFFECTS, PLAYLIST, VISUALIZER, the whole screen, TAPES */
    int tapes = ui.menu == 9, i, cur, want = -1;
    if ((ui.menu && !tapes) || ui.layer != LY_PLAY || ui.hold_kind) return 0;
    for (i = 0, cur = 0; i < 5; i++) if (CYC[i] == (tapes ? 3 : (int)wf_view)) cur = i;
    if (wf_view == 4u && !tapes) {                                     /* the whole screen: left / right the picture, twice back to the window */
        if (kind == 3u) return TCH(4, 0, 4);
        if (kind == 2u) return TCH(2, 0, d > 0 ? 1 : -1);
        if (kind == 0u) return TCH(2, 0, x < 120 ? -1 : 1);
        return 0;
    }
    if (y < WL_Y) {                                                    /* the player */
        if (y >= VB_Y && y < VB_Y + 13 && x >= MQ_X && x < MQ_X + 120) {      /* FX PL VIS and the tape: HOME round to it */
            static const uint8_t VV[4] = {1, 0, 2, 3};
            int32_t b = (x - MQ_X) / 30, n;
            for (i = 0; i < 5; i++) if (CYC[i] == VV[b]) want = i;
            n = (want - cur + 5) % 5;
            return kind == 0u && n ? TCH(4, 0, n) : 0;
        }
        if (y >= TK_Y && y < TK_Y + TK_H && x >= 5 && x < 5 + 4 * 58) {  /* a track: ALGORITHM to it (the wheel: the next one) */
            if (kind == 2u) return TCH(2, 1, d > 0 ? -1 : 1);          /* (the wheel down: the next one) */
            return kind == 0u && (x - 5) / 58 != (int32_t)song.sel ? TCH(2, 1, (x - 5) / 58 - (int32_t)song.sel) : 0;
        }
        if (x >= LCD_X && x < LCD_X + LCD_W && y >= LCD_Y && y < LCD_Y + LCD_H && kind == 0u) return TCH(1, 12, 0);   /* the time: PLAY */
        return 0;
    }
    if (tapes) {                                                       /* TAPES: the sides roll the carousel, the middle one is copied */
        if (kind == 2u) return TCH(2, 6, d > 0 ? -1 : 1);
        if (kind != 0u) return 0;
        return x < 80 ? TCH(2, 6, -1) : x >= 160 ? TCH(2, 6, 1) : TCH(1, 3, 0);
    }
    if (wf_pg) return 0;                                               /* (a page or a layer below: the hands are on the panel) */
    if (wf_view == 0u) {                                               /* PLAYLIST: a row loads its sound; the wheel / a drag scroll */
        uint32_t total, c = wf_list(&total), top = c > 3u ? c - 3u : 0u;
        if (total > PL_ROWS && top > total - PL_ROWS) top = total - PL_ROWS;
        if (kind == 2u) return TCH(2, 6, d > 0 ? -1 : 1);       /* (the wheel down: down the list) */
        if (kind == 1u) return TCH(3, 6, 0);
        if (kind != 0u || y < LO_Y || y >= LO_Y + PL_H) return 0;
        if (x >= 228) return TCH(2, 6, (int32_t)((uint32_t)(y - LO_Y) * (total - 1u) / (uint32_t)(PL_H - 1)) - (int32_t)c);   /* the scrollbar */
        {
            uint32_t n = top + (uint32_t)(y - LO_Y - 1) / PL_RH;
            return n < total && n != c ? TCH(2, 6, (int32_t)n - (int32_t)c) : 0;
        }
    }
    if (wf_view == 2u) {                                               /* VISUALIZER: the next picture; twice, the whole screen */
        if (kind == 3u) return TCH(4, 0, 1);
        if (kind == 2u) return TCH(2, 0, d > 0 ? 1 : -1);
        return kind == 0u ? TCH(2, 0, 1) : 0;
    }
    if (wf_view == 1u) {                                               /* EFFECTS: the group buttons; a slider is dragged by its knob */
        int32_t k = -1, g = -1;
        if (y >= LO_Y && y < LO_Y + 13 && x >= 5 && x < 5 + 5 * 46) {
            g = (x - 5) / 46;
            return kind == 0u && g != (int32_t)wf_grp ? TCH(2, 0, g - (int32_t)wf_grp) : 0;
        }
        if (y < EQ_TOP - 8 || y > EQ_TOP + EQ_RAIL + 14) return kind == 2u ? TCH(2, 0, d > 0 ? 1 : -1) : 0;
        for (i = 0; i < 12; i++) {
            int32_t sx = wf_grp == 3u ? (i < 4 ? 38 + i * 17 : -99) : wf_grp == 4u ? (i < 4 ? 52 + i * 50 : -99) : wf_slx((uint32_t)i);
            if (x >= sx - 6 && x <= sx + 7) { if (wf_grp >= 3u) k = i, g = (int32_t)wf_grp; else k = i % 4, g = i / 4; break; }
        }
        if (k < 0) return 0;
        if (g != (int32_t)wf_grp) return kind == 0u ? TCH(2, 0, g - (int32_t)wf_grp) : 0;   /* (another group's slider: that group first) */
        if (kind == 1u) return TCH(3, 2 + k, 0);
        if (kind == 2u) return TCH(2, 2 + k, d > 0 ? 1 : -1);
        return 0;
    }
    return 0;
}
WASM_EXPORT("emu_tp") void emu_tp(uint32_t tr, uint32_t id, int32_t v) { trk[tr % NTRK].p[id] = (int16_t)v; }   /* (tests: a track parameter) */
WASM_EXPORT("emu_tp_id") int32_t emu_tp_id(uint32_t w) { static const int16_t I[] = {P_AMODE, P_ARATE, P_AOCT, P_AGATE, P_AHOLD, P_AORDER, P_ROOT, P_SCALE, P_QUANT}; return I[w % 9u]; }
WASM_EXPORT("emu_arp_note") uint32_t emu_arp_note(uint32_t tr) { return trk[tr % NTRK].arp_note; }   /* (tests: the note the arp plays) */
WASM_EXPORT("emu_lcd_count") uint32_t emu_lcd_count(void) { return emu_lcd_px; }
WASM_EXPORT("emu_vis") void emu_vis(uint32_t m) { wf_view = 4; gv_mode = (uint8_t)m; }   /* (tests: a full-screen picture) */
WASM_EXPORT("emu_usr_demo") void emu_usr_demo(uint32_t k)   /* (tests: a one-sound USR slot, decaying bursts, as the ½ sampler sends) */
{
    uint8_t *f = (uint8_t *)SMP_USER_XIP(k), *d = f + SMP_USER_DATA;
    smp_user_hdr_t *h = (smp_user_hdr_t *)f;
    uint32_t n = 60000u, i;
    for (i = 0; i < SMP_USER_DATA; i++) f[i] = 0;
    for (i = 0; i < n / 2u; i++) {
        uint32_t ph = i % 7000u, lv = ph < 3000u ? 7u : ph < 5000u ? 3u : 1u;
        d[i] = (uint8_t)((i & 1u) ? (lv | (lv << 4)) : ((lv | 8u) | ((lv | 8u) << 4)));
    }
    h->magic = SMP_USER_MAGIC, h->version = 1, h->nz = 1, h->data_len = n / 2u;
    h->zone[0].off = 0, h->zone[0].n = n, h->zone[0].le = n - 1u, h->zone[0].rate = 32100u, h->zone[0].root16 = 60 * 16, h->zone[0].lo = 0, h->zone[0].hi = 127;
    h->crc = st_crc32(d, n / 2u);
    smp_user_scan(k);
    for (i = 0; i < NENGINES; i++) if (ENGINES[i] == &ENG_SAMPLE) TSEL->eng_req = (uint8_t)i;
    TSEL->p[P_E0] = (int16_t)(SMP_NSETS + k);
}
WASM_EXPORT("emu_bpm") uint32_t emu_bpm(void) { return (uint32_t)song.g[G_BPM]; }   /* (the video scripts: set the tempo by the knob) */
WASM_EXPORT("emu_fx") int32_t emu_fx(uint32_t w)   /* (the video scripts: 0 DJ filter, 1 delay send, 2 reverb send of the selected track, 3 feedback) */
{
    return w == 0u ? song.g[G_FILT] : w == 1u ? TSEL->p[P_DLY] : w == 2u ? TSEL->p[P_REV] : w == 3u ? song.g[G_DFDBK] : w == 4u ? TSEL->p[P_DIST] : (int32_t)wf_grp;   /* (4 dist send, 5 the slider group) */
}
/* (the video scripts) what KNOB i moves now on the player: its travel 0..1024 | the group << 12 (0 SEND 1 SPACE 2 MASTER 3 PARTY); its name */
WASM_EXPORT("emu_knob") uint32_t emu_knob(uint32_t i)
{
    int16_t *vp;
    static const uint8_t TOP[4] = {3, 3, 4, 1};
    uint32_t f;
    i &= 3u;
    if (wf_grp == 3u) { const uint8_t v[4] = {wp_form, wp_lead, wp_turn, wp_style}; f = (uint32_t)v[i] * 1024u / TOP[i]; }
    else if (wf_grp == 4u) { const param_desc_t *d = wt_par(i, &vp); f = (uint32_t)wf_frac(d, *vp); }
    else { const param_desc_t *d = wf_sl(wf_grp * 4u + i, &vp); f = (uint32_t)wf_frac(d, *vp); }
    return f | (uint32_t)wf_grp << 12;
}
WASM_EXPORT("emu_knob_name") uint32_t emu_knob_name(uint32_t i) { return (uint32_t)(uintptr_t)(wf_grp == 3u ? WP_KN[i & 3u] : wf_grp == 4u ? WT_KN[i & 3u] : WF_SL[wf_grp * 4u + (i & 3u)].shrt); }
WASM_EXPORT("emu_wfm_age") uint32_t emu_wfm_age(void) { return wft_age(); }   /* (debug: the copy animation's clock) */
