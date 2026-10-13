# Building the Car Alarm app (Windows)

Two ways. **Way A** (command line) is installed and tested on the author's PC; **Way B** (Android Studio) is for those who prefer a graphical interface. On GitHub, `.github/workflows/ci.yml` and `release.yml` build the app on Ubuntu with JDK 17 and the Android SDK.

## Way A: command line (no Android Studio)

### Installed toolchain (12/10/2026, outside OneDrive)
| Component | Location |
|---|---|
| JDK 17 (Temurin 17.0.20) | `C:\Users\LENOVO\android-build\jdk17` |
| Android SDK (platform 34, build-tools 34.0.0, cmdline-tools) | `C:\Users\LENOVO\android-build\sdk` |
| Gradle 8.7 (only to create the wrapper) | `C:\Users\LENOVO\android-build\gradle-8.7` |

`app/local.properties` (not committed) points to the SDK: `sdk.dir=C:/Users/LENOVO/android-build/sdk` (use forward slashes; backslashes are treated as escape characters). Use your own paths on another machine.

### Build and test
Run from the `app/` folder (PowerShell):

```powershell
$env:JAVA_HOME = "C:\Users\LENOVO\android-build\jdk17"
$env:ANDROID_HOME = "C:\Users\LENOVO\android-build\sdk"
$env:CARALARM_BUILD_DIR = "C:\Users\LENOVO\android-build\caralarm-build"   # build outside OneDrive
.\gradlew.bat test            # unit tests (Protocol, LineBuffer, DeviceLines)
.\gradlew.bat assembleDebug   # builds the APK
```

`CARALARM_BUILD_DIR` matters when the project lives in OneDrive: OneDrive locks files inside the project folder so Gradle fails ("Unable to delete directory ... mergeDebugResources"). With this variable the output is `C:\Users\LENOVO\android-build\caralarm-build\app\outputs\apk\debug\app-debug.apk`. Without it the APK is in `app/app/build/outputs/apk/debug/`. The first build downloads dependencies (a few minutes); later builds are fast.

### Install on the phone
**By file:** copy `app-debug.apk` to the phone, open it, and allow "Install apps from this source" if asked.

**Over USB (adb, installed in `C:\Users\LENOVO\android-build\sdk\platform-tools`):**
1. Enable **USB debugging** on the phone (Samsung/One UI): *Settings → About phone → Software information*, tap **Build number** 7 times (enter the PIN if asked) → back to *Settings → Developer options* → turn on **USB debugging**.
2. Plug the phone into the PC. On the phone, pull down the notification shade → tap the USB notification → choose **File transfer** (not "Charging only").
3. The **Allow USB debugging?** dialog appears → tick "Always allow from this computer" → **Allow**.
4. Check: `adb devices` must list the device with status `device` (not `unauthorized`).
5. Install: `adb install -r "C:\Users\LENOVO\android-build\caralarm-build\app\outputs\apk\debug\app-debug.apk"`.
6. Afterwards turn USB debugging off if you no longer need it.

## Way B: Android Studio
1. Download **Android Studio** from https://developer.android.com/studio and install it (it brings JDK 17).
2. Open Android Studio, follow the first-run wizard (Standard) until the Android SDK is installed. Make sure **SDK Platform 34** and **Build-Tools 34** exist in *Settings → Languages & Frameworks → Android SDK*.
3. **File → Open**, choose this project's `app/` folder, wait for *Gradle sync* to finish.
4. Enable **USB debugging** on the phone, plug it in, choose it in the device menu and press **Run ▶**.
5. Unit tests: right-click `app/src/test` → *Run Tests*.

## Testing on the phone
1. Make sure the UNO runs `firmware/car_alarm` (or `tests/test_06_web_console`) and the module is powered (LED blinking fast).
2. Open the app, **Select device**, wait for "HC-05" in the scan results, select it, then **Connect**. Allow "Nearby devices" (Android 12+) or Location (Android 6-11, the Location switch must be on) and notifications.
3. The app history should log "UART characteristic: …ffe1…".
4. With the main firmware the distance in the app follows the HC-SR04: bring a hand close and the status card changes (SAFE/WARNING/DANGER). A hard shock shows "IMPACT!". Note: a sketch that does not send `...,DIST,<cm>` over Bluetooth leaves the distance at "--".
5. In **Settings → App alarm tone** choose a tone and press **Test tone**.
6. The app UI is in English.

## Common problems
- **HC-05 does not show in the scan:** make sure the module is not connected to another phone/PC (close nRF Connect), the Location switch is on (Android ≤11) and the Nearby devices permission is granted.
- **`The filename, directory name, or volume label syntax is incorrect` while building:** check `local.properties` (use `/`, not `\`).
- **Installation blocked:** allow unknown sources for the file-opener app; Play Protect may warn about debug APKs.
