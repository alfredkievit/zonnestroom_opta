#pragma once
#include <ArduinoMqttClient.h>
#include "types.h"
#include "mqtt_manager.h"
#include "settings_storage.h"

// Handles Home Assistant MQTT integration:
//   - Publishes status/alarms periodically and on change
//   - Parses received command topics and updates Settings + triggers save
//   - Publishes heartbeat to Opta2
class HaInterface {
public:
    HaInterface(MqttManager& mqtt, Settings& settings, SystemStatus& status,
                AlarmState& alarms, IOState& io, SettingsStorage& storage);

    void update();
    // Called by MqttManager when a HA command topic arrives
    void handleCommand(const String& topic, const char* payload, int len);

private:
    MqttManager&    _mqtt;
    Settings&       _settings;
    SystemStatus&   _status;
    AlarmState&     _alarms;
    IOState&        _io;
    SettingsStorage& _storage;

    unsigned long _lastPublishMs   = 0;
    unsigned long _lastHeartbeatMs = 0;
    bool          _heartbeatBit    = false;

    // Previous values for change detection
    int   _prevSurplusF1    = INT16_MIN;
    int   _prevSurplusTot   = INT16_MIN;
    float _prevBoilerLow    = -999.0f;
    bool  _prevMqttValid    = false;
    bool  _prevWpActive     = false;
    bool  _prevElemActive   = false;
    bool  _prevHottubPerm   = false;
    uint8_t _prevPriority   = 255;

    void _publishAll();
    void _publishAlarms();
    void _publishHeartbeat();
};
