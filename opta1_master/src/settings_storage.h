#pragma once
#include "types.h"

// Persists Settings to Opta onboard flash (MbedOS KVStore).
// On reboot, settings survive without requiring Home Assistant.
class SettingsStorage {
public:
    void begin();
    void save(const Settings& s);
    bool load(Settings& s);   // returns false if no saved data found
};
