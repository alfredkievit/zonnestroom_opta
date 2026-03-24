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

// ── Helper: write physical outputs ───────────────────────────────────────────
static void writeOutputs() {
    digitalWrite(PIN_DO_WP_EXTRA_WW,      gIo.doWpExtraWW      ? HIGH : LOW);
    digitalWrite(PIN_DO_WP_COMFORT_EXTRA, gIo.doWpComfortExtra  ? HIGH : LOW);
    digitalWrite(PIN_DO_BOILER_ELEMENT,   gIo.doBoilerElement   ? HIGH : LOW);
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

    // Safe state on boot
    digitalWrite(PIN_DO_WP_EXTRA_WW,      LOW);
    digitalWrite(PIN_DO_WP_COMFORT_EXTRA, LOW);
    digitalWrite(PIN_DO_BOILER_ELEMENT,   LOW);
    digitalWrite(PIN_DO_SPARE,            LOW);

    // Configure inputs
    pinMode(PIN_DI_ELEMENT_THERM, INPUT);
    pinMode(PIN_DI_FAULT_RESET,   INPUT);

    // ADC resolution: 16-bit for Opta
    analogReadResolution(16);

    // Load settings from flash; fall back to defaults
    gStorage.begin();
    if (!gStorage.load(gSettings)) {
        gSettings = defaultSettings();
    }

    // Initialise status to safe/unknown state
    gStatus = {};
    gAlarms = {};
    gIo     = {};

    gMqtt.begin();
}

// ─────────────────────────────────────────────────────────────────────────────
void loop() {
    // 1. MQTT: receive messages, update surplus values, check timeout
    gMqtt.update(gSettings);

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
