// Test 1.1: LEDs and buzzer (via NPN transistor).
// Expected: each LED lights for 1 s in turn, then the buzzer beeps 3 times.

const uint8_t PIN_BUZZER = 8;
const uint8_t PIN_LED_RED = 5;
const uint8_t PIN_LED_GREEN = 6;
const uint8_t PIN_LED_YELLOW = 7;

const uint8_t LEDS[] = {PIN_LED_GREEN, PIN_LED_YELLOW, PIN_LED_RED};

void setup() {
  Serial.begin(9600);
  pinMode(PIN_BUZZER, OUTPUT);
  for (uint8_t pin : LEDS) pinMode(pin, OUTPUT);
}

void loop() {
  for (uint8_t pin : LEDS) {
    Serial.print(F("LED pin "));
    Serial.println(pin);
    digitalWrite(pin, HIGH);
    delay(1000);
    digitalWrite(pin, LOW);
  }

  Serial.println(F("Buzzer x3"));
  for (uint8_t i = 0; i < 3; i++) {
    digitalWrite(PIN_BUZZER, HIGH);
    delay(200);
    digitalWrite(PIN_BUZZER, LOW);
    delay(200);
  }
  delay(2000);
}
