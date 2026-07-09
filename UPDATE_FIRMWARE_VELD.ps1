# UPDATE_FIRMWARE_VELD.ps1 — Quick firmware update from GitHub (for field use)
# Usage: .\UPDATE_FIRMWARE_VELD.ps1
# This script is standalone — it clones the latest code from GitHub and builds

Write-Host "==========================================" -ForegroundColor Cyan
Write-Host "Zonnestroom_Opta: Field Firmware Update" -ForegroundColor Cyan
Write-Host "==========================================" -ForegroundColor Cyan
Write-Host ""

# Config
$GITHUB_REPO = "https://github.com/yourname/Zonnestroom_Opta.git"  # UPDATE THIS WITH YOUR GITHUB URL
$TEMP_DIR = "C:\Temp\Zonnestroom_Opta_Update"
$BUILD_TARGET = "opta1_master"  # or "opta2_hottub"

Write-Host "Configuration:" -ForegroundColor Yellow
Write-Host "  Repo: $GITHUB_REPO"
Write-Host "  Temp dir: $TEMP_DIR"
Write-Host "  Build target: $BUILD_TARGET"
Write-Host ""

# Prompt for target
$target = Read-Host "Which device to update? (opta1_master/opta2_hottub) [default: opta1_master]"
if ($target -eq "") { $target = "opta1_master" }
$BUILD_TARGET = $target

Write-Host ""
Write-Host "Step 1: Clone latest code from GitHub" -ForegroundColor Yellow
if (Test-Path $TEMP_DIR) {
    Write-Host "Cleaning old temp directory..."
    Remove-Item -Recurse -Force $TEMP_DIR
}
New-Item -ItemType Directory -Path $TEMP_DIR | Out-Null
cd $TEMP_DIR

if (-not (git clone $GITHUB_REPO .)) {
    Write-Host "❌ Clone failed!" -ForegroundColor Red
    Write-Host "Check:"
    Write-Host "  1. Internet connection"
    Write-Host "  2. GitHub repo URL in this script (update line ~10)"
    Write-Host "  3. GitHub credentials"
    exit 1
}
Write-Host "✓ Latest code cloned" -ForegroundColor Green

Write-Host ""
Write-Host "Step 2: Verify build environment" -ForegroundColor Yellow
if (-not (Get-Command pio -ErrorAction SilentlyContinue)) {
    Write-Host "❌ PlatformIO not found!" -ForegroundColor Red
    Write-Host "Install: pip install platformio"
    exit 1
}
pio --version
Write-Host "✓ PlatformIO ready" -ForegroundColor Green

Write-Host ""
Write-Host "Step 3: Build firmware" -ForegroundColor Yellow
cd "$BUILD_TARGET"
pio run -e arduino_opta
if ($LASTEXITCODE -ne 0) {
    Write-Host "❌ Build failed!" -ForegroundColor Red
    exit 1
}
Write-Host "✓ Build successful" -ForegroundColor Green

Write-Host ""
Write-Host "Step 4: Show binary location" -ForegroundColor Yellow
$binary = Get-ChildItem -Recurse -Filter "*.bin" | Where-Object { $_.FullName -match "\.pio" } | Select-Object -Last 1
if ($binary) {
    Write-Host "Firmware ready to flash:" -ForegroundColor Green
    Write-Host "  $($binary.FullName)"
    Write-Host ""
    Write-Host "To flash via Arduino IDE or CLI:"
    Write-Host "  arduino-cli upload -b arduino:mbed_opta:opta -p COMX --input-dir .pio/build/arduino_opta"
    Write-Host ""
    Write-Host "Or use your usual flashing method (Arduino IDE, PlatformIO Home, etc.)"
} else {
    Write-Host "⚠ Binary not found, but build succeeded. Check .pio/build/arduino_opta/" -ForegroundColor Yellow
}

Write-Host ""
Write-Host "==========================================" -ForegroundColor Green
Write-Host "Ready to flash!" -ForegroundColor Green
Write-Host "==========================================" -ForegroundColor Green
Write-Host ""
Write-Host "Next:"
Write-Host "1. Connect Opta to USB"
Write-Host "2. Flash the binary (above)"
Write-Host "3. Monitor serial: pio device monitor"
Write-Host "4. Verify MQTT timeout behavior per VERIFICATION_CHECKLIST.md"
Write-Host ""
