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
};
