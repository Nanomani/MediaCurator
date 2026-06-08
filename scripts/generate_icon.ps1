# generate_icon.ps1 — Converts app_icon.svg to app_icon.ico (multi-resolution).
#
# Requires ImageMagick 7+ to be installed and `magick` in PATH.
# Download: https://imagemagick.org/script/download.php#windows
#
# Run once from the repo root:
#   .\scripts\generate_icon.ps1
#
# The resulting app_icon.ico should be committed to the repo so that CI
# and other developers do not need ImageMagick installed.

$RepoRoot  = Split-Path $PSScriptRoot -Parent
$SvgPath   = "$RepoRoot\src\resources\icons\app_icon.svg"
$IcoPath   = "$RepoRoot\src\resources\icons\app_icon.ico"

if (-not (Get-Command magick -ErrorAction SilentlyContinue)) {
    Write-Error "ImageMagick (magick) not found in PATH. Install from https://imagemagick.org"
    exit 1
}

Write-Host "Generating $IcoPath from $SvgPath ..."

# Rasterise the SVG at each required size then pack into a single .ico.
# -background none preserves SVG transparency.
# icon:auto-resize packs all sizes into one file.
magick -background none `
    ( "$SvgPath" -resize 16x16   ) `
    ( "$SvgPath" -resize 24x24   ) `
    ( "$SvgPath" -resize 32x32   ) `
    ( "$SvgPath" -resize 48x48   ) `
    ( "$SvgPath" -resize 64x64   ) `
    ( "$SvgPath" -resize 128x128 ) `
    ( "$SvgPath" -resize 256x256 ) `
    "$IcoPath"

if ($LASTEXITCODE -ne 0) {
    Write-Error "magick failed with exit code $LASTEXITCODE"
    exit $LASTEXITCODE
}

Write-Host "Done: $IcoPath"
