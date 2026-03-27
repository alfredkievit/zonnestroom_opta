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
    io.doWpComfortExtra   = false;
    io.doBoilerElement    = false;
    io.doMasterPermHottub = false;
}

// ---------------------------------------------------------------------------
void PriorityManager::update(const Settings& settings, const SystemStatus& status,
                              const AlarmState& alarms, IOState& io) {
    // ── Fault state ────────────────────────────────────────────────────────
    if (_state == SystemState::FAULT) {
        _setAllOff(io);
        // Manual forces always pass through even in fault
        io.doWpExtraWW        = io.manualForceWp;
        io.doWpComfortExtra   = io.manualForceComfort;
        io.doMasterPermHottub = io.manualForceHottub;
        if (!_anyFault(alarms) && io.inFaultReset) {
            _state = SystemState::IDLE;
        }
        return;
    }

    // ── Enter FAULT if any fault is detected ───────────────────────────────
    if (_anyFault(alarms)) {
        _setAllOff(io);
        io.doWpExtraWW        = io.manualForceWp;
        io.doWpComfortExtra   = io.manualForceComfort;
        io.doMasterPermHottub = io.manualForceHottub;
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
            io.doWpExtraWW      = io.manualForceWp;      // always freely switchable
            io.doWpComfortExtra = io.manualForceComfort;  // always freely switchable
            if (wpReady) {
                _state = SystemState::WP_BOILER;
            } else if (elReady) {
                _state = SystemState::BOILER_ELEMENT;
            } else if (htReady) {
                _state = SystemState::HOTTUB;
            }
            break;

        case SystemState::WP_BOILER:
            io.doWpExtraWW        = true;                   // auto: WP running
            io.doWpComfortExtra   = io.manualForceComfort;
            io.doBoilerElement    = false;
            io.doMasterPermHottub = io.manualForceHottub;   // manual always wins

            if (!status.boilerWpRequest) {
                // Stop immediately – no stop delay
                io.doWpExtraWW = io.manualForceWp;          // hand-override while WP is off
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
            io.doWpExtraWW        = io.manualForceWp;       // always freely switchable
            io.doWpComfortExtra   = io.manualForceComfort;
            io.doBoilerElement    = true;
            io.doMasterPermHottub = io.manualForceHottub || htReady;

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
            io.doWpExtraWW        = io.manualForceWp;
            io.doWpComfortExtra   = io.manualForceComfort;
            io.doBoilerElement    = false;
            io.doMasterPermHottub = true;  // auto: surplus-based request

            if (!status.hottubRequest) {
                io.doMasterPermHottub = io.manualForceHottub;  // auto off → keep if manual
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
                    io.doMasterPermHottub = io.manualForceHottub;
                    _state = SystemState::WP_BOILER;
                } else if (elReady) {
                    io.doMasterPermHottub = io.manualForceHottub;
                    _state = SystemState::BOILER_ELEMENT;
                }
            }
            break;

        default:
            _setAllOff(io);
            io.doWpExtraWW        = io.manualForceWp;
            io.doWpComfortExtra   = io.manualForceComfort;
            io.doMasterPermHottub = io.manualForceHottub;
            _state = SystemState::IDLE;
            break;
    }

    // ── Update public status fields ────────────────────────────────────────
    // (const_cast acceptable: status is owned by main, passed const here for clarity)
}
