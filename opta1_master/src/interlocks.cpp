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
    // Manual force flags bypass the surplus interlock for direct switching.
    if (out.inSurplusFase1W <= settings.spSurplusStopW) {
        if (!out.manualForceWp)      out.doWpExtraWW     = false;
        out.doBoilerElement = false;
    }
    if (out.inSurplusTotaalW <= settings.spSurplusStopW) {
        if (!out.manualForceHottub)  out.doMasterPermHottub = false;
    }

    // ── Interlock 3: MQTT invalid → all energy loads off ──────────────────
    // Manual force flags still permitted without MQTT (local override).
    if (!status.mqttValid) {
        if (!out.manualForceWp)      out.doWpExtraWW        = false;
        out.doBoilerElement = false;
        if (!out.manualForceHottub)  out.doMasterPermHottub = false;
    }

    // ── Interlock 4: Boiler sensor fault → WP and element off ─────────────
    if (alarms.boilerSensorFault) {
        out.doWpExtraWW     = false;
        out.doBoilerElement = false;
    }

    // ── Interlock 5: WP compressor draait → element geblokkeerd ─────────────
    // Blokkeer element zolang de SMO40 compressor draait (freq > 0 Hz),
    // ook als het WP-relais al uit is (compressor loopt nog uit).
    // Bij geen MQTT-update blijft waarde 0 (permissief).
    if (io.inCompressorFreqHz > 0.0f) {
        out.doBoilerElement = false;
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
