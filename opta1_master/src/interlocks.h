#pragma once
#include "types.h"

// Applies all 5 hard interlock rules.
// MUST be called as the LAST step before writing physical outputs.
class Interlocks {
public:
    void apply(const Settings& settings, const IOState& io,
               SystemStatus& status, AlarmState& alarms,
               IOState& out);
};
