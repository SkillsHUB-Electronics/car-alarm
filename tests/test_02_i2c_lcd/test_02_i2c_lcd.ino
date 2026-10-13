// Test 1.2 + 1.6: I2C scan and LCD 16x2 text.
// Library: "LiquidCrystal I2C" by Frank de Brabander.
// Expected with LCD and MPU6050 on the same bus: scan lists 0x27 (or 0x3F) and 0x68.

#include <Wire.h>
#include <LiquidCrystal_I2C.h>

const uint8_t LCD_ADDR = 0x27;  // change to 0x3F if the scan shows that address

LiquidCrystal_I2C lcd(LCD_ADDR, 16, 2);

void scanBus() {
  Serial.println(F("I2C scan..."));
  uint8_t found = 0;
  for (uint8_t addr = 1; addr < 127; addr++) {
    Wire.beginTransmission(addr);
    if (Wire.endTransmission() == 0) {
      Serial.print(F("  found 0x"));
      Serial.println(addr, HEX);
      found++;
    }
  }
  if (found == 0) Serial.println(F("  no device found, check SDA/SCL wiring"));
}

void setup() {
  Serial.begin(9600);
  Wire.begin();
  Wire.setWireTimeout(3000, true);  // a stuck I2C bus must not freeze the sketch
  scanBus();

  lcd.init();
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print(F("Car Alarm Test"));
  lcd.setCursor(0, 1);
  lcd.print(F("LCD OK"));
}

void loop() {
  // Counter on the right of line 2 to prove the display keeps updating.
  static uint16_t count = 0;
  lcd.setCursor(9, 1);
  lcd.print(F("t="));
  lcd.print(count++);
  lcd.print(F("   "));
  delay(1000);
}
