# Car Distance & Shock Alarm (Arduino UNO)

English · [Bahasa Indonesia](README.id.md)

An Arduino UNO based car alarm that:

1. **Measures the distance to the vehicle ahead** (HC-SR04). Too close → alarm.
2. **Detects shocks, impacts and hard braking** (MPU6050). Detected → alarm.
3. Shows every alarm on a **buzzer, LED, 16x2 I2C LCD** and **sends it to an Android phone over Bluetooth LE**.

> Status (12/10/2026): **FINAL.** Phases 0-3 and 5 are done; every module, the main firmware and the Android BLE app passed bench tests (evidence: [docs/test-results.md](docs/test-results.md)). Phase 4 (in-car testing) was skipped by the owner; the checklist to resume is [docs/phase4-checklist.md](docs/phase4-checklist.md). Later work (firmware 2.1, web console, app tone and logo) is tracked in [PLAN.md](PLAN.md) phase 6. See also [CLAUDE.md](CLAUDE.md).

This is an assistive device, **not a safety system**; do not rely on it to prevent collisions.

---

## Repository layout, web console and releases

| Folder | Contents |
|---|---|
| `firmware/` | Arduino UNO sketch (`car_alarm`) |
| `app/` | Android app (Kotlin, BLE), see [app/README.md](app/README.md) |
| `console/` | Web console (monitoring, IO test, calibration, learning, settings over Web Serial), see [console/README.md](console/README.md) |
| `tests/` | Per-module hardware test sketches |
| `docs/` | Test evidence, wiring, logo, checklist |
| `.github/workflows/` | `ci.yml` (tests), `pages.yml` (deploys the console to GitHub Pages), `release.yml` (firmware hex + APK + console zip) |

**Web console:** <https://skillshub-electronics.github.io/car-alarm/> (after the repository is published and *Settings → Pages → Source: GitHub Actions* is enabled). It uses Web Serial (Chrome/Edge desktop). Compile & Upload and writing `model_data.h` need the local helper server (`console/server`).

**Releases:** push a tag such as `v2.1.0` (`git tag v2.1.0 && git push --tags`); `release.yml` builds the firmware, the APK (debug-signed) and the console zip and attaches them to a GitHub Release. License: MIT ([LICENSE](LICENSE)).

---

## 1. Components

| Component | Purpose | Interface |
|---|---|---|
| Arduino UNO (ATmega328P, 5 V) | Brain | - |
| HC-SR04 | Distance ahead (2-400 cm, ~15° cone) | Digital: Trig, Echo |
| MPU6050 (GY-521) | 3-axis accelerometer + gyroscope | I2C (0x68) |
| Bluetooth module labelled "HC-05" (turned out to be BLE-only, see decision D1) | Notifications to the phone | UART 9600 (SoftwareSerial), BLE GATT `FFE0/FFE1` |
| 16x2 LCD + I2C backpack | Status display | I2C (0x27 or 0x3F) |
| Buzzer | Audible alarm | Digital via transistor |
| LED (+ resistor) | Visual alarm | Digital |
| NPN transistor (2N2222 / BC547), resistors | Buzzer driver, LED current limit, Bluetooth voltage divider | - |

## 2. How it works (summary)

```
HC-SR04 --> distance (cm) --+
                            +--> Status logic --> Buzzer / LED / LCD / Bluetooth
MPU6050 --> acceleration ---+
```

### System states

| State | Trigger | Buzzer | LED | LCD | Bluetooth |
|---|---|---|---|---|---|
| NORMAL | Safe distance, no shock | Off | Green / off | "SAFE" + "Dist: 124cm" | Periodic status (optional) |
| WARNING | Distance inside the warning limit | Slow beep | Slow blink | "CLOSE DISTANCE" | `WARN,DIST,<cm>` |
| DANGER | Distance inside the critical limit | Fast beep | Fast blink | "DANGER: CLOSE!" | `ALARM,DIST,<cm>` |
| SHOCK | Acceleration spike above the threshold | Long tone | On / blink | "IMPACT!" | `ALARM,SHOCK,<g>` |
| HARD BRAKING | Deceleration along the travel axis above the threshold | Long tone | On / blink | "HARD BRAKING!" | `ALARM,BRAKE,<g>` |

LCD text is English (max 16 characters per line); line 2 is `Dist:%4dcm`, plus `ERR` if the MPU6050 cannot be read. Boot messages: "Car Alarm" / "Calibrating MPU", then "Ready" or "MPU not found!". The distance level rises **immediately** when closer, but falls to a safer level only after the safe condition has held for **1 second** (`DIST_RELEASE_HOLD_MS`).

Priority when several apply: SHOCK/BRAKE > DANGER > WARNING > NORMAL.
Threshold numbers are **starting values** and should be calibrated in the field (see section 5).

## 3. Pin map (final, matches `firmware/car_alarm/config.h`)

Wiring diagrams: [docs/wiring.svg](docs/wiring.svg) (Indonesian labels) · [docs/wiring-en.svg](docs/wiring-en.svg) (English labels).

| Function | UNO pin | Notes |
|---|---|---|
| LCD I2C SDA / SCL | A4 / A5 | Shares the bus with the MPU6050 |
| MPU6050 SDA / SCL | A4 / A5 | Address 0x68 (AD0 to GND) |
| HC-SR04 Trig | D2 | Output |
| HC-SR04 Echo | D3 | Input |
| Bluetooth TX → UNO | D10 (SoftwareSerial RX) | Avoid D0/D1 so uploads do not conflict |
| Bluetooth RX ← UNO | D11 (SoftwareSerial TX) | **5 V→3.3 V voltage divider required** (HC-05/06/HM-10) |
| Buzzer (via transistor) | D8 | Transistor base through ~1 kΩ |
| Red LED | D5 | 220 Ω resistor |
| Green/yellow LED (optional) | D6 / D7 | 220 Ω resistor |
| Mute/reset button | - | Not used (decided) |

### Supporting circuits
- **Buzzer:** pin D8 → 1 kΩ resistor → NPN base; emitter to GND; buzzer between 5 V and the collector; flyback diode only for magnetic/coil buzzers.
- **LED:** pin → 220 Ω resistor → LED → GND.
- **Bluetooth RX:** UNO TX (D11) → 1 kΩ resistor → module RX, and module RX → 2 kΩ resistor → GND (divider ≈ 3.3 V).

## 4. Decisions

**Locked:** D1 = the "HC-05" labelled module that turned out to be BLE-only (changed 12/10/2026, see PLAN.md), D2 = lightweight native Android app over BLE GATT, D5 = fixed thresholds, D10 = USB charger. Android phone, no mute/reset button (D8 adjusted: shock/brake alarms auto-clear after 10 seconds).
**Locked 12/10/2026 (phase 0 done):** D3 active buzzer, D4 3 LEDs, D6 automatic calibration, D7 starting values (calibrated in phase 4), D9 `Wire`, D11 `arduino-cli` + command-line Gradle.
The options below are kept as a decision history (the ⭐ recommendations there may differ from the final choice; PLAN.md is authoritative).

### D1. Bluetooth module
| Option | Pros | Cons |
|---|---|---|
| ⭐ **A. HC-05** | Cheap, easy, classic SPP, many Android apps, master/slave | Not compatible with iPhone |
| B. HC-06 | Simplest (slave only) | Not compatible with iPhone, limited configuration |
| C. HM-10 (BLE) | Works with Android **and** iPhone, low power | Slightly more complex app/protocol (GATT) |

### D2. Phone app ("notifications")
| Option | Description |
|---|---|
| A. Serial Bluetooth Terminal (ready-made app) | No coding; text appears on screen. Not a real push notification. Good for a prototype. |
| B. MIT App Inventor (own app) | Notifications, speech (TTS), vibration, coloured UI. No heavy coding. |
| ⭐ **C. Native app (Kotlin)** | Most flexible, longest to build. Chosen, see `app/`. |

A notification that really appears in the notification bar needs an app of your own (B/C).

### D3. Buzzer type
| Option | Description |
|---|---|
| ⭐ **A. Active buzzer** | Just HIGH/LOW; patterns come from timing. Simple. |
| B. Passive buzzer | Different tones (melodies, rising pitch) with `tone()`, but uses Timer2 and more code. |

### D4. LEDs
| Option | Description |
|---|---|
| A. 1 red LED | Simplest; states are told apart by blink pattern. |
| ⭐ **B. 3 LEDs (green/yellow/red)** | State visible at a glance. Needs 3 pins and 3 resistors. |

### D5. Distance threshold logic
| Option | Description |
|---|---|
| ⭐ **A. Fixed thresholds, 2 levels** | E.g. WARNING < 100 cm, DANGER < 50 cm. Easy to test and understand. |
| B. Dynamic threshold based on speed | Safe distance grows with speed. Needs a speed source (GPS or OBD-II) that is **not** in the parts list; integrating the MPU6050 accelerometer drifts and is inaccurate. |
| C. Thresholds set from the phone | Send commands over Bluetooth (e.g. `SET,WARN,120`) and store in EEPROM. Implemented later on top of A (see PLAN.md phase 6). |

### D6. MPU6050 mounting and calibration
| Option | Description |
|---|---|
| A. Mount straight, X axis = car's forward direction | Simple, but must be precise. |
| ⭐ **B. Automatic calibration at start** (car stationary, level) | Records gravity as zero; the "forward" axis is set at calibration/configuration. Tolerates a tilted mount. |

### D7. Shock and hard-braking thresholds (starting values)
| Parameter | Proposed start | Notes |
|---|---|---|
| Shock (magnitude difference from 1 g) | > 0.6 g, brief | Detects impacts/strong vibration |
| Hard braking (deceleration along travel axis) | < -0.4 g for ≥ 150 ms | Typical hard braking 0.5-0.8 g |
| Cooldown between alarms | 3 seconds | Prevents notification spam |

All values are `#define`s to be calibrated on the road. Hard braking is hard to tell from "car tilted / downhill" without calibration D6-B.

### D8. Alarm behaviour after the condition clears
| Option | Description |
|---|---|
| ⭐ **A. Auto-clear for distance, latch for shock** | Distance alarm stops by itself when safe; shock alarm stays a few seconds / until a button is pressed. |
| B. Everything auto-clears | Simple, but shock events are easily missed. |
| C. Everything latches until reset | Safe, but needs an extra button. |

Final: no button, so shock/brake alarms auto-clear after 10 s and the event stays in the app history.

### D9. MPU6050 library
| Option | Description |
|---|---|
| ⭐ **A. Read registers directly via `Wire`** | No dependency, low memory, little code. |
| B. `MPU6050_light` | Easy, built-in calibration. |
| C. Adafruit MPU6050 | Complete but heavier. |

### D10. Power in the car
| Option | Description |
|---|---|
| ⭐ **A. USB 5 V car charger → USB cable to the UNO** | Safe, already regulated, easiest. |
| B. Buck converter module (LM2596) 12 V→5 V to the 5V/Vin pin | Tidier/permanent. Set to 5.0 V **before** connecting. |
| C. 12 V straight to Vin/jack | **Not recommended**: car voltage is 13.8-14.4 V with many spikes, the UNO regulator overheats quickly. |

### D11. Development environment
| Option | Description |
|---|---|
| ⭐ **A. Arduino IDE + Wokwi simulation (optional)** | Easiest; Wokwi supports UNO, HC-SR04, MPU6050 and I2C LCD for logic tests before hardware. |
| B. PlatformIO (VS Code) | Tidier project structure, automatic library management. |

## 5. Thresholds and test results

Source of truth: `firmware/car_alarm/config.h`. **Set as final by the owner (12/10/2026), but only bench-tested; not calibrated in a car.**

| Parameter | Value | Status |
|---|---|---|
| WARNING / DANGER | ≤ 100 cm / ≤ 50 cm, 5 cm hysteresis, 1 s hold before dropping a level | Bench tested |
| Shock | magnitude difference > 0.6 g, 2 consecutive samples | Bench tested (2.1 g and 1.1 g trigger, ≤ 0.33 g does not) |
| Hard braking | +Y axis deceleration > 0.4 g for ≥ 150 ms | Bench tested (measured 0.5 g; thin margin) |
| Shock/brake auto-clear | 10 s (continuous buzzer for the first 3 s) | Tested (10.0 s) |
| BLE messages | one line `TYPE,SOURCE[,VALUE]`; 3 s cooldown per event type; distance every 500 ms | Tested end-to-end with the app |
| MPU6050 calibration | automatic at start (car stationary, level) | Tested |

Not tested (phase 4 skipped): ordinary vs hard braking on the road, speed bumps, rough roads, slammed doors, car charger power, runs > 1 hour. All are in [docs/phase4-checklist.md](docs/phase4-checklist.md).

## 6. Limitations to be aware of (honest)

- **HC-SR04** is effective at ~2-3 m, has a narrow cone, and is affected by slanted/soft surfaces, rain and temperature. Suited to **low speed / traffic jams / parking**, **not** collision warning on highways. It is an assistive system, not a safety device.
- The sensor needs a clear view (e.g. behind the front grille/bumper; on the dashboard facing forward it does not work through glass, which reflects ultrasound).
- An accelerometer cannot tell hard braking from rough roads 100 %; filtering (moving average + minimum duration) is needed. A constant tilt (≈ 24°) equals −0.4 g and can read as braking; mount the board flat and rigid.
- The braking margin is thin (measured 0.5 g vs 0.4 g threshold) until calibrated on the road.
- A lost echo (slanted/soft surface, outside the sensor cone) is handled with a 1 s hold and never triggers an alarm, which also means a distance alarm may not appear for a non-reflective obstacle.
- Bluetooth LE reaches ~10 m and the phone must run the app (Foreground Service) for notifications. The app was tested on one phone only (Samsung SM-A566B).
- The "HC-05" module is BLE-only: it does not show up in classic Bluetooth scans and cannot be used with ordinary SPP apps. Use the Car Alarm app or a BLE app such as Serial Bluetooth Terminal.
- No button: shock/brake alarms clear themselves after 10 s; the event is kept in the app history.
- Thresholds can now be changed from the console and the app (EEPROM), but the new values are only bench-tested.

## 7. Ideas for later

- Dynamic distance threshold from speed, using phone GPS or OBD-II (accelerometer alone drifts).
- Event logging to an SD card or export of the app history.
- Separating braking from tilt with the MPU6050 gyroscope (pitch).
- A second (rear) distance sensor for reversing.
- App: sound/vibration settings per event, iPhone support (needs an iOS app).
- Battery charger and INA current sensor (hardware slots already stored in EEPROM, no function yet).

## 8. Folder structure

```
car-alarm/
├─ README.md / README.id.md   <- this document (English / Indonesian)
├─ PLAN.md                    <- work phases + locked decisions
├─ CLAUDE.md                  <- guide for Claude / contributors
├─ firmware/car_alarm/        <- main sketch (.h/.cpp modules, config.h)
├─ app/                       <- native Android app (Kotlin, BLE)
├─ console/                   <- web console (index.html, ml.js), console/server local helper
├─ docs/                      <- wiring, test-results.md, phase4-checklist.md, logo.png
├─ tests/                     <- per-module test sketches
├─ tools/                     <- helper scripts
└─ .github/workflows/         <- CI, Pages, Release
```

## 9. Shopping list

UNO + USB cable, HC-SR04, GY-521 (MPU6050), BLE module labelled HC-05 (or HM-10), 1602 LCD + I2C backpack (0x27), active buzzer, red/yellow/green LEDs, NPN 2N2222/BC547, resistors (220 Ω ×3, 1 kΩ ×2, 2 kΩ ×1), breadboard/perfboard + jumpers, USB car charger.
