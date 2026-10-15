#include "DistanceSensor.h"
#include "config.h"
#include "Settings.h"

void DistanceSensor::begin() {
#if INC_DIST
  pinMode(PIN_TRIG, OUTPUT);
  pinMode(PIN_ECHO, INPUT);
  digitalWrite(PIN_TRIG, LOW);
#endif
}

// Echo time in microseconds, 0 on timeout.
unsigned long DistanceSensor::readEchoUs() {
#if !INC_DIST
  return 0;  // no sensor in this build: every read is a timeout, which never raises an alarm
#endif
  digitalWrite(PIN_TRIG, LOW);
  delayMicroseconds(2);
  digitalWrite(PIN_TRIG, HIGH);
  delayMicroseconds(10);
  digitalWrite(PIN_TRIG, LOW);
  return pulseIn(PIN_ECHO, HIGH, DIST_ECHO_TIMEOUT_US);
}

// Returns calibrated centimetres, or -1 on timeout / out-of-range.
int16_t DistanceSensor::readRawCm() {
  unsigned long us = readEchoUs();
  if (us == 0) return -1;

  // speed of sound 0.0343 cm/us, round trip; then the stored linear correction
  int16_t cm = (int16_t)(us * 0.01715f * calib.gain + calib.offset + 0.5f);
  if (cm < DIST_MIN_VALID_CM || cm > DIST_MAX_VALID_CM) return -1;
  return cm;
}

// Mean of up to n uncalibrated readings in 0.1 cm, or -1 if none got an echo. Blocks ~60 ms per reading.
int16_t DistanceSensor::rawX10(uint8_t n) {
  unsigned long sum = 0;
  uint8_t good = 0;
  for (uint8_t i = 0; i < n; i++) {
    unsigned long us = readEchoUs();
    if (us != 0) {
      sum += us;
      good++;
    }
    delay(60);  // let the previous echo die out
  }
  if (good == 0) return -1;
  return (int16_t)((sum / good) * 0.1715f + 0.5f);
}

DistLevel DistanceSensor::nextLevel(DistLevel current, int16_t cm) {
  switch (current) {
    case DistLevel::NORMAL:
      if (cm <= settings.dangerCm) return DistLevel::DANGER;
      if (cm <= settings.warnCm) return DistLevel::WARN;
      return DistLevel::NORMAL;
    case DistLevel::WARN:
      if (cm <= settings.dangerCm) return DistLevel::DANGER;
      if (cm > settings.warnCm + DIST_HYST_CM) return DistLevel::NORMAL;
      return DistLevel::WARN;
    case DistLevel::DANGER:
      if (cm > settings.warnCm + DIST_HYST_CM) return DistLevel::NORMAL;
      if (cm > settings.dangerCm + DIST_HYST_CM) return DistLevel::WARN;
      return DistLevel::DANGER;
  }
  return DistLevel::NORMAL;
}

void DistanceSensor::update(unsigned long now) {
  if (now - lastRead_ < DIST_PERIOD_MS) return;
  lastRead_ = now;

  int16_t raw = readRawCm();
  if (raw < 0) {
    if (++timeouts_ >= DIST_TIMEOUTS_TO_UNKNOWN) {
      timeouts_ = DIST_TIMEOUTS_TO_UNKNOWN;
      cm_ = -1;
      count_ = 0;
      // Unknown must never raise an alarm, but it must not cut an active one instantly either:
      // go through the same release hold as a genuinely safer reading.
      applyTarget(DistLevel::NORMAL, now);
    }
    return;  // fewer misses than the limit: keep the previous value
  }

  timeouts_ = 0;
  samples_[head_] = raw;
  head_ = (head_ + 1) % 3;
  if (count_ < 3) count_++;

  if (count_ < 3) {
    cm_ = raw;
  } else {
    int16_t a = samples_[0], b = samples_[1], c = samples_[2];
    // median of three
    cm_ = max(min(a, b), min(max(a, b), c));
  }
  applyTarget(nextLevel(level_, cm_), now);
}

void DistanceSensor::applyTarget(DistLevel target, unsigned long now) {
  if (target > level_) {
    // Closer: react immediately.
    level_ = target;
    releasing_ = false;
  } else if (target < level_) {
    // Safer: only after the safer condition has held continuously for DIST_RELEASE_HOLD_MS.
    if (!releasing_) {
      releasing_ = true;
      releaseSince_ = now;
    } else if (now - releaseSince_ >= DIST_RELEASE_HOLD_MS) {
      level_ = target;
      releasing_ = false;
    }
  } else {
    releasing_ = false;
  }
}
