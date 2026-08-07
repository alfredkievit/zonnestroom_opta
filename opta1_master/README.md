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

`readPT1000()` (`analog_input.cpp`) filtert sinds 2026-07-12 instantane ADC-sprongen die fysiek niet mogelijk zijn (bv. EMI van het element-relais): een sprong groter dan `SENSOR_MAX_RATE_C_PER_SEC` wordt genegeerd en de laatst geldige waarde blijft aangehouden. Pas als de afwijking `SENSOR_GLITCH_FAULT_MS` aanhoudt, wordt dit als echte `boilerSensorFault` gemeld. Dit voorkomt dat kortstondige sensor-ruis het boiler-element laat pendelen, zonder de reactietijd op een echte sensorstoring te vertragen.

## Logische I/O via MQTT

Naast de fysieke klemmen gebruikt Opta1 ook logische I/O via MQTT:

| Logische I/O | Topic / bron | Functie |
|---|---|---|
| Solix status | `homeassistant/Solix_Smartmeter/status` | Enige surplusbron via HA/Node-RED |
| Compressor frequentie | `opta1/extern/compressor_freq_hz` | Veiligheidsinterlock voor element |
| Hottub permissie | `opta1/device/permission_hottub` | Logische uitgang naar Opta2 |
| Heartbeat | `opta1/device/heartbeat` | Bewaking communicatie met Opta2 |

## Solix bronselectie

`homeassistant/Solix_Smartmeter/status` is de enige surplusbron van Opta1.
Er is geen fallback meer naar de oude meter-topics. Daarbij geldt:

- `surplusFase1W` wordt intern berekend uit de Solix fase-1 export/import
- batterij-ontlading wordt van fase 1 afgetrokken voordat WP/element beslissen
- `surplusTotaalW` komt uit de Solix totaal export/import velden
- als de Solix topic stale wordt (MQTT-timeout hysteresis), gaan WP/element/hottub
	naar de veilige uit-stand — er wordt niet meer teruggevallen op een andere meter

Deze bronselectie filtert schijn-overschot door batterij-ontlading op fase 1
weg, en houdt de firmware af van een tweede, hoogfrequente MQTT-feed die eerder
tot een vastgelopen Opta1 leidde (zie "Waarom geen fallback meer" hieronder).

## Verwerkingsvolgorde

De volgorde in runtime is nu:

1. Home Assistant publiceert de Solix status via Node-RED.
2. Opta1 ontvangt de Solix status op `homeassistant/Solix_Smartmeter/status`.
3. Opta1 berekent het fase-1 surplus uit export/import en corrigeert dit met
	batterij-ontlading.
4. De bestaande surplus-drempels uit Home Assistant blijven de beslisgrens.
5. De boilerlogica beslist WP, element en hottub-permissie.
6. Bij een Solix-timeout gaan alle energielasten uit (interlock `mqttValid`);
	er is geen alternatieve meter meer die overneemt.

## Waarom geen fallback meer naar `b0b21c913c34/PUB/*`

De oude energiemeter blijft fysiek aangesloten en publiceert nog (CH6
warmtepomp-verbruik en boiler-elementverbruik zijn nog steeds nuttig — zie de
hoofdrepo-README onder "Alternatieve meterintegratie"), maar Opta1 subscribet
er niet meer op. Reden: elk bericht op die feed werd altijd met een
heap-allocatie geparsed, ook als het resultaat meteen werd weggegooid omdat
Solix al vers was. Na een snellere publicatiecadans op die meter liep Opta1
vast en stopte met publiceren op zijn device/status-topics — zonder
zelfherstel, omdat de software-only loop-stall-detectie in `main.cpp` alleen
werkt als `loop()` uiteindelijk teruggeeft. Volledige ontkoppeling verkleint
de aanvalsoppervlakte (subscripties, JSON-parsing, heap-gebruik) het meest.

## Hardware watchdog

Naast bovenstaande ontkoppeling heeft Opta1 nu ook een echte hardware
watchdog (`mbed::Watchdog`, IWDG-peripheral), gestart in `setup()` met een
timeout van `WATCHDOG_TIMEOUT_MS` (15s, zie `config.h`) en gekickt aan het
begin van elke `loop()`-iteratie.

Dit is een ander vangnet dan de bestaande software-detectie
(`gConsecutiveLoopStalls` / `LOOP_RESET_MS` in `main.cpp`), die alleen werkt
als `loop()` uiteindelijk teruggeeft. Bij een echte hang — een blocking
WiFi/MQTT-call die nooit terugkeert, of iets dat we nog niet kennen — komt
`loop()` nooit meer terug en wordt de software-teller dus nooit bereikt. De
hardware watchdog reset de chip dan alsnog zelfstandig binnen 15s, zonder dat
er fysiek ingegrepen hoeft te worden. De timeout is ruim boven de bekende
langst-mogelijke legitieme loop-duur gekozen (MQTT connect-timeout = 10s)
zodat een trage maar normale reconnect geen onterechte reset veroorzaakt.

## Home Assistant

Opta1 publiceert boilerstatus, prioriteit, permissies, alarmen en surpluswaarden naar Home Assistant. Deze README benoemt hiermee alle huidige fysieke en logische I/O die in de firmware zijn vastgelegd.

### Instellingen: flash is autoritatief

De `opta1/cmd/*`-topics (setpoints, hysterese, enable-schakelaars) zijn retained en dienen zowel als commandotopic vanuit HA als statustopic voor de schuiven (`command_topic == state_topic`). Bij elke MQTT-(re)connect speelt de broker automatisch de laatst retained waarde af. Om te voorkomen dat een verouderde retained waarde de zojuist uit flash geladen instellingen overschrijft (bv. na een reflash), publiceert Opta1 direct na elke (re)connect zijn eigen huidige instellingen terug naar diezelfde topics (`HaInterface::publishSettingsSnapshot()`, retained). Flash is dus altijd leidend; de broker en de HA-schuiven volgen automatisch.