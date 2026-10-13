#include "Settings.h"
#include <EEPROM.h>
#include <math.h>
#include "config.h"

Settings settings;
Calib calib;

namespace {
constexpr uint16_t MAGIC = 0xCA63;  // layout changed in 2.1: older EEPROM contents fall back to defaults

struct Stored {
  uint16_t magic;
  Settings s;
  Calib c;
  uint8_t sum;
};

uint8_t checksum(const Stored &st) {
  const uint8_t *p = reinterpret_cast<const uint8_t *>(&st);
  uint8_t sum = 0x5A;
  for (size_t i = 0; i < offsetof(Stored, sum); i++) sum = (uint8_t)(sum * 31 + p[i]);
  return sum;
}

void setDefaults() {
  settings.warnCm = DIST_WARN_CM;
  settings.dangerCm = DIST_DANGER_CM;
  settings.shock100 = (uint8_t)(SHOCK_G * 100 + 0.5f);
  settings.brake100 = (uint8_t)(BRAKE_G * 100 + 0.5f);
  settings.brakeMs = (uint16_t)BRAKE_MIN_MS;
  settings.holdS = (uint8_t)(MOTION_HOLD_MS / 1000);
  settings.buzzer = 1;
  settings.lcdAddr = LCD_ADDR;
  settings.lcdOn = 1;
  settings.backlight = 1;
  settings.charger = 0;
  settings.ina = 0;
  calib.gain = 1.0f;
  calib.offset = 0.0f;
}

void save() {
  Stored st;
  memset(&st, 0, sizeof(st));  // padding bytes must be stable for the checksum
  st.magic = MAGIC;
  st.s = settings;
  st.c = calib;
  st.sum = checksum(st);
  EEPROM.put(0, st);  // put() only writes bytes that changed
}
}  // namespace

bool settingsValid(const Settings &s) {
  return s.warnCm >= 20 && s.warnCm <= 300 && s.dangerCm >= 10 && s.dangerCm + 10 <= s.warnCm &&
         s.shock100 >= 10 && s.shock100 <= 300 && s.brake100 >= 5 && s.brake100 <= 200 &&
         s.brakeMs >= 50 && s.brakeMs <= 2000 && s.holdS >= 1 && s.holdS <= 60 && s.buzzer <= 1 &&
         s.lcdAddr >= 8 && s.lcdAddr <= 119 && s.lcdOn <= 1 && s.backlight <= 1 && s.charger <= 1 && s.ina <= 1;
}

bool calibValid(float gain, float offset) {
  return isfinite(gain) && isfinite(offset) && gain >= 0.5f && gain <= 2.0f && offset >= -50.0f && offset <= 50.0f;
}

void settingsBegin() {
  setDefaults();
  Stored st;
  EEPROM.get(0, st);
  if (st.magic == MAGIC && st.sum == checksum(st) && settingsValid(st.s) && calibValid(st.c.gain, st.c.offset)) {
    settings = st.s;
    calib = st.c;
  }
}

void settingsSave() { save(); }

void settingsReset() {
  Calib keep = calib;  // resetting settings must not wipe the sensor calibration
  setDefaults();
  calib = keep;
  save();
}

void calibSave() { save(); }

void calibReset() {
  calib.gain = 1.0f;
  calib.offset = 0.0f;
  save();
}
