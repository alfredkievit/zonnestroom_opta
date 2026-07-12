#!/bin/bash
# COMMIT_AND_PUSH.sh — Commit all local changes and push to GitHub
# Usage: bash COMMIT_AND_PUSH.sh
# Or from PowerShell: bash .\COMMIT_AND_PUSH.sh

set -e  # Exit on error

echo "=========================================="
echo "Zonnestroom_Opta: Commit & Push to GitHub"
echo "=========================================="
echo ""

# Navigate to repo root
cd "$(dirname "$0")"

# Show current status
echo "Current status:"
git status --short
echo ""

# Check if there are changes
if ! git diff-index --quiet HEAD --; then
    echo "✓ Found uncommitted changes"
else
    echo "⚠ No changes to commit"
    exit 0
fi

# Check if on master
CURRENT_BRANCH=$(git rev-parse --abbrev-ref HEAD)
if [ "$CURRENT_BRANCH" != "master" ]; then
    echo "⚠ WARNING: Not on master branch (currently on $CURRENT_BRANCH)"
    read -p "Continue anyway? (y/n) " -n 1 -r
    echo
    if [[ ! $REPLY =~ ^[Yy]$ ]]; then
        echo "Aborted."
        exit 1
    fi
fi

echo ""
echo "Step 1: Add all changes"
git add -A
echo "✓ All files staged"

echo ""
echo "Step 2: Create commit"
TIMESTAMP=$(date '+%Y-%m-%d %H:%M:%S')
COMMIT_MSG="fix(mqtt): Add timeout hysteresis to prevent false alarms

- Implement 1.5x multiplier for alarm threshold (prevents transient WiFi hiccups)
- Add recovery window at 0.5x timeout (clears alarm when data briefly returns)
- Increase default mqttTimeoutSec from 30s to 60s for WiFi margin
- Debug logging for timeout state transitions
- All other recent improvements and stability fixes

Changes affect:
- mqtt_manager.h: Hysteresis state tracking
- mqtt_manager.cpp: Debounced _checkTimeout() logic
- types.h: Default timeout increased to 60s
- Various other Opta1/Opta2 stability improvements

Verified with: VERIFICATION_CHECKLIST.md
Reference: MQTT_TIMEOUT_FIX.md"

git commit -m "$COMMIT_MSG"
echo "✓ Commit created"

echo ""
echo "Step 3: Verify commit"
git log --oneline -1
echo ""
git log --format="%B" -1
echo ""

echo ""
echo "Step 4: Push to GitHub"
if git push origin master; then
    echo "✓ Push successful!"
    echo ""
    echo "=========================================="
    echo "SUCCESS: All changes pushed to GitHub"
    echo "=========================================="
    echo ""
    echo "Next steps:"
    echo "1. Verify on GitHub: https://github.com/yourname/Zonnestroom_Opta"
    echo "2. In the field: git clone/pull and build normally"
    echo "3. Monitor: VERIFICATION_CHECKLIST.md"
    echo ""
else
    echo "❌ Push failed!"
    echo "Check your GitHub credentials and internet connection"
    exit 1
fi
