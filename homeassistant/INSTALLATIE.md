# HA Dashboard – Installatiehandleiding

## Wat is al klaar

De bestanden staan klaar in je workspace:

| Bestand | Doel |
|---|---|
| `homeassistant/packages/zonnestroom.yaml` | Alle MQTT entities (sensors, switches, numbers) |
| `homeassistant/lovelace.zonnestroom_dashboard.json` | Lovelace dashboard (4 tabs) – HA storage formaat |
| `homeassistant/deploy.sh` | Automatisch deployen via SSH |

> **Let op:** Het dashboard werkt via HA's interne storage (`/config/.storage/`), **niet** via een YAML config-bestand.

---

## Dashboard wijzigen en deployen

1. Bewerk `homeassistant/dashboards/zonnestroom_dashboard.yaml` lokaal in VS Code
2. Deploy via SCP:
   ```bash
   scp homeassistant/dashboards/zonnestroom_dashboard.yaml \
       HAS:/config/dashboards/zonnestroom_dashboard.yaml
   ```
3. **HA herstarten is verplicht** – browser refresh alleen is niet voldoende:
   ```bash
   ssh HAS "ha core restart"
   ```

> Het dashboard staat in YAML-modus (`mode: yaml`). HA herleest de YAML alleen bij opstart.

Of gebruik `bash homeassistant/deploy.sh` om alles (packages + dashboard + herstart) in één keer te doen.

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

### Optie A – YAML bestanden dashboard (aanbevolen)
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
