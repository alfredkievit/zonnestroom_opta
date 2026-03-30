#include "comm_manager.h"
#include "settings_storage.h"
#include <WiFi.h>
#include <math.h>
#include <string.h>

CommManager* CommManager::_instance = nullptr;

void CommManager::_onMessageCb(int size) {
    if (_instance) _instance->_handleMessage(size);
}

CommManager::CommManager(Settings& settings, SettingsStorage& storage,
                                                 SystemStatus& status, AlarmState& alarms, IOState& io)
    : _mqtt(_wifiClient), _settings(settings), _storage(storage),
            _status(status), _alarms(alarms), _io(io)
{}

// ---------------------------------------------------------------------------
void CommManager::begin() {
    _lastWifiBeginMs = millis();
    WiFi.begin(OPTA2_WIFI_SSID, OPTA2_WIFI_PASS);

    _mqtt.setId("opta2_hottub");
    _mqtt.setKeepAliveInterval(15 * 1000L);
    _mqtt.setConnectionTimeout(5 * 1000L);

    // Register static callback — ArduinoMqttClient does not support lambdas
    _instance = this;
    _mqtt.onMessage(_onMessageCb);
}

// ---------------------------------------------------------------------------
void CommManager::update(const Settings& settings) {
    _ensureWifiConnected();

    const bool wifiConnected = (WiFi.status() == WL_CONNECTED);
    if (_wifiWasConnected != wifiConnected) {
        _wifiWasConnected = wifiConnected;
#if DEBUG_DIAG
        Serial.println(wifiConnected ? "[Opta2] WiFi connected" : "[Opta2] WiFi disconnected");
#endif
    }

    if (!wifiConnected) {
        if (_mqttWasConnected) {
            _mqttWasConnected = false;
#if DEBUG_DIAG
            Serial.println("[Opta2] MQTT unavailable: WiFi down");
#endif
        }
        _checkCommTimeout(settings);
        _flushPendingSave();
        return;
    }

    if (!_mqtt.connected()) {
        _reconnect(settings);
    }
    if (_mqtt.connected()) {
        if (!_mqttWasConnected) {
            _mqttWasConnected = true;
#if DEBUG_DIAG
            Serial.println("[Opta2] MQTT connected");
#endif
        }
        _mqtt.poll();
    } else if (_mqttWasConnected) {
        _mqttWasConnected = false;
#if DEBUG_DIAG
        Serial.println("[Opta2] MQTT disconnected");
#endif
    }

    _checkCommTimeout(settings);
    _flushPendingSave();
}

// ---------------------------------------------------------------------------
bool CommManager::connected() { return _mqtt.connected(); }

// ---------------------------------------------------------------------------
void CommManager::publish(const char* topic, const char* payload, bool retain) {
    if (!_mqtt.connected()) return;
    _mqtt.beginMessage(topic, retain);
    _mqtt.print(payload);
    if (!_mqtt.endMessage()) {
#if DEBUG_DIAG
        Serial.print("[Opta2] MQTT publish failed: ");
        Serial.println(topic);
#endif
    }
}
void CommManager::publish(const char* topic, int value, bool retain) {
    char buf[16]; snprintf(buf, sizeof(buf), "%d", value);
    publish(topic, buf, retain);
}
void CommManager::publish(const char* topic, float value, uint8_t decimals, bool retain) {
    char buf[16];
    snprintf(buf, sizeof(buf), "%.*f", (int)decimals, (double)value);
    publish(topic, buf, retain);
}

// ---------------------------------------------------------------------------
void CommManager::_reconnect(const Settings& settings) {
    const unsigned long now = millis();
    if ((now - _lastReconnectTryMs) < MQTT_RETRY_INTERVAL_MS) {
        return;
    }
    _lastReconnectTryMs = now;

    IPAddress broker(BROKER_IP[0], BROKER_IP[1], BROKER_IP[2], BROKER_IP[3]);
    if (!_mqtt.connect(broker, BROKER_PORT)) {
#if DEBUG_DIAG
        if ((now - _lastConnectLogMs) >= CONNECT_LOG_INTERVAL_MS) {
            _lastConnectLogMs = now;
            Serial.println("[Opta2] MQTT connect failed");
        }
#endif
        return;
    }

#if DEBUG_DIAG
    Serial.println("[Opta2] MQTT subscribe setup");
#endif

    // Opta1 device topics
    _mqtt.subscribe(TOPIC_MASTER_PERM_HOTTUB);
    _mqtt.subscribe(TOPIC_MASTER_HEARTBEAT);

    // HA command topics
    _mqtt.subscribe(TOPIC_CMD_SP_HOTTUB_TARGET);
    _mqtt.subscribe(TOPIC_CMD_SP_HOTTUB_HYST);
    _mqtt.subscribe(TOPIC_CMD_SP_HOTTUB_MAX);
    _mqtt.subscribe(TOPIC_CMD_SP_PUMP_1_HOUR);
    _mqtt.subscribe(TOPIC_CMD_SP_PUMP_2_HOUR);
    _mqtt.subscribe(TOPIC_CMD_SP_PUMP_DURATION);
    _mqtt.subscribe(TOPIC_CMD_ENABLE_HOTTUB);
    _mqtt.subscribe(TOPIC_CMD_ENABLE_AUTO_PUMP);
    _mqtt.subscribe(TOPIC_CMD_MANUAL_PUMP);
    _mqtt.subscribe(TOPIC_CMD_MANUAL_LEVEL_PUMP);
    _mqtt.subscribe(TOPIC_CMD_ENABLE_LEVELPUMP);
    _mqtt.subscribe(TOPIC_CMD_CLOCK_MINUTE);
}

// ---------------------------------------------------------------------------
void CommManager::_ensureWifiConnected() {
    if (WiFi.status() == WL_CONNECTED) {
        return;
    }

    const unsigned long now = millis();
    if ((now - _lastWifiBeginMs) >= WIFI_RETRY_INTERVAL_MS) {
        _lastWifiBeginMs = now;
        WiFi.disconnect();
        WiFi.begin(OPTA2_WIFI_SSID, OPTA2_WIFI_PASS);
    }

#if DEBUG_DIAG
    if ((now - _lastConnectLogMs) >= CONNECT_LOG_INTERVAL_MS) {
        _lastConnectLogMs = now;
        Serial.print("[Opta2] WiFi reconnect pending, status=");
        Serial.println((int)WiFi.status());
    }
#endif
}

// ---------------------------------------------------------------------------
void CommManager::_handleMessage(int messageSize) {
    String topicStr = _mqtt.messageTopic();
    char topic[96] = {};
    topicStr.toCharArray(topic, sizeof(topic));
    char   buf[32] = {};
    int    len = min(messageSize, (int)sizeof(buf) - 1);
    for (int i = 0; i < len; i++) buf[i] = (char)_mqtt.read();
    while (_mqtt.available()) _mqtt.read();

    if (strcmp(topic, TOPIC_MASTER_PERM_HOTTUB) == 0) {
        _io.inMasterPermissionHottub = (buf[0] == '1');
    }
    else if (strcmp(topic, TOPIC_MASTER_HEARTBEAT) == 0) {
        bool bit = (buf[0] == '1');
        if (!_heartbeatEverRx || bit != _lastHeartbeatBit) {
            // Heartbeat toggled → comm is alive
            _lastHeartbeatBit  = bit;
            _lastHeartbeatRxMs = millis();
            _heartbeatEverRx   = true;
            _status.commOk     = true;
            _alarms.hottubCommTimeout = false;
#if DEBUG_DIAG
            Serial.print("[Opta2] heartbeat rx bit=");
            Serial.println(bit ? "1" : "0");
#endif
        }
        _io.inMasterHeartbeat = bit;
    }
    else if (strcmp(topic, TOPIC_CMD_CLOCK_MINUTE) == 0) {
        int minuteOfDay = atoi(buf);
        if (minuteOfDay >= 0 && minuteOfDay <= 1439) {
            _io.clockMinuteOfDay = minuteOfDay;
            _io.clockMinuteValid = true;
            _status.clockOk = true;
            _lastClockRxMs = millis();
        }
    }
    else if (strcmp(topic, TOPIC_CMD_MANUAL_PUMP) == 0) {
        _io.manualForcePump = (buf[0] == '1' || buf[0] == 't' || buf[0] == 'T');
    }
    else if (strcmp(topic, TOPIC_CMD_MANUAL_LEVEL_PUMP) == 0) {
        _io.manualForceLevelPump = (buf[0] == '1' || buf[0] == 't' || buf[0] == 'T');
    }
    else {
        handleCommand(topic, buf, len, _settings);
    }
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

    if (_lastClockRxMs > 0 && (millis() - _lastClockRxMs) > (2UL * 60UL * 1000UL)) {
        _io.clockMinuteValid = false;
        _status.clockOk = false;
    }
}

// ---------------------------------------------------------------------------
void CommManager::handleCommand(const char* topic, const char* payload, int len,
                                Settings& settings) {
    char buf[32] = {};
    memcpy(buf, payload, min(len, (int)sizeof(buf) - 1));
    float val  = atof(buf);
    bool  bval = (buf[0] == '1' || buf[0] == 't' || buf[0] == 'T');

    bool changed = false;

    if      (strcmp(topic, TOPIC_CMD_SP_HOTTUB_TARGET) == 0) { if (fabsf(settings.spHottubTargetC - val) > 0.001f) { settings.spHottubTargetC = val; changed = true; } }
    else if (strcmp(topic, TOPIC_CMD_SP_HOTTUB_HYST) == 0)   { if (fabsf(settings.spHottubHystC - val) > 0.001f) { settings.spHottubHystC = val; changed = true; } }
    else if (strcmp(topic, TOPIC_CMD_SP_HOTTUB_MAX) == 0)    { if (fabsf(settings.spHottubMaxC - val) > 0.001f) { settings.spHottubMaxC = val; changed = true; } }
    else if (strcmp(topic, TOPIC_CMD_SP_PUMP_1_HOUR) == 0)   { int v = constrain((int)val, 0, 24); if (settings.spPumpRun1Hour != v) { settings.spPumpRun1Hour = v; changed = true; } }
    else if (strcmp(topic, TOPIC_CMD_SP_PUMP_2_HOUR) == 0)   { int v = constrain((int)val, 0, 24); if (settings.spPumpRun2Hour != v) { settings.spPumpRun2Hour = v; changed = true; } }
    else if (strcmp(topic, TOPIC_CMD_SP_PUMP_DURATION) == 0) { int v = constrain((int)val, 1, 120); if (settings.spPumpRunDurationMin != v) { settings.spPumpRunDurationMin = v; changed = true; } }
    else if (strcmp(topic, TOPIC_CMD_ENABLE_HOTTUB) == 0)    { if (settings.enableHottub != bval) { settings.enableHottub = bval; changed = true; } }
    else if (strcmp(topic, TOPIC_CMD_ENABLE_AUTO_PUMP) == 0) { if (settings.enableAutoPump != bval) { settings.enableAutoPump = bval; changed = true; } }
    else if (strcmp(topic, TOPIC_CMD_ENABLE_LEVELPUMP) == 0) { if (settings.enableLevelPump != bval) { settings.enableLevelPump = bval; changed = true; } }

    if (changed) {
        _markSettingsDirty();
    }
}

// ---------------------------------------------------------------------------
void CommManager::_markSettingsDirty() {
    _settingsDirty = true;
    _settingsDirtySinceMs = millis();
}

// ---------------------------------------------------------------------------
void CommManager::_flushPendingSave() {
    if (!_settingsDirty) {
        return;
    }
    if ((millis() - _settingsDirtySinceMs) < SETTINGS_SAVE_DEBOUNCE_MS) {
        return;
    }
    _storage.save(_settings);
    _settingsDirty = false;
#if DEBUG_DIAG
    Serial.println("[Opta2] settings persisted");
#endif
}
