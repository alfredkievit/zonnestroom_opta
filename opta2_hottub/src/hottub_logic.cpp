#include "hottub_logic.h"

void HottubLogic::update(const Settings& settings, IOState& io,
                          SystemStatus& status, AlarmState& alarms) {
    const float temp = io.aiHottubTempC;

    // ── Overtemperature check (hard safety) ────────────────────────────────
    if (temp >= settings.spHottubMaxC) {
        alarms.hottubOvertemp  = true;
        io.doHottubHeater      = false;
        io.doHottubPump        = false;
        io.doHottubAlarm       = true;
        status.hottubHeaterActive = false;
        status.hottubPumpActive   = false;
        // Skip all further heater logic
        goto level_pump;
    }
    alarms.hottubOvertemp = false;

    // ── Heater / WP logic ──────────────────────────────────────────────────
    {
        bool localFault = io.diHottubFault || alarms.hottubSensorFault;
        bool levelOk    = !io.diHottubLevelHigh;
        bool permOk     = io.inMasterPermissionHottub && io.inMasterCommValid;

        if (!io.doHottubHeater) {
            // Start condition (with hysteresis on cold side)
            bool startOk =
                settings.enableHottub &&
                permOk &&
                !localFault &&
                levelOk &&
                temp < (settings.spHottubTargetC - settings.spHottubHystC);

            if (startOk) {
                io.doHottubHeater = true;
                io.doHottubPump   = true;
            }
        } else {
            // Stop conditions (direct, no delay)
            bool stopNow =
                !permOk ||
                localFault ||
                !levelOk ||
                temp >= settings.spHottubTargetC ||
                !settings.enableHottub;

            if (stopNow) {
                io.doHottubHeater = false;
                io.doHottubPump   = false;
            }
        }

        status.hottubHeaterActive = io.doHottubHeater;
        status.hottubPumpActive   = io.doHottubPump;
        status.hottubLevelOk      = levelOk;
        status.hottubTempOk       = !alarms.hottubSensorFault;
        status.hottubReady        = !localFault && levelOk && status.commOk;
        alarms.hottubGeneral      = localFault || alarms.hottubOvertemp || alarms.hottubLevelHigh;
        alarms.hottubLevelHigh    = io.diHottubLevelHigh;
        io.doHottubAlarm          = alarms.hottubGeneral || alarms.hottubCommTimeout;
    }

level_pump:
    // ── Level pump: autonomous – not an energy-surplus load ───────────────
    if (settings.enableLevelPump && io.diHottubLevelHigh) {
        if (!_levelPumpRunning) {
            _levelPumpRunning = true;
            _levelPumpStartMs = millis();
        }
        unsigned long runTime = millis() - _levelPumpStartMs;
        if (runTime > (unsigned long)settings.tLevelPumpMaxRunSec * 1000UL) {
            alarms.levelPumpTimeout = true;
            io.doHottubLevelPump    = false;   // stop pump on timeout alarm
        } else {
            io.doHottubLevelPump    = true;
        }
    } else {
        _levelPumpRunning       = false;
        io.doHottubLevelPump    = false;
        if (!io.diHottubLevelHigh) {
            alarms.levelPumpTimeout = false;   // auto-clear once level is gone
        }
    }
    status.levelPumpActive = io.doHottubLevelPump;
}
