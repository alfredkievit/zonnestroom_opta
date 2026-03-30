#include "mqtt_manager.h"
#include "ha_interface.h"
#include <WiFi.h>
#include <ArduinoJson.h>
#include <string.h>

namespace {
constexpr unsigned long WIFI_RETRY_INTERVAL_MS = 15000UL;
constexpr unsigned long MQTT_RETRY_INTERVAL_MS = 5000UL;
constexpr unsigned long CONNECT_LOG_INTERVAL_MS = 10000UL;
}

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
    _lastWifiBeginMs = millis();
    WiFi.begin(OPTA1_WIFI_SSID, OPTA1_WIFI_PASS);
    delay(500);
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
    _ensureWifiConnected();

    const bool wifiConnected = (WiFi.status() == WL_CONNECTED);
    if (_wifiWasConnected != wifiConnected) {
        _wifiWasConnected = wifiConnected;
        Serial.println(wifiConnected ? "[Opta1] WiFi connected" : "[Opta1] WiFi disconnected");
    }

    if (!wifiConnected) {
        if (_mqttWasConnected) {
            _mqttWasConnected = false;
            Serial.println("[Opta1] MQTT unavailable: WiFi down");
        }
        _checkTimeout(settings);
        return;
    }

    if (!_mqtt.connected()) {
        _reconnect();
    }
    if (_mqtt.connected()) {
        if (!_mqttWasConnected) {
            _mqttWasConnected = true;
            Serial.println("[Opta1] MQTT connected");
        }
        _mqtt.poll();
    } else if (_mqttWasConnected) {
        _mqttWasConnected = false;
        Serial.println("[Opta1] MQTT disconnected");
    }
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
    if (!_mqtt.endMessage()) {
#if DEBUG_DIAG
        Serial.print("[Opta1] MQTT publish failed: ");
        Serial.println(topic);
#endif
    }
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
void MqttManager::_ensureWifiConnected() {
    if (WiFi.status() == WL_CONNECTED) {
        return;
    }

    const unsigned long now = millis();
    if ((now - _lastWifiBeginMs) >= WIFI_RETRY_INTERVAL_MS) {
        _lastWifiBeginMs = now;
        WiFi.disconnect();
        WiFi.begin(OPTA1_WIFI_SSID, OPTA1_WIFI_PASS);
    }

    if ((now - _lastConnectLogMs) >= CONNECT_LOG_INTERVAL_MS) {
        _lastConnectLogMs = now;
        Serial.print("[Opta1] WiFi reconnect pending, status=");
        Serial.println((int)WiFi.status());
    }
}

// ---------------------------------------------------------------------------
void MqttManager::_reconnect() {
    const unsigned long now = millis();
    if ((now - _lastReconnectTryMs) < MQTT_RETRY_INTERVAL_MS) {
        return;
    }
    _lastReconnectTryMs = now;

    IPAddress broker(BROKER_IP[0], BROKER_IP[1], BROKER_IP[2], BROKER_IP[3]);
    if (!_mqtt.connect(broker, BROKER_PORT)) {
        if ((now - _lastConnectLogMs) >= CONNECT_LOG_INTERVAL_MS) {
            _lastConnectLogMs = now;
            Serial.println("[Opta1] MQTT connect failed");
        }
        // Connection failed – timeout will fire and force safe state
        return;
    }
    Serial.println("[Opta1] MQTT subscribe setup");

    // Subscribe to all meter publishes; keepalive is based on any meter topic
    _mqtt.subscribe(TOPIC_METER_ROOT);

    // Subscribe explicitly to the four channels used for surplus calculation.
    // This makes retained and live values for the calculation channels reliable.
    _mqtt.subscribe(TOPIC_METER_CH1);
    _mqtt.subscribe(TOPIC_METER_CH10);
    _mqtt.subscribe(TOPIC_METER_CH13);
    _mqtt.subscribe(TOPIC_METER_CH14);

    // Subscribe to HA command topics (retained settings arrive immediately)
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
    _mqtt.subscribe(TOPIC_CMD_MANUAL_FORCE_HOTTUB);
    _mqtt.subscribe(TOPIC_CMD_MANUAL_FORCE_COMFORT);
    _mqtt.subscribe(TOPIC_CMD_FAULT_RESET);

    // External sensor data from HA
    _mqtt.subscribe(TOPIC_EXTERN_COMPRESSOR_FREQ);
}

// ---------------------------------------------------------------------------
// Called by the ArduinoMqttClient onMessage callback
void MqttManager::_handleMessage(int messageSize) {
    String topicStr = _mqtt.messageTopic();
    char topic[96] = {};
    topicStr.toCharArray(topic, sizeof(topic));
    char   buf[256] = {};
    int    len     = min(messageSize, (int)sizeof(buf) - 1);
    for (int i = 0; i < len; i++) buf[i] = (char)_mqtt.read();
    // Drain any remaining bytes
    while (_mqtt.available()) _mqtt.read();

    bool isMeterTopic = (strncmp(topic, TOPIC_METER_PREFIX, strlen(TOPIC_METER_PREFIX)) == 0);
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
    if (strcmp(topic, TOPIC_METER_CH1) == 0)  { _ch1W  = _parseP(buf, len); _ch1Rx  = true; }
    else if (strcmp(topic, TOPIC_METER_CH10) == 0) { _ch10W = _parseP(buf, len); _ch10Rx = true; }
    else if (strcmp(topic, TOPIC_METER_CH13) == 0) { _ch13W = _parseP(buf, len); _ch13Rx = true; }
    else if (strcmp(topic, TOPIC_METER_CH14) == 0) { _ch14W = _parseP(buf, len); _ch14Rx = true; }
    else {
        return;  // keepalive-only meter topic, no power field needed
    }

    // Calculate phase-1 and total surplus independently.
    // WP logic should not wait for total channels, and hottub logic should not
    // block phase-1 surplus publication.
    if (_ch1Rx && _ch10Rx) {
        _status.surplusFase1W = _ch1W - _ch10W;
        _io.inSurplusFase1W   = _status.surplusFase1W;
    }

    if (_ch13Rx && _ch14Rx) {
        _status.surplusTotaalW = _ch13W - _ch14W;
        _io.inSurplusTotaalW   = _status.surplusTotaalW;
    }

    if ((_ch1Rx && _ch10Rx) || (_ch13Rx && _ch14Rx)) {
        _alarms.invalidPowerData = false;
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
