#pragma once
#if defined(ARDUINO_ARCH_ESP32)
#include <HardwareSerial.h>
using BtSerial = HardwareSerial;  // ESP32: UART2 on PIN_BT_RX/PIN_BT_TX
#else
#include <SoftwareSerial.h>
using BtSerial = SoftwareSerial;
#endif
#include "config.h"
#include "types.h"

// Prints to USB serial and the BLE module at once.
class TeePrint : public Print {
 public:
  explicit TeePrint(Print &bt) : bt_(bt) {}
  size_t write(uint8_t c) override {
    Serial.write(c);
    return bt_.write(c);
  }

 private:
  Print &bt_;
};

// Line output on USB serial (9600) and the BLE module (HC-05 label, UART 9600) with the same protocol
// `TYPE,SOURCE[,VALUE]\n` (see app/README.md and PLAN.md D12):
//   OK|WARN|ALARM,DIST,<cm>   periodically and on every level change
//   ALARM,SHOCK,<g>           on shock (cooldown per type)
//   ALARM,BRAKE,<g>           on hard braking (cooldown per type)
//   FEAT,<label>,f0..f7       learning features (milli-g), while recording
//   CLS,<class>,<percent>     classifier output, once per feature window
//   RX,BT,<text>              USB only: text received from the paired phone that is not a command
// Command replies (`RSP,...` / `ERR,...`) are written by Commands to the channel the command came from.
class BtLink {
 public:
  BtLink();
  void begin();
  void update(DistLevel level, int16_t cm, unsigned long now);
  void sendMotion(MotionEvent event, float g, unsigned long now);
  void sendFeatures(uint8_t label, const int16_t f[FEAT_COUNT]);
  void sendClass(uint8_t cls, uint8_t percent);

  Stream &ble() { return bt_; }  // the BLE module's UART (command input / replies)
  Print &both() { return out_; }

 private:
  void sendDistance(DistLevel level, int16_t cm);

  BtSerial bt_;
  TeePrint out_;
  unsigned long lastDist_ = 0;
  DistLevel lastLevel_ = DistLevel::NORMAL;
  unsigned long lastMotion_[2] = {0, 0};  // [0] = shock, [1] = brake
  bool sentMotion_[2] = {false, false};
};
