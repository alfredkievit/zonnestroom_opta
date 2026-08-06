# Anker Smart Meter Gen 2 via Node-RED

Deze implementatie start bewust veilig: eerst Modbus TCP uitlezen en valideren,
pas daarna bestaande energiemeter-topics voor Opta1 emuleren of vervangen.

## Status

- Doelmeter: Anker SOLIX Smart Meter Gen 2 driefase (`AE1X0311`)
- Verwacht Modbus TCP endpoint: `192.168.0.188:502`
- Gebruikershandleiding bevestigt het apparaat en interfaces, maar bevat geen
  publieke registermap of datatype-tabel.
- Daarom begint deze repo met een probe-flow voor registerdiscovery en ruwe
  uitlezing.

## Bestanden

- `docs/nodered/anker_smartmeter_gen2_modbus_probe.json`
- `docs/nodered/ha_solix_status_poll.json`

## Fase 1: Home Assistant entity -> MQTT brug

Omdat directe Modbus-verwerking op de Opta eerder onpraktisch bleek, start de
eerste implementatiestap via Home Assistant en Node-RED.

Bron-entiteiten:

- `sensor.anker_solix_smart_meter_gen_2_080_hoofd_ct_fase_1_actief_vermogen`
- `sensor.anker_solix_smart_meter_gen_2_080_hoofd_ct_fase_2_actief_vermogen`
- `sensor.anker_solix_smart_meter_gen_2_080_hoofd_ct_fase_3_actief_vermogen`
- `sensor.anker_solix_smart_meter_gen_2_080_totale_actieve_vermogen_van_de_hoofd_ct`
- `sensor.anker_solix_smart_meter_gen_2_080_hoofd_ct_fase_1_stroom`
- `sensor.anker_solix_smart_meter_gen_2_080_hoofd_ct_fase_2_stroom`
- `sensor.anker_solix_smart_meter_gen_2_080_hoofd_ct_fase_3_stroom`
- `sensor.anker_solix_solarbank_max_ac_124_laadvermogen_van_de_batterij`
- `sensor.anker_solix_solarbank_max_ac_124_ontlaadvermogen_van_de_batterij`

Node-RED flowbestand:

- `docs/nodered/ha_solix_status_poll.json`

Doel-topic op broker `192.168.0.10`:

- `homeassistant/Solix_Smartmeter/status`

Tekenconventie:

- export naar net = negatief
- import van net = positief

Payload bevat minimaal:

- `ts` en `ts_ms`
- `valid`
- `missing`
- `values.fase_1_w`
- `values.fase_2_w`
- `values.fase_3_w`
- `values.totaal_w`
- `values.fase_1_a`
- `values.fase_2_a`
- `values.fase_3_a`
- `values.battery_charge_w`
- `values.battery_discharge_w`
- `derived.export_l1_w`
- `derived.import_l1_w`
- `derived.export_total_w`
- `derived.import_total_w`
- `derived.battery_net_discharge_w`
- `derived.battery_net_charge_w`

De flow publiceert elke 5 seconden en gebruikt retained MQTT, zodat downstream
consumers direct de laatste geldige status zien.

## Deploy op Home Assistant OS

Het flowbestand is geplaatst op Home Assistant in:

- `/config/nodered_imports/ha_solix_status_poll.json`

Importeer dit bestand in de Node-RED add-on via de editor. De flow verwacht dat
de bestaande config nodes al aanwezig zijn:

- Home Assistant server node: `10e11fca.f917f`
- MQTT broker node `192.168.0.10`: `ceeee23fff5f978b`

## Voorwaarden

Installeer in Node-RED:

- `node-red-contrib-modbus`

## Wat de flow doet

De flow heeft twee delen:

1. `Probe raw block FC4` en `Probe raw block FC3`
   Leest handmatig een registerblok uit en publiceert ruwe registers naar MQTT
   en de debug sidebar.
2. `Poll mapped metrics`
   Leest een kleine set gemapte waarden uit. De offsets zijn placeholders en
   moeten na discovery worden aangepast.

## MQTT topics uit de flow

Ruw:

- `anker_smartmeter_gen2/raw/fc4_block`
- `anker_smartmeter_gen2/raw/fc3_block`

Gemapt:

- `anker_smartmeter_gen2/p_l1_w`
- `anker_smartmeter_gen2/p_l2_w`
- `anker_smartmeter_gen2/p_l3_w`
- `anker_smartmeter_gen2/p_total_w`
- `anker_smartmeter_gen2/status`

## Eerste gebruik

1. Importeer de flow in Node-RED.
2. Controleer de Modbus client:
   - host `192.168.0.188`
   - port `502`
   - unit id `1`
3. Klik eerst op `Probe raw block FC4`.
4. Als dat geen geldige data geeft, probeer `Probe raw block FC3`.
5. Bekijk in de debug sidebar welk registerblok stabiele en veranderende waarden
   oplevert.
6. Pas daarna in de function `build mapped metric requests` de offsets,
   `quantity`, `fc`, `datatype` en `scale` aan.

## Verwachte vervolgstap na discovery

Zodra de juiste registermap bekend is, kan Node-RED dezelfde MQTT-topics gaan
publiceren die Opta1 nu al verwacht:

- `b0b21c913c34/PUB/CH1`
- `b0b21c913c34/PUB/CH10`
- `b0b21c913c34/PUB/CH13`
- `b0b21c913c34/PUB/CH14`

Daarmee kan Opta1 ongewijzigd blijven werken terwijl de bron fysiek Modbus TCP
wordt in plaats van de oude MQTT-meterfeed.

## Let op

- Adressen in de flow zijn nog niet definitief; ze zijn een template.
- Gebruik eerst alleen de raw probe tot de waarden zeker kloppen.
- Pas pas daarna Home Assistant entiteiten en Opta1-topicemulatie toe.