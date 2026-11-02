/* SPDX-License-Identifier: GPL-3.0-only */
/* winampFM: the SLOOP web editor's SysEx left out (WFM_EDITOR 0, felucca.c; editor.c is ~16 KB of flash and 2 KB of RAM). What stays:
 * its frames are taken off the queue and dropped (so the OTA updater behind them is never blocked), and the importer's ERASE of a
 * user sample slot (ui_menu.c). Samples already in the user slots keep playing; uploading new ones, backups and the FM6 / SYN kit
 * editing go through a firmware that has the editor (SLOOP). */
#define ED_HDR0 0x7D
#define ED_HDR1 0x46
#define ED_HDR2 0x4C
static void ed_smp_inval(uint32_t k)
{
    fm1_irq_off();
    fl_inval(SMP_USER_OFF(k), SMP_USER_SIZE);
    fm1_irq_on();
}
static int ed_smp_erase(uint32_t k, uint32_t all)  /* header sector, or the whole slot */
{
    uint32_t i, took;
    int rc = 0;
    usr_nz[k] = 0;
    for (i = 0; i < 16u; i++)
        usr_zone[k][i].n = 0;                     /* a sounding voice ends instead of reading 0xFF */
    for (i = 0; i < (all ? SMP_USER_SIZE / 0x1000u : 1u) && !rc; i++) {
        rc = fl_erase4k_quiet(SMP_USER_OFF(k) + i * 0x1000u, &took);
        fm1_wdt_feed();
    }
    ed_smp_inval(k);
    return rc;
}
static void ed_service(void)                       /* an editor frame: dropped */
{
    const uint8_t *p;
    uint32_t n;
    if (ota_frame_get(&p, &n) && n >= 3u && p[0] == ED_HDR0 && p[1] == ED_HDR1 && p[2] == ED_HDR2)
        ota_frame_done();
}
