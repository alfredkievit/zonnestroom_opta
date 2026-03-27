#pragma once
#include "types.h"
#include "comm_manager.h"
#include "settings_storage.h"

class HaInterface {
public:
    HaInterface(CommManager& comm, Settings& settings, SystemStatus& status,
                AlarmState& alarms, IOState& io, SettingsStorage& storage);
    void update();

private:
    CommManager&    _comm;
    Settings&       _settings;
    SystemStatus&   _status;
    AlarmState&     _alarms;
    IOState&        _io;
    SettingsStorage& _storage;

    unsigned long _lastPublishMs = 0;

    float _prevTemp         = -999.0f;
    bool  _prevHeater       = false;
    bool  _prevPump         = false;
    bool  _prevLevelHigh    = false;
    bool  _prevLevelPumpAct = false;
    bool  _prevCommOk       = false;
    bool  _prevClockOk      = false;
    bool  _heartbeatBit     = false;

    void _publishAll();
    void _publishAlarms();
};
