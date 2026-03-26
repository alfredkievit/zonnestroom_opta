#!/usr/bin/env bash
# ============================================================
#  Zonnestroom Opta – HA Deployment Script
#  HA server: 192.168.0.60  SSH alias: HAS
#  Gebruiksaanwijzing: bash homeassistant/deploy.sh
#
#  Wat wordt gedeployed:
#   - homeassistant/packages/zonnestroom.yaml  →  /config/packages/
#   - homeassistant/lovelace.zonnestroom_dashboard.json
#                                              →  /config/.storage/lovelace.zonnestroom_dashboard
#
#  Het dashboard werkt via HA storage (JSON), NIET via een YAML config-bestand.
#  Na deploy: hard refresh browser (Ctrl+Shift+R) – HA herstart is niet nodig
#  voor dashboard wijzigingen; voor package wijzigingen wel.
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

# Maak packages directory aan
$HA_SSH "mkdir -p $HA_CONFIG/packages"

# Kopieer package YAML
$HA_SCP homeassistant/packages/zonnestroom.yaml \
        HAS:$HA_CONFIG/packages/zonnestroom.yaml

echo "==> Package YAML gekopieerd."

# Kopieer dashboard (HA storage JSON)
$HA_SCP homeassistant/lovelace.zonnestroom_dashboard.json \
        HAS:$HA_CONFIG/.storage/lovelace.zonnestroom_dashboard

echo "==> Dashboard JSON gekopieerd naar .storage."

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

# HA config check + herstarten (alleen nodig na package wijzigingen)
echo "==> HA configuratie valideren en herstarten..."
$HA_SSH "ha core check" || echo "!! Config check mislukt – controleer de YAML syntax"
$HA_SSH "ha core restart"

echo ""
echo "✓ Deployment klaar!"
echo "  Dashboard: hard refresh browser (Ctrl+Shift+R)"
