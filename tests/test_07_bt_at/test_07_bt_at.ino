// Test 1.5b: USB <-> HC-05 pass-through for AT commands (identify the module).
// USB serial: 115200 baud. HC-05 in AT mode talks at 38400 (some clones use 9600).
//
// Enter AT mode: HC-05 EN/KEY must be HIGH while the module powers up. KEY is driven from D4
// through a 1k/2k divider (KEY,1), then power-cycle the module's VCC. LED blinks slowly
// (about once every 2 s) in AT mode. Then send e.g.: AT, AT+VERSION?, AT+NAME?, AT+ADDR?
// Wiring: HC-05 TX -> D10, HC-05 RX <- D11 via the 1k/2k divider (see README).
//
// Local commands (not forwarded to the module):
//   BAUD,<n>    change the HC-05 side baud rate
//   KEY,1 / KEY,0   drive the HC-05 EN/KEY pin from D4
//   SWAP,1 / SWAP,0 swap the SoftwareSerial pins (RX=D11, TX=D10) to detect crossed TX/RX wiring
//   LOOP        print the idle level of D10 and D11 (wiring sanity check)

#include <SoftwareSerial.h>

const uint8_t PIN_A = 10;      // normal: Arduino RX (HC-05 TX)
const uint8_t PIN_B = 11;      // normal: Arduino TX (HC-05 RX, via divider)
const uint8_t PIN_BT_KEY = 4;  // HC-05 EN/KEY via 1k (from D4) + 2k (to GND) divider

SoftwareSerial *bt = nullptr;
long btBaud = 38400;
bool swapped = false;

char line[24];
uint8_t len = 0;

void openBt() {
  if (bt) delete bt;
  bt = swapped ? new SoftwareSerial(PIN_B, PIN_A) : new SoftwareSerial(PIN_A, PIN_B);
  bt->begin(btBaud);
}

void setup() {
  // Some HC-05 clones stay in AT mode only while KEY is HIGH, so hold it HIGH from boot.
  pinMode(PIN_BT_KEY, OUTPUT);
  digitalWrite(PIN_BT_KEY, HIGH);
  Serial.begin(115200);
  openBt();
  Serial.println(F("BT-AT bridge ready (HC-05 side: 38400)"));
}

void handleLocal(const char *cmd, bool &handled) {
  handled = true;
  if (strncmp_P(cmd, PSTR("BAUD,"), 5) == 0) {
    long b = atol(cmd + 5);
    if (b > 0) {
      btBaud = b;
      openBt();
      Serial.print(F("HC-05 side baud = "));
      Serial.println(b);
    }
  } else if (strncmp_P(cmd, PSTR("KEY,"), 4) == 0) {
    digitalWrite(PIN_BT_KEY, cmd[4] == '1' ? HIGH : LOW);
    Serial.print(F("KEY pin = "));
    Serial.println(cmd[4] == '1' ? 1 : 0);
  } else if (strncmp_P(cmd, PSTR("SWAP,"), 5) == 0) {
    swapped = cmd[5] == '1';
    openBt();
    Serial.print(F("swapped = "));
    Serial.println(swapped ? 1 : 0);
  } else if (strncmp_P(cmd, PSTR("RAW,"), 4) == 0) {
    // HM-10 style modules want the AT text without CR/LF.
    bt->print(cmd + 4);
  } else if (strcmp_P(cmd, PSTR("LOOP")) == 0) {
    if (bt) bt->end();
    pinMode(PIN_A, INPUT);
    Serial.print(F("D10 idle level = "));
    Serial.println(digitalRead(PIN_A));
    Serial.print(F("D11 idle level = "));
    pinMode(PIN_B, INPUT);
    Serial.println(digitalRead(PIN_B));
    openBt();
  } else {
    handled = false;
  }
}

void loop() {
  while (Serial.available()) {
    char c = Serial.read();
    if (c == '\n') {
      line[len] = '\0';
      bool handled;
      handleLocal(line, handled);
      if (!handled) {
        bt->print(line);
        bt->print(F("\r\n"));
      }
      len = 0;
    } else if (c != '\r' && len < sizeof(line) - 1) {
      line[len++] = c;
    }
  }
  while (bt->available()) {
    Serial.write(bt->read());
  }
}
