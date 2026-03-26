#pragma once
#include "types.h"

// Local hottub temperature control + level pump logic.
class HottubLogic {
public:
    void update(const Settings& settings, IOState& io,
                SystemStatus& status, AlarmState& alarms);

private:
    unsigned long _levelPumpStartMs  = 0;
    bool          _levelPumpRunning  = false;
    unsigned long _filterPumpRunStartMs   = 0;
    bool          _filterPumpRunActive    = false;
    int           _lastClockMinuteOfDay   = -1;
    int           _clockDayCounter        = 0;
    int           _lastRun1Day            = -1;
    int           _lastRun2Day            = -1;
};
