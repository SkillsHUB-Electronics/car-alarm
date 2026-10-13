// Car Distance & Shock Alarm - main sketch.
// Libraries: "LiquidCrystal I2C" (Frank de Brabander). Everything else is part of the core.
// See README.md (pin map, behaviour) and PLAN.md (decisions, command protocol D12).

#include <Wire.h>
#include "Alerts.h"
#include "BtLink.h"
#include "Classifier.h"
#include "Commands.h"
#include "DistanceSensor.h"
#include "Display.h"
#include "Motion.h"
#include "Settings.h"
#include "config.h"
#include "types.h"

DistanceSensor distance;
Motion motion;
Alerts alerts;
Display display;
BtLink bt;
Commands commands;
Classifier classifier;

bool motionOk = false;
Status status = Status::NORMAL;
unsigned long statusSince = 0;

// Latest shock/brake event; its status is held for the configured time, then auto-clears (no reset button).
MotionEvent motionKind = MotionEvent::NONE;
unsigned long motionSince = 0;

static Status fromDistance(DistLevel level) {
  switch (level) {
    case DistLevel::WARN: return Status::WARN;
    case DistLevel::DANGER: return Status::DANGER;
    default: return Status::NORMAL;
  }
}

void setup() {
  Serial.begin(9600);  // USB: same line protocol as the BLE module (commands in, events out)
  settingsBegin();
  Wire.begin();
  Wire.setWireTimeout(3000, true);  // a stuck I2C bus must not freeze the alarm
  display.begin();
  display.showBoot("Car Alarm", "Calibrating MPU");

  alerts.begin();
  distance.begin();
  bt.begin();

  motionOk = motion.begin();  // car must be stationary here
  commands.begin(&bt, &distance, &motion, &alerts, &display, &motionOk);
  display.showBoot("Car Alarm", motionOk ? "Ready" : "MPU not found!");
  delay(800);
}

// Periodic raw values for threshold tuning (enabled with the DBG command).
static void debugLine(unsigned long now) {
  static float pkShock = 0, pkBrake = 0, pkAccel = 0;
  static unsigned long lastDebug = 0;
  pkShock = max(pkShock, motion.lastShockDev());
  pkBrake = max(pkBrake, -motion.lastForward());
  pkAccel = max(pkAccel, motion.lastForward());
  if (now - lastDebug < 500) return;
  lastDebug = now;
  if (commands.debug()) {
    Print &o = bt.both();
    o.print(F("DBG,"));
    o.print((uint8_t)status);
    o.print(',');
    o.print(distance.cm());
    o.print(',');
    o.print(motion.lastShockDev(), 2);
    o.print(',');
    o.print(motion.lastForward(), 2);
    o.print(',');
    o.print(pkShock, 2);
    o.print(',');
    o.print(pkBrake, 2);
    o.print(',');
    o.print(pkAccel, 2);
    float a[3];
    motion.lastAccel(a);
    for (uint8_t k = 0; k < 3; k++) {  // ax, ay, az in mg (device frame)
      o.print(',');
      o.print((int)(a[k] * 1000.0f));
    }
    o.print('\n');
  }
  pkShock = pkBrake = pkAccel = 0;
}

void loop() {
  unsigned long now = millis();

  commands.update(now);
  distance.update(now);

  float g = 0;
  MotionEvent ev = motion.update(now, g);
  if (ev != MotionEvent::NONE) {
    motionKind = ev;
    motionSince = now;
    bt.sendMotion(ev, g, now);
  }

  int16_t feat[FEAT_COUNT];
  if (motion.takeFeatures(feat)) {
    if (commands.recording()) bt.sendFeatures(commands.label(), feat);
    uint8_t cls, pct;
    if (classifier.classify(feat, cls, pct)) bt.sendClass(cls, pct);
  }

  Status next;
  if (motionKind != MotionEvent::NONE && now - motionSince < settings.holdS * 1000UL) {
    next = (motionKind == MotionEvent::SHOCK) ? Status::SHOCK : Status::BRAKE;
  } else {
    motionKind = MotionEvent::NONE;
    next = fromDistance(distance.level());
  }
  if (next != status) {
    status = next;
    statusSince = now;
  }

  // While the host runs the output test it owns the LEDs, buzzer and LCD.
  if (!commands.ioActive()) {
    alerts.update(status, now, motionKind != MotionEvent::NONE ? motionSince : statusSince);
    display.update(status, distance.cm(), motionOk, now);
  }
  bt.update(distance.level(), distance.cm(), now);
  debugLine(now);
}
