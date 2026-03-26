#include "ha_interface.h"
#include "config.h"
#include <ArduinoJson.h>
#include <math.h>

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
    uint8_t prio = static_cast<uint8_t>(_status.priorityActive);
    if (prio != _prevPriority) {
        _mqtt.publish(TOPIC_HA_ACTIVE_PRIORITY, (int)prio);
        _prevPriority = prio;
    }

    // ── Heartbeat to Opta2 ─────────────────────────────────────────────────
    if ((now - _lastHeartbeatMs) >= HEARTBEAT_INTERVAL_MS) {
        _lastHeartbeatMs = now;
        _publishHeartbeat();
    }
}

// ---------------------------------------------------------------------------
void HaInterface::_publishAll() {
    _mqtt.publish(TOPIC_HA_SURPLUS_F1,      _status.surplusFase1W);
    _mqtt.publish(TOPIC_HA_SURPLUS_TOTAAL,  _status.surplusTotaalW);
    _mqtt.publish(TOPIC_HA_BOILER_TEMP_LOW, _status.boilerTempLowC);
    _mqtt.publish(TOPIC_HA_BOILER_TEMP_HIGH,_status.boilerTempHighC);
    _mqtt.publish(TOPIC_HA_ACTIVE_PRIORITY, (int)_status.priorityActive);
    _mqtt.publish(TOPIC_HA_MQTT_VALID,      _status.mqttValid    ? "1" : "0");
    _mqtt.publish(TOPIC_HA_WP_ACTIVE,       _status.wpActive     ? "1" : "0");
    _mqtt.publish(TOPIC_HA_ELEMENT_ACTIVE,  _status.elementActive? "1" : "0");
    _mqtt.publish(TOPIC_HA_HOTTUB_PERM,     _status.hottubPermitted ? "1" : "0");
    _mqtt.publish(TOPIC_HA_COMFORT_ACTIVE,  _io.doWpComfortExtra ? "1" : "0");
    _mqtt.publish(TOPIC_HA_MANUAL_MODE,     _io.inManualMode     ? "1" : "0");
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
    char buf[128];
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
void HaInterface::handleCommand(const String& topic, const char* payload, int len) {
    char buf[32] = {};
    int  copyLen = min(len, (int)sizeof(buf) - 1);
    memcpy(buf, payload, copyLen);
    float val  = atof(buf);
    bool  bval = (buf[0] == '1' || buf[0] == 't' || buf[0] == 'T');

    bool changed = false;

    if      (topic == TOPIC_CMD_ENABLE_SYSTEM)      { _settings.enableSystem        = bval; changed = true; }
    else if (topic == TOPIC_CMD_ENABLE_WP)           { _settings.enableWpBoiler      = bval; changed = true; }
    else if (topic == TOPIC_CMD_ENABLE_ELEMENT)      { _settings.enableBoilerElement = bval; changed = true; }
    else if (topic == TOPIC_CMD_ENABLE_HOTTUB)       { _settings.enableHottub        = bval; changed = true; }
    else if (topic == TOPIC_CMD_SP_WP_TARGET)        { _settings.spBoilerWpTargetC      = val;  changed = true; }
    else if (topic == TOPIC_CMD_SP_WP_HYST)          { _settings.spBoilerWpHystC        = val;  changed = true; }
    else if (topic == TOPIC_CMD_SP_ELEMENT_TARGET)   { _settings.spBoilerElementTargetC = val;  changed = true; }
    else if (topic == TOPIC_CMD_SP_ELEMENT_HYST)     { _settings.spBoilerElementHystC   = val;  changed = true; }
    else if (topic == TOPIC_CMD_SP_SURPLUS_WP)       { _settings.spSurplusWpStartW      = (int)val; changed = true; }
    else if (topic == TOPIC_CMD_SP_SURPLUS_ELEMENT)  { _settings.spSurplusElementStartW = (int)val; changed = true; }
    else if (topic == TOPIC_CMD_SP_SURPLUS_HOTTUB)   { _settings.spSurplusHottubStartW  = (int)val; changed = true; }
    else if (topic == TOPIC_CMD_SP_SURPLUS_STOP)     { _settings.spSurplusStopW         = (int)val; changed = true; }
    else if (topic == TOPIC_CMD_MANUAL_FORCE_WP)      { _io.manualForceWp      = bval; }
    else if (topic == TOPIC_CMD_MANUAL_FORCE_ELEM)    { _io.manualForceElement = bval; }
    else if (topic == TOPIC_CMD_MANUAL_FORCE_HOTTUB)  { _io.manualForceHottub  = bval; }
    else if (topic == TOPIC_CMD_MANUAL_FORCE_COMFORT) { _io.manualForceComfort = bval; }
    else if (topic == TOPIC_CMD_MANUAL_MODE)          { _io.inManualMode       = bval; }
    else if (topic == TOPIC_CMD_FAULT_RESET)          { _io.inFaultReset       = bval; }

    if (changed) {
        _storage.save(_settings);
    }
}
