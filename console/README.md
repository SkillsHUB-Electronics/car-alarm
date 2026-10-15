# Car Alarm Console

Web console for the Car Distance & Shock Alarm: **monitoring** (state, distance, IMU X/Y/Z, AI confidence), **Test IO**, **calibration**, **learning** (train the edge-AI model in the browser), **serial monitor** and **settings** (alarm thresholds and hardware). It talks to the UNO over USB (Web Serial, 9600 baud) with the same protocol as the Android app.

The page is purely static (`index.html` + `ml.js` + `cloud.js`, no dependencies).

Layout: a 244 px sidebar on desktop; below 760 px the sidebar becomes a bottom tab bar (Dashboard, Test IO, Learning, Calibrate, More) and Serial Monitor, Settings, Guide and About move under *More*. Light and dark themes follow the design tokens at the top of `index.html` (`:root` and `:root[data-theme=dark]`); the theme choice is remembered per browser. Fonts (Barlow, Barlow Condensed, JetBrains Mono) load from Google Fonts and fall back to system fonts offline.

## Two ways to use it

| | GitHub Pages (hosted) | Local (`console/server`) |
|---|---|---|
| Monitoring, Test IO, calibration, learning, serial monitor, settings | yes | yes |
| Compile & Upload firmware | via **Cloud build** (GitHub Actions + browser flashing) | yes (needs `arduino-cli`) |
| Write `model_data.h` into `firmware/` | no (download it instead) | yes |

Hosted: <https://skillshub-electronics.github.io/car-alarm/>

**BLE / iPhone (PWA):** the page is installable (manifest + service worker) and can connect over **Web Bluetooth** to the HM-10 style BLE module (service `FFE0`, characteristic `FFE1`), with the same line protocol as USB. Android Chrome and desktop Chrome/Edge support it directly. **iOS Safari does not support Web Bluetooth**: on iPhone/iPad open the hosted URL in the free **Bluefy** browser (App Store), then use *Connect BLE*. Not yet tested on real hardware.

Browser for USB: **desktop Chrome or Edge** (Web Serial). Close the Arduino IDE Serial Monitor first; a port can only be used by one program at a time.

## Cloud build (hosted page, no local install)

GitHub Pages is static, so it cannot run a compiler. Settings → **Cloud build** asks GitHub Actions to compile instead:

1. The page dispatches `.github/workflows/build-firmware.yml` (options: board `uno`/`esp32`, power module, CHRG pin, INA219 address and shunt, the same switches as `firmware/car_alarm/config_user.h`).
2. It polls the run, downloads the `firmware` artifact (`.hex` for the UNO, a merged `.bin` for the ESP32) and unzips it in the browser.
3. **Flash to board** writes it over Web Serial: the UNO through its Optiboot bootloader (STK500v1, 115200 baud, with read-back verify), the ESP32 through [esptool-js](https://github.com/espressif/esptool-js) (loaded from unpkg when you flash). *Download* saves the file instead.

**Token.** The page needs a GitHub token to start the workflow. Create a *fine-grained personal access token*: Settings → Developer settings → Personal access tokens → Fine-grained tokens, repository access *only this repository*, permission **Actions: Read and write**, short expiry. Paste it in the page. It is sent only to `api.github.com`. Unless you tick *Remember*, it lives only in the open tab; if you do, it sits in this browser's local storage (readable by any script on the page), so do not do that on a shared computer, and revoke the token when finished. The token is never committed or stored by the repository.

Limits: `model_data.h` from the Learning tab cannot be built in the cloud (use *Download* and commit it, or use the local helper); GitHub Actions runs take roughly 1 to 3 minutes; artifacts expire after 7 days; the workflow must exist on the branch you build.

## Running locally

```bash
node console/server/server.js     # or run console/server/start.bat on Windows
# then open http://localhost:8765
```

The server binds to `127.0.0.1` only and has no npm dependencies. `arduino-cli` is found from Arduino IDE 2 or the `PATH`; set `ARDUINO_CLI=<path>` to point at a specific binary. Only sketches in `firmware/` and `tests/` can be compiled.

## Tests

```bash
node console/test/ml.test.js
node console/test/cloud.test.js   # hex parser, zip reader, UNO flasher against a fake bootloader, GitHub API flow
```
