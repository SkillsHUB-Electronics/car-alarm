#pragma once
#include <Arduino.h>

// Runtime-adjustable parameters. Defaults come from config.h; values are kept in EEPROM.
struct Settings {
  int16_t warnCm;      // WARN at or below this distance
  int16_t dangerCm;    // DANGER at or below this distance
  uint8_t shock100;    // shock threshold in 0.01 g
  uint8_t brake100;    // hard-braking threshold in 0.01 g
  uint16_t brakeMs;    // braking must last at least this long
  uint8_t holdS;       // shock/brake status hold time in seconds
  uint8_t buzzer;      // 1 = buzzer enabled
};

// Linear correction of the HC-SR04: corrected_cm = raw_cm * gain + offset.
struct Calib {
  float gain;
  float offset;
};

// Hardware options (separate EEPROM block, so Settings/Calib survive). Charger and INA are stored only:
// the firmware has no driver for them yet (no SRAM/pins to spare), the web console marks them "inactive".
struct HwConfig {
  uint8_t lcdAddr;     // I2C address of the LCD (usually 0x27 or 0x3F)
  uint8_t lcdOn;       // 1 = LCD enabled (0 = no I2C traffic to it)
  uint8_t backlight;   // 1 = LCD backlight on
  uint8_t charger;     // reserved: battery charger module
  uint8_t ina;         // reserved: INA current sensor
};

extern Settings settings;
extern HwConfig hw;
extern Calib calib;

void settingsBegin();                       // load from EEPROM, or defaults if empty/corrupt
bool settingsValid(const Settings &s);
void settingsSave();
void settingsReset();                       // defaults, saved
bool calibValid(float gain, float offset);
void calibSave();
bool hwValid(const HwConfig &h);
void hwSave();
void hwReset();                             // defaults (LCD_ADDR, LCD on, backlight on), saved
void calibReset();                          // gain 1, offset 0, saved
