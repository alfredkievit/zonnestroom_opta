#pragma once
#include <Arduino.h>
#include "types.h"

// State machine that translates boiler/hottub requests into desired output states.
// Start-delay timers prevent chattering. Interlocks are applied AFTER this class.
class PriorityManager {
public:
    void update(const Settings& settings, const SystemStatus& status,
                const AlarmState& alarms, IOState& io);

    SystemState currentState() const { return _state; }

private:
    SystemState   _state = SystemState::IDLE;

    // Start-delay TON timers (millis snapshot of when condition became true)
    unsigned long _wpConditionSinceMs      = 0;
    unsigned long _elementConditionSinceMs = 0;
    unsigned long _hottubConditionSinceMs  = 0;
    bool          _wpConditionActive       = false;
    bool          _elementConditionActive  = false;
    bool          _hottubConditionActive   = false;

    bool _delayElapsed(bool& condActive, unsigned long& sinceMs,
                       bool condition, unsigned long delayMs);
    bool _anyFault(const AlarmState& alarms) const;
    void _setAllOff(IOState& io);
};
