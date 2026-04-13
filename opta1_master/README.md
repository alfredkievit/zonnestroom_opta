# Opta1 Master

Beschrijving van de fysieke I/O en de huidige taakverdeling van de Opta1 energie-master.

## Functie

Opta1 verzorgt:

- surplusberekening uit de energiemeter via MQTT
- boilerregeling voor warmtepomp en elektrisch element
- permissie en heartbeat voor de hottub-controller op Opta2
- status- en alarmpublicatie naar Home Assistant

## Uitgangen

| Fysieke uitgang | Code pin | Huidige functie | Status |
|---|---|---|---|
| O1 | D0 | Warmtepomp extra warm water | Actief gebruikt |
| O2 | D1 | WP comfort / extra verwarming | Gereserveerd voor toekomstige uitbreiding |
| O3 | D2 | Boiler-element | Actief gebruikt |
| O4 | D3 | Reserve relais | Momenteel niet aangestuurd |

## Basis-ingangen op de Opta

De standaard Opta heeft 8 ingangskanalen I1 t/m I8. In deze applicatie worden ze als volgt gebruikt of gereserveerd.

| Fysieke ingang | Code pin | Type in huidige code | Huidige functie | Status |
|---|---|---|---|---|
| I1 | A0 | Analoog 0-10V | Boiler temperatuur sensor PLC | Actief gebruikt als regelwaarheid |
| I2 | A1 | Analoog 0-10V | Extra boiler temperatuur sensor | Aangesloten / optioneel, niet bepalend voor de regeling |
| I3 | A2 | Digitaal | Boilerthermostaat / beveiliging | In config benoemd, in huidige installatie niet blokkerend gebruikt |
| I4 | A3 | Digitaal | Fault reset knop | Actief gebruikt |
| I5 | A4 | Vrij | Niet toegewezen | Reserve |
| I6 | A5 | Vrij | Niet toegewezen | Reserve |
| I7 | A6 | Vrij | Niet toegewezen | Reserve |
| I8 | A7 | Vrij | Niet toegewezen | Reserve |

## Regelwaarheid

De boilerregeling gebruikt momenteel alleen I1 / A0 als primaire temperatuurwaarde. Deze temperatuur stuurt:

- start/stop warmtepomp boilerlading
- start/stop boiler-element
- publicatie naar Home Assistant
- historische boilergrafieken

I2 / A1 blijft beschikbaar als extra meetpunt, maar is in de huidige firmware niet de regelwaarheid.

## Logische I/O via MQTT

Naast de fysieke klemmen gebruikt Opta1 ook logische I/O via MQTT:

| Logische I/O | Topic / bron | Functie |
|---|---|---|
| Surplus fase 1 | `b0b21c913c34/PUB/CH1` en `CH10` | Beslissing warmtepomp en element |
| Surplus totaal | `b0b21c913c34/PUB/CH13` en `CH14` | Beslissing hottub-permissie |
| Compressor frequentie | `opta1/extern/compressor_freq_hz` | Veiligheidsinterlock voor element |
| Hottub permissie | `opta1/device/permission_hottub` | Logische uitgang naar Opta2 |
| Heartbeat | `opta1/device/heartbeat` | Bewaking communicatie met Opta2 |

## Home Assistant

Opta1 publiceert boilerstatus, prioriteit, permissies, alarmen en surpluswaarden naar Home Assistant. Deze README benoemt hiermee alle huidige fysieke en logische I/O die in de firmware zijn vastgelegd.