#include "priority_manager.h"

// ---------------------------------------------------------------------------
// Returns true when 'condition' has been continuously true for >= delayMs.
// Resets the timer when condition goes false.
bool PriorityManager::_delayElapsed(bool& condActive, unsigned long& sinceMs,
                                     bool condition, unsigned long delayMs) {
    if (!condition) {
        condActive = false;
        return false;
    }
    if (!condActive) {
        condActive = true;
        sinceMs    = millis();
    }
    return (millis() - sinceMs) >= delayMs;
}

bool PriorityManager::_anyFault(const AlarmState& alarms) const {
    return alarms.mqttTimeout ||
           alarms.invalidPowerData ||
           alarms.boilerSensorFault ||
           alarms.boilerThermostatFault ||
           alarms.interlockConflict ||
           alarms.masterGeneral;
}

void PriorityManager::_setAllOff(IOState& io) {
    io.doWpExtraWW        = false;
    io.doBoilerElement    = false;
    io.doMasterPermHottub = false;
}

// ---------------------------------------------------------------------------
void PriorityManager::update(const Settings& settings, const SystemStatus& status,
                              const AlarmState& alarms, IOState& io) {
    // ── Manual mode bypass ─────────────────────────────────────────────────
    if (io.inManualMode) {
        io.doWpExtraWW        = io.manualForceWp;
        io.doBoilerElement    = io.manualForceElement;
        io.doMasterPermHottub = io.manualForceHottub;
        _state = SystemState::IDLE;
        return;
    }

    // ── Fault state ────────────────────────────────────────────────────────
    if (_state == SystemState::FAULT) {
        _setAllOff(io);
        // Exit FAULT only when all faults cleared AND reset received
        if (!_anyFault(alarms) && io.inFaultReset) {
            _state = SystemState::IDLE;
        }
        return;
    }

    // ── Enter FAULT if any fault is detected ───────────────────────────────
    if (_anyFault(alarms)) {
        _setAllOff(io);
        _state = SystemState::FAULT;
        return;
    }

    // ── Evaluate start delays ──────────────────────────────────────────────
    unsigned long wpDelay  = (unsigned long)settings.tWpStartDelaySec      * 1000UL;
    unsigned long elDelay  = (unsigned long)settings.tElementStartDelaySec  * 1000UL;
    unsigned long htDelay  = (unsigned long)settings.tHottubStartDelaySec   * 1000UL;

    bool wpReady  = _delayElapsed(_wpConditionActive,      _wpConditionSinceMs,
                                   status.boilerWpRequest,      wpDelay);
    bool elReady  = _delayElapsed(_elementConditionActive, _elementConditionSinceMs,
                                   status.boilerElementRequest, elDelay);
    bool htReady  = _delayElapsed(_hottubConditionActive,  _hottubConditionSinceMs,
                                   status.hottubRequest,        htDelay);

    // ── State machine ──────────────────────────────────────────────────────
    switch (_state) {

        case SystemState::IDLE:
            _setAllOff(io);
            if (wpReady) {
                _state = SystemState::WP_BOILER;
            } else if (elReady) {
                _state = SystemState::BOILER_ELEMENT;
            } else if (htReady) {
                _state = SystemState::HOTTUB;
            }
            break;

        case SystemState::WP_BOILER:
            io.doWpExtraWW        = true;
            io.doBoilerElement    = false;
            io.doMasterPermHottub = false;

            if (!status.boilerWpRequest) {
                // Stop immediately – no stop delay
                io.doWpExtraWW = false;
                if (elReady) {
                    _state = SystemState::BOILER_ELEMENT;
                } else if (htReady) {
                    _state = SystemState::HOTTUB;
                } else {
                    _state = SystemState::IDLE;
                }
            }
            break;

        case SystemState::BOILER_ELEMENT:
            io.doWpExtraWW        = false;
            io.doBoilerElement    = true;
            io.doMasterPermHottub = false;

            if (!status.boilerElementRequest) {
                io.doBoilerElement = false;
                // If boiler drops back below WP zone, return to WP_BOILER
                if (wpReady) {
                    _state = SystemState::WP_BOILER;
                } else if (htReady) {
                    _state = SystemState::HOTTUB;
                } else {
                    _state = SystemState::IDLE;
                }
            }
            break;

        case SystemState::HOTTUB:
            io.doWpExtraWW        = false;
            io.doBoilerElement    = false;
            io.doMasterPermHottub = true;

            if (!status.hottubRequest) {
                io.doMasterPermHottub = false;
                if (wpReady) {
                    _state = SystemState::WP_BOILER;
                } else if (elReady) {
                    _state = SystemState::BOILER_ELEMENT;
                } else {
                    _state = SystemState::IDLE;
                }
            } else {
                // Higher-priority load reclaims → leave HOTTUB immediately
                if (wpReady) {
                    io.doMasterPermHottub = false;
                    _state = SystemState::WP_BOILER;
                } else if (elReady) {
                    io.doMasterPermHottub = false;
                    _state = SystemState::BOILER_ELEMENT;
                }
            }
            break;

        default:
            _setAllOff(io);
            _state = SystemState::IDLE;
            break;
    }

    // ── Update public status fields ────────────────────────────────────────
    // (const_cast acceptable: status is owned by main, passed const here for clarity)
}
