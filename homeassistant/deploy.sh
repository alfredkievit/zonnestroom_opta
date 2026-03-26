#!/usr/bin/env bash
# ============================================================
#  Zonnestroom Opta – HA Deployment Script
#  HA server: 192.168.0.60  SSH alias: HAS
#  Gebruiksaanwijzing: bash homeassistant/deploy.sh
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
  echo "   Probeer ook poort 22222:"
  echo "   ssh -p 22222 root@$HA_HOST"
  echo ""
  echo "   Alternatief: kopieer bestanden handmatig via HA Samba share"
  echo "   of via de File Editor addon."
  exit 1
fi

echo "==> SSH OK. Bestanden kopiëren..."

# Maak directories aan
$HA_SSH "mkdir -p $HA_CONFIG/packages $HA_CONFIG/dashboards"

# Kopieer package YAML
$HA_SCP homeassistant/packages/zonnestroom.yaml \
        HAS:$HA_CONFIG/packages/zonnestroom.yaml

echo "==> Package YAML gekopieerd."

# Kopieer dashboard YAML
$HA_SCP homeassistant/dashboards/zonnestroom_dashboard.yaml \
        HAS:$HA_CONFIG/dashboards/zonnestroom_dashboard.yaml

echo "==> Dashboard YAML gekopieerd."

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

# HA config check
echo "==> HA configuratie valideren..."
$HA_SSH "ha core check" || echo "!! Config check mislukt – controleer de YAML syntax"

echo ""
echo "==> Herstarten van HA core..."
$HA_SSH "ha core restart"

echo ""
echo "✓ Deployment klaar!"
echo ""
echo "  Dashboard instellen in HA:"
echo "  1. Ga naar Instellingen → Dashboards → Dashboard toevoegen"
echo "  2. Kies 'Vanuit YAML (opgeslagen modus)'"
echo "  3. Of: gebruik onderstaand lovelace-raw YAML in een nieuw dashboard"
echo "     Bestand staat op: $HA_CONFIG/dashboards/zonnestroom_dashboard.yaml"
