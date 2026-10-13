#pragma once
#include <Arduino.h>
#include "config.h"

// Battery monitoring: INA219 bus voltage + current over I2C (raw registers, no library) and the
// charger's CHRG pin. Enabled at build time (POWER_ENABLED) and at run time (hw.ina / hw.charger).
struct PowerReading {
  bool inaOk;       // INA219 answered
  int16_t mV;       // bus voltage
  int32_t mA;       // current through the shunt (negative = reverse)
  bool charging;    // CHRG pin low
};

class Power {
 public:
  void begin();
  PowerReading read();  // reads the hardware now; fields are zero for parts that are disabled
};

extern Power power;
