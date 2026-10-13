// Test 8: INA219 (bus voltage + current) and charger CHRG pin. No library needed.
// Wiring: INA219 on the I2C bus (default 0x40), VIN+ to the supply side, VIN- to the load side;
// TP4056 CHRG pin to D12 (low while charging). Open the Serial Monitor at 9600 baud.
// Expected: "INA219 found", voltage near the battery voltage, current changes with the load,
// and "charging" while a charger is connected and the cell is not full.

#include <Wire.h>

const uint8_t INA_ADDR = 0x40;
const uint16_t SHUNT_MOHM = 100;  // INA219 breakout with R100 shunt
const uint8_t PIN_CHRG = 12;

bool readReg(uint8_t reg, int16_t &out) {
  Wire.beginTransmission(INA_ADDR);
  Wire.write(reg);
  if (Wire.endTransmission() != 0) return false;
  if (Wire.requestFrom(INA_ADDR, (uint8_t)2) != 2) return false;
  uint8_t hi = Wire.read();
  uint8_t lo = Wire.read();
  out = (int16_t)((hi << 8) | lo);
  return true;
}

void setup() {
  Serial.begin(9600);
  Wire.begin();
  pinMode(PIN_CHRG, INPUT_PULLUP);
  int16_t probe;
  Serial.println(readReg(0x01, probe) ? F("INA219 found") : F("INA219 NOT found: check wiring/address"));
}

void loop() {
  int16_t bus, shunt;
  if (readReg(0x02, bus) && readReg(0x01, shunt)) {
    long mV = ((uint16_t)bus >> 3) * 4L;
    long mA = (long)shunt * 10 / SHUNT_MOHM;
    Serial.print(mV);
    Serial.print(F(" mV, "));
    Serial.print(mA);
    Serial.print(F(" mA, "));
  } else {
    Serial.print(F("INA read failed, "));
  }
  Serial.println(digitalRead(PIN_CHRG) == LOW ? F("charging") : F("not charging"));
  delay(1000);
}
