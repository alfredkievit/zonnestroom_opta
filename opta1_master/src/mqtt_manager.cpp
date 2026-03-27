#include "mqtt_manager.h"
#include "ha_interface.h"
#include <WiFi.h>
#include <ArduinoJson.h>

// Static instance pointer required for ArduinoMqttClient plain-function callback
MqttManager* MqttManager::_instance = nullptr;

void MqttManager::_onMessageCb(int size) {
    if (_instance) _instance->_handleMessage(size);
}

// ---------------------------------------------------------------------------
MqttManager::MqttManager(SystemStatus& status, AlarmState& alarms, IOState& io)
    : _mqtt(_wifiClient), _status(status), _alarms(alarms), _io(io)
{}

// ---------------------------------------------------------------------------
void MqttManager::begin() {
    WiFi.begin(OPTA1_WIFI_SSID, OPTA1_WIFI_PASS);
    delay(500);

    IPAddress broker(BROKER_IP[0], BROKER_IP[1], BROKER_IP[2], BROKER_IP[3]);
    _mqtt.setId("opta1_master");
    _mqtt.setKeepAliveInterval(15 * 1000L);
    _mqtt.setConnectionTimeout(5 * 1000L);

    // Register static callback (ArduinoMqttClient does not support lambdas)
    _instance = this;
    _mqtt.onMessage(_onMessageCb);

    _reconnect();
}

// ---------------------------------------------------------------------------
void MqttManager::update(const Settings& settings) {
    if (!_mqtt.connected()) {
        _reconnect();
    }
    _mqtt.poll();
    _checkTimeout(settings);
}

// ---------------------------------------------------------------------------
bool MqttManager::connected() {
    return _mqtt.connected();
}

// ---------------------------------------------------------------------------
void MqttManager::publish(const char* topic, const char* payload, bool retain) {
    if (!_mqtt.connected()) return;
    _mqtt.beginMessage(topic, retain);
    _mqtt.print(payload);
    _mqtt.endMessage();
}

void MqttManager::publish(const char* topic, int value, bool retain) {
    char buf[16];
    snprintf(buf, sizeof(buf), "%d", value);
    publish(topic, buf, retain);
}

void MqttManager::publish(const char* topic, float value, uint8_t decimals, bool retain) {
    char buf[16];
    snprintf(buf, sizeof(buf), "%.*f", (int)decimals, (double)value);
    publish(topic, buf, retain);
}

// ---------------------------------------------------------------------------
void MqttManager::setHaInterface(HaInterface* ha) {
    _ha = ha;
}

// ---------------------------------------------------------------------------
void MqttManager::_reconnect() {
    IPAddress broker(BROKER_IP[0], BROKER_IP[1], BROKER_IP[2], BROKER_IP[3]);
    if (!_mqtt.connect(broker, BROKER_PORT)) {
        // Connection failed – timeout will fire and force safe state
        return;
    }
    // Subscribe to all meter publishes; keepalive is based on any meter topic
    _mqtt.subscribe(TOPIC_METER_ROOT);

    // Subscribe to HA command topics (retained settings arrive immediately)
    _mqtt.subscribe(TOPIC_CMD_ENABLE_SYSTEM);
    _mqtt.subscribe(TOPIC_CMD_ENABLE_WP);
    _mqtt.subscribe(TOPIC_CMD_ENABLE_ELEMENT);
    _mqtt.subscribe(TOPIC_CMD_ENABLE_HOTTUB);
    _mqtt.subscribe(TOPIC_CMD_SP_WP_TARGET);
    _mqtt.subscribe(TOPIC_CMD_SP_WP_HYST);
    _mqtt.subscribe(TOPIC_CMD_SP_ELEMENT_TARGET);
    _mqtt.subscribe(TOPIC_CMD_SP_ELEMENT_HYST);
    _mqtt.subscribe(TOPIC_CMD_SP_SURPLUS_WP);
    _mqtt.subscribe(TOPIC_CMD_SP_SURPLUS_ELEMENT);
    _mqtt.subscribe(TOPIC_CMD_SP_SURPLUS_HOTTUB);
    _mqtt.subscribe(TOPIC_CMD_SP_SURPLUS_STOP);
    _mqtt.subscribe(TOPIC_CMD_MANUAL_FORCE_WP);
    _mqtt.subscribe(TOPIC_CMD_MANUAL_FORCE_ELEM);
    _mqtt.subscribe(TOPIC_CMD_MANUAL_FORCE_HOTTUB);
    _mqtt.subscribe(TOPIC_CMD_MANUAL_MODE);
    _mqtt.subscribe(TOPIC_CMD_MANUAL_FORCE_COMFORT);
    _mqtt.subscribe(TOPIC_CMD_FAULT_RESET);
}

// ---------------------------------------------------------------------------
// Called by the ArduinoMqttClient onMessage callback
void MqttManager::_handleMessage(int messageSize) {
    String topic   = _mqtt.messageTopic();
    char   buf[64] = {};
    int    len     = min(messageSize, (int)sizeof(buf) - 1);
    for (int i = 0; i < len; i++) buf[i] = (char)_mqtt.read();
    // Drain any remaining bytes
    while (_mqtt.available()) _mqtt.read();

    bool isMeterTopic = topic.startsWith(TOPIC_METER_PREFIX);
    if (!isMeterTopic) {
        // ── HA command topics ────────────────────────────────────────────
        // Forward to HaInterface for command processing
        if (_ha) {
            _ha->handleCommand(topic, buf, len);
        }
        return;
    }

    // Any publish from the configured meter means the meter is alive.
    _status.mqttLastUpdateMs = millis();
    _status.mqttRxOk         = true;
    _status.mqttValid        = true;
    _alarms.mqttTimeout      = false;
    _io.inMqttPowerValid     = true;

    // ── Energy meter channels used for surplus calculation ──────────────
    if (topic == TOPIC_METER_CH1)  { _ch1W  = _parseP(buf, len); _ch1Rx  = true; }
    else if (topic == TOPIC_METER_CH10) { _ch10W = _parseP(buf, len); _ch10Rx = true; }
    else if (topic == TOPIC_METER_CH13) { _ch13W = _parseP(buf, len); _ch13Rx = true; }
    else if (topic == TOPIC_METER_CH14) { _ch14W = _parseP(buf, len); _ch14Rx = true; }
    else {
        return;  // keepalive-only meter topic, no power field needed
    }

    // If all four channels received at least once → data is valid
    if (_ch1Rx && _ch10Rx && _ch13Rx && _ch14Rx) {
        _status.surplusFase1W   = _ch1W  - _ch10W;
        _status.surplusTotaalW  = _ch13W - _ch14W;
        _alarms.invalidPowerData = false;
        _io.inSurplusFase1W      = _status.surplusFase1W;
        _io.inSurplusTotaalW     = _status.surplusTotaalW;
    }
}

// ---------------------------------------------------------------------------
void MqttManager::_checkTimeout(const Settings& settings) {
    if (!_status.mqttRxOk) return;  // never received anything yet → already invalid
    unsigned long elapsed = millis() - _status.mqttLastUpdateMs;
    if (elapsed > (unsigned long)settings.mqttTimeoutSec * 1000UL) {
        _status.mqttValid        = false;
        _alarms.mqttTimeout      = true;
        _io.inMqttPowerValid     = false;
    }
}

// ---------------------------------------------------------------------------
// Parse "P" field from JSON payload: {"P":"123", ...}
int MqttManager::_parseP(const char* payload, int payloadLen) {
    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, payload, payloadLen);
    if (err) {
        _alarms.invalidPowerData = true;
        return 0;
    }
    // "P" is transmitted as string by this meter firmware
    const char* pStr = doc["P"];
    if (pStr == nullptr) {
        // Try numeric
        int pVal = doc["P"] | 0;
        return pVal;
    }
    return atoi(pStr);
}
