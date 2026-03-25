# Implementatieplan

Dit document beschrijft het plan en de huidige implementatiestatus van de energiegestuurde regeling met 2x Arduino Opta PLC.

## 1. Doel

Volledig lokale regeling zonder cloud-afhankelijkheid.

- Opta 1: centrale energieregeling
- Opta 2: autonome hottubregeling
- Home Assistant: UI en instellingen, niet kritisch voor basiswerking

## 2. Regelfilosofie

Vaste prioriteit:

1. Boiler via warmtepomp
2. Boiler via elektrisch element
3. Hottub als restlast

Belangrijke nuance uit de praktijk:

- WP + element zitten op fase 1
- Hottub zit op andere fase
- Daarom:
  - WP/element besluiten op fase-1 surplus
  - Hottub besluit op totaal surplus

## 3. Energie-input en surplus

MQTT meter topics:

- `CH1`: fase 1 export
- `CH10`: fase 1 import
- `CH13`: totaal export
- `CH14`: totaal import

Berekeningen:

- `SURPLUS_FASE1_W = CH1.P - CH10.P`
- `SURPLUS_TOTAAL_W = CH13.P - CH14.P`

## 4. Opta 1 (Energy Master)

Geïmplementeerde modules:

- `analog_input`: PT1000 0-10V conversie
- `mqtt_manager`: meter ingest + timeout
- `boiler_logic`: requestbepaling WP/element/hottub
- `priority_manager`: state machine + startvertragingen
- `interlocks`: harde veiligheid als laatste stap
- `ha_interface`: status publish + command handling
- `settings_storage`: flash persistente settings

State machine:

- `IDLE (0)`
- `WP_BOILER (1)`
- `BOILER_ELEMENT (2)`
- `HOTTUB (3)`
- `FAULT (99)`

## 5. Opta 2 (Hottub Controller)

Geïmplementeerde modules:

- `analog_input`: PT1000 0-10V conversie
- `comm_manager`: permissie + heartbeat watchdog
- `hottub_logic`: heater/pomp en level-pump logica
- `ha_interface`: status en alarmen
- `settings_storage`: flash persistente settings

Lokale veiligheidsregels:

- Stop heater direct bij verlies permissie/communicatie
- Stop heater bij overtemp of level-high
- Niveaupomp timeout-alarm bij te lange looptijd

## 6. Interlocks en fail-safe

Topregels (boven alle logica):

1. WP en element nooit tegelijk actief
2. Geen geldig overschot => lasten uit
3. MQTT invalid => lasten uit
4. Sensorfout boiler => WP/element uit
5. Comm-fout naar Opta2 => hottub permissie uit

## 7. Build- en teststatus

Huidige status:

- Beide projecten bouwen succesvol met PlatformIO
- Platform geverifieerd:
  - `platform = ststm32`
  - `board = opta`

Resultaat laatste build:

- `opta1_master`: SUCCESS
- `opta2_hottub`: SUCCESS

Uitvoeringsstatus (hardware):

- `2026-03-25`: Opta1 firmware succesvol geupload via `COM12` (DFU upload SUCCESS)
- Seriele verbinding op `COM12` gedetecteerd met Arduino VID/PID (`2341:0064`)
- `2026-03-25`: Opta2 (hottub) firmware succesvol geupload via `COM6` (DFU upload SUCCESS)
- Seriele verbinding op `COM6` gedetecteerd met Arduino VID/PID (`2341:0164`)
- Runtime logoutput is momenteel minimaal (geen uitgebreide `Serial.println` diagnostiek actief)

## 8. Open punten voor inbedrijfstelling

1. Drempelwaarden in het veld tunen:
   - `SP_SURPLUS_WP_START_W`
   - `SP_SURPLUS_ELEMENT_START_W`
   - `SP_SURPLUS_HOTTUB_START_W`
2. Startvertragingen finetunen op stabiliteit
3. Definitieve pin mapping valideren tegen kast/wiring
4. HA entiteiten finaliseren (namen, retain beleid, dashboard)
5. End-to-end test met echte vermogenslasten en beveiligingen
6. Eventueel extra runtime logging toevoegen voor commissioning (MQTT valid, priority state, output flags)

## 9. Aanbevolen vervolgstappen

1. Hardware-in-the-loop testscenario maken
2. Alarm reset-flow op beide Opta's volledig valideren
3. Integratietestscript voor MQTT input sequenties toevoegen
4. Eventueel loggingniveau en diagnostische topics uitbreiden
