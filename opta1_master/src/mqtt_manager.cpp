#include "mqtt_manager.h"
#include "ha_interface.h"
#include <WiFi.h>
#include <ArduinoJson.h>
#include <string.h>

namespace {
constexpr unsigned long WIFI_RETRY_INTERVAL_MS = 15000UL;
constexpr unsigned long MQTT_RETRY_INTERVAL_MS = 5000UL;
constexpr unsigned long MQTT_RETRY_BACKOFF_MAX_MS = 60000UL;
constexpr unsigned long WIFI_STABLE_BEFORE_MQTT_MS = 3000UL;
constexpr unsigned long CONNECT_LOG_INTERVAL_MS = 10000UL;

// Timeout hysteresis tuning: prevents transient WiFi hiccups from triggering alarms
// TIMEOUT_MULTIPLIER: alarm only fires if timeout condition persists for 1.5x the configured timeout
// RECOVERY_DIVISOR: alarm clears once data returns *and* was briefly stale (data within 0.5x timeout)
constexpr float TIMEOUT_MULTIPLIER = 1.5f;
constexpr float RECOVERY_DIVISOR = 2.0f;
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
    _mqtt.setKeepAliveInterval(30 * 1000L);
    _mqtt.setConnectionTimeout(10 * 1000L);

    // Register static callback (ArduinoMqttClient does not support lambdas)
    _instance = this;
    _mqtt.onMessage(_onMessageCb);

    _reconnect();
}

// ---------------------------------------------------------------------------
void MqttManager::update(const Settings& settings) {
    _ensureWifiConnected();

    const unsigned long now = millis();
    const bool wifiConnected = (WiFi.status() == WL_CONNECTED);
    if (_wifiWasConnected != wifiConnected) {
        _wifiWasConnected = wifiConnected;
        if (wifiConnected) {
            _wifiConnectedSinceMs = now;
            Serial.println("[Opta1] WiFi connected");
        } else {
            _wifiConnectedSinceMs = 0;
            _mqtt.stop();
            Serial.println("[Opta1] WiFi disconnected");
        }
    }

    if (!wifiConnected) {
        if (_mqttWasConnected) {
            _mqttWasConnected = false;
            Serial.println("[Opta1] MQTT unavailable: WiFi down");
        }
        _checkTimeout(settings);
        return;
    }

    if (_wifiConnectedSinceMs == 0) {
        _wifiConnectedSinceMs = now;
    }

    if ((now - _wifiConnectedSinceMs) < WIFI_STABLE_BEFORE_MQTT_MS) {
        _checkTimeout(settings);
        return;
    }

    if (!_mqtt.connected()) {
        _reconnect();
    }
    if (_mqtt.connected()) {
        if (!_mqttWasConnected) {
            _mqttWasConnected = true;
            _resetMqttBackoff();
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
    const unsigned long retryIntervalMs = (_mqttRetryBackoffMs > 0) ? _mqttRetryBackoffMs : MQTT_RETRY_INTERVAL_MS;
    if ((now - _lastReconnectTryMs) < retryIntervalMs) {
        return;
    }
    _lastReconnectTryMs = now;

    IPAddress broker(BROKER_IP[0], BROKER_IP[1], BROKER_IP[2], BROKER_IP[3]);
    if (!_mqtt.connect(broker, BROKER_PORT)) {
        if (_mqttConsecutiveFails < 255) {
            _mqttConsecutiveFails++;
        }
        if (_mqttRetryBackoffMs == 0) {
            _mqttRetryBackoffMs = MQTT_RETRY_INTERVAL_MS;
        } else {
            _mqttRetryBackoffMs = min(_mqttRetryBackoffMs * 2UL, MQTT_RETRY_BACKOFF_MAX_MS);
        }
        if ((now - _lastConnectLogMs) >= CONNECT_LOG_INTERVAL_MS) {
            _lastConnectLogMs = now;
            Serial.print("[Opta1] MQTT connect failed, retry in ms=");
            Serial.println(_mqttRetryBackoffMs);
        }
        // Connection failed – timeout will fire and force safe state
        return;
    }
    _resetMqttBackoff();
    Serial.println("[Opta1] MQTT subscribe setup");

    // Solix status is the sole surplus source; keepalive is based on it.
    _mqtt.subscribe(TOPIC_SOLIX_STATUS);

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

    // Re-assert our own (flash-authoritative) settings onto the retained cmd
    // topics, so the broker's replay-on-subscribe can never silently leave a
    // stale value in place (see HaInterface::publishSettingsSnapshot).
    if (_ha) _ha->publishSettingsSnapshot();
}

void MqttManager::_resetMqttBackoff() {
    _mqttConsecutiveFails = 0;
    _mqttRetryBackoffMs = MQTT_RETRY_INTERVAL_MS;
}

// ---------------------------------------------------------------------------
// Called by the ArduinoMqttClient onMessage callback
void MqttManager::_handleMessage(int messageSize) {
    String topicStr = _mqtt.messageTopic();
    char topic[96] = {};
    topicStr.toCharArray(topic, sizeof(topic));
    char   buf[768] = {};
    int    len     = min(messageSize, (int)sizeof(buf) - 1);
    for (int i = 0; i < len; i++) buf[i] = (char)_mqtt.read();
    // Drain any remaining bytes
    while (_mqtt.available()) _mqtt.read();

    bool isSolixTopic = (strcmp(topic, TOPIC_SOLIX_STATUS) == 0);
    if (!isSolixTopic) {
        // ── HA command topics ────────────────────────────────────────────
        // Forward to HaInterface for command processing
        if (_ha) {
            _ha->handleCommand(topic, buf, len);
        }
        return;
    }

    // A Solix status publish means the meter is alive.
    _status.mqttLastUpdateMs = millis();
    _status.mqttRxOk         = true;
    _status.mqttValid        = true;
    _alarms.mqttTimeout      = false;
    _io.inMqttPowerValid     = true;

    if (_applySolixStatus(buf, len)) {
        _alarms.invalidPowerData = false;
    } else {
        _status.mqttValid = false;
        _io.inMqttPowerValid = false;
        _alarms.invalidPowerData = true;
    }
}

bool MqttManager::_applySolixStatus(const char* payload, int payloadLen) {
    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, payload, payloadLen);
    if (err) {
        return false;
    }

    if (!(doc["valid"] | false)) {
        return false;
    }

    JsonVariant values = doc["values"];
    JsonVariant derived = doc["derived"];
    if (values.isNull() || derived.isNull()) {
        return false;
    }

    bool hasFase1 = values["fase_1_w"].is<int>() || values["fase_1_w"].is<float>();
    bool hasTotaal = values["totaal_w"].is<int>() || values["totaal_w"].is<float>();
    bool hasExportL1 = derived["export_l1_w"].is<int>() || derived["export_l1_w"].is<float>();
    bool hasImportL1 = derived["import_l1_w"].is<int>() || derived["import_l1_w"].is<float>();
    bool hasExportTotal = derived["export_total_w"].is<int>() || derived["export_total_w"].is<float>();
    bool hasImportTotal = derived["import_total_w"].is<int>() || derived["import_total_w"].is<float>();
    if (!hasFase1 || !hasTotaal || !hasExportL1 || !hasImportL1 || !hasExportTotal || !hasImportTotal) {
        return false;
    }

    const int exportL1W = lround((double)(derived["export_l1_w"] | 0.0f));
    const int importL1W = lround((double)(derived["import_l1_w"] | 0.0f));
    const int exportTotalW = lround((double)(derived["export_total_w"] | 0.0f));
    const int importTotalW = lround((double)(derived["import_total_w"] | 0.0f));
    _batteryChargeW = lround((double)(values["battery_charge_w"] | 0.0f));
    _batteryDischargeW = lround((double)(values["battery_discharge_w"] | 0.0f));

    const int rawSurplusFase1W = exportL1W - importL1W;
    const int correctedSurplusFase1W = rawSurplusFase1W - max(_batteryDischargeW - _batteryChargeW, 0);
    const int surplusTotaalW = exportTotalW - importTotalW;

    _status.surplusFase1W = correctedSurplusFase1W;
    _status.surplusTotaalW = surplusTotaalW;
    _io.inSurplusFase1W = correctedSurplusFase1W;
    _io.inSurplusTotaalW = surplusTotaalW;
    return true;
}

// ---------------------------------------------------------------------------
// Timeout detection with hysteresis to prevent transient WiFi hiccups from
// triggering alarms. Rising edge: alarm only fires if timeout persists for
// 1.5x the configured timeout. Falling edge: alarm clears when data returns
// within 0.5x timeout (i.e., brief stale window, now recovered).
void MqttManager::_checkTimeout(const Settings& settings) {
    if (!_status.mqttRxOk) return;  // never received anything yet → already invalid

    unsigned long now = millis();
    unsigned long elapsed = now - _status.mqttLastUpdateMs;
    unsigned long timeoutMs = (unsigned long)settings.mqttTimeoutSec * 1000UL;
    unsigned long alarmThresholdMs = (unsigned long)(timeoutMs * TIMEOUT_MULTIPLIER);
    unsigned long recoveryThresholdMs = (unsigned long)(timeoutMs / RECOVERY_DIVISOR);

    // Rising edge: timeout condition detected, start tracking
    if (elapsed > timeoutMs && !_mqttTimeoutTriggered) {
        _mqttTimeoutTriggered = true;
        _mqttTimeoutAlarmedAtMs = now;
#if DEBUG_DIAG
        Serial.print("[Opta1] MQTT timeout condition detected, will alarm if persists > ");
        Serial.print(alarmThresholdMs);
        Serial.println(" ms");
#endif
    }

    // Hysteresis: only raise alarm if condition persists long enough
    if (_mqttTimeoutTriggered && !_alarms.mqttTimeout) {
        unsigned long alarmElapsed = now - _mqttTimeoutAlarmedAtMs;
        if (alarmElapsed >= alarmThresholdMs) {
            _status.mqttValid = false;
            _alarms.mqttTimeout = true;
            _io.inMqttPowerValid = false;
            Serial.println("[Opta1] MQTT timeout alarm TRIGGERED (hysteresis threshold reached)");
        }
    }

    // Falling edge: data recovered within recovery window
    if (_mqttTimeoutTriggered && elapsed <= recoveryThresholdMs) {
        if (_alarms.mqttTimeout) {
            _mqttRecoveredAtMs = now;
            _status.mqttValid = true;
            _alarms.mqttTimeout = false;
            _io.inMqttPowerValid = true;
            _mqttTimeoutTriggered = false;
#if DEBUG_DIAG
            Serial.print("[Opta1] MQTT recovered (was briefly stale, elapsed=");
            Serial.print(elapsed);
            Serial.println(" ms)");
#endif
        } else {
            // Timeout condition is gone, but alarm never fired → just reset trigger
            _mqttTimeoutTriggered = false;
#if DEBUG_DIAG
            Serial.println("[Opta1] MQTT timeout condition cleared (transient hiccup)");
#endif
        }
    }
}
