// Test 1.5: HC-05 data mode over SoftwareSerial.
// Pair the phone with HC-05 first (PIN 1234 or 0000), open "Serial Bluetooth Terminal"
// and connect. Expected: phone receives "TEST,BT,<n>" every 2 s; text typed on the
// phone is echoed in the Arduino Serial Monitor (9600) and back to the phone.
//
// Wiring: HC-05 TX -> D10, HC-05 RX <- D11 through a 1k/2k divider (5V -> 3.3V).
// HC-05 default data baud rate is 9600.

#include <SoftwareSerial.h>

const uint8_t PIN_BT_RX = 10;  // Arduino receives here (HC-05 TX)
const uint8_t PIN_BT_TX = 11;  // Arduino transmits here (HC-05 RX, via divider)

SoftwareSerial bt(PIN_BT_RX, PIN_BT_TX);

void setup() {
  Serial.begin(9600);
  bt.begin(9600);
  Serial.println(F("HC-05 test ready"));
}

void loop() {
  static unsigned long lastSend = 0;
  static uint16_t counter = 0;

  if (millis() - lastSend >= 2000) {
    lastSend = millis();
    bt.print(F("TEST,BT,"));
    bt.println(counter++);
  }

  while (bt.available()) {
    char c = bt.read();
    Serial.write(c);  // phone -> PC
    bt.write(c);      // echo back to phone
  }
}
