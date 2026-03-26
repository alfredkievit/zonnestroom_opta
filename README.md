# Zonnestroom_Opta

Lokale energiegestuurde regeling met 2x Arduino Opta PLC en Home Assistant als bedieningslaag.

- Opta 1 (`opta1_master`): energy master (boiler + prioriteitsregeling + permissie hottub)
- Opta 2 (`opta2_hottub`): lokale hottubregeling (verwarming, circulatiepomp, niveaupomp)
- Home Assistant: visualisatie, instellingen en handbediening via MQTT
- Fail-safe: geen cloud-afhankelijkheid voor basiswerking

## Huidige functionaliteit

Het systeem ondersteunt op dit moment:

- MQTT ingest van de energiemeter op Opta1
- Surplus-berekening op fase 1 en totaal
- Boiler-prioriteitsregeling op Opta1:
  - warmtepomp extra warm water
  - boiler-element
  - hottub-permissie
- Onafhankelijke handbediening van WP-contacten en hottub-permissie
- Lokale hottubregeling op Opta2:
  - verwarming tot setpoint
  - circulatiepomp gekoppeld aan verwarming
  - handmatige circulatiepomp
  - klokgestuurde circulatiepomp-runs 2x per dag
  - niveaupomp automatisch op hoog niveau of handmatig
- Heartbeat/watchdog tussen Opta1 en Opta2
- Persistente settingsopslag in flash (KVStore/TDBStore)
- Home Assistant MQTT package en Lovelace-dashboard

## Architectuur

## Opta 1: `opta1_master`

Verantwoordelijkheden:

- Inlezen energiemeter MQTT
- Berekenen surplus:
  - Fase 1: `SURPLUS_FASE1 = CH1.P - CH10.P`
  - Totaal: `SURPLUS_TOTAAL = CH13.P - CH14.P`
- Boilerregeling met prioriteit:
  1. Warmtepomp (WP)
  2. Elektrisch element
  3. Hottub permissie
- Harde interlocks met handmatige uitzonderingen waar gewenst
- Heartbeat + permissie publicatie naar Opta 2
- Status/alarms publicatie naar Home Assistant
- Ontvangst van instellingen en handcommando's vanuit Home Assistant

Belangrijk:

- WP + element zitten op fase 1 en mogen nooit tegelijk aan
- Hottub gebruikt totaaloverschot als besluitbasis

## Opta 2: `opta2_hottub`

Verantwoordelijkheden:

- Lokale hottubtemperatuurregeling
- Circulatiepompregeling:
  - mee aan met verwarming
  - handmatig schakelbaar
  - automatisch 2x per dag via kloksetpoints
- Lokale niveaupompregeling:
  - automatisch bij hoog water
  - handmatig schakelbaar
- Lokale alarmen (overtemp, level high, sensor fault)
- Watchdog op master-heartbeat/permissie
- Veilige toestand bij communicatiefout
- MQTT-klokfeed vanuit Home Assistant voor geplande pompstarts

## I/O overzicht

### Opta1 uitgangen

- `D0`: WP extra warm water
- `D1`: WP comfort / extra warmte
- `D2`: boiler-element
- `D3`: reserve

### Opta2 uitgangen

- `D0`: hottub verwarming
- `D1`: circulatiepomp
- `D2`: niveaupomp
- `D3`: alarm

### Opta2 ingangen

- `A0` / fysieke `I1`: hottub temperatuursensor 0-10V
- `A1` / fysieke `I2`: niveau hoog
- `A2` / fysieke `I3`: externe fout
- `A3` / fysieke `I4`: flow OK

## Mappenstructuur

```text
Zonnestroom_Opta/
├── opta1_master/
│   ├── platformio.ini
│   └── src/
│       ├── main.cpp
│       ├── types.h
│       ├── config.h
│       ├── analog_input.h/.cpp
│       ├── mqtt_manager.h/.cpp
│       ├── boiler_logic.h/.cpp
│       ├── priority_manager.h/.cpp
│       ├── interlocks.h/.cpp
│       ├── ha_interface.h/.cpp
│       └── settings_storage.h/.cpp
└── opta2_hottub/
    ├── platformio.ini
    └── src/
        ├── main.cpp
        ├── types.h
        ├── config.h
        ├── analog_input.h/.cpp
        ├── comm_manager.h/.cpp
        ├── hottub_logic.h/.cpp
        ├── ha_interface.h/.cpp
        └── settings_storage.h/.cpp
```

## Home Assistant

De map [homeassistant](c:/Users/alfre/VSC%20projects/Zonnestroom_Opta/homeassistant) bevat:

- MQTT package met sensoren, switches, numbers en automations
- Lovelace dashboard voor Opta1 en Opta2
- MQTT-klokpublicatie voor de geplande circulatiepompruns

Belangrijke bediening in Home Assistant:

- Verwarming bij surplus toestaan
- Hottub meenemen in automaat
- Hottub handmatig aan tot setpoint
- Circulatiepomp automatisch
- Circulatiepomp handmatig
- Niveaupomp automatisch toestaan
- Niveaupomp handmatig

## Build

PlatformIO wordt gebruikt met:

- `platform = ststm32`
- `board = opta`
- `framework = arduino`

### Opta 1 build

```bash
cd opta1_master
~/.platformio/penv/Scripts/pio.exe run
```

### Opta 2 build

```bash
cd opta2_hottub
~/.platformio/penv/Scripts/pio.exe run
```

## MQTT energiemeter

Broker:

- `192.168.0.10`

Topics:

- `b0b21c913c34/PUB/CH1`  (fase 1 export)
- `b0b21c913c34/PUB/CH10` (fase 1 import)
- `b0b21c913c34/PUB/CH13` (totaal export)
- `b0b21c913c34/PUB/CH14` (totaal import)

Payload:

- JSON met veld `"P"` (string), altijd positief

Voorbeeld:

```json
{"ident":"b0b21c913c34","CHname":"fase 1 export","P":"1234"}
```

## Belangrijke MQTT topics

### Opta1

- `opta1/cmd/enable_hottub`
- `opta1/cmd/manual_force_hottub`
- `opta1/status/hottub_permission`

### Opta2

- `opta2/cmd/enable_hottub`
- `opta2/cmd/enable_auto_pump`
- `opta2/cmd/manual_force_pump`
- `opta2/cmd/manual_force_level_pump`
- `opta2/cmd/sp_pump_run1_hour`
- `opta2/cmd/sp_pump_run1_minute`
- `opta2/cmd/sp_pump_run2_hour`
- `opta2/cmd/sp_pump_run2_minute`
- `opta2/cmd/sp_pump_run_duration_min`
- `opta2/device/current_minute_of_day`

## Veiligheidslogica

Belangrijkste fail-safe regels:

- Bij `surplus <= 0` schakelen energielasten direct uit
- Bij MQTT timeout schakelen energielasten direct uit
- Bij sensorfout boiler: WP + element uit
- Bij Opta1/Opta2 communicatiefout: hottub permissie direct uit
- WP en boiler-element mogen nooit tegelijk actief zijn (harde interlock)
- Hottub overtemperatuur schakelt verwarming direct uit
- Handmatige overrides zijn bewust toegestaan voor geselecteerde functies

## Home Assistant integratie

Geïmplementeerd als MQTT interface:

- Status en alarmen publish
- Settings en commando's subscribe
- Handbediening via `manual_force_*` op Opta 1 en Opta 2
- Dashboard met actuele status en setpoints
- Klokautomatisering voor de geplande circulatiepompruns

## Belangrijke opmerkingen

- Drempels, hysterese en pompstarts zijn bedoeld om inbedrijfstelling op locatie te ondersteunen.
- Pin mapping en elektrische aansluiting altijd valideren op de echte installatie.
- Test eerst met lage risico-setup (simulatie van MQTT waarden, zonder vermogenslasten aangesloten).
- Home Assistant is momenteel de klokbron voor de geplande circulatiepompruns.

## Changelog

Zie [CHANGELOG.md](c:/Users/alfre/VSC%20projects/Zonnestroom_Opta/CHANGELOG.md) voor de wijzigingshistoriek.

## Zie ook

- [IMPLEMENTATIEPLAN.md](IMPLEMENTATIEPLAN.md)
