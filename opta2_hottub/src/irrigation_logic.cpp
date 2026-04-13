#include "irrigation_logic.h"

void IrrigationLogic::_dropOldestRequests(IOState& io) {
    while (true) {
        size_t requestedCount = 0;
        int oldestIndex = -1;
        unsigned long oldestSequence = 0;

        for (size_t idx = 0; idx < IRRIGATION_ZONE_COUNT; ++idx) {
            if (!io.irrigationZoneRequest[idx]) {
                continue;
            }
            ++requestedCount;
            if (oldestIndex < 0 || io.irrigationZoneRequestSequence[idx] < oldestSequence) {
                oldestIndex = static_cast<int>(idx);
                oldestSequence = io.irrigationZoneRequestSequence[idx];
            }
        }

        if (requestedCount <= 2 || oldestIndex < 0) {
            return;
        }

        io.irrigationZoneRequest[oldestIndex] = false;
        io.irrigationZoneRequestSequence[oldestIndex] = 0;
    }
}

void IrrigationLogic::update(const Settings& settings, IOState& io,
                             SystemStatus& status, AlarmState& alarms) {
    for (size_t idx = 0; idx < IRRIGATION_ZONE_COUNT; ++idx) {
        io.doIrrigationZones[idx] = false;
        status.irrigationZoneActive[idx] = false;
    }

    status.irrigationEnabled = settings.enableIrrigation;
    status.irrigationAnyZoneActive = false;
    status.irrigationPumpActive = false;
    status.irrigationActiveZoneCount = 0;

    alarms.irrigationExpansionMissing = settings.enableIrrigation && !status.irrigationExpansionPresent;

    _dropOldestRequests(io);

    if (!settings.enableIrrigation || !status.irrigationExpansionPresent) {
        io.doIrrigationPump = status.irrigationExpansionPresent && io.manualForceIrrigationPump;
        status.irrigationPumpActive = io.doIrrigationPump;
        return;
    }

    for (size_t idx = 0; idx < IRRIGATION_ZONE_COUNT; ++idx) {
        if (!io.irrigationZoneRequest[idx]) {
            continue;
        }
        io.doIrrigationZones[idx] = true;
        status.irrigationZoneActive[idx] = true;
        status.irrigationAnyZoneActive = true;
        ++status.irrigationActiveZoneCount;
    }

    io.doIrrigationPump = io.manualForceIrrigationPump || status.irrigationAnyZoneActive;
    status.irrigationPumpActive = io.doIrrigationPump;
}