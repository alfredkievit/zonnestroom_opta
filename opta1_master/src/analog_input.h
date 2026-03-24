#pragma once
#include <Arduino.h>
#include "config.h"

// Reads a PT1000 sensor connected via a 0–10 V transmitter.
// Returns temperature in °C, or sets faultOut = true on out-of-range.
float readPT1000(uint8_t pin, bool& faultOut);
