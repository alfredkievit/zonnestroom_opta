#include "ha_interface.h"
#include "config.h"
#include <ArduinoJson.h>
#include <math.h>
#include <string.h>

HaInterface::HaInterface(MqttManager& mqtt, Settings& settings, SystemStatus& status,
                         AlarmState& alarms, IOState& io, SettingsStorage& storage)
    : _mqtt(mqtt), _settings(settings), _status(status),
      _alarms(alarms), _io(io), _storage(storage)
{}

// ---------------------------------------------------------------------------
void HaInterface::setMqttManager(MqttManager* mqtt) {
    _mqttPtr = mqtt;
}

// ---------------------------------------------------------------------------
void HaInterface::update() {
    unsigned long now = millis();

    if (!_boilerHighAvgInit) {
        _boilerHighAvgC = _status.boilerTempHighC;
        _boilerHighAvgInit = true;
    } else {
        // Slow EWMA for 1000L boiler visualization; control logic remains unfiltered.
        _boilerHighAvgC += 0.02f * (_status.boilerTempHighC - _boilerHighAvgC);
    }

    const char* wpBlockReason = _getWpBlockReason();

    if (_prevWpBlockReason == nullptr) {
        _prevWpBlockReason = wpBlockReason;
    }

    // ── Periodic publish ───────────────────────────────────────────────────
    bool doPublish = (now - _lastPublishMs) >= HA_PUBLISH_INTERVAL_MS;
    if (doPublish) {
        _lastPublishMs = now;
        _publishAll();
        _publishAlarms();
    }

    // ── Change-triggered publish ───────────────────────────────────────────
    if (_status.surplusFase1W != _prevSurplusF1) {
        _mqtt.publish(TOPIC_HA_SURPLUS_F1, _status.surplusFase1W);
        _prevSurplusF1 = _status.surplusFase1W;
    }
    if (_status.surplusTotaalW != _prevSurplusTot) {
        _mqtt.publish(TOPIC_HA_SURPLUS_TOTAAL, _status.surplusTotaalW);
        _prevSurplusTot = _status.surplusTotaalW;
    }
    if (fabsf(_status.boilerTempLowC - _prevBoilerLow) >= 0.5f) {
        _mqtt.publish(TOPIC_HA_BOILER_TEMP_LOW, _status.boilerTempLowC);
        _prevBoilerLow = _status.boilerTempLowC;
    }
    if (_status.mqttValid != _prevMqttValid) {
        _mqtt.publish(TOPIC_HA_MQTT_VALID, _status.mqttValid ? "1" : "0");
        _prevMqttValid = _status.mqttValid;
    }
    if (_io.inElementThermostatOk != _prevElementThermOk) {
        _mqtt.publish(TOPIC_HA_ELEMENT_THERM_OK, _io.inElementThermostatOk ? "1" : "0");
        _prevElementThermOk = _io.inElementThermostatOk;
    }
    if (_status.wpActive != _prevWpActive) {
        _mqtt.publish(TOPIC_HA_WP_ACTIVE, _status.wpActive ? "1" : "0");
        _prevWpActive = _status.wpActive;
    }
    if (_status.elementActive != _prevElemActive) {
        _mqtt.publish(TOPIC_HA_ELEMENT_ACTIVE, _status.elementActive ? "1" : "0");
        _prevElemActive = _status.elementActive;
    }
    if (_status.hottubPermitted != _prevHottubPerm) {
        _mqtt.publish(TOPIC_HA_HOTTUB_PERM, _status.hottubPermitted ? "1" : "0");
        _prevHottubPerm = _status.hottubPermitted;
    }
    if (strcmp(wpBlockReason, _prevWpBlockReason) != 0) {
        _mqtt.publish(TOPIC_HA_WP_BLOCK_REASON, wpBlockReason);
        _prevWpBlockReason = wpBlockReason;
    }
    if (_status.boilerWpRequest != _prevAutoWpReq) {
        _mqtt.publish(TOPIC_HA_AUTO_WP_REQ, _status.boilerWpRequest ? "1" : "0");
        _prevAutoWpReq = _status.boilerWpRequest;
    }
    if (_status.boilerElementRequest != _prevAutoElemReq) {
        _mqtt.publish(TOPIC_HA_AUTO_ELEMENT_REQ, _status.boilerElementRequest ? "1" : "0");
        _prevAutoElemReq = _status.boilerElementRequest;
    }
    if (_status.hottubRequest != _prevAutoHtReq) {
        _mqtt.publish(TOPIC_HA_AUTO_HOTTUB_REQ, _status.hottubRequest ? "1" : "0");
        _prevAutoHtReq = _status.hottubRequest;
    }
    uint8_t prio = static_cast<uint8_t>(_status.priorityActive);
    if (prio != _prevPriority) {
        _mqtt.publish(TOPIC_HA_ACTIVE_PRIORITY, (int)prio);
        _prevPriority = prio;
    }

    if ((now - _lastBoilerHighPublishMs) >= HA_BOILER_HIGH_PUBLISH_INTERVAL_MS) {
        _lastBoilerHighPublishMs = now;
        _mqtt.publish(TOPIC_HA_BOILER_TEMP_HIGH, _boilerHighAvgC);
    }

    // ── Heartbeat to Opta2 ─────────────────────────────────────────────────
    if ((now - _lastHeartbeatMs) >= HEARTBEAT_INTERVAL_MS) {
        _lastHeartbeatMs = now;
        _publishHeartbeat();
    }

    _flushPendingSave();
}

// ---------------------------------------------------------------------------
void HaInterface::_publishAll() {
    _mqtt.publish(TOPIC_HA_SURPLUS_F1,      _status.surplusFase1W);
    _mqtt.publish(TOPIC_HA_SURPLUS_TOTAAL,  _status.surplusTotaalW);
    _mqtt.publish(TOPIC_HA_BOILER_TEMP_LOW, _status.boilerTempLowC);
    _mqtt.publish(TOPIC_HA_ACTIVE_PRIORITY, (int)_status.priorityActive);
    _mqtt.publish(TOPIC_HA_MQTT_VALID,      _status.mqttValid    ? "1" : "0");
    _mqtt.publish(TOPIC_HA_WP_ACTIVE,       _status.wpActive     ? "1" : "0");
    _mqtt.publish(TOPIC_HA_ELEMENT_ACTIVE,  _status.elementActive? "1" : "0");
    _mqtt.publish(TOPIC_HA_HOTTUB_PERM,     _status.hottubPermitted ? "1" : "0");
    _mqtt.publish(TOPIC_HA_WP_BLOCK_REASON, _prevWpBlockReason ? _prevWpBlockReason : "unknown");
    _mqtt.publish(TOPIC_HA_ELEMENT_THERM_OK, _io.inElementThermostatOk ? "1" : "0");
    _mqtt.publish(TOPIC_HA_AUTO_WP_REQ,     _status.boilerWpRequest ? "1" : "0");
    _mqtt.publish(TOPIC_HA_AUTO_ELEMENT_REQ, _status.boilerElementRequest ? "1" : "0");
    _mqtt.publish(TOPIC_HA_AUTO_HOTTUB_REQ, _status.hottubRequest ? "1" : "0");
    _mqtt.publish(TOPIC_HA_COMFORT_ACTIVE,  _io.doWpComfortExtra ? "1" : "0");
    _mqtt.publish(TOPIC_HA_MANUAL_MODE,     _io.inManualMode     ? "1" : "0");
}

// ---------------------------------------------------------------------------
const char* HaInterface::_getWpBlockReason() const {
    if (_status.wpActive) {
        return "wp_active";
    }
    if (_alarms.mqttTimeout || _alarms.invalidPowerData || _alarms.boilerSensorFault ||
        _alarms.boilerThermostatFault || _alarms.interlockConflict || _alarms.masterGeneral) {
        return "alarm_or_fault";
    }
    if (!_settings.enableSystem) {
        return "system_disabled";
    }
    if (!_status.mqttValid) {
        return "mqtt_invalid";
    }
    if (_io.inSurplusFase1W <= _settings.spSurplusWpStartW) {
        return "surplus_too_low";
    }
    if (_status.boilerTempHighC >= (_settings.spBoilerWpTargetC - _settings.spBoilerWpHystC)) {
        return "boiler_temp_high_enough";
    }
    if (_status.boilerWpRequest) {
        return "start_delay_or_priority_wait";
    }
    return "ready_to_request";
}

// ---------------------------------------------------------------------------
void HaInterface::_publishAlarms() {
    JsonDocument doc;
    doc["mqtt_timeout"]         = _alarms.mqttTimeout;
    doc["invalid_power"]        = _alarms.invalidPowerData;
    doc["boiler_sensor"]        = _alarms.boilerSensorFault;
    doc["boiler_thermostat"]    = _alarms.boilerThermostatFault;
    doc["interlock_conflict"]   = _alarms.interlockConflict;
    doc["master_general"]       = _alarms.masterGeneral;
    char buf[256];
    serializeJson(doc, buf, sizeof(buf));
    _mqtt.publish(TOPIC_HA_ALARM_JSON, buf);
}

// ---------------------------------------------------------------------------
void HaInterface::_publishHeartbeat() {
    _heartbeatBit = !_heartbeatBit;
    _mqtt.publish(TOPIC_MASTER_HEARTBEAT, _heartbeatBit ? "1" : "0");
    _mqtt.publish(TOPIC_MASTER_PERM_HOTTUB,
                  _status.hottubPermitted ? "1" : "0");
}

// ---------------------------------------------------------------------------
// Called from MqttManager when a subscribed HA command topic arrives
void HaInterface::handleCommand(const char* topic, const char* payload, int len) {
    char buf[32] = {};
    int  copyLen = min(len, (int)sizeof(buf) - 1);
    memcpy(buf, payload, copyLen);
    float val  = atof(buf);
    bool  bval = (buf[0] == '1' || buf[0] == 't' || buf[0] == 'T');

    bool changed = false;

    if      (strcmp(topic, TOPIC_CMD_ENABLE_ELEMENT) == 0)     { if (_settings.enableBoilerElement != bval) { _settings.enableBoilerElement = bval; changed = true; } }
    else if (strcmp(topic, TOPIC_CMD_ENABLE_HOTTUB) == 0)      { if (_settings.enableHottub != bval) { _settings.enableHottub = bval; changed = true; } }
    else if (strcmp(topic, TOPIC_CMD_SP_WP_TARGET) == 0)       { if (fabsf(_settings.spBoilerWpTargetC - val) > 0.001f) { _settings.spBoilerWpTargetC = val; changed = true; } }
    else if (strcmp(topic, TOPIC_CMD_SP_WP_HYST) == 0)         { if (fabsf(_settings.spBoilerWpHystC - val) > 0.001f) { _settings.spBoilerWpHystC = val; changed = true; } }
    else if (strcmp(topic, TOPIC_CMD_SP_ELEMENT_TARGET) == 0)  { if (fabsf(_settings.spBoilerElementTargetC - val) > 0.001f) { _settings.spBoilerElementTargetC = val; changed = true; } }
    else if (strcmp(topic, TOPIC_CMD_SP_ELEMENT_HYST) == 0)    { if (fabsf(_settings.spBoilerElementHystC - val) > 0.001f) { _settings.spBoilerElementHystC = val; changed = true; } }
    else if (strcmp(topic, TOPIC_CMD_SP_SURPLUS_WP) == 0)      { int v = (int)val; if (_settings.spSurplusWpStartW != v) { _settings.spSurplusWpStartW = v; changed = true; } }
    else if (strcmp(topic, TOPIC_CMD_SP_SURPLUS_ELEMENT) == 0) { int v = (int)val; if (_settings.spSurplusElementStartW != v) { _settings.spSurplusElementStartW = v; changed = true; } }
    else if (strcmp(topic, TOPIC_CMD_SP_SURPLUS_HOTTUB) == 0)  { int v = (int)val; if (_settings.spSurplusHottubStartW != v) { _settings.spSurplusHottubStartW = v; changed = true; } }
    else if (strcmp(topic, TOPIC_CMD_SP_SURPLUS_STOP) == 0)    { int v = (int)val; if (_settings.spSurplusStopW != v) { _settings.spSurplusStopW = v; changed = true; } }
    else if (strcmp(topic, TOPIC_CMD_MANUAL_FORCE_WP) == 0)      { _io.manualForceWp      = bval; }
    else if (strcmp(topic, TOPIC_CMD_MANUAL_FORCE_HOTTUB) == 0)  { _io.manualForceHottub  = bval; }
    else if (strcmp(topic, TOPIC_CMD_MANUAL_FORCE_COMFORT) == 0) { _io.manualForceComfort = bval; }
    else if (strcmp(topic, TOPIC_CMD_FAULT_RESET) == 0)          { _io.inFaultReset       = bval; }
    else if (strcmp(topic, TOPIC_EXTERN_COMPRESSOR_FREQ) == 0) {
        _io.inCompressorFreqHz = (val >= 0.0f) ? val : 0.0f;
#if DEBUG_DIAG
        Serial.print("[Opta1] compressor_freq_hz=");
        Serial.println(_io.inCompressorFreqHz, 2);
#endif
    }

    if (changed) {
        _markSettingsDirty();
    }
}

// ---------------------------------------------------------------------------
void HaInterface::_markSettingsDirty() {
    _settingsDirty = true;
    _settingsDirtySinceMs = millis();
}

// ---------------------------------------------------------------------------
void HaInterface::_flushPendingSave() {
    if (!_settingsDirty) {
        return;
    }
    if ((millis() - _settingsDirtySinceMs) < SETTINGS_SAVE_DEBOUNCE_MS) {
        return;
    }
    _storage.save(_settings);
    _settingsDirty = false;
#if DEBUG_DIAG
    Serial.println("[Opta1] settings persisted");
#endif
}
