# Car Alarm: Android app

A lightweight native app (Kotlin, no runtime dependencies) that receives messages from the BLE module labelled "HC-05" and shows the status and alarm notifications.

> Status: **changed to BLE GATT on 12/10/2026** (the module we own turned out to be BLE-only; see PLAN.md). **Built and unit tests pass (10/10) on 12/10/2026**; the debug APK was **tested on a phone (Samsung SM-A566B)**: it connected to the module over BLE (`FFE1`), the distance showed and the state changed (SAFE → DANGER → recovered) following the HC-SR04 via `test_06_web_console`. Confirmed by the owner (12/10/2026): **impact** detection shows in the app, the connection **survives screen-off**, and **auto-reconnect** works. Phase 6 additions (13/10/2026) were compiled and installed with adb on the phone but not yet exercised against the firmware. Build and install instructions: [BUILD.md](BUILD.md).

## Features
- Scan BLE and pick the "HC-05" module from the results (no pairing; stored in SharedPreferences).
- BLE GATT connection inside a Foreground Service (stays alive with the screen off). UART characteristic notifications are cut to ≤20 bytes, so `LineBuffer` reassembles them into lines.
- UART characteristic: `FFE1` in service `FFE0` (confirmed on the module we own with nRF Connect: NOTIFY + WRITE; `FFE2` is WRITE only and unused). If absent, the first notify/indicate characteristic outside the standard GATT services is used. The UUID in use is logged in the history ("UART characteristic: …"); if needed change the constants in `BluetoothService.kt` after reading the real UUID with nRF Connect.
- Auto-reconnect with 1 s → 10 s backoff.
- One screen: coloured status card (green/yellow/red/grey), last distance, event history (last 100).
- Heads-up notification + vibration + alarm tone for `ALARM` messages (5 s cooldown per type).
- Shock/brake display stays for 10 s, matching the firmware auto-clear.

## Alarm tone and logo (13/10/2026)
- The alarm tone is played by the app itself (`AlarmSound`) on the alarm volume for 8 s per alarm. Default: a built-in two-tone siren; or choose a tone from the phone in **Settings → App alarm tone** (with Test/Stop). The notification channel is silent so there is no double sound.
- The launcher icon is the adaptive logo from `docs/logo.png`.

## Menus (phase 6)
From the main screen (Dashboard): buttons **IO test**, **Calibration**, **Learning**, **Settings**. All send commands to the firmware over BLE (writes to `FFE1`, split into 20 bytes); commands and replies are in `firmware/README.md` (D12 in PLAN.md).
- **Dashboard:** state, distance, live IMU X/Y/Z (+peak, tap to reset) and the edge-AI classification (`CLS,class,percent`) when the firmware has a model; informational only.
- **IO test:** start test mode, switch LEDs/buzzer, send text to the LCD, backlight, Bluetooth TX, all off, read raw distance, acceleration, MPU/LCD status. Exits by itself when the screen closes.
- **Calibration:** enter the true distance, measure (mean of 10 raw readings on the device), repeat at several points, compute the linear regression (or offset only), apply to the firmware EEPROM.
- **Learning:** pick a class (still, 6 speeds, 3 braking, rough road, 3 turns), Record/Stop; one feature window per second is stored in `learn.csv` (keeps recording with the screen off). **Export CSV** through the file picker, then import it in the web console (`console/index.html`) to train a model and put it in the firmware. Training is **not** done on the phone.
- **Settings:** app alarm tone; distance/shock/brake thresholds, hold time, buzzer; hardware (LCD I2C address, LCD, backlight, charger/INA stored only, I2C scan). Device settings are stored in the firmware EEPROM.

Code: `CommandBus` (send/receive commands), `DeviceLines` (`FEAT`/`CLS` parser, class list, calibration maths, 20-byte splitter; tested in `DeviceLinesTest`), `LearningStore`, `ToolActivity` + four activities, `AlarmSound`. The UI is built in code with platform widgets (no new dependencies).

## Protocol (from the Arduino, one line per message)
`TYPE,SOURCE[,VALUE]\n`

| Message | Meaning |
|---|---|
| `OK,DIST,150` | Safe distance, 150 cm |
| `WARN,DIST,80` | Warning, 80 cm |
| `ALARM,DIST,42` | Danger, 42 cm |
| `ALARM,SHOCK,0.9` | Shock, 0.9 g |
| `ALARM,BRAKE,0.5` | Hard braking, 0.5 g |
| `TEST,BT,n` | Test message (sketch `test_05_bluetooth`) |

Corrupt or unknown lines are ignored. The parser is in `Protocol.kt` and tested in `ProtocolTest.kt`.

**Firmware note:** for the status card and distance to keep updating, the firmware should send `OK/WARN/ALARM,DIST,<cm>` periodically (e.g. 2-5 Hz) or at least when the distance changes level.

## Building
Quick way (command line): see [BUILD.md](BUILD.md). Manual alternative:
1. Install **Android Studio** (it brings JDK 17 and Android SDK 34).
2. `File > Open` this `app/` folder. If asked, let Android Studio sync Gradle (the wrapper uses Gradle 8.7, AGP 8.5.2, Kotlin 1.9.24).
3. Enable **USB debugging** on the phone, connect it and press Run. Or `Build > Build APK(s)`.
4. Unit tests: `./gradlew test`.

## Usage
1. **No pairing** is needed in the Bluetooth settings. Make sure the module is powered (LED blinking fast) and not connected to another app or a PC.
2. Open the app, **Select device**, wait for "HC-05" to appear in the scan results (stay close), select it and press **Connect**.
3. Allow Bluetooth access ("Nearby devices" on Android 12+, or Location on Android 6-11 with the Location switch on) and notifications when asked.
4. Test without the full Arduino: run `tests/test_06_web_console` or `tests/test_05_bluetooth`, or use the main firmware's `IO,BT,<text>` command from the web console.

## Code structure (`app/src/main/java/com/caralarm/app/`)
| File | Role |
|---|---|
| `Protocol.kt` | Message model and parser |
| `AlarmRepository.kt` | Shared state (connection, distance, level, log) + listeners, main thread only |
| `BluetoothService.kt` | Foreground service, BLE GATT connection, reconnect, UART notifications |
| `LineBuffer.kt` | Reassembles BLE notification chunks into lines (tested in `LineBufferTest.kt`) |
| `AlarmNotifier.kt` | Channels and notifications (ongoing status + alarm) |
| `AlarmSound.kt` | Alarm tone: built-in siren or a tone chosen from the phone |
| `MainActivity.kt` | UI, runtime permissions, BLE scan and device picker |

## Known limitations
- BLE only (the module we own). Not for classic Bluetooth/SPP (real HC-05/HC-06).
- Targets Android 8.0+ (minSdk 26). Android 12+ asks for `BLUETOOTH_SCAN` and `BLUETOOTH_CONNECT`; Android 6-11 asks for `ACCESS_FINE_LOCATION` for scanning; 13+ asks for `POST_NOTIFICATIONS`.
- If the notification permission is denied, alarms only show inside the app (the tone still plays while the service runs).
- Some phones aggressively kill background services; disable battery optimisation for this app if the connection drops often.
