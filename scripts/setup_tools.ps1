# setup_tools.ps1 — Download and place ffprobe and mkvmerge for Windows
# Run once after cloning the repo, from the repo root:
#   powershell -ExecutionPolicy Bypass -File scripts/setup_tools.ps1

$ErrorActionPreference = "Stop"
$ToolsDir = Join-Path $PSScriptRoot "..\tools\windows"
New-Item -ItemType Directory -Force -Path $ToolsDir | Out-Null

Write-Host "Setting up MediaCurator external tools for Windows..." -ForegroundColor Cyan

# ── ffprobe (gyan.dev LGPL essentials build) ───────────────────────────────────
$FfprobeExe = Join-Path $ToolsDir "ffprobe.exe"
if (-not (Test-Path $FfprobeExe)) {
    Write-Host "Downloading ffprobe (LGPL build)..."

    # Check latest release from gyan.dev
    $FfmpegRelease = "7.1"  # Update this when a newer build is desired
    $FfprobeUrl = "https://www.gyan.dev/ffmpeg/builds/ffmpeg-release-essentials.zip"
    $ZipPath = Join-Path $env:TEMP "ffmpeg-essentials.zip"
    $ExtractPath = Join-Path $env:TEMP "ffmpeg-extract"

    Invoke-WebRequest -Uri $FfprobeUrl -OutFile $ZipPath -UseBasicParsing
    Expand-Archive -Path $ZipPath -DestinationPath $ExtractPath -Force

    # Find ffprobe.exe inside the extracted folder
    $FfprobeSource = Get-ChildItem -Path $ExtractPath -Filter "ffprobe.exe" -Recurse | Select-Object -First 1
    if ($FfprobeSource) {
        Copy-Item $FfprobeSource.FullName $FfprobeExe
        Write-Host "  ffprobe.exe installed at $FfprobeExe" -ForegroundColor Green
    } else {
        Write-Warning "Could not find ffprobe.exe in the downloaded archive."
    }

    Remove-Item $ZipPath -Force -ErrorAction SilentlyContinue
    Remove-Item $ExtractPath -Recurse -Force -ErrorAction SilentlyContinue
} else {
    Write-Host "  ffprobe.exe already present" -ForegroundColor Gray
}

# ── MKVToolNix (mkvmerge + mkvpropedit) ────────────────────────────────────────
$MkvmergeExe    = Join-Path $ToolsDir "mkvmerge.exe"
$MkvpropeditExe = Join-Path $ToolsDir "mkvpropedit.exe"

if (-not (Test-Path $MkvmergeExe)) {
    Write-Host "Downloading MKVToolNix portable..."

    # MKVToolNix portable zip from official GitLab releases
    $MkvVersion = "88.0"  # Update as needed
    $MkvUrl = "https://mkvtoolnix.download/windows/releases/${MkvVersion}/mkvtoolnix-64-bit-${MkvVersion}.7z"

    Write-Host "  NOTE: MKVToolNix uses 7z format. Please download manually from:"
    Write-Host "  https://mkvtoolnix.download/downloads.html#windows" -ForegroundColor Yellow
    Write-Host "  Extract mkvmerge.exe and mkvpropedit.exe to: $ToolsDir"
    Write-Host ""
    Write-Host "  OR install MKVToolNix normally and copy from the install dir:"
    Write-Host "  C:\Program Files\MKVToolNix\mkvmerge.exe"
    Write-Host "  C:\Program Files\MKVToolNix\mkvpropedit.exe"
} else {
    Write-Host "  mkvmerge.exe already present" -ForegroundColor Gray
}

# ── Verify ─────────────────────────────────────────────────────────────────────
Write-Host ""
Write-Host "Tool status:"
@("ffprobe.exe", "mkvmerge.exe", "mkvpropedit.exe") | ForEach-Object {
    $Path = Join-Path $ToolsDir $_
    if (Test-Path $Path) {
        Write-Host "  [OK] $_" -ForegroundColor Green
    } else {
        Write-Host "  [MISSING] $_" -ForegroundColor Red
    }
}
