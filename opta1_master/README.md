# Opta1 Master

Beschrijving van de fysieke in- en uitgangen van de Opta1 energie-master.

## Functie

Opta1 verzorgt:

- surplusberekening uit de energiemeter via MQTT
- boilerregeling voor warmtepomp en element
- permissie voor de hottub naar Opta2
- status- en alarmpublicatie naar Home Assistant

## Uitgangen

| PLC uitgang | Code pin | Functie | Opmerking |
|---|---|---|---|
| O1 | D0 | Warmtepomp extra warm water | Actieve WP-uitgang voor boilerlading |
| O2 | D1 | WP comfort / extra verwarming | Gereserveerd / optioneel |
| O3 | D2 | Boiler-element | Relais voor elektrisch element |
| O4 | D3 | Reserve | Momenteel niet gebruikt |

## Analoge ingangen

| PLC ingang | Code pin | Functie | Opmerking |
|---|---|---|---|
| I1 | A0 | Boiler boven sensor PLC | Huidige regelwaarheid, PT1000 met 0-10V omvormer |
| I2 | A1 | Boiler boven sensor extra | Optioneel, momenteel niet gebruikt in de regeling |

## Digitale ingangen

| PLC ingang | Code pin | Functie | Opmerking |
|---|---|---|---|
| I3 | A2 | Boilerthermostaat / beveiliging | Moet OK zijn om element toe te laten |
| I4 | A3 | Fault reset knop | Handmatige reset-ingang |

## Regelwaarheid

De boilerregeling gebruikt momenteel alleen:

- I1 / A0 als Boiler Boven (PLC)

Deze temperatuur wordt gebruikt voor:

- start/stop warmtepomp boilerlading
- start/stop boiler-element
- gauge in Home Assistant
- boiler temperatuurhistoriek

## Monitoring in Home Assistant

Naast de PLC-regeltemperatuur blijven deze warmtepomp-sensoren zichtbaar voor monitoring:

- Warmwater Boven
- Warmwater Laden