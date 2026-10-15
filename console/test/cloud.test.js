// Run: node console/test/cloud.test.js
// Tests the hosted-console build/flash logic against a fake Optiboot, a fake zip and a fake GitHub API.
'use strict';
const assert = require('assert');
const zlib = require('zlib');
const { execFileSync } = require('child_process');
const fs = require('fs');
const path = require('path');
const C = require('../cloud.js');

// --- Intel HEX: real hex from arduino-cli if available, else a small hand-made one
function hexRec(addr, type, data) {
  const b = [data.length, addr >> 8, addr & 255, type, ...data];
  b.push((256 - (b.reduce((s, v) => s + v, 0) & 255)) & 255);
  return ':' + b.map((v) => v.toString(16).padStart(2, '0')).join('').toUpperCase();
}
const payload = Array.from({ length: 300 }, (_, i) => (i * 7 + 3) & 255);
const hexText = [hexRec(0, 0, payload.slice(0, 16)), hexRec(16, 0, payload.slice(16, 300 > 32 ? 32 : 300)), hexRec(0, 1, [])].join('\n');
const img = C.parseHex(hexText);
assert.strictEqual(img.length, 32);
assert.deepStrictEqual(Array.from(img), payload.slice(0, 32));
assert.throws(() => C.parseHex(hexText.replace(/.$/m, '0')), /checksum/);

// --- fake Optiboot over the io interface
function fakeBootloader(opts = {}) {
  const flash = new Uint8Array(32768).fill(0xff);
  let addr = 0, out = [], inProg = false, syncAfterReset = opts.needReset ? false : true;
  const io = {
    resets: 0,
    async reset() { io.resets++; syncAfterReset = true; },
    async write(b) {
      if (!syncAfterReset) return;                       // not in bootloader yet: silence
      const c = b[0];
      if (c === 0x30 || c === 0x50 || c === 0x51) { if (c === 0x50) inProg = true; out.push(0x14, 0x10); }
      else if (c === 0x55) { addr = (b[1] | (b[2] << 8)) * 2; out.push(0x14, 0x10); }
      else if (c === 0x64) { const n = (b[1] << 8) | b[2]; assert(inProg); flash.set(b.subarray(4, 4 + n), addr); out.push(0x14, 0x10); }
      else if (c === 0x74) { const n = (b[1] << 8) | b[2]; out.push(0x14, ...flash.subarray(addr, addr + n), 0x10); }
      else throw new Error('unexpected command ' + c);
    },
    async read(n) { const r = Uint8Array.from(out.splice(0, n)); return r; },
  };
  return { io, flash };
}
(async () => {
  const image = new Uint8Array(1000).map((_, i) => (i * 13 + 5) & 255);
  let fb = fakeBootloader({ needReset: true });
  const prog = [];
  await C.flashUno(fb.io, image, (d, t) => prog.push([d, t]));
  assert.deepStrictEqual(Array.from(fb.flash.subarray(0, 1000)), Array.from(image));
  assert.strictEqual(fb.flash[1000], 0xff);
  assert(fb.io.resets >= 1);
  assert.deepStrictEqual(prog[prog.length - 1], [16, 16]);   // 8 pages written + 8 verified

  // corrupted flash must fail the verify
  fb = fakeBootloader();
  const origWrite = fb.io.write;
  fb.io.write = async (b) => { await origWrite(b); if (b[0] === 0x64) fb.flash[200] ^= 1; };
  await assert.rejects(C.flashUno(fb.io, image, null), /Verify failed/);

  // dead port
  await assert.rejects(C.flashUno({ async reset() {}, async write() {}, async read() { return new Uint8Array(0); } }, image, null, { syncTries: 2 }), /No answer/);

  // --- zip: stored and deflate entries
  function zip(entries) {
    const parts = [], cd = []; let off = 0;
    for (const [name, data, method] of entries) {
      const comp = method === 8 ? zlib.deflateRawSync(data) : data;
      const nm = Buffer.from(name);
      const lh = Buffer.alloc(30); lh.writeUInt32LE(0x04034b50, 0); lh.writeUInt16LE(method, 8); lh.writeUInt32LE(comp.length, 18); lh.writeUInt32LE(data.length, 22); lh.writeUInt16LE(nm.length, 26);
      parts.push(lh, nm, comp);
      const ch = Buffer.alloc(46); ch.writeUInt32LE(0x02014b50, 0); ch.writeUInt16LE(method, 10); ch.writeUInt32LE(comp.length, 20); ch.writeUInt32LE(data.length, 24); ch.writeUInt16LE(nm.length, 28); ch.writeUInt32LE(off, 42);
      cd.push(ch, nm); off += 30 + nm.length + comp.length;
    }
    const cdb = Buffer.concat(cd), eo = Buffer.alloc(22);
    eo.writeUInt32LE(0x06054b50, 0); eo.writeUInt16LE(entries.length, 8); eo.writeUInt16LE(entries.length, 10); eo.writeUInt32LE(cdb.length, 12); eo.writeUInt32LE(off, 16);
    return new Uint8Array(Buffer.concat([...parts, cdb, eo]));
  }
  const files = await C.unzip(zip([['firmware.hex', Buffer.from(hexText), 8], ['size.txt', Buffer.from('Sketch uses 1 bytes'), 0]]));
  assert.strictEqual(Buffer.from(files['firmware.hex']).toString(), hexText);
  assert.strictEqual(Buffer.from(files['size.txt']).toString(), 'Sketch uses 1 bytes');

  // --- fake GitHub API: dispatch -> run lookup by build_id -> poll -> artifact download
  const calls = [];
  let buildId = '', polls = 0;
  const fetchFn = async (url, opts = {}) => {
    calls.push([opts.method || 'GET', url.replace('https://api.github.com', '')]);
    assert.strictEqual(opts.headers.Authorization, 'Bearer tok');
    const ok = (body) => ({ ok: true, status: 200, json: async () => body, arrayBuffer: async () => body });
    if (url.endsWith('/dispatches')) { const b = JSON.parse(opts.body); assert.strictEqual(b.inputs.board, 'uno'); buildId = b.inputs.build_id; return { ok: true, status: 204 }; }
    if (url.includes('/runs?')) return ok({ workflow_runs: [{ id: 1, display_title: 'Firmware other' }, { id: 42, display_title: 'Firmware ' + buildId }] });
    if (url.endsWith('/runs/42')) return ok({ id: 42, status: ++polls < 3 ? 'in_progress' : 'completed', conclusion: 'success' });
    if (url.endsWith('/runs/42/artifacts')) return ok({ artifacts: [{ id: 7, name: 'firmware', expired: false }] });
    if (url.endsWith('/artifacts/7/zip')) { const z = zip([['firmware.hex', Buffer.from(hexText), 8]]); return ok(z.buffer.slice(z.byteOffset, z.byteOffset + z.length)); }
    return { ok: false, status: 404, json: async () => ({ message: 'Not Found' }) };
  };
  const run = await C.startBuild({ token: 'tok', repo: 'o/r', ref: 'main', inputs: { board: 'uno' }, fetchFn, pollMs: 0 });
  assert.strictEqual(run.id, 42);
  const done = await C.waitForRun({ token: 'tok', repo: 'o/r', runId: 42, fetchFn, pollMs: 0 });
  assert.strictEqual(done.conclusion, 'success');
  const res = await C.downloadResult({ token: 'tok', repo: 'o/r', runId: 42, fetchFn });
  assert.strictEqual(Buffer.from(res['firmware.hex']).toString(), hexText);
  await assert.rejects(C.downloadResult({ token: 'tok', repo: 'o/r', runId: 99, fetchFn }), /No access/);

  // --- a real hex from arduino-cli round-trips through the fake bootloader (skipped if no hex around)
  const real = process.env.REAL_HEX;
  if (real && fs.existsSync(real)) {
    const im = C.parseHex(fs.readFileSync(real, 'utf8'));
    const f2 = fakeBootloader();
    await C.flashUno(f2.io, im, null);
    assert.deepStrictEqual(Array.from(f2.flash.subarray(0, im.length)), Array.from(im));
    console.log('real hex ok (' + im.length + ' bytes)');
  }
  console.log('ok');
})().catch((e) => { console.error(e); process.exit(1); });
