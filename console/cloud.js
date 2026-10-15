// Cloud build + browser flashing for the hosted console (GitHub Pages).
// GitHub Pages cannot run a compiler, so the console asks GitHub Actions to compile
// (.github/workflows/build-firmware.yml), downloads the result and flashes it over Web Serial.
// No dependencies; esptool-js is loaded from a CDN only when an ESP32 is flashed.
// Works as a browser global (CloudBuild) and as a CommonJS module for the tests.
(function (root) {
  'use strict';

  const API = 'https://api.github.com';
  const WORKFLOW = 'build-firmware.yml';
  const ESPTOOL_URL = 'https://unpkg.com/esptool-js@0.5.7/bundle.js';
  const sleep = (ms) => new Promise((r) => setTimeout(r, ms));

  // ---------- GitHub API ----------
  function headers(token) {
    return { Authorization: 'Bearer ' + token, Accept: 'application/vnd.github+json', 'X-GitHub-Api-Version': '2022-11-28' };
  }

  async function gh(token, path, opts, fetchFn) {
    const f = fetchFn || fetch;
    const r = await f(API + path, Object.assign({ headers: headers(token) }, opts));
    if (!r.ok) {
      let msg = '';
      try { msg = (await r.json()).message || ''; } catch (e) { /* not json */ }
      if (r.status === 401) msg = 'Token rejected (expired or wrong). ' + msg;
      else if (r.status === 403 || r.status === 404) msg = 'No access (check the repository name and that the token has Actions read & write for it). ' + msg;
      throw new Error('GitHub ' + r.status + ': ' + msg.trim());
    }
    return r;
  }

  // Dispatches the workflow; resolves to the run once GitHub lists it.
  // The dispatch call returns no run id, so the console puts a unique build_id into the run name.
  async function startBuild(o) {
    const { token, repo, ref, inputs, fetchFn } = o;
    const buildId = 'b' + Date.now().toString(36) + Math.random().toString(36).slice(2, 6);
    await gh(token, '/repos/' + repo + '/actions/workflows/' + WORKFLOW + '/dispatches',
      { method: 'POST', body: JSON.stringify({ ref: ref || 'main', inputs: Object.assign({}, inputs, { build_id: buildId }) }) }, fetchFn);
    for (let i = 0; i < 30; i++) {
      await sleep(o.pollMs === undefined ? 2000 : o.pollMs);
      const r = await gh(token, '/repos/' + repo + '/actions/workflows/' + WORKFLOW + '/runs?event=workflow_dispatch&per_page=20', {}, fetchFn);
      const run = (await r.json()).workflow_runs.find((x) => (x.display_title || x.name || '').includes(buildId));
      if (run) return run;
    }
    throw new Error('The build was requested but GitHub did not list the run. Open the Actions tab of the repository.');
  }

  async function waitForRun(o) {
    const { token, repo, runId, onStatus, fetchFn } = o;
    for (;;) {
      const run = await (await gh(token, '/repos/' + repo + '/actions/runs/' + runId, {}, fetchFn)).json();
      if (onStatus) onStatus(run);
      if (run.status === 'completed') return run;
      await sleep(o.pollMs === undefined ? 4000 : o.pollMs);
    }
  }

  // Returns {files: {name: Uint8Array}} of the "firmware" artifact.
  async function downloadResult(o) {
    const { token, repo, runId, fetchFn } = o;
    const list = await (await gh(token, '/repos/' + repo + '/actions/runs/' + runId + '/artifacts', {}, fetchFn)).json();
    const art = list.artifacts.find((a) => a.name === 'firmware' && !a.expired);
    if (!art) throw new Error('The run has no firmware artifact (build failed or artifact expired).');
    const r = await gh(token, '/repos/' + repo + '/actions/artifacts/' + art.id + '/zip', {}, fetchFn);
    return unzip(new Uint8Array(await r.arrayBuffer()));
  }

  // ---------- minimal ZIP reader (stored + deflate) ----------
  async function inflateRaw(bytes) {
    const ds = new DecompressionStream('deflate-raw');
    const stream = new Blob([bytes]).stream().pipeThrough(ds);
    return new Uint8Array(await new Response(stream).arrayBuffer());
  }

  async function unzip(buf) {
    const dv = new DataView(buf.buffer, buf.byteOffset, buf.byteLength);
    let eocd = -1;
    for (let i = buf.length - 22; i >= Math.max(0, buf.length - 65557); i--) {
      if (dv.getUint32(i, true) === 0x06054b50) { eocd = i; break; }
    }
    if (eocd < 0) throw new Error('Not a zip file');
    const count = dv.getUint16(eocd + 10, true);
    let p = dv.getUint32(eocd + 16, true);
    const files = {};
    for (let n = 0; n < count; n++) {
      if (dv.getUint32(p, true) !== 0x02014b50) throw new Error('Corrupt zip directory');
      const method = dv.getUint16(p + 10, true);
      const csize = dv.getUint32(p + 20, true);
      const nameLen = dv.getUint16(p + 28, true), extraLen = dv.getUint16(p + 30, true), commentLen = dv.getUint16(p + 32, true);
      const local = dv.getUint32(p + 42, true);
      const name = new TextDecoder().decode(buf.subarray(p + 46, p + 46 + nameLen));
      p += 46 + nameLen + extraLen + commentLen;
      if (name.endsWith('/')) continue;
      const start = local + 30 + dv.getUint16(local + 26, true) + dv.getUint16(local + 28, true);
      const raw = buf.subarray(start, start + csize);
      if (method === 0) files[name] = raw.slice();
      else if (method === 8) files[name] = await inflateRaw(raw);
      else throw new Error('Unsupported zip method ' + method);
    }
    return files;
  }

  // ---------- Intel HEX ----------
  // Returns a Uint8Array covering address 0..highest byte (gaps are 0xFF).
  function parseHex(text) {
    const chunks = [];
    let base = 0, max = 0;
    for (const raw of text.split(/\r?\n/)) {
      const line = raw.trim();
      if (!line) continue;
      if (line[0] !== ':') throw new Error('Bad hex line: ' + line.slice(0, 20));
      const b = [];
      for (let i = 1; i + 1 < line.length; i += 2) b.push(parseInt(line.substr(i, 2), 16));
      const len = b[0], addr = (b[1] << 8) | b[2], type = b[3];
      if (b.length !== len + 5 || b.some(Number.isNaN)) throw new Error('Bad hex record length');
      if (b.reduce((s, v) => s + v, 0) & 0xff) throw new Error('Hex checksum error');
      if (type === 0) { chunks.push([base + addr, b.slice(4, 4 + len)]); max = Math.max(max, base + addr + len); }
      else if (type === 1) break;
      else if (type === 2) base = ((b[4] << 8) | b[5]) << 4;
      else if (type === 4) base = ((b[4] << 8) | b[5]) << 16;
    }
    const out = new Uint8Array(max).fill(0xff);
    for (const [a, d] of chunks) out.set(d, a);
    return out;
  }

  // ---------- STK500v1 (Arduino UNO / Optiboot) ----------
  // io = { write(Uint8Array), read(n, timeoutMs) -> Uint8Array|null, reset() }
  const STK = { OK: 0x10, INSYNC: 0x14, SYNC: 0x30, ENTER: 0x50, LEAVE: 0x51, LOAD_ADDR: 0x55, PROG_PAGE: 0x64, READ_PAGE: 0x74, EOP: 0x20 };

  async function stkCmd(io, bytes, replyLen, timeoutMs) {
    await io.write(Uint8Array.from(bytes));
    const r = await io.read(replyLen + 2, timeoutMs || 1000);
    if (!r || r.length < replyLen + 2) throw new Error('Bootloader did not answer');
    if (r[0] !== STK.INSYNC || r[r.length - 1] !== STK.OK) throw new Error('Bootloader out of sync');
    return r.subarray(1, 1 + replyLen);
  }

  async function flashUno(io, image, onProgress, opts) {
    const PAGE = 128;                                      // ATmega328P flash page
    const o = Object.assign({ verify: true, syncTries: 10 }, opts);
    let synced = false;
    for (let i = 0; i < o.syncTries && !synced; i++) {
      if (i % 3 === 0) await io.reset();                   // pulse DTR/RTS: restart into the bootloader
      try { await stkCmd(io, [STK.SYNC, STK.EOP], 0, 300); synced = true; } catch (e) { /* retry */ }
    }
    if (!synced) throw new Error('No answer from the bootloader. Check the board is a UNO on this port and that no other program (Serial Monitor, Arduino IDE) uses it.');
    await stkCmd(io, [STK.ENTER, STK.EOP], 0);
    const pages = Math.ceil(image.length / PAGE);
    for (let p = 0; p < pages; p++) {
      const data = new Uint8Array(PAGE).fill(0xff);
      data.set(image.subarray(p * PAGE, (p + 1) * PAGE));
      const word = (p * PAGE) >> 1;
      await stkCmd(io, [STK.LOAD_ADDR, word & 0xff, word >> 8, STK.EOP], 0);
      await stkCmd(io, [STK.PROG_PAGE, 0, PAGE, 0x46, ...data, STK.EOP], 0, 2000);
      if (onProgress) onProgress(p + 1, pages * (o.verify ? 2 : 1));
    }
    if (o.verify) {
      for (let p = 0; p < pages; p++) {
        const word = (p * PAGE) >> 1;
        await stkCmd(io, [STK.LOAD_ADDR, word & 0xff, word >> 8, STK.EOP], 0);
        const got = await stkCmd(io, [STK.READ_PAGE, 0, PAGE, 0x46, STK.EOP], PAGE, 2000);
        const want = new Uint8Array(PAGE).fill(0xff);
        want.set(image.subarray(p * PAGE, (p + 1) * PAGE));
        for (let i = 0; i < PAGE; i++) if (got[i] !== want[i]) throw new Error('Verify failed at 0x' + (p * PAGE + i).toString(16));
        if (onProgress) onProgress(pages + p + 1, pages * 2);
      }
    }
    await stkCmd(io, [STK.LEAVE, STK.EOP], 0);
  }

  // Wraps a Web Serial port (already opened at 115200) as the io object above.
  function serialIo(port) {
    const writer = port.writable.getWriter();
    const reader = port.readable.getReader();
    let buf = [];
    let pending = null;
    const io = {
      write: (b) => writer.write(b),
      async read(n, timeoutMs) {
        const end = Date.now() + timeoutMs;
        while (buf.length < n) {
          const left = end - Date.now();
          if (left <= 0) break;
          pending = pending || reader.read();
          const r = await Promise.race([pending, sleep(left).then(() => null)]);
          if (r === null) break;
          pending = null;
          if (r.done) break;
          buf.push(...r.value);
        }
        if (buf.length < n) { const got = Uint8Array.from(buf); buf = []; return got; }
        return Uint8Array.from(buf.splice(0, n));
      },
      async reset() {
        buf = [];
        await port.setSignals({ dataTerminalReady: false, requestToSend: false }); await sleep(250);
        await port.setSignals({ dataTerminalReady: true, requestToSend: true }); await sleep(50);
        buf = [];
      },
      async close() {
        try { await reader.cancel(); } catch (e) { /* ignore */ }
        reader.releaseLock(); writer.releaseLock();
        await port.close();
      },
    };
    return io;
  }

  // Asks the user for a port (needs a click) and flashes a UNO. firmware = hex text.
  async function flashUnoInBrowser(hexText, onProgress) {
    const port = await navigator.serial.requestPort();
    await port.open({ baudRate: 115200 });
    const io = serialIo(port);
    try { await flashUno(io, parseHex(hexText), onProgress); } finally { await io.close(); }
  }

  // ---------- ESP32 ----------
  // bin = merged image (bootloader + partitions + app), written at 0x0.
  async function flashEsp32InBrowser(bin, onProgress, log) {
    const { ESPLoader, Transport } = await import(/* @vite-ignore */ ESPTOOL_URL);
    const port = await navigator.serial.requestPort();
    const transport = new Transport(port, false);
    const term = { clean() {}, writeLine: (l) => log && log(l), write: (d) => log && log(d) };
    const loader = new ESPLoader({ transport, baudrate: 460800, romBaudrate: 115200, terminal: term });
    try {
      await loader.main();
      let s = ''; for (let i = 0; i < bin.length; i++) s += String.fromCharCode(bin[i]);
      await loader.writeFlash({
        fileArray: [{ data: s, address: 0x0 }], flashSize: 'keep', flashMode: 'keep', flashFreq: 'keep',
        eraseAll: false, compress: true, reportProgress: (i, w, t) => onProgress && onProgress(w, t),
      });
      await loader.after('hard_reset');
    } finally { try { await transport.disconnect(); } catch (e) { /* ignore */ } }
  }

  const api = { startBuild, waitForRun, downloadResult, unzip, parseHex, flashUno, flashUnoInBrowser, flashEsp32InBrowser, WORKFLOW };
  if (typeof module !== 'undefined' && module.exports) module.exports = api; else root.CloudBuild = api;
})(typeof self !== 'undefined' ? self : this);
