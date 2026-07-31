# Crystal Dungeons — enemy/boss sprite review harness (M73).
#
# Composites every sprite under assets/textures/enemies/ into review artefacts:
#
#   <prefix>_contact.png     magnified contact sheet, labelled, grouped
#                            normal / elite / boss, art bottom-aligned in its
#                            cell so relative height reads at a glance.
#   <prefix>_silhouette.png  the same grid with every sprite's alpha channel
#                            rendered solid black on white. This is the
#                            binding review artefact: two enemies that are
#                            indistinguishable here fail regardless of colour.
#   <prefix>_strip1x.png     each family laid out side by side at NATIVE size
#                            on a 426x240 canvas — the read the player gets.
#   <prefix>_strip4x.png     the same 1x strip magnified 4x, so the native
#                            read can actually be inspected in a document.
#
# Tier grouping comes from data/enemies.json + data/bosses.json, so the sheets
# follow content, not filenames.
#
# Nothing here writes to assets/ — this script only reads the generated PNGs.
# Requires Windows PowerShell (System.Drawing).
#
# Usage:
#   pwsh tools/asset_gen/preview.ps1
#   pwsh tools/asset_gen/preview.ps1 -Only goblin_grunt,kobold_scout -OutName batch_goblinoid
#   pwsh tools/asset_gen/preview.ps1 -Only boss_keep_warden -Zoom 12 -Columns 3

[CmdletBinding()]
param(
  [string[]]$Only = @(),                 # sprite base names; empty = every sprite
  [string]$OutName = 'sprites',          # output file prefix
  [string]$OutDir = '',                  # default: docs/sprite_review
  [int]$Zoom = 8,
  [int]$Columns = 6
)

$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing

$repo = Split-Path (Split-Path $PSScriptRoot -Parent) -Parent
$spriteDir = Join-Path $repo 'assets\textures\enemies'
if ($OutDir -eq '') { $OutDir = Join-Path $repo 'docs\sprite_review' }
New-Item -ItemType Directory -Force $OutDir | Out-Null

# --- Review-sheet palette (deliberately not the game palette: these are
# --- diagnostic artefacts, and the backdrop must not flatter the art). ---
$SHEET_BG    = [System.Drawing.ColorTranslator]::FromHtml('#141218')
$SHEET_CELL  = [System.Drawing.ColorTranslator]::FromHtml('#1E1B24')
$SHEET_RULE  = [System.Drawing.ColorTranslator]::FromHtml('#3A3646')
$SHEET_TEXT  = [System.Drawing.ColorTranslator]::FromHtml('#C8C4D2')
$SHEET_HEAD  = [System.Drawing.ColorTranslator]::FromHtml('#E8D670')
$SIL_BG      = [System.Drawing.Color]::White
$SIL_INK     = [System.Drawing.Color]::Black
$SIL_TEXT    = [System.Drawing.ColorTranslator]::FromHtml('#303030')
$SIL_RULE    = [System.Drawing.ColorTranslator]::FromHtml('#B8B8B8')

function Read-Png([string]$path) {
  # Via a memory stream so the source PNG is never left locked.
  $bytes = [System.IO.File]::ReadAllBytes($path)
  $ms = New-Object System.IO.MemoryStream(, $bytes)
  $img = [System.Drawing.Image]::FromStream($ms)
  $bmp = New-Object System.Drawing.Bitmap($img.Width, $img.Height, `
    [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
  $g = [System.Drawing.Graphics]::FromImage($bmp)
  $g.SmoothingMode = 'None'; $g.InterpolationMode = 'NearestNeighbor'; $g.PixelOffsetMode = 'Half'
  $g.DrawImage($img, 0, 0, $img.Width, $img.Height)
  $g.Dispose(); $img.Dispose(); $ms.Dispose()
  return $bmp
}

# Solid-black-on-white alpha rendering: the silhouette test.
function To-Silhouette($src) {
  $out = New-Object System.Drawing.Bitmap($src.Width, $src.Height, `
    [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
  for ($y = 0; $y -lt $src.Height; $y++) {
    for ($x = 0; $x -lt $src.Width; $x++) {
      if ($src.GetPixel($x, $y).A -ne 0) { $out.SetPixel($x, $y, $SIL_INK) }
      else { $out.SetPixel($x, $y, $SIL_BG) }
    }
  }
  return $out
}

# Nearest-neighbour blit; TileFlipXY wrap stops the edge-sample bleed that
# GDI+ otherwise adds around a magnified sprite.
function Blit($g, $src, [int]$dx, [int]$dy, [int]$scale) {
  $ia = New-Object System.Drawing.Imaging.ImageAttributes
  $ia.SetWrapMode([System.Drawing.Drawing2D.WrapMode]::TileFlipXY)
  $dest = New-Object System.Drawing.Rectangle($dx, $dy, ($src.Width * $scale), ($src.Height * $scale))
  $g.DrawImage($src, $dest, 0, 0, $src.Width, $src.Height, `
    [System.Drawing.GraphicsUnit]::Pixel, $ia)
  $ia.Dispose()
}

function New-Canvas([int]$w, [int]$h, $bg) {
  $b = New-Object System.Drawing.Bitmap($w, $h, [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
  $g = [System.Drawing.Graphics]::FromImage($b)
  $g.SmoothingMode = 'None'; $g.InterpolationMode = 'NearestNeighbor'; $g.PixelOffsetMode = 'Half'
  $g.Clear($bg)
  return @($b, $g)
}

# --- Tier lookup from content, not filenames ---
$enemyTier = @{}
$enemies = (Get-Content (Join-Path $repo 'data\enemies.json') -Raw | ConvertFrom-Json).enemies
foreach ($e in $enemies) { $enemyTier[$e.id] = $e.tier }
$bossIds = @{}
$bosses = (Get-Content (Join-Path $repo 'data\bosses.json') -Raw | ConvertFrom-Json).bosses
foreach ($bd in $bosses) { $bossIds[$bd.id] = $true }

function Get-Group([string]$base) {
  if ($base -eq 'boss_battle') { return 'boss' }
  if ($base -eq 'elite_battle') { return 'elite' }
  if ($base -eq 'normal_battle') { return 'normal' }
  if ($base.StartsWith('boss_') -and $bossIds.ContainsKey($base.Substring(5))) { return 'boss' }
  if ($enemyTier.ContainsKey($base)) {
    if ($enemyTier[$base] -eq 'elite') { return 'elite' }
    return 'normal'
  }
  return 'unclassified'
}

# --- Collect the sprites under review ---
$files = Get-ChildItem $spriteDir -Filter *.png | Sort-Object Name
# `powershell -File` hands "a,b,c" over as one string, so split it here too.
$Only = @($Only | ForEach-Object { $_ -split ',' } | Where-Object { $_ -ne '' })
if ($Only.Count -gt 0) {
  $wanted = @{}; foreach ($n in $Only) { $wanted[$n] = $true }
  $files = $files | Where-Object { $wanted.ContainsKey([System.IO.Path]::GetFileNameWithoutExtension($_.Name)) }
}
if (-not $files -or @($files).Count -eq 0) { throw "preview.ps1: no sprites matched." }

$sprites = @()
foreach ($f in @($files)) {
  $base = [System.IO.Path]::GetFileNameWithoutExtension($f.Name)
  $bmp = Read-Png $f.FullName
  $sprites += [pscustomobject]@{
    Base = $base; Group = (Get-Group $base); Bmp = $bmp
    W = $bmp.Width; H = $bmp.Height
  }
}

$groupOrder = 'normal', 'elite', 'boss', 'unclassified'
$grouped = [ordered]@{}
foreach ($grp in $groupOrder) {
  $inGrp = @($sprites | Where-Object { $_.Group -eq $grp } | Sort-Object Base)
  if ($inGrp.Count -gt 0) { $grouped[$grp] = $inGrp }
}

$maxW = ($sprites | Measure-Object -Property W -Maximum).Maximum
$maxH = ($sprites | Measure-Object -Property H -Maximum).Maximum

# --- Sheet geometry ---
$pad = 6
$labelH = 15
$headH = 26
$artW = $maxW * $Zoom
$artH = $maxH * $Zoom
$cellW = $artW + $pad * 2
$cellH = $artH + $pad * 2 + $labelH
$cols = [math]::Min($Columns, ($sprites | Measure-Object).Count)
if ($cols -lt 1) { $cols = 1 }

$sheetW = $cols * $cellW + $pad * 2
$sheetH = $pad
foreach ($grp in $grouped.Keys) {
  $rows = [math]::Ceiling(@($grouped[$grp]).Count / [double]$cols)
  $sheetH += $headH + $rows * $cellH + $pad
}

$fontLabel = New-Object System.Drawing.Font('Consolas', 8.0)
$fontHead = New-Object System.Drawing.Font('Consolas', 12.0, [System.Drawing.FontStyle]::Bold)
$fmt = New-Object System.Drawing.StringFormat
$fmt.Alignment = [System.Drawing.StringAlignment]::Center

# Draws one grid sheet. $silhouette switches both the palette and the source.
function Draw-Sheet([bool]$silhouette) {
  if ($silhouette) { $bg = $SIL_BG; $cellBg = $SIL_BG; $rule = $SIL_RULE; $txt = $SIL_TEXT; $head = $SIL_INK }
  else { $bg = $SHEET_BG; $cellBg = $SHEET_CELL; $rule = $SHEET_RULE; $txt = $SHEET_TEXT; $head = $SHEET_HEAD }

  $pair = New-Canvas $sheetW $sheetH $bg
  $b = $pair[0]; $g = $pair[1]
  $brushTxt = New-Object System.Drawing.SolidBrush($txt)
  $brushHead = New-Object System.Drawing.SolidBrush($head)
  $brushCell = New-Object System.Drawing.SolidBrush($cellBg)
  $penRule = New-Object System.Drawing.Pen($rule)

  $y = $pad
  foreach ($grp in $grouped.Keys) {
    $items = @($grouped[$grp])
    $title = "$($grp.ToUpper())  ($($items.Count))"
    $g.DrawString($title, $fontHead, $brushHead, 4, ($y + 3))
    $g.DrawLine($penRule, 4, ($y + $headH - 4), ($sheetW - 4), ($y + $headH - 4))
    $y += $headH

    for ($i = 0; $i -lt $items.Count; $i++) {
      $col = $i % $cols
      $row = [math]::Floor($i / $cols)
      $cx = $pad + $col * $cellW
      $cy = $y + $row * $cellH
      $g.FillRectangle($brushCell, $cx, $cy, ($cellW - 2), ($cellH - 2))
      $g.DrawRectangle($penRule, $cx, $cy, ($cellW - 2), ($cellH - 2))

      $s = $items[$i]
      $src = $s.Bmp
      if ($silhouette) { $src = To-Silhouette $s.Bmp }
      # Bottom-aligned, horizontally centred: matches how BattleState anchors
      # sprites, so height differences between tiers read directly.
      $dx = $cx + $pad + [int](($artW - $s.W * $Zoom) / 2)
      $dy = $cy + $pad + ($artH - $s.H * $Zoom)
      Blit $g $src $dx $dy $Zoom
      if ($silhouette) { $src.Dispose() }

      $label = "$($s.Base)  $($s.W)x$($s.H)"
      $g.DrawString($label, $fontLabel, $brushTxt, `
        (New-Object System.Drawing.RectangleF($cx, ($cy + $pad * 2 + $artH - 2), ($cellW - 2), $labelH)), $fmt)
    }
    $y += ([math]::Ceiling($items.Count / [double]$cols)) * $cellH + $pad
  }
  $g.Dispose()
  return $b
}

$out = Join-Path $OutDir "${OutName}_contact.png"
$sheet = Draw-Sheet $false
$sheet.Save($out, [System.Drawing.Imaging.ImageFormat]::Png); $sheet.Dispose()
Write-Output "  $out"

$out = Join-Path $OutDir "${OutName}_silhouette.png"
$sheet = Draw-Sheet $true
$sheet.Save($out, [System.Drawing.Imaging.ImageFormat]::Png); $sheet.Dispose()
Write-Output "  $out"

# --- 1x strips: the native read at the real 426-wide native width ---
# Sprites are drawn at 1:1 with the game's own bottom-aligned anchoring, so
# this is exactly the size the player sees. Width is pinned to the native 426;
# height grows to fit every family rather than truncating the boss row.
$STRIP_W = 426
$fontTiny = New-Object System.Drawing.Font('Consolas', 7.0)

# Layout pass first, so the canvas is allocated at the height actually needed.
$placements = @()
$bandY = 4
foreach ($grp in $grouped.Keys) {
  $placements += [pscustomobject]@{ Kind = 'head'; Text = $grp.ToUpper(); X = 2; Y = $bandY }
  $rowY = $bandY + 10
  $x = 2
  foreach ($s in @($grouped[$grp])) {
    if ($x + $s.W -gt $STRIP_W - 2) { $x = 2; $rowY += $maxH + 4 }
    $placements += [pscustomobject]@{ Kind = 'sprite'; Sprite = $s; X = $x; Y = ($rowY + $maxH - $s.H) }
    $x += $s.W + 2
  }
  $rowY += $maxH + 4
  $placements += [pscustomobject]@{ Kind = 'rule'; X = 0; Y = ($rowY - 2) }
  $bandY = $rowY
}
$STRIP_H = [math]::Max(240, $bandY + 2)

$pairS = New-Canvas $STRIP_W $STRIP_H ([System.Drawing.ColorTranslator]::FromHtml('#1C1826'))
$bs = $pairS[0]; $gs = $pairS[1]
$brushHS = New-Object System.Drawing.SolidBrush($SHEET_HEAD)
$penS = New-Object System.Drawing.Pen($SHEET_RULE)
foreach ($pl in $placements) {
  switch ($pl.Kind) {
    'head'   { $gs.DrawString($pl.Text, $fontTiny, $brushHS, $pl.X, $pl.Y) }
    'sprite' { Blit $gs $pl.Sprite.Bmp $pl.X $pl.Y 1 }
    'rule'   { $gs.DrawLine($penS, 0, $pl.Y, $STRIP_W, $pl.Y) }
  }
}
$gs.Dispose()
$out = Join-Path $OutDir "${OutName}_strip1x.png"
$bs.Save($out, [System.Drawing.Imaging.ImageFormat]::Png)
Write-Output "  $out"

# The same pixels at 4x, so the native read is inspectable in a document.
$pair4 = New-Canvas ($STRIP_W * 4) ($STRIP_H * 4) `
  ([System.Drawing.ColorTranslator]::FromHtml('#1C1826'))
Blit $pair4[1] $bs 0 0 4
$pair4[1].Dispose()
$out = Join-Path $OutDir "${OutName}_strip4x.png"
$pair4[0].Save($out, [System.Drawing.Imaging.ImageFormat]::Png)
$pair4[0].Dispose(); $bs.Dispose()
Write-Output "  $out"

foreach ($s in $sprites) { $s.Bmp.Dispose() }
$fontLabel.Dispose(); $fontHead.Dispose(); $fontTiny.Dispose()

$counts = @()
foreach ($grp in $grouped.Keys) { $counts += ($grp + '=' + @($grouped[$grp]).Count) }
Write-Output ''
Write-Output ('Reviewed ' + @($sprites).Count + ' sprites - ' + ($counts -join ', '))
Write-Output ('Largest canvas: ' + $maxW + 'x' + $maxH)
