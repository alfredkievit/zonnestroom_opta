// ============================================================================
// Opta 1 – Energy Master
//
// Priority: 1) WP boiler (fase 1 surplus)
//           2) Boiler element (fase 1 surplus, above WP setpoint)
//           3) Hottub (total surplus, boiler done)
//
// Interlocks applied last – WP and element never active simultaneously.
// All energy loads off on surplus <= 0 or MQTT timeout.
// ============================================================================
#include <Arduino.h>
#include "config.h"
#include "types.h"
#include "analog_input.h"
#include "settings_storage.h"
#include "mqtt_manager.h"
#include "boiler_logic.h"
#include "priority_manager.h"
#include "interlocks.h"
#include "ha_interface.h"

// ── Global state ─────────────────────────────────────────────────────────────
static Settings        gSettings;
static SystemStatus    gStatus;
static AlarmState      gAlarms;
static IOState         gIo;

// ── Module instances ──────────────────────────────────────────────────────────
static SettingsStorage gStorage;
static MqttManager     gMqtt(gStatus, gAlarms, gIo);
static BoilerLogic     gBoilerLogic;
static PriorityManager gPriorityMgr;
static Interlocks      gInterlocks;
static HaInterface     gHa(gMqtt, gSettings, gStatus, gAlarms, gIo, gStorage);

static void updateStatusLed(bool online) {
#if defined(LEDR) && defined(LEDG)
    static unsigned long lastToggleMs = 0;
    static bool blinkOn = false;
    const unsigned long now = millis();
    if ((now - lastToggleMs) >= 600UL) {
        lastToggleMs = now;
        blinkOn = !blinkOn;
    }

    // Opta RGB status LED is active-high on this setup.
    const int onState  = HIGH;
    const int offState = LOW;

    digitalWrite(LEDG, (online && blinkOn) ? onState : offState);
    digitalWrite(LEDR, (!online && blinkOn) ? onState : offState);
#if defined(LEDB)
    digitalWrite(LEDB, offState);
#endif
#endif
}

static void writeOutputWithLed(uint8_t outputPin, int outputState) {
    digitalWrite(outputPin, outputState);
    if (outputPin == PIN_DO_WP_EXTRA_WW) {
        digitalWrite(LED_D0, outputState);
    } else if (outputPin == PIN_DO_WP_COMFORT_EXTRA) {
        digitalWrite(LED_D1, outputState);
    } else if (outputPin == PIN_DO_BOILER_ELEMENT) {
        digitalWrite(LED_D2, outputState);
    } else if (outputPin == PIN_DO_SPARE) {
        digitalWrite(LED_D3, outputState);
    }
}

// ── Helper: write physical outputs ───────────────────────────────────────────
static void writeOutputs() {
    writeOutputWithLed(PIN_DO_WP_EXTRA_WW,      gIo.doWpExtraWW      ? HIGH : LOW);
    writeOutputWithLed(PIN_DO_WP_COMFORT_EXTRA, gIo.doWpComfortExtra  ? HIGH : LOW);
    writeOutputWithLed(PIN_DO_BOILER_ELEMENT,   gIo.doBoilerElement   ? HIGH : LOW);
    // PIN_DO_SPARE not driven by control logic
}

// ── Helper: read physical inputs ─────────────────────────────────────────────
static void readInputs() {
    bool lowFault  = false;
    bool highFault = false;

    gIo.aiBoilerTempLowC  = readPT1000(PIN_AI_BOILER_LOW,  lowFault);
    gIo.aiBoilerTempHighC = readPT1000(PIN_AI_BOILER_HIGH, highFault);

    gAlarms.boilerSensorFault = lowFault || highFault;

    gIo.inElementThermostatOk = (digitalRead(PIN_DI_ELEMENT_THERM) == HIGH);
    gIo.inFaultReset          = (digitalRead(PIN_DI_FAULT_RESET)   == HIGH);

    // Mirror measured temps into status for HA publishing
    gStatus.boilerTempLowC  = gIo.aiBoilerTempLowC;
    gStatus.boilerTempHighC = gIo.aiBoilerTempHighC;
}

// ─────────────────────────────────────────────────────────────────────────────
void setup() {
    Serial.begin(115200);

    // Configure outputs
    pinMode(PIN_DO_WP_EXTRA_WW,      OUTPUT);
    pinMode(PIN_DO_WP_COMFORT_EXTRA, OUTPUT);
    pinMode(PIN_DO_BOILER_ELEMENT,   OUTPUT);
    pinMode(PIN_DO_SPARE,            OUTPUT);

    pinMode(LED_D0, OUTPUT);
    pinMode(LED_D1, OUTPUT);
    pinMode(LED_D2, OUTPUT);
    pinMode(LED_D3, OUTPUT);
#if defined(LEDR) && defined(LEDG)
    pinMode(LEDR, OUTPUT);
    pinMode(LEDG, OUTPUT);
#if defined(LEDB)
    pinMode(LEDB, OUTPUT);
#endif
#endif

    // Safe state on boot
    writeOutputWithLed(PIN_DO_WP_EXTRA_WW,      LOW);
    writeOutputWithLed(PIN_DO_WP_COMFORT_EXTRA, LOW);
    writeOutputWithLed(PIN_DO_BOILER_ELEMENT,   LOW);
    writeOutputWithLed(PIN_DO_SPARE,            LOW);

    // Configure inputs
    pinMode(PIN_DI_ELEMENT_THERM, INPUT);
    pinMode(PIN_DI_FAULT_RESET,   INPUT);

    // ADC resolution: 16-bit for Opta
    analogReadResolution(16);

    // Load settings from flash; fall back to defaults
    gStorage.begin();
    if (!gStorage.load(gSettings)) {
        gSettings = defaultSettings();
    } else if (gSettings.mqttTimeoutSec == 5 || gSettings.mqttTimeoutSec == 10 || gSettings.mqttTimeoutSec == 15) {
        gSettings.mqttTimeoutSec = 30;
        gStorage.save(gSettings);
    }

    // Migration: old default hottub delay was 30s; update it to 180s.
    if (gSettings.tHottubStartDelaySec == 30) {
        gSettings.tHottubStartDelaySec = 180;
        gStorage.save(gSettings);
    }

    // System is intentionally always enabled; do not allow persisted OFF state.
    if (!gSettings.enableSystem) {
        gSettings.enableSystem = true;
        gStorage.save(gSettings);
    }

    // Initialise status to safe/unknown state
    gStatus = {};
    gAlarms = {};
    gIo     = {};

    gMqtt.begin();
    
    // Wire up HaInterface so MQTT manager can forward commands
    gHa.setMqttManager(&gMqtt);
    gMqtt.setHaInterface(&gHa);
}

// ─────────────────────────────────────────────────────────────────────────────
void loop() {
    // 1. MQTT: receive messages, update surplus values, check timeout
    gMqtt.update(gSettings);

    // Status LED above reset: green blink online, red blink offline
    updateStatusLed(gMqtt.connected());

    // 2. Read physical sensors
    readInputs();

    // 3. Evaluate boiler zone logic → sets request flags in gStatus
    gBoilerLogic.evaluate(gSettings, gIo, gStatus);

    // 4. State machine: translate requests into desired output states in gIo
    gPriorityMgr.update(gSettings, gStatus, gAlarms, gIo);

    // 5. Apply interlocks (LAST before writing)
    IOState finalIo = gIo;
    gInterlocks.apply(gSettings, gIo, gStatus, gAlarms, finalIo);
    gIo = finalIo;

    // 6. Write physical relay outputs
    writeOutputs();

    // 7. Home Assistant: publish status, receive commands, send heartbeat
    gHa.update();
}
