#pragma once
#include <Arduino.h>
#include "config_user.h"

// ---- Firmware ---------------------------------------------------------------
#define FW_VERSION "2.1"

// ---- Pins -----------------------------------------------------------------
// Every pin and I2C address can be overridden at build time with a -D flag (the web console's Config page
// does this), e.g. -DPIN_TRIG=4. Defaults per board:
#if defined(ARDUINO_ARCH_ESP32)
// ESP32 DevKit (30/38-pin): avoids strapping pins (0, 2, 5, 12, 15) and input-only pins (34-39).
// I2C uses the default Wire pins SDA 21 / SCL 22. HC-SR04 ECHO outputs 5 V: divide it to 3.3 V (e.g. 1k/2k).
#define PIN_TRIG_DEFAULT 25
#define PIN_ECHO_DEFAULT 26
#define PIN_BT_RX_DEFAULT 16     // ESP32 receives (HM-10 TX)
#define PIN_BT_TX_DEFAULT 17     // ESP32 transmits (HM-10 RX; 3.3 V logic, no divider needed)
#define PIN_BUZZER_DEFAULT 27    // via NPN transistor
#define PIN_LED_RED_DEFAULT 32
#define PIN_LED_GREEN_DEFAULT 33
#define PIN_LED_YELLOW_DEFAULT 14
#else
#define PIN_TRIG_DEFAULT 2
#define PIN_ECHO_DEFAULT 3
#define PIN_BT_RX_DEFAULT 10     // Arduino receives (HC-05 TX)
#define PIN_BT_TX_DEFAULT 11     // Arduino transmits (HC-05 RX, via 1k/2k divider)
#define PIN_BUZZER_DEFAULT 8     // via NPN transistor
#define PIN_LED_RED_DEFAULT 5
#define PIN_LED_GREEN_DEFAULT 6
#define PIN_LED_YELLOW_DEFAULT 7
#endif
#ifndef PIN_TRIG
#define PIN_TRIG PIN_TRIG_DEFAULT
#endif
#ifndef PIN_ECHO
#define PIN_ECHO PIN_ECHO_DEFAULT
#endif
#ifndef PIN_BT_RX
#define PIN_BT_RX PIN_BT_RX_DEFAULT
#endif
#ifndef PIN_BT_TX
#define PIN_BT_TX PIN_BT_TX_DEFAULT
#endif
#ifndef PIN_BUZZER
#define PIN_BUZZER PIN_BUZZER_DEFAULT
#endif
#ifndef PIN_LED_RED
#define PIN_LED_RED PIN_LED_RED_DEFAULT
#endif
#ifndef PIN_LED_GREEN
#define PIN_LED_GREEN PIN_LED_GREEN_DEFAULT
#endif
#ifndef PIN_LED_YELLOW
#define PIN_LED_YELLOW PIN_LED_YELLOW_DEFAULT
#endif

// ---- I2C addresses --------------------------------------------------------
#ifndef LCD_ADDR
#define LCD_ADDR 0x27
#endif
#ifndef MPU_ADDR
#define MPU_ADDR 0x68
#endif

// ---- Distance (D5: defaults; runtime values live in Settings/EEPROM) --------------------------------------
// NORMAL > 100 cm, WARN 50..100 cm, DANGER <= 50 cm.
constexpr int16_t DIST_WARN_CM = 100;
constexpr int16_t DIST_DANGER_CM = 50;
constexpr int16_t DIST_HYST_CM = 5;  // must clear a threshold by this much to leave a level
constexpr unsigned long DIST_RELEASE_HOLD_MS = 1000;  // a safer level must hold this long before it is applied
constexpr int16_t DIST_MIN_VALID_CM = 2;
constexpr int16_t DIST_MAX_VALID_CM = 400;
constexpr unsigned long DIST_PERIOD_MS = 100;          // ~10 Hz
constexpr unsigned long DIST_ECHO_TIMEOUT_US = 25000;  // ~4.3 m round trip
constexpr uint8_t DIST_TIMEOUTS_TO_UNKNOWN = 5;        // ~0.5 s without echo -> "no object"

// ---- Motion (D6/D7: auto-calibration, default thresholds; runtime values live in Settings/EEPROM) -----------------
constexpr unsigned long MOTION_PERIOD_MS = 20;  // 50 Hz
constexpr float MOTION_LPF_ALPHA = 0.3f;        // low-pass for braking detection
constexpr float SHOCK_G = 0.6f;                 // |magnitude - 1 g| above this = shock
constexpr uint8_t SHOCK_SAMPLES = 2;            // consecutive samples above SHOCK_G
constexpr float BRAKE_G = 0.4f;                 // forward deceleration above this = hard braking
constexpr unsigned long BRAKE_MIN_MS = 150;     // ... sustained at least this long

// Which MPU6050 axis points to the front of the car (0 = X, 1 = Y, 2 = Z) and its sign.
// If the board is mounted so that braking shows up as positive acceleration, flip the sign.
// Current mounting (12/10/2026): +Y of the GY-521 points to the front of the car (toward the HC-SR04).
constexpr uint8_t FORWARD_AXIS = 1;
constexpr int8_t FORWARD_SIGN = 1;

// ---- Alarm behaviour (D8: no button, auto-clear) --------------------------
constexpr unsigned long MOTION_HOLD_MS = 10000;  // shock/brake status lasts this long
constexpr unsigned long MOTION_BUZZ_MS = 3000;   // continuous buzzer at the start of that window

// ---- Output timing --------------------------------------------------------
constexpr unsigned long LCD_PERIOD_MS = 200;
constexpr unsigned long BT_DIST_PERIOD_MS = 500;       // periodic distance message
constexpr unsigned long BT_MOTION_COOLDOWN_MS = 3000;  // per event type

// ---- Phase 6: commands, test IO, learning ----------------------------------
constexpr unsigned long BLE_IDLE_MS = 250;       // BLE text without a newline counts as a finished line after this pause
constexpr unsigned long IO_TIMEOUT_MS = 30000;   // test-IO mode ends after this long without a command
constexpr unsigned long FEAT_WINDOW_MS = 1000;   // IMU feature window
constexpr uint8_t FEAT_COUNT = 8;                // features per window (see Motion.h)
constexpr uint8_t FEAT_MIN_SAMPLES = 10;         // ignore windows with fewer IMU samples
