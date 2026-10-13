# Test results

## 12/10/2026: interface test (web tester + `test_06_web_console`)

| Item | Result |
|---|---|
| Red LED lights (D5) | Pass |
| Green LED lights (D6) | Pass |
| Yellow LED lights (D7) | Pass |
| Buzzer sounds (D8) | Pass |
| LCD shows text | Pass |
| Phone receives text from HC-05 | Not yet |
| HC-SR04 reads and changes ≥ 10 cm | Pass |
| MPU6050 at rest ≈ 1.00 g (0.90-1.10) | Pass |
| MPU6050 detects a shock (> 0.6 g) | Pass |
| I2C: LCD (0x27/0x3F) and MPU (0x68) detected | Pass |
| Arduino receives text from the phone (HC-05) | Not yet |

- Distance read: min 2.3 cm, max 123.3 cm. Accuracy vs ruler (20/50/100/200 cm) not measured.
- MPU6050: at rest ≈ 1.01 g (X -0.06, Y 0.00, Z 1.01); peak deviation from 1 g = 0.75 g when shaken.
- I2C: 0x27 (LCD) and 0x68 (MPU6050).

### Problem notes
- At first the HC-SR04, MPU6050 and LCD were not read (empty I2C scan, `DIST,-1.0`). After re-checking the power wiring, everything read. The exact cause was not recorded.
- The stock Wire library has no timeout: a stuck I2C bus freezes the sketch. `Wire.setWireTimeout(3000, true)` was added to every sketch that uses I2C.

## Continued 12/10/2026: Bluetooth and HC-SR04

- **Two-way Bluetooth passed** over BLE: text from the phone was received identically in the web tester, and `TEST,BT,1` from the web page was received identically on the phone (pins D10/D11, 1k/2k divider).
- **This "HC-05" module is BLE-only.** It is detected as a BLE device "HC-05" (address `52:25:F1:D8:E0:44`); it does not appear in classic Bluetooth scans on the PC or the phone. It disappears from scans when VCC is removed, so it is this module. AT commands over serial are not answered (tested 4800-115200 baud, with and without CR/LF, EN/KEY HIGH).
- **HC-SR04** briefly stopped reading (wiring/contact on D2/D3), then read normally again: ≈124.4 cm stable. Pulses on D12/D13 were also tested (≈1500 µs ≈ 26 cm); the sensor is healthy.
- The UNO COM port moves around (COM12 → COM14) after unplugging/replugging USB.

## Continued 12/10/2026: Android app (BLE)
- Module UUIDs confirmed with nRF Connect: service `FFE0`, characteristic `FFE1` (NOTIFY + WRITE, CCCD 0x2902), `FFE2` (WRITE).
- The app (Kotlin, BLE GATT) was built with command-line Gradle (JDK 17, SDK 34), 10 unit tests pass, installed with adb on a Samsung SM-A566B.
- Real test with `test_06_web_console` (protocol mirrored to BLE): the app connected, a 125 cm distance was shown, bringing a hand close triggered DANGER (6-19 cm), then it recovered ("Distance safe again").
- Finding and fix: the status notification was updated ~5×/s until Android throttled it; it is now updated only when its text changes.

## Continued 12/10/2026: calibration and main firmware
- **HC-SR04 calibration vs ruler: OK** (declared by the owner; per-point error figures were not recorded). A calibration page was added to the web tester (error table, linear correction, MPU still/peak calibration) for repeating and recording.
- **Main firmware** `firmware/car_alarm` uploaded to COM14 (CH340 clone board): running, `status=0 cm=124 shockDev=0.00 fwd=0.00` (NORMAL, MPU calibrated). Full alarm behaviour was not yet tested.

## Main firmware distance test (12/10/2026, serial debug 9600 + app)
One quick hand approach, 95 samples (2 Hz):

| Transition | Read at the change | Matches threshold? |
|---|---|---|
| NORMAL → WARN | 65 cm (previous sample 124 cm: the hand moved fast) | Correct level, exact 100 cm crossing not measured |
| WARN → DANGER | 50 cm | Yes (≤ 50 cm) |
| DANGER → WARN | 58 cm | Yes (5 cm hysteresis: exits at > 55 cm) |
| WARN → NORMAL | 124 cm (previous sample 85 cm) | Correct level, exit point 105 cm not measured |

- The minimum distance reached was only 49 cm; the deep DANGER zone (≈20 cm) was not tested in this recording.
- The app logged the same sequence (Warning 61 cm → DANGER 50 cm → Warning 57 cm → "Distance safe again") with no meaningful delay.
- Not recorded: LED, buzzer pattern and LCD text in each zone (needed the owner's observation).

### Slow approach (12/10/2026, 165 samples, 20-159 cm)

| Event | Read | Threshold |
|---|---|---|
| NORMAL → WARN | 101 cm (printed sample; the state uses a 3-sample median, ±1 cm difference) | ≤ 100 cm ✓ |
| WARN → DANGER | 49 cm (previous sample 51 cm) | ≤ 50 cm ✓ |
| DANGER zone 49 → 20 cm | DANGER stable for ±20 s, no flicker | ✓ |
| Max `shockDev` | 0.01 | no shock false alarm ✓ |

**Finding:** at 21.3 s the state dropped WARN → NORMAL for ±0.5 s and then returned to WARN, although the obstacle was still at ±97 cm. Cause: a 125 cm reading (the hand briefly left the sensor cone and the wall behind was read) appeared ≥2 times in a row, so even the 3-sample median became 125 cm, and leaving the level happened **immediately** with no hold. In a car, a momentary loss of echo/reflection could make a WARN/DANGER alarm intermittent. The hysteresis exit points (55 cm and 105 cm) were not recorded on the retreat because the retreat was too fast; the DANGER → WARN exit at 58 cm was already observed in the first test.

### Retest with the 1 second hold (12/10/2026, 205 samples, 16-125 cm)

| Event | Read | Result |
|---|---|---|
| NORMAL → WARN | 95-100 cm (re-approach, previous sample 102 cm) | ✓ (≤ 100 cm) |
| WARN holds at 100-105 cm | state stayed WARN at 99-106 cm | ✓ upper hysteresis |
| WARN → NORMAL | 109 cm, ±1 s after the first reading > 105 cm (106) | ✓ 1 s hold |
| WARN → DANGER | 50 cm (previous sample 51 cm) | ✓ (≤ 50 cm) |
| Deep DANGER zone | 16-20 cm, DANGER stable ±24 s | ✓ |
| Max `shockDev` | 0.01 | ✓ no shock false alarm |

**Echo disturbance (deliberate test):**
- Around 97 cm the reading jumped to 125 cm (hand outside the cone) and the state stayed WARN, did not drop to NORMAL: the **hold fix is proven** (previously it flickered WARN → NORMAL → WARN).
- In DANGER, readings jumped 19 → 124 cm (57.8 s), 19 → 59 cm (60.3 s), and the state stayed DANGER until the 125 cm reading persisted ±1 s (63.8 → 64.8 s), then NORMAL: as designed.

Not observed: leaving DANGER → WARN on a slow retreat at 55 cm (the last retreat was too fast; 58 cm was observed in the first test).

### LED observation per zone (reported by the owner, 12/10/2026)
- NORMAL: green LED on ✓
- WARN: yellow LED slow blink ✓
- DANGER: red LED fast blink ✓
- Buzzer and LCD text per zone: **as expected** (reported by the owner, 12/10/2026).
- **Complete main-firmware distance test**: thresholds, hysteresis, 1 s hold, LED, buzzer, LCD and the BLE message to the app.

## Main firmware shock test (12/10/2026, serial debug + app)

| Observation | Result |
|---|---|
| Gentle touch and moderate shaking (printed shockDev 0.20-0.33 g) | **No alarm** ✓ (threshold 0.6 g) |
| Hard tap/shake #1 | SHOCK state at 31.3 s; the app logged **Impact (2.1 g)** |
| Hard tap/shake #2 | SHOCK state at 69.3 s; the app logged **Impact (1.1 g)** |
| Duration of the SHOCK state | 31.3 → 41.3 s = **10.0 s**; 69.3 → 79.4 s = **10.1 s** ✓ (10 s auto-clear, D8) |
| BLE message | 1 `ALARM,SHOCK` line per event in the app (cooldown works) ✓ |
| Gap between events | 38 s on serial, 39 s in the app (17:14:09 → 17:14:48) ✓ consistent |

Note: the serial debug prints at 2 Hz, so the `shockDev` peak is not always captured; the real g value was taken from the message to the app. At 8.3 s the state briefly went DANGER (distance 5 cm) because a hand approached the sensor while touching the board; not a motion problem.

Confirmed by the owner (12/10/2026): during SHOCK the LCD shows `IMPACT!`, the buzzer sounds continuously for the first ±3 s then goes quiet, the LED behaves accordingly, and everything switches off automatically after 10 s (LCD back to `SAFE`). **Shock test complete.**

## Brake test, attempt 1 (12/10/2026): **invalid**
The firmware still used the X axis as forward (`FORWARD_AXIS = 0`), whereas in this mounting **+Y faces forward** (HC-SR04). The two BRAKE states (51.4 s and 66.9 s) and `fwd` down to -0.74 g came from the X axis, not the forward direction, so they cannot be used to judge the brake threshold. Fix: `FORWARD_AXIS = 1`, `FORWARD_SIGN = +1` (uploaded to COM14); the brake test was repeated.

## Brake test, attempt 2 (12/10/2026, `FORWARD_AXIS = 1`, +Y forward, 175 samples)
- 6 BRAKE episodes (3.3; 14.3; 30.8; 48.9; 60.4; 75.4 s), each held 10 s (auto-clear D8).
- Each episode was preceded by a printed **negative** `fwd` peak: −0.52, −0.48, −0.35, −0.68, −0.66, −0.36 g (2 Hz snapshots; real peaks may be larger) → sign +1 is correct: deceleration along +Y is negative.
- No SHOCK; max `shockDev` 0.19 g.
- **Test limitation:** the owner's timing did not follow the script (first brake already at 3.3 s) and the direction of each slide was not recorded, so the "reverse must not trigger braking" test **cannot be judged**. Test design B was also flawed: sliding the board backwards quickly gives negative forward acceleration when it **starts** moving, which reasonably reads as braking.
- At 76-82 s `fwd` stayed ≈ −0.2 g with `shockDev` 0: the board was tilted by hand (≈ 12°). A ≈ 24° tilt already equals −0.4 g, so a constant tilt can trigger BRAKE; such a tilt does not occur on a real road, but hard braking adds vehicle pitch.

## Brake test, attempt 3 (12/10/2026): 3 forward slides then sudden braking (+Y forward)

| Slide | Initial acceleration (preceding sample) | BRAKE | Duration |
|---|---|---|---|
| 1 | fwd **+0.82** (21.8 s) | 22.3 s (fwd −0.37) | 10.0 s |
| 2 | – (not caught by a snapshot) | 37.4 s (fwd −0.26) | 10.0 s |
| 3 | fwd **+0.45** (53.9 s) | 54.4 s (fwd −0.27) | 10.0 s |

- **3 slides → exactly 3 BRAKE episodes**; no SHOCK; max `shockDev` 0.17 g.
- The forward-acceleration phase (large **positive** fwd) does **not** trigger braking; only deceleration (negative) does ✓. The D7a orientation (+Y forward, `FORWARD_SIGN = +1`) is proven.
- The 10 s auto-clear works ✓. The printed `fwd` values are snapshots after the peak (2 Hz low-pass), so the real peak is larger than −0.26…−0.37 g.

**Note in the app (BLE):** `Hard braking (0.5 g)` at 17:22:48, 17:23:03 and 17:23:20, i.e. exactly 3 slides 15 s and 17 s apart, matching serial (22.3 → 37.4 → 54.4 s). The measured peak deceleration was **0.5 g** on all three, above the 0.4 g threshold with a small margin (0.1 g). After the recording, at 17:23:33 the app logged `Impact (1.1 g)` together with `Hard braking (0.8 g)`: a hard impact that triggers both (SHOCK/BRAKE priority is equal, see the README).

Confirmed by the owner (12/10/2026): during BRAKE the LCD shows `HARD BRAKING!`, the buzzer and LED behave accordingly; the alarm notification appears on the phone with vibration and sound. **All bench tests of phases 2-3 complete.**

### Not yet tested
- Real road tests: ordinary vs hard braking, speed bumps, rough roads (phase 4).
- Alarm vibration/sound in detail. (Confirmed by the owner 12/10/2026: impact detected in the app, connection survives screen-off, auto-reconnect works.)
- The main firmware `firmware/car_alarm` together with the app and in a car.

## Phase 4: software preparation (12/10/2026)
- Bug 1 fixed: when the echo is lost (5 consecutive timeouts) the previous distance level returned straight to NORMAL with no hold; it now goes through the same hold (`DIST_RELEASE_HOLD_MS` = 1 s). Compiled (Flash 43%, SRAM 43%); **not yet tested on hardware**.
- Bug 2 fixed: `AlarmRepository.applyDistance` kept the old distance when a message had no value; `distanceCm` now becomes null so the UI shows `--`. **Not yet built/tested** at that time.
- Serial debug adds `pkShock`, `pkBrake`, `pkAccel` (peaks per 0.5 s, sampled every loop).

## 13/10/2026: firmware 2.1 over USB (UNO)
- Commands `PING`, `HW,GET|SET|RESET`, `I2C,SCAN` (0x27 and 0x68 found) and `DBG,1` with ax/ay/az tested on the UNO over USB. Flash 70%, SRAM 60% (805 B free). Details in PLAN.md 6.9.
- The Android app (with the new tone and logo) was installed with adb on the phone (SM-A566B / A56); it has not yet been exercised against the firmware.
