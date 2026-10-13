#pragma once
// Per-build options. Edit here, or override on the command line, e.g.
//   arduino-cli compile --build-property "build.extra_flags=-DPOWER_ENABLED=1" ...
// Defaults keep the original UNO footprint (Flash 71%, SRAM 60%): the power module is compiled out.

#ifndef POWER_ENABLED
#define POWER_ENABLED 0       // 1 = compile the Power module (INA219 + charger CHRG pin)
#endif

#ifndef PIN_CHRG
#if defined(ARDUINO_ARCH_ESP32)
#define PIN_CHRG 13           // GPIO13 (GPIO12 is a strapping pin)
#else
#define PIN_CHRG 12           // TP4056 CHRG (open-drain, LOW while charging); D12 is free on the UNO
#endif
#endif

#ifndef INA_ADDR
#define INA_ADDR 0x40         // INA219 default address (A0/A1 to GND)
#endif

#ifndef INA_SHUNT_MOHM
#define INA_SHUNT_MOHM 100    // shunt resistor in milliohm (INA219 breakout: R100 = 100 mOhm)
#endif
