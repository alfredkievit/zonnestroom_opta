#include "comm_manager.h"
#include "settings_storage.h"
#include <WiFi.h>

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
    WiFi.begin(OPTA2_WIFI_SSID, OPTA2_WIFI_PASS);

    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 20) {
        delay(500);
        attempts++;
    }
    delay(500);

    _mqtt.setId("opta2_hottub");
    _mqtt.setKeepAliveInterval(15 * 1000L);
    _mqtt.setConnectionTimeout(5 * 1000L);

    // Register static callback — ArduinoMqttClient does not support lambdas
    _instance = this;
    _mqtt.onMessage(_onMessageCb);
}

// ---------------------------------------------------------------------------
void CommManager::update(const Settings& settings) {
    if (!_mqtt.connected()) {
        _reconnect(settings);
    }
    _mqtt.poll();
    _checkCommTimeout(settings);
}

// ---------------------------------------------------------------------------
bool CommManager::connected() { return _mqtt.connected(); }

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
    char buf[16];
    snprintf(buf, sizeof(buf), "%.*f", (int)decimals, (double)value);
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
void CommManager::_handleMessage(int messageSize) {
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
    else if (topic == TOPIC_CMD_CLOCK_MINUTE) {
        int minuteOfDay = atoi(buf);
        if (minuteOfDay >= 0 && minuteOfDay <= 1439) {
            _io.clockMinuteOfDay = minuteOfDay;
            _io.clockMinuteValid = true;
            _status.clockOk = true;
            _lastClockRxMs = millis();
        }
    }
    else if (topic == TOPIC_CMD_MANUAL_PUMP) {
        _io.manualForcePump = (buf[0] == '1' || buf[0] == 't' || buf[0] == 'T');
    }
    else if (topic == TOPIC_CMD_MANUAL_LEVEL_PUMP) {
        _io.manualForceLevelPump = (buf[0] == '1' || buf[0] == 't' || buf[0] == 'T');
    }
    else {
        handleCommand(topic, buf, len, _settings);
        _storage.save(_settings);
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
void CommManager::handleCommand(const String& topic, const char* payload, int len,
                                Settings& settings) {
    char buf[32] = {};
    memcpy(buf, payload, min(len, (int)sizeof(buf) - 1));
    float val  = atof(buf);
    bool  bval = (buf[0] == '1' || buf[0] == 't' || buf[0] == 'T');

    if      (topic == TOPIC_CMD_SP_HOTTUB_TARGET) { settings.spHottubTargetC     = val;  }
    else if (topic == TOPIC_CMD_SP_HOTTUB_HYST)   { settings.spHottubHystC       = val;  }
    else if (topic == TOPIC_CMD_SP_HOTTUB_MAX)    { settings.spHottubMaxC        = val;  }
    else if (topic == TOPIC_CMD_SP_PUMP_1_HOUR)   { settings.spPumpRun1Hour      = constrain((int)val, 0, 24); }
    else if (topic == TOPIC_CMD_SP_PUMP_2_HOUR)   { settings.spPumpRun2Hour      = constrain((int)val, 0, 24); }
    else if (topic == TOPIC_CMD_SP_PUMP_DURATION) { settings.spPumpRunDurationMin = constrain((int)val, 1, 120); }
    else if (topic == TOPIC_CMD_ENABLE_HOTTUB)    { settings.enableHottub        = bval; }
    else if (topic == TOPIC_CMD_ENABLE_AUTO_PUMP) { settings.enableAutoPump      = bval; }
    else if (topic == TOPIC_CMD_ENABLE_LEVELPUMP) { settings.enableLevelPump     = bval; }
}
