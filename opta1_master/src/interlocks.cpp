#include "interlocks.h"

// ---------------------------------------------------------------------------
// All interlocks operate on the same IOState object (in-place, applied last).
// 'out' is the same struct as 'io' in practice – kept separate for clarity.
void Interlocks::apply(const Settings& settings, const IOState& io,
                       SystemStatus& status, AlarmState& alarms,
                       IOState& out) {
    // Copy current desired state
    out = io;

    // ── Interlock 1: WP and element must NEVER be active simultaneously ────
    if (out.doWpExtraWW && out.doBoilerElement) {
        out.doWpExtraWW     = false;
        out.doBoilerElement = false;
        alarms.interlockConflict = true;
    }

    // ── Interlock 2: No surplus → all energy loads off ─────────────────────
    if (out.inSurplusFase1W <= settings.spSurplusStopW) {
        out.doWpExtraWW     = false;
        out.doBoilerElement = false;
    }
    if (out.inSurplusTotaalW <= settings.spSurplusStopW) {
        out.doMasterPermHottub = false;
    }

    // ── Interlock 3: MQTT invalid → all energy loads off ──────────────────
    if (!status.mqttValid) {
        out.doWpExtraWW        = false;
        out.doBoilerElement    = false;
        out.doMasterPermHottub = false;
    }

    // ── Interlock 4: Boiler sensor fault → WP and element off ─────────────
    if (alarms.boilerSensorFault) {
        out.doWpExtraWW     = false;
        out.doBoilerElement = false;
    }

    // ── Interlock 5: Comm fault to Opta2 → revoke hottub permission ────────
    // Comm validity is tracked by ha_interface / mqtt_manager via heartbeat.
    // If the status flag is cleared, permission is withdrawn immediately.
    if (!status.hottubPermitted) {
        out.doMasterPermHottub = false;
    }

    // ── Update status mirror fields from final output ──────────────────────
    status.wpActive        = out.doWpExtraWW;
    status.elementActive   = out.doBoilerElement;
    status.hottubPermitted = out.doMasterPermHottub;
    status.priorityActive  = out.doWpExtraWW        ? SystemState::WP_BOILER      :
                             out.doBoilerElement     ? SystemState::BOILER_ELEMENT :
                             out.doMasterPermHottub  ? SystemState::HOTTUB         :
                             alarms.interlockConflict ? SystemState::FAULT         :
                                                        SystemState::IDLE;
}
