#include "Settings.h"
#include <EEPROM.h>
#include <math.h>
#include "config.h"

Settings settings;
Calib calib;
HwConfig hw;

namespace {
constexpr uint16_t MAGIC = 0xCA62;

struct Stored {
  uint16_t magic;
  Settings s;
  Calib c;
  uint8_t sum;
};

constexpr uint16_t HW_MAGIC = 0xCA64;

struct HwStored {
  uint16_t magic;
  HwConfig h;
  uint8_t sum;
};

uint8_t checksum(const Stored &st) {
  const uint8_t *p = reinterpret_cast<const uint8_t *>(&st);
  uint8_t sum = 0x5A;
  for (size_t i = 0; i < offsetof(Stored, sum); i++) sum = (uint8_t)(sum * 31 + p[i]);
  return sum;
}

uint8_t hwChecksum(const HwStored &st) {
  const uint8_t *p = reinterpret_cast<const uint8_t *>(&st);
  uint8_t sum = 0xA5;
  for (size_t i = 0; i < offsetof(HwStored, sum); i++) sum = (uint8_t)(sum * 31 + p[i]);
  return sum;
}

void hwDefaults() {
  hw.lcdAddr = LCD_ADDR;
  hw.lcdOn = 1;
  hw.backlight = 1;
  hw.charger = 0;
  hw.ina = 0;
}

void setDefaults() {
  settings.warnCm = DIST_WARN_CM;
  settings.dangerCm = DIST_DANGER_CM;
  settings.shock100 = (uint8_t)(SHOCK_G * 100 + 0.5f);
  settings.brake100 = (uint8_t)(BRAKE_G * 100 + 0.5f);
  settings.brakeMs = (uint16_t)BRAKE_MIN_MS;
  settings.holdS = (uint8_t)(MOTION_HOLD_MS / 1000);
  settings.buzzer = 1;
  calib.gain = 1.0f;
  calib.offset = 0.0f;
  hwDefaults();
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
         s.brakeMs >= 50 && s.brakeMs <= 2000 && s.holdS >= 1 && s.holdS <= 60 && s.buzzer <= 1;
}

bool calibValid(float gain, float offset) {
  return (gain - gain == 0.0f) && (offset - offset == 0.0f) && gain >= 0.5f && gain <= 2.0f && offset >= -50.0f && offset <= 50.0f;
}

bool hwValid(const HwConfig &h) {
  return h.lcdAddr >= 0x08 && h.lcdAddr <= 0x77 && h.lcdOn <= 1 && h.backlight <= 1 && h.charger <= 1 && h.ina <= 1;
}

void hwSave() {
  HwStored st;
  memset(&st, 0, sizeof(st));
  st.magic = HW_MAGIC;
  st.h = hw;
  st.sum = hwChecksum(st);
  EEPROM.put(sizeof(Stored), st);  // after the Settings/Calib block
}

void hwReset() {
  hwDefaults();
  hwSave();
}

void settingsBegin() {
  setDefaults();
  Stored st;
  EEPROM.get(0, st);
  if (st.magic == MAGIC && st.sum == checksum(st) && settingsValid(st.s) && calibValid(st.c.gain, st.c.offset)) {
    settings = st.s;
    calib = st.c;
  }
  HwStored hs;
  EEPROM.get(sizeof(Stored), hs);
  if (hs.magic == HW_MAGIC && hs.sum == hwChecksum(hs) && hwValid(hs.h)) hw = hs.h;
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
