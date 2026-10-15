# CLAUDE.md

Guide for Claude and contributors on the **Car Distance & Shock Alarm** project (Arduino UNO).

## Project summary
A car alarm: an HC-SR04 measures the distance to the vehicle ahead, an MPU6050 detects shocks and sudden braking. Alarms go out through a buzzer, LEDs, a 16x2 I2C LCD and Bluetooth to a phone.

Main documents:
- `README.md` / `README.id.md`: description, pin map, decision options D1-D11, limitations (English / Indonesian)
- `PLAN.md`: work phases, **locked decisions**, risks
- Read both before changing anything.

## Locked decisions
The module labelled "HC-05" is actually **BLE-only** (UART 9600, SoftwareSerial D10/D11), a lightweight native Android app (Kotlin, **BLE GATT**), fixed 2-level distance thresholds, power from a USB charger, Android phone, **no button**. Shock/brake alarms auto-clear after 10 seconds. Details in `PLAN.md`.

## Repository layout
```
firmware/   main sketch car_alarm (Arduino UNO)
app/        Android app (Kotlin, BLE)
console/    web console (index.html + ml.js + cloud.js, purely static; published to GitHub Pages)
  server/   optional local helper (Node + arduino-cli) for compile/upload and writing model_data.h
  test/     ml.js unit tests (node console/test/ml.test.js)
tests/      per-module hardware test sketches
docs/       test evidence, wiring, logo, checklist
tools/      helper scripts (firmware compile hook)
.github/    CI, Pages (deploys the console) and Release (hex + APK + console zip) workflows
```
Repository: <https://github.com/SkillsHUB-Electronics/car-alarm>. Web console: monitoring, Test IO, calibration, learning, serial monitor and settings over Web Serial (Chrome/Edge). The GitHub Pages version has no local helper, so the local Compile & Upload and "Write model_data.h" are disabled there; instead it offers **Cloud build** (`console/cloud.js` dispatches `.github/workflows/build-firmware.yml`, downloads the artifact, flashes UNO via STK500v1 and ESP32 via esptool-js; the user's own fine-grained token, never stored in the repo). At `localhost:8765` everything is enabled.

## Android app (`app/`)
- Kotlin, Android Studio, no heavy dependencies; BLE GATT to the "HC-05" module (scan, no pairing); notifications are cut to ≤20 bytes, so reassemble them into lines.
- Foreground Service for the connection + notifications; auto-reconnect; runtime permissions `BLUETOOTH_CONNECT` and `POST_NOTIFICATIONS`.
- The message parser must tolerate corrupt/truncated lines.
- The app plays its own alarm tone (`AlarmSound`: built-in siren or a tone chosen from the phone in Settings).
- Code in `app/` (module `app/app`, package `com.caralarm.app`). See `app/README.md` and `app/BUILD.md`.

## Status
**Repository layout (13/10/2026):** `tools/web-tester` replaced by `console/` (the old `index.html` tester was removed, the console covers it), `.github/workflows` (CI, Pages, Release), `.gitignore`, `LICENSE` (MIT), all files translated to English with `README.id.md` as the Indonesian README. Workflows have never run (repository not yet published). See PLAN.md 6.13.

**Phase 6 v2.1 (13/10/2026):** firmware 2.1 (`HW,*`, `I2C,SCAN`, `DBG` with ax/ay/az; tested on a UNO over USB), a new sidebar web console (Dashboard, Test IO, Learning, Calibration, Serial Monitor, Settings), the app updated (IMU + AI confidence on the dashboard, hardware Settings, fuller IO test, alarm tone and logo, PLAN.md 6.9-6.12); the app compiles, passes unit tests and is installed on the phone, but has **not been exercised against the firmware**. Charger/INA are only stored slots. See PLAN.md 6.9-6.12.

The project (phases 0-5) is **FINAL** (12/10/2026) by the owner's decision: phases 0-3 and 5 are done; phase 4 (car) was skipped, the checklist in `docs/phase4-checklist.md` applies if resumed. Thresholds in `config.h` are final, bench-tested only. MPU6050: +Y faces forward. Test evidence: `docs/test-results.md`. See PLAN.md.

## Working rules
- Language: **all repository files are English** (documents, code identifiers and comments, UI text). The only Indonesian file is `README.id.md`. Reply to the project owner in Indonesian in chat unless asked otherwise.
- If a design choice is not locked, **ask or propose options with a recommendation**; do not pick silently.
- Decision changes must be updated in `PLAN.md` and `README.md` (and `README.id.md`) so they stay consistent.
- Work phase by phase in `PLAN.md`; test one module before merging; tick finished items.

## Target hardware and constraints
- Arduino UNO (ATmega328P): 5 V logic, 32 KB flash, **2 KB SRAM**, one hardware UART (used by USB).
- The I2C bus (A4/A5) is shared: MPU6050 (0x68) and LCD (0x27 or 0x3F).
- Bluetooth goes over **SoftwareSerial D10/D11**, not D0/D1. The 3.3 V Bluetooth module RX pin needs a voltage divider.
- Buzzer through an NPN transistor; LEDs through 220 Ω resistors.

## Code conventions
- Language: Arduino C++. Structure in `firmware/` (one main sketch, a `.h/.cpp` module per function: distance, motion, display, alert, bluetooth).
- **Non-blocking**: use `millis()`, avoid `delay()` in the main loop. The HC-SR04 `pulseIn` must use a timeout.
- Save memory: avoid `String`; use `F()` for literals and fixed `char[]` buffers.
- All pins and thresholds are `constexpr`/`#define` in one configuration file (`config.h`).
- Alarms need hysteresis and cooldown so they do not flicker/spam.
- A timeout or invalid sensor data **must not** trigger a distance alarm.
- Bluetooth protocol: text lines `TYPE,SOURCE,VALUE\n` (e.g. `ALARM,DIST,42`), unless changed in PLAN.md.
- State priority: SHOCK/BRAKE > DANGER > WARNING > NORMAL.

## Safety
- This is an assistive device, not a safety system; never claim it prevents collisions.
- Never suggest the car's 12 V directly to the UNO's Vin; use a USB charger or a 5 V buck converter.
- Never propose tests that distract the driver while driving.

## Testing
- Each module has a test sketch in `tests/` (LED/buzzer, LCD, HC-SR04, MPU6050, Bluetooth).
- Wokwi simulation is optional for logic; final verification must be on hardware and in the car.
- Record threshold calibration results in `docs/`.

## Build / upload
- **Firmware:** the `arduino-cli` bundled with Arduino IDE 2 (board `arduino:avr:uno`), called from the local helper `console/server/start.bat` (compile/upload from the web page) or directly: `arduino-cli compile --upload -p <PORT> --fqbn arduino:avr:uno --config-file ~/.arduinoIDE/arduino-cli.yaml <sketch folder>`. The UNO COM port can move after unplugging USB; check with `arduino-cli board list`. A port cannot be shared (web console, Serial Monitor) during an upload.
- **Android app:** command-line Gradle with JDK 17 and the SDK in `C:\Users\LENOVO\android-build` (see `app/BUILD.md`).

## Knowledge graph (graphify)
The code + document graph is in `graphify-out/` (`GRAPH_REPORT.md`, `graph.json`, `graph.html`; generated, not committed). Built with `graphify update .` (pip package `graphifyy`, no LLM; `.graphifyignore` excludes build/APK/hex).
- For architecture/cross-module questions, read `graphify-out/GRAPH_REPORT.md` first.
- Query: `graphify query "<question>"`, `graphify explain "<node>"`, `graphify path "A" "B"`.
- After changing code, refresh with `graphify update .` (`.ino`, `.xml`, `.kts` files are not yet indexed by the extractor).
- For semantic extraction of documents/images: `/graphify --update` in Claude Code.
- The PreToolUse hook (`graphify claude install`) is deliberately not installed.

## Karpathy-style coding guidelines
Adapted from [andrej-karpathy-skills](https://github.com/forrestchang/andrej-karpathy-skills) (four principles to reduce common LLM mistakes when writing code).
1. **Think before coding:** state assumptions, offer several interpretations when ambiguous, ask when unsure; do not pick silently.
2. **Simplicity first:** the minimum code that solves the problem; no speculative features, abstractions or configuration.
3. **Surgical changes:** change only what was asked, follow the existing style, do not refactor working code; remove only leftovers (imports/variables/functions) caused by your own change, and report other dead code without deleting it.
4. **Goal-driven execution:** turn tasks into verifiable targets (a test that reproduces the bug first, then the fix); for multi-step work, write a plan with a check per step and repeat until it passes.
