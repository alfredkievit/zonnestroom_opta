#!/usr/bin/env bash
# ============================================================
#  Zonnestroom Opta – HA Deployment Script
#  HA server: 192.168.0.60  SSH alias: HAS
#  Gebruiksaanwijzing: bash homeassistant/deploy.sh
#
#  Wat wordt gedeployed:
#   - homeassistant/packages/zonnestroom.yaml  →  /config/packages/
#   - homeassistant/dashboards/zonnestroom_dashboard.yaml
#                                              →  /config/dashboards/
#   - homeassistant/lovelace.zonnestroom_dashboard.json
#                                              →  /config/.storage/lovelace.zonnestroom_dashboard
#
#  Let op: dashboard kan in YAML-modus of storage-modus staan.
#  Daarom deployen we zowel YAML als de storage JSON.
#  Na deploy is een HA HERSTART VERPLICHT – browser refresh is niet genoeg.
# ============================================================

set -e

HA_HOST="192.168.0.60"
HA_SSH="ssh HAS"
HA_SCP="scp -o StrictHostKeyChecking=no"
HA_CONFIG="/config"

echo "==> Controleer SSH verbinding naar $HA_HOST..."
if ! ssh -o ConnectTimeout=5 HAS "echo OK" 2>/dev/null; then
  echo ""
  echo "!! SSH niet bereikbaar op poort 22."
  echo "   Controleer of de SSH addon actief is in HA:"
  echo "   Instellingen → Add-ons → 'Advanced SSH & Web Terminal' → Start"
  echo ""
  exit 1
fi

echo "==> SSH OK. Bestanden kopiëren..."

# Maak directories aan
$HA_SSH "mkdir -p $HA_CONFIG/packages $HA_CONFIG/dashboards $HA_CONFIG/.storage"

# Kopieer package YAML
$HA_SCP homeassistant/packages/zonnestroom.yaml \
        HAS:$HA_CONFIG/packages/zonnestroom.yaml

echo "==> Package YAML gekopieerd."

# Kopieer dashboard YAML (YAML-modus – vereist HA herstart)
$HA_SCP homeassistant/dashboards/zonnestroom_dashboard.yaml \
        HAS:$HA_CONFIG/dashboards/zonnestroom_dashboard.yaml

echo "==> Dashboard YAML gekopieerd."

# Kopieer dashboard storage JSON naar actieve storage-file (zonder .json extensie)
$HA_SCP homeassistant/lovelace.zonnestroom_dashboard.json \
  HAS:$HA_CONFIG/.storage/lovelace.zonnestroom_dashboard

# Optioneel ook een .json kopie bewaren voor inspectie/backup
$HA_SCP homeassistant/lovelace.zonnestroom_dashboard.json \
  HAS:$HA_CONFIG/.storage/lovelace.zonnestroom_dashboard.json

echo "==> Dashboard storage JSON gekopieerd."

# Controleer of packages al in configuration.yaml staan
if ! $HA_SSH "grep -q 'packages' $HA_CONFIG/configuration.yaml"; then
  echo ""
  echo "!! WAARSCHUWING: 'packages' niet gevonden in configuration.yaml"
  echo "   Voeg het volgende handmatig toe aan /config/configuration.yaml:"
  echo ""
  echo "   homeassistant:"
  echo "     packages: !include_dir_named packages"
  echo ""
fi

# HA config check + herstarten (verplicht voor YAML dashboard)
echo "==> HA configuratie valideren en herstarten..."
$HA_SSH "ha core check" || echo "!! Config check mislukt – controleer de YAML syntax"
$HA_SSH "ha core restart"

echo ""
echo "✓ Deployment klaar! HA herstart is gestart."
echo "  Wacht ~30 seconden en refresh dan de browser."
