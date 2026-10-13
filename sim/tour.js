// winampFM · tour: drives the simulator through every screen (the same presses a person would make) and, when asked, saves a PNG of each one into
// lab/capturas/ (serve.py: POST /__cap/NAME). Use it from the console after powering on:  await WFM_TOUR({ save: true })
// It is also the script for a presentation video: run it with save off and record the screen.
(function (G) {
  'use strict';
  const sl = (ms) => new Promise((r) => setTimeout(r, ms));
  G.WFM_TOUR = async function (opt) {
    opt = opt || {};
    const A = G.SIM.api, W = G.FM1W, save = !!opt.save, z = opt.zoom || 3, done = [];
    const shot = async (name, gap) => { await sl(gap || opt.gap || 650); if (save) await G.__save(name + '.png', z); done.push(name); };
    const tap = async (b, ms) => { A.btnDown(b); await sl(ms || 90); A.btnUp(b); await sl(120); };
    const hold = async (b, ms) => { A.btnDown(b); await sl(ms); };
    const knob = async (id, n, ms) => { for (let i = 0; i < Math.abs(n); i++) { A.knobTurn(id, n > 0 ? 1 : -1); await sl(ms || 35); } };
    const play = async (keys, dur) => { for (const k of keys) { A.keyPress(k, false); await sl(dur || 150); A.keyRelease(k, false); await sl(40); } };
    const chord = async (keys, ms) => { keys.forEach((k) => A.keyPress(k, false)); await sl(ms || 300); };
    const unchord = (keys) => keys.forEach((k) => A.keyRelease(k, false));
    if (!W.on) throw new Error('ligue o simulador primeiro');
    // the player and its three windows
    await tap('home');                                                       // EFFECTS (the first window) -> PLAYLIST
    await tap('play'); await play([0, 2, 4, 7, 4, 2]); await chord([0, 4, 7], 200); await shot('02-player-playlist'); unchord([0, 4, 7]);
    await knob('algo', 1); await knob('presets', 6, 60); await play([0, 4, 7]); await shot('03-playlist-faixa2');
    await knob('k4', 6); await knob('k3', 3); await play([2, 5]); await shot('04-effects-send');
    await knob('select', 1); await knob('k1', 2); await play([2, 5]); await shot('05-effects-space');
    await knob('select', 1); await knob('k1', -3); await shot('06-effects-master');
    await knob('k1', 3);
    await tap('home'); await tap('home');                                    // EFFECTS -> PLAYLIST -> VISUALIZER
    const vis = ['07-vis-scope', '08-vis-bars', '09-vis-milk', '10-vis-rays', '10b-vis-tunnel'];
    for (let i = 0; i < 5; i++) { await play([2, 7, 11], 90); await chord([0, 4, 9], 60); await shot(vis[i], 350); unchord([0, 4, 9]); await knob('select', 1); await sl(200); }
    await tap('home'); await sl(900); await shot('40-tapes');                // VISUALIZER -> TAPES
    await knob('presets', 3, 60); await sl(1300); await shot('41-tapes-ouvindo', 100);
    A.btnDown('sel'); await sl(60); A.btnUp('sel'); { const t0 = performance.now(); for (const [t, n] of [[480, '42a-copia'], [700, '42b-copia'], [900, '42c-copia'], [1400, '42d-copia'], [2300, '42e-copia']]) { while (performance.now() - t0 < t) await sl(5); await shot(n, 1); } }
    await tap('home'); await tap('home');                                    // TAPES -> EFFECTS -> PLAYLIST
    await tap('play'); await sl(300);
    // recording: arm, play, close the loop
    await knob('algo', -1);
    await tap('rec'); await shot('11-rec-armado');
    A.keyPress(0, false); await sl(200); A.keyRelease(0, false); await play([2, 4, 7, 4], 200); await shot('12-gravando');
    await play([2, 0], 200); await tap('rec'); await sl(1200); await shot('13-loop-gravado');
    // pages: each button, the same window
    const pages = [['env', '14-pagina-env'], ['lfo', '15-pagina-lfo'], ['edit', '16-pagina-edit'], ['glo', '17-pagina-glo'], ['arp', '18-pagina-arp']];
    for (const [b, n] of pages) { await tap(b); await knob('k2', 3); await shot(n); }
    await tap('home');
    // layers: hold a button, press keys
    const layers = [['fx', '19-camada-fx', [4]], ['glo', '20-camada-mix', []], ['sel', '21-camada-escala', []], ['seq', '22-camada-steps', []], ['edit', '23-camada-erase', []]];
    for (const [b, n, keys] of layers) { await hold(b, 650); for (const k of keys) A.keyPress(k, false); await shot(n); for (const k of keys) A.keyRelease(k, false); A.btnUp(b); await sl(500); }
    // the menu (HOME held)
    await hold('home', 900); A.btnUp('home'); await knob('presets', 2, 120); await shot('24-menu-preferences'); await hold('home', 900); A.btnUp('home'); await sl(400);
    // SEQ: the step page and the pattern; SAVE: the song chain; REC held: clear
    await tap('seq'); await knob('k1', 2); await shot('25-step'); await tap('seq'); await shot('26-pattern'); await tap('home');
    await tap('save'); await shot('27-song'); await tap('home');
    await hold('rec', 1500); await shot('28-segurar-rec', 100); A.btnUp('rec'); await sl(600);
    // the drum track: its kits in the playlist, the grid, the kit
    await knob('algo', -4, 120); await shot('29-playlist-bateria');
    await tap('seq'); await play([0, 2, 0, 4], 120); await shot('30-bateria-grid'); await tap('seq'); A.keyPress(2, false); await shot('31-bateria-kit', 120); A.keyRelease(2, false); await tap('home');
    // the menu's own screens
    await hold('home', 900); A.btnUp('home'); await sl(300);
    await tap('octp'); await shot('32-menu-led'); await tap('octm');
    await knob('presets', 1, 150); await tap('octp'); await shot('33-menu-importer'); await tap('octm');
    await knob('presets', 4, 150); await tap('octp'); await shot('34-menu-about'); await knob('presets', 1); await shot('35-menu-creditos'); await tap('octm');
    await hold('home', 900); A.btnUp('home'); await sl(400);
    // SAVE from a page: PRESETS, USER, PROJECT, TOOLS
    await tap('env'); const sv = ['36-save-presets', '37-save-user', '38-save-project', '39-save-tools'];
    for (const n of sv) { await tap('save'); await shot(n); }
    await tap('home');
    await tap('play');
    return done;
  };
})(window);
