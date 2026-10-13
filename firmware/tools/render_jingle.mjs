// winampFM: renders the opening sound straight from the firmware (sim/winampfm.wasm), as the simulator would play it, into a WAV.
//   node firmware/tools/render_jingle.mjs OUT.wav [seconds]
import { readFileSync, writeFileSync } from 'fs';
const [out, secs = '4'] = process.argv.slice(2);
const wasm = readFileSync(new URL('../../sim/winampfm.wasm', import.meta.url));
const { instance } = await WebAssembly.instantiate(wasm, {});
const e = instance.exports;
e.emu_init();
e.emu_adc_set(4, 820);                                   // the MASTER pot, as the simulator starts it
for (let i = 0; i < 30; i++) e.emu_render(128);          // (the splash moment)
e.emu_start();
const n = Math.round(44100 * +secs / 128), L = [];
for (let b = 0; b < n; b++) {
  e.emu_render(128);
  const a = new Float32Array(e.memory.buffer, e.emu_audio_ptr(), 256);
  for (let i = 0; i < 128; i++) L.push(a[2 * i], a[2 * i + 1]);
  if (b % 6 === 0) e.emu_frame();
}
let peak = 1e-9; for (const v of L) peak = Math.max(peak, Math.abs(v));
const g = Math.min(1 / peak * 0.89, 8), buf = Buffer.alloc(44 + L.length * 2);
buf.write('RIFF', 0); buf.writeUInt32LE(36 + L.length * 2, 4); buf.write('WAVEfmt ', 8); buf.writeUInt32LE(16, 16); buf.writeUInt16LE(1, 20); buf.writeUInt16LE(2, 22);
buf.writeUInt32LE(44100, 24); buf.writeUInt32LE(44100 * 4, 28); buf.writeUInt16LE(4, 32); buf.writeUInt16LE(16, 34); buf.write('data', 36); buf.writeUInt32LE(L.length * 2, 40);
L.forEach((v, i) => buf.writeInt16LE(Math.max(-32768, Math.min(32767, Math.round(v * g * 32767))), 44 + 2 * i));
writeFileSync(out, buf);
console.log(out, (L.length / 2 / 44100).toFixed(2) + ' s, peak ' + peak.toFixed(3) + ', gain ' + g.toFixed(2));
