#pragma once
#include <Arduino.h>
#include "config.h"

// Reads a PT100 sensor connected via a 0-10 V transmitter.
float readPT100(uint8_t pin, bool& faultOut);
