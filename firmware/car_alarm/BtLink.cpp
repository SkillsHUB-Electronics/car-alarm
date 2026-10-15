#include "BtLink.h"
#include "config.h"

#if defined(ARDUINO_ARCH_ESP32)
BtLink::BtLink() : bt_(2), out_(bt_) {}
#else
BtLink::BtLink() : bt_(PIN_BT_RX, PIN_BT_TX), out_(bt_) {}
#endif

void BtLink::begin() {
#if !INC_BT
  return;  // no BLE module in this build: USB serial only
#endif
#if defined(ARDUINO_ARCH_ESP32)
  bt_.begin(9600, SERIAL_8N1, PIN_BT_RX, PIN_BT_TX);  // BLE module default data baud rate
#else
  bt_.begin(9600);  // BLE module default data baud rate
#endif
}

void BtLink::sendDistance(DistLevel level, int16_t cm) {
  switch (level) {
    case DistLevel::NORMAL: out_.print(F("OK,DIST")); break;
    case DistLevel::WARN: out_.print(F("WARN,DIST")); break;
    case DistLevel::DANGER: out_.print(F("ALARM,DIST")); break;
  }
  if (cm >= 0) {
    out_.print(',');
    out_.print(cm);
  }
  out_.print('\n');
}

void BtLink::update(DistLevel level, int16_t cm, unsigned long now) {
  bool changed = level != lastLevel_;
  if (!changed && now - lastDist_ < BT_DIST_PERIOD_MS) return;
  lastLevel_ = level;
  lastDist_ = now;
  sendDistance(level, cm);
}

void BtLink::sendMotion(MotionEvent event, float g, unsigned long now) {
  if (event == MotionEvent::NONE) return;
  uint8_t i = (event == MotionEvent::SHOCK) ? 0 : 1;
  if (sentMotion_[i] && now - lastMotion_[i] < BT_MOTION_COOLDOWN_MS) return;
  sentMotion_[i] = true;
  lastMotion_[i] = now;

  out_.print(i == 0 ? F("ALARM,SHOCK,") : F("ALARM,BRAKE,"));
  out_.print(g, 1);
  out_.print('\n');
}

void BtLink::sendFeatures(uint8_t label, const int16_t f[FEAT_COUNT]) {
  out_.print(F("FEAT,"));
  out_.print(label);
  for (uint8_t i = 0; i < FEAT_COUNT; i++) {
    out_.print(',');
    out_.print(f[i]);
  }
  out_.print('\n');
}

void BtLink::sendClass(uint8_t cls, uint8_t percent) {
  out_.print(F("CLS,"));
  out_.print(cls);
  out_.print(',');
  out_.print(percent);
  out_.print('\n');
}
