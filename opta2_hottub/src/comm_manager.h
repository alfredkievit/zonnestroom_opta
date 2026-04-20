#pragma once
#include <Arduino.h>
#include <ArduinoMqttClient.h>
#include <Ethernet.h>
#include <WiFi.h>
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
    bool controlLinkReady() const;

    // Static callback required by ArduinoMqttClient
    static void _onMessageCb(int size);

    // Called externally when a HA command topic arrives
    void handleCommand(const char* topic, const char* payload, int len,
                       Settings& settings);

private:
    EthernetClient _ethernetClient;
    WiFiClient _wifiClient;
    MqttClient     _mqttLan;
    MqttClient     _mqttWifi;
    MqttClient*    _activeMqtt = nullptr;

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
    unsigned long _lastLanBeginMs      = 0;
    unsigned long _lastWifiBeginMs     = 0;
    unsigned long _lastReconnectTryMs  = 0;
    unsigned long _lastConnectLogMs    = 0;
    unsigned long _lastLanLinkUpMs     = 0;
    unsigned long _lanRecoveryDeadlineMs = 0;
    unsigned long _startupLanProbeUntilMs = 0;
    unsigned long _commGraceUntilMs    = 0;
    unsigned long _publishHoldUntilMs  = 0;
    unsigned long _zoneRequestSequenceCounter = 0;
    NetworkTransport _activeTransport  = NetworkTransport::NONE;
    bool          _lanConfigured       = false;
    bool          _wifiWasConnected    = false;
    bool          _lanWasConnected     = false;
    bool          _mqttWasConnected    = false;
    bool          _settingsDirty       = false;
    unsigned long _settingsDirtySinceMs = 0;
    uint8_t       _lanMqttFailureCount = 0;
    unsigned long _forceWifiUntilMs    = 0;

    void _reconnect(const Settings& settings);
    void _ensureLanConnected(bool lanLinkPresent);
    void _ensureWifiConnected(bool lanAvailable);
    void _handleMessage(int messageSize);
    void _checkCommTimeout(const Settings& settings);
    void _flushPendingSave();
    void _markSettingsDirty();
    void _subscribeTopics(MqttClient& client);
    void _switchTransport(NetworkTransport transport);
    void _configureMqttClient(MqttClient& client);
    void _handleLanRecovery(bool lanLinkPresent, bool wifiConnected);
    void _dropLanSession();
    bool _isLanAvailable();
    bool _isWifiAvailable() const;
    MqttClient* _mqttForTransport(NetworkTransport transport);
};
