#pragma once
#include "types.h"

// LEDs and buzzer patterns per status. Non-blocking: call every loop.
//   NORMAL : green on
//   WARN   : yellow blinks slowly, short beep every second
//   DANGER : red blinks fast, fast beeps
//   SHOCK/BRAKE : red solid, continuous buzzer for MOTION_BUZZ_MS after `since`
class Alerts {
 public:
  void begin();
  // `since` = time the current motion event (SHOCK/BRAKE) started; ignored for other statuses.
  void update(Status status, unsigned long now, unsigned long since);

  // Test-IO mode: set the outputs directly.
  void test(bool red, bool yellow, bool green, bool buzzer);

 private:
  void set(bool red, bool yellow, bool green, bool buzzer);
};
