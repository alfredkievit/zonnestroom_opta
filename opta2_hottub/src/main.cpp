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
#include "irrigation_logic.h"
#include "ha_interface.h"
#include "OptaBlue.h"

using namespace Opta;

// ── Global state ──────────────────────────────────────────────────────────────
static Settings     gSettings;
static SystemStatus gStatus;
static AlarmState   gAlarms;
static IOState      gIo;

// ── Module instances ──────────────────────────────────────────────────────────
static SettingsStorage gStorage;
static CommManager     gComm(gSettings, gStorage, gStatus, gAlarms, gIo);
static HottubLogic     gHottubLogic;
static IrrigationLogic gIrrigationLogic;
static HaInterface     gHa(gComm, gSettings, gStatus, gAlarms, gIo, gStorage);
static DigitalExpansion gIrrigationExpansion;
static unsigned long   gLastLoopHeartbeatMs       = 0;
static unsigned long   gLastExpansionOutputWriteMs = 0;
static uint8_t         gConsecutiveLoopStalls      = 0;

static void updateStatusLed(NetworkTransport transport) {
#if defined(LEDR) && defined(LEDG)
    static unsigned long lastToggleMs = 0;
    static bool blinkOn = false;
    const unsigned long now = millis();
    if ((now - lastToggleMs) >= 600UL) {
        lastToggleMs = now;
        blinkOn = !blinkOn;
    }

    // Drive the tri-color status LED above reset as a connection heartbeat:
    // - LAN/WiFi: blinking green
    // - No transport: blinking red
    // Use the separate blue LED channel near the user button as an extra
    // WiFi indicator when that hardware mapping is available.
    const int redOnState   = STATUS_LED_RED_ACTIVE_LOW ? LOW : HIGH;
    const int redOffState  = STATUS_LED_RED_ACTIVE_LOW ? HIGH : LOW;
    const int greenOnState = STATUS_LED_GREEN_ACTIVE_LOW ? LOW : HIGH;
    const int greenOffState = STATUS_LED_GREEN_ACTIVE_LOW ? HIGH : LOW;
#if defined(LEDB)
    const int blueOnState  = STATUS_LED_BLUE_ACTIVE_LOW ? LOW : HIGH;
    const int blueOffState = STATUS_LED_BLUE_ACTIVE_LOW ? HIGH : LOW;
#endif

    bool showGreen = false;
    bool showRed = false;
#if defined(LEDB)
    bool showBlue = false;
#endif

    if (transport == NetworkTransport::LAN || transport == NetworkTransport::WIFI) {
        showGreen = true;
    }

    if (transport == NetworkTransport::WIFI) {
#if defined(LEDB)
        showBlue = true;
#endif
    } else {
        if (transport == NetworkTransport::NONE) {
            showRed = true;
        }
    }

    digitalWrite(LEDG, (showGreen && blinkOn) ? greenOnState : greenOffState);
    digitalWrite(LEDR, (showRed && blinkOn) ? redOnState : redOffState);
#if defined(LEDB)
    digitalWrite(LEDB, showBlue ? blueOnState : blueOffState);
#endif
#endif
}

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

static void forceSafeOutputs() {
    gIo.doHottubHeater = false;
    gIo.doHottubPump = false;
    gIo.doHottubLevelPump = false;
    gIo.doHottubAlarm = false;
    gIo.doIrrigationPump = false;
    for (size_t idx = 0; idx < IRRIGATION_ZONE_COUNT; ++idx) {
        gIo.doIrrigationZones[idx] = false;
    }
}

// ── Helper: write physical outputs ───────────────────────────────────────────
static void writeOutputs() {
    writeOutputWithLed(PIN_DO_HOTTUB_HEATER,    gIo.doHottubHeater    ? HIGH : LOW);
    writeOutputWithLed(PIN_DO_HOTTUB_PUMP,      gIo.doHottubPump      ? HIGH : LOW);
    writeOutputWithLed(PIN_DO_HOTTUB_LEVELPUMP, gIo.doHottubLevelPump ? HIGH : LOW);
    writeOutputWithLed(PIN_DO_HOTTUB_ALARM,     gIo.doHottubAlarm     ? HIGH : LOW);

    if (!gIrrigationExpansion) {
        return;
    }

    const unsigned long now = millis();
    if ((now - gLastExpansionOutputWriteMs) >= 100UL) {
        gLastExpansionOutputWriteMs = now;
        gIrrigationExpansion.digitalWrite(PIN_EXP_IRRIGATION_ZONE_1, gIo.doIrrigationZones[0] ? HIGH : LOW);
        gIrrigationExpansion.digitalWrite(PIN_EXP_IRRIGATION_ZONE_2, gIo.doIrrigationZones[1] ? HIGH : LOW);
        gIrrigationExpansion.digitalWrite(PIN_EXP_IRRIGATION_ZONE_3, gIo.doIrrigationZones[2] ? HIGH : LOW);
        gIrrigationExpansion.digitalWrite(PIN_EXP_IRRIGATION_ZONE_4, gIo.doIrrigationZones[3] ? HIGH : LOW);
        gIrrigationExpansion.digitalWrite(PIN_EXP_IRRIGATION_ZONE_5, gIo.doIrrigationZones[4] ? HIGH : LOW);
        gIrrigationExpansion.digitalWrite(PIN_EXP_IRRIGATION_ZONE_6, gIo.doIrrigationZones[5] ? HIGH : LOW);
        gIrrigationExpansion.digitalWrite(PIN_EXP_IRRIGATION_PUMP,   gIo.doIrrigationPump ? HIGH : LOW);
        gIrrigationExpansion.digitalWrite(PIN_EXP_IRRIGATION_SPARE,  LOW);
        gIrrigationExpansion.updateDigitalOutputs();
    }
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
#if defined(LEDR) && defined(LEDG)
    pinMode(LEDR, OUTPUT);
    pinMode(LEDG, OUTPUT);
#if defined(LEDB)
    pinMode(LEDB, OUTPUT);
#endif
#endif

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

    OptaController.begin();
    // Run expansion scan in setup() to completion so loop() stays fast.
    // The Opta Blueprint RS485 scan can take several seconds on first call.
    {
        const unsigned long scanStart = millis();
        while ((millis() - scanStart) < 10000UL) {
            OptaController.update();
        }
        DigitalExpansion exp = OptaController.getExpansion(0);
        gIrrigationExpansion = exp;
        gStatus.irrigationExpansionPresent = static_cast<bool>(exp);
    }

    gComm.begin();
}

// ─────────────────────────────────────────────────────────────────────────────
void loop() {
    const unsigned long loopStartMs = millis();

    // Service the Opta Blueprint RS485 protocol every tick.
    // Must be called frequently to keep expansion module in sync.
    OptaController.update();

    // 1. Receive MQTT: permission, heartbeat, HA commands
    gComm.update(gSettings);

    // Status LED above reset: LAN=green, WiFi=blue, no transport=red
    updateStatusLed(gStatus.activeNetworkTransport);

    // 2. Read physical sensors and digital inputs
    readInputs();

    // 3. Hottub temp control + level pump logic
    gHottubLogic.update(gSettings, gIo, gStatus, gAlarms);

    // 4. Irrigation control on the expansion module
    gIrrigationLogic.update(gSettings, gIo, gStatus, gAlarms);

    const bool communicationConfirmed = gComm.controlLinkReady();
    if (!communicationConfirmed) {
        forceSafeOutputs();
    }

    // 5. Write relay outputs
    writeOutputs();

    // 6. Publish status + alarms to HA
    gHa.update();

    const unsigned long loopDurationMs = millis() - loopStartMs;
#if DEBUG_DIAG
    if (loopDurationMs > LOOP_WARN_MS) {
        Serial.print("[Opta2] slow loop ms=");
        Serial.println(loopDurationMs);
    }
    if ((millis() - gLastLoopHeartbeatMs) >= LOOP_HEARTBEAT_INTERVAL_MS) {
        gLastLoopHeartbeatMs = millis();
        Serial.print("[Opta2] loop heartbeat mqttConnected=");
        Serial.print(gComm.connected() ? "1" : "0");
        Serial.print(" commOk=");
        Serial.print(gStatus.commOk ? "1" : "0");
        Serial.print(" clockOk=");
        Serial.println(gStatus.clockOk ? "1" : "0");
    }
#endif

    if (loopDurationMs > LOOP_RESET_MS) {
        if (gConsecutiveLoopStalls < 255) {
            gConsecutiveLoopStalls++;
        }
#if DEBUG_DIAG
        Serial.print("[Opta2] loop stall count=");
        Serial.println(gConsecutiveLoopStalls);
#endif
        // Log repeated stalls, but do not auto-reset by default. Network
        // transitions can legitimately block for tens of seconds on Opta.
    #if ENABLE_LOOP_STALL_RESET
        if (gConsecutiveLoopStalls >= 3 && millis() > 60000UL) {
#if DEBUG_DIAG
            Serial.println("[Opta2] repeated loop stalls -> software reset");
            delay(20);
#endif
            NVIC_SystemReset();
        }
    #endif
    } else {
        gConsecutiveLoopStalls = 0;
    }
}
