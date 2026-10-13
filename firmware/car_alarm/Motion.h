#pragma once
#include "config.h"
#include "types.h"

// MPU6050 shock and hard-braking detector (raw registers via Wire, no library).
// begin() must be called with the car stationary: it records the resting gravity vector as the zero reference.
class Motion {
 public:
  // Returns false if the MPU6050 is missing or calibration failed (distance alarm still works).
  bool begin();

  // Call every loop. Returns an event on the sample that triggers it, otherwise NONE.
  // `g` receives the strength of the event in g.
  MotionEvent update(unsigned long now, float &g);

  bool available() const { return ok_; }
  float lastShockDev() const { return lastDev_; }  // |magnitude - 1 g|, for calibration
  float lastForward() const { return lastLon_; }   // + = accelerating forward, - = braking
  void lastAccel(float v[3]) const {               // latest raw acceleration in g, for the IO test
    for (uint8_t k = 0; k < 3; k++) v[k] = lastA_[k];
  }

  // Learning features, one set per FEAT_WINDOW_MS, in milli-g, in the car frame (forward / vertical / lateral):
  //   0 std vertical   1 std lateral   2 std forward   3 mean forward (- = braking)
  //   4 |mean lateral| (cornering)   5 peak |vertical|   6 min forward   7 max forward
  // Returns true once per completed window.
  bool takeFeatures(int16_t out[FEAT_COUNT]);

 private:
  bool readAccel(float v[3]);
  void accumulate(const float a[3], unsigned long now);
  void resetWindow(unsigned long now);
  static float stdDev(float sum, float sum2, float n);
  static int16_t mg(float g);

  bool ok_ = false;
  float g0_[3] = {0, 0, 0};    // resting vector
  float m0_ = 1.0f;            // resting magnitude
  float filt_[3] = {0, 0, 0};  // low-passed acceleration
  unsigned long lastSample_ = 0;
  uint8_t shockCount_ = 0;
  unsigned long brakeStart_ = 0;
  bool brakeLatched_ = false;
  float lastDev_ = 0;
  float lastLon_ = 0;
  float lastA_[3] = {0, 0, 0};

  uint8_t vAxis_ = 2, lAxis_ = 0;
  unsigned long winStart_ = 0;
  uint16_t n_ = 0;
  float sv_ = 0, sv2_ = 0, sl_ = 0, sl2_ = 0, sf_ = 0, sf2_ = 0, pkV_ = 0, minF_ = 9, maxF_ = -9;
  int16_t featMg_[FEAT_COUNT] = {0};
  bool featReady_ = false;
};
