#include "boiler_logic.h"

void BoilerLogic::evaluate(const Settings& settings, const IOState& io, SystemStatus& status) {
    const float temp = io.aiBoilerTempLowC;

    // ── Priority 1: WP boiler request ──────────────────────────────────────
    // Request is set when boiler needs heating via WP.
    // Start: temp < (WP_TARGET - HYST) and sufficient fase-1 surplus.
    // Keep:  temp < WP_TARGET.
    // Uses hysteresis to prevent rapid toggling.
    if (!status.boilerWpRequest) {
        // Not currently requesting → check start condition (with hysteresis)
        status.boilerWpRequest =
            settings.enableSystem &&
            settings.enableWpBoiler &&
            status.mqttValid &&
            io.inSurplusFase1W > settings.spSurplusWpStartW &&
            temp < (settings.spBoilerWpTargetC - settings.spBoilerWpHystC);
    } else {
        // Currently requesting → keep active until stop conditions
        bool stopCondition =
            !status.mqttValid ||
            io.inSurplusFase1W <= settings.spSurplusStopW ||
            temp >= settings.spBoilerWpTargetC ||
            !settings.enableSystem ||
            !settings.enableWpBoiler;
        if (stopCondition) status.boilerWpRequest = false;
    }

    // ── Priority 2: Boiler element request ─────────────────────────────────
    // Only relevant when WP zone is already satisfied (temp >= WP_TARGET).
    // Start: WP zone done, temp < (ELEMENT_TARGET - HYST), thermostat OK,
    //        sufficient fase-1 surplus, WP not active.
    if (!status.boilerElementRequest) {
        status.boilerElementRequest =
            settings.enableSystem &&
            settings.enableBoilerElement &&
            status.mqttValid &&
            io.inSurplusFase1W > settings.spSurplusElementStartW &&
            temp >= settings.spBoilerWpTargetC &&
            temp < (settings.spBoilerElementTargetC - settings.spBoilerElementHystC) &&
            io.inElementThermostatOk &&
            !status.wpActive;    // WP must be off before element can start
    } else {
        // Keep active until stop conditions
        bool stopCondition =
            !status.mqttValid ||
            io.inSurplusFase1W <= settings.spSurplusStopW ||
            temp >= settings.spBoilerElementTargetC ||
            !io.inElementThermostatOk ||
            status.wpActive ||
            !settings.enableSystem ||
            !settings.enableBoilerElement;
        if (stopCondition) status.boilerElementRequest = false;
    }

    // ── Priority 3: Hottub request ─────────────────────────────────────────
    // Hottub may only be requested when boiler is fully satisfied AND total surplus remains.
    // "Boiler fully satisfied" means element target reached OR element not needed.
    bool boilerDone = (temp >= settings.spBoilerElementTargetC) ||
                      (!status.boilerWpRequest && !status.boilerElementRequest);

    if (!status.hottubRequest) {
        status.hottubRequest =
            settings.enableSystem &&
            settings.enableHottub &&
            status.mqttValid &&
            io.inSurplusTotaalW > settings.spSurplusHottubStartW &&
            boilerDone;
    } else {
        bool stopCondition =
            !status.mqttValid ||
            io.inSurplusTotaalW <= settings.spSurplusStopW ||
            !boilerDone ||
            !settings.enableSystem ||
            !settings.enableHottub;
        if (stopCondition) status.hottubRequest = false;
    }
}
