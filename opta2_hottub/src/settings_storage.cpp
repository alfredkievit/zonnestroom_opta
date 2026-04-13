#include "settings_storage.h"
#include <KVStore.h>
#include <TDBStore.h>
#include <FlashIAP.h>
#include <FlashIAPBlockDevice.h>

static const char* KV_KEY = "/kv/opta2_settings";
static const uint32_t SETTINGS_MAGIC = 0x4F325332UL;
static const uint16_t SETTINGS_VERSION = 2;

struct LegacySettingsV1 {
    float spHottubTargetC;
    float spHottubHystC;
    float spHottubMaxC;
    int   spPumpRun1Hour;
    int   spPumpRun2Hour;
    int   spPumpRunDurationMin;
    int   tLevelPumpMaxRunSec;
    int   tCommWatchdogSec;
    bool  enableHottub;
    bool  enableAutoPump;
    bool  enableLevelPump;
};

struct StoredSettingsV2 {
    uint32_t magic;
    uint16_t version;
    uint16_t size;
    Settings settings;
};

static FlashIAPBlockDevice bd(0x081C0000, 0x40000);
static mbed::TDBStore      store(&bd);

void SettingsStorage::begin()              { store.init(); }

void SettingsStorage::save(const Settings& s) {
    StoredSettingsV2 stored { SETTINGS_MAGIC, SETTINGS_VERSION, static_cast<uint16_t>(sizeof(Settings)), s };
    store.set(KV_KEY, &stored, sizeof(stored), 0);
}

bool SettingsStorage::load(Settings& s) {
    uint8_t raw[sizeof(StoredSettingsV2)] = {};
    size_t actual = 0;
    int rc = store.get(KV_KEY, raw, sizeof(raw), &actual);
    if (rc != MBED_SUCCESS) {
        return false;
    }

    if (actual == sizeof(StoredSettingsV2)) {
        StoredSettingsV2 stored {};
        memcpy(&stored, raw, sizeof(stored));
        if (stored.magic == SETTINGS_MAGIC &&
            stored.version == SETTINGS_VERSION &&
            stored.size == sizeof(Settings)) {
            s = stored.settings;
            return true;
        }
    }

    if (actual == sizeof(LegacySettingsV1)) {
        LegacySettingsV1 legacy {};
        memcpy(&legacy, raw, sizeof(legacy));

        s = defaultSettings();
        s.spHottubTargetC = legacy.spHottubTargetC;
        s.spHottubHystC = legacy.spHottubHystC;
        s.spHottubMaxC = legacy.spHottubMaxC;
        s.spPumpRun1Hour = legacy.spPumpRun1Hour;
        s.spPumpRun2Hour = legacy.spPumpRun2Hour;
        s.spPumpRunDurationMin = legacy.spPumpRunDurationMin;
        s.tLevelPumpMaxRunSec = legacy.tLevelPumpMaxRunSec;
        s.tCommWatchdogSec = legacy.tCommWatchdogSec;
        s.enableHottub = legacy.enableHottub;
        s.enableAutoPump = legacy.enableAutoPump;
        s.enableLevelPump = legacy.enableLevelPump;

        save(s);
        return true;
    }

    return false;
}
