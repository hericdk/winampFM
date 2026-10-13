// winampFM · TUTORIAL: the simulator shows how to use it, playing by itself (the real firmware, real sound), one step at a time, or all of them.
// Pressing a step again first undoes what it did (the effects back where they were, the track it records cleared): nothing doubles.
// window.WFM_DEMO(name) / WFM_DEMO_STOP() from the console too.
(function (G) {
  'use strict';
  const sl = (ms) => new Promise((r) => setTimeout(r, ms));
  const EN = G.WFM_LANG === 'en';
  let run = 0, busy = null;                                         // a stop bumps run; every step checks it
  const live = (id) => { if (id !== run) throw new Error('stopped'); };
  const FX = { filt: 0, dly: 1, rev: 2, fdbk: 3, dist: 4, grp: 5 };
  function api(id) {
    const A = G.SIM.api, W = G.FM1W;
    const w = async (ms) => { await sl(ms); live(id); };
    const tap = async (b, ms) => { A.btnDown(b); await w(ms || 90); A.btnUp(b); await w(260); };
    const hold = async (b, ms) => { A.btnDown(b); await w(ms); A.btnUp(b); await w(200); };
    const knob = async (k, n, ms) => { for (let i = 0; i < Math.abs(n); i++) { A.knobTurn(k, n > 0 ? 1 : -1); await w(ms || 45); } };
    const fx = (k) => (W.fx ? W.fx[FX[k]] : 0);
    const to = async (k, name, v, ms) => {                           // turn a knob until the effect reads v (the knobs accelerate: counted turns drift)
      for (let i = 0; i < 260 && fx(name) !== v; i++) { const d = v - fx(name); A.knobTurn(k, d > 0 ? 1 : -1); await w(Math.abs(d) < 8 ? 120 : ms || 40); }
    };
    const note = async (k, ms, gap) => { A.keyPress(k, false); await w(ms || 160); A.keyRelease(k, false); await w(gap == null ? 40 : gap); };
    const play = async (keys, ms, gap) => { for (const k of keys) await note(k, ms, gap); };
    const chord = async (keys, ms) => { keys.forEach((k) => A.keyPress(k, false)); await w(ms); keys.forEach((k) => A.keyRelease(k, false)); };
    const st = () => W.st | 0;                                      // the firmware's state: window (0 PL 1 FX 2 VIS 3 TAPES), a menu up, playing
    const vw = () => (st() >> 20) & 3, menu = () => (st() >> 22) & 1, playing = () => (st() >> 23) & 1, sel = () => st() & 15, row = () => (st() >> 8) & 4095;
    const view = async (want) => {
      if (want === 1 && vw() === 0 && !menu()) { await knob('select', 1, 120); await w(250); return; }   // (PLAYLIST -> EFFECTS: SELECT, not HOME)
      for (let i = 0; i < 6 && (vw() !== want || (menu() && want !== 3)); i++) await tap('home', 90); await w(250);
    };
    const group = async (g) => { await view(1); for (let i = 0; i < 6 && fx('grp') !== g; i++) await knob('select', fx('grp') < g ? 1 : -1, 110); };
    const track = async (t) => { for (let i = 0; i < 6 && sel() !== t; i++) await knob('algo', sel() < t ? 1 : -1, 120); };
    const stop = async () => { if (playing()) await tap('play'); };
    const start = async () => { if (!playing()) await tap('play'); };
    return { A, W, w, tap, hold, knob, to, fx, note, play, chord, view, group, track, stop, start, sel, row };
  }
  const say = (t) => { const c = document.getElementById('demo-cap'); if (c) c.textContent = t; };
  const T = (en, pt) => (EN ? en : pt);
  const MEL = [0, 4, 7, 9, 7, 4, 2, 4, 0, 2, 4, 7, 11, 9, 7, 4];
  let snap = null;                                                   // the screen step: how the effects and the sound were before it touched them
  async function partyOff(d) { await d.group(3); await d.knob('k1', 4, 25); await d.knob('k3', -5, 25); await d.knob('k4', -2, 25); }   // (PARTY all, AUTOTURN off, KEEP: the ends, exact)
  async function restore(d) {
    if (!snap) return;
    await d.track(snap.sel);
    await partyOff(d);
    await d.group(0); await d.to('k1', 'dist', snap.dist); await d.to('k3', 'dly', snap.dly); await d.to('k4', 'rev', snap.rev);
    await d.group(1); await d.to('k2', 'fdbk', snap.fdbk);
    await d.group(2); await d.to('k1', 'filt', snap.filt);
    for (let i = 0; i < 80 && d.row() !== snap.row; i++) await d.knob('presets', d.row() < snap.row ? 1 : -1, 60);
    await d.group(0);
    snap = null;
  }
  const DEMOS = {
    async player(id) {
      const d = api(id);
      await d.stop(); await d.view(1);
      await restore(d);                                              // (pressed again: back where it started first)
      snap = { sel: d.sel(), row: d.row(), dist: d.fx('dist'), dly: d.fx('dly'), rev: d.fx('rev'), fdbk: d.fx('fdbk'), filt: d.fx('filt') };
      say(T('The screen: the sound of the track in the marquee, the time, the real spectrum; below, the EFFECTS window.', 'A tela: o som da faixa no letreiro, o tempo, o espectro real; embaixo, a janela EFFECTS.'));
      await d.start(); await d.chord([0, 4, 7], 900); await d.play([0, 4, 7, 11], 140);
      say(T('PRESETS changes the sound (the PLAYLIST opens by itself)', 'PRESETS muda o som (a PLAYLIST abre sozinha)')); await d.knob('presets', 5, 160); await d.play([0, 4, 7, 9], 160);
      say(T('KNOB 1–4 move the effect sliders (EFFECTS comes back)', 'KNOB 1–4 movem os sliders de efeito (a janela EFFECTS volta)')); await d.group(0); await d.to('k1', 'dist', Math.min(127, snap.dist + 24), 50); await d.play([0, 2, 4], 160);
      say(T('DELAY: the line gets echoes that cross', 'DELAY: a linha ganha ecos que se cruzam')); await d.to('k3', 'dly', 96, 30); await d.group(1); await d.to('k2', 'fdbk', 100, 30);
      await d.play(MEL.slice(0, 8), 150);
      say(T('MASTER: the DJ filter', 'MASTER: o filtro DJ')); await d.group(2); await d.to('k1', 'filt', -40, 50); await d.play([0, 4, 7], 200); await d.to('k1', 'filt', 0, 30);
      await d.stop();
      say(T('Back as it was.', 'De volta como estava.')); await restore(d);
    },
    async record(id) {
      const d = api(id);
      await d.stop(); await d.view(1); await d.track(0);
      say(T('Recording a loop. First the track is cleared (REC held), so nothing doubles.', 'Gravar um loop. Primeiro a faixa é limpa (REC segurado), para nada dobrar.'));
      await d.hold('rec', 1700); await d.w(500);
      say(T('REC arms it: the recorder appears below and the bolt becomes 4 LEDs', 'REC arma: o gravador aparece embaixo e o raio vira 4 LEDs'));
      await d.tap('rec'); await d.w(1200);
      say(T('The first note starts the take: the reels turn, the LEDs tick the beat', 'A primeira nota começa a gravar: os carretéis giram, os LEDs marcam o tempo'));
      await d.play(MEL, 170);
      say(T('REC again closes the loop: the firmware finds the tempo and the loop plays', 'REC de novo fecha o loop: o firmware acha o BPM e o loop toca'));
      await d.tap('rec'); await d.w(3500);
      say(T('PARTY (after MASTER): SOLO with AUTOTURN and YOYO, the stage walks over the tracks', 'PARTY (depois de MASTER): SOLO com AUTOTURN e YOYO, o palco anda pelas faixas'));
      await d.group(3); await d.knob('k1', -4, 120); await d.knob('k3', 1, 120); await d.knob('k4', 1, 120); await d.w(5000);
      await partyOff(d); await d.group(0);
      say(T('The seek bar shows the recorded notes', 'A barra de posição mostra as notas gravadas')); await d.w(2000);
      await d.stop();
    },
    async tapes(id) {
      const d = api(id);
      await d.stop(); await d.view(1); await d.track(1);
      say(T('TAPES: hold PLAY (or HOME to the tape). Every sound is a tape.', 'TAPES: segure PLAY (ou HOME até a fita). Cada som é uma fita.'));
      await d.hold('play', 1000); await d.w(800);
      say(T('PRESETS spins the carousel; when it stops, the tape plays and its reels turn', 'PRESETS gira o carrossel; parado, a fita toca e os carretéis giram'));
      await d.knob('presets', 3, 300); await d.w(2200); await d.knob('presets', 4, 300); await d.w(2200);
      say(T('SEL copies the sound and its phrase into the track, then back to EFFECTS', 'SEL copia o som e a frase para a faixa e volta para EFFECTS'));
      await d.tap('sel'); await d.w(3800);
      await d.view(1); await d.stop();
    },
    async visual(id) {
      const d = api(id);
      say(T('VISUALIZER: HOME changes the window; SELECT changes the picture', 'VISUALIZER: HOME troca a janela; SELECT troca a imagem'));
      await d.view(2); await d.start();
      const names = ['SCOPE', 'BARS', 'MILK', 'RAYS', 'TUNNEL'];
      for (let v = 0; v < 5; v++) { say('VISUALIZER: ' + names[v]); for (let r = 0; r < 2; r++) { await d.chord([0, 4, 9], 300); await d.play([2, 7, 11, 7], 120); } if (v < 4) await d.knob('select', 1, 80); }
      await d.knob('select', -4, 30); await d.view(1); await d.stop();
    },
  };
  DEMOS.all = async (id) => { for (const k of ['player', 'record', 'tapes', 'visual']) { await DEMOS[k](id); await sl(600); live(id); } };
  function releaseAll() {                                            // nothing left held: the keys, the buttons
    const A = G.SIM && G.SIM.api; if (!A) return;
    for (let k = 0; k < 16; k++) A.keyRelease(k, false);
    for (let k = 0; k < 11; k++) A.keyRelease(k, true);
    ['play', 'rec', 'sel', 'home', 'fx', 'glo', 'seq', 'save'].forEach((b) => A.btnUp(b));
  }
  const mark = () => {
    document.querySelectorAll('[data-demo]').forEach((b) => b.classList.toggle('on', b.dataset.demo === busy));
    const all = document.getElementById('demo-all'); if (all) { all.classList.toggle('on', !!busy); all.innerHTML = busy ? '■ ' + T('STOP', 'PARAR') : '▶ ' + T('PLAY ALL', 'TOCAR TUDO'); }
  };
  G.WFM_DEMO = async (name) => {
    if (!G.FM1W.on) { await G.FM1W.start(); await sl(2600); }
    const id = ++run; releaseAll(); await sl(220);                    // (a step pressed again stops the one running first)
    if (id !== run) return;
    busy = name; mark();
    try { await DEMOS[name](id); say(T('Done. Pick another step, or play it yourself.', 'Pronto. Escolha outro passo ou toque você mesmo.')); } catch (e) { if (e.message !== 'stopped') throw e; }
    if (id === run) { busy = null; mark(); }
  };
  G.WFM_DEMO_STOP = () => { run++; busy = null; releaseAll(); mark(); say(T('Stopped.', 'Parado.')); };
  addEventListener('DOMContentLoaded', () => {
    const bar = document.getElementById('demos'); if (!bar) return;
    mark();
    bar.addEventListener('click', (e) => {
      const b = e.target.closest('[data-demo]'); if (b) { G.WFM_DEMO(b.dataset.demo); return; }
      if (e.target.closest('#demo-all')) { if (busy) G.WFM_DEMO_STOP(); else G.WFM_DEMO('all'); }
    });
    // the volume: a little knob (drag up / down, the wheel, the arrows) on the firmware's MASTER, and the level in growing bars
    const vk = document.getElementById('vol-k'), vm = document.getElementById('vol-m');
    if (vk) {
      const W = G.FM1W, show = () => { const f = (W.master || 0) / 1023; vk.style.setProperty('--a', (-135 + 270 * f) + 'deg'); vk.setAttribute('aria-valuenow', Math.round(f * 100)); };
      const setv = (v) => { if (W.setMaster) W.setMaster(v); else W.master = v; show(); };
      let y0 = null, v0 = 0;
      vk.addEventListener('pointerdown', (e) => { vk.setPointerCapture(e.pointerId); y0 = e.clientY; v0 = W.master || 0; e.preventDefault(); });
      vk.addEventListener('pointermove', (e) => { if (y0 != null) setv(Math.max(0, Math.min(1023, v0 + (y0 - e.clientY) * 8))); });
      vk.addEventListener('pointerup', () => { y0 = null; });
      vk.addEventListener('wheel', (e) => { e.preventDefault(); setv(Math.max(0, Math.min(1023, (W.master || 0) + (e.deltaY < 0 ? 48 : -48)))); }, { passive: false });
      vk.addEventListener('keydown', (e) => { const d = e.key === 'ArrowUp' || e.key === 'ArrowRight' ? 48 : e.key === 'ArrowDown' || e.key === 'ArrowLeft' ? -48 : 0; if (d) { e.preventDefault(); setv(Math.max(0, Math.min(1023, (W.master || 0) + d))); } });
      show(); setInterval(show, 500);
    }
    if (vm) {
      const x = vm.getContext('2d'); let buf = null, lv = 0;
      const tick = () => {
        requestAnimationFrame(tick);
        const an = G.FM1W.an; let m = 0;
        if (an) { buf = buf || new Uint8Array(an.fftSize); an.getByteTimeDomainData(buf); for (let i = 0; i < buf.length; i++) m = Math.max(m, Math.abs(buf[i] - 128)); }
        lv = Math.max(m / 128, lv * 0.9);
        const n = Math.round(Math.min(1, lv * 1.6) * 8); x.clearRect(0, 0, 48, 16);
        for (let i = 0; i < 8; i++) { const h = 3 + i * 1.6; x.fillStyle = i < n ? (i > 5 ? '#f07818' : '#1ce61c') : '#2a2a3a'; x.fillRect(i * 6, 16 - h, 4, h); }
      };
      tick();
    }
  });
})(window);
