#include "Motion.h"
#include <Wire.h>
#include "config.h"
#include "Settings.h"

namespace {
constexpr float LSB_PER_G = 8192.0f;  // +/-4 g range
constexpr uint8_t REG_CONFIG = 0x1A;
constexpr uint8_t REG_ACCEL_CONFIG = 0x1C;
constexpr uint8_t REG_ACCEL_XOUT_H = 0x3B;
constexpr uint8_t REG_PWR_MGMT_1 = 0x6B;

bool writeReg(uint8_t reg, uint8_t value) {
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(reg);
  Wire.write(value);
  return Wire.endTransmission() == 0;
}
}  // namespace

bool Motion::readAccel(float v[3]) {
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(REG_ACCEL_XOUT_H);
  if (Wire.endTransmission(false) != 0) return false;
  if (Wire.requestFrom(MPU_ADDR, (uint8_t)6) != 6) return false;

  for (uint8_t i = 0; i < 3; i++) {
    int16_t raw = (Wire.read() << 8) | Wire.read();
    v[i] = raw / LSB_PER_G;
  }
  return true;
}

bool Motion::begin() {
  ok_ = false;
#if !INC_MPU
  return false;  // no MPU6050 in this build: no shock/brake detection
#endif
  if (!writeReg(REG_PWR_MGMT_1, 0)) return false;  // wake up
  writeReg(REG_CONFIG, 0x03);                      // digital low-pass ~44 Hz: less engine/road vibration
  writeReg(REG_ACCEL_CONFIG, 0x08);                // +/-4 g
  delay(100);

  float sum[3] = {0, 0, 0};
  uint8_t good = 0;
  for (uint8_t i = 0; i < 100; i++) {
    float v[3];
    if (readAccel(v)) {
      for (uint8_t k = 0; k < 3; k++) sum[k] += v[k];
      good++;
    }
    delay(10);
  }
  if (good < 50) return false;

  for (uint8_t k = 0; k < 3; k++) {
    g0_[k] = sum[k] / good;
    filt_[k] = g0_[k];
  }
  m0_ = sqrt(g0_[0] * g0_[0] + g0_[1] * g0_[1] + g0_[2] * g0_[2]);

  // Car frame for the learning features: forward = FORWARD_AXIS; vertical = the other axis that carries
  // most of gravity at rest; lateral = the remaining one.
  uint8_t other[2], n = 0;
  for (uint8_t k = 0; k < 3; k++) {
    if (k != FORWARD_AXIS) other[n++] = k;
  }
  vAxis_ = (fabs(g0_[other[0]]) >= fabs(g0_[other[1]])) ? other[0] : other[1];
  lAxis_ = (vAxis_ == other[0]) ? other[1] : other[0];
  resetWindow(millis());

  ok_ = true;
  return true;
}

MotionEvent Motion::update(unsigned long now, float &g) {
  if (!ok_ || now - lastSample_ < MOTION_PERIOD_MS) return MotionEvent::NONE;
  lastSample_ = now;

  float a[3];
  if (!readAccel(a)) return MotionEvent::NONE;
  for (uint8_t k = 0; k < 3; k++) lastA_[k] = a[k];
  accumulate(a, now);

  // Shock: magnitude deviates from the resting 1 g for a few consecutive samples.
  float mag = sqrt(a[0] * a[0] + a[1] * a[1] + a[2] * a[2]);
  lastDev_ = fabs(mag - m0_);
  shockCount_ = (lastDev_ > settings.shock100 / 100.0f) ? shockCount_ + 1 : 0;

  // Braking: low-passed acceleration along the forward axis, relative to the resting vector.
  for (uint8_t k = 0; k < 3; k++) filt_[k] += MOTION_LPF_ALPHA * (a[k] - filt_[k]);
  lastLon_ = FORWARD_SIGN * (filt_[FORWARD_AXIS] - g0_[FORWARD_AXIS]);

  if (shockCount_ >= SHOCK_SAMPLES) {
    shockCount_ = 0;
    g = lastDev_;
    return MotionEvent::SHOCK;
  }

  const float brakeG = settings.brake100 / 100.0f;
  if (lastLon_ < -brakeG) {
    if (brakeStart_ == 0) {
      brakeStart_ = now;
    } else if (!brakeLatched_ && now - brakeStart_ >= settings.brakeMs) {
      brakeLatched_ = true;  // one event per braking maneuver
      g = -lastLon_;
      return MotionEvent::BRAKE;
    }
  } else {
    brakeStart_ = 0;
    if (lastLon_ > -brakeG * 0.5f) brakeLatched_ = false;  // re-arm once braking has eased
  }
  return MotionEvent::NONE;
}

void Motion::resetWindow(unsigned long now) {
  winStart_ = now;
  n_ = 0;
  sv_ = sv2_ = sl_ = sl2_ = sf_ = sf2_ = 0;
  pkV_ = 0;
  minF_ = 9;
  maxF_ = -9;
}

// Accumulates one sample (deviation from the resting vector, in the car frame) into the current window.
void Motion::accumulate(const float a[3], unsigned long now) {
  float v = a[vAxis_] - g0_[vAxis_];
  float l = a[lAxis_] - g0_[lAxis_];
  float f = FORWARD_SIGN * (a[FORWARD_AXIS] - g0_[FORWARD_AXIS]);
  n_++;
  sv_ += v;
  sv2_ += v * v;
  sl_ += l;
  sl2_ += l * l;
  sf_ += f;
  sf2_ += f * f;
  if (fabs(v) > pkV_) pkV_ = fabs(v);
  if (f < minF_) minF_ = f;
  if (f > maxF_) maxF_ = f;

  if (now - winStart_ < FEAT_WINDOW_MS) return;
  if (n_ >= FEAT_MIN_SAMPLES) {
    float n = n_;
    featMg_[0] = mg(stdDev(sv_, sv2_, n));
    featMg_[1] = mg(stdDev(sl_, sl2_, n));
    featMg_[2] = mg(stdDev(sf_, sf2_, n));
    featMg_[3] = mg(sf_ / n);
    featMg_[4] = mg(fabs(sl_ / n));
    featMg_[5] = mg(pkV_);
    featMg_[6] = mg(minF_);
    featMg_[7] = mg(maxF_);
    featReady_ = true;
  }
  resetWindow(now);
}

float Motion::stdDev(float sum, float sum2, float n) {
  float mean = sum / n;
  float var = sum2 / n - mean * mean;
  return var > 0 ? sqrt(var) : 0;
}

int16_t Motion::mg(float g) {
  float v = g * 1000.0f;
  if (v > 32000) v = 32000;
  if (v < -32000) v = -32000;
  return (int16_t)(v + (v >= 0 ? 0.5f : -0.5f));
}

bool Motion::takeFeatures(int16_t out[FEAT_COUNT]) {
  if (!featReady_) return false;
  featReady_ = false;
  for (uint8_t i = 0; i < FEAT_COUNT; i++) out[i] = featMg_[i];
  return true;
}
