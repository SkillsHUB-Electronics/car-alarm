#pragma once
#include <Arduino.h>
#include "config.h"

// Tiny MLP (FEAT_COUNT -> hidden ReLU -> softmax) trained in the web console; weights live in PROGMEM
// (model_data.h). Output is informational only: it never changes the alarm logic.
class Classifier {
 public:
  static bool present();
  // Features in milli-g (Motion::takeFeatures). Returns false when no model is compiled in.
  bool classify(const int16_t f[FEAT_COUNT], uint8_t &cls, uint8_t &percent);
};
