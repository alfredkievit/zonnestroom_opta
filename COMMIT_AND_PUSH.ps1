# COMMIT_AND_PUSH.ps1 — Commit all local changes and push to GitHub
# Usage: .\COMMIT_AND_PUSH.ps1
# Or: powershell -ExecutionPolicy Bypass -File .\COMMIT_AND_PUSH.ps1

Write-Host "==========================================" -ForegroundColor Cyan
Write-Host "Zonnestroom_Opta: Commit & Push to GitHub" -ForegroundColor Cyan
Write-Host "==========================================" -ForegroundColor Cyan
Write-Host ""

# Navigate to script directory
Set-Location (Split-Path -Parent $MyInvocation.MyCommand.Path)

# Show current status
Write-Host "Current status:" -ForegroundColor Yellow
git status --short
Write-Host ""

# Check if there are changes
$hasChanges = -not [string]::IsNullOrWhiteSpace((git diff-index HEAD --))
if ($hasChanges) {
    Write-Host "✓ Found uncommitted changes" -ForegroundColor Green
} else {
    Write-Host "⚠ No changes to commit" -ForegroundColor Yellow
    exit 0
}

# Check if on master
$currentBranch = git rev-parse --abbrev-ref HEAD
if ($currentBranch -ne "master") {
    Write-Host "⚠ WARNING: Not on master branch (currently on $currentBranch)" -ForegroundColor Yellow
    $response = Read-Host "Continue anyway? (y/n)"
    if ($response -ne "y" -and $response -ne "Y") {
        Write-Host "Aborted." -ForegroundColor Red
        exit 1
    }
}

Write-Host ""
Write-Host "Step 1: Add all changes" -ForegroundColor Yellow
git add -A
Write-Host "✓ All files staged" -ForegroundColor Green

Write-Host ""
Write-Host "Step 2: Create commit" -ForegroundColor Yellow

$commitMsg = @"
fix(mqtt): Add timeout hysteresis to prevent false alarms

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
Reference: MQTT_TIMEOUT_FIX.md
"@

git commit -m $commitMsg
Write-Host "✓ Commit created" -ForegroundColor Green

Write-Host ""
Write-Host "Step 3: Verify commit" -ForegroundColor Yellow
git log --oneline -1
Write-Host ""
git log --format="%B" -1
Write-Host ""

Write-Host ""
Write-Host "Step 4: Push to GitHub" -ForegroundColor Yellow
git push origin master
if ($LASTEXITCODE -eq 0) {
    Write-Host "✓ Push successful!" -ForegroundColor Green
    Write-Host ""
    Write-Host "==========================================" -ForegroundColor Green
    Write-Host "SUCCESS: All changes pushed to GitHub" -ForegroundColor Green
    Write-Host "==========================================" -ForegroundColor Green
    Write-Host ""
    Write-Host "Next steps:" -ForegroundColor Cyan
    Write-Host "1. Verify on GitHub: https://github.com/yourname/Zonnestroom_Opta"
    Write-Host "2. In the field: git clone/pull and build normally"
    Write-Host "3. Monitor: VERIFICATION_CHECKLIST.md"
    Write-Host ""
} else {
    Write-Host "❌ Push failed!" -ForegroundColor Red
    Write-Host "Check your GitHub credentials and internet connection" -ForegroundColor Red
    exit 1
}
