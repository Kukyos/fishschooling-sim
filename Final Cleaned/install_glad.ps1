# Automated GLAD Setup
# Run this after downloading glad.zip from https://glad.dav1d.de/

param(
    [string]$GladZipPath = "$env:USERPROFILE\Downloads\glad.zip"
)

$ErrorActionPreference = "Stop"

Write-Host "========================================" -ForegroundColor Cyan
Write-Host "GLAD Loader Setup" -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan
Write-Host ""

# Check if glad.zip exists
if (-not (Test-Path $GladZipPath)) {
    Write-Host "[ERROR] glad.zip not found at: $GladZipPath" -ForegroundColor Red
    Write-Host ""
    Write-Host "Please download GLAD first:" -ForegroundColor Yellow
    Write-Host "1. Go to: https://glad.dav1d.de/" -ForegroundColor Yellow
    Write-Host "2. Settings: Language=C/C++, gl=4.5, Profile=Core" -ForegroundColor Yellow
    Write-Host "3. Generate and download glad.zip" -ForegroundColor Yellow
    Write-Host "4. Save to: $GladZipPath" -ForegroundColor Yellow
    Write-Host "5. Run this script again" -ForegroundColor Yellow
    exit 1
}

Write-Host "[OK] Found glad.zip" -ForegroundColor Green

# Create temp extraction directory
$tempDir = Join-Path $env:TEMP "glad_extract"
if (Test-Path $tempDir) {
    Remove-Item -Recurse -Force $tempDir
}
New-Item -ItemType Directory -Path $tempDir | Out-Null

# Extract
Write-Host "Extracting glad.zip..." -ForegroundColor Cyan
Expand-Archive -Path $GladZipPath -DestinationPath $tempDir -Force

# Find the actual files (they might be in subdirectories)
$gladC = Get-ChildItem -Path $tempDir -Recurse -Filter "glad.c" | Select-Object -First 1
$gladH = Get-ChildItem -Path $tempDir -Recurse -Filter "glad.h" | Select-Object -First 1
$khrH = Get-ChildItem -Path $tempDir -Recurse -Filter "khrplatform.h" | Select-Object -First 1

if (-not $gladC -or -not $gladH -or -not $khrH) {
    Write-Host "[ERROR] Could not find GLAD files in archive!" -ForegroundColor Red
    Write-Host "Expected: glad.c, glad.h, khrplatform.h" -ForegroundColor Yellow
    exit 1
}

# Copy files to src/
$srcDir = Join-Path $PSScriptRoot "src"
$gladDir = Join-Path $srcDir "glad"
$khrDir = Join-Path $srcDir "KHR"

Write-Host "Creating directories..." -ForegroundColor Cyan
New-Item -ItemType Directory -Path $gladDir -Force | Out-Null
New-Item -ItemType Directory -Path $khrDir -Force | Out-Null

Write-Host "Copying files..." -ForegroundColor Cyan
Copy-Item $gladC.FullName -Destination (Join-Path $srcDir "glad.c") -Force
Copy-Item $gladH.FullName -Destination (Join-Path $gladDir "glad.h") -Force
Copy-Item $khrH.FullName -Destination (Join-Path $khrDir "khrplatform.h") -Force

# Cleanup
Remove-Item -Recurse -Force $tempDir

Write-Host ""
Write-Host "========================================" -ForegroundColor Green
Write-Host "[SUCCESS] GLAD installed!" -ForegroundColor Green
Write-Host "========================================" -ForegroundColor Green
Write-Host ""
Write-Host "Files installed:" -ForegroundColor Cyan
Write-Host "  src/glad.c" -ForegroundColor Gray
Write-Host "  src/glad/glad.h" -ForegroundColor Gray
Write-Host "  src/KHR/khrplatform.h" -ForegroundColor Gray
Write-Host ""
Write-Host "Next steps:" -ForegroundColor Yellow
Write-Host "1. Ensure GLFW and GLM are installed" -ForegroundColor Gray
Write-Host "2. Run: .\setup.bat" -ForegroundColor Gray
Write-Host "3. Run: .\build.bat" -ForegroundColor Gray
Write-Host ""
