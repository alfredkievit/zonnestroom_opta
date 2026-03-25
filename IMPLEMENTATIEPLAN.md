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

## 8. Commissioning Phase 1-2 Resultaten (2026-03-25)

**MQTT Connectivity**: ✓ VERIFIED
- MQTT broker operationeel op 192.168.0.10:1883
- Opta1 & Opta2 beide live en communicerend
- Energy meter publiceert live CH1/CH10/CH13/CH14 data
- Heartbeat mechanism werkt (Opta1→Opta2 coupling actief)

**MQTT Command Routing**: ✓ FIXED
- Issue gevonden: Enable flags werden niet verwerkt
- Root cause: mqtt_manager forwarde commands niet naar ha_interface
- Fix: Added pointer wiring tussen mqtt_manager en ha_interface
- Firmware updated 2026-03-25 COM12

**State Machine Debugging**: ⏳ IN PROGRESS
- WP nog niet activerend ondanks sufficient surplus
- Mogelijk: Settings storage, enable flags, of sensor fault
- **REQUIRES**: Offline testing zonder legacy system actief!

## 9. KRITIEKE VEILIGHEID NOTITIE

⚠️ **GEVAAR**: Test scripts mogen NIET naar echte meter topics publiceert terwijl legacy systeem actief!
- Kan ongewild WP + element gelijktijdig activeren
- Gebruikt enerzijds echte energiemeter data

**VEILGE Test Procedure**:
1. Gebruik alleen `test_safe_connectivity.js` (heartbeat monitoring, geen injecties)
2. Voor state machine testen: legacy systeem offline zetten EERST
3. Pas daarna volledig commissioning test starten

## 10. Open punten voor inbedrijfstelling

1. **PRIORITEIT 1**: Offline het WP activation probleem debuggen
   - Drempelwaarden in het veld tunen
   - Enable flags in KVStore verificatie
   - Serial logging toevoegen voor diagnostiek

2. **PRIORITEIT 2**: Safety & Verification
   - Definitieve pin mapping valideren tegen kast/wiring
   - Alarm reset-flow volledig valideren
   - Fail-safe interlocks stress-testen

3. **PRIORITEIT 3**: Integration & Tuning
   - HA dashboard finaliseren
   - Startvertragingen op stabiliteit finetunen
   - Runtime logging & diagnostische topics

## 11. Aanbevolen vervolgstappen (Offline Phase)

1. Legacy systeem offline zetten
2. Opta1/Opta2 state machine debugging met Serial logging
3. Surplus injection tests (now safe)
4. HA integration & dashboard verificatie
5. Full system acceptance test met werkende warmwater voorraad
