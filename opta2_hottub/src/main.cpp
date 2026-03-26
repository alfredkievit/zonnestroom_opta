// ============================================================================
// Opta 2 – Hottub Controller
//
// Autonomous local control of hottub heater, circulation pump and level pump.
// Heater only runs with valid permission + heartbeat from Opta 1.
// All outputs go to safe state immediately on comm timeout or local fault.
// ============================================================================
#include <Arduino.h>
#include "config.h"
#include "types.h"
#include "analog_input.h"
#include "settings_storage.h"
#include "comm_manager.h"
#include "hottub_logic.h"
#include "ha_interface.h"

// ── Global state ──────────────────────────────────────────────────────────────
static Settings     gSettings;
static SystemStatus gStatus;
static AlarmState   gAlarms;
static IOState      gIo;

// ── Module instances ──────────────────────────────────────────────────────────
static SettingsStorage gStorage;
static CommManager     gComm(gSettings, gStorage, gStatus, gAlarms, gIo);
static HottubLogic     gHottubLogic;
static HaInterface     gHa(gComm, gSettings, gStatus, gAlarms, gIo, gStorage);

static void writeOutputWithLed(uint8_t outputPin, int outputState) {
    digitalWrite(outputPin, outputState);
    if (outputPin == PIN_DO_HOTTUB_HEATER) {
        digitalWrite(LED_D0, outputState);
    } else if (outputPin == PIN_DO_HOTTUB_PUMP) {
        digitalWrite(LED_D1, outputState);
    } else if (outputPin == PIN_DO_HOTTUB_LEVELPUMP) {
        digitalWrite(LED_D2, outputState);
    } else if (outputPin == PIN_DO_HOTTUB_ALARM) {
        digitalWrite(LED_D3, outputState);
    }
}

// ── Helper: write physical outputs ───────────────────────────────────────────
static void writeOutputs() {
    writeOutputWithLed(PIN_DO_HOTTUB_HEATER,    gIo.doHottubHeater    ? HIGH : LOW);
    writeOutputWithLed(PIN_DO_HOTTUB_PUMP,      gIo.doHottubPump      ? HIGH : LOW);
    writeOutputWithLed(PIN_DO_HOTTUB_LEVELPUMP, gIo.doHottubLevelPump ? HIGH : LOW);
    writeOutputWithLed(PIN_DO_HOTTUB_ALARM,     gIo.doHottubAlarm     ? HIGH : LOW);
}

// ── Helper: read physical inputs ─────────────────────────────────────────────
static void readInputs() {
    bool fault = false;
    gIo.aiHottubTempC       = readPT1000(PIN_AI_HOTTUB_TEMP, fault);
    gAlarms.hottubSensorFault = fault;
    gStatus.hottubTempC     = gIo.aiHottubTempC;

    gIo.diHottubLevelHigh   = (digitalRead(PIN_DI_LEVEL_HIGH)   == HIGH);
    gIo.diHottubFault       = (digitalRead(PIN_DI_HOTTUB_FAULT) == HIGH);
    gIo.diFlowOk            = (digitalRead(PIN_DI_FLOW_OK)      == HIGH);
}

// ─────────────────────────────────────────────────────────────────────────────
void setup() {
    Serial.begin(115200);

    // Configure outputs – safe state
    pinMode(PIN_DO_HOTTUB_HEATER,    OUTPUT);
    pinMode(PIN_DO_HOTTUB_PUMP,      OUTPUT);
    pinMode(PIN_DO_HOTTUB_LEVELPUMP, OUTPUT);
    pinMode(PIN_DO_HOTTUB_ALARM,     OUTPUT);

    pinMode(LED_D0, OUTPUT);
    pinMode(LED_D1, OUTPUT);
    pinMode(LED_D2, OUTPUT);
    pinMode(LED_D3, OUTPUT);

    writeOutputWithLed(PIN_DO_HOTTUB_HEATER,    LOW);
    writeOutputWithLed(PIN_DO_HOTTUB_PUMP,      LOW);
    writeOutputWithLed(PIN_DO_HOTTUB_LEVELPUMP, LOW);
    writeOutputWithLed(PIN_DO_HOTTUB_ALARM,     LOW);

    // Configure inputs
    pinMode(PIN_DI_LEVEL_HIGH,   INPUT);
    pinMode(PIN_DI_HOTTUB_FAULT, INPUT);
    pinMode(PIN_DI_FLOW_OK,      INPUT);

    analogReadResolution(16);

    gStorage.begin();
    if (!gStorage.load(gSettings)) {
        gSettings = defaultSettings();
    }

    gStatus = {};
    gAlarms = {};
    gIo     = {};

    gComm.begin();
}

// ─────────────────────────────────────────────────────────────────────────────
void loop() {
    // 1. Receive MQTT: permission, heartbeat, HA commands
    gComm.update(gSettings);

    // 2. Read physical sensors and digital inputs
    readInputs();

    // 3. Hottub temp control + level pump logic
    gHottubLogic.update(gSettings, gIo, gStatus, gAlarms);

    // 4. Write relay outputs
    writeOutputs();

    // 5. Publish status + alarms to HA
    gHa.update();
}
