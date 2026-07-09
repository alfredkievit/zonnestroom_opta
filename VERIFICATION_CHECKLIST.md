# MQTT Timeout Hysteresis — Verification Checklist

## Pre-Deployment

- [ ] Code compiles without errors
  ```bash
  pio run -e arduino_opta
  ```
- [ ] No new compiler warnings
- [ ] No `#include` or symbol issues

## Post-Flash Testing (Local)

### 1. Normal Operation (meter publishing regularly)
- [ ] Flash code to Opta1
- [ ] Meter connected to network, publishing meter data
- [ ] **Expected:** Serial console shows `[Opta1] MQTT connected`
- [ ] **Expected:** No alarm messages, no `mqtt_timeout` in HA
- [ ] **Duration:** 10 minutes
- [ ] **Pass:** No alarms in console

### 2. Transient Hiccup Test (brief WiFi stutter)
- [ ] Meter still connected, publishing normally
- [ ] Unplug WiFi router for **45 seconds** only
- [ ] Plug it back in
- [ ] **Expected around t=61s:** Serial shows `[Opta1] MQTT timeout condition detected, will alarm if persists > 90000 ms`
- [ ] **Expected around t=80s:** Meter reconnects, Serial shows `[Opta1] MQTT recovered (was briefly stale, elapsed=X ms)`
- [ ] **Expected:** NO alarm fired (no `MQTT_TIMEOUT` in console)
- [ ] **Expected:** No alarm in HA
- [ ] **Pass:** Timeout condition was detected but alarm never fired

### 3. Genuine Problem Test (real timeout, >90 sec)
- [ ] **Starting state:** Meter connected, no alarm
- [ ] Unplug WiFi router
- [ ] Wait 95 seconds
- [ ] **Expected around t=61s:** Serial shows `MQTT timeout condition detected`
- [ ] **Expected around t=96s:** Serial shows `MQTT timeout alarm TRIGGERED`
- [ ] **Expected:** Alarm appears in HA as `mqtt_timeout: true`
- [ ] **Expected:** Regeling stops (WP off, element off, hottub permission removed)
- [ ] Plug WiFi back in
- [ ] **Expected:** Data returns within 30s, alarm clears in console and HA
- [ ] **Pass:** Alarm correctly triggered for real problem

### 4. Rapid Reconnect Test (immediate recovery)
- [ ] **Starting state:** Meter connected, no alarm
- [ ] Unplug WiFi router for **15 seconds only**
- [ ] Plug it back in
- [ ] **Expected:** No timeout condition at all (elapsed never reaches 60s)
- [ ] **Expected:** No alarm
- [ ] **Duration:** 30 seconds total
- [ ] **Pass:** Very brief outages completely ignored

### 5. Multiple Hiccups (back-to-back stutter)
- [ ] **Starting state:** Meter connected
- [ ] Unplug WiFi for 40 sec → reconnect → wait 20 sec
- [ ] Unplug WiFi for 40 sec again → reconnect
- [ ] **Expected:** `timeout condition detected` appears twice, but alarm never fires
- [ ] **Expected:** System stays active both times (no false alarms)
- [ ] **Pass:** Hysteresis applies equally to each cycle

## Production Monitoring (24+ hours)

### Day 1 (Daytime, normal usage)
- [ ] System running on new code
- [ ] Manual inspection of HA: `mqtt_timeout` should be `false` most of the time
- [ ] Note any alarm events: `mqttTimeout: true` in logs
- [ ] Expected: 0–2 transient conditions, 0 full alarms
- [ ] Regeling active: WP/element/hottub control working

### Night 1 (Overnight, low activity)
- [ ] System still running
- [ ] Monitor logs: Serial shows normal keepalive
- [ ] Expected: 0 alarms

### Day 2+ (Continuous)
- [ ] Check HA history: `mqtt_timeout` alarm count
- [ ] Compare to old behavior:
  - **Old:** 5–10 alarm events per day
  - **New:** 0–1 alarm events per day (only on real problems)
- [ ] Regeling behavior: did it trigger more or less than before?
  - If less: that's correct (fewer false stops)
  - If more: alarm hysteresis is working, real outages now detected

## Logs to Capture

### Serial Console During Testing
Save output to a file for analysis:
```bash
# In PlatformIO, enable serial monitor and log to file:
pio device monitor -p COM3 > mqtt_test_$(date +%s).log
```

### Look for these patterns in logs
- `MQTT timeout condition detected` — False alarm risk, check if it recovers
- `MQTT timeout alarm TRIGGERED` — Genuine problem, should stop regeling
- `MQTT recovered` — Recovery successful, regeling resumes
- `MQTT connected` / `MQTT disconnected` — WiFi state changes

## Rollback Plan

If alarm behavior is worse, rollback is simple:
```cpp
// In mqtt_manager.cpp, revert _checkTimeout() to old version
// Or temporarily: increase TIMEOUT_MULTIPLIER to 2.0 or 3.0 for more patience
// Or: increase mqttTimeoutSec to 90–120 in types.h
```

## Sign-Off

- [ ] All local tests passed
- [ ] 24+ hour production monitoring complete
- [ ] Alarm count acceptable (< 2 per day)
- [ ] Regeling behavior correct (no unwanted stops)
- [ ] Serial logs reviewed for anomalies
- **Ready:** Code is stable

---

**Test Date:** ________________  
**Tester:** ________________  
**Result:** ☐ PASS ☐ FAIL ☐ NEEDS ADJUSTMENT  
**Notes:** ________________________________________________________________

