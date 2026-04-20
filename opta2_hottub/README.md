# Opta2 Hottub + Beregening

Beschrijving van de fysieke en logische I/O van de Opta2-controller. Opta2 stuurt zowel de hottub als de beregening via een AFX00005 digitale expansiemodule.

## Functie

Opta2 verzorgt:

- lokale hottub temperatuurregeling
- circulatiepompregeling van de hottub
- niveaupompregeling van de hottub
- lokale alarmen en fail-safe gedrag
- watchdog op permissie en heartbeat van Opta1 voor de hottub
- zelfstandige beregeningsregeling voor 6 zones en 1 bronpomp
- Home Assistant bediening voor hottub en beregening

## Netwerk

Opta2 gebruikt twee netwerkpaden:

- LAN is primair en gebruikt een vast IP-adres: `192.168.0.51`
- WiFi blijft beschikbaar als fallback
- bij verlies van de LAN-link valt Opta2 na korte debounce direct terug op WiFi
- zodra LAN fysiek terug beschikbaar is, schakelt Opta2 automatisch terug naar LAN
- als LAN fysiek aanwezig is maar MQTT over LAN onbruikbaar blijft, houdt Opta2 WiFi maximaal 1 uur actief en opent daarna een LAN-herstelvenster; lukt dat herstel niet binnen 15 seconden, dan volgt een software reset

De MQTT-broker blijft `192.168.0.10:1883`.

## Netwerk failover test (LAN prioriteit)

Deze test verifieert volledig dat Opta2:

- op LAN werkt zolang de kabel aanwezig is
- naar WiFi terugvalt bij kabelverlies
- automatisch terug naar LAN schakelt zodra de kabel terug is

### Voorbereiding

1. Flash de laatste firmware op Opta2.
2. Zorg dat broker `192.168.0.10:1883` bereikbaar is.
3. Start in de repo-root:

```bash
node test_opta2_network_failover.js
```

### Uitvoering

1. Start met netwerkkabel ingeplugd in Opta2.
2. Wacht op `transport=lan` in de testoutput.
3. Trek de kabel uit de Opta2 en wacht op `transport=wifi`.
4. Steek de kabel opnieuw in en controleer dat `transport=lan` terugkomt.

### Verwachte uitkomst

- De test eindigt met `PASS - LAN recovered and regained priority.`
- Tijdens de test blijft `opta2/device/heartbeat` periodiek binnenkomen.

### Opmerking

De firmware forceert LAN als voorkeursroute. Zodra fysieke link + IP op LAN terug beschikbaar zijn, schakelt MQTT direct terug naar LAN en wordt WiFi losgekoppeld.

## Basis-uitgangen op de Opta

| Fysieke uitgang | Code pin | Huidige functie | Status |
|---|---|---|---|
| O1 | D0 | Hottub verwarming | Actief gebruikt |
| O2 | D1 | Hottub circulatiepomp | Actief gebruikt |
| O3 | D2 | Hottub niveaupomp | Actief gebruikt |
| O4 | D3 | Alarm uitgang | Actief gebruikt |

## Basis-ingangen op de Opta

De standaard Opta heeft 8 ingangskanalen I1 t/m I8. In deze applicatie worden ze als volgt gebruikt of gereserveerd.

| Fysieke ingang | Code pin | Type in huidige code | Huidige functie | Status |
|---|---|---|---|---|
| I1 | A0 | Analoog 0-10V | Hottub temperatuursensor | Actief gebruikt |
| I2 | A1 | Digitaal | Hoog niveau schakelaar | Actief gebruikt |
| I3 | A2 | Digitaal | Externe fout | Actief gebruikt |
| I4 | A3 | Digitaal | Flow OK | Actief gebruikt |
| I5 | A4 | Vrij | Niet toegewezen | Reserve |
| I6 | A5 | Vrij | Niet toegewezen | Reserve |
| I7 | A6 | Vrij | Niet toegewezen | Reserve |
| I8 | A7 | Vrij | Niet toegewezen | Reserve |

## Expansiemodule AFX00005 – gebruikte uitgangen

De beregening gebruikt de eerste digitale expansie op Opta2. De firmware behandelt deze als een digitale uitbreiding met 8 uitgangen. In de huidige indeling zijn 7 uitgangen toegewezen.

| Expansie-uitgang | Index in code | Huidige functie | Status |
|---|---|---|---|
| DO1 | 0 | Beregening zone 1 klep | Actief gebruikt |
| DO2 | 1 | Beregening zone 2 klep | Actief gebruikt |
| DO3 | 2 | Beregening zone 3 klep | Actief gebruikt |
| DO4 | 3 | Beregening zone 4 klep | Actief gebruikt |
| DO5 | 4 | Beregening zone 5 klep | Actief gebruikt |
| DO6 | 5 | Beregening zone 6 klep | Actief gebruikt |
| DO7 | 6 | Bronpomp relaiscontact | Actief gebruikt |
| DO8 | 7 | Reserve | Momenteel niet gebruikt |

## Expansiemodule AFX00005 – ingangen

De digitale expansie biedt ook ingangen, maar in fase 1 van de beregening worden die nog niet gebruikt.

| Expansie-ingang | Huidige functie | Status |
|---|---|---|
| DI1 t/m DI16 | Niet toegewezen | Reserve voor latere veldsignalen |

## Bedrijfslogica hottub

Opta2 mag de hottub alleen verwarmen als:

- permissie van Opta1 aanwezig is
- heartbeat van Opta1 geldig blijft
- lokale alarmen dit toelaten

Bij communicatieverlies of fout gaat de hottubregeling naar veilige toestand.

## Bedrijfslogica beregening

De beregening is bewust energie-onafhankelijk en draait volledig lokaal op Opta2.

- maximaal 2 zones mogen tegelijk actief zijn
- bij een 3e zone-aanvraag geldt FIFO: de oudste actieve zone valt af
- de bronpomp start automatisch zodra minimaal 1 zone actief is
- handmatige pompbediening blijft beschikbaar via Home Assistant
- tijdschema's zijn nog niet geïmplementeerd, maar de firmwarestructuur is erop voorbereid

## Belastingen

- 24V kleppen worden rechtstreeks via de droge contacten van de expansiemodule geschakeld
- de 230V bronpomp wordt gestart via een extern relais of contactor dat door een droog contact van de expansiemodule wordt aangestuurd

## Logische I/O via MQTT

Opta2 gebruikt naast fysieke I/O ook logische I/O via MQTT:

| Logische I/O | Topic / bron | Functie |
|---|---|---|
| Hottub permissie | `opta1/device/permission_hottub` | Surplus-permissie van Opta1 |
| Heartbeat Opta1 | `opta1/device/heartbeat` | Communicatiewatchdog voor hottub |
| Kloksync | `opta2/device/current_minute_of_day` | Minuut-van-de-dag vanuit Home Assistant |
| Beregening enable | `opta2/cmd/enable_irrigation` | Globale beregening aan/uit |
| Beregeningszones | `opta2/cmd/irrigation_zone_X_request` | Handmatige zone-aanvragen vanuit Home Assistant |
| Beregening pomp handmatig | `opta2/cmd/manual_force_irrigation_pump` | Servicebediening voor bronpomp |

Deze README benoemt hiermee alle huidige fysieke en logische I/O die in de firmware van Opta2 zijn vastgelegd.