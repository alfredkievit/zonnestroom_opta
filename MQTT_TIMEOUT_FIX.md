# MQTT Timeout Hysteresis Fix

## Problem
Dagelijks krijgt je `mqtt_timeout` alarm te veel valse positieven. Korte WiFi-hiccups (30–45 sec netwerk-stutter) triggeren het alarm, wat direct alles afsluit (WP, element, hottub). Dit is niet veilig genoeg voor een overschot-regeling.

## Root Cause
- **Geen debouncing**: Zodra meter zwijgt > 30 sec, gaat alarm aan. Zodra 1 bericht binnenkomt, gaat alarm af.
- **Te agressief default**: 30 sec is 1 gemiste update-cycle op WiFi. Ruis/interference = transient disconnect = false alarm.
- **Direct shutdown**: `!mqttValid` stopt alles instantaan — geen grace period.

## Solution
**Hysteresis in `mqtt_manager.cpp`:**
1. Timeout condition moet **1,5× langer aanhouden** voordat alarm werkelijk schiet
2. Alarm cleared alleen als data terug is **en** was maximaal 0,5× timeout weg
3. Transient hiccups (data snel terug) triggeren alarm niet meer

**Config update:**
- Default `mqttTimeoutSec` verhoogd van 30 naar **60 seconden**
  - Geeft meer margin voor WiFi-netwerk-stutter
  - Hysteresis: alarm schiet pas na ~90 sec zwijgen
  - Reset: alarm wist als data terug is binnen ~30 sec

## Files Changed

### 1. `mqtt_manager.h`
**Added:** Hysteresis state tracking
```cpp
bool          _mqttTimeoutTriggered = false;
unsigned long _mqttTimeoutAlarmedAtMs = 0;
unsigned long _mqttRecoveredAtMs = 0;
```

### 2. `mqtt_manager.cpp`
**Tuning constants:**
```cpp
constexpr float TIMEOUT_MULTIPLIER = 1.5f;  // Alarm fires at 1.5× timeout
constexpr float RECOVERY_DIVISOR = 2.0f;    // Recovery threshold = timeout / 2
```

**New logic in `_checkTimeout()`:**
- Rising edge: Start tracking when `elapsed > timeoutMs`
- Hysteresis arm: Alarm only fires if `elapsed > (timeoutMs × 1.5)`
- Falling edge: Clear alarm if data returns within recovery window
- Debug: Serial logging for timeout state changes

### 3. `types.h`
**Config change:**
```cpp
s.mqttTimeoutSec = 60;  // Was 30 seconds
```

## Behavior Timeline

### Before (Old Code)
```
t=0s    : Meter publishes normally
t=30s   : Last message received; meter goes silent
t=30.1s : elapsed > 30s → MQTT_TIMEOUT alarm triggers! Everything off.
t=35s   : Meter publishes again
t=35.1s : Data arrived → alarm clears
Result: 5 sec outage, user sees alarm, devices cycled unnecessarily
```

### After (New Code)
```
t=0s    : Meter publishes normally
t=60s   : Last message received; meter goes silent
t=61s   : elapsed > 60s → _mqttTimeoutTriggered = true (but no alarm yet)
t=75s   : elapsed > 90s (60×1.5) → MQTT_TIMEOUT alarm triggers
t=80s   : Meter publishes again
t=80.1s : elapsed < 30s (recovery threshold) → alarm clears immediately
Result: Brief stutter, alarm briefly notified (if really stale), recovered gracefully
```

## Testing

### Validate Hysteresis
1. Stop MQTT broker
2. Watch serial console: should see "timeout condition detected" at ~60s
3. At ~90s: "alarm TRIGGERED"
4. Restart broker: should see "recovered"

### Verify Default Behavior
- Normal meter: publishes every 10–20s, no alarm
- Temporary WiFi glitch: connects within 60s, alarm never fires
- Real problem (WiFi down >90s): alarm fires correctly

### Fine-Tune (if needed)
Adjust in `mqtt_manager.cpp`:
```cpp
constexpr float TIMEOUT_MULTIPLIER = 1.5f;  // Increase to 2.0 for longer patience
constexpr float RECOVERY_DIVISOR = 2.0f;    // Increase to 3.0 for tighter recovery
```

Or via HA settings: increase `mqttTimeoutSec` to 90+ if you want even more patience.

## Safety Notes

1. **Alarm still fires:** If MQTT is genuinely down for 90+ seconds, the alarm goes off and regeling stops. Safe.
2. **No loss of logic:** State machine (boiler_logic.cpp) unchanged — still checks `mqttValid` correctly.
3. **Backwards compatible:** If you revert, old code still works; new code just smoother.

## Deployment

1. Commit changes to your repo
2. Build & flash to Opta1
3. Monitor serial for 24 hours (normal day + night cycle)
4. Verify alarm count drops to ~0 (or only on real network issues)
5. Done!

---

**Author:** Alfred (code review patch)  
**Date:** 2026-07-09  
**Status:** Ready for testing
