# PLAN: Car Distance & Shock Alarm

See [README.md](README.md) for the summary and options D1-D11.

## Locked decisions

| ID | Topic | Choice | Status |
|---|---|---|---|
| D1 | Bluetooth module | **The module labelled "HC-05" that we own (turned out to be BLE-only, advertises as "HC-05"; UART 9600 via SoftwareSerial D10/D11)** | ✅ changed 12/10/2026 (option B), originally classic HC-05 |
| D2 | Phone app | **Lightweight native Android app** (Kotlin, **BLE GATT**, the module's UART service) | ✅ changed 12/10/2026 (option B), originally classic SPP |
| D3 | Buzzer | **Active** (via NPN, D8) | ✅ locked 12/10/2026 (passed test 1.1) |
| D4 | LEDs | **3 LEDs green/yellow/red** (D6/D7/D5) | ✅ locked 12/10/2026 (passed test 1.1) |
| D5 | Distance thresholds | **Fixed, 2 levels**: safe >100 cm, warning 50-100 cm, danger ≤50 cm (5 cm hysteresis) | ✅ locked |
| D6 | MPU6050 calibration | **Automatic at start** (car stationary, level) | ✅ locked 12/10/2026 (MPU reads ≈1.01 g at rest; firmware calibration not yet tested on hardware at the time) |
| D7a | MPU6050 orientation | **+Y faces forward** (`FORWARD_AXIS = 1`, `FORWARD_SIGN = +1` in `config.h`); X/Z are not forward | ✅ 12/10/2026 (bench mounting; repeat when mounted in the car) |
| D7 | Shock/brake thresholds | shock >0.6 g, brake < -0.4 g for ≥150 ms, cooldown 3 s | ✅ **FINAL (12/10/2026)** by owner decision; bench-tested only, not calibrated in a car |
| D8 | Alarm behaviour | **No button**: distance auto-clears; shock/brake hold 10 s then auto-clear | ✅ adjusted (no reset button) |
| D9 | MPU library | **`Wire` directly** (with `Wire.setWireTimeout(3000, true)`) | ✅ locked 12/10/2026 (passed test 1.4) |
| D10 | Power | **USB car charger → USB cable to the UNO** | ✅ locked |
| D11 | Dev environment | **`arduino-cli` bundled with Arduino IDE 2** + `console/` (compile/upload from the web page); app: **command-line Gradle** (JDK 17, SDK 34; see `app/BUILD.md`) | ✅ locked 12/10/2026 (in use and tested; Android Studio optional) |

Target device: **Android phone** (BLE, Android 6+; `BLUETOOTH_SCAN`/`BLUETOOTH_CONNECT` permissions on Android 12+). Mute/reset button: **not used**.

**D1/D2 change (12/10/2026):** the module we own turned out to be BLE-only, so D1/D2 moved from classic SPP to **BLE GATT** (option B; buying a real HC-05 (A) or an HM-10 (C) were not chosen). The firmware did not change. nRF Connect (12/10/2026) confirmed the module exposes service **`0xFFE0`** besides 0x1800/0x1801. The app looks for characteristic `FFE1` in that service and falls back to the first notify/indicate characteristic. Also confirmed: service `FFE0` holds `FFE1` (NOTIFY + WRITE, CCCD `0x2902`; this is the UART data) and `FFE2` (WRITE only; purpose unknown, unused). The default constants in `BluetoothService.kt` already match.

Open questions:
- Where is the distance sensor mounted (front bumper / dashboard)?
- Typical speed of use (city / traffic jams / highway)?

## Phases

### Phase 0. Decisions and shopping
- [x] Lock decisions D1-D11 (done 12/10/2026; D1/D2 changed to BLE because the module is BLE-only)
- [x] Check every component is available and works: UNO, HC-SR04, MPU6050, I2C LCD, buzzer + NPN, 3 LEDs, "HC-05" BLE module all passed the phase 1 tests (except HC-SR04 accuracy vs ruler, item 1.3). The USB car charger is untested (phase 4.2).
- [x] Final pin map (README section 3), per `firmware/car_alarm/config.h`: HC-SR04 D2/D3, BT D10/D11, buzzer D8, red/green/yellow LEDs D5/D6/D7, I2C A4/A5. D4 (module EN) was only used for AT diagnosis and is **not fitted** in the final circuit; D12/D13 are free.

### Phase 1. Test modules one by one (`tests/`)
- [x] 1.1 LED + buzzer (including transistor) sound/light (passed via the web tester, 12/10/2026)
- [x] 1.2 I2C LCD: address **0x27** confirmed
- [x] 1.3 HC-SR04: read distance, check accuracy vs ruler (20, 50, 100, 200 cm). The sensor reads and changes (2.3-123.3 cm); **calibration vs ruler declared OK by the owner (12/10/2026)**; per-point error figures were not recorded. The console's calibration page can repeat and record them.
- [x] 1.4 MPU6050: read acceleration, check 1 g at rest (≈1.01 g at rest; shock peak 0.75 g)
- [x] 1.5 Bluetooth: text both ways phone ↔ Arduino passed (web tester, 12/10/2026). **Finding: this module labelled "HC-05" is BLE-only** (detected as a BLE device named "HC-05", not classic SPP). This contradicted the original D1/D2; the decision was changed to BLE (see "D1/D2 change" above).
- [x] 1.6 Verify the LCD + MPU6050 share I2C without conflict (scan: 0x27 and 0x68)

Pass criterion: each module works stably on its own before they are combined.

### Phase 2. Core firmware (`firmware/car_alarm/`)

> **Status 12/10/2026:** firmware compiled (Flash 42%, SRAM 43%), uploaded to COM14 (CH340 clone board) and **bench-tested**: distance (three zones), shock and braking behave as designed. Per-test evidence is in [docs/test-results.md](docs/test-results.md). Remaining: confirm LCD/buzzer/LED during BRAKE, and calibrate thresholds with road data (phase 4).
- [x] 2.1 Non-blocking structure (`millis()`, no long `delay()`); I2C timeout `Wire.setWireTimeout(3000, true)` so a stuck bus cannot freeze the alarm
- [x] 2.2 Distance module: read ~10 Hz, median of 3 samples, drop out-of-range/timeout values. **Tested:** NORMAL→WARN ≈100 cm, WARN→DANGER 50 cm, 5 cm hysteresis, stable readings at 16-20 cm
- [x] 2.3 Motion module: initial calibration, acceleration magnitude, shock detection, brake detection. **Tested:** hard shocks (2.1 g and 1.1 g) trigger SHOCK, touches ≤ 0.33 g do not; 3 consecutive brakes (0.5 g) trigger 3 BRAKE, forward acceleration does not. MPU6050 orientation: **+Y forward** (D7a)
- [x] 2.4 State machine NORMAL/WARN/DANGER/SHOCK/BRAKE with hysteresis and priority. **1 second hold** before the distance level drops (`DIST_RELEASE_HOLD_MS`; rises immediately), chosen by the owner after a slow test showed the alarm flickering on momentary echo loss; **proven** to fix it. Shock/brake hold 10 s then auto-clear (**tested**, 10.0 s)
- [x] 2.5 Outputs: LED, buzzer, LCD (update every 200 ms), Bluetooth. **Owner confirmed:** NORMAL/WARN/DANGER and SHOCK (LED, buzzer pattern, LCD text). LCD/buzzer/LED during BRAKE also confirmed (2.7). LCD text and boot messages are **English** (SAFE / CLOSE DISTANCE / DANGER: CLOSE! / IMPACT! / HARD BRAKING!)
- [x] 2.6 Bluetooth message cooldown: one message per shock/brake event in the app, distance sent periodically
- [x] 2.7 Confirm LCD, buzzer and LED during BRAKE: OK (owner confirmed 12/10/2026)
- [ ] 2.8 Calibrate shock/brake thresholds with real road data → done in phase 4.4 (brake margin is currently thin: measured 0.5 g vs 0.4 g threshold)

### Phase 3. Integration and Android app (`app/`)
- [x] 3.1 Final message protocol: one line `TYPE,SOURCE[,VALUE]` ending in newline: `OK,DIST,150`, `WARN,DIST,80`, `ALARM,DIST,42`, `ALARM,SHOCK,0.9`, `ALARM,BRAKE,0.5`, and `TEST,BT,n` (test). Tested end-to-end with the main firmware and the app (distance, shock, brake)
- [x] 3.2 Bench end-to-end test: move an obstacle and shake/slide the board. Done with the web tester (USB), Serial Bluetooth Terminal (BLE) and the Car Alarm app itself; main firmware + app passed
- [x] 3.3 Native Android app (Kotlin, lightweight, no heavy libraries), code in `app/` (`app/README.md`, `app/BUILD.md`). Built with command-line Gradle, 10 unit tests pass, installed with adb on the phone (SM-A566B), **English** UI. **Tested on the phone with the main firmware:** BLE connection (`FFE1`), distance and coloured state, IMPACT and HARD BRAKING appear in the history with g values, connection survives screen-off, auto-reconnect works (owner confirmed). Alarm vibration and notification sound also confirmed (3.5)
  - scan BLE and pick the "HC-05" device from the scan results (no pairing)
  - GATT: enable notifications on the UART characteristic, reassemble chunks (≤20 bytes) into lines
  - one screen: connection status, last distance, coloured state (green/yellow/red), event history
  - notification + vibration + sound on ALARM; Foreground Service so it survives screen-off; the status notification is only updated when its text changes
  - permissions: Android 12+ `BLUETOOTH_SCAN` and `BLUETOOTH_CONNECT`; Android 6-11 location (for scanning); Android 13+ `POST_NOTIFICATIONS`
- [x] 3.4 Commands from the app to change thresholds + store in EEPROM (implemented in phase 6, `CFG,*`)
- [x] 3.5 Confirm alarm vibration and notification sound on the phone: OK (owner confirmed 12/10/2026)

### Phase 4. Install and test in the car

> **Skipped by owner decision (12/10/2026):** the project was finalised without in-car testing. The items below remain open as optional follow-up work.

> **Status 12/10/2026:** software side ready: step-by-step checklist in [docs/phase4-checklist.md](docs/phase4-checklist.md); two bugs fixed (DANGER now drops through the 1 s hold when the echo is lost; the app shows `--` instead of a stale distance); the serial debug output records peaks (`pkShock`/`pkBrake`/`pkAccel`) for calibration. Physical steps in the car wait for the owner.
- [ ] 4.1 Wire onto perfboard/PCB or a sturdy breadboard; enclosure; tidy cables
- [ ] 4.2 Power per D10, check 5 V
- [ ] 4.3 Mount the sensor, test parked (car stationary) and at low speed
- [ ] 4.4 Calibrate thresholds (D5, D7) with real data, record in `docs/`
- [ ] 4.5 False-alarm tests (rough road, speed bumps, slammed doors)

### Phase 5. Finalisation
- [x] Wiring documentation (diagram/photo) in `docs/`. Schematics exist: [docs/wiring.svg](docs/wiring.svg) (ID) and [docs/wiring-en.svg](docs/wiring-en.svg) (EN); a photo of the real circuit was not made (optional).
- [x] Update the README with final thresholds and test results (12/10/2026; final values = contents of config.h, bench-tested only)
- [x] Record limitations and ideas (README sections 6 and 7, 12/10/2026)

## Phase 6. Commands, Test IO, calibration, edge-AI learning, settings (started 12/10/2026)

Owner request (12/10/2026): since the Serial Monitor can send commands, Bluetooth should too; the app and the web page get Dashboard, Test IO, Calibration, Learning and Settings menus. The web page connects to the UNO over USB (Web Serial), imports learning data from the app's export file, trains a model and flashes the firmware. This phase does **not** revoke the locked decisions D1-D11 (BLE-only, 2 distance levels, no button, UART 9600, SoftwareSerial D10/D11).

| ID | Topic | Choice | Status |
|---|---|---|---|
| D12 | Command protocol | Host sends a line `COMMAND[,arg...]` + newline (≤40 bytes); the device replies `RSP,...` or `ERR,...` **on the originating channel**; events (`OK/WARN/ALARM`, `FEAT`, `CLS`) go to **USB and BLE at once**. USB stays at 9600 baud. Commands over BLE are split into ≤20-byte writes in the app. | ✅ default chosen |
| D13 | Test IO | Manual mode (`IO,ON`) that disables the automatic LED/buzzer/LCD; exits automatically after 30 s without commands (the buzzer must never be left on) | ✅ default |
| D14 | Distance calibration | Manual measurement points (true distance vs mean raw reading of 10 pings); linear regression `true = gain × raw + offset` or offset-only mode (mean difference); stored in EEPROM | ✅ default |
| D15 | Learning data | Features are computed **on the device**: 8 features per 1-second window (milli-g) sent as `FEAT,label,f0..f7`, not raw 50 Hz data (9600 baud is not enough). 14 classes: still, 6 speeds (40/80/100/120/140/160), gentle/medium/hard braking, rough road, gentle/medium/fast turn. Same CSV export for the app and the web page. | ✅ default |
| D16 | Edge-AI model | Small MLP 8→12→14 (ReLU, softmax), float weights in PROGMEM (~1.6 KB flash, ~110 B SRAM during inference), trained in the browser (plain JS), exported as `model_data.h` and compiled in. The `CLS,class,percent` output is **informational only**; it does not change the alarm logic (safe). | ✅ default, open to change |
| D17 | Settings | Distance thresholds, shock/brake thresholds, brake duration, hold time, buzzer on/off stored in EEPROM; defaults = `config.h` | ✅ default |
| D19 | Hardware settings | I2C LCD address (8..119), LCD on/off, backlight stored in EEPROM and applied immediately; battery charger and INA slots are only stored (no function yet) so the UI is ready for future hardware | ✅ default |
| D20 | Live IMU | The dashboard uses `DBG,1` (0.5 s, ax/ay/az in mg) to show the IMU; the app enables it only while the main screen is open | ✅ default |
| D18 | Web | `console/` gets matching menus; Web Serial (Chrome/Edge); compile/flash via `arduino-cli` in the local helper `console/server/server.js`, or on the hosted page via Cloud build (GitHub Actions `build-firmware.yml` compiles, the browser flashes with Web Serial; 15/10/2026) | ✅ default |

**Honest limitation:** the IMU does not measure speed; the speed classes are only an estimate from vibration/shock patterns and depend heavily on the car, tyres and road. Accuracy must be proven with real data. Without road data the model has only been tested with synthetic/bench data.

- [x] 6.0 Update PLAN.md/README.md (this phase)
- [x] 6.1 Firmware: command protocol on USB and BLE (`Commands`), EEPROM (`Settings`). Compiled; not yet tested on hardware
- [x] 6.2 Firmware: Test IO (`IO,*`). Compiled; not yet tested on hardware
- [x] 6.3 Firmware: distance calibration (`CAL,*`, EEPROM). Compiled; not yet tested on hardware
- [x] 6.4 Firmware: 1-second IMU features + `REC` + MLP inference (`Classifier`, `model_data.h`). Compiled with a synthetic model (Flash 76%, SRAM 59%); train+test in the web page passed on synthetic data (val 99.6%, **not** evidence of in-car accuracy)
- [x] 6.5 Firmware: settings (`CFG`, EEPROM)
- [x] 6.6 Web: `console/index.html` (Dashboard, Test IO, Calibration, Learning, Settings, Firmware); the server writes a validated `model_data.h`. Tested in the browser with a fake firmware (not yet with a real UNO)
- [x] 6.7 Android app: code written (IoTestActivity, CalibrationActivity, LearningActivity, SettingsActivity, CommandBus, LearningStore)
- [x] 6.7b App compiled on the Lenovo PC (13/10/2026): `gradlew test assembleDebug` passes, new unit tests pass (toolchain in `C:/Users/LENOVO/android-build`)
- [x] 6.9 Firmware 2.1 (13/10/2026): commands `HW,GET|SET|RESET` (LCD I2C address, LCD on/off, backlight, charger and INA slots that are **stored only** in EEPROM with no function yet), `I2C,SCAN`, `IO,BL`, `IO,BT`, and the `DBG` line extended with `ax,ay,az` (mg). EEPROM layout changed (magic `0xCA63`): settings and distance calibration revert to defaults after the first upload. Flash 70%, SRAM 60% (805 B free). **Tested on the UNO** over USB: PING, `HW,*`, `I2C,SCAN` (0x27 and 0x68 found), `DBG,1` with ax/ay/az
- [x] 6.14 Bluetooth text input (15/10/2026, not yet bench-tested): a line from the BLE module that is not a command is forwarded to USB as `RX,BT,<text>`; BLE text without a newline counts as a line after 250 ms of silence (`BLE_IDLE_MS`). The console's Test IO tab shows it in the "Bluetooth received" card. Flash +346 B, SRAM +8 B on the UNO (compiles for UNO and ESP32).
- [x] 6.10 New web console (`console/index.html`): sidebar Dashboard (SAFE/WARNING/DANGER state, distance with scale, IMU X/Y/Z, AI confidence, Compile & Upload panel), Test IO (LED, buzzer, LCD, backlight, Bluetooth TX, all off), Learning, Calibration, Serial Monitor, Settings (alarm + hardware), About. Tested in the browser with fake data; not yet clicked through with a real UNO
- [x] 6.11 Android app: dashboard adds IMU X/Y/Z (+peak, tap to reset) and an AI-confidence bar; Settings adds a Hardware section (I2C address, LCD, backlight, charger/INA stored only, I2C scan); IO test adds backlight, Bluetooth TX, all off
- [x] 6.12 App (13/10/2026): new launcher logo (`docs/logo.png`, adaptive icon `drawable-nodpi/ic_launcher_fg.png`); the alarm tone is now played by the app (`AlarmSound`): a built-in two-tone siren (default) or a tone picked from the phone, chosen in Settings (Test/Stop), 8 s per alarm on the alarm stream; the notification channel `alarm_silent` has no sound. Installed on the phone (SM-A566B / A56) with adb on 13/10/2026; **tone and logo not yet checked by the owner**
- [x] 6.13 Repository layout for GitHub (13/10/2026): `tools/web-tester` replaced by `console/` (`index.html`, `ml.js`, `server/` optional local helper, `test/`); the old tester was removed because the console covers it; the hosted version (GitHub Pages) disables Compile & Upload and writing `model_data.h`. Added `.github/workflows` (ci, pages, release), `.gitignore`, `LICENSE` (MIT). All repository files translated to English; `README.id.md` is the Indonesian README. **Workflows have never run** (repository not yet published); the local path (`localhost:8765`) and the static mode were tested in a browser
- [ ] 6.8 Test everything on hardware / phone together (waiting for the owner)

## Risks and mitigations

| Risk | Mitigation |
|---|---|
| Bluetooth module RX pin damaged by 5 V | 1k/2k voltage divider |
| UNO overloaded by the buzzer | Use a transistor, not the pin directly |
| I2C address conflict | MPU 0x68, LCD 0x27/0x3F; scan first |
| HC-SR04 reads 0/timeout | Filter + ignore invalid values, never alarm on a timeout |
| Shock false alarms | Threshold + minimum duration + cooldown, field calibration |
| UNO SRAM (2 KB) | Avoid `String`, use `F()` for text, fixed buffers |
| Unstable car voltage | USB charger/buck converter, not 12 V directly |
| Android 12+ refuses connections without permission | Request `BLUETOOTH_CONNECT` at runtime |
| Shock alarm cannot be reset without a button | Auto-clear after 10 s, recorded in the app log |
| "HC-05" module is BLE-only (clone) | D1/D2 changed to BLE GATT (12/10/2026); UUIDs `FFE0/FFE1` confirmed |
| Momentary HC-SR04 echo loss cuts the alarm | 1 s hold before dropping a level (`DIST_RELEASE_HOLD_MS`), proven in the bench test |
| Thin brake-detection margin (measured 0.5 g vs 0.4 g threshold) | Calibrate with real braking in phase 4.4; thresholds are now adjustable (`CFG`) |
| Board tilt / uphill read as braking (≈ −0.2 g at 12°) | Calibrate at rest on start; mount the board flat and rigid; check downhill/uphill (phase 4.5) |
| Stuck I2C bus freezes the alarm | `Wire.setWireTimeout(3000, true)` in all sketches |
| OneDrive locks the Gradle build folder | Build outside OneDrive via `CARALARM_BUILD_DIR` (see `app/BUILD.md`) |
| Corrupted commands because SoftwareSerial drops bytes while sending (BLE) | The parser rejects lines with the wrong argument count and ranges are validated; the UI retries and shows the reply values for verification |
| IMU "speed" classes are inaccurate | The IMU does not measure speed; the model is informational only (never triggers an alarm); needs lots of real multi-session data |
| UNO COM port number moves after unplugging USB | Check `arduino-cli board list` / the ⟳ button in the console before uploading |

## Definition of done
- Distance below the threshold triggers buzzer, LED, LCD and Bluetooth in < 500 ms
- Shocks and hard braking are detected reliably in real tests, with minimal false alarms
- The system runs stably for > 1 hour without hanging/resetting
- Wiring and configuration documentation complete

## Final status (12/10/2026, updated 13/10/2026)
**The project was finalised by the owner.** Phases 0-3 and 5 are done; phase 4 (car) was skipped. Thresholds in `config.h` are final as they are. Limitations: no road test; thin brake margin (0.5 g vs 0.4 g); the last two fixes (hold on echo loss, `--` distance in the app) were verified only on the bench. Phase 6 (console, commands, settings, learning) is done and compiled; firmware 2.1 was tested over USB, and the app was installed on the phone on 13/10/2026 but not yet exercised against the firmware. To continue, see README sections 5-7 and [docs/phase4-checklist.md](docs/phase4-checklist.md).
