#include "ha_interface.h"
#include "config.h"
#include <ArduinoJson.h>
#include <math.h>

HaInterface::HaInterface(CommManager& comm, Settings& settings, SystemStatus& status,
                         AlarmState& alarms, IOState& io, SettingsStorage& storage)
    : _comm(comm), _settings(settings), _status(status),
      _alarms(alarms), _io(io), _storage(storage)
{}

void HaInterface::update() {
    unsigned long now = millis();
    bool doPublish = (now - _lastPublishMs) >= HA_PUBLISH_INTERVAL_MS;
    if (doPublish) {
        _lastPublishMs = now;
        _publishAll();
        _publishAlarms();
    }

    // Change-triggered
    if (fabsf(_status.hottubTempC - _prevTemp) >= 0.5f) {
        _comm.publish(TOPIC_HA_HOTTUB_TEMP, _status.hottubTempC);
        _prevTemp = _status.hottubTempC;
    }
    if (_status.hottubHeaterActive != _prevHeater) {
        _comm.publish(TOPIC_HA_HEATER_ACTIVE, _status.hottubHeaterActive ? "1" : "0");
        _prevHeater = _status.hottubHeaterActive;
    }
    if (_status.hottubPumpActive != _prevPump) {
        _comm.publish(TOPIC_HA_PUMP_ACTIVE, _status.hottubPumpActive ? "1" : "0");
        _prevPump = _status.hottubPumpActive;
    }
    if (_io.diHottubLevelHigh != _prevLevelHigh) {
        _comm.publish(TOPIC_HA_LEVEL_HIGH, _io.diHottubLevelHigh ? "1" : "0");
        _prevLevelHigh = _io.diHottubLevelHigh;
    }
    if (_status.levelPumpActive != _prevLevelPumpAct) {
        _comm.publish(TOPIC_HA_LEVEL_PUMP_ACT, _status.levelPumpActive ? "1" : "0");
        _prevLevelPumpAct = _status.levelPumpActive;
    }
    if (_status.commOk != _prevCommOk) {
        _comm.publish(TOPIC_HA_COMM_OK, _status.commOk ? "1" : "0");
        _prevCommOk = _status.commOk;
    }
    if (_status.clockOk != _prevClockOk) {
        _comm.publish(TOPIC_HA_CLOCK_OK, _status.clockOk ? "1" : "0");
        _prevClockOk = _status.clockOk;
    }
}

void HaInterface::_publishAll() {
    _comm.publish(TOPIC_HA_HOTTUB_TEMP,   _status.hottubTempC);
    _comm.publish(TOPIC_HA_HEATER_ACTIVE, _status.hottubHeaterActive ? "1" : "0");
    _comm.publish(TOPIC_HA_PUMP_ACTIVE,   _status.hottubPumpActive   ? "1" : "0");
    _comm.publish(TOPIC_HA_LEVEL_HIGH,    _io.diHottubLevelHigh      ? "1" : "0");
    _comm.publish(TOPIC_HA_LEVEL_PUMP_ACT,_status.levelPumpActive    ? "1" : "0");
    _comm.publish(TOPIC_HA_COMM_OK,       _status.commOk             ? "1" : "0");
    _comm.publish(TOPIC_HA_CLOCK_OK,      _status.clockOk            ? "1" : "0");

    // Publish a toggling heartbeat so HA can detect Opta2 liveness.
    _heartbeatBit = !_heartbeatBit;
    _comm.publish(TOPIC_HA_HEARTBEAT, _heartbeatBit ? "1" : "0");
}

void HaInterface::_publishAlarms() {
    JsonDocument doc;
    doc["sensor_fault"]    = _alarms.hottubSensorFault;
    doc["overtemp"]        = _alarms.hottubOvertemp;
    doc["level_high"]      = _alarms.hottubLevelHigh;
    doc["comm_timeout"]    = _alarms.hottubCommTimeout;
    doc["level_pump_tout"] = _alarms.levelPumpTimeout;
    doc["general"]         = _alarms.hottubGeneral;
    char buf[256];
    serializeJson(doc, buf, sizeof(buf));
    _comm.publish(TOPIC_HA_ALARM_JSON, buf);
}
