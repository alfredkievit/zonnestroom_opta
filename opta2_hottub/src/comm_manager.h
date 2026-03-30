#pragma once
#include <Arduino.h>
#include <ArduinoMqttClient.h>
#include <WiFiClient.h>
#include "types.h"
#include "config.h"
#include "settings_storage.h"

// Manages MQTT connection for Opta2.
// Subscribes to Opta1 permission + heartbeat topics.
// Also handles HA command subscriptions.
class CommManager {
public:
    CommManager(Settings& settings, SettingsStorage& storage,
                SystemStatus& status, AlarmState& alarms, IOState& io);

    void begin();
    void update(const Settings& settings);

    void publish(const char* topic, const char* payload, bool retain = false);
    void publish(const char* topic, int value,   bool retain = false);
    void publish(const char* topic, float value, uint8_t decimals = 1, bool retain = false);

    bool connected();

    // Static callback required by ArduinoMqttClient
    static void _onMessageCb(int size);

    // Called externally when a HA command topic arrives
    void handleCommand(const char* topic, const char* payload, int len,
                       Settings& settings);

private:
    WiFiClient _wifiClient;
    MqttClient     _mqtt;

    Settings&     _settings;
    SettingsStorage& _storage;
    SystemStatus& _status;
    AlarmState&   _alarms;
    IOState&      _io;

    static CommManager* _instance;

    bool          _lastHeartbeatBit    = false;
    bool          _heartbeatEverRx     = false;
    unsigned long _lastHeartbeatRxMs   = 0;
    unsigned long _lastClockRxMs       = 0;
    unsigned long _lastWifiBeginMs     = 0;
    unsigned long _lastReconnectTryMs  = 0;
    unsigned long _lastConnectLogMs    = 0;
    bool          _wifiWasConnected    = false;
    bool          _mqttWasConnected    = false;
    bool          _settingsDirty       = false;
    unsigned long _settingsDirtySinceMs = 0;

    void _reconnect(const Settings& settings);
    void _ensureWifiConnected();
    void _handleMessage(int messageSize);
    void _checkCommTimeout(const Settings& settings);
    void _flushPendingSave();
    void _markSettingsDirty();
};
