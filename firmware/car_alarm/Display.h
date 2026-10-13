#pragma once
#include <LiquidCrystal_I2C.h>
#include "types.h"

// 16x2 I2C LCD. Redraws only when the text changed and at most every LCD_PERIOD_MS.
class Display {
 public:
  Display();
  void begin();
  void apply();  // re-reads settings: LCD address, enable and backlight (takes effect immediately)
  void setBacklight(bool on);
  void showBoot(const char *line1, const char *line2);
  void update(Status status, int16_t cm, bool motionOk, unsigned long now);

 private:
  void print(uint8_t row, const char *text);

  bool enabled() const;

  LiquidCrystal_I2C lcd_;
  char last_[2][17] = {"", ""};
  unsigned long lastDraw_ = 0;
};
