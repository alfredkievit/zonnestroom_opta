#include "ha_interface.h"
#include "config.h"
#include <ArduinoJson.h>
#include <math.h>

static const char* transportToString(NetworkTransport transport) {
    switch (transport) {
        case NetworkTransport::LAN:
            return "lan";
        case NetworkTransport::WIFI:
            return "wifi";
        default:
            return "offline";
    }
}

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
    if (_settings.enableIrrigation != _prevIrrigationEnabled) {
        _comm.publish(TOPIC_HA_IRRIGATION_ENABLED, _settings.enableIrrigation ? "1" : "0");
        _prevIrrigationEnabled = _settings.enableIrrigation;
    }
    if (_status.irrigationPumpActive != _prevIrrigationPump) {
        _comm.publish(TOPIC_HA_IRRIGATION_PUMP_ACTIVE, _status.irrigationPumpActive ? "1" : "0");
        _prevIrrigationPump = _status.irrigationPumpActive;
    }
    if (_io.manualForceIrrigationPump != _prevIrrigationPumpManualReq) {
        _comm.publish(TOPIC_HA_IRRIGATION_PUMP_MANUAL_REQ, _io.manualForceIrrigationPump ? "1" : "0");
        _prevIrrigationPumpManualReq = _io.manualForceIrrigationPump;
    }
    if (_status.irrigationExpansionPresent != _prevIrrigationExpansionPresent) {
        _comm.publish(TOPIC_HA_IRRIGATION_EXPANSION_PRESENT, _status.irrigationExpansionPresent ? "1" : "0");
        _prevIrrigationExpansionPresent = _status.irrigationExpansionPresent;
    }
    if (_status.irrigationActiveZoneCount != _prevIrrigationActiveZoneCount) {
        _comm.publish(TOPIC_HA_IRRIGATION_ACTIVE_ZONE_COUNT, _status.irrigationActiveZoneCount);
        _prevIrrigationActiveZoneCount = _status.irrigationActiveZoneCount;
    }
    if (_status.activeNetworkTransport != _prevTransport) {
        _comm.publish(TOPIC_HA_NETWORK_TRANSPORT, transportToString(_status.activeNetworkTransport));
        _prevTransport = _status.activeNetworkTransport;
    }
    for (size_t idx = 0; idx < IRRIGATION_ZONE_COUNT; ++idx) {
        if (_io.irrigationZoneRequest[idx] != _prevIrrigationZoneRequested[idx]) {
            _comm.publish(TOPIC_HA_IRRIGATION_ZONE_REQUESTED[idx], _io.irrigationZoneRequest[idx] ? "1" : "0");
            _prevIrrigationZoneRequested[idx] = _io.irrigationZoneRequest[idx];
        }
        if (_status.irrigationZoneActive[idx] != _prevIrrigationZoneActive[idx]) {
            _comm.publish(TOPIC_HA_IRRIGATION_ZONE_ACTIVE[idx], _status.irrigationZoneActive[idx] ? "1" : "0");
            _prevIrrigationZoneActive[idx] = _status.irrigationZoneActive[idx];
        }
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
    _comm.publish(TOPIC_HA_IRRIGATION_ENABLED, _settings.enableIrrigation ? "1" : "0");
    _comm.publish(TOPIC_HA_IRRIGATION_PUMP_ACTIVE, _status.irrigationPumpActive ? "1" : "0");
    _comm.publish(TOPIC_HA_IRRIGATION_PUMP_MANUAL_REQ, _io.manualForceIrrigationPump ? "1" : "0");
    _comm.publish(TOPIC_HA_IRRIGATION_EXPANSION_PRESENT, _status.irrigationExpansionPresent ? "1" : "0");
    _comm.publish(TOPIC_HA_IRRIGATION_ACTIVE_ZONE_COUNT, _status.irrigationActiveZoneCount);
    _comm.publish(TOPIC_HA_NETWORK_TRANSPORT, transportToString(_status.activeNetworkTransport));
    for (size_t idx = 0; idx < IRRIGATION_ZONE_COUNT; ++idx) {
        _comm.publish(TOPIC_HA_IRRIGATION_ZONE_REQUESTED[idx], _io.irrigationZoneRequest[idx] ? "1" : "0");
        _comm.publish(TOPIC_HA_IRRIGATION_ZONE_ACTIVE[idx], _status.irrigationZoneActive[idx] ? "1" : "0");
    }

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
    doc["irrigation_expansion_missing"] = _alarms.irrigationExpansionMissing;
    char buf[256];
    serializeJson(doc, buf, sizeof(buf));
    _comm.publish(TOPIC_HA_ALARM_JSON, buf);
}
