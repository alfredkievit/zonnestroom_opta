#pragma once
#include <Arduino.h>
#include <ArduinoMqttClient.h>
#include <WiFiClient.h>
#include "types.h"
#include "config.h"

// Forward declare HaInterface to avoid circular deps
class HaInterface;

class MqttManager {
public:
    MqttManager(SystemStatus& status, AlarmState& alarms, IOState& io);

    void begin();
    void setHaInterface(HaInterface* ha);  // Called by setup() to wire up callbacks
    
    // Must be called every loop iteration
    void update(const Settings& settings);

    // Publish a single topic (used by ha_interface)
    void publish(const char* topic, const char* payload, bool retain = false);
    void publish(const char* topic, int value, bool retain = false);
    void publish(const char* topic, float value, uint8_t decimals = 1, bool retain = false);

    bool connected();

    // Static callback required by ArduinoMqttClient (no lambda support)
    static void _onMessageCb(int size);

private:
    HaInterface* _ha = nullptr;  // To forward command topics
    static MqttManager* _instance;
    WiFiClient  _wifiClient;
    MqttClient  _mqtt;

    SystemStatus& _status;
    AlarmState&   _alarms;
    IOState&      _io;

    // Raw values from the Solix status payload
    int  _batteryChargeW = 0;
    int  _batteryDischargeW = 0;

    unsigned long _lastWifiBeginMs      = 0;
    unsigned long _lastReconnectTryMs   = 0;
    unsigned long _lastConnectLogMs     = 0;
    bool          _wifiWasConnected     = false;
    bool          _mqttWasConnected     = false;
    unsigned long _wifiConnectedSinceMs = 0;
    unsigned long _mqttRetryBackoffMs   = 0;
    uint8_t       _mqttConsecutiveFails = 0;

    // MQTT timeout hysteresis: debounce transient disconnects
    bool          _mqttTimeoutTriggered = false;
    unsigned long _mqttTimeoutAlarmedAtMs = 0;
    unsigned long _mqttRecoveredAtMs = 0;

    void _reconnect();
    void _ensureWifiConnected();
    void _resetMqttBackoff();
    void _handleMessage(int messageSize);
    void _checkTimeout(const Settings& settings);
    bool _applySolixStatus(const char* payload, int payloadLen);

};
