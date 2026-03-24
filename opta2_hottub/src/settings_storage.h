#pragma once
#include "types.h"

class SettingsStorage {
public:
    void begin();
    void save(const Settings& s);
    bool load(Settings& s);
};
