#pragma once
#include <Arduino.h>

// ─── System state machine ──────────────────────────────────────────────────
enum class SystemState : uint8_t {
    IDLE            = 0,
    WP_BOILER       = 1,
    BOILER_ELEMENT  = 2,
    HOTTUB          = 3,
    FAULT           = 99
};

// ─── Settings (all setpoints and enable flags) ────────────────────────────
struct Settings {
    // System
    bool  enableSystem;
    bool  enableWpBoiler;
    bool  enableBoilerElement;
    bool  enableHottub;
    bool  enableLevelPump;   // unused on Opta1, kept for HA symmetry
    int   mqttTimeoutSec;
    int   surplusFilterTimeSec;

    // Surplus thresholds  [W]
    int   spSurplusWpStartW;
    int   spSurplusElementStartW;
    int   spSurplusHottubStartW;
    int   spSurplusStopW;

    // Boiler temperatures [°C]
    float spBoilerWpTargetC;
    float spBoilerWpHystC;
    float spBoilerElementTargetC;
    float spBoilerElementHystC;

    // Timers [s]
    int   tWpStartDelaySec;
    int   tElementStartDelaySec;
    int   tHottubStartDelaySec;
    int   tWpMinOnSec;
    int   tElementMinOnSec;
    int   tHottubMinOnSec;
    int   tCommWatchdogSec;
};

// Default values matching specification
inline Settings defaultSettings() {
    Settings s{};
    s.enableSystem           = true;
    s.enableWpBoiler         = true;
    s.enableBoilerElement    = true;
    s.enableHottub           = true;
    s.enableLevelPump        = true;
    s.mqttTimeoutSec         = 10;
    s.surplusFilterTimeSec   = 0;

    s.spSurplusWpStartW      = 1500;   // to be tuned in practice
    s.spSurplusElementStartW = 3200;   // 3 kW element + margin
    s.spSurplusHottubStartW  = 1400;   // ~1.2 kW hottub + margin
    s.spSurplusStopW         = 0;

    s.spBoilerWpTargetC      = 50.0f;
    s.spBoilerWpHystC        = 0.5f;
    s.spBoilerElementTargetC = 62.0f;
    s.spBoilerElementHystC   = 0.5f;

    s.tWpStartDelaySec       = 20;
    s.tElementStartDelaySec  = 20;
    s.tHottubStartDelaySec   = 30;
    s.tWpMinOnSec            = 0;
    s.tElementMinOnSec       = 0;
    s.tHottubMinOnSec        = 0;
    s.tCommWatchdogSec       = 30;
    return s;
}

// ─── System status ─────────────────────────────────────────────────────────
struct SystemStatus {
    bool        systemReady;
    bool        surplusAvailable;
    bool        mqttValid;
    bool        boilerWpRequest;
    bool        boilerElementRequest;
    bool        hottubRequest;
    bool        wpActive;
    bool        elementActive;
    bool        hottubPermitted;
    SystemState priorityActive;   // 0/1/2/3/99

    // Measured values
    int         surplusFase1W;    // CH1.P - CH10.P  [W]
    int         surplusTotaalW;   // CH13.P - CH14.P [W]
    float       boilerTempLowC;
    float       boilerTempHighC;

    // Timers (millis snapshots, managed inside modules)
    unsigned long mqttLastUpdateMs;
    bool          mqttRxOk;
};

// ─── Alarms ────────────────────────────────────────────────────────────────
struct AlarmState {
    bool mqttTimeout;
    bool invalidPowerData;
    bool boilerSensorFault;
    bool boilerThermostatFault;
    bool interlockConflict;
    bool masterGeneral;
};

// ─── Physical I/O (logical representation before writing) ─────────────────
struct IOState {
    // Outputs (desired)
    bool doWpExtraWW;            // DO0 – extra warmwater contact WP
    bool doWpComfortExtra;       // DO1 – optionele comfortverwarming (toekomstig)
    bool doBoilerElement;        // DO2 – relais boiler-element
    bool doMasterPermHottub;     // DO3 – permissie naar Opta 2 (via MQTT, niet fysiek)
    bool doMasterHeartbeat;      // heartbeat toggle bit

    // Inputs (read)
    float aiBoilerTempLowC;
    float aiBoilerTempHighC;
    bool  inMqttPowerValid;
    int   inSurplusFase1W;
    int   inSurplusTotaalW;
    bool  inElementThermostatOk;
    bool  inManualMode;
    bool  inFaultReset;

    // Manual force overrides
    bool  manualForceWp;
    bool  manualForceElement;
    bool  manualForceHottub;
};
