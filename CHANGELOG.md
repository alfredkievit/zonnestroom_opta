# Changelog

All notable changes to this project will be documented in this file.

The format is inspired by Keep a Changelog and this project follows date-based entries until semantic versioning is introduced.

## [2026-03-26]

### Added
- Home Assistant package en dashboardbestanden voor de Opta-installatie.
- Klokgestuurde circulatiepomp op Opta2 met twee instelbare startmomenten per dag.
- Instelbare automatische circulatiepomp-duur in minuten.
- Handmatige circulatiepompbediening op Opta2.
- Handmatige niveaupompbediening op Opta2.
- MQTT-klokfeed vanuit Home Assistant naar Opta2.
- Extra Home Assistant entities voor Opta2 klokstatus en pompplanning.

### Changed
- Opta1 MQTT-validiteitsdetectie herschreven naar wildcard keepalive op energiemetertopics.
- MQTT-timeout voor energiemeterbewaking verhoogd naar 30 seconden.
- Opta1 prioriteitslogica aangepast zodat handmatige overrides voor WP-contacten en hottub-permissie door alle relevante states heen blijven werken.
- Opta1 interlocks aangepast zodat handmatige overrides geselecteerde outputs niet onbedoeld blokkeren.
- Opta2 hottublogica aangepast zodat handmatige hottubstart de verwarming plus circulatiepomp laat lopen tot setpoint.
- Opta2 circulatiepomp-logica aangepast zodat de pomp altijd meeloopt met verwarming.
- Dashboardteksten en bediening voor hottub, circulatiepomp en niveaupomp verduidelijkt.
- Dashboardkaart voor hottubbediening vereenvoudigd.

### Fixed
- IntelliSense-fouten in `opta1_master` door ontbrekende `stdint` include en verkeerde compile commands verwijzing.
- Flapping van MQTT-validiteit in Home Assistant door te korte timeout.
- Opta2 offline-indicatie in Home Assistant door verkeerde templatebron.
- LED-mirroring op beide Opta's door het verwijderen van foutieve `defined(LED_Dx)` guards.
- Handmatige WP extra warm water die door interlocks werd geblokkeerd.
- Handmatige hottubstart die wel permissie gaf maar geen lokale warmtevraag op Opta2 startte.

### Verified
- `opta1_master` build en upload succesvol.
- `opta2_hottub` build en upload succesvol.
- Home Assistant configuratiecontrole succesvol na package- en dashboardupdates.

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
