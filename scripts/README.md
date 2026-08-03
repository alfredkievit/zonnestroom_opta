# Scripts

Losse Node.js/Python hulpscripts voor MQTT-diagnose tegen de broker op `192.168.0.10`. Geen onderdeel van de firmware; alleen voor handmatig gebruik vanaf een laptop.

## Installeren

```bash
cd scripts
npm install
```

## Overzicht

| Script | Doel |
|---|---|
| `check_status.js` | Snelle eenmalige uitlezing van Opta1-boilerstatus |
| `diagnostic_opta1.js` | Uitgebreider: temperaturen, state transitions, WP-activatie volgen |
| `debug_enable_flags.js` | Controleert of enable-flags aankomen en persisteren |
| `debug_meter_topics.js` | Toont welke energiemeter-topics binnenkomen |
| `test_safe_connectivity.js` | **Veilig**: alleen connectiviteit/heartbeat, geen data-injectie |
| `test_mqtt.js` / `test_mqtt.py` | Connectiviteit- en state-machine test, injecteert gesimuleerde meterwaarden (JS en Python variant, functioneel identiek) |
| `test_wp_activation.js` | Injecteert surplus en volgt WP-activatie |
| `test_wp_full.js` | Volledige WP-activatietest over meerdere temperaturen |
| `test_opta2_network_failover.js` | Monitor tijdens handmatig los-/vastmaken van de Opta2 LAN-kabel |
| `check_github_sync.sh` | Vergelijkt lokale branch met GitHub cloud (fetch, ahead/behind, kritieke Opta2-bestanden) |

## GitHub local vs cloud check

Voer uit vanaf de repo-root:

```bash
bash scripts/check_github_sync.sh
```

Dit script geeft:

- local HEAD vs cloud HEAD
- behind/ahead aantallen
- on-gecommitte wijzigingen
- verschillen op kritieke Opta2-bestanden

## ⚠️ Veiligheid

Scripts die meterwaarden **injecteren** (`test_mqtt.js`/`.py`, `test_wp_activation.js`, `test_wp_full.js`) publiceren op de echte energiemeter-topics. Als het live systeem op dat moment actief is, kan dit ongewild WP en boiler-element tegelijk activeren (interlock-conflict) of andere echte lasten aansturen.

- Gebruik voor een levend systeem alleen `test_safe_connectivity.js` (geen injectie).
- Injectiescripts alleen gebruiken met het systeem bewust offline/in onderhoud, nooit tegen een productie-installatie met echte lasten aangesloten.
