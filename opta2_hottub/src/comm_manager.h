#pragma once
#include <Arduino.h>
#include <ArduinoMqttClient.h>
#include <EthernetClient.h>
#include "types.h"
#include "config.h"

// Manages MQTT connection for Opta2.
// Subscribes to Opta1 permission + heartbeat topics.
// Also handles HA command subscriptions.
class CommManager {
public:
    CommManager(SystemStatus& status, AlarmState& alarms, IOState& io);

    void begin();
    void update(const Settings& settings);

    void publish(const char* topic, const char* payload, bool retain = false);
    void publish(const char* topic, int value,   bool retain = false);
    void publish(const char* topic, float value, uint8_t decimals = 1, bool retain = false);

    bool connected() const;

    // Called externally when a HA command topic arrives
    void handleCommand(const String& topic, const char* payload, int len,
                       Settings& settings);

private:
    EthernetClient _ethClient;
    MqttClient     _mqtt;

    SystemStatus& _status;
    AlarmState&   _alarms;
    IOState&      _io;

    bool          _lastHeartbeatBit    = false;
    bool          _heartbeatEverRx     = false;
    unsigned long _lastHeartbeatRxMs   = 0;

    void _reconnect(const Settings& settings);
    void _handleMessage(int messageSize, const Settings& settings);
    void _checkCommTimeout(const Settings& settings);
};
