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

// Interfaces that can be left out of the build (-DINC_LCD=0 etc.). A disabled interface is never initialised
// and does nothing at runtime (no traffic on its pins or bus). 1 = present (default).
#ifndef INC_DIST
#define INC_DIST 1            // HC-SR04 distance sensor (off: no distance readings, no distance alarm)
#endif
#ifndef INC_BT
#define INC_BT 1              // Bluetooth/BLE module (off: USB serial only)
#endif
#ifndef INC_BUZZER
#define INC_BUZZER 1
#endif
#ifndef INC_LED
#define INC_LED 1             // red, yellow and green LEDs
#endif
#ifndef INC_LCD
#define INC_LCD 1             // 16x2 I2C LCD
#endif
#ifndef INC_MPU
#define INC_MPU 1             // MPU6050 (off: no shock/brake detection, no learning features)
#endif
