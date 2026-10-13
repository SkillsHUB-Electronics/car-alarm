#pragma once
#include "types.h"

// HC-SR04 reader: ~10 Hz, median-of-3 filter, hysteresis between levels.
// Going to a more severe level is immediate; going back to a less severe one only after the safer
// condition has held for DIST_RELEASE_HOLD_MS, so a brief echo dropout does not interrupt an alarm.
// Echo timeouts never raise an alarm: after DIST_TIMEOUTS_TO_UNKNOWN misses the reading becomes "unknown"
// and the level falls back to NORMAL through the same release hold.
class DistanceSensor {
 public:
  void begin();
  void update(unsigned long now);

  int16_t cm() const { return cm_; }  // -1 = no valid reading
  DistLevel level() const { return level_; }

  // Uncalibrated mean of n readings in 0.1 cm (blocking, for calibration / IO test); -1 = no echo.
  int16_t rawX10(uint8_t n);

 private:
  unsigned long readEchoUs();
  int16_t readRawCm();
  static DistLevel nextLevel(DistLevel current, int16_t cm);
  void applyTarget(DistLevel target, unsigned long now);

  unsigned long lastRead_ = 0;
  int16_t samples_[3] = {0, 0, 0};
  uint8_t count_ = 0;
  uint8_t head_ = 0;
  uint8_t timeouts_ = 0;
  int16_t cm_ = -1;
  DistLevel level_ = DistLevel::NORMAL;
  bool releasing_ = false;        // a less severe level is pending
  unsigned long releaseSince_ = 0;  // when the pending release started
};
