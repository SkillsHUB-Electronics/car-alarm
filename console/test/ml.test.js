// Run: node test/ml.test.js   (writes the synthetic-model header to the path in argv[2] if given)
'use strict';
const assert = require('assert');
const fs = require('fs');
const ML = require('../ml.js');

// Deterministic synthetic windows: vibration/brake/turn signatures per class, with noise.
let seed = 7;
const rnd = () => { seed = (seed * 1664525 + 1013904223) >>> 0; return seed / 4294967296; };
const noise = (s) => (rnd() + rnd() + rnd() - 1.5) * s;
function sample(label) {
  const speed = [0, 40, 80, 100, 120, 140, 160];
  let v = 20, l = 15, f = 15, mf = 0, ml = 0, pk = 60, mn = -40, mx = 40;
  if (label >= 1 && label <= 6) { const s = speed[label]; v = 30 + s * 1.2; l = 20 + s * 0.4; f = 20 + s * 0.5; pk = v * 3; mn = -f * 2; mx = f * 2; }
  if (label === 7) { mf = -120; mn = -180; }
  if (label === 8) { mf = -280; mn = -380; }
  if (label === 9) { mf = -520; mn = -700; }
  if (label === 10) { v = 300; pk = 900; }
  if (label === 11) { ml = 90; l = 40; }
  if (label === 12) { ml = 220; l = 60; }
  if (label === 13) { ml = 400; l = 90; }
  return [v + noise(10), l + noise(8), f + noise(8), mf + noise(25), Math.abs(ml + noise(20)), pk + noise(40), mn + noise(40), mx + noise(40)].map(Math.round);
}

(async () => {
  const rows = [];
  for (let c = 0; c < ML.N_OUT; c++) for (let i = 0; i < 80; i++) rows.push({ session: 's' + (i % 3), label: c, f: sample(c) });

  // CSV round trip, including a malformed line
  const csv = ML.toCsv(rows) + 'broken,line\n';
  const back = ML.parseCsv(csv);
  assert.strictEqual(back.rows.length, rows.length);
  assert.strictEqual(back.skipped, 1);
  assert.deepStrictEqual(back.rows[5].f, rows[5].f);

  const res = await ML.train(rows, { hidden: 12, epochs: 150, seed: 3 });
  console.log('val accuracy', (res.val.accuracy * 100).toFixed(1) + '%', 'train', (res.train.accuracy * 100).toFixed(1) + '%', 'loss', res.loss.toFixed(3));
  assert.ok(res.val.accuracy > 0.85, 'validation accuracy too low: ' + res.val.accuracy);

  const sz = ML.modelSize(12);
  console.log('size', JSON.stringify(sz));
  const hdr = ML.toHeader(res.model, 'synthetic test');
  assert.ok(hdr.includes('#define MODEL_N_HID 12'));
  assert.ok(!/NaN|Infinity/.test(hdr));
  if (process.argv[2]) fs.writeFileSync(process.argv[2], hdr);
  // A few reference predictions for the firmware comparison
  const ref = [0, 3, 9, 13].map((c) => { const f = sample(c); const p = ML.predict(res.model, f); return { f, cls: ML.argmax(p), pct: Math.round(100 * p[ML.argmax(p)]) }; });
  if (process.argv[3]) fs.writeFileSync(process.argv[3], JSON.stringify(ref));
  console.log('ok');
})().catch((e) => { console.error(e); process.exit(1); });
