# Phase 4 checklist: installation and calibration in the car

An assistive device, not a safety system. **Every moving test is done by two people**: one drives, one records. The driver must not look at the phone, LCD or laptop while the car is moving. Never connect the car's 12 V directly to the UNO's Vin.

Firmware: `firmware/car_alarm` with `DEBUG_SERIAL 1`. Serial (9600) prints every 0.5 s:
`status=… cm=… shockDev=… fwd=… pkShock=… pkBrake=… pkAccel=…`
(`pk*` = peak since the previous print, sampled every loop, not a 2 Hz snapshot). `status`: 0 NORMAL, 1 WARN, 2 DANGER, 3 SHOCK, 4 BRAKE (see `types.h`). Record with a laptop on the passenger seat, or note it from the app/history.

## Before going to the car (on the bench, software side done)
- [x] Fix: DANGER no longer drops to NORMAL immediately when the echo is lost (it now goes through the 1 s hold, `DistanceSensor.cpp`)
- [x] Fix: the app shows `--` when the firmware does not send a distance (`AlarmRepository.kt`)
- [x] Serial debug records peaks (pkShock/pkBrake/pkAccel) for calibration
- [ ] Re-upload the firmware to the UNO (`arduino-cli board list` for the port) and reinstall the APK (build needs JDK 17 + SDK, see `app/BUILD.md`)
- [ ] Repeat the bench test: cover the sensor with cloth during DANGER → the state holds for ±1.5 s (0.5 s timeout + 1 s hold), then NORMAL; the app shows `--` instead of an old number

## 4.1 Sturdy circuit
- [ ] Move to a perfboard/sturdy breadboard; enclosure; strain relief on cables
- [ ] 1k/2k voltage divider on the BLE module RX is fitted (D11 → RX)
- [ ] Buzzer through the NPN, LEDs through 220 Ω
- [ ] MPU6050 board **flat, rigid, aligned with the car**: +Y faces forward. Glue it down, do not let it hang
- [ ] Run 1 hour on the bench with the enclosure closed: no hang/reset (Definition of Done)

## 4.2 Power
- [ ] USB 5 V car charger + USB cable to the UNO (D10). Measure 5 V at the UNO 5V pin with the engine on, off and while cranking (check for resets)
- [ ] Record: voltage ____ V (engine off), ____ V (on), reset while cranking: yes/no
- [ ] LCD/MPU/BLE still normal after the engine starts

## 4.3 Mount the sensor, parked test (car stationary, engine on)
Answer first (from the owner): **where is the sensor mounted (front bumper or dashboard)?** and **what is the typical speed (city/traffic/highway)?** The answers affect the 50/100 cm distance thresholds.
- [ ] Sensor facing straight ahead, ±40-60 cm high (bumper), free of bodywork; the ±15° cone does not touch the bonnet
- [ ] Car level and stationary when the firmware starts (automatic MPU calibration); LCD shows "Ready"
- [ ] Place an obstacle (cardboard/person) at 200, 100, 50, 20 cm measured with a tape from the sensor: record `cm` and state
- [ ] Repeat with a slanted, soft (cloth/person in a jacket) and small (pole) obstacle: record whether the echo is lost
- [ ] Engine + A/C + radio on: no false shock alarm while stationary (`pkShock` < 0.1)
- [ ] Slam a door (see 4.5) while stationary: record `pkShock`

Distance table (fill in):

| True distance (cm) | `cm` read | State | Notes |
|---|---|---|---|
| 200 | | | |
| 100 | | | |
| 50 | | | |
| 20 | | | |

## 4.4 Threshold calibration (D5, D7)
Do this on a quiet road/empty lot at low speed, two people.
**Braking (vehicle pitch changes the reading; first check that `fwd` on a level stationary road is ≈ 0):**
- [ ] Level, stationary: `fwd` = ____ (should be near 0; if ≥ ±0.1 remount the board flatter, then restart to recalibrate)
- [ ] Gentle (normal) braking ×3: `pkBrake` = ____ , ____ , ____ (must not trigger BRAKE)
- [ ] Medium braking ×3: `pkBrake` = ____ , ____ , ____
- [ ] Hard braking (safe, low speed ±20 km/h) ×3: `pkBrake` = ____ , ____ , ____
- [ ] Strong acceleration ×3: `pkAccel` = ____ (must not trigger braking)
**Shock:**
- [ ] Rough/dirt road: max `pkShock` = ____ (must not trigger SHOCK)
- [ ] Slow speed bump: `pkShock` = ____
- [ ] Hit the body hard with a palm (simulated impact, car stationary): `pkShock` = ____ (must trigger SHOCK)
**Rule for choosing thresholds** (proposal, decide with the owner): `BRAKE_G` midway between the medium-braking and hard-braking peaks, and above the gentle-braking peak + 0.1 g margin; `SHOCK_G` above the rough-road/speed-bump peak + 0.1 g margin and below the impact peak. If there is no clean gap, report it to the owner and use the adjustable thresholds (`CFG`, console/app settings) instead of forcing one value.
- [ ] If thresholds change: edit `config.h`, record in `docs/` and update D7 in PLAN.md and README.md
- [ ] Distance thresholds: do 100/50 cm suit the typical speed? If not, propose options to the owner (D5 changes only by the owner's decision)

## 4.5 False-alarm tests
At least 5 times each; record whether a false alarm appeared.
- [ ] Rough road
- [ ] Speed bump (slow, then a little faster)
- [ ] Slammed doors (driver, passenger, boot)
- [ ] Downhill/uphill (tilt must not read as braking; see the risk in PLAN)
- [ ] Heavy traffic (jam): the sensor sees the car ahead; the distance alarm does not flicker from lost echoes
- [ ] Rain/steam or cloth in front of the sensor: a lost echo does not trigger a distance alarm
- [ ] 1 hour running: no hang/reset, the app stays connected and reconnects when the phone is moved away

## Results
Record all numbers in `docs/test-results.md` (the "Phase 4" section) and tick the 4.x items in `PLAN.md`.
