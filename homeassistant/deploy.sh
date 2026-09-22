#!/usr/bin/env bash
# ============================================================
#  Zonnestroom Opta – HA Deployment Script
#  HA server: 192.168.0.60  SSH alias: HAS
#  Gebruiksaanwijzing: bash homeassistant/deploy.sh
#
#  Wat wordt gedeployed:
#   - homeassistant/packages/zonnestroom.yaml     →  /config/packages/
#   - homeassistant/packages/solix_regelaar.yaml  →  /config/packages/
#   - homeassistant/mqtt.yaml                     →  /config/mqtt.yaml
#
#  Het dashboard wordt NIET gedeployed: het live dashboard draait in
#  storage-modus (/config/.storage/lovelace.dashboard_zonnestroom).
#  Wijzig het via de Raw configuratie-editor, zie INSTALLATIE.md.
#
#  Na deploy volgt een config check en een HA herstart.
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
$HA_SSH "mkdir -p $HA_CONFIG/packages"

# Kopieer package YAML
$HA_SCP homeassistant/packages/zonnestroom.yaml \
        homeassistant/packages/solix_regelaar.yaml \
        HAS:$HA_CONFIG/packages/

echo "==> Packages gekopieerd."

# Kopieer MQTT-entities (mqtt: !include mqtt.yaml in configuration.yaml)
$HA_SCP homeassistant/mqtt.yaml HAS:$HA_CONFIG/mqtt.yaml

echo "==> mqtt.yaml gekopieerd."

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

# HA config check + herstarten (nodig voor nieuwe/gewijzigde entities)
echo "==> HA configuratie valideren en herstarten..."
$HA_SSH "ha core check" || { echo "!! Config check mislukt – controleer de YAML syntax"; exit 1; }
$HA_SSH "ha core restart"

echo ""
echo "✓ Deployment klaar! HA herstart is gestart."
echo "  Wacht ~30 seconden en refresh dan de browser."
