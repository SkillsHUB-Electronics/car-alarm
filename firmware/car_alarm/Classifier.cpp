#include "Classifier.h"
#include <Arduino.h>
#include <math.h>
#include "model_data.h"

#if MODEL_PRESENT
static_assert(MODEL_N_IN == FEAT_COUNT, "model_data.h does not match FEAT_COUNT");
#endif

bool Classifier::present() { return MODEL_PRESENT; }

bool Classifier::classify(const int16_t f[FEAT_COUNT], uint8_t &cls, uint8_t &percent) {
#if MODEL_PRESENT
  float x[MODEL_N_IN];
  for (uint8_t i = 0; i < MODEL_N_IN; i++) {
    x[i] = (f[i] / 1000.0f - pgm_read_float(&MODEL_MEAN[i])) / pgm_read_float(&MODEL_STD[i]);
  }

  float h[MODEL_N_HID];
  for (uint8_t j = 0; j < MODEL_N_HID; j++) {
    float s = pgm_read_float(&MODEL_B1[j]);
    for (uint8_t i = 0; i < MODEL_N_IN; i++) s += pgm_read_float(&MODEL_W1[j * MODEL_N_IN + i]) * x[i];
    h[j] = s > 0 ? s : 0;
  }

  float z[MODEL_N_OUT];
  float zmax = -1e30f;
  for (uint8_t k = 0; k < MODEL_N_OUT; k++) {
    float s = pgm_read_float(&MODEL_B2[k]);
    for (uint8_t j = 0; j < MODEL_N_HID; j++) s += pgm_read_float(&MODEL_W2[k * MODEL_N_HID + j]) * h[j];
    z[k] = s;
    if (s > zmax) zmax = s;
  }

  float sum = 0;
  uint8_t best = 0;
  for (uint8_t k = 0; k < MODEL_N_OUT; k++) {
    z[k] = exp(z[k] - zmax);  // shifted by the max for numeric safety; the best class has exp(0) = 1
    sum += z[k];
    if (z[k] > z[best]) best = k;
  }
  cls = best;
  percent = (uint8_t)(100.0f * z[best] / sum + 0.5f);
  return true;
#else
  (void)f; (void)cls; (void)percent;
  return false;
#endif
}
