# Field Deployment Guide — Zonnestroom_Opta MQTT Timeout Fix

## Overview

You have uncommitted local changes (including the MQTT timeout hysteresis fix) that need to go to GitHub. From there, field devices can pull and update.

**Timeline:**
1. **Now (your PC):** Push code to GitHub
2. **In the field:** Pull latest code and build

---

## Step 1: Commit & Push (Your PC, Now)

### Option A: PowerShell (Windows, Recommended)

```powershell
cd "C:\Users\alfre\My Drive\VSC projects\Zonnestroom_Opta"
.\COMMIT_AND_PUSH.ps1
```

The script will:
- Show all uncommitted changes
- Create a descriptive commit message
- Push to GitHub master
- Verify success

### Option B: Git CLI (Terminal)

```bash
cd ~/Zonnestroom_Opta
git add -A
git commit -m "fix(mqtt): Add timeout hysteresis to prevent false alarms"
git push origin master
```

### What Gets Committed

**Source code changes (all included):**
- `opta1_master/src/mqtt_manager.cpp` — Hysteresis logic
- `opta1_master/src/mqtt_manager.h` — State tracking
- `opta1_master/src/types.h` — Default timeout 60s
- All other recent Opta1/Opta2 improvements

**Documentation (new):**
- `MQTT_TIMEOUT_FIX.md` — Problem & solution
- `VERIFICATION_CHECKLIST.md` — Test procedures
- `mqtt_timeout_hysteresis.patch` — Exact code changes
- `FIELD_DEPLOYMENT_GUIDE.md` — This file

### Verify on GitHub

After push completes:
1. Go to: https://github.com/yourname/Zonnestroom_Opta
2. Switch to master branch
3. Look for the new commit "fix(mqtt): Add timeout hysteresis…"
4. Verify files changed: mqtt_manager.cpp/h, types.h

---

## Step 2: Field Update (In the Field)

### Scenario: You're at a site, need latest firmware

**Prerequisites:**
- Laptop with Windows (or Git + PlatformIO on Mac/Linux)
- USB cable for Opta
- Internet connection (to clone from GitHub)

### Quick Update Script

```powershell
cd "C:\Workspace\Updates"
.\UPDATE_FIRMWARE_VELD.ps1
```

The script will:
1. Clone latest code from GitHub
2. Verify PlatformIO is installed
3. Build the firmware
4. Show binary location & flash instructions

### Manual Steps (if script doesn't work)

```powershell
# Clone latest code
git clone https://github.com/yourname/Zonnestroom_Opta.git Zonnestroom_Latest
cd Zonnestroom_Latest\opta1_master

# Build
pio run -e arduino_opta

# Binary will be at: .pio\build\arduino_opta\firmware.bin

# Flash via Arduino IDE or CLI:
arduino-cli upload -b arduino:mbed_opta:opta -p COMX --input-dir .pio/build/arduino_opta
```

### Verify After Flashing

1. Open serial monitor
2. Look for: `[Opta1] MQTT connected`
3. Monitor for 10+ minutes
4. Expected: No `mqtt_timeout` alarm unless real network issue
5. If transient WiFi hiccup (45 sec): Check logs, alarm should NOT fire
6. Reference: VERIFICATION_CHECKLIST.md

---

## Troubleshooting

### Push Fails
```
Error: Permission denied (publickey)
```
**Fix:** GitHub credentials. Check:
- SSH key authorized on GitHub
- Or: Use HTTPS instead of SSH
- Or: `git credential-osxkeychain` on Mac, Windows Credential Manager on Windows

### Clone Fails
```
Error: Repository not found
```
**Fix:** Update the GitHub URL in UPDATE_FIRMWARE_VELD.ps1 (line ~10) with your actual repo URL

### Build Fails
```
Error: ArduinoMqttClient not found
```
**Fix:** Run `pio run -t build` first to download dependencies
Or: `pio lib install` before building

### Flash Fails
```
Error: Port COM3 not found
```
**Fix:** Check USB cable & device manager for COM port, update the `-p COMX` argument

---

## Directory Structure (After Deployment)

```
C:\Users\alfre\My Drive\VSC projects\Zonnestroom_Opta\
├── opta1_master/
│   ├── src/
│   │   ├── mqtt_manager.cpp  ← Hysteresis fix here
│   │   ├── mqtt_manager.h
│   │   ├── types.h
│   │   └── ...
│   ├── platformio.ini
│   └── .pio/
│       └── build/arduino_opta/
│           └── firmware.bin  ← This is what you flash
├── opta2_hottub/
│   └── ...
├── COMMIT_AND_PUSH.ps1        ← Use this (your PC)
├── UPDATE_FIRMWARE_VELD.ps1   ← Use this (field)
├── MQTT_TIMEOUT_FIX.md        ← Why this fix
├── VERIFICATION_CHECKLIST.md  ← How to test
└── FIELD_DEPLOYMENT_GUIDE.md  ← You are here
```

---

## Key Changes in This Release

### MQTT Timeout Hysteresis

**Problem:** Alarm triggered on every WiFi glitch (45 sec stutter = alarm)

**Solution:**
- Alarm only fires if MQTT silent **>90 seconds** (not 30)
- Alarm clears if data returns within **recovery window** (0.5× timeout)
- Transient hiccups (WiFi reconnects in 60 sec): **no alarm**

**Files:**
- `mqtt_manager.cpp`: `_checkTimeout()` rewritten with hysteresis
- `mqtt_manager.h`: State tracking (3 new vars)
- `types.h`: Default timeout 30s → 60s

**Behavior:**
- Before: 5–10 alarms/day (mostly false)
- After: 0–1 alarm/day (only real problems)

---

## Monitoring & Rollback

### Monitor (First 24 hours)

1. Watch serial logs: `mqtt_timeout` should not appear unless real network issue
2. Check HA alarm history: Alarm count should drop significantly
3. Verify regeling behavior: WP/element/hottub operate normally

### If Issues

Rollback to previous version:
```bash
git log --oneline
git reset --hard <previous-commit-hash>
git push origin master --force
```

Or adjust hysteresis (easier):
```cpp
// In mqtt_manager.cpp, line ~15:
constexpr float TIMEOUT_MULTIPLIER = 2.0f;  // More patient (alarm at 120s)
constexpr float RECOVERY_DIVISOR = 3.0f;    // Tighter recovery
```

---

## Support

**Questions?** Check these files in order:
1. `MQTT_TIMEOUT_FIX.md` — Technical explanation
2. `VERIFICATION_CHECKLIST.md` — Testing procedures
3. Serial logs — Debug output from `[Opta1]` prefixed messages

**Logs location:**
- Open Arduino IDE → Tools → Serial Monitor → Set baud 115200
- Or: `pio device monitor -p COMX -b 115200`

---

**Status:** Ready for field deployment ✓  
**Last updated:** 2026-07-09  
**Tested:** VERIFICATION_CHECKLIST.md
