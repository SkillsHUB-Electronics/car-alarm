// Test 1.3: HC-SR04 distance.
// Compare with a ruler at 20, 50, 100, 200 cm and note the error.
// A timeout is reported as "out of range" and must never be treated as "close".

const uint8_t PIN_TRIG = 2;
const uint8_t PIN_ECHO = 3;

const unsigned long ECHO_TIMEOUT_US = 25000UL;  // ~4.3 m round trip
const float SPEED_OF_SOUND_CM_PER_US = 0.0343f;  // at ~20 C

// Returns distance in cm, or -1 on timeout.
float readDistanceCm() {
  digitalWrite(PIN_TRIG, LOW);
  delayMicroseconds(2);
  digitalWrite(PIN_TRIG, HIGH);
  delayMicroseconds(10);
  digitalWrite(PIN_TRIG, LOW);

  unsigned long duration = pulseIn(PIN_ECHO, HIGH, ECHO_TIMEOUT_US);
  if (duration == 0) return -1;
  return duration * SPEED_OF_SOUND_CM_PER_US / 2.0f;
}

void setup() {
  Serial.begin(9600);
  pinMode(PIN_TRIG, OUTPUT);
  pinMode(PIN_ECHO, INPUT);
}

void loop() {
  float cm = readDistanceCm();
  if (cm < 0) {
    Serial.println(F("out of range / timeout"));
  } else {
    Serial.print(cm, 1);
    Serial.println(F(" cm"));
  }
  delay(100);  // ~10 Hz
}
