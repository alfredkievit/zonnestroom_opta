#include "hottub_logic.h"
#include "config.h"

void HottubLogic::update(const Settings& settings, IOState& io,
                          SystemStatus& status, AlarmState& alarms) {
    const float temp = io.aiHottubTempC;
    const unsigned long now = millis();
    if (io.clockMinuteValid) {
        if (_lastClockMinuteOfDay >= 0 && io.clockMinuteOfDay < _lastClockMinuteOfDay) {
            ++_clockDayCounter;
        }
        _lastClockMinuteOfDay = io.clockMinuteOfDay;
    }

    // ── Overtemperature check (hard safety) ────────────────────────────────
    alarms.hottubOvertemp = (temp >= settings.spHottubMaxC);
    if (alarms.hottubOvertemp) {
        io.doHottubHeater = false;
    }

    // ── Heater / WP logic ──────────────────────────────────────────────────
    {
        bool localFault = io.diHottubFault || alarms.hottubSensorFault;
        bool levelOk    = !io.diHottubLevelHigh;
        bool permOk     = io.inMasterPermissionHottub && io.inMasterCommValid;
        bool heatRequestAllowed = settings.enableHottub || io.inMasterPermissionHottub;

        if (!io.doHottubHeater) {
            // Start condition (with hysteresis on cold side)
            bool startOk =
                heatRequestAllowed &&
                permOk &&
                !localFault &&
                levelOk &&
                !alarms.hottubOvertemp &&
                temp < (settings.spHottubTargetC - settings.spHottubHystC);

            if (startOk) {
                io.doHottubHeater = true;
            }
        } else {
            // Stop conditions (direct, no delay)
            bool stopNow =
                !permOk ||
                localFault ||
                !levelOk ||
                alarms.hottubOvertemp ||
                temp >= settings.spHottubTargetC ||
                !heatRequestAllowed;

            if (stopNow) {
                io.doHottubHeater = false;
            }
        }

        int run1Minute = (settings.spPumpRun1Hour * 60) + settings.spPumpRun1Minute;
        int run2Minute = (settings.spPumpRun2Hour * 60) + settings.spPumpRun2Minute;

        if (io.clockMinuteValid) {
            if (io.clockMinuteOfDay == run1Minute && _lastRun1Day != _clockDayCounter) {
                _filterPumpRunActive = true;
                _filterPumpRunStartMs = now;
                _lastRun1Day = _clockDayCounter;
            }
            if (io.clockMinuteOfDay == run2Minute && _lastRun2Day != _clockDayCounter) {
                _filterPumpRunActive = true;
                _filterPumpRunStartMs = now;
                _lastRun2Day = _clockDayCounter;
            }
        }

        unsigned long filterRunDurationMs = (unsigned long)settings.spPumpRunDurationMin * 60UL * 1000UL;
        if (_filterPumpRunActive) {
            if ((now - _filterPumpRunStartMs) >= filterRunDurationMs) {
                _filterPumpRunActive = false;
            }
        }

        bool autoPumpRun = settings.enableAutoPump && _filterPumpRunActive;
        io.doHottubPump = io.doHottubHeater || io.manualForcePump || autoPumpRun;

        status.hottubHeaterActive = io.doHottubHeater;
        status.hottubPumpActive   = io.doHottubPump;
        status.hottubLevelOk      = levelOk;
        status.hottubTempOk       = !alarms.hottubSensorFault;
        status.clockOk            = io.clockMinuteValid;
        status.hottubReady        = !localFault && levelOk && status.commOk;
        alarms.hottubGeneral      = localFault || alarms.hottubOvertemp || alarms.hottubLevelHigh;
        alarms.hottubLevelHigh    = io.diHottubLevelHigh;
        io.doHottubAlarm          = alarms.hottubGeneral || alarms.hottubCommTimeout;
    }

    // ── Niveaupomp: handmatig of automatisch bij hoog niveau ──────────────
    if (io.manualForceLevelPump) {
        _levelPumpRunning      = false;
        alarms.levelPumpTimeout = false;
        io.doHottubLevelPump   = true;
    } else if (settings.enableLevelPump && io.diHottubLevelHigh) {
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
