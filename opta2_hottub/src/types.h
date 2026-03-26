#pragma once
#include <Arduino.h>

// ─── Settings ──────────────────────────────────────────────────────────────
struct Settings {
    // Hottub temperatures [°C]
    float spHottubTargetC;
    float spHottubHystC;
    float spHottubMaxC;

    // Circulatiepomp schema [uur/min]
    int   spPumpRun1Hour;
    int   spPumpRun1Minute;
    int   spPumpRun2Hour;
    int   spPumpRun2Minute;
    int   spPumpRunDurationMin;

    // Timers [s]
    int   tLevelPumpMaxRunSec;
    int   tCommWatchdogSec;

    // Enable flags
    bool  enableHottub;
    bool  enableAutoPump;
    bool  enableLevelPump;
};

inline Settings defaultSettings() {
    Settings s{};
    s.spHottubTargetC      = 38.0f;
    s.spHottubHystC        = 0.5f;
    s.spHottubMaxC         = 42.0f;
    s.spPumpRun1Hour       = 8;
    s.spPumpRun1Minute     = 0;
    s.spPumpRun2Hour       = 20;
    s.spPumpRun2Minute     = 0;
    s.spPumpRunDurationMin = 5;
    s.tLevelPumpMaxRunSec  = 300;    // 5 min max level pump run
    s.tCommWatchdogSec     = 30;
    s.enableHottub         = true;
    s.enableAutoPump       = true;
    s.enableLevelPump      = true;
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
};

// ─── Alarms ────────────────────────────────────────────────────────────────
struct AlarmState {
    bool hottubSensorFault;
    bool hottubOvertemp;
    bool hottubLevelHigh;
    bool hottubCommTimeout;
    bool levelPumpTimeout;
    bool hottubGeneral;
};

// ─── I/O ────────────────────────────────────────────────────────────────────
struct IOState {
    // Outputs
    bool doHottubHeater;
    bool doHottubPump;
    bool doHottubLevelPump;
    bool doHottubStatusRun;
    bool doHottubAlarm;
    bool manualForcePump;
    bool manualForceLevelPump;

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
};
