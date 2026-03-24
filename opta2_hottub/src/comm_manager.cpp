#include "comm_manager.h"
#include "settings_storage.h"
#include <Ethernet.h>

CommManager::CommManager(SystemStatus& status, AlarmState& alarms, IOState& io)
    : _mqtt(_ethClient), _status(status), _alarms(alarms), _io(io)
{}

// ---------------------------------------------------------------------------
void CommManager::begin() {
    byte mac[] = { OPTA2_MAC[0], OPTA2_MAC[1], OPTA2_MAC[2],
                   OPTA2_MAC[3], OPTA2_MAC[4], OPTA2_MAC[5] };
    IPAddress ip(OPTA2_IP[0], OPTA2_IP[1], OPTA2_IP[2], OPTA2_IP[3]);
    Ethernet.begin(mac, ip);
    delay(500);

    _mqtt.setId("opta2_hottub");
    _mqtt.setKeepAliveInterval(15 * 1000L);
    _mqtt.setConnectionTimeout(5 * 1000L);
}

// ---------------------------------------------------------------------------
void CommManager::update(const Settings& settings) {
    if (!_mqtt.connected()) {
        _reconnect(settings);
    }
    // Poll with lambda capturing this + settings ref
    _mqtt.onMessage([this, &settings](int size){
        _handleMessage(size, settings);
    });
    _mqtt.poll();
    _checkCommTimeout(settings);
}

// ---------------------------------------------------------------------------
bool CommManager::connected() const { return _mqtt.connected(); }

// ---------------------------------------------------------------------------
void CommManager::publish(const char* topic, const char* payload, bool retain) {
    if (!_mqtt.connected()) return;
    _mqtt.beginMessage(topic, retain);
    _mqtt.print(payload);
    _mqtt.endMessage();
}
void CommManager::publish(const char* topic, int value, bool retain) {
    char buf[16]; snprintf(buf, sizeof(buf), "%d", value);
    publish(topic, buf, retain);
}
void CommManager::publish(const char* topic, float value, uint8_t decimals, bool retain) {
    char buf[16]; dtostrf(value, 1, decimals, buf);
    publish(topic, buf, retain);
}

// ---------------------------------------------------------------------------
void CommManager::_reconnect(const Settings& settings) {
    IPAddress broker(BROKER_IP[0], BROKER_IP[1], BROKER_IP[2], BROKER_IP[3]);
    if (!_mqtt.connect(broker, BROKER_PORT)) return;

    // Opta1 device topics
    _mqtt.subscribe(TOPIC_MASTER_PERM_HOTTUB);
    _mqtt.subscribe(TOPIC_MASTER_HEARTBEAT);

    // HA command topics
    _mqtt.subscribe(TOPIC_CMD_SP_HOTTUB_TARGET);
    _mqtt.subscribe(TOPIC_CMD_SP_HOTTUB_HYST);
    _mqtt.subscribe(TOPIC_CMD_SP_HOTTUB_MAX);
    _mqtt.subscribe(TOPIC_CMD_ENABLE_HOTTUB);
    _mqtt.subscribe(TOPIC_CMD_ENABLE_LEVELPUMP);
}

// ---------------------------------------------------------------------------
void CommManager::_handleMessage(int messageSize, const Settings& settings) {
    String topic = _mqtt.messageTopic();
    char   buf[32] = {};
    int    len = min(messageSize, (int)sizeof(buf) - 1);
    for (int i = 0; i < len; i++) buf[i] = (char)_mqtt.read();
    while (_mqtt.available()) _mqtt.read();

    if (topic == TOPIC_MASTER_PERM_HOTTUB) {
        _io.inMasterPermissionHottub = (buf[0] == '1');
    }
    else if (topic == TOPIC_MASTER_HEARTBEAT) {
        bool bit = (buf[0] == '1');
        if (!_heartbeatEverRx || bit != _lastHeartbeatBit) {
            // Heartbeat toggled → comm is alive
            _lastHeartbeatBit  = bit;
            _lastHeartbeatRxMs = millis();
            _heartbeatEverRx   = true;
            _status.commOk     = true;
            _alarms.hottubCommTimeout = false;
        }
        _io.inMasterHeartbeat = bit;
    }
    // HA commands forwarded to handleCommand (called by main)
}

// ---------------------------------------------------------------------------
void CommManager::_checkCommTimeout(const Settings& settings) {
    if (!_heartbeatEverRx) return;
    unsigned long elapsed = millis() - _lastHeartbeatRxMs;
    if (elapsed > (unsigned long)settings.tCommWatchdogSec * 1000UL) {
        _status.commOk              = false;
        _alarms.hottubCommTimeout   = true;
        _io.inMasterCommValid       = false;
        _io.inMasterPermissionHottub = false;   // fail-safe
    } else {
        _io.inMasterCommValid = true;
    }
}

// ---------------------------------------------------------------------------
void CommManager::handleCommand(const String& topic, const char* payload, int len,
                                Settings& settings) {
    char buf[32] = {};
    memcpy(buf, payload, min(len, (int)sizeof(buf) - 1));
    float val  = atof(buf);
    bool  bval = (buf[0] == '1' || buf[0] == 't' || buf[0] == 'T');

    if      (topic == TOPIC_CMD_SP_HOTTUB_TARGET) { settings.spHottubTargetC     = val;  }
    else if (topic == TOPIC_CMD_SP_HOTTUB_HYST)   { settings.spHottubHystC       = val;  }
    else if (topic == TOPIC_CMD_SP_HOTTUB_MAX)    { settings.spHottubMaxC        = val;  }
    else if (topic == TOPIC_CMD_ENABLE_HOTTUB)    { settings.enableHottub        = bval; }
    else if (topic == TOPIC_CMD_ENABLE_LEVELPUMP) { settings.enableLevelPump     = bval; }
}
