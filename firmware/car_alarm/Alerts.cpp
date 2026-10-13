#include "Alerts.h"
#include "config.h"
#include "Settings.h"

void Alerts::begin() {
  pinMode(PIN_BUZZER, OUTPUT);
  pinMode(PIN_LED_RED, OUTPUT);
  pinMode(PIN_LED_YELLOW, OUTPUT);
  pinMode(PIN_LED_GREEN, OUTPUT);
  set(false, false, false, false);
}

void Alerts::set(bool red, bool yellow, bool green, bool buzzer) {
  digitalWrite(PIN_LED_RED, red);
  digitalWrite(PIN_LED_YELLOW, yellow);
  digitalWrite(PIN_LED_GREEN, green);
  digitalWrite(PIN_BUZZER, buzzer && settings.buzzer);
}

// Test-IO mode: drive the outputs directly (the buzzer ignores the Settings mute).
void Alerts::test(bool red, bool yellow, bool green, bool buzzer) {
  digitalWrite(PIN_LED_RED, red);
  digitalWrite(PIN_LED_YELLOW, yellow);
  digitalWrite(PIN_LED_GREEN, green);
  digitalWrite(PIN_BUZZER, buzzer);
}

void Alerts::update(Status status, unsigned long now, unsigned long since) {
  switch (status) {
    case Status::NORMAL:
      set(false, false, true, false);
      break;
    case Status::WARN:
      set(false, (now / 250) % 2 == 0, false, (now % 1000) < 100);
      break;
    case Status::DANGER:
      set((now / 100) % 2 == 0, false, false, (now % 200) < 100);
      break;
    case Status::SHOCK:
    case Status::BRAKE:
      set(true, false, false, (now - since) < MOTION_BUZZ_MS);
      break;
  }
}
