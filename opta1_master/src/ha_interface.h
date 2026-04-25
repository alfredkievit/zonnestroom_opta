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

    void setMqttManager(MqttManager* mqtt);  // Optional setter for late binding
    void update();
    // Called by MqttManager when a HA command topic arrives
    void handleCommand(const char* topic, const char* payload, int len);

private:
    MqttManager&    _mqtt;
    MqttManager*    _mqttPtr = nullptr;  // Optional for late command forwarding
    Settings&       _settings;
    SystemStatus&   _status;
    AlarmState&     _alarms;
    IOState&        _io;
    SettingsStorage& _storage;

    unsigned long _lastPublishMs   = 0;
    unsigned long _lastHeartbeatMs = 0;
    unsigned long _lastBoilerLowPublishMs = 0;
    unsigned long _lastBoilerHighPublishMs = 0;
    unsigned long _lastWpBlockReasonPublishMs = 0;
    bool          _heartbeatBit    = false;
    float         _boilerHighAvgC  = 0.0f;
    bool          _boilerHighAvgInit = false;

    // Previous values for change detection
    int   _prevSurplusF1    = INT16_MIN;
    int   _prevSurplusTot   = INT16_MIN;
    float _prevBoilerLow    = -999.0f;
    bool  _prevMqttValid    = false;
    bool  _prevWpActive     = false;
    bool  _prevElemActive   = false;
    bool  _prevHottubPerm   = false;
    bool  _prevAutoWpReq    = false;
    bool  _prevAutoElemReq  = false;
    bool  _prevAutoHtReq    = false;
    bool  _prevElementThermOk = false;
    const char* _prevWpBlockReason = nullptr;
    uint8_t _prevPriority   = 255;
    bool _settingsDirty = false;
    unsigned long _settingsDirtySinceMs = 0;

    void _publishAll();
    void _publishAlarms();
    void _publishHeartbeat();
    const char* _getWpBlockReason() const;
    void _markSettingsDirty();
    void _flushPendingSave();
};
