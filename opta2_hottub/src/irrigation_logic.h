#pragma once

#include "types.h"

class IrrigationLogic {
public:
    void update(const Settings& settings, IOState& io,
                SystemStatus& status, AlarmState& alarms);

private:
    void _dropOldestRequests(IOState& io);
};