#pragma once
#include <Arduino.h>

// ---- Firmware ---------------------------------------------------------------
#define FW_VERSION "2.1"

// ---- Pins -----------------------------------------------------------------
constexpr uint8_t PIN_TRIG = 2;
constexpr uint8_t PIN_ECHO = 3;
constexpr uint8_t PIN_BT_RX = 10;  // Arduino receives (HC-05 TX)
constexpr uint8_t PIN_BT_TX = 11;  // Arduino transmits (HC-05 RX, via 1k/2k divider)
constexpr uint8_t PIN_BUZZER = 8;  // via NPN transistor
constexpr uint8_t PIN_LED_RED = 5;
constexpr uint8_t PIN_LED_GREEN = 6;
constexpr uint8_t PIN_LED_YELLOW = 7;

// ---- I2C addresses --------------------------------------------------------
constexpr uint8_t LCD_ADDR = 0x27;
constexpr uint8_t MPU_ADDR = 0x68;

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
constexpr unsigned long IO_TIMEOUT_MS = 30000;   // test-IO mode ends after this long without a command
constexpr unsigned long FEAT_WINDOW_MS = 1000;   // IMU feature window
constexpr uint8_t FEAT_COUNT = 8;                // features per window (see Motion.h)
constexpr uint8_t FEAT_MIN_SAMPLES = 10;         // ignore windows with fewer IMU samples
