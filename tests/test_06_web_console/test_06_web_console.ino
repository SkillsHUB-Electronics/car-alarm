// Test 1.7 (legacy): interface test firmware that was driven by an old browser page (removed; the console Test IO page now does this with the main firmware).
// Talks to the PC over USB serial at 115200 baud (Web Serial), line based, '\n' terminated.
// Library: "LiquidCrystal I2C" by Frank de Brabander.
//
// Commands (PC -> Arduino):
//   PING                 -> PONG
//   LED,<R|G|Y>,<0|1>    set a LED
//   BUZ,<0|1>            buzzer on/off
//   BEEP,<ms>            buzzer on for ms (50..2000), non-blocking
//   LCD,CLR              clear LCD
//   LCD,BL,<0|1>         backlight
//   LCD,<0|1>,<text>     write text (max 16 chars) on row 0 or 1
//   BT,<text>            send text + newline to the HC-05
//   STREAM,<0|1>         start/stop sensor stream
//   SCAN                 I2C scan
//   ALL,0                everything off
//   BTMIRROR,<0|1>       mirror DIST/SHOCK to the phone in the app protocol (default on)
//   BTBAUD,<n>           HC-05 side baud (default 9600)
//   SRTEST / SRPIN,t,e   HC-SR04 wiring diagnostics
// Output (Arduino -> PC):
//   HELLO,TEST06         sent once at boot
//   DIST,<cm>            distance in cm, -1 = timeout (no echo)
//   MPU,<ax>,<ay>,<az>,<mag>   in g, or MPU,ERR
//   OUT,<r>,<g>,<y>,<buzzer>   real pin states
//   I2C,0x27 ... I2CEND,<n>    scan result
//   BTRX,<text>          line received from the HC-05
//   ACK,<cmd> / ERR,<reason>

#include <Wire.h>
#include <SoftwareSerial.h>
#include <LiquidCrystal_I2C.h>

const uint8_t PIN_TRIG = 2;
const uint8_t PIN_ECHO = 3;
const uint8_t PIN_BT_RX = 10;  // Arduino receives (HC-05 TX)
const uint8_t PIN_BT_TX = 11;  // Arduino transmits (HC-05 RX, via 1k/2k divider)
const uint8_t PIN_BUZZER = 8;
const uint8_t PIN_LED_RED = 5;
const uint8_t PIN_LED_GREEN = 6;
const uint8_t PIN_LED_YELLOW = 7;

const uint8_t LCD_ADDR = 0x27;
const uint8_t MPU_ADDR = 0x68;
const float LSB_PER_G = 16384.0f;  // +/-2 g range

const unsigned long ECHO_TIMEOUT_US = 25000UL;
const unsigned long STREAM_PERIOD_MS = 200;
const unsigned long OUT_PERIOD_MS = 1000;

LiquidCrystal_I2C lcd(LCD_ADDR, 16, 2);
SoftwareSerial bt(PIN_BT_RX, PIN_BT_TX);

char cmdBuf[40];
uint8_t cmdLen = 0;
bool cmdOverflow = false;

char btBuf[40];
uint8_t btLen = 0;

bool streaming = true;
bool btMirror = true;  // BTMIRROR,<0|1>: send OK/WARN/ALARM,DIST and ALARM,SHOCK lines to the phone
unsigned long buzzOffAt = 0;  // 0 = no timed beep pending

void printOut() {
  Serial.print(F("OUT,"));
  Serial.print(digitalRead(PIN_LED_RED));
  Serial.print(',');
  Serial.print(digitalRead(PIN_LED_GREEN));
  Serial.print(',');
  Serial.print(digitalRead(PIN_LED_YELLOW));
  Serial.print(',');
  Serial.println(digitalRead(PIN_BUZZER));
}

void mpuWake() {
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(0x6B);  // PWR_MGMT_1
  Wire.write(0);     // wake up
  Wire.endTransmission();
}

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

// Returns distance in cm, or -1 on timeout.
float readDistanceCm() {
  digitalWrite(PIN_TRIG, LOW);
  delayMicroseconds(2);
  digitalWrite(PIN_TRIG, HIGH);
  delayMicroseconds(10);
  digitalWrite(PIN_TRIG, LOW);
  unsigned long duration = pulseIn(PIN_ECHO, HIGH, ECHO_TIMEOUT_US);
  if (duration == 0) return -1;
  return duration * 0.0343f / 2.0f;
}

// Wiring check for the HC-SR04: idle level of both pins, then one raw echo measurement with
// the pins in the normal role (trig=D2, echo=D3) and swapped (trig=D3, echo=D2).
// Prints SRTEST,<trig>,<echo>,<echo pulse in us, 0 = no echo>.
unsigned long probeEcho(uint8_t trig, uint8_t echo) {
  pinMode(trig, OUTPUT);
  pinMode(echo, INPUT);
  digitalWrite(trig, LOW);
  delayMicroseconds(2);
  digitalWrite(trig, HIGH);
  delayMicroseconds(10);
  digitalWrite(trig, LOW);
  // 60 ms: a sensor with nothing in range still pulses ~38 ms, which proves it is alive.
  return pulseIn(echo, HIGH, 60000UL);
}

void srTest() {
  pinMode(PIN_TRIG, INPUT);
  pinMode(PIN_ECHO, INPUT);
  Serial.print(F("SRIDLE,D2="));
  Serial.print(digitalRead(PIN_TRIG));
  Serial.print(F(",D3="));
  Serial.println(digitalRead(PIN_ECHO));
  for (uint8_t i = 0; i < 3; i++) {
    Serial.print(F("SRTEST,2,3,"));
    Serial.println(probeEcho(PIN_TRIG, PIN_ECHO));
    delay(60);
  }
  for (uint8_t i = 0; i < 3; i++) {
    Serial.print(F("SRTEST,3,2,"));
    Serial.println(probeEcho(PIN_ECHO, PIN_TRIG));
    delay(60);
  }
  pinMode(PIN_TRIG, OUTPUT);
  pinMode(PIN_ECHO, INPUT);
}

void scanBus() {
  uint8_t found = 0;
  for (uint8_t addr = 1; addr < 127; addr++) {
    Wire.beginTransmission(addr);
    if (Wire.endTransmission() == 0) {
      Serial.print(F("I2C,0x"));
      Serial.println(addr, HEX);
      found++;
    }
  }
  Serial.print(F("I2CEND,"));
  Serial.println(found);
}

void lcdLine(uint8_t row, const char *text) {
  lcd.setCursor(0, row);
  bool end = false;
  for (uint8_t i = 0; i < 16; i++) {
    if (!end && text[i] == '\0') end = true;
    lcd.print(end ? ' ' : text[i]);
  }
}

void allOff() {
  digitalWrite(PIN_LED_RED, LOW);
  digitalWrite(PIN_LED_GREEN, LOW);
  digitalWrite(PIN_LED_YELLOW, LOW);
  digitalWrite(PIN_BUZZER, LOW);
  buzzOffAt = 0;
}

#define IS(s) (strcmp_P(tok, PSTR(s)) == 0)

void handleCommand(char *line) {
  char *tok = strtok(line, ",");
  if (!tok) return;

  if (IS("PING")) {
    Serial.println(F("PONG"));
    return;
  } else if (IS("LED")) {
    char *c = strtok(NULL, ",");
    char *v = strtok(NULL, ",");
    if (!c || !v) { Serial.println(F("ERR,args")); return; }
    uint8_t pin = 0;
    if (c[0] == 'R') pin = PIN_LED_RED;
    else if (c[0] == 'G') pin = PIN_LED_GREEN;
    else if (c[0] == 'Y') pin = PIN_LED_YELLOW;
    else { Serial.println(F("ERR,led")); return; }
    digitalWrite(pin, v[0] == '1' ? HIGH : LOW);
  } else if (IS("BUZ")) {
    char *v = strtok(NULL, ",");
    if (!v) { Serial.println(F("ERR,args")); return; }
    buzzOffAt = 0;
    digitalWrite(PIN_BUZZER, v[0] == '1' ? HIGH : LOW);
  } else if (IS("BEEP")) {
    char *v = strtok(NULL, ",");
    long ms = v ? atol(v) : 200;
    ms = constrain(ms, 50, 2000);
    digitalWrite(PIN_BUZZER, HIGH);
    buzzOffAt = millis() + (unsigned long)ms;
    if (buzzOffAt == 0) buzzOffAt = 1;
  } else if (IS("LCD")) {
    char *sub = strtok(NULL, ",");
    if (!sub) { Serial.println(F("ERR,args")); return; }
    if (strcmp_P(sub, PSTR("CLR")) == 0) {
      lcd.clear();
    } else if (strcmp_P(sub, PSTR("BL")) == 0) {
      char *v = strtok(NULL, ",");
      if (v && v[0] == '1') lcd.backlight(); else lcd.noBacklight();
    } else if (sub[0] == '0' || sub[0] == '1') {
      char *text = strtok(NULL, "");  // rest of the line, may contain commas
      lcdLine(sub[0] - '0', text ? text : "");
    } else {
      Serial.println(F("ERR,lcd"));
      return;
    }
  } else if (IS("BT")) {
    char *text = strtok(NULL, "");
    bt.println(text ? text : "");
  } else if (IS("STREAM")) {
    char *v = strtok(NULL, ",");
    streaming = v && v[0] == '1';
  } else if (IS("SCAN")) {
    scanBus();
  } else if (IS("ALL")) {
    allOff();
  } else if (IS("BTBAUD")) {
    char *v = strtok(NULL, ",");
    long b = v ? atol(v) : 0;
    if (b < 1200 || b > 115200) { Serial.println(F("ERR,baud")); return; }
    bt.begin(b);
  } else if (IS("BTMIRROR")) {
    char *v = strtok(NULL, ",");
    btMirror = v && v[0] == '1';
  } else if (IS("SRTEST")) {
    srTest();
  } else if (IS("SRPIN")) {
    // SRPIN,<trig>,<echo>: probe the sensor on any two free pins (e.g. SRPIN,12,13).
    char *a = strtok(NULL, ",");
    char *b = strtok(NULL, ",");
    int t = a ? atoi(a) : -1, e = b ? atoi(b) : -1;
    if (t < 2 || t > 13 || e < 2 || e > 13 || t == e || t == PIN_BT_RX || t == PIN_BT_TX ||
        e == PIN_BT_RX || e == PIN_BT_TX) {
      Serial.println(F("ERR,pins"));
      return;
    }
    pinMode(e, INPUT);
    Serial.print(F("SRIDLE,echo="));
    Serial.println(digitalRead(e));
    for (uint8_t i = 0; i < 3; i++) {
      Serial.print(F("SRPIN,"));
      Serial.print(t);
      Serial.print(',');
      Serial.print(e);
      Serial.print(',');
      Serial.println(probeEcho(t, e));
      delay(60);
    }
    pinMode(t, INPUT);
    pinMode(e, INPUT);
    pinMode(PIN_TRIG, OUTPUT);
    pinMode(PIN_ECHO, INPUT);
  } else {
    Serial.println(F("ERR,unknown"));
    return;
  }
  Serial.print(F("ACK,"));
  Serial.println(tok);
  printOut();
}

void readPcSerial() {
  while (Serial.available()) {
    char c = Serial.read();
    if (c == '\r') continue;
    if (c == '\n') {
      if (cmdOverflow) {
        Serial.println(F("ERR,too long"));
      } else if (cmdLen > 0) {
        cmdBuf[cmdLen] = '\0';
        handleCommand(cmdBuf);
      }
      cmdLen = 0;
      cmdOverflow = false;
    } else if (cmdLen < sizeof(cmdBuf) - 1) {
      cmdBuf[cmdLen++] = c;
    } else {
      cmdOverflow = true;
    }
  }
}

void readBluetooth() {
  while (bt.available()) {
    char c = bt.read();
    if (c == '\r') continue;
    if (c == '\n') {
      if (btLen > 0) {
        btBuf[btLen] = '\0';
        Serial.print(F("BTRX,"));
        Serial.println(btBuf);
      }
      btLen = 0;
    } else if (btLen < sizeof(btBuf) - 1) {
      btBuf[btLen++] = c;
    }
  }
}

void setup() {
  pinMode(PIN_TRIG, OUTPUT);
  pinMode(PIN_ECHO, INPUT);
  pinMode(PIN_BUZZER, OUTPUT);
  pinMode(PIN_LED_RED, OUTPUT);
  pinMode(PIN_LED_GREEN, OUTPUT);
  pinMode(PIN_LED_YELLOW, OUTPUT);
  allOff();

  Serial.begin(115200);
  bt.begin(9600);
  Wire.begin();
  Wire.setWireTimeout(3000, true);  // a stuck I2C bus must not freeze the sketch
  mpuWake();
  lcd.init();
  lcd.backlight();
  lcdLine(0, "Web test ready");
  lcdLine(1, "");

  Serial.println(F("HELLO,TEST06"));
  printOut();
}

void loop() {
  readPcSerial();
  readBluetooth();

  if (buzzOffAt != 0 && (long)(millis() - buzzOffAt) >= 0) {
    digitalWrite(PIN_BUZZER, LOW);
    buzzOffAt = 0;
    printOut();
  }

  static unsigned long lastStream = 0;
  if (streaming && millis() - lastStream >= STREAM_PERIOD_MS) {
    lastStream = millis();

    float cm = readDistanceCm();
    Serial.print(F("DIST,"));
    Serial.println(cm, 1);

    // Mirror the production protocol to the phone so the Android app can be tested with this sketch.
    // A timeout (cm < 0) is never reported as a close object.
    if (btMirror && cm >= 0) {
      int icm = (int)(cm + 0.5f);
      bt.print(icm <= 50 ? F("ALARM") : (icm <= 100 ? F("WARN") : F("OK")));
      bt.print(F(",DIST,"));
      bt.println(icm);
    }

    float ax, ay, az;
    if (readAccel(ax, ay, az)) {
      Serial.print(F("MPU,"));
      Serial.print(ax, 3);
      Serial.print(',');
      Serial.print(ay, 3);
      Serial.print(',');
      Serial.print(az, 3);
      Serial.print(',');
      float mag = sqrt(ax * ax + ay * ay + az * az);
      Serial.println(mag, 3);

      static unsigned long lastShockBt = 0;
      if (btMirror && fabs(mag - 1.0f) > 0.6f && millis() - lastShockBt >= 3000) {
        lastShockBt = millis();
        bt.print(F("ALARM,SHOCK,"));
        bt.println(fabs(mag - 1.0f), 1);
      }
    } else {
      Serial.println(F("MPU,ERR"));
      mpuWake();
    }
  }

  static unsigned long lastOut = 0;
  if (millis() - lastOut >= OUT_PERIOD_MS) {
    lastOut = millis();
    printOut();
  }
}
