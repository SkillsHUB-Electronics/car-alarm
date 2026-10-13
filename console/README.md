# Car Alarm Console

Web console for the Car Distance & Shock Alarm: **monitoring** (state, distance, IMU X/Y/Z, AI confidence), **Test IO**, **calibration**, **learning** (train the edge-AI model in the browser), **serial monitor** and **settings** (alarm thresholds and hardware). It talks to the UNO over USB (Web Serial, 9600 baud) with the same protocol as the Android app.

The page is purely static (`index.html` + `ml.js`, no dependencies).

Layout: a 244 px sidebar on desktop; below 760 px the sidebar becomes a bottom tab bar (Dashboard, Test IO, Learning, Calibrate, More) and Serial Monitor, Settings, Guide and About move under *More*. Light and dark themes follow the design tokens at the top of `index.html` (`:root` and `:root[data-theme=dark]`); the theme choice is remembered per browser. Fonts (Barlow, Barlow Condensed, JetBrains Mono) load from Google Fonts and fall back to system fonts offline.

## Two ways to use it

| | GitHub Pages (hosted) | Local (`console/server`) |
|---|---|---|
| Monitoring, Test IO, calibration, learning, serial monitor, settings | yes | yes |
| Compile & Upload firmware | no | yes (needs `arduino-cli`) |
| Write `model_data.h` into `firmware/` | no (download it instead) | yes |

Hosted: <https://skillshub-electronics.github.io/car-alarm/>

**BLE / iPhone (PWA):** the page is installable (manifest + service worker) and can connect over **Web Bluetooth** to the HM-10 style BLE module (service `FFE0`, characteristic `FFE1`), with the same line protocol as USB. Android Chrome and desktop Chrome/Edge support it directly. **iOS Safari does not support Web Bluetooth**: on iPhone/iPad open the hosted URL in the free **Bluefy** browser (App Store), then use *Connect BLE*. Not yet tested on real hardware.

Browser for USB: **desktop Chrome or Edge** (Web Serial). Close the Arduino IDE Serial Monitor first; a port can only be used by one program at a time.

## Running locally

```bash
node console/server/server.js     # or run console/server/start.bat on Windows
# then open http://localhost:8765
```

The server binds to `127.0.0.1` only and has no npm dependencies. `arduino-cli` is found from Arduino IDE 2 or the `PATH`; set `ARDUINO_CLI=<path>` to point at a specific binary. Only sketches in `firmware/` and `tests/` can be compiled.

## Tests

```bash
node console/test/ml.test.js
```
