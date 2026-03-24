# Changelog

All notable changes to this project will be documented in this file.

The format is inspired by Keep a Changelog and this project follows date-based entries until semantic versioning is introduced.

## [2026-03-24]

### Added
- Initial dual-Opta implementation scaffold:
  - `opta1_master` (Energy Master)
  - `opta2_hottub` (Hottub Controller)
- Core modules for Opta1:
  - MQTT ingest and surplus processing
  - Boiler logic and priority manager
  - Interlocks and fail-safe output handling
  - Home Assistant MQTT interface
  - Flash settings persistence
- Core modules for Opta2:
  - Master communication manager with heartbeat watchdog
  - Local hottub logic (heater/pump/level pump)
  - Home Assistant MQTT interface
  - Flash settings persistence
- PT1000 0-10V sensor conversion utilities in both projects.
- Initial repository documentation:
  - `README.md`
  - `IMPLEMENTATIEPLAN.md`

### Changed
- PlatformIO target configuration corrected for Arduino Opta:
  - `platform = ststm32`
  - `board = opta`
- MQTT callback handling refactored to static function callbacks compatible with `ArduinoMqttClient`.
- Float publishing switched from `dtostrf` to `snprintf` for mbed compatibility.
- `connected()` signatures aligned with library non-const API.

### Fixed
- Build errors caused by unsupported lambda callback for `onMessage`.
- Build errors from unavailable `dtostrf` in this framework.
- Build errors from const mismatch on `connected()`.

### Verified
- `opta1_master` compiles successfully with PlatformIO.
- `opta2_hottub` compiles successfully with PlatformIO.

---

## Commit Reference (latest first)
- `3ae73ff` Add project README and implementation plan
- `6f3e2a6` Fix build errors: static MQTT callback, connected() const, snprintf for float
- `ae1b2fe` Initial implementation: Opta1 energy master + Opta2 hottub controller
