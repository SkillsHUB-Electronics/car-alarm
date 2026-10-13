// Learning helpers for the Car Alarm console: class list, CSV import/export, a small MLP trainer and the
// model_data.h exporter. Plain JS with no dependencies; runs in the browser and in Node (tests).
(function (root, factory) {
  if (typeof module === 'object' && module.exports) module.exports = factory();
  else root.CarML = factory();
})(typeof self !== 'undefined' ? self : this, function () {
  'use strict';

  const N_IN = 8;
  // Class ids are part of the firmware/app protocol (REC,START,<id> and CLS,<id>,<percent>). Do not renumber.
  const CLASSES = [
    'Idle', 'Speed 40 km/h', 'Speed 80 km/h', 'Speed 100 km/h', 'Speed 120 km/h', 'Speed 140 km/h', 'Speed 160 km/h',
    'Brake slow', 'Brake medium', 'Brake hard', 'Bumpy road', 'Turn slow', 'Turn medium', 'Turn fast',
  ];
  const N_OUT = CLASSES.length;
  const FEATURES = ['std vertikal', 'std lateral', 'std maju', 'rata maju', '|rata lateral|', 'puncak vertikal', 'min maju', 'maks maju'];
  const CSV_HEADER = 'session,label_id,label,' + FEATURES.map((_, i) => 'f' + i).join(',');

  // ---------- CSV ----------
  // Row: {session, label, f: [8 ints in milli-g]}
  function toCsv(rows) {
    const out = ['# car-alarm-learn v1 (fitur dalam milli-g)', CSV_HEADER];
    for (const r of rows) out.push([csvText(r.session), r.label, csvText(CLASSES[r.label] || ''), ...r.f].join(','));
    return out.join('\n') + '\n';
  }
  function csvText(s) { return String(s).replace(/[,\r\n]/g, ' '); }

  // Tolerant: skips comments, header, blank and malformed lines; returns {rows, skipped}.
  function parseCsv(text) {
    const rows = [];
    let skipped = 0;
    for (const raw of String(text).split(/\r?\n/)) {
      const line = raw.trim();
      if (!line || line[0] === '#' || /^session\s*,/i.test(line)) continue;
      const p = line.split(',');
      if (p.length !== 3 + N_IN) { skipped++; continue; }
      const label = Number(p[1]);
      const f = p.slice(3).map(Number);
      if (!Number.isInteger(label) || label < 0 || label >= N_OUT || f.some((v) => !Number.isFinite(v))) { skipped++; continue; }
      rows.push({ session: p[0].trim() || 'import', label, f });
    }
    return { rows, skipped };
  }

  // ---------- trainer ----------
  function rng(seed) {
    let s = seed >>> 0 || 1;
    return () => { s ^= s << 13; s >>>= 0; s ^= s >>> 17; s ^= s << 5; s >>>= 0; return s / 4294967296; };
  }
  function gauss(r) { return Math.sqrt(-2 * Math.log(r() || 1e-12)) * Math.cos(2 * Math.PI * r()); }

  function standardizer(rows) {
    const mean = new Array(N_IN).fill(0), std = new Array(N_IN).fill(0);
    for (const r of rows) for (let i = 0; i < N_IN; i++) mean[i] += r.f[i] / 1000;
    for (let i = 0; i < N_IN; i++) mean[i] /= rows.length;
    for (const r of rows) for (let i = 0; i < N_IN; i++) std[i] += (r.f[i] / 1000 - mean[i]) ** 2;
    for (let i = 0; i < N_IN; i++) std[i] = Math.max(Math.sqrt(std[i] / rows.length), 0.005);  // floor: avoid dividing by ~0
    return { mean, std };
  }
  function normalize(f, norm) { return f.map((v, i) => (v / 1000 - norm.mean[i]) / norm.std[i]); }

  // Forward pass. model: {hidden, norm, W1[h][in], b1[h], W2[out][h], b2[out]}. Returns probabilities.
  function predict(model, f) {
    const x = normalize(f, model.norm), H = model.hidden;
    const h = new Array(H);
    for (let j = 0; j < H; j++) {
      let s = model.b1[j];
      for (let i = 0; i < N_IN; i++) s += model.W1[j][i] * x[i];
      h[j] = s > 0 ? s : 0;
    }
    const z = new Array(N_OUT);
    let zmax = -Infinity;
    for (let k = 0; k < N_OUT; k++) {
      let s = model.b2[k];
      for (let j = 0; j < H; j++) s += model.W2[k][j] * h[j];
      z[k] = s;
      if (s > zmax) zmax = s;
    }
    let sum = 0;
    for (let k = 0; k < N_OUT; k++) { z[k] = Math.exp(z[k] - zmax); sum += z[k]; }
    return z.map((v) => v / sum);
  }
  function argmax(a) { let b = 0; for (let i = 1; i < a.length; i++) if (a[i] > a[b]) b = i; return b; }

  function evaluate(model, rows) {
    const conf = Array.from({ length: N_OUT }, () => new Array(N_OUT).fill(0));
    let ok = 0;
    for (const r of rows) { const p = argmax(predict(model, r.f)); conf[r.label][p]++; if (p === r.label) ok++; }
    return { accuracy: rows.length ? ok / rows.length : 0, conf, n: rows.length };
  }

  // Stratified hold-out split so every class keeps validation samples (when it has >= 5 windows).
  function split(rows, valFrac, r) {
    const byClass = {};
    for (const row of rows) (byClass[row.label] = byClass[row.label] || []).push(row);
    const train = [], val = [];
    for (const k of Object.keys(byClass)) {
      const g = byClass[k];
      for (let i = g.length - 1; i > 0; i--) { const j = Math.floor(r() * (i + 1)); [g[i], g[j]] = [g[j], g[i]]; }
      const nv = g.length >= 5 ? Math.max(1, Math.round(g.length * valFrac)) : 0;
      val.push(...g.slice(0, nv));
      train.push(...g.slice(nv));
    }
    return { train, val };
  }

  // Trains a one-hidden-layer MLP (ReLU, softmax, Adam, class-weighted cross-entropy).
  // opts: hidden, epochs, lr, seed, valFrac, onProgress(epoch, loss). Async so the UI stays responsive.
  async function train(rows, opts) {
    opts = Object.assign({ hidden: 12, epochs: 300, lr: 0.01, seed: 1, valFrac: 0.2, batch: 32, onProgress: null }, opts);
    if (rows.length < 20) throw new Error('Too little data (at least 20 windows).');
    const classesPresent = new Set(rows.map((r) => r.label)).size;
    if (classesPresent < 2) throw new Error('Need data from at least 2 classes.');
    const r = rng(opts.seed), H = opts.hidden;
    const { train: tr, val } = split(rows.slice(), opts.valFrac, r);
    const norm = standardizer(tr);
    const X = tr.map((row) => normalize(row.f, norm)), Y = tr.map((row) => row.label);

    const counts = new Array(N_OUT).fill(0);
    for (const y of Y) counts[y]++;
    const cw = counts.map((c) => (c ? tr.length / (classesPresent * c) : 0));

    const rnd = (a, b, s) => Array.from({ length: a }, () => Array.from({ length: b }, () => gauss(r) * s));
    const m = { hidden: H, norm, W1: rnd(H, N_IN, Math.sqrt(2 / N_IN)), b1: new Array(H).fill(0),
                W2: rnd(N_OUT, H, Math.sqrt(2 / H) * 0.5), b2: new Array(N_OUT).fill(0) };
    // Absent classes get a strongly negative bias so they are never predicted.
    for (let k = 0; k < N_OUT; k++) if (!counts[k]) m.b2[k] = -8;

    const params = [];  // flat views for Adam: [array, owner index]
    const zeros = (a) => Array.isArray(a[0]) ? a.map((row) => row.map(() => 0)) : a.map(() => 0);
    const P = { W1: m.W1, b1: m.b1, W2: m.W2, b2: m.b2 };
    const M = {}, V = {};
    for (const k of Object.keys(P)) { M[k] = zeros(P[k]); V[k] = zeros(P[k]); }
    const b1c = 0.9, b2c = 0.999;
    let step = 0;
    const order = X.map((_, i) => i);
    let lastLoss = 0;

    for (let ep = 1; ep <= opts.epochs; ep++) {
      for (let i = order.length - 1; i > 0; i--) { const j = Math.floor(r() * (i + 1)); [order[i], order[j]] = [order[j], order[i]]; }
      let loss = 0;
      for (let s = 0; s < order.length; s += opts.batch) {
        const idx = order.slice(s, s + opts.batch);
        const G = { W1: zeros(m.W1), b1: zeros(m.b1), W2: zeros(m.W2), b2: zeros(m.b2) };
        for (const n of idx) {
          const x = X[n], y = Y[n];
          const hpre = new Array(H), h = new Array(H);
          for (let j = 0; j < H; j++) {
            let a = m.b1[j];
            for (let i = 0; i < N_IN; i++) a += m.W1[j][i] * x[i];
            hpre[j] = a; h[j] = a > 0 ? a : 0;
          }
          const z = new Array(N_OUT);
          let zmax = -Infinity;
          for (let k = 0; k < N_OUT; k++) {
            let a = m.b2[k];
            for (let j = 0; j < H; j++) a += m.W2[k][j] * h[j];
            z[k] = a; if (a > zmax) zmax = a;
          }
          let sum = 0;
          for (let k = 0; k < N_OUT; k++) { z[k] = Math.exp(z[k] - zmax); sum += z[k]; }
          const w = cw[y];
          loss += -Math.log(Math.max(z[y] / sum, 1e-9)) * w;
          const dz = z.map((v, k) => ((v / sum) - (k === y ? 1 : 0)) * w);
          const dh = new Array(H).fill(0);
          for (let k = 0; k < N_OUT; k++) {
            G.b2[k] += dz[k];
            for (let j = 0; j < H; j++) { G.W2[k][j] += dz[k] * h[j]; dh[j] += dz[k] * m.W2[k][j]; }
          }
          for (let j = 0; j < H; j++) {
            const d = hpre[j] > 0 ? dh[j] : 0;
            G.b1[j] += d;
            for (let i = 0; i < N_IN; i++) G.W1[j][i] += d * x[i];
          }
        }
        step++;
        const c1 = 1 - Math.pow(b1c, step), c2 = 1 - Math.pow(b2c, step), inv = 1 / idx.length;
        const upd = (p, g, mm, vv) => {
          const g0 = g * inv;
          mm = b1c * mm + (1 - b1c) * g0; vv = b2c * vv + (1 - b2c) * g0 * g0;
          return [p - opts.lr * (mm / c1) / (Math.sqrt(vv / c2) + 1e-8), mm, vv];
        };
        for (const key of Object.keys(P)) {
          const A = P[key];
          if (Array.isArray(A[0])) {
            for (let a = 0; a < A.length; a++) for (let b = 0; b < A[a].length; b++) {
              const u = upd(A[a][b], G[key][a][b], M[key][a][b], V[key][a][b]);
              A[a][b] = u[0]; M[key][a][b] = u[1]; V[key][a][b] = u[2];
            }
          } else {
            for (let a = 0; a < A.length; a++) {
              if (key === 'b2' && !counts[a]) continue;  // keep absent classes suppressed
              const u = upd(A[a], G[key][a], M[key][a], V[key][a]);
              A[a] = u[0]; M[key][a] = u[1]; V[key][a] = u[2];
            }
          }
        }
      }
      lastLoss = loss / X.length;
      if (opts.onProgress && (ep % 10 === 0 || ep === opts.epochs)) {
        opts.onProgress(ep, lastLoss);
        await new Promise((res) => setTimeout(res, 0));
      }
    }
    return { model: m, loss: lastLoss, train: evaluate(m, tr), val: evaluate(m, val), counts };
  }

  // ---------- export ----------
  function modelSize(hidden) {
    const weights = hidden * N_IN + hidden + N_OUT * hidden + N_OUT + 2 * N_IN;
    return { weights, flashBytes: weights * 4, sramBytes: 4 * (N_IN + hidden + N_OUT) };
  }

  function fmt(v) { const s = Number(v).toPrecision(7); return (/[.eE]/.test(s) ? s : s + '.0') + 'f'; }
  function arr(name, flat) {
    const lines = [];
    for (let i = 0; i < flat.length; i += 8) lines.push('  ' + flat.slice(i, i + 8).map(fmt).join(', '));
    return 'const float ' + name + '[] PROGMEM = {\n' + lines.join(',\n') + '\n};\n';
  }
  function toHeader(model, info) {
    const H = model.hidden;
    return [
      '// Edge-AI model for the IMU driving-state classifier. GENERATED by the web console (Learning); do not edit.',
      '// ' + (info || '') + ' ' + N_IN + ' -> ' + H + ' (ReLU) -> ' + N_OUT + ' (softmax), features in g (milli-g / 1000).',
      '#pragma once',
      '#include <avr/pgmspace.h>',
      '#define MODEL_PRESENT 1',
      '#define MODEL_N_IN ' + N_IN,
      '#define MODEL_N_HID ' + H,
      '#define MODEL_N_OUT ' + N_OUT,
      arr('MODEL_MEAN', model.norm.mean),
      arr('MODEL_STD', model.norm.std),
      arr('MODEL_W1', [].concat(...model.W1)),
      arr('MODEL_B1', model.b1),
      arr('MODEL_W2', [].concat(...model.W2)),
      arr('MODEL_B2', model.b2),
    ].join('\n');
  }

  return { N_IN, N_OUT, CLASSES, FEATURES, CSV_HEADER, toCsv, parseCsv, train, predict, evaluate, argmax, modelSize, toHeader };
});
