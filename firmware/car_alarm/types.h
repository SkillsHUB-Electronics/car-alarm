#pragma once
#include <Arduino.h>

// Level derived from the HC-SR04 distance only.
enum class DistLevel : uint8_t { NORMAL, WARN, DANGER };

// Overall system status (what the user sees/hears). Priority: SHOCK/BRAKE > DANGER > WARN > NORMAL.
enum class Status : uint8_t { NORMAL, WARN, DANGER, SHOCK, BRAKE };

enum class MotionEvent : uint8_t { NONE, SHOCK, BRAKE };
