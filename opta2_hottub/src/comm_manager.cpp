#include "comm_manager.h"
#include "settings_storage.h"
#include <PortentaEthernet.h>
#include <Ethernet.h>
#include <SPI.h>
#include <WiFi.h>
#include <math.h>
#include <string.h>

static constexpr uint8_t LAN_MQTT_FAIL_THRESHOLD = 3;

CommManager* CommManager::_instance = nullptr;

void CommManager::_onMessageCb(int size) {
    if (_instance) _instance->_handleMessage(size);
}

CommManager::CommManager(Settings& settings, SettingsStorage& storage,
                                                 SystemStatus& status, AlarmState& alarms, IOState& io)
    : _mqttLan(_ethernetClient), _mqttWifi(_wifiClient),
      _settings(settings), _storage(storage),
            _status(status), _alarms(alarms), _io(io)
{}

void CommManager::_configureMqttClient(MqttClient& client) {
    client.setId("opta2_hottub");
    client.setKeepAliveInterval(15 * 1000L);
    client.setConnectionTimeout(5 * 1000L);
    client.onMessage(_onMessageCb);
}

// ---------------------------------------------------------------------------
void CommManager::begin() {
    _instance = this;
    _configureMqttClient(_mqttLan);
    _configureMqttClient(_mqttWifi);
    _lastLanLinkUpMs = millis();
    _startupLanProbeUntilMs = millis() + LAN_STARTUP_PROBE_MS;
    _ensureLanConnected(true);
}

// ---------------------------------------------------------------------------
void CommManager::update(const Settings& settings) {
    const unsigned long now = millis();
    const bool rawLanLinkPresent = (Ethernet.linkStatus() != LinkOFF);
    if (rawLanLinkPresent) {
        _lastLanLinkUpMs = now;
    }
    const bool lanLinkPresent = rawLanLinkPresent || ((now - _lastLanLinkUpMs) < LAN_LINK_LOSS_DEBOUNCE_MS);
    const bool lanHardDown = ((now - _lastLanLinkUpMs) > LAN_LINK_HARD_DOWN_MS);

    _ensureLanConnected(lanLinkPresent);

    if (lanHardDown) {
        _dropLanSession();
    }

    const bool lanConnected = _isLanAvailable();
    const bool lanSuppressed = (now < _forceWifiUntilMs);

    _ensureWifiConnected(true);
    const bool wifiConnected = _isWifiAvailable();

    _status.lanConnected = lanConnected;
    _status.wifiConnected = wifiConnected;

    if (_lanWasConnected != lanConnected) {
        _lanWasConnected = lanConnected;
#if DEBUG_DIAG
        Serial.println(lanConnected ? "[Opta2] LAN connected" : "[Opta2] LAN disconnected");
#endif
    }

    if (_wifiWasConnected != wifiConnected) {
        _wifiWasConnected = wifiConnected;
#if DEBUG_DIAG
        Serial.println(wifiConnected ? "[Opta2] WiFi connected" : "[Opta2] WiFi disconnected");
#endif
    }

    _handleLanRecovery(lanLinkPresent, wifiConnected);

    if (!lanLinkPresent && _activeTransport == NetworkTransport::LAN && wifiConnected && !_mqttLan.connected()) {
        _dropLanSession();
    }

    const bool forceWifiOnLinkLoss = !lanLinkPresent && wifiConnected;

    NetworkTransport desiredTransport = NetworkTransport::NONE;
    if (lanConnected && !lanSuppressed && !forceWifiOnLinkLoss) {
        desiredTransport = NetworkTransport::LAN;
    } else if (wifiConnected) {
        desiredTransport = NetworkTransport::WIFI;
    } else if (lanConnected) {
        desiredTransport = NetworkTransport::LAN;
    }

    _switchTransport(desiredTransport);
    _status.activeNetworkTransport = _activeTransport;

    if (_activeMqtt == nullptr) {
        if (_mqttWasConnected) {
            _mqttWasConnected = false;
#if DEBUG_DIAG
            Serial.println("[Opta2] MQTT unavailable: no active network transport");
#endif
        }
        _checkCommTimeout(settings);
        _flushPendingSave();
        return;
    }

    if (!_activeMqtt->connected()) {
        _reconnect(settings);
    }
    if (_activeMqtt->connected()) {
        if (!_mqttWasConnected) {
            _mqttWasConnected = true;
#if DEBUG_DIAG
            Serial.println("[Opta2] MQTT connected");
#endif
        }
        _activeMqtt->poll();
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
bool CommManager::connected() { return (_activeMqtt != nullptr) && _activeMqtt->connected(); }

bool CommManager::controlLinkReady() const {
    return (_activeMqtt != nullptr) && _activeMqtt->connected() && _status.commOk && (millis() >= _publishHoldUntilMs);
}

// ---------------------------------------------------------------------------
void CommManager::publish(const char* topic, const char* payload, bool retain) {
    if (millis() < _publishHoldUntilMs) {
        return;
    }
    if (_activeMqtt == nullptr || !_activeMqtt->connected()) return;
    _activeMqtt->beginMessage(topic, retain);
    _activeMqtt->print(payload);
    if (!_activeMqtt->endMessage()) {
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
    if (_activeMqtt == nullptr) {
        return;
    }

    const unsigned long now = millis();
    if ((now - _lastReconnectTryMs) < MQTT_RETRY_INTERVAL_MS) {
        return;
    }
    _lastReconnectTryMs = now;

    IPAddress broker(BROKER_IP[0], BROKER_IP[1], BROKER_IP[2], BROKER_IP[3]);
    if (!_activeMqtt->connect(broker, BROKER_PORT)) {
        if (_activeTransport == NetworkTransport::LAN) {
            if (_lanMqttFailureCount < 255) {
                _lanMqttFailureCount++;
            }
            if (_lanMqttFailureCount >= LAN_MQTT_FAIL_THRESHOLD && _isWifiAvailable()) {
                _forceWifiUntilMs = now + LAN_WIFI_FALLBACK_HOLD_MS;
                _lanRecoveryDeadlineMs = 0;
#if DEBUG_DIAG
                Serial.println("[Opta2] LAN MQTT unstable -> WiFi fallback until next LAN recovery window");
#endif
            }
        }
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

    _subscribeTopics(*_activeMqtt);

    if (_activeTransport == NetworkTransport::LAN) {
        _lanMqttFailureCount = 0;
        _forceWifiUntilMs = 0;
    }
}

void CommManager::_subscribeTopics(MqttClient& client) {
    // Opta1 device topics
    client.subscribe(TOPIC_MASTER_PERM_HOTTUB);
    client.subscribe(TOPIC_MASTER_HEARTBEAT);

    // HA command topics
    client.subscribe(TOPIC_CMD_SP_HOTTUB_TARGET);
    client.subscribe(TOPIC_CMD_SP_HOTTUB_HYST);
    client.subscribe(TOPIC_CMD_SP_HOTTUB_MAX);
    client.subscribe(TOPIC_CMD_SP_PUMP_1_HOUR);
    client.subscribe(TOPIC_CMD_SP_PUMP_2_HOUR);
    client.subscribe(TOPIC_CMD_SP_PUMP_DURATION);
    client.subscribe(TOPIC_CMD_ENABLE_HOTTUB);
    client.subscribe(TOPIC_CMD_ENABLE_AUTO_PUMP);
    client.subscribe(TOPIC_CMD_MANUAL_PUMP);
    client.subscribe(TOPIC_CMD_MANUAL_LEVEL_PUMP);
    client.subscribe(TOPIC_CMD_ENABLE_LEVELPUMP);
    client.subscribe(TOPIC_CMD_CLOCK_MINUTE);
    client.subscribe(TOPIC_CMD_ENABLE_IRRIGATION);
    client.subscribe(TOPIC_CMD_MANUAL_IRRIGATION_PUMP);
    for (size_t idx = 0; idx < IRRIGATION_ZONE_COUNT; ++idx) {
        client.subscribe(TOPIC_CMD_IRRIGATION_ZONE_REQUEST[idx]);
    }
}

// ---------------------------------------------------------------------------
bool CommManager::_isLanAvailable() {
    const bool lanHardDown = ((millis() - _lastLanLinkUpMs) > LAN_LINK_HARD_DOWN_MS);
    if (_mqttLan.connected() && !lanHardDown) {
        return true;
    }

    IPAddress ip = Ethernet.localIP();
    if (ip[0] == 0) {
        return false;
    }

    const bool lanLinkPresent = ((millis() - _lastLanLinkUpMs) < LAN_LINK_LOSS_DEBOUNCE_MS);
    const bool lanStartupGrace = ((millis() - _lastLanBeginMs) < LAN_RECOVERY_GRACE_MS);
    return lanLinkPresent || lanStartupGrace;
}

bool CommManager::_isWifiAvailable() const {
    return WiFi.status() == WL_CONNECTED;
}

MqttClient* CommManager::_mqttForTransport(NetworkTransport transport) {
    if (transport == NetworkTransport::LAN) {
        return &_mqttLan;
    }
    if (transport == NetworkTransport::WIFI) {
        return &_mqttWifi;
    }
    return nullptr;
}

void CommManager::_switchTransport(NetworkTransport transport) {
    if (_activeTransport == transport) {
        return;
    }

#if DEBUG_DIAG
    Serial.print("[Opta2] switching transport to ");
    if (transport == NetworkTransport::LAN) {
        Serial.println("LAN");
    } else if (transport == NetworkTransport::WIFI) {
        Serial.println("WiFi");
    } else {
        Serial.println("NONE");
    }
#endif

    _ethernetClient.stop();
    _wifiClient.stop();
    _activeTransport = transport;
    _activeMqtt = _mqttForTransport(transport);
    _mqttWasConnected = false;
    _lastReconnectTryMs = 0;
    _commGraceUntilMs = millis() + NETWORK_TRANSITION_GRACE_MS;
    _publishHoldUntilMs = millis() + MQTT_PUBLISH_HOLD_AFTER_SWITCH_MS;

}

void CommManager::_handleLanRecovery(bool lanLinkPresent, bool wifiConnected) {
    const unsigned long now = millis();

    if (_activeTransport != NetworkTransport::WIFI) {
        _lanRecoveryDeadlineMs = 0;
        return;
    }

    if (_forceWifiUntilMs != 0 && now >= _forceWifiUntilMs) {
        _forceWifiUntilMs = 0;
        _lanMqttFailureCount = 0;
        _lastLanBeginMs = 0;
        _dropLanSession();
        if (lanLinkPresent) {
            _lanRecoveryDeadlineMs = now + LAN_RECOVERY_GRACE_MS;
#if DEBUG_DIAG
            Serial.println("[Opta2] LAN recovery window opened");
#endif
        }
    }

    if (_lanRecoveryDeadlineMs == 0) {
        return;
    }

    if (_mqttLan.connected()) {
        _lanRecoveryDeadlineMs = 0;
        return;
    }

    if (now < _lanRecoveryDeadlineMs) {
        return;
    }

    _lanRecoveryDeadlineMs = 0;
    if (lanLinkPresent && wifiConnected) {
#if DEBUG_DIAG
        Serial.println("[Opta2] LAN recovery failed during grace period -> software reset");
        delay(20);
#endif
        NVIC_SystemReset();
    }
}

void CommManager::_dropLanSession() {
    _mqttLan.stop();
    _ethernetClient.stop();
    _lanConfigured = false;
}

void CommManager::_ensureLanConnected(bool lanLinkPresent) {
    const unsigned long now = millis();
    if (_activeTransport == NetworkTransport::LAN) {
        return;
    }

    const bool startupProbeActive = now < _startupLanProbeUntilMs;
    const bool allowLanProbe = lanLinkPresent || startupProbeActive;

    if (!allowLanProbe) {
        return;
    }

    if (_isLanAvailable()) {
        return;
    }

    if ((now - _lastLanBeginMs) < LAN_RETRY_INTERVAL_MS) {
        return;
    }

    _lastLanBeginMs = now;
    Ethernet.begin(OPTA2_LAN_IP);
    _lanConfigured = true;
}

void CommManager::_ensureWifiConnected(bool wifiNeeded) {
    if (!wifiNeeded) {
        return;
    }

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
    if (_activeMqtt == nullptr) {
        return;
    }

    String topicStr = _activeMqtt->messageTopic();
    char topic[96] = {};
    topicStr.toCharArray(topic, sizeof(topic));
    char   buf[32] = {};
    int    len = min(messageSize, (int)sizeof(buf) - 1);
    for (int i = 0; i < len; i++) buf[i] = (char)_activeMqtt->read();
    while (_activeMqtt->available()) _activeMqtt->read();

    bool bval = (buf[0] == '1' || buf[0] == 't' || buf[0] == 'T');

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
            _commGraceUntilMs  = 0;
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
        _io.manualForceLevelPump = bval;
    }
    else if (strcmp(topic, TOPIC_CMD_MANUAL_IRRIGATION_PUMP) == 0) {
        _io.manualForceIrrigationPump = bval;
    }
    else {
        for (size_t idx = 0; idx < IRRIGATION_ZONE_COUNT; ++idx) {
            if (strcmp(topic, TOPIC_CMD_IRRIGATION_ZONE_REQUEST[idx]) == 0) {
                bool previous = _io.irrigationZoneRequest[idx];
                _io.irrigationZoneRequest[idx] = bval;
                if (bval && !previous) {
                    _io.irrigationZoneRequestSequence[idx] = ++_zoneRequestSequenceCounter;
                } else if (!bval) {
                    _io.irrigationZoneRequestSequence[idx] = 0;
                }
                return;
            }
        }

        handleCommand(topic, buf, len, _settings);
    }
}

// ---------------------------------------------------------------------------
void CommManager::_checkCommTimeout(const Settings& settings) {
    if (!_heartbeatEverRx) return;
    if (_commGraceUntilMs != 0 && millis() < _commGraceUntilMs) {
        _status.commOk = true;
        _alarms.hottubCommTimeout = false;
        _io.inMasterCommValid = true;
        return;
    }
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
    else if (strcmp(topic, TOPIC_CMD_ENABLE_IRRIGATION) == 0) {
        if (settings.enableIrrigation != bval) {
            settings.enableIrrigation = bval;
            changed = true;
        }
        if (!bval) {
            for (size_t idx = 0; idx < IRRIGATION_ZONE_COUNT; ++idx) {
                _io.irrigationZoneRequest[idx] = false;
                _io.irrigationZoneRequestSequence[idx] = 0;
            }
        }
    }

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
