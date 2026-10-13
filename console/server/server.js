// Local helper for the web console: serves console/index.html and wraps arduino-cli
// (list ports, compile, upload). No npm dependencies.
//
//   node server.js            -> http://127.0.0.1:8765
//   PORT=9000 node server.js  -> other port
//   ARDUINO_CLI=<path>        -> use a specific arduino-cli binary
//
// Binds to 127.0.0.1 only. Sketches are limited to the ones found in tests/ and
// firmware/; ports must come from arduino-cli's own port list.

'use strict';
const http = require('http');
const fs = require('fs');
const os = require('os');
const path = require('path');
const { spawn, spawnSync } = require('child_process');

const PORT = Number(process.env.PORT) || 8765;
const HOST = '127.0.0.1';
const ROOT = path.resolve(__dirname, '..', '..');           // repository root
const WEB = path.resolve(__dirname, '..');                   // console/ (index.html, ml.js)
const FQBN = 'arduino:avr:uno';
const BUILD_ROOT = path.join(os.tmpdir(), 'car-alarm-build');

// ---------- locate arduino-cli ----------
function findCli() {
  const candidates = [process.env.ARDUINO_CLI];
  const exe = process.platform === 'win32' ? 'arduino-cli.exe' : 'arduino-cli';
  const ide = ['resources', 'app', 'lib', 'backend', 'resources', exe];
  if (process.platform === 'win32') {
    for (const base of [process.env.LOCALAPPDATA && path.join(process.env.LOCALAPPDATA, 'Programs', 'Arduino IDE'),
                        process.env.ProgramFiles && path.join(process.env.ProgramFiles, 'Arduino IDE')]) {
      if (base) candidates.push(path.join(base, ...ide));
    }
  } else if (process.platform === 'darwin') {
    candidates.push(path.join('/Applications/Arduino IDE.app/Contents', ...ide));
  }
  for (const c of candidates) if (c && fs.existsSync(c)) return c;
  const probe = spawnSync(exe, ['version'], { encoding: 'utf8' });
  return probe.status === 0 ? exe : null;
}

const CLI = findCli();
// Arduino IDE 2 keeps its settings (sketchbook libraries etc.) here; reuse them if present.
const IDE_CFG = path.join(os.homedir(), '.arduinoIDE', 'arduino-cli.yaml');
const CFG_ARGS = fs.existsSync(IDE_CFG) ? ['--config-file', IDE_CFG] : [];

function cliVersion() {
  if (!CLI) return null;
  const r = spawnSync(CLI, ['version'], { encoding: 'utf8' });
  return r.status === 0 ? r.stdout.trim() : null;
}

// ---------- sketches ----------
function listSketches() {
  const out = [];
  const add = (rel) => {
    const dir = path.join(ROOT, rel);
    const name = path.basename(dir);
    if (fs.existsSync(path.join(dir, name + '.ino'))) out.push({ id: rel.replace(/\\/g, '/'), name });
  };
  const testsDir = path.join(ROOT, 'tests');
  if (fs.existsSync(testsDir)) {
    for (const d of fs.readdirSync(testsDir).sort()) add(path.join('tests', d));
  }
  add(path.join('firmware', 'car_alarm'));
  return out;
}

// ---------- ports ----------
function listPorts() {
  if (!CLI) return [];
  const r = spawnSync(CLI, ['board', 'list', '--format', 'json', ...CFG_ARGS], { encoding: 'utf8', timeout: 15000 });
  if (r.status !== 0) return [];
  let data;
  try { data = JSON.parse(r.stdout); } catch (e) { return []; }
  const items = Array.isArray(data) ? data : data.detected_ports || [];
  return items
    .map((it) => {
      const p = it.port || {};
      const b = (it.matching_boards || it.boards || [])[0];
      return { address: p.address, protocol: p.protocol, label: p.protocol_label || p.protocol, board: b ? b.name : null, fqbn: b ? b.fqbn : null };
    })
    .filter((p) => p.address && p.protocol === 'serial');
}

// ---------- jobs ----------
let busy = false;

function runJob(res, args) {
  if (busy) {
    res.writeHead(409, { 'Content-Type': 'text/plain; charset=utf-8' });
    res.end('Another compile/upload is already running.\n__EXIT__ 1\n');
    return;
  }
  busy = true;
  res.writeHead(200, { 'Content-Type': 'text/plain; charset=utf-8', 'Cache-Control': 'no-cache' });
  const child = spawn(CLI, args, { cwd: ROOT, windowsHide: true });
  let finished = false;
  const done = (code) => {
    if (finished) return;
    finished = true;
    busy = false;
    res.end(`\n__EXIT__ ${code}\n`);
  };
  child.stdout.on('data', (d) => res.write(d));
  child.stderr.on('data', (d) => res.write(d));
  child.on('error', (e) => { res.write('Failed to start arduino-cli: ' + e.message + '\n'); done(1); });
  child.on('close', (code) => done(code === null ? 1 : code));
  // Browser closed the connection: stop the job.
  res.on('close', () => { if (!finished) { child.kill(); busy = false; finished = true; } });
}

function buildArgs(sketchId, upload, port) {
  const sketch = listSketches().find((s) => s.id === sketchId);
  if (!sketch) return { error: 'Unknown sketch' };
  const args = ['compile', '--fqbn', FQBN, '--build-path', path.join(BUILD_ROOT, sketch.name), ...CFG_ARGS];
  if (upload) {
    const known = listPorts().find((p) => p.address === port);
    if (!known) return { error: 'Port not found: ' + port };
    args.push('--upload', '--port', port);
  }
  args.push(path.join(ROOT, sketch.id));
  return { args };
}

// ---------- http ----------
function allowedHost(h) {
  return h === `127.0.0.1:${PORT}` || h === `localhost:${PORT}`;
}

function readBody(req, limit = 10000) {
  return new Promise((resolve) => {
    let b = '';
    req.on('data', (d) => { b += d; if (b.length > limit) req.destroy(); });
    req.on('end', () => { try { resolve(JSON.parse(b || '{}')); } catch (e) { resolve({}); } });
  });
}

function json(res, code, obj) {
  res.writeHead(code, { 'Content-Type': 'application/json; charset=utf-8', 'Cache-Control': 'no-cache' });
  res.end(JSON.stringify(obj));
}

// ---------- model_data.h ----------
const MODEL_FILE = path.join(ROOT, 'firmware', 'car_alarm', 'model_data.h');
const MODEL_STUB = '// Edge-AI model for the IMU driving-state classifier. THIS FILE IS GENERATED by the web console\n' +
  '// (Learning > "Write model_data.h"). The stub below disables the classifier.\n#pragma once\n#define MODEL_PRESENT 0\n';
const NUM = '-?\\d+(?:\\.\\d+)?(?:e[-+]?\\d+)?f';
const NUM_LINE = new RegExp('^(?:' + NUM + ')(?:, (?:' + NUM + '))*,?$', 'i');

// Accepts only the exact shape the web trainer produces (numbers in PROGMEM arrays), so this endpoint
// cannot be used to put arbitrary code into the firmware. Returns an error text or null.
function validateModel(text) {
  const defs = {};
  const arrays = {};
  let cur = null;
  for (const raw of text.split(/\r?\n/)) {
    const line = raw.trim();
    if (!line || line.startsWith('//')) continue;
    if (line === '#pragma once' || line === '#include <avr/pgmspace.h>' || line === '#define MODEL_PRESENT 1') continue;
    let m = /^#define (MODEL_N_IN|MODEL_N_HID|MODEL_N_OUT) (\d{1,3})$/.exec(line);
    if (m) { defs[m[1]] = Number(m[2]); continue; }
    m = /^const float (MODEL_(?:MEAN|STD|W1|B1|W2|B2))\[\] PROGMEM = \{$/.exec(line);
    if (m) { cur = m[1]; arrays[cur] = []; continue; }
    if (line === '};') { cur = null; continue; }
    if (cur && NUM_LINE.test(line)) { arrays[cur].push(...line.replace(/,$/, '').split(', ')); continue; }
    return 'Unknown line: ' + line.slice(0, 60);
  }
  const { MODEL_N_IN: I, MODEL_N_HID: H, MODEL_N_OUT: O } = defs;
  if (!I || !H || !O) return 'Incomplete model size definitions';
  if (H > 32) return 'Hidden maximum is 32';
  const want = { MODEL_MEAN: I, MODEL_STD: I, MODEL_W1: H * I, MODEL_B1: H, MODEL_W2: O * H, MODEL_B2: O };
  for (const k of Object.keys(want)) if ((arrays[k] || []).length !== want[k]) return 'Array size mismatch for ' + k;
  return null;
}

const server = http.createServer(async (req, res) => {
  // Block DNS-rebinding and cross-site requests: only our own origin may talk to this server.
  if (!allowedHost(req.headers.host || '')) { res.writeHead(403); return res.end('Forbidden'); }
  if (req.method === 'POST') {
    const origin = req.headers.origin;
    if (!origin || !allowedHost(origin.replace(/^https?:\/\//, ''))) { res.writeHead(403); return res.end('Forbidden'); }
  }

  const url = (req.url || '/').split('?')[0];

  if (req.method === 'GET' && (url === '/' || url === '/index.html' || url === '/console' || url === '/console.html')) {
    res.writeHead(200, { 'Content-Type': 'text/html; charset=utf-8', 'Cache-Control': 'no-cache' });
    return res.end(fs.readFileSync(path.join(WEB, 'index.html')));
  }
  if (req.method === 'GET' && url === '/ml.js') {
    res.writeHead(200, { 'Content-Type': 'text/javascript; charset=utf-8', 'Cache-Control': 'no-cache' });
    return res.end(fs.readFileSync(path.join(WEB, 'ml.js')));
  }
  if (req.method === 'GET' && url === '/api/model') {
    const text = fs.existsSync(MODEL_FILE) ? fs.readFileSync(MODEL_FILE, 'utf8') : '';
    return json(res, 200, { present: /#define MODEL_PRESENT 1/.test(text), bytes: text.length });
  }
  if (req.method === 'POST' && url === '/api/model') {
    const body = await readBody(req, 200000);
    const text = String(body.header || '');
    const problem = body.clear ? null : validateModel(text);
    if (problem) return json(res, 400, { error: problem });
    fs.writeFileSync(MODEL_FILE, body.clear ? MODEL_STUB : (text.endsWith('\n') ? text : text + '\n'));
    return json(res, 200, { ok: true, present: !body.clear });
  }
  if (req.method === 'GET' && url === '/api/status') {
    return json(res, 200, { cli: CLI, version: cliVersion(), fqbn: FQBN, sketches: listSketches() });
  }
  if (req.method === 'GET' && url === '/api/ports') {
    return json(res, 200, { ports: listPorts() });
  }
  if (req.method === 'POST' && (url === '/api/compile' || url === '/api/upload')) {
    if (!CLI) return json(res, 500, { error: 'arduino-cli not found' });
    const body = await readBody(req);
    const upload = url === '/api/upload';
    const { args, error } = buildArgs(String(body.sketch || ''), upload, String(body.port || ''));
    if (error) { res.writeHead(400, { 'Content-Type': 'text/plain; charset=utf-8' }); return res.end(error + '\n__EXIT__ 1\n'); }
    return runJob(res, args);
  }
  res.writeHead(404);
  res.end('Not found');
});

server.listen(PORT, HOST, () => {
  console.log(`Car Alarm web console: http://localhost:${PORT}`);
  console.log(CLI ? `arduino-cli: ${CLI} (${cliVersion()})` : 'arduino-cli NOT FOUND (install Arduino IDE 2 or set ARDUINO_CLI)');
  if (CFG_ARGS.length) console.log(`config: ${IDE_CFG}`);
  console.log('Press Ctrl+C to stop.');
});
