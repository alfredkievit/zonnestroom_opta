# Zonnestroom_Opta

Lokale energiegestuurde regeling met 2x Arduino Opta PLC.

- Opta 1 (`opta1_master`): energy master (boiler + prioriteitsregeling + permissie hottub)
- Opta 2 (`opta2_hottub`): lokale hottubregeling (heater/pomp/niveaupomp)
- Home Assistant: visualisatie, instellingen en handbediening via MQTT
- Fail-safe: geen cloud-afhankelijkheid voor basiswerking

## Status

Dit is een eerste werkende implementatiebasis met:

- Projectstructuur voor beide Opta-projecten
- MQTT ingest voor energiemeting
- Surplus-berekening op fase- en totaalniveau
- Prioriteits-state-machine op Opta 1
- Interlocks en fail-safe afschakeling
- Lokale hottublogica op Opta 2
- Heartbeat/watchdog tussen Opta 1 en Opta 2
- Persistente settingsopslag in flash (KVStore/TDBStore)
- Home Assistant MQTT publish/subscribe basis

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
- Harde interlocks
- Heartbeat + permissie publicatie naar Opta 2
- Status/alarms publicatie naar Home Assistant

Belangrijk:

- WP + element zitten op fase 1 en mogen nooit tegelijk aan
- Hottub gebruikt totaaloverschot als besluitbasis

## Opta 2: `opta2_hottub`

Verantwoordelijkheden:

- Lokale hottubtemperatuurregeling
- Lokale niveaupompregeling
- Lokale alarmen (overtemp, level high, sensor fault)
- Watchdog op master-heartbeat/permissie
- Veilige toestand bij communicatiefout

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

## Veiligheidslogica

Belangrijkste fail-safe regels:

- Bij `surplus <= 0` schakelen energielasten direct uit
- Bij MQTT timeout schakelen energielasten direct uit
- Bij sensorfout boiler: WP + element uit
- Bij Opta1/Opta2 communicatiefout: hottub permissie direct uit
- WP en boiler-element mogen nooit tegelijk actief zijn (harde interlock)

## Home Assistant integratie

Geïmplementeerd als MQTT interface:

- Status en alarmen publish
- Settings en commando's subscribe
- Basis handbediening via `manual_force_*` op Opta 1

## Belangrijke opmerkingen

- Dit is een stevige eerste basis; tuning van drempels en delays gebeurt tijdens inbedrijfstelling.
- Pin mapping en elektrische aansluiting altijd valideren op de echte installatie.
- Test eerst met lage risico-setup (simulatie van MQTT waarden, zonder vermogenslasten aangesloten).

## Zie ook

- [IMPLEMENTATIEPLAN.md](IMPLEMENTATIEPLAN.md)
