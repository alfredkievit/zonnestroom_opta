#include "analog_input.h"

float readPT1000(uint8_t pin, bool& faultOut) {
    int raw = analogRead(pin);

    // Linear mapping: 0 ADC → SENSOR_TEMP_AT_0V °C,  ADC_MAX_RAW → SENSOR_TEMP_AT_10V °C
    float tempC = SENSOR_TEMP_AT_0V +
                  (static_cast<float>(raw) / ADC_MAX_RAW) *
                  (SENSOR_TEMP_AT_10V - SENSOR_TEMP_AT_0V);

    if (tempC < SENSOR_TEMP_MIN || tempC > SENSOR_TEMP_MAX) {
        faultOut = true;
        return 0.0f;
    }

    faultOut = false;
    return tempC;
}
