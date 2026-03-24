#pragma once
#include <Arduino.h>
#include "config.h"

float readPT1000(uint8_t pin, bool& faultOut);
