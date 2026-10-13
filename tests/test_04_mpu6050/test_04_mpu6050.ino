// Test 1.4: MPU6050 raw register read via Wire (no library).
// Expected at rest: magnitude close to 1.00 g. Shake the board: magnitude jumps.
// AD0 pin to GND -> address 0x68.

#include <Wire.h>

const uint8_t MPU_ADDR = 0x68;
const float LSB_PER_G = 16384.0f;  // +/-2 g range

bool readAccel(float &ax, float &ay, float &az) {
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(0x3B);  // ACCEL_XOUT_H
  if (Wire.endTransmission(false) != 0) return false;
  if (Wire.requestFrom(MPU_ADDR, (uint8_t)6) != 6) return false;

  int16_t rx = (Wire.read() << 8) | Wire.read();
  int16_t ry = (Wire.read() << 8) | Wire.read();
  int16_t rz = (Wire.read() << 8) | Wire.read();
  ax = rx / LSB_PER_G;
  ay = ry / LSB_PER_G;
  az = rz / LSB_PER_G;
  return true;
}

void setup() {
  Serial.begin(9600);
  Wire.begin();
  Wire.setWireTimeout(3000, true);  // a stuck I2C bus must not freeze the sketch

  Wire.beginTransmission(MPU_ADDR);
  Wire.write(0x6B);  // PWR_MGMT_1
  Wire.write(0);     // wake up
  if (Wire.endTransmission() != 0) {
    Serial.println(F("MPU6050 not found at 0x68"));
  }
}

void loop() {
  float ax, ay, az;
  if (!readAccel(ax, ay, az)) {
    Serial.println(F("read failed"));
    delay(500);
    return;
  }
  float mag = sqrt(ax * ax + ay * ay + az * az);

  Serial.print(F("ax=")); Serial.print(ax, 2);
  Serial.print(F(" ay=")); Serial.print(ay, 2);
  Serial.print(F(" az=")); Serial.print(az, 2);
  Serial.print(F(" |a|=")); Serial.println(mag, 2);
  delay(100);
}
