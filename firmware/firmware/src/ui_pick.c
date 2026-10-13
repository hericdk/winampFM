/* SPDX-License-Identifier: GPL-3.0-only
 * SMK-37 port: Copyright (C) 2026 M. Geisler (genkigenki/makkuro) */
/* The pad sound picker. The 16 pads (and the screen, 8 x 2) are the sounds of one kind, in the PRESETS
 * order (ui.c BANK), 16 a page, then the user presets; on the drum track the kits. A pad loads its sound
 * (as PRESETS does) and plays a short preview; < > step the pages. KNOB BANK opens and closes it (a PRESETS
 * turn keeps winampFM's own playlist on the screen). Pad hits never reach the notes while it is up
 * (smk_input.h smk_pad_pick). */
enum { PK_USER = 8, PK_KITS = 9 };
static const char *const PICK_KIND[10] = {"BASS", "KEYS", "ORGN", "PAD", "LEAD", "PLCK", "STAB", "FX", "USER", "KITS"};
static const uint8_t PICK_RGB[10][3] = {           /* a colour a kind (pads and screen) */
    {255, 40, 40},  {255, 140, 0},  {255, 220, 0}, {40, 220, 80},  {0, 200, 255},
    {60, 80, 255},  {170, 60, 255}, {255, 60, 170}, {220, 210, 170}, {255, 98, 26}};
static uint8_t pick_pin, pick_sel = 0xFF;
static uint16_t pick_page;
static uint32_t pick_frame;                        /* the frame drawn last */
typedef struct { uint16_t first, len, sub, nsub, kind; } pick_pg_t;

static uint32_t pick_total(void)
{
    uint32_t total;
    if (is_drum(TSEL))
        return DRUM_KITS;
    preset_pos(&total);
    return total;
}
static uint32_t pick_cur(void)
{
    uint32_t total;
    return is_drum(TSEL) ? drum_kit() : preset_pos(&total);
}
static uint32_t pick_kind_of(uint32_t n)
{
    if (is_drum(TSEL))
        return n >= DRUM_USR ? PK_USER : PK_KITS;
    return n < NBANK ? BANK[n].kind : PK_USER;
}
static void pick_name(uint32_t n, char *b)        /* 13 bytes */
{
    uint32_t k, e;
    if (is_drum(TSEL)) {
        str_cpy(b, DRUM_KIT_NAMES[n % DRUM_KITS], 13);
        return;
    }
    e = preset_at(n, &k);
    if (e == NENGINES)
        up_name(k, b);
    else
        str_cpy(b, ENGINES[e]->presets[k].name, 13);
}
/* a cell's two short lines: the first two words' first 3 letters ("PLUGG BASS": PLU / BAS), one word: 3 + 3 */
static void pick_abbr(const char *s, char *l1, char *l2)
{
    const char *w2 = s, *r;
    uint32_t j;
    while (*w2 && *w2 != ' ')
        w2++;
    while (*w2 == ' ')
        w2++;
    for (j = 0; s[j] && s[j] != ' ' && j < 3u; j++)
        l1[j] = s[j];
    l1[j] = 0;
    r = *w2 ? w2 : s + j;
    for (j = 0; r[j] && r[j] != ' ' && j < 3u; j++)
        l2[j] = r[j];
    l2[j] = 0;
}

/* the pages: each kind's run of entries, 16 a page. Fills *pg for page want, *of = the page of entry n;
 * returns the number of pages */
static uint32_t pick_walk(uint32_t want, pick_pg_t *pg, uint32_t n_of, uint32_t *of)
{
    uint32_t total = pick_total(), n = 0, p = 0;
    while (n < total) {
        uint32_t k = pick_kind_of(n), end = n, nsub, i;
        while (end < total && pick_kind_of(end) == k)
            end++;
        nsub = (end - n + 15u) / 16u;
        for (i = 0; i < nsub; i++, p++) {
            uint32_t f = n + i * 16u, l = end - f > 16u ? 16u : end - f;
            if (p == want && pg) {
                pg->first = (uint16_t)f;
                pg->len = (uint16_t)l;
                pg->sub = (uint16_t)i;
                pg->nsub = (uint16_t)nsub;
                pg->kind = (uint16_t)k;
            }
            if (of && n_of >= f && n_of < f + l)
                *of = p;
        }
        n = end;
    }
    return p;
}
static void pick_home(void)                        /* to the page of the current sound */
{
    uint32_t p = 0;
    pick_walk(0xFFFFu, 0, pick_cur(), &p);
    pick_page = (uint16_t)p;
    pick_sel = song.sel;
}
static uint32_t pick_info(pick_pg_t *pg)          /* the page shown (back to the current one on a new track) */
{
    uint32_t np;
    if (pick_sel != song.sel)
        pick_home();
    np = pick_walk(pick_page, pg, 0xFFFFu, 0);
    if (np && pick_page >= np) {
        pick_page = 0;
        pick_walk(0, pg, 0xFFFFu, 0);
    }
    return np;
}

static int pick_shown(void)
{
    return pick_pin && !ui.menu && ui.layer == LY_PLAY && !ui.hold_kind;
}
static void pick_close(void)
{
    pick_pin = 0;
}
static void pick_pin_toggle(void)                  /* KNOB BANK */
{
    pick_pin = !pick_pin && !ui.menu;
    if (pick_pin)
        pick_home();
}
static void pick_turn(int32_t d)                   /* < > */
{
    pick_pg_t pg;
    uint32_t np = pick_info(&pg);
    if (np)
        pick_page = (uint16_t)((pick_page + np + (uint32_t)(d > 0 ? 1 : (int32_t)np - 1)) % np);
    ui.force = 1;
}
static void pick_select(uint32_t w)                /* white key / cell w (0..15) of the page */
{
    pick_pg_t pg;
    uint32_t n;
    if (!pick_info(&pg) || w >= pg.len)
        return;
    n = pg.first + w;
    if (is_drum(TSEL)) {
        TDRUM->p[P_E0] = (int16_t)n;
        audition_req(1u << 0 | 1u << 4, 0u);      /* kick + closed hat */
    } else {
        preset_go(n);
#if BOARD_SMK37
        pick_aud_req = (uint8_t)((pg.kind == 0u ? 48u : 60u) + 1u);   /* basses an octave down */
#endif
    }
    ui.force = 1;
}

static uint16_t pick_col(uint32_t kind, uint32_t sh)   /* the kind's colour, >> sh */
{
    const uint8_t *c = PICK_RGB[kind % 10u];
    return RGB(c[0] >> sh, c[1] >> sh, c[2] >> sh);
}
#if BOARD_SMK37
static void pick_leds(void)                         /* the current sound white, the others their kind's colour dim */
{
    pick_pg_t pg;
    uint32_t w, cur = pick_cur();
    const uint8_t *c;
    if (!pick_info(&pg))
        pg.len = 0;
    c = PICK_RGB[pg.kind % 10u];
    for (w = 0; w < 16u; w++) {
        uint32_t p = w;                            /* reading order: pads 0..7 the top row */
        if (w >= pg.len)
            padled_set(p, 0, 0, 0);
        else if (pg.first + w == cur)
            padled_set(p, 150, 150, 150);
        else
            padled_set(p, (uint8_t)(c[0] >> 2), (uint8_t)(c[1] >> 2), (uint8_t)(c[2] >> 2));
    }
}
#endif

static void pick_draw(void);                       /* the screen mirrors the pads (ui_fbank.c, in winampFM's skin) */
