#pragma once
#include <Arduino.h>
#include <ArduinoMqttClient.h>
#include <EthernetClient.h>
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
    EthernetClient  _ethClient;
    MqttClient      _mqtt;

    SystemStatus& _status;
    AlarmState&   _alarms;
    IOState&      _io;

    // Raw channel values from energy meter
    int  _ch1W  = 0;   // fase 1 export
    int  _ch10W = 0;   // fase 1 import
    int  _ch13W = 0;   // totaal export
    int  _ch14W = 0;   // totaal import

    // Track which channels have been received at least once
    bool _ch1Rx  = false;
    bool _ch10Rx = false;
    bool _ch13Rx = false;
    bool _ch14Rx = false;

    void _reconnect();
    void _handleMessage(int messageSize);
    void _checkTimeout(const Settings& settings);
    int  _parseP(const char* payload, int payloadLen);

};
