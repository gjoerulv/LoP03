# Crystal Dungeons — gear-icon review harness (M81).
#
# Composites the 10x10 icons under assets/textures/ui/icons/ into one labelled,
# magnified contact sheet (docs/sprite_review/icons_contact.png), on both a
# dark row (the Inset list panels they actually sit on) and a light row, so
# contrast can be judged against both. Nothing here writes to assets/ — this
# script only reads the generated PNGs. Requires Windows PowerShell
# (System.Drawing), like preview.ps1.
#
# Usage:
#   powershell tools/asset_gen/preview_icons.ps1
#   powershell tools/asset_gen/preview_icons.ps1 -Zoom 16

[CmdletBinding()]
param(
  [int]$Zoom = 12
)

$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing

$repo = Split-Path (Split-Path $PSScriptRoot -Parent) -Parent
$iconDir = Join-Path $repo 'assets\textures\ui\icons'
$outDir = Join-Path $repo 'docs\sprite_review'
New-Item -ItemType Directory -Force $outDir | Out-Null

$files = Get-ChildItem $iconDir -Filter *.png | Sort-Object Name
if ($files.Count -eq 0) { throw "no icons found under $iconDir" }

$cell = 10 * $Zoom + 16          # icon + padding
$labelH = 16
$rowH = $cell + $labelH
$sheetW = $cell * $files.Count + 16
$sheetH = $rowH * 2 + 24         # dark row + light row

$sheet = New-Object System.Drawing.Bitmap($sheetW, $sheetH)
$g = [System.Drawing.Graphics]::FromImage($sheet)
$g.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::NearestNeighbor
$g.PixelOffsetMode = [System.Drawing.Drawing2D.PixelOffsetMode]::Half
$g.Clear([System.Drawing.Color]::FromArgb(255, 60, 60, 64))
$font = New-Object System.Drawing.Font('Consolas', 9)
$ink = New-Object System.Drawing.SolidBrush([System.Drawing.Color]::White)
$dark = New-Object System.Drawing.SolidBrush([System.Drawing.Color]::FromArgb(255, 22, 20, 30))
$light = New-Object System.Drawing.SolidBrush([System.Drawing.Color]::FromArgb(255, 190, 188, 180))

for ($i = 0; $i -lt $files.Count; $i++) {
  $img = [System.Drawing.Bitmap]::FromFile($files[$i].FullName)
  $x = 8 + $i * $cell
  foreach ($row in 0..1) {
    $y = 8 + $row * $rowH
    $bg = if ($row -eq 0) { $dark } else { $light }
    $g.FillRectangle($bg, $x, $y, $cell - 8, $cell - 8)
    $g.DrawImage($img, $x + 8, $y + 8, 10 * $Zoom, 10 * $Zoom)
  }
  $g.DrawString([System.IO.Path]::GetFileNameWithoutExtension($files[$i].Name), $font, $ink,
                $x, $sheetH - $labelH - 4)
  $img.Dispose()
}

$g.Dispose()
$out = Join-Path $outDir 'icons_contact.png'
$sheet.Save($out, [System.Drawing.Imaging.ImageFormat]::Png)
$sheet.Dispose()
Write-Output "  docs/sprite_review/icons_contact.png ($($files.Count) icons at $($Zoom)x)"
