# HA Dashboard – Installatiehandleiding

## Wat is al klaar

De bestanden staan klaar in je workspace:

| Bestand | Doel |
|---|---|
| `homeassistant/packages/zonnestroom.yaml` | Alle MQTT entities (sensors, switches, numbers) |
| `homeassistant/packages/solix_regelaar.yaml` | Solix vrijgave-helpers, calc-sensoren, meldingen |
| `homeassistant/mqtt.yaml` | Meter-, Growatt-, FP4All- en droogloop-sensoren (`mqtt: !include mqtt.yaml`) |
| `homeassistant/dashboards/zonnestroom_dashboard.yaml` | Lovelace dashboard (6 tabs) – bron voor de Raw configuratie-editor |
| `homeassistant/deploy.sh` | Packages + mqtt.yaml deployen via SSH |

> **Let op:** Het dashboard draait in **storage-modus** (`/config/.storage/lovelace.dashboard_zonnestroom`,
> id `dashboard_zonnestroom`, URL `/dashboard-zonnestroom`). Een YAML-bestand in `/config/dashboards/`
> wordt door HA **niet** gelezen.

---

## Dashboard wijzigen en deployen

1. Bewerk `homeassistant/dashboards/zonnestroom_dashboard.yaml` lokaal in VS Code
2. Open in HA het dashboard **Zonnestroom → ⋮ → Bewerken → ⋮ → Raw configuratie-editor**
3. Vervang de volledige inhoud door de YAML en klik **Opslaan**. Het resultaat is direct zichtbaar, er is geen herstart nodig.

Andersom werkt het ook: na een wijziging in de UI kopieer je de Raw config terug naar
`homeassistant/dashboards/zonnestroom_dashboard.yaml` en commit je die, zodat repo en live gelijk blijven.

`bash homeassistant/deploy.sh` deployt alleen packages + `mqtt.yaml` (met config check en herstart), niet het dashboard.

---

## Stap 1 – SSH addon inschakelen (eenmalig)

Als SSH nog niet beschikbaar is:

1. Open HA via browser: `http://192.168.0.60:8123`
2. **Instellingen → Add-ons → Add-on winkel**
3. Zoek: **Advanced SSH & Web Terminal**
4. Installeer → Configureer → **Start**

---

## Stap 2 – Package bestanden deployen

```bash
bash homeassistant/deploy.sh
```

Dit kopieert de MQTT package naar `/config/packages/` en herstart HA.

---

## Stap 3 – configuration.yaml aanpassen (eenmalig)

Voeg toe aan `/config/configuration.yaml` (als nog niet aanwezig):

```yaml
homeassistant:
  packages: !include_dir_named packages
```

---

## Stap 4 – HA herstarten (na package wijzigingen)

1. **Ontwikkelaarstools → YAML valideren** (controleer eerst op fouten)
2. **Instellingen → Systeem → Opnieuw opstarten**


Als SSH nog niet beschikbaar is:

1. Open HA via browser: `http://192.168.0.60:8123`
2. **Instellingen → Add-ons → Add-on winkel**
3. Zoek: **Advanced SSH & Web Terminal**
4. Installeer → Configureer → **Start**
5. Poort instelling: standaard is 22222 of 22 (zie config van de addon)

Alternatief: gebruik de **Samba Share** addon → dan mount je `\\192.168.0.60\config` als netwerk schijf.

---

## Stap 2 – Package bestanden kopiëren

### Via SSH (als beschikbaar):
```bash
bash homeassistant/deploy.sh
```

### Via Samba (handmatig):
1. Open Windows Verkenner → `\\192.168.0.60\config`
2. Maak map `packages` aan (als die niet bestaat)
3. Kopieer `homeassistant/packages/zonnestroom.yaml` → `\\192.168.0.60\config\packages\zonnestroom.yaml`

### Via HA File Editor addon:
1. Installeer **File editor** addon in HA
2. Open `/config/packages/zonnestroom.yaml`
3. Plak de inhoud van `homeassistant/packages/zonnestroom.yaml`

---

## Stap 3 – configuration.yaml aanpassen

Voeg toe aan `/config/configuration.yaml` (als nog niet aanwezig):

```yaml
homeassistant:
  packages: !include_dir_named packages
```

> Let op: als de `homeassistant:` sectie al bestaat, voeg dan alleen `packages:` toe eronder.

---

## Stap 4 – Dashboard aanmaken

### Optie A – Nieuw dashboard (storage-modus, aanbevolen)
1. Ga naar **Instellingen → Dashboards**
2. Klik **+ Dashboard toevoegen**
3. Kies naam: `Zonnestroom`
4. Klik op het dashboard → **⋮ → Bewerken → Raw configuratie-editor**
5. Plak de inhoud van `homeassistant/dashboards/zonnestroom_dashboard.yaml`

### Optie B – Bestaand dashboard
Vervang de inhoud van een bestaand dashboard via de Raw configuratie-editor.

---

## Stap 5 – HA herstarten

Na het kopiëren van de bestanden:

1. **Ontwikkelaarstools → YAML valideren** (controleer eerst op fouten)  
2. **Instellingen → Systeem → Opnieuw opstarten**

Of via SSH:
```bash
ssh HAS "ha core restart"
```

---

## Dashboard Overzicht

### Tab 1 – Overzicht
- Verbindingstatus Opta1 + Opta2
- Alarmbanner (rood, alleen bij actief alarm)
- Surplus gauges fase1 en totaal
- Systeem prioriteit (IDLE / WP / Element / Hottub / FOUT)
- Actieve apparaten glance
- Surplus historiek grafiek

### Tab 2 – Hottub
- Temperatuur gauge (huidig vs setpoint)
- Doel / hysterese / max-beveiliging instellen
- Status: verwarming, pomp, niveau, vulpomp, comm
- Enable schakelaars
- Handmatige modus + handmatig hottub permissie

### Tab 3 – Warmtepomp & Boiler
- Boiler temperaturen (hoog + laag gauges)
- WP D0 (extra WW) + D1 (comfort verwarming) status
- Handmatige modus schakelaar
- Handmatig D0 / D1 / Element
- Temperatuur setpoints WP + Element

### Tab 4 – Instellingen
- Alle enable flags
- Surplus drempelwaarden (start WP/element/hottub, stop)
- Fault Reset knop
- Debug / diagnostiek entiteiten

---

## Handmatige modus uitleg

⚠️ De handmatige modus overschrijft de automatische prioriteitsmanager.

| Stap | Actie |
|---|---|
| 1 | Zet "Handmatige Modus" **aan** |
| 2 | Zet gewenste schakelaar aan (WP Extra WW, Comfort, Element, Hottub) |
| 3 | Apparaat wordt direct aangestuurd |
| 4 | Zet na gebruik "Handmatige Modus" **uit** |
| Auto | Na 2 uur zet HA de handmatige modus automatisch uit |

---

## Nieuwe MQTT topics (ge-update firmware)

| Topic | Richting | Functie |
|---|---|---|
| `opta1/cmd/manual_mode` | HA → Opta1 | Handmatige modus aan/uit |
| `opta1/cmd/manual_force_comfort` | HA → Opta1 | D1 comfort verwarming forceren |
| `opta1/status/comfort_active` | Opta1 → HA | D1 status terugmelding |
| `opta1/status/manual_mode` | Opta1 → HA | Manual mode status terugmelding |
