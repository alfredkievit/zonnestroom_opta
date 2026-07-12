#include "analog_input.h"
#include <math.h>

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

    // Reject implausible instantaneous jumps (e.g. EMI on the 0-10V line when
    // the boiler element relay switches). A jump only becomes a real fault
    // once it persists for SENSOR_GLITCH_FAULT_MS; momentary spikes are held
    // at the last good value so they never reach the control logic.
    static bool          haveGood      = false;
    static float         lastGoodC     = 0.0f;
    static unsigned long lastGoodMs    = 0;
    static unsigned long glitchSinceMs = 0;

    unsigned long now = millis();

    if (!haveGood) {
        haveGood   = true;
        lastGoodC  = tempC;
        lastGoodMs = now;
        faultOut   = false;
        return tempC;
    }

    float dtSeconds = (now - lastGoodMs) / 1000.0f;
    float maxDelta  = fmaxf(SENSOR_MIN_DELTA_C, SENSOR_MAX_RATE_C_PER_SEC * dtSeconds);

    if (fabsf(tempC - lastGoodC) > maxDelta) {
        if (glitchSinceMs == 0) {
            glitchSinceMs = now;
        }
        if ((now - glitchSinceMs) >= SENSOR_GLITCH_FAULT_MS) {
            // Deviation persisted - accept it as the new real reading and fault.
            lastGoodC     = tempC;
            lastGoodMs    = now;
            glitchSinceMs = 0;
            faultOut      = true;
            return tempC;
        }
        faultOut = false;
        return lastGoodC;
    }

    glitchSinceMs = 0;
    lastGoodC     = tempC;
    lastGoodMs    = now;
    faultOut      = false;
    return tempC;
}
