#include "settings_storage.h"
#include <KVStore.h>
#include <TDBStore.h>
#include <FlashIAP.h>
#include <FlashIAPBlockDevice.h>

// KVStore key – single blob for the entire Settings struct
static const char* KV_KEY = "/kv/opta1_settings";

// Flash block device for the Opta's internal flash (last 256 KB)
static FlashIAPBlockDevice bd(0x081C0000, 0x40000);
static mbed::TDBStore      store(&bd);

void SettingsStorage::begin() {
    store.init();
}

void SettingsStorage::save(const Settings& s) {
    store.set(KV_KEY, &s, sizeof(Settings), 0);
}

bool SettingsStorage::load(Settings& s) {
    size_t actual = 0;
    int rc = store.get(KV_KEY, &s, sizeof(Settings), &actual);
    return (rc == MBED_SUCCESS && actual == sizeof(Settings));
}
