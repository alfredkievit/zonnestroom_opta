#pragma once
#include <Arduino.h>

static constexpr size_t IRRIGATION_ZONE_COUNT = 6;

enum class NetworkTransport : uint8_t {
    NONE = 0,
    LAN  = 1,
    WIFI = 2
};

// ─── Settings ──────────────────────────────────────────────────────────────
struct Settings {
    // Hottub temperatures [°C]
    float spHottubTargetC;
    float spHottubHystC;
    float spHottubMaxC;

    // Circulatiepomp schema [uur] – läuft immer zur vollen Stunde
    int   spPumpRun1Hour;
    int   spPumpRun2Hour;
    int   spPumpRunDurationMin;

    // Timers [s]
    int   tLevelPumpMaxRunSec;
    int   tCommWatchdogSec;

    // Enable flags
    bool  enableHottub;
    bool  enableAutoPump;
    bool  enableLevelPump;

    // Irrigation
    bool  enableIrrigation;
    bool  enableIrrigationSchedules;
};

inline Settings defaultSettings() {
    Settings s{};
    s.spHottubTargetC      = 38.0f;
    s.spHottubHystC        = 0.5f;
    s.spHottubMaxC         = 42.0f;
    s.spPumpRun1Hour       = 8;
    s.spPumpRun2Hour       = 20;
    s.spPumpRunDurationMin = 5;
    s.tLevelPumpMaxRunSec  = 300;    // 5 min max level pump run
    s.tCommWatchdogSec     = 30;
    s.enableHottub         = true;
    s.enableAutoPump       = true;
    s.enableLevelPump      = true;
    s.enableIrrigation     = true;
    s.enableIrrigationSchedules = false;
    return s;
}

// ─── System status ─────────────────────────────────────────────────────────
struct SystemStatus {
    float hottubTempC;
    bool  hottubTempOk;
    bool  hottubLevelOk;
    bool  hottubHeaterActive;
    bool  hottubPumpActive;
    bool  levelPumpActive;
    bool  commOk;
    bool  clockOk;
    bool  hottubReady;

    bool  irrigationEnabled;
    bool  irrigationPumpActive;
    bool  irrigationAnyZoneActive;
    bool  irrigationExpansionPresent;
    bool  irrigationZoneActive[IRRIGATION_ZONE_COUNT];
    uint8_t irrigationActiveZoneCount;

    bool  lanConnected;
    bool  wifiConnected;
    NetworkTransport activeNetworkTransport;
};

// ─── Alarms ────────────────────────────────────────────────────────────────
struct AlarmState {
    bool hottubSensorFault;
    bool hottubOvertemp;
    bool hottubLevelHigh;
    bool hottubCommTimeout;
    bool levelPumpTimeout;
    bool hottubGeneral;
    bool irrigationExpansionMissing;
};

// ─── I/O ────────────────────────────────────────────────────────────────────
struct IOState {
    // Outputs
    bool doHottubHeater;
    bool doHottubPump;
    bool doHottubLevelPump;
    bool doHottubStatusRun;
    bool doHottubAlarm;
    bool doIrrigationPump;
    bool doIrrigationZones[IRRIGATION_ZONE_COUNT];
    bool manualForcePump;
    bool manualForceLevelPump;
    bool manualForceIrrigationPump;

    // Inputs – physical
    float aiHottubTempC;
    bool  diHottubLevelHigh;
    bool  diHottubFault;
    bool  diFlowOk;

    // Inputs – from Opta 1 via MQTT
    bool  inMasterPermissionHottub;
    bool  inMasterHeartbeat;
    bool  inMasterCommValid;
    int   clockMinuteOfDay;
    bool  clockMinuteValid;

    bool  irrigationZoneRequest[IRRIGATION_ZONE_COUNT];
    unsigned long irrigationZoneRequestSequence[IRRIGATION_ZONE_COUNT];
};
