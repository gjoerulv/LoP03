# Crystal Dungeons — deterministic slice-texture generator (M15).
# Produces every generated PNG under assets/textures/ from the palette and
# shape rules in docs/art_bible.md. Rerunning reproduces identical files.
# Requires Windows PowerShell (System.Drawing). All output is original art.

$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing

$repo = Split-Path (Split-Path $PSScriptRoot -Parent) -Parent
$outRoot = Join-Path $repo 'assets\textures'

# --- Palette (art_bible.md §2) ---
$PAL = @{
  outline='#0E0C14'; night1='#12101A'; night2='#1C1826'; night3='#262233'
  stone1='#3A3646'; stone2='#4A4658'; stone3='#5C566B'; stone4='#736D82'
  earth1='#4A3B2A'; earth2='#6B5138'; earth3='#8A6D48'; earth4='#A98F63'
  veg0='#22371E'; veg1='#2A4432'; veg2='#3A5C40'; veg3='#4E7A50'
  wat0='#22304E'; wat1='#2A3C64'; wat2='#34506E'; wat3='#4A6A8A'
  cyan='#64E0DC'; violet='#9C6CE8'; glint='#C9A8F5'
  danger='#D85A5A'; gold='#E8D670'; heal='#8CD98C'
  clsKnight='#C0C6D0'; clsRanger='#4E9A50'; clsMage='#6C7CE8'
  clsCleric='#E8E2C8'; clsRogue='#8A5FB0'; clsGuardian='#C87E3A'
  maroon='#5C3038'; maroonD='#43242B'; bossBody='#3A2C4E'; bossD='#2A2038'
}
function C([string]$hex) { [System.Drawing.ColorTranslator]::FromHtml($hex) }

# Deterministic LCG so speckle is reproducible.
$script:rng = 424242
function Rnd { $script:rng = ($script:rng * 1103515245 + 12345) -band 0x7FFFFFFF; $script:rng / 2147483647.0 }

function New-Img([int]$w, [int]$h) {
  $bmp = New-Object System.Drawing.Bitmap($w, $h, [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
  return $bmp
}
function FR($bmp, [int]$x, [int]$y, [int]$w, [int]$h, [string]$hex) {
  $c = C $hex
  for ($j = $y; $j -lt $y + $h; $j++) { for ($i = $x; $i -lt $x + $w; $i++) {
    if ($i -ge 0 -and $j -ge 0 -and $i -lt $bmp.Width -and $j -lt $bmp.Height) { $bmp.SetPixel($i, $j, $c) } } }
}
function P($bmp, [int]$x, [int]$y, [string]$hex) {
  if ($x -ge 0 -and $y -ge 0 -and $x -lt $bmp.Width -and $y -lt $bmp.Height) { $bmp.SetPixel($x, $y, (C $hex)) }
}
function Speckle($bmp, [int]$x, [int]$y, [int]$w, [int]$h, [string]$hex, [double]$density) {
  for ($j = $y; $j -lt $y + $h; $j++) { for ($i = $x; $i -lt $x + $w; $i++) {
    if ((Rnd) -lt $density) { P $bmp $i $j $hex } } }
}
# 1px outside-outline around opaque pixels (props/actors only).
function Outline($bmp) {
  $w = $bmp.Width; $h = $bmp.Height; $mark = @()
  for ($j = 0; $j -lt $h; $j++) { for ($i = 0; $i -lt $w; $i++) {
    if ($bmp.GetPixel($i, $j).A -ne 0) { continue }
    $near = $false
    foreach ($d in @(@(-1,0),@(1,0),@(0,-1),@(0,1))) {
      $ni = $i + $d[0]; $nj = $j + $d[1]
      if ($ni -ge 0 -and $nj -ge 0 -and $ni -lt $w -and $nj -lt $h) {
        $p = $bmp.GetPixel($ni, $nj)
        if ($p.A -ne 0 -and $p.ToArgb() -ne (C $PAL.outline).ToArgb()) { $near = $true; break } } }
    if ($near) { $mark += ,@($i, $j) } } }
  foreach ($m in $mark) { P $bmp $m[0] $m[1] $PAL.outline }
}
function SaveImg($bmp, [string]$rel) {
  $path = Join-Path $outRoot $rel
  New-Item -ItemType Directory -Force (Split-Path $path -Parent) | Out-Null
  $bmp.Save($path, [System.Drawing.Imaging.ImageFormat]::Png)
  $bmp.Dispose()
  Write-Output "  $rel"
}

Write-Output 'Generating environment tiles...'

# --- Town tiles (16x16, opaque, no outer outline) ---
$b = New-Img 16 16; FR $b 0 0 16 16 $PAL.earth1; Speckle $b 0 0 16 16 '#57503C' 0.55; Speckle $b 0 0 16 16 $PAL.earth2 0.10
SaveImg $b 'environments/town_ground.png'

$b = New-Img 16 16; FR $b 0 0 16 16 $PAL.veg2; Speckle $b 0 0 16 16 $PAL.veg1 0.14
for ($i = 0; $i -lt 5; $i++) { $x = [int]((Rnd)*14)+1; $y = [int]((Rnd)*13)+1; P $b $x $y $PAL.veg3; P $b $x ($y+1) $PAL.veg1 }
SaveImg $b 'environments/town_grass.png'

$b = New-Img 16 16; FR $b 0 0 16 16 $PAL.earth2; Speckle $b 0 0 16 16 $PAL.earth3 0.12; Speckle $b 0 0 16 16 $PAL.earth1 0.08
FR $b 0 0 16 1 $PAL.earth1; SaveImg $b 'environments/town_path.png'

$b = New-Img 16 16; FR $b 0 0 16 16 $PAL.veg2; Speckle $b 0 0 16 16 $PAL.veg1 0.14
$g = [System.Drawing.Graphics]::FromImage($b); $g.SmoothingMode = 'None'
$g.FillEllipse((New-Object System.Drawing.SolidBrush(C $PAL.veg0)), 1, 0, 14, 12)
$g.FillEllipse((New-Object System.Drawing.SolidBrush(C $PAL.veg1)), 3, 1, 9, 8); $g.Dispose()
P $b 4 2 $PAL.veg3; P $b 5 2 $PAL.veg3; P $b 4 3 $PAL.veg3
FR $b 7 12 2 3 $PAL.earth1; P $b 7 12 $PAL.earth2
SaveImg $b 'environments/town_tree.png'

$b = New-Img 16 16
for ($j = 0; $j -lt 16; $j++) { $hex = if (($j % 8) -lt 4) { $PAL.wat1 } else { $PAL.wat0 }; FR $b 0 $j 16 1 $hex }
foreach ($r in @(@(2,3),@(9,6),@(5,11),@(12,13))) { FR $b $r[0] $r[1] 3 1 $PAL.wat2; P $b ($r[0]+1) $r[1] $PAL.wat3 }
SaveImg $b 'environments/town_water.png'

$b = New-Img 16 16
for ($row = 0; $row -lt 4; $row++) {
  $y = $row * 4; FR $b 0 $y 16 4 $PAL.stone2; FR $b 0 ($y+3) 16 1 $PAL.night3
  $off = if ($row % 2 -eq 0) { 0 } else { 4 }
  for ($x = $off; $x -lt 16; $x += 8) { FR $b $x $y 1 3 $PAL.night3 }
  Speckle $b 0 $y 16 3 $PAL.stone3 0.08
}
FR $b 0 0 16 1 $PAL.stone4; SaveImg $b 'environments/town_building.png'

$b = New-Img 16 16; FR $b 0 0 16 16 $PAL.earth1
for ($x = 2; $x -lt 14; $x += 3) { FR $b $x 1 2 14 $PAL.earth2; FR $b ($x+2) 1 1 14 $PAL.earth1 }
FR $b 2 1 12 1 $PAL.earth3; P $b 11 8 $PAL.gold; SaveImg $b 'environments/town_door.png'

# --- Ruined Keep tiles ---
$b = New-Img 16 16; FR $b 0 0 16 16 $PAL.stone2
FR $b 0 7 16 1 $PAL.night3; FR $b 7 0 1 8 $PAL.night3; FR $b 11 8 1 8 $PAL.night3
Speckle $b 0 0 16 16 $PAL.stone1 0.10; Speckle $b 0 0 16 16 $PAL.stone3 0.06
P $b 3 3 $PAL.night3; P $b 4 4 $PAL.night3; P $b 5 4 $PAL.night3
SaveImg $b 'environments/keep_floor.png'

$b = New-Img 16 16
for ($row = 0; $row -lt 4; $row++) {
  $y = $row * 4; FR $b 0 $y 16 4 $PAL.stone1; FR $b 0 ($y+3) 16 1 $PAL.night2
  $off = if ($row % 2 -eq 0) { 2 } else { 6 }
  for ($x = $off; $x -lt 16; $x += 8) { FR $b $x $y 1 3 $PAL.night2 }
  Speckle $b 0 $y 16 3 '#322E3E' 0.12
}
FR $b 0 0 16 1 $PAL.stone2; SaveImg $b 'environments/keep_wall.png'

$b = New-Img 16 16; FR $b 0 0 16 16 $PAL.stone1
FR $b 4 0 8 16 $PAL.night1; FR $b 3 0 1 16 $PAL.stone2; FR $b 12 0 1 16 $PAL.stone2
P $b 4 15 $PAL.stone1; P $b 5 14 $PAL.stone1; P $b 10 15 $PAL.stone1; P $b 6 15 $PAL.stone2
SaveImg $b 'environments/keep_door.png'

Write-Output 'Generating props and overworld actor...'

# --- Player (12x12, hooded traveler, gold clasp) ---
$b = New-Img 12 12
FR $b 4 1 4 3 $PAL.night3; FR $b 4 3 4 1 $PAL.night1            # hood + face shadow
FR $b 3 4 6 5 $PAL.earth1; FR $b 3 4 6 1 $PAL.earth2            # cloak
P $b 5 5 $PAL.gold; P $b 6 5 $PAL.gold                          # clasp
FR $b 4 9 2 2 $PAL.night3; FR $b 6 9 2 2 $PAL.night3            # legs
Outline $b; SaveImg $b 'actors/player_overworld.png'

# --- Chest (12x12) ---
$b = New-Img 12 12
FR $b 1 4 10 6 $PAL.earth2; FR $b 1 4 10 1 $PAL.earth3          # body + lid edge
FR $b 1 2 10 2 $PAL.earth3; FR $b 1 2 10 1 $PAL.earth4          # lid
FR $b 5 2 2 8 $PAL.gold; P $b 5 6 $PAL.earth1; P $b 6 6 $PAL.earth1  # band + lock
Outline $b; SaveImg $b 'props/chest.png'

# --- Gate marker (12x12, crossed blades on danger disc) ---
$b = New-Img 12 12
$g = [System.Drawing.Graphics]::FromImage($b); $g.SmoothingMode = 'None'
$g.FillEllipse((New-Object System.Drawing.SolidBrush(C $PAL.danger)), 1, 1, 10, 10)
$g.FillEllipse((New-Object System.Drawing.SolidBrush(C '#B24040')), 2, 2, 8, 8); $g.Dispose()
for ($i = 0; $i -lt 6; $i++) { P $b (3+$i) (3+$i) $PAL.clsKnight; P $b (8-$i) (3+$i) $PAL.clsKnight }
Outline $b; SaveImg $b 'props/gate_marker.png'

# --- Boss marker (12x12, crowned skull on violet disc) ---
$b = New-Img 12 12
$g = [System.Drawing.Graphics]::FromImage($b); $g.SmoothingMode = 'None'
$g.FillEllipse((New-Object System.Drawing.SolidBrush(C $PAL.violet)), 1, 1, 10, 10)
$g.FillEllipse((New-Object System.Drawing.SolidBrush(C '#7B50BC')), 2, 2, 8, 8); $g.Dispose()
foreach ($x in @(3,5,7)) { P $b $x 1 $PAL.gold; P $b $x 2 $PAL.gold }
FR $b 4 5 4 3 $PAL.clsCleric; P $b 5 6 $PAL.night1; P $b 7 6 $PAL.night1
Outline $b; SaveImg $b 'props/boss_marker.png'

Write-Output 'Generating battle sprites...'

# Shared humanoid base (24x24), then class add-ons. $mirror=true faces right.
function New-Humanoid([string]$helmHex, [string]$torsoHex, [string]$accent) {
  $b = New-Img 24 24
  FR $b 9 3 6 5 $helmHex; FR $b 9 6 6 1 $PAL.night1               # head + face shadow
  FR $b 8 8 8 8 $torsoHex; FR $b 8 8 8 1 $accent                  # torso + trim
  FR $b 7 9 1 5 $torsoHex; FR $b 16 9 1 5 $torsoHex               # arms
  FR $b 9 16 2 5 $PAL.night3; FR $b 13 16 2 5 $PAL.night3         # legs
  FR $b 8 21 3 1 $PAL.night1; FR $b 13 21 3 1 $PAL.night1         # boots
  return $b
}
function Save-Actor($b, [string]$name) { Outline $b; SaveImg $b "actors/$name.png" }

$b = New-Humanoid $PAL.clsKnight $PAL.stone3 $PAL.clsKnight       # Knight: blade + shield
FR $b 3 6 2 11 $PAL.clsKnight; FR $b 2 8 4 1 $PAL.earth3; FR $b 17 10 4 6 $PAL.stone2; FR $b 17 10 4 1 $PAL.clsKnight
Save-Actor $b 'knight_battle'

$b = New-Humanoid $PAL.veg1 $PAL.veg2 $PAL.clsRanger              # Ranger: bow
for ($j = 5; $j -le 17; $j++) { $x = 4 - [int][math]::Round(2*[math]::Sin(($j-5)/12.0*[math]::PI)); P $b $x $j $PAL.earth2 }
for ($j = 5; $j -le 17; $j++) { P $b 5 $j $PAL.earth4 }
Save-Actor $b 'ranger_battle'

$b = New-Humanoid $PAL.clsMage $PAL.night3 $PAL.clsMage           # Mage: hat brim + staff
FR $b 7 2 10 1 $PAL.clsMage; FR $b 10 0 4 3 $PAL.clsMage
FR $b 4 4 2 15 $PAL.earth2; FR $b 4 2 2 2 $PAL.cyan
Save-Actor $b 'mage_battle'

$b = New-Humanoid $PAL.clsCleric $PAL.clsCleric $PAL.gold         # Cleric: rod
FR $b 4 6 2 13 $PAL.earth3; $g=[System.Drawing.Graphics]::FromImage($b); $g.SmoothingMode='None'
$g.FillEllipse((New-Object System.Drawing.SolidBrush(C $PAL.gold)), 2, 2, 5, 5); $g.Dispose()
Save-Actor $b 'cleric_battle'

$b = New-Humanoid $PAL.night3 $PAL.night2 $PAL.clsRogue           # Rogue: scarf + daggers
FR $b 8 7 8 2 $PAL.clsRogue; FR $b 4 10 2 5 $PAL.clsKnight; FR $b 18 12 2 4 $PAL.clsKnight
Save-Actor $b 'rogue_battle'

$b = New-Humanoid $PAL.clsGuardian $PAL.stone2 $PAL.clsGuardian   # Guardian: tower shield
FR $b 2 5 5 14 $PAL.clsGuardian; FR $b 3 6 3 12 $PAL.earth3; FR $b 4 8 1 8 $PAL.gold
Save-Actor $b 'guardian_battle'

# Enemies face right.
$b = New-Img 24 24                                                # Normal: hunched beast
$g = [System.Drawing.Graphics]::FromImage($b); $g.SmoothingMode = 'None'
$g.FillEllipse((New-Object System.Drawing.SolidBrush(C $PAL.maroon)), 3, 8, 17, 12)
$g.FillEllipse((New-Object System.Drawing.SolidBrush(C $PAL.maroonD)), 4, 12, 14, 8)
$g.FillEllipse((New-Object System.Drawing.SolidBrush(C $PAL.maroon)), 14, 4, 8, 8); $g.Dispose()
P $b 19 7 $PAL.danger; P $b 17 7 $PAL.danger
FR $b 6 19 3 2 $PAL.maroonD; FR $b 13 19 3 2 $PAL.maroonD
P $b 20 10 $PAL.earth4; P $b 21 11 $PAL.earth4
Outline $b; SaveImg $b 'enemies/normal_battle.png'

$b = New-Img 24 24                                                # Elite: horned, violet
$g = [System.Drawing.Graphics]::FromImage($b); $g.SmoothingMode = 'None'
$g.FillEllipse((New-Object System.Drawing.SolidBrush(C $PAL.maroon)), 2, 7, 18, 13)
$g.FillEllipse((New-Object System.Drawing.SolidBrush(C $PAL.maroonD)), 3, 11, 15, 9)
$g.FillEllipse((New-Object System.Drawing.SolidBrush(C $PAL.maroon)), 14, 3, 9, 9); $g.Dispose()
P $b 19 6 $PAL.danger; P $b 17 6 $PAL.danger
FR $b 15 1 1 3 $PAL.glint; FR $b 20 1 1 3 $PAL.glint
FR $b 5 9 6 1 $PAL.violet; FR $b 4 13 7 1 $PAL.violet
FR $b 5 19 3 2 $PAL.maroonD; FR $b 13 19 3 2 $PAL.maroonD
Outline $b; SaveImg $b 'enemies/elite_battle.png'

$b = New-Img 32 32                                                # Boss: crowned crystal hulk
$g = [System.Drawing.Graphics]::FromImage($b); $g.SmoothingMode = 'None'
$g.FillEllipse((New-Object System.Drawing.SolidBrush(C $PAL.bossBody)), 3, 8, 26, 22)
$g.FillEllipse((New-Object System.Drawing.SolidBrush(C $PAL.bossD)), 5, 14, 21, 15)
$g.FillEllipse((New-Object System.Drawing.SolidBrush(C $PAL.bossBody)), 17, 2, 12, 12); $g.Dispose()
foreach ($x in @(19,22,25)) { P $b $x 1 $PAL.gold; P $b $x 2 $PAL.gold }
P $b 25 7 $PAL.danger; P $b 22 7 $PAL.danger
FR $b 6 10 2 5 $PAL.violet; P $b 6 9 $PAL.glint
FR $b 10 8 2 6 $PAL.cyan; P $b 10 7 $PAL.glint
FR $b 8 28 5 2 $PAL.bossD; FR $b 19 28 5 2 $PAL.bossD
Outline $b; SaveImg $b 'enemies/boss_battle.png'

Write-Output 'Generating UI...'

# --- Nine-patch frame (24x24, 8px borders, transparent center) ---
$b = New-Img 24 24
FR $b 0 0 24 1 $PAL.outline; FR $b 0 23 24 1 $PAL.outline; FR $b 0 0 1 24 $PAL.outline; FR $b 23 0 1 24 $PAL.outline
FR $b 1 1 22 2 $PAL.stone2; FR $b 1 21 22 2 $PAL.stone2; FR $b 1 1 2 22 $PAL.stone2; FR $b 21 1 2 22 $PAL.stone2
FR $b 1 1 22 1 $PAL.stone3; FR $b 1 1 1 22 $PAL.stone3
FR $b 3 3 18 1 $PAL.night3; FR $b 3 20 18 1 $PAL.night3; FR $b 3 3 1 18 $PAL.night3; FR $b 20 3 1 18 $PAL.night3
foreach ($c in @(@(1,1),@(21,1),@(1,21),@(21,21))) { P $b ($c[0]+1) ($c[1]+1) $PAL.cyan }
SaveImg $b 'ui/frame_default.png'

# --- Crystal emblem (32x32) ---
$b = New-Img 32 32
$g = [System.Drawing.Graphics]::FromImage($b); $g.SmoothingMode = 'None'
$violet = New-Object System.Drawing.SolidBrush(C $PAL.violet)
$glintB = New-Object System.Drawing.SolidBrush(C $PAL.glint)
$cyanB  = New-Object System.Drawing.SolidBrush(C $PAL.cyan)
$pts = [System.Drawing.Point[]]@((New-Object System.Drawing.Point(16,2)),(New-Object System.Drawing.Point(21,14)),(New-Object System.Drawing.Point(16,27)),(New-Object System.Drawing.Point(11,14)))
$g.FillPolygon($violet, $pts)
$ptsL = [System.Drawing.Point[]]@((New-Object System.Drawing.Point(16,2)),(New-Object System.Drawing.Point(16,27)),(New-Object System.Drawing.Point(11,14)))
$g.FillPolygon($glintB, $ptsL)
$ptsS1 = [System.Drawing.Point[]]@((New-Object System.Drawing.Point(8,12)),(New-Object System.Drawing.Point(11,20)),(New-Object System.Drawing.Point(8,26)),(New-Object System.Drawing.Point(5,20)))
$g.FillPolygon($cyanB, $ptsS1)
$ptsS2 = [System.Drawing.Point[]]@((New-Object System.Drawing.Point(24,10)),(New-Object System.Drawing.Point(27,19)),(New-Object System.Drawing.Point(24,26)),(New-Object System.Drawing.Point(21,19)))
$g.FillPolygon($cyanB, $ptsS2)
$g.Dispose()
FR $b 6 26 20 3 $PAL.stone1; FR $b 8 25 16 1 $PAL.stone2
Outline $b; SaveImg $b 'ui/emblem_crystal.png'

Write-Output 'Generating M17 exploration art...'

# --- Player walk sheet (36x48: 3 frames x 4 rows = down, up, left, right) ---
# Frame 0 = stand (the static player_overworld pose), 1/2 = alternating steps.
function Draw-PlayerFrame($bmp, [int]$ox, [int]$oy, [string]$dir, [int]$step) {
  FR $bmp ($ox+4) ($oy+1) 4 3 $PAL.night3                          # hood
  if ($dir -ne 'up') { FR $bmp ($ox+4) ($oy+3) 4 1 $PAL.night1 }   # face shadow
  FR $bmp ($ox+3) ($oy+4) 6 5 $PAL.earth1; FR $bmp ($ox+3) ($oy+4) 6 1 $PAL.earth2  # cloak
  switch ($dir) {                                                  # gold clasp per facing
    'down'  { P $bmp ($ox+5) ($oy+5) $PAL.gold; P $bmp ($ox+6) ($oy+5) $PAL.gold }
    'left'  { P $bmp ($ox+4) ($oy+5) $PAL.gold }
    'right' { P $bmp ($ox+7) ($oy+5) $PAL.gold }
  }
  switch ($step) {                                                 # legs: stand / step A / step B
    0 { FR $bmp ($ox+4) ($oy+9) 2 2 $PAL.night3; FR $bmp ($ox+6) ($oy+9) 2 2 $PAL.night3 }
    1 { FR $bmp ($ox+4) ($oy+9) 2 2 $PAL.night3; FR $bmp ($ox+6) ($oy+9) 2 1 $PAL.night3 }
    2 { FR $bmp ($ox+4) ($oy+9) 2 1 $PAL.night3; FR $bmp ($ox+6) ($oy+9) 2 2 $PAL.night3 }
  }
}
$b = New-Img 36 48
$rows = @('down','up','left','right')
for ($r = 0; $r -lt 4; $r++) { for ($f = 0; $f -lt 3; $f++) {
  Draw-PlayerFrame $b ($f*12) ($r*12) $rows[$r] $f } }
Outline $b; SaveImg $b 'actors/player_walk.png'

# --- Overworld enemy silhouettes (12x12; shape encodes tier, never color alone) ---
$b = New-Img 12 12                                                # normal: hunched beast
$g = [System.Drawing.Graphics]::FromImage($b); $g.SmoothingMode = 'None'
$g.FillEllipse((New-Object System.Drawing.SolidBrush(C $PAL.maroon)), 1, 4, 9, 6)
$g.FillEllipse((New-Object System.Drawing.SolidBrush(C $PAL.maroon)), 6, 2, 5, 5); $g.Dispose()
P $b 9 4 $PAL.danger; P $b 7 4 $PAL.danger
FR $b 2 9 2 2 $PAL.maroonD; FR $b 6 9 2 2 $PAL.maroonD
Outline $b; SaveImg $b 'props/enemy_normal.png'

$b = New-Img 12 12                                                # elite: horned + banded
$g = [System.Drawing.Graphics]::FromImage($b); $g.SmoothingMode = 'None'
$g.FillEllipse((New-Object System.Drawing.SolidBrush(C $PAL.maroon)), 1, 4, 10, 7)
$g.FillEllipse((New-Object System.Drawing.SolidBrush(C $PAL.maroon)), 6, 2, 5, 5); $g.Dispose()
P $b 7 1 $PAL.glint; P $b 10 1 $PAL.glint                          # horns
P $b 9 4 $PAL.danger; P $b 7 4 $PAL.danger
FR $b 2 6 4 1 $PAL.violet                                          # war-band
FR $b 2 10 2 1 $PAL.maroonD; FR $b 7 10 2 1 $PAL.maroonD
Outline $b; SaveImg $b 'props/enemy_elite.png'

$b = New-Img 12 12                                                # boss: tall, crowned
FR $b 3 3 6 8 $PAL.bossBody; FR $b 4 4 4 6 $PAL.bossD
foreach ($x in @(4,6,8)) { P $b $x 1 $PAL.gold }; FR $b 4 2 5 1 $PAL.gold
P $b 5 5 $PAL.danger; P $b 7 5 $PAL.danger
FR $b 3 11 2 1 $PAL.night1; FR $b 7 11 2 1 $PAL.night1
Outline $b; SaveImg $b 'props/enemy_boss.png'

# --- Crystal Mine tiles: braced rock, luminous mineral seams ---
$b = New-Img 16 16; FR $b 0 0 16 16 $PAL.night2
Speckle $b 0 0 16 16 $PAL.night3 0.30; Speckle $b 0 0 16 16 $PAL.stone1 0.10
P $b 4 11 $PAL.cyan; P $b 12 3 $PAL.wat3
SaveImg $b 'environments/mine_floor.png'

$b = New-Img 16 16; FR $b 0 0 16 16 $PAL.stone1                    # rock + timber brace
Speckle $b 0 0 16 16 $PAL.night2 0.25; Speckle $b 0 0 16 16 $PAL.stone2 0.12
FR $b 0 0 16 3 $PAL.earth2; FR $b 0 2 16 1 $PAL.earth1             # beam
FR $b 0 3 2 13 $PAL.earth1; FR $b 14 3 2 13 $PAL.earth1            # posts
P $b 6 8 $PAL.cyan; P $b 10 12 $PAL.violet                          # embedded minerals
SaveImg $b 'environments/mine_wall.png'

$b = New-Img 16 16; FR $b 0 0 16 16 $PAL.stone1                    # timber-framed opening
FR $b 4 0 8 16 $PAL.night1
FR $b 2 0 2 16 $PAL.earth2; FR $b 12 0 2 16 $PAL.earth2            # posts
FR $b 2 0 12 2 $PAL.earth3                                          # lintel
P $b 7 13 $PAL.cyan
SaveImg $b 'environments/mine_door.png'

$b = New-Img 16 16; FR $b 0 0 16 16 $PAL.night2                    # accent: crystal cluster
Speckle $b 0 0 16 16 $PAL.night3 0.30
FR $b 6 6 2 7 $PAL.cyan; P $b 6 5 $PAL.glint                        # main shard
FR $b 9 8 2 5 $PAL.violet; P $b 9 7 $PAL.glint
FR $b 4 9 1 4 $PAL.wat3
FR $b 4 13 8 1 $PAL.stone1                                          # rubble base
SaveImg $b 'environments/mine_crystals.png'

# --- Hollow Forest tiles: mossy litter, root-mass walls ---
$b = New-Img 16 16; FR $b 0 0 16 16 $PAL.veg0
Speckle $b 0 0 16 16 $PAL.veg1 0.35; Speckle $b 0 0 16 16 $PAL.earth1 0.08
P $b 3 5 $PAL.veg3; P $b 11 12 $PAL.veg2
SaveImg $b 'environments/forest_floor.png'

$b = New-Img 16 16; FR $b 0 0 16 16 $PAL.earth1                    # trunk/root mass
for ($x = 1; $x -lt 16; $x += 5) { FR $b $x 0 2 16 $PAL.earth2; FR $b ($x+2) 0 1 16 '#3A2E20' }
FR $b 0 0 16 2 $PAL.veg1; Speckle $b 0 0 16 2 $PAL.veg2 0.30       # moss cap
Speckle $b 0 10 16 6 '#3A2E20' 0.15
SaveImg $b 'environments/forest_wall.png'

$b = New-Img 16 16; FR $b 0 0 16 16 $PAL.earth1                    # root archway
FR $b 4 0 8 16 $PAL.night1
for ($j = 0; $j -lt 5; $j++) { P $b (4+$j) $j $PAL.earth2; P $b (11-$j) $j $PAL.earth2 }  # arch roots
FR $b 3 0 1 16 $PAL.earth2; FR $b 12 0 1 16 $PAL.earth2
P $b 5 14 $PAL.veg1; P $b 10 15 $PAL.veg1
SaveImg $b 'environments/forest_door.png'

$b = New-Img 16 16; FR $b 0 0 16 16 $PAL.veg0                      # accent: mossy shrine stone
Speckle $b 0 0 16 16 $PAL.veg1 0.35
FR $b 5 5 6 7 $PAL.stone2; FR $b 5 5 6 1 $PAL.stone3               # stone
FR $b 4 12 8 1 $PAL.stone1                                          # base
P $b 7 7 $PAL.cyan; P $b 8 8 $PAL.cyan                              # carved sigil
P $b 5 5 $PAL.veg2; P $b 10 11 $PAL.veg2                            # moss creep
SaveImg $b 'environments/forest_shrine.png'

# --- Accents for existing themes ---
$b = New-Img 16 16; FR $b 0 0 16 16 $PAL.stone2                    # keep: collapsed rubble
FR $b 0 7 16 1 $PAL.night3; Speckle $b 0 0 16 16 $PAL.stone1 0.10
FR $b 4 8 4 3 $PAL.stone1; FR $b 9 6 3 2 $PAL.stone1; FR $b 7 11 5 2 $PAL.stone1
P $b 5 8 $PAL.stone3; P $b 10 6 $PAL.stone3; P $b 8 11 $PAL.stone3
SaveImg $b 'environments/keep_rubble.png'

$b = New-Img 16 16; FR $b 0 0 16 16 $PAL.veg2                      # town: flower patch
Speckle $b 0 0 16 16 $PAL.veg1 0.14
foreach ($f in @(@(3,4,'gold'),@(10,7,'glint'),@(6,11,'danger'))) {
  P $b $f[0] $f[1] $PAL[$f[2]]; P $b $f[0] ($f[1]+1) $PAL.veg3 }
SaveImg $b 'environments/town_flowers.png'

# --- Facing brackets (32x16: 2 frames of 16x16; tight then loose pulse) ---
$b = New-Img 32 16
function Draw-Brackets($bmp, [int]$ox, [int]$inset) {
  $lo = $inset; $hi = 15 - $inset
  foreach ($c in @(@($lo,$lo,1,1),@($hi,$lo,-1,1),@($lo,$hi,1,-1),@($hi,$hi,-1,-1))) {
    $x = $c[0]; $y = $c[1]; $dx = $c[2]; $dy = $c[3]
    for ($i = 0; $i -lt 4; $i++) {
      P $bmp ($ox + $x + $dx*$i) ($y + $dy) $PAL.night1              # shadow
      P $bmp ($ox + $x + $dx*$i) $y $PAL.gold                        # arm horizontal
      P $bmp ($ox + $x) ($y + $dy*$i) $PAL.gold                      # arm vertical
    }
  }
}
Draw-Brackets $b 0 1
Draw-Brackets $b 16 0
SaveImg $b 'ui/facing_brackets.png'

Write-Output 'Texture generation complete.'
