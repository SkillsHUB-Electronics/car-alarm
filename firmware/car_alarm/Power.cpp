#include "Power.h"
#include <Wire.h>
#include "Settings.h"

Power power;

#if POWER_ENABLED
namespace {
constexpr uint8_t REG_SHUNT = 0x01;  // 10 uV per bit
constexpr uint8_t REG_BUS = 0x02;    // bits 15..3, 4 mV per bit

bool readReg(uint8_t reg, int16_t &out) {
  Wire.beginTransmission(INA_ADDR);
  Wire.write(reg);
  if (Wire.endTransmission() != 0) return false;
  if (Wire.requestFrom((uint8_t)INA_ADDR, (uint8_t)2) != 2) return false;
  uint8_t hi = Wire.read();
  uint8_t lo = Wire.read();
  out = (int16_t)((hi << 8) | lo);
  return true;
}
}  // namespace

void Power::begin() { pinMode(PIN_CHRG, INPUT_PULLUP); }

PowerReading Power::read() {
  PowerReading r = {false, 0, 0, false};
  if (hw.charger) r.charging = digitalRead(PIN_CHRG) == LOW;
  if (hw.ina) {
    int16_t bus, shunt;
    if (readReg(REG_BUS, bus) && readReg(REG_SHUNT, shunt)) {
      r.inaOk = true;
      int32_t mv = ((uint16_t)bus >> 3) * 4L;
      int32_t ma = ((int32_t)shunt * 10) / INA_SHUNT_MOHM;  // uV / mOhm = mA
      r.mV = (int16_t)(mv * pcal.vGain / 1000);
      r.mA = ma * pcal.iGain / 1000 + pcal.iOffsetMa;
    }
  }
  return r;
}
#else
void Power::begin() {}
PowerReading Power::read() { return {false, 0, 0, false}; }
#endif
