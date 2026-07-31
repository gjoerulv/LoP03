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
  # M73 ramp completions (art_bible §2). The flesh and boss-void ramps shipped
  # from M26 with only two steps each, and the neutral white pair arrived
  # undocumented with the M62 geese — none of them could carry the bible's
  # mandated 3-band shading. The darkest/highlight steps below complete them;
  # `flesh1`/`flesh2` and `void1`/`void2` are the existing maroon/boss values
  # under ramp names, so nothing already shipped changes hue.
  flesh0='#2E1820'; flesh1='#43242B'; flesh2='#5C3038'; flesh3='#7A4650'
  void0='#1E1628';  void1='#2A2038';  void2='#3A2C4E';  void3='#4E3C68'
  white0='#B8B8BC'; white1='#D8D8D4'; white2='#F2F2F0'
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

# --- M45 unlockable classes (silhouette-distinct: none of them is a humanoid
# --- outline you could mistake for the six originals) ---
$b = New-Img 24 24                                                # Dragon: horned wyrm
$g = [System.Drawing.Graphics]::FromImage($b); $g.SmoothingMode = 'None'
$g.FillEllipse((New-Object System.Drawing.SolidBrush(C $PAL.veg2)), 3, 9, 16, 11)   # body
$g.FillEllipse((New-Object System.Drawing.SolidBrush(C $PAL.veg1)), 2, 3, 9, 8); $g.Dispose()  # head
FR $b 2 1 2 3 $PAL.stone3; FR $b 8 1 2 3 $PAL.stone3               # horns
P $b 4 6 $PAL.danger; P $b 8 6 $PAL.danger                         # eyes
FR $b 12 4 3 6 $PAL.veg1; FR $b 15 6 4 5 $PAL.veg2                 # wing
FR $b 5 20 3 2 $PAL.night1; FR $b 13 20 3 2 $PAL.night1            # claws
Outline $b; SaveImg $b 'actors/dragon_battle.png'

$b = New-Humanoid $PAL.clsRogue $PAL.violet $PAL.gold              # Jester: belled cap
FR $b 7 2 10 1 $PAL.violet
FR $b 6 0 3 3 $PAL.clsRogue; FR $b 15 0 3 3 $PAL.gold              # two drooping points
P $b 6 3 $PAL.gold; P $b 17 3 $PAL.gold                            # bells
FR $b 9 9 2 6 $PAL.gold; FR $b 13 9 2 6 $PAL.gold                  # motley stripes
FR $b 4 12 3 3 $PAL.gold                                           # a juggled something
Outline $b; SaveImg $b 'actors/jester_battle.png'

$b = New-Img 24 24                                                # Goose: goose.
$g = [System.Drawing.Graphics]::FromImage($b); $g.SmoothingMode = 'None'
$g.FillEllipse((New-Object System.Drawing.SolidBrush(C '#F2F2F0')), 4, 11, 15, 10)  # body
$g.FillEllipse((New-Object System.Drawing.SolidBrush(C '#F2F2F0')), 5, 2, 7, 7); $g.Dispose()  # head
FR $b 8 8 3 5 '#F2F2F0'                                            # neck
FR $b 2 4 4 2 $PAL.gold                                            # beak
P $b 8 4 $PAL.night1                                               # eye
FR $b 12 14 5 3 '#D8D8D4'                                          # wing
FR $b 7 21 2 2 $PAL.gold; FR $b 12 21 2 2 $PAL.gold                # feet
Outline $b; SaveImg $b 'actors/goose_battle.png'

# Enemy and boss battle sprites (including the three generic tier fallbacks)
# are authored as explicit pixel grids in the M73 section further down.

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

Write-Output 'Generating M20 event props...'

# --- Event props (12x12; shape-distinct so kind reads without color) ---
$b = New-Img 12 12                                                # shrine: stone + sigil
FR $b 3 3 6 7 $PAL.stone2; FR $b 3 3 6 1 $PAL.stone3
FR $b 2 10 8 1 $PAL.stone1
P $b 5 5 $PAL.cyan; P $b 6 6 $PAL.cyan; P $b 5 7 $PAL.cyan
Outline $b; SaveImg $b 'props/event_shrine.png'

$b = New-Img 12 12                                                # spring: pool + glints
$g = [System.Drawing.Graphics]::FromImage($b); $g.SmoothingMode = 'None'
$g.FillEllipse((New-Object System.Drawing.SolidBrush(C $PAL.wat1)), 1, 3, 10, 7)
$g.FillEllipse((New-Object System.Drawing.SolidBrush(C $PAL.wat2)), 2, 4, 8, 5); $g.Dispose()
P $b 4 5 $PAL.wat3; P $b 7 6 $PAL.wat3; P $b 5 7 $PAL.glint
Outline $b; SaveImg $b 'props/event_spring.png'

$b = New-Img 12 12                                                # merchant: pack figure
FR $b 4 2 4 3 $PAL.earth3; FR $b 4 4 4 1 $PAL.night1              # hood + face
FR $b 3 5 6 4 $PAL.earth2                                          # coat
FR $b 8 4 3 6 $PAL.earth1; P $b 9 5 $PAL.gold                      # pack + buckle
FR $b 4 9 2 2 $PAL.night3; FR $b 6 9 2 2 $PAL.night3
Outline $b; SaveImg $b 'props/event_merchant.png'

$b = New-Img 12 12                                                # totem: carved post
FR $b 4 1 4 10 $PAL.earth2; FR $b 4 1 4 1 $PAL.earth3
FR $b 5 3 2 1 $PAL.danger; FR $b 5 6 2 1 $PAL.danger; FR $b 5 9 2 1 $PAL.danger
FR $b 3 10 6 1 $PAL.earth1
Outline $b; SaveImg $b 'props/event_totem.png'

$b = New-Img 12 12                                                # omen: floating orb
$g = [System.Drawing.Graphics]::FromImage($b); $g.SmoothingMode = 'None'
$g.FillEllipse((New-Object System.Drawing.SolidBrush(C $PAL.violet)), 2, 1, 8, 8)
$g.FillEllipse((New-Object System.Drawing.SolidBrush(C '#7B50BC')), 3, 2, 6, 6); $g.Dispose()
P $b 4 3 $PAL.glint; P $b 6 5 $PAL.glint
FR $b 4 10 4 1 $PAL.night3                                         # shadow
Outline $b; SaveImg $b 'props/event_omen.png'

$b = New-Img 12 12                                                # rest: campfire (M30)
$g = [System.Drawing.Graphics]::FromImage($b); $g.SmoothingMode = 'None'
$g.FillPolygon((New-Object System.Drawing.SolidBrush(C $PAL.gold)), [System.Drawing.Point[]]@((New-Object System.Drawing.Point(6, 2)), (New-Object System.Drawing.Point(9, 7)), (New-Object System.Drawing.Point(3, 7)))); $g.Dispose()
FR $b 5 4 2 3 $PAL.danger                                          # flame core
P $b 6 3 $PAL.glint
FR $b 2 8 8 2 $PAL.earth2; FR $b 3 9 6 1 $PAL.earth1               # logs
Outline $b; SaveImg $b 'props/event_rest.png'

$b = New-Img 12 12                                                # relic: reliquary casket (M44)
FR $b 2 5 8 5 $PAL.earth2; FR $b 2 5 8 1 $PAL.earth1               # chest body + rim
FR $b 3 2 6 3 $PAL.gold; FR $b 4 3 4 1 $PAL.glint                  # domed gold lid
P $b 5 7 $PAL.glint; P $b 6 7 $PAL.glint                           # clasp
FR $b 1 10 10 1 $PAL.night3                                        # shadow
Outline $b; SaveImg $b 'props/event_relic.png'

# ===================== M73 enemy & boss battle sprites =====================
# Every enemy and boss battle sprite is authored here as an explicit pixel
# grid: one ASCII block per sprite, one character per pixel, keyed to the
# art_bible §2 ramps by the table below. Rows are directly readable and
# surgically editable in a diff, and `tools/asset_gen/preview.ps1` renders
# contact + silhouette sheets so the result is reviewed, never assumed.
#
# Conventions (art_bible §3/§4/§5):
#   * enemies face RIGHT; normal/elite 24x24, bosses 36x36.
#   * grids leave the outer ring clear so `Outline` can trace a full 1px
#     #0E0C14 outline; art that deliberately touches an edge loses it there.
#   * light source top-left: highlight band up-left, shadow band down-right.
#   * danger tier is carried by shape and size, never by hue alone.
#
# BOSS CANVAS (36x36, not the 36x44 first considered). `BattleState::drawUnit`
# anchors bottom-centre at `sy = enemyBaseY() + 16 - tex.height`, and
# `enemyBaseY()` drops to 20 once a fight fields 5+ enemies. Three authored
# boss teams do exactly that — rush_tyrant and abyssal_tyrant (4 minions, 5
# units) and deadly_duck (5 minions, 6 units) — so the tallest sprite that
# never clips off the top of the screen is 20 + 16 = 36 rows. Width is bounded
# by the 40px unit footprint (x 36..76 at native 426x240); 36 wide spans
# 38..74, clear of the HP meter edge and of the enemy status column at x+44.
# Going taller than 36 needs an `enemyBaseY()` change — an owner call, not a
# generator one.
#
# DETERMINISM: this section calls NO random helper. Every speckle pixel is
# placed by hand in its grid, so `$script:rng` is untouched here and adding,
# removing or reordering a sprite can never shift another file's bytes. That
# retires the failure mode the M49 note warned about, where one stray `Speckle`
# re-rolled every sprite generated after it.
Write-Output 'Generating enemy and boss battle sprites (M73 pixel grids)...'

# Palette key. Lower-case letters walk a ramp dark -> light; upper-case are the
# accent and signal colours. '.' is transparent.
$GRIDC = New-Object 'System.Collections.Generic.Dictionary[char,System.Drawing.Color]'
function GridKey([char]$ch, [string]$hex) { $GRIDC[$ch] = (C $hex) }
GridKey 'K' $PAL.outline                                    # ink / deep shadow
GridKey '1' $PAL.night1;  GridKey '2' $PAL.night2;  GridKey '3' $PAL.night3
GridKey 'q' $PAL.stone1;  GridKey 'w' $PAL.stone2
GridKey 'e' $PAL.stone3;  GridKey 'r' $PAL.stone4
GridKey 'a' $PAL.earth1;  GridKey 's' $PAL.earth2
GridKey 'd' $PAL.earth3;  GridKey 'f' $PAL.earth4
GridKey 'z' $PAL.veg0;    GridKey 'x' $PAL.veg1
GridKey 'c' $PAL.veg2;    GridKey 'v' $PAL.veg3
GridKey 'u' $PAL.wat0;    GridKey 'i' $PAL.wat1
GridKey 'o' $PAL.wat2;    GridKey 'p' $PAL.wat3
GridKey 't' $PAL.flesh0;  GridKey 'y' $PAL.flesh1
GridKey 'g' $PAL.flesh2;  GridKey 'h' $PAL.flesh3
GridKey 'b' $PAL.void0;   GridKey 'n' $PAL.void1
GridKey 'm' $PAL.void2;   GridKey 'B' $PAL.void3
GridKey 'Z' $PAL.white0;  GridKey 'S' $PAL.white1;  GridKey 'W' $PAL.white2
GridKey 'C' $PAL.cyan;    GridKey 'V' $PAL.violet;  GridKey 'G' $PAL.glint
GridKey 'D' $PAL.danger;  GridKey 'Y' $PAL.gold;    GridKey 'H' $PAL.heal
GridKey 'L' $PAL.clsKnight;  GridKey 'M' $PAL.clsMage;   GridKey 'N' $PAL.clsCleric
GridKey 'R' $PAL.clsRogue;   GridKey 'O' $PAL.clsGuardian; GridKey 'J' $PAL.clsRanger

# Turns an ASCII block into a bitmap. Reports EVERY malformed row and unknown
# key in one throw, so a mis-typed grid is fixed in a single pass.
function Draw-Grid([string[]]$rows) {
  $h = $rows.Count
  $w = $rows[0].Length
  $errs = @()
  for ($y = 0; $y -lt $h; $y++) {
    if ($rows[$y].Length -ne $w) {
      $errs += "  row $y : $($rows[$y].Length) chars, expected $w"
    }
    foreach ($ch in $rows[$y].ToCharArray()) {
      if ($ch -ne '.' -and -not $GRIDC.ContainsKey($ch)) {
        $errs += "  row $y : unknown palette key '$ch'"
        break
      }
    }
  }
  if ($errs.Count -gt 0) { throw "Draw-Grid rejected a sprite:`n$($errs -join "`n")" }
  $b = New-Img $w $h
  for ($y = 0; $y -lt $h; $y++) {
    $row = $rows[$y]
    for ($x = 0; $x -lt $w; $x++) {
      $ch = $row[$x]
      if ($ch -ne '.') { $b.SetPixel($x, $y, $GRIDC[$ch]) }
    }
  }
  return $b
}
function Save-EnemyGrid([string]$name, [string[]]$rows) {
  $b = Draw-Grid $rows
  # The boss canvas is load-bearing, not a style choice: 36 rows is the tallest
  # sprite that never clips off the top of a 5+ enemy fight, and 36 columns is
  # the widest that stays inside the 40px unit footprint. See the section
  # header for the arithmetic. Guarded so it cannot drift unnoticed.
  if ($name.StartsWith('boss_') -and ($b.Width -ne 36 -or $b.Height -ne 36)) {
    throw "Save-EnemyGrid: boss '$name' is $($b.Width)x$($b.Height); must be 36x36."
  }
  Outline $b
  SaveImg $b "enemies/$name.png"
}

# --- Family: goblinoid & raider (bruisers with comically outsized arms) ---

Save-EnemyGrid 'goblin_grunt' @(     # tiny goblin, club three times its head
  '..ddddd.................'
  '.dfffffd................'
  '.dfffffdd...............'
  '.dffffdddd..............'
  '.ddfffddd...............'
  '..ddddss................'
  '...dds..................'
  '....ds..................'
  '....ds....v......v......'
  '.....ds...vv....vv......'
  '.....ds..vvvvvvvvvc.....'
  '......ds.vcccccccccx....'
  '......dscvcKcccKccccx...'
  '.....cccscccccccccccx...'
  '.....cccvcNNNNNNccxxx...'
  '......cccccccccccxx.....'
  '.......ccccccccxx.......'
  '.......xcccccccx........'
  '.......xcccccccx........'
  '.......xccccccxx........'
  '.......xxcccxxx.........'
  '.......xx...xx..........'
  '......xxx...xxx.........'
  '........................'
)

Save-EnemyGrid 'kobold_scout' @(    # all ears and dagger, almost no body
  '........................'
  '..f..............f......'
  '..ff............ff......'
  '..fff..........fff......'
  '...fff........ffff......'
  '...ffffddddddffff.......'
  '....fffdddddddfff.......'
  '.....fddddddddddf.......'
  '.....fddKdddKdddd.......'
  '......dddddddddddss.....'
  '......ddddddddddss......'
  '.....sdddddddddd........'
  '....ssdddddddds.........'
  '..sss.sddddddd......L...'
  '.ss....sdddddd.....LL...'
  '.......sdddddd....LL....'
  '.......sddddddd..LL.....'
  '.......ssddddddYLL......'
  '.......ssdddddYY........'
  '.......ssdddds..........'
  '.......ss...ss..........'
  '.......ss...ss..........'
  '......sss...sss.........'
  '........................'
)

Save-EnemyGrid 'bandit' @(          # slouch-hat raider, red mask, sabre
  '........................'
  '.....................LL.'
  '....................LL..'
  '...22222222........LL...'
  '..2222222222......LL....'
  '....333333.......LL.....'
  '....fddddf......LL......'
  '....fKddKf.....LL.......'
  '....DDDDDD....YY........'
  '...aaaaaaaaa............'
  '..aaassssssa............'
  '.aaaasssssssa...........'
  '.aaaasssssssa...........'
  '.aaassssssssa...........'
  '..aassssssssa...........'
  '...assssssssa...........'
  '....ssssssss............'
  '....ssssssss............'
  '....2222.222............'
  '.....22...22............'
  '.....22...22............'
  '.....22...22............'
  '....222...222...........'
  '........................'
)

Save-EnemyGrid 'dune_reaver' @(     # lean raider, scimitar held flat overhead
  '........................'
  '........................'
  '.....LLLLLLLLLLLL.......'
  '....LLLLLLLLLLLLLL......'
  '.....LLLLL......YY......'
  '........................'
  '.ff.....ffffff..........'
  '.fff...fdddddff.........'
  '..fff.fdddddddf.........'
  '...ffffdKdddKdd.........'
  '....ffddddddddd.........'
  '.....fddddddddd.........'
  '......ssssssss..........'
  '.....sssssssssss........'
  '....sssdddddddsss.......'
  '....ssddddddddds........'
  '.....sddddddddd.........'
  '.....sdddddddd..........'
  '.....addddddda..........'
  '.....aaa..aaaa..........'
  '.....aa....aa...........'
  '.....aa....aa...........'
  '....aaa....aaa..........'
  '........................'
)

Save-EnemyGrid 'ogre_marauder' @(   # elite: slab of a brute, studded log club
  '..............ddddddd...'
  '.............dffffffdd..'
  '....V....V...dffffffffd.'
  '....VV..VV...dfafafafad.'
  '....hhhhhh...dffffffffd.'
  '...hhhhhhhh..dfafafafad.'
  '...hhDhhDhh..dffffffffd.'
  '...hhhhhhhh...dddddddd..'
  '...hNhhhhNh.....ssss....'
  '..gggggggggg...ssss.....'
  '.ggggggggggggssss.......'
  'ggggghhhhhggggg.........'
  'ggggghhhhhgggggg........'
  'gggggghhhgggggggg.......'
  'gggggggggggggggg........'
  'ygggggggggggggg.........'
  'yygggggggggggg..........'
  'yyyggggggggggg..........'
  'yyyygggggggggg..........'
  'yyyyyggggggggg..........'
  '.yyyy.....yyyy..........'
  '.yyyy.....yyyy..........'
  '.tttt.....tttt..........'
  '........................'
)

Save-EnemyGrid 'troll_berserker' @( # elite: lanky, knuckle-dragging arms
  '........................'
  '........V.....V.........'
  '........VV...VV.........'
  '........vvvvvvv.........'
  '.......vvvvvvvvv........'
  '.......vvDvvvDvv........'
  '.......vvvvvvvvv........'
  '.......vNvvvvvNv........'
  '.....ccccvvvvvcccc......'
  '....cvvcccccccccxvc.....'
  '....cvvcccccccccxvc.....'
  '....cvvcccccccccxvc.....'
  '....cvvcccccccccxvc.....'
  '....cvvcccccccccxvc.....'
  '....cvvcccccccccxvc.....'
  '...cvvvccccccccccxvc....'
  '...cvvvcxcccccccxxvc....'
  '..cvvvvc.xcccccx.cvvc...'
  '..cvvvvc.xcccccx.cvvc...'
  '..cvvvvc.xxcccxx.cvvc...'
  '..cxxxxc..xcccx..cxxc...'
  '...cccc...xcccx...cccc..'
  '..........xxxxx.........'
  '........................'
)

Save-EnemyGrid 'ironclad_reaver' @( # elite: plated brute, axe grounded right
  '........................'
  '.....V..........V.......'
  '.....VV........VV.......'
  '......rrrrrrrrrr........'
  '.....rreeeeeeeerr.......'
  '.....reeDeeeeDeer.......'
  '.....reeeeeeeeeer.......'
  '......eeeeeeeeee........'
  '....rrrrrrrrrrrr........'
  '...rreeeeeeeeeerr.......'
  '...reeeeeeeeeeeers......'
  '...reeeVVVVVVeeers......'
  '...reeeeeeeeeeeers......'
  '...rreeeeeeeeeerrs......'
  '....reeeeeeeeeer.s......'
  '....qreeeeeeeerq.s......'
  '....qqreeeeeerqq.s......'
  '....qqqrrrrrrqqq.s...LLL'
  '....qqq......qqq.sLLLLLL'
  '....qqq......qqq.sLLLLLL'
  '....www......www.sLLLLL.'
  '...wwww......wwww.LLL...'
  '...qqqq......qqqq.......'
  '........................'
)

Save-EnemyGrid 'dread_knight' @(    # elite: black plate, greatsword planted
  '........................'
  '...V..........V.........'
  '...VV........VV.........'
  '....3333333333..........'
  '...32222222223..........'
  '...32DD2222DD23.........'
  '...322222222223...LL....'
  '....322222222.....LL....'
  '..3333333333333...LL....'
  '.33222222222233...LL....'
  '.32222222222223...LL....'
  '.3222VVVVVVV223...LL....'
  '.3222222222223..LLLLLL..'
  '.33222222222233..LLLL...'
  '..322222222223....LL....'
  '..132222222231....LL....'
  '..11322222231111..LL....'
  '..1113333331111...LL....'
  '...111......111...LL....'
  '...111......111...LL....'
  '...222......222...LL....'
  '..2222......2222..rr....'
  '..1111......1111........'
  '........................'
)

# --- Family: undead (bone showing through, negative space in the ribs) ---

Save-EnemyGrid 'skeleton_archer' @( # skeleton, bow taller than the archer
  '........................'
  '........................'
  '.....NNNN....Zss........'
  '....NNNNNN...Z..ss......'
  '....NKNNKN...Z....s.....'
  '....NNNNNN...Z.....s....'
  '.....NNNN....Z......s...'
  '......ZZ.....Z......s...'
  '...NNNNNNN...Z.......s..'
  '..NNZ.NN.ZNNZZ.......s..'
  '..NN.NNNN.NN.Z.......s..'
  '..N.NNZZNN.N.Z.......s..'
  '....NNNNNN...Z.......s..'
  '..N.NNZZNN.N.Z.......s..'
  '..NN.NNNN.NN.Z.......s..'
  '...NNNNNNN...Z......s...'
  '.....ZZZZ....Z......s...'
  '.....NNNN....Z.....s....'
  '....NN..NN...Z....s.....'
  '....NN..NN...Z..ss......'
  '....NN..NN...Zss........'
  '....NN..NN..............'
  '...NNN..NNN.............'
  '........................'
)

Save-EnemyGrid 'zombie' @(          # shambler: arm out front, torn-open gut
  '........................'
  '........................'
  '..........xxxxx.........'
  '.........xxxxxxx........'
  '.........xYxxxYx........'
  '.........xxxxxxx........'
  '..........xxxxx.........'
  '..........xKKKx.........'
  '.......xxxxxxxx.........'
  '.....xxxxxxxxxxxxxxxxx..'
  '....xxxxxxxxxxxxxxxxxxz.'
  '....xxxxxxxxxxxxxx......'
  '....xxxxxxxxxxx.........'
  '..x.xxxxx..xxxx.........'
  '..xx.xxx....xxx.........'
  '..xx.xxxx..xxxx.........'
  '..xx.xxxxxxxxxx.........'
  '..xx.xxxxxxxxx..........'
  '..zz.xxxxxxxxx..........'
  '.....xxxx..xxx..........'
  '.....xxx...xxx..........'
  '.....xxx...xxx..........'
  '....zxxx...xxxz.........'
  '........................'
)

Save-EnemyGrid 'grave_wight' @(     # gaunt shroud, long reaching claws
  '........................'
  '.........22222..........'
  '........2333332.........'
  '........3YY33YY3........'
  '........33333333........'
  '.........333333.........'
  '..........3333..........'
  '.......3333333333.......'
  '.....q33333333333q......'
  '....q3333333333333q.....'
  '...q33333333333333q.....'
  '...N333333333333333N....'
  '..NN33333333333333NN....'
  '.N.N3333333333333N.N....'
  'N..N33333333333N.N..N...'
  '....333333333333........'
  '....333333333333........'
  '....33322333333.........'
  '....3332..33333.........'
  '.....332...3333.........'
  '.....22.....333.........'
  '.....22.....222.........'
  '....222.....222.........'
  '........................'
)

Save-EnemyGrid 'corpse_hound' @(    # the ribs are HOLES: gaps carry the read
  '........................'
  '........................'
  '...............zzz......'
  '..............zcccz.....'
  '.............zcccccz....'
  '............zccDccccz...'
  '...........zccccccccz...'
  '..zzz.....zcccccccccNN..'
  '.zcccz...zccccccccNNN...'
  'zcccccz.zcccccccccc.....'
  'zcc.cc.zccccccccccc.....'
  'zc.cc.c.cccccccccc......'
  'zc.cc.c.cccccccccc......'
  'zcc.cc.zcccccccccc......'
  'zcccccczcccccccccc......'
  '.zcccccccccccccccz......'
  '..zc..zc...zc..zcz......'
  '..zc..zc...zc..zc.......'
  '..zc..zc...zc..zc.......'
  '..zc..zc...zc..zc.......'
  '..zc..zc...zc..zc.......'
  '.zzc..zzc..zzc.zzc......'
  '........................'
  '........................'
)

Save-EnemyGrid 'grave_chanter' @(   # elite: skeletal reader, enormous tome
  '........................'
  '.....V........V.........'
  '.....VV......VV.........'
  '......NNNNNNNN..........'
  '.....NNKNNNKNNN.........'
  '.....NNNNNNNNNN.........'
  '......NZZZZZZN..........'
  '....33333333333.........'
  '...3333333333333........'
  '..333333333333333.......'
  '..33333333333333LLLLLLL.'
  '..3333VVVVVV333LNNNNNNNL'
  '..333333333333.LNZZZZZNL'
  '..3333333333333LNNNNNNNL'
  '..3333333333333LNZZZZZNL'
  '..3333333333333LNNNNNNNL'
  '...333333333333LLLLLLLL.'
  '...33333333333..........'
  '...333333333333.........'
  '...3333333333333........'
  '..233333333333332.......'
  '..222222222222222.......'
  '...2222222222222........'
  '........................'
)

Save-EnemyGrid 'bone_colossus' @(   # elite: a wall of ribs, arms like pillars
  '........................'
  '.......V......V.........'
  '.......VV....VV.........'
  '........NNNNNN..........'
  '.......NNKNNKNN.........'
  '.......NNNNNNNN.........'
  '........NZZZZN..........'
  '.NNNNNNNNNNNNNNNNNNNN...'
  'NNZZNNNNNNNNNNNNNNZZNN..'
  'NNZZNN.NNNNNNNN.NNZZNN..'
  'NNZZN.NNNNNNNNNN.NZZNN..'
  'NNZZN.N.NNNNNN.N.NZZNN..'
  'NNZZN.NNNNNNNNNN.NZZNN..'
  'NNZZN.N.NNNNNN.N.NZZNN..'
  'NNZZN.NNNNNNNNNN.NZZNN..'
  'NNZZN.N.NNNNNN.N.NZZNN..'
  'NNZZNN.NNNNNNNN.NNZZNN..'
  'NNZZNNNNVVVVVVNNNNZZNN..'
  '.NNNNNNNNNNNNNNNNNNNN...'
  '.....NNNNN..NNNNN.......'
  '.....NNNN....NNNN.......'
  '.....NNNN....NNNN.......'
  '....ZNNNN....ZNNNN......'
  '........................'
)

Save-EnemyGrid 'soul_render' @(     # elite: legless wraith, huge scythe
  '........................'
  '...V.......V......LLLLLL'
  '...VV.....VV.....LLL....'
  '....2222222.....LL......'
  '...222222222....L.......'
  '...22VV2VV22....s.......'
  '...222222222....s.......'
  '....2222222.....s.......'
  '..12222222221...s.......'
  '.1122222222211..s.......'
  '.1222222222221..s.......'
  '.12222VVVV22221.s.......'
  '.1222222222221..s.......'
  '..122222222211..s.......'
  '..1222222222.1..s.......'
  '...12222222....ss.......'
  '...1222222.1...s........'
  '....12222.....s.........'
  '....1.222..1..s.........'
  '.....1222....s..........'
  '.....1.22...s...........'
  '......122.1.............'
  '.......1................'
  '........................'
)

Save-EnemyGrid 'plague_bearer' @(   # elite: bloated gut, tiny head, buboes
  '........................'
  '............V...V.......'
  '...........VV...VV......'
  '...........cccccc.......'
  '..........ccVccVcc......'
  '..........cccccccc......'
  '...........cccccc.......'
  '......ccccccccccccc.....'
  '....cccccccccccccccc....'
  '...cccccvvvvvvvcccccc...'
  '..cccvvvvvvvvvvvvcccc...'
  '.ccccvvvvVvvvvVvvvcccc..'
  '.cccvvvvvvvvvvvvvvvccc..'
  '.cccvvvVvvvvvvvVvvvccc..'
  '.cccvvvvvvvvvvvvvvvccc..'
  '.cccvvvvvVvvvVvvvvvccc..'
  '.ccccvvvvvvvvvvvvvccc...'
  '..xccccvvvvvvvvvcccc....'
  '..xxccccccccccccccx.....'
  '...xxxccccccccccxx......'
  '....xxx........xxx......'
  '....xxx........xxx......'
  '...zxxx........xxxz.....'
  '........................'
)

# --- Family: beast (no weapons; the body plan itself is the silhouette) ---

Save-EnemyGrid 'cave_bat' @(        # wing span three times the body
  '........................'
  '.......3.......3........'
  '.......33.....33........'
  '........3333333.........'
  '.......333333333........'
  '.......3D33333D3........'
  '.......333333333........'
  '........3NNNNN3.........'
  '..2222...33333...2222...'
  '.222222.3333333.222222..'
  '22222222233333222222222.'
  '2233222223333322222332..'
  '223.22222333332222.322..'
  '22...2222333322222...22.'
  '.2....222333322222....2.'
  '.......2233332222.......'
  '........233332..........'
  '.........33333..........'
  '.........33333..........'
  '..........333...........'
  '.........N...N..........'
  '........................'
  '........................'
  '........................'
)

Save-EnemyGrid 'venom_spider' @(    # eight legs, all negative space
  '........................'
  '.z....................z.'
  '.zz..................zz.'
  '..zz................zz..'
  '..zz.z...........z..zz..'
  '...zz.zz.......zz..zz...'
  '...zz..zz.....zz...zz...'
  '....zz..xxxxxxx...zz....'
  '.....z.xxxxxxxxx..z.....'
  '......xxccccccxxx.......'
  '.....xxcccccccccxx......'
  '....xxccccVccVcccxx.....'
  '....xcccccccccccccx.....'
  '....xccccHHHHHccccx.....'
  '....xcccccccccccccx.....'
  '.....xxcccccccccxx......'
  '...zz..xxcccccxx..zz....'
  '..zz.zz..xxxxx...zz.zz..'
  '..zz..zz.......zz...zz..'
  '.zz....zz.....zz.....zz.'
  '.zz.....z.....z.......zz'
  '.z....................z.'
  '........................'
  '........................'
)

Save-EnemyGrid 'wild_boar' @(       # bristle hump forward, tusks up
  '........................'
  '........................'
  '.......ss...............'
  '......ssss..............'
  '.....ssdsds.............'
  '....ssdddddss...........'
  '...sddddddddss..........'
  '..sdddddddddddss...NN...'
  '.sddddddddddddddss.NN...'
  '.sdddddddddddddddsNNs...'
  'sddffddddddddddddddds...'
  'sdffffdddddddDddddds....'
  'sdffffddddddddddddss....'
  'saffffdddddddddddss.....'
  '.saffdddddddddddss......'
  '..sadddddddddddss.......'
  '...saaddddddddss........'
  '....sa.ss.ss.ss.........'
  '....sa.ss.ss.ss.........'
  '.......ss.ss.ss.........'
  '.......ss.ss.ss.........'
  '......aaa.aa.aaa........'
  '........................'
  '........................'
)

Save-EnemyGrid 'forest_wolf' @(     # lean, long legs, ears and tail up
  '........................'
  '........................'
  '.q..................q...'
  '.qq...............q.q...'
  '.qqq..............qq.q..'
  '..qqq............eqq.q..'
  '..qqqq..........eeeqq...'
  '...qqqqqqqqqqqqeeeeee...'
  '...qeeeeeeeeeeeeeYeeYe..'
  '..qeeeeeeeeeeeeeeeeeee..'
  '.qeerrrreeeeeeeeeeeeeNN.'
  '.qeerrrrrreeeeeeeeeNNN..'
  '.qeerrrreeeeeeeeeeee....'
  '..qeeeeeeeeeeeeeeee.....'
  '...qqeeeeeeeeeeeee......'
  '....qq.ee..ee..ee.......'
  '.......ee..ee..ee.......'
  '.......ee..ee..ee.......'
  '.......ee..ee..ee.......'
  '.......ee..ee..ee.......'
  '......qee.qee.qee.......'
  '......qqq.qqq.qqq.......'
  '........................'
  '........................'
)

Save-EnemyGrid 'sand_lurker' @(     # limbless: one long diagonal serpent
  '........................'
  '........................'
  '........................'
  '..............ffffff....'
  '.............ffddddff...'
  '.............fdddddddf..'
  '.............fdVddVddf..'
  '.............fddddddddf.'
  '.............fddNNdddd..'
  '............ffddddddf...'
  '...........ffddddddf....'
  '..........ffdddddff.....'
  '.........ffdddddf.......'
  '........ffdddddf........'
  '.......ffdddddf.........'
  '......ffdddddf..........'
  '.....ffdddddf...........'
  '....ffdddddf............'
  '...ffdddddf.............'
  '..ffdddddf..............'
  '..fddddddffffff.........'
  '..fdddddddddddffffff....'
  '..sffdddddddddddddddff..'
  '...sssssssssssssssssss..'
)

Save-EnemyGrid 'mud_crawler' @(     # low armoured woodlouse, plate ridges
  '........................'
  '........................'
  '........................'
  '........................'
  '........................'
  '........................'
  '.......ww...ww..........'
  '......wwww.wwww.........'
  '.....wwwwwwwwwww........'
  '....weewweewweewww......'
  '...weeeweeeweeeewwww....'
  '..seeeeseeeeseeeeewww...'
  '.saaaaasaaaaasaaaaawwq..'
  '.sddddsddddsddddsddwYq..'
  'sddddsddddsddddsdddwwYq.'
  'sdddsddddsddddsddddwwwq.'
  'ssdssddddsddddsdddsswwq.'
  '.aa.aa..aa..aa..aa.wwq..'
  '.a...a..a....a..a..qq...'
  '........................'
  '........................'
  '........................'
  '........................'
  '........................'
)

Save-EnemyGrid 'frost_imp' @(       # slender, tall ice horns, sharp wings
  '........................'
  '.....C..........C.......'
  '.....C..........C.......'
  '....CG..........GC......'
  '....pC..........Cp......'
  '.....pooooooooop........'
  '....poooooooooooop......'
  '....pooCoooooCooop......'
  '....pooooooooooooop.....'
  '.....pooooooooooop......'
  '.p....poooooooop....p...'
  '.pp....pooooooop...pp...'
  '.opp....pooooop...ppo...'
  '.oopp...pooooop..ppoo...'
  '.ooopp..pooooop.ppooo...'
  '.oooopp.pooooop.pooo....'
  '..ooop..pooooop..poo....'
  '...op...ppooopp...p.....'
  '........poooop..........'
  '........po..op..........'
  '........po..op..........'
  '.......ppo..opp.........'
  '........................'
  '........................'
)

Save-EnemyGrid 'mire_imp' @(        # squat toad, wide grin, drooping wings
  '........................'
  '........................'
  '........................'
  '.......cc.....cc........'
  '......cVVc...cVVc.......'
  '......ccccccccccc.......'
  '.....ccccccccccccc......'
  '....cccVccccccVccc......'
  '....ccccccccccccccc.....'
  '....cNNNNNNNNNNNNNc.....'
  '..zzccccccccccccccczz...'
  '.zxxccccccccccccccxxxz..'
  '.zxxxccccccccccccxxxxz..'
  '.zxxxxcccccccccccxxxxz..'
  '..zxxxxcccccccccxxxxz...'
  '...zxxxccccccccxxxxz....'
  '....zxxccccccccxxxz.....'
  '.....zcccccccccccz......'
  '......cccc..cccc........'
  '......ccc....ccc........'
  '.....zccz....zccz.......'
  '.....zzzz....zzzz.......'
  '........................'
  '........................'
)

# --- Family: caster (robes are interchangeable, so the HEADGEAR carries the
# --- silhouette: cone hood / antlers / mitre / plague beak / orb / shard) ---

Save-EnemyGrid 'dark_acolyte' @(    # tall cone hood, orb floating at the hand
  '........................'
  '.........22.............'
  '........2222............'
  '........2222............'
  '.......222222...........'
  '.......222222...........'
  '......22222222..........'
  '......22111122..........'
  '......22VKKV22..........'
  '.....221111122..........'
  '.....2222222222.........'
  '....222222222222........'
  '....2222222222222.......'
  '...22222222222222.VVVV..'
  '...222222222222222VGGGV.'
  '...3222222222222.VGGGGGV'
  '...3322222222222..VGGGV.'
  '..333222222222222..VVVV.'
  '..3332222222222222......'
  '..33322222222222........'
  '..333222222222222.......'
  '.3333222222222222.......'
  '.3333333222222233.......'
  '........................'
)

Save-EnemyGrid 'bog_shaman' @(      # antler crown wider than the shaman
  '........................'
  '.f...................f..'
  '.f...f.........f.....f..'
  '..f..f.........f....f...'
  '..ff.ff.......ff.ffff...'
  '...fffff.....fffff......'
  '.....ffff...ffff........'
  '......cccccccccc........'
  '.....cczzzzzzzcc........'
  '.....ccHzzzzzHcc........'
  '.....cczzzzzzzcc........'
  '......cccccccc.......f..'
  '....cccccccccccc....ff..'
  '...ccccccccccccc...fHf..'
  '..cccccxxxxxcccc....ff..'
  '..ccccxxxxxxxccc.....f..'
  '..cccxxxxxxxxxcc.....f..'
  '..cccxxxxxxxxxcc.....f..'
  '..ccxxxxxxxxxxxcc....f..'
  '..ccxxxxxxxxxxxcc....f..'
  '.zccxxxxxxxxxxxccz...f..'
  '.zzcxxxxxxxxxxxczz...f..'
  '.zzzzzzzzzzzzzzzzz...f..'
  '........................'
)

Save-EnemyGrid 'gloom_priest' @(    # wide flat mitre, censer on a long chain
  '........................'
  '....NNNNNNNNNNNN........'
  '...NNNNNNNNNNNNNN.......'
  '....NNNNNNNNNNNN........'
  '.......NN22NN...........'
  '......NN2222NN..........'
  '......N2HKKH2N..........'
  '......NN2222NN..........'
  '.......NNNNNN...........'
  '....NNNNNNNNNNNN........'
  '...NN2222222222NN.......'
  '...N222222222222N....Z..'
  '...N222222222222N....Z..'
  '...N222HHHH22222N....Z..'
  '...N222222222222N....Z..'
  '...NN2222222222NN...ZZ..'
  '....N2222222222N....HH..'
  '....N2222222222N...HHHH.'
  '....N2222222222N...HYYH.'
  '....N2222222222N...HHHH.'
  '....N2222222222N....HH..'
  '...NN2222222222NN.......'
  '...NNNNNNNNNNNNNN.......'
  '........................'
)

Save-EnemyGrid 'blight_chanter' @(  # elite: plague beak, brim, censer vial
  '........................'
  '.....V..........V.......'
  '.....VV........VV.......'
  '...zzzzzzzzzzzzzz.......'
  '..zzzzzzzzzzzzzzzz......'
  '...zzzcccccccczzz.......'
  '.....ccccccccccc........'
  '.....ccHccccHccc........'
  '.....cccccccccccff......'
  '.....ccccccccffff.......'
  '.....cccccccff..........'
  '....zzcccccz............'
  '...zzzcccczzz.......H...'
  '..zzzzccczzzzz.....HHH..'
  '..zzzzzzzzzzzz.....HHH..'
  '..zzzzcHHHczzzz.....H...'
  '..zzzzzHHHzzzzz.....z...'
  '..zzzzzcHczzzzz.....z...'
  '..zzzzzzzzzzzzz.....z...'
  '..zzzzzzzzzzzzz.....z...'
  '.zzzzzzzzzzzzzzz....z...'
  '.zzzzzzzzzzzzzzz....z...'
  '.zzzzzzzzzzzzzzz........'
  '........................'
)

Save-EnemyGrid 'wisp' @(            # bodiless: a core and its corona
  '........................'
  '..........C.............'
  '........................'
  '.....C.........C........'
  '..........VV............'
  '........VVGGVV..........'
  '.......VGGGGGGV.........'
  '..C...VGGCCCCGGV.....C..'
  '......VGCCCCCCGV........'
  '.....VGGCCWWCCGGV.......'
  '.....VGCCCWWCCCGV.......'
  '.....VGCCCWWCCCGV.......'
  '.....VGGCCWWCCGGV.......'
  '......VGCCCCCCGV........'
  '..C...VGGCCCCGGV.....C..'
  '.......VGGGGGGV.........'
  '........VVGGVV..........'
  '..........VV............'
  '.....C.........C........'
  '........................'
  '..........C.............'
  '........................'
  '........................'
  '........................'
)

Save-EnemyGrid 'hex_wisp' @(        # orb trailing a curtain of tendrils
  '........................'
  '.........VVVV...........'
  '.......VVVVVVVV.........'
  '......VVGGGGGGVV........'
  '.....VVGGGGGGGGVV.......'
  '.....VGGGKGGKGGGV.......'
  '.....VGGGGGGGGGGV.......'
  '.....VVGGGGGGGGVV.......'
  '......VVGGGGGGVV........'
  '.......VVVVVVVV.........'
  '......V.V.VV.V.V........'
  '......V.V.VV.V.V........'
  '.....V..V.VV.V..V.......'
  '.....V..V.VV.V..V.......'
  '.....V.V..VV..V.V.......'
  '....V..V..VV..V..V......'
  '....V..V..VV..V..V......'
  '....V.V...VV...V.V......'
  '...V..V...VV...V..V.....'
  '...V.V....VV....V.V.....'
  '...V.V....VV....V.V.....'
  '..V.V.....VV.....V.V....'
  '..V.......VV.......V....'
  '........................'
)

Save-EnemyGrid 'shardling' @(       # a crystal splinter, no body at all
  '........................'
  '..............C.........'
  '.............CG.........'
  '............CGGC........'
  '...........CGGGC........'
  '..........CGGGGC........'
  '....C....CGGGGGC........'
  '...CG...CGGGGGCo........'
  '..CGGC.CGGKGGCoo........'
  '..CGGCCGGGGGCooo........'
  '..CGGGGGGGGCoooo........'
  '..CGGGGGGGCooooo...C....'
  '..CGGGGGGCoooooo..CG....'
  '...CGGGGCooooooo.CGGC...'
  '...CGGGCoooooooo.CGGC...'
  '....CGCooooooooo.CGoC...'
  '....CCoooooooooo.CooC...'
  '.....Coooooooooo.CooC...'
  '.....Cooooooooo...CoC...'
  '......Cooooooo.....CC...'
  '.......Cooooo...........'
  '........Cooo............'
  '.........Co.............'
  '........................'
)

Save-EnemyGrid 'void_weaver' @(     # elite: thread-thin arms, unravelling hem
  '........................'
  '.....V..........V.......'
  '.....VV........VV.......'
  '.......1111111..........'
  '......111111111.........'
  '......11VKKKV11.........'
  '......111111111.........'
  '.......1111111..........'
  'V.......11111.......V...'
  '.V.....1111111.....V....'
  '..V...111111111...V.....'
  '...VV11111111111VV......'
  '.....11111111111........'
  '.....11111111111........'
  '.....1V111111V11........'
  '.....11111111111........'
  '.....11111111111........'
  '.....1.1.1.1.1.1........'
  '.....1.1.1.1.1.1........'
  '.....1...1.1...1........'
  '.....1...1.1...1........'
  '.....1...1.1............'
  '.........1..............'
  '........................'
)

# --- Family: construct & armoured protector (hard 90-degree geometry; the
# --- shield or the growth on the back does the silhouette work) ---

Save-EnemyGrid 'stone_golem' @(     # nothing but slabs; arms hang clear
  '........................'
  '........................'
  '.........qqqqqq.........'
  '.........rwwwwq.........'
  '.........rwCwCq.........'
  '.........rwwwwq.........'
  '..rrrr...qqqqqq...rrrr..'
  '..rwwq.rrrrrrrrrr.rwwq..'
  '..rwwq.rwwwwwwwwq.rwwq..'
  '..rwwq.rwwwwwwwwq.rwwq..'
  '..rwwq.rwwqqqqwwq.rwwq..'
  '..rwwq.rwwqwwqwwq.rwwq..'
  '..rwwq.rwwqqqqwwq.rwwq..'
  '..rwwq.rwwwwwwwwq.rwwq..'
  '..rwwq.rwwwwwwwwq.rwwq..'
  '..qwwq.rwwwwwwwwq.qwwq..'
  '..qqqq.qqqqqqqqqq.qqqq..'
  '.......rwwq.rwwq........'
  '.......rwwq.rwwq........'
  '.......rwwq.rwwq........'
  '.......rwwq.rwwq........'
  '......rrwwqqrrwwqq......'
  '......qqqqq.qqqqq.......'
  '........................'
)

Save-EnemyGrid 'rune_sentry' @(     # floating monolith, one great rune eye
  '........................'
  '........................'
  '.......qqqqqqqq.........'
  '......qwwwwwwwwq........'
  '.....qwwwwwwwwwwq.......'
  '.....qwwwwwwwwwwq.......'
  '.....qwwCCCCCCwwq.......'
  '.....qwCCGGGGCCwq.......'
  '.....qwCGGKKGGCwq.......'
  '.....qwCGGKKGGCwq.......'
  '.....qwCCGGGGCCwq.......'
  '.....qwwCCCCCCwwq.......'
  '.....qwwwwwwwwwwq.......'
  '.....qwwwCwwCwwwq.......'
  '.....qwwwwwwwwwwq.......'
  '.....qwwwwwwwwwwq.......'
  '.....qqwwwwwwwwqq.......'
  '......qqwwwwwwqq........'
  '.......qqwwwwqq.........'
  '........qqwwqq..........'
  '.........qqqq...........'
  '..........C.............'
  '...........C............'
  '........................'
)

Save-EnemyGrid 'crystal_guardian' @( # elite: shards erupting from the back
  '.C......................'
  '.CG..V.........V........'
  '..CG.VV.......VV........'
  'C..CG..eeeeee...........'
  'CG..C.eeCeeCee..........'
  '.CG...eeeeeeee..........'
  '..C....eeeeee...........'
  'C....eeeeeeeeee.........'
  'CG..eeeeeeeeeeee........'
  '.CG.eerrrrrrreee........'
  '..C.eerCCCCCreee........'
  '....eerCGGGCreee........'
  '....eerCCCCCreee........'
  '....eerrrrrrreee........'
  '....eeeeeeeeeeee........'
  '....weeeeeeeeeew........'
  '....wweeeeeeeeww........'
  '....wwweeeeeewww........'
  '....wwww....wwww........'
  '....wwww....wwww........'
  '....qqqq....qqqq........'
  '...qqqqq....qqqqq.......'
  '........................'
  '........................'
)

Save-EnemyGrid 'iron_sentinel' @(   # elite: narrow, tower shield front-on
  '........................'
  '........V....V..........'
  '........VV..VV..........'
  '.........eeee...........'
  '........eeeeee..........'
  '........eDeeDe..........'
  '........eeeeee..........'
  '.........eeee...........'
  '.....rrrrrrrrrrrr.......'
  '.....rwwwwwwwwwwr.......'
  '.....rwqqqqqqqqwr.......'
  '.....rwqrrrrrrqwr.......'
  '.....rwqrVVVVrqwr.......'
  '.....rwqrVGGVrqwr.......'
  '.....rwqrVVVVrqwr.......'
  '.....rwqrrrrrrqwr.......'
  '.....rwqqqqqqqqwr.......'
  '.....rwwwwwwwwwwr.......'
  '.....rrrrrrrrrrrr.......'
  '......ww......ww........'
  '......ww......ww........'
  '.....qqqq....qqqq.......'
  '........................'
  '........................'
)

Save-EnemyGrid 'titan_guard' @(     # elite: squat colossus, slab shield right
  '........................'
  '..V.........V...........'
  '..VV.......VV...........'
  '....wwwwwwwww...........'
  '...wwwCwwwCwww..........'
  '...wwwwwwwwwww..........'
  '.rrrrrrrrrrrrrrr........'
  'rrwwwwwwwwwwwwwrr.qqqqqq'
  'rrwwwwwwwwwwwwwrr.qrrrrq'
  'rrwwqqqqqqqqqwwrr.qrwwrq'
  'rrwwqwwwwwwwqwwrr.qrwwrq'
  'rrwwqwVVVVVwqwwrr.qrwwrq'
  'rrwwqwwwwwwwqwwrr.qrwwrq'
  'rrwwqqqqqqqqqwwrr.qrwwrq'
  'rrwwwwwwwwwwwwwrr.qrwwrq'
  '.rrwwwwwwwwwwwrr..qrwwrq'
  '..rrrrrrrrrrrrr...qrrrrq'
  '...wwww...wwww....qqqqqq'
  '...wwww...wwww..........'
  '...wwww...wwww..........'
  '..qqqqq...qqqqq.........'
  '..qqqqq...qqqqq.........'
  '........................'
  '........................'
)

Save-EnemyGrid 'royal_guard_sword' @( # elite: gold-trimmed, blade upright
  '........................'
  '..........Y.............'
  '.........YYY......L.....'
  '.........eee.....LLL....'
  '........YeeeeY...LLL....'
  '........eVeeVe...LLL....'
  '........eeeeee...LLL....'
  '.........eeee....LLL....'
  '.....YYYYYYYYYY..LLL....'
  '....reeeeeeeeeer.LLL....'
  '....reVVVVVVVVer.LLL....'
  '....reVeeeeeeVerYYYYY...'
  '....reVeYYYYeVer.LLL....'
  '....reVeYVVYeVer.LLL....'
  '....reVeYYYYeVer.LLL....'
  '....reVeeeeeeVer.LLL....'
  '....reVVVVVVVVer..L.....'
  '....reeeeeeeeeer........'
  '....rYYYYYYYYYYr........'
  '.....eee....eee.........'
  '.....eee....eee.........'
  '....YYYY....YYYY........'
  '....qqqq....qqqq........'
  '........................'
)

Save-EnemyGrid 'royal_guard_staff' @( # elite: matched twin, crowned stave
  '........................'
  '..........Y.............'
  '.........YYY.....CCC....'
  '.........eee....CGGGC...'
  '........YeeeeY..CGGGGC..'
  '........eCeeCe..CGGGGC..'
  '........eeeeee..CGGGGC..'
  '.........eeee....CGGGC..'
  '.....YYYYYYYYYY...CCC...'
  '....rVVVVVVVVVVr...Y....'
  '....rVeeeeeeeeVr...s....'
  '....rVeVVVVVVeVr...s....'
  '....rVeVCCCCVeVrYYYYY...'
  '....rVeVCGGCVeVr...s....'
  '....rVeVCCCCVeVr...s....'
  '....rVeVVVVVVeVr...s....'
  '....rVeeeeeeeeVr...s....'
  '....rVVVVVVVVVVr...s....'
  '....rYYYYYYYYYYr...s....'
  '.....VVV....VVV....s....'
  '.....VVV....VVV....s....'
  '....YYYY....YYYY........'
  '....qqqq....qqqq........'
  '........................'
)

Save-EnemyGrid 'archon_of_ruin' @(  # elite: broken halo, feet never land
  '........................'
  '......VVVVVVVV..........'
  '.....VV......VV.........'
  '....VV........VV........'
  '....V...2222....V.......'
  '...VV..222222...VV......'
  '...V..22CKKC22...V......'
  '...V..22222222...V......'
  '....V..222222...V.......'
  '.....VV.2222..VV........'
  '......V222222V..........'
  '....2222222222222.......'
  '...222222222222222......'
  '...222CCCCCCCCC222......'
  '...222222222222222......'
  '....2222222222222.......'
  '....2222222222222.......'
  '.....22222222222........'
  '.....22222222222........'
  '.....22222222222........'
  '......2222222222........'
  '.......11111111.........'
  '........111111..........'
  '........................'
)

# --- Family: buffer & stalker (the buffers ARE their instrument) ---

Save-EnemyGrid 'war_drummer' @(     # a drum wider than the drummer
  '........................'
  '..........dddd..........'
  '.........dsssdd.........'
  '.........dsDDsd.........'
  '.........dssssd.........'
  '..........dddd..........'
  '.f.....ssssssssss.....f.'
  '.ff...sssssssssss....ff.'
  '.ff..ssssssssssss....ff.'
  '..ff.yyyyyyyyyyyy...ff..'
  '...ffyggggggggggy..ff...'
  '..yygggggggggggggyyy....'
  '.yyggggggggggggggggy....'
  '.ygDDgggggggggggDDgy....'
  '.yggggggggggggggggggy...'
  '.ygDDgggggggggggDDgy....'
  '.yyggggggggggggggggy....'
  '..yyggggggggggggggy.....'
  '...yyyyyyyyyyyyyyy......'
  '.....ss........ss.......'
  '.....ss........ss.......'
  '....sss........sss......'
  '........................'
  '........................'
)

Save-EnemyGrid 'standard_bearer' @( # banner twice the soldier's height
  '.....gggggggggg.........'
  '.a...gDDDDDDDDg.........'
  '.a...gDDDDDDDDDg........'
  '.a...gDDDYYYYDDg........'
  '.a...gDDYYYYYYDg........'
  '.a...gDDDYYYYDDg........'
  '.a...gDDDDDDDDDg........'
  '.a...gDDDDDDDDg.........'
  '.a...gDDDDDDDg..........'
  '.a...gggggggg...........'
  '.a......................'
  '.a.....ddddd............'
  '.a....ddddddd...........'
  '.a....dLddLdd...........'
  '.a....ddddddd...........'
  '.a...sssssssss..........'
  '.a..sssssssssss.........'
  '.aa.sssyyyyysss.........'
  '..s.sssssssssss.........'
  '....ssssssssss..........'
  '.....222...222..........'
  '.....222...222..........'
  '....2222...2222.........'
  '........................'
)

Save-EnemyGrid 'war_caller' @(      # elite: a horn he can barely lift
  '........................'
  '.....V.......V..........'
  '.....VV.....VV.....YYY..'
  '......ddddddd.....YYYYY.'
  '.....ddddddddd...YYY.YYY'
  '.....ddDdddDddd..YY...YY'
  '.....ddddddddd..YYY..YYY'
  '......ddddddd..YYYYYYYY.'
  '....gggggggggYYYYYYYY...'
  '...ggggggggggYYYYY......'
  '..gyyyyyyyygggYY........'
  '..gyOOOOOOyyggg.........'
  '..gyOOOOOOyyyg..........'
  '..gyyyyyyyyyyg..........'
  '..ggyyyyyyyyg...........'
  '...gyyyyyyyyg...........'
  '...gyyyyyyyyg...........'
  '...ggyyyyyyg............'
  '....gggggggg............'
  '.....22..22.............'
  '.....22..22.............'
  '....222..222............'
  '........................'
  '........................'
)

Save-EnemyGrid 'shadow_stalker' @(  # elite: upright, twin daggers wide
  '........................'
  '.......V......V.........'
  '.......VV....VV.........'
  '........111111..........'
  '.......11111111.........'
  '.......1VV11VV1.........'
  '.......11111111.........'
  '........111111..........'
  '.L.......1111.......L...'
  '.LL....11111111....LL...'
  'LLL...1111111111...LLL..'
  '.LL..111111111111..LL...'
  '.L..1111111111111...L...'
  'Y...1111VVVVV1111...Y...'
  '....11111111111.........'
  '....11111111111.........'
  '....11111111111.........'
  '....1111111111..........'
  '....2211111122..........'
  '....22.....22...........'
  '....22.....22...........'
  '...222.....222..........'
  '........................'
  '........................'
)

Save-EnemyGrid 'void_stalker' @(    # elite: four-legged prowler, blade tail
  '........................'
  '.V......................'
  '.VV.....................'
  '..VV....................'
  '...VV...................'
  '....VV..................'
  '.....VV.................'
  '.....V11................'
  '.....111....V.....V.....'
  '....11111...VV...VV.....'
  '...1111111111111111.....'
  '..111111111111111111....'
  '.11111111111111111111...'
  '.1111111111111VV1VV11...'
  '.11111111111111111111...'
  '.1111111111111111111....'
  '..11111111111111111.....'
  '..11..11.....11..11.....'
  '..11..11.....11..11.....'
  '..11..11.....11..11.....'
  '..11..11.....11..11.....'
  '.222..222...222..222....'
  '........................'
  '........................'
)

# The two generic tier fallbacks. `BattleState::drawUnit` only reaches these
# when a content id has no bespoke sprite, so they are deliberately anonymous:
# a shape that says "an enemy" and "a tougher enemy" and nothing more.
Save-EnemyGrid 'normal_battle' @(   # generic: plain hunched beast
  '........................'
  '........................'
  '........................'
  '.............gggggg.....'
  '............ggggggggg...'
  '............ggDggDgggg..'
  '............gggggggggg..'
  '.....ggggg..ggNggNgggg..'
  '...ggggggggggggggggg....'
  '..gggggggggggggggggg....'
  '.ggggggggggggggggggg....'
  '.ygggggggggggggggggg....'
  '.yyggggggggggggggggg....'
  '.yyyggggggggggggggg.....'
  '.yyyyyggggggggggggg.....'
  '..yyyyyyygggggggggg.....'
  '...yyyyyyyyygggggg......'
  '....yyyyyyyyyyyyy.......'
  '.....yyy.....yyyy.......'
  '.....yyy.....yyy........'
  '.....yyy.....yyy........'
  '....tyyy.....yyyt.......'
  '........................'
  '........................'
)

# Same beast as `normal_battle`, but REARED UP: the tier difference is posture
# and height, not a recolour, so it survives the grayscale/colour-blind check.
Save-EnemyGrid 'elite_battle' @(    # generic: the same beast, rearing, horned
  '........................'
  '..........V.....V.......'
  '..........VV...VV.......'
  '..........gggggg........'
  '.........ggggggggg......'
  '.........ggDggDgggg.....'
  '.........gggggggggg.....'
  '.........ggNggNgggg.....'
  '..........gggggggg......'
  '.......ggggggggg........'
  '.....ggggggggggg........'
  '...gggggggggggg.........'
  '..ggggggggggggg.........'
  '..gggVVVVVVVggg.........'
  '..ggggggggggggg.........'
  '.yggggggggggggg.........'
  '.yyggggggggggggg........'
  '.yyygggggggggggg........'
  '.yyyyggggggggggg........'
  '.yyyyygggggggggg........'
  '.yyyyy.....ggggg........'
  '.yyyyy.....ggggg........'
  'tyyyyyt...tggggt........'
  '........................'
)

# --- Family: the five Evil Geese (M61/M62). They share one body on purpose —
# --- they are a gang — so every one of them is told apart above the neck and
# --- at the wing, never by colour. ---

Save-EnemyGrid 'evil_goose_vanguard' @(   # crested war-helm, shield on the wing
  '........................'
  '...............DD.......'
  '..............DDDD......'
  '............eeeeeeee....'
  '............errrrrree...'
  '............WWWWWWWWW...'
  '............WWKWWWWWWYYY'
  '............WWWWWWWWWYYY'
  '............WWWWWWWWW...'
  '............WWWWWWS.....'
  '............WWWWWS......'
  '...WWWWW....WWWWS.......'
  '..WWWWWWWWWWWWWWS.......'
  '.WWWeeeeeWWWWWWWS.......'
  'WWWerrrrreWWWWWWS.......'
  'WWWerLLLreWWWWWSS.......'
  'WWWerrrrreWWWWWS........'
  'WWWSeeeeeSWWWWSS........'
  'SWWSSSSSSSSSWWS.........'
  'SSSSSSSSSSSSSS..........'
  '.ZZSSSSSSSSSSS..........'
  '..ZZZZZZZZZZZ...........'
  '.....YY...YY............'
  '.....YY...YY............'
)

Save-EnemyGrid 'evil_goose_hexwing' @(    # tall cone hood, rune orb in escort
  '..............V.........'
  '.............VVV........'
  '.............VVV........'
  '............VVVVV.......'
  '............VVVVV.......'
  '...........VVVVVVV......'
  '...........VVCWWWWWWYYYY'
  '...........VVVWWWWWWYYYY'
  '............WWWWWWWW....'
  '............WWWWWWS.....'
  '.......VVV..WWWWWS......'
  '...WWWVGGGV.WWWWS.......'
  '..WWWWVGGGVWWWWWS.......'
  '.WWWWWVGGGVWWWWWS.......'
  'WWWWWWWVVVWWWWWWS.......'
  'WWWWWWWWWWWWWWWSS.......'
  'WWWWSSSSSSWWWWWS........'
  'WWWSSSSSSSSWWWSS........'
  'SWWSSSSSSSSSWWS.........'
  'SSSSSSSSSSSSSS..........'
  '.ZZSSSSSSSSSSS..........'
  '..ZZZZZZZZZZZ...........'
  '.....YY...YY............'
  '.....YY...YY............'
)

Save-EnemyGrid 'evil_goose_mender' @(     # broad mantle collar, cross-tipped rod
  '........................'
  '........................'
  '..............WWWW......'
  '.............WWWWWW.....'
  '.............WWWWWWW....'
  '............WWWWWWWW....'
  '............WWKWWWWWYYYY'
  '............WWWWWWWWYYYY'
  '..........HHHHHHHHHH....'
  '.........HHHHHHHHHHH....'
  '.........HHHHWWWWH......'
  '...WWWWWHHHHWWWWH.......'
  '..WWWWWWWWWWWWWWH....H..'
  '.WWWWWWWWWWWWWWWH...HHH.'
  'WWWWWWWWWWWWWWWWH..HHHHH'
  'WWWWWWWWWWWWWWWSS...HHH.'
  'WWWWSSSSSSWWWWWS.....H..'
  'WWWSSSHHHSSSWWSS.....s..'
  'SWWSSHHHHHSSWWS......s..'
  'SSSSSSHHHSSSSS.......s..'
  '.ZZSSSSSSSSSSS.......s..'
  '..ZZZZZZZZZZZ........s..'
  '.....YY...YY.........s..'
  '.....YY...YY............'
)

Save-EnemyGrid 'evil_goose_trickster' @(  # belled jester cap, motley wing
  '........................'
  '..........Y.......Y.....'
  '..........DD.....CC.....'
  '...........DD...CC......'
  '............DDDCC.......'
  '............WWWWWWWW....'
  '............WWKWWWWWYYYY'
  '............WWWWWWWWYYYY'
  '............WWWWWWWW....'
  '............WWWWWWS.....'
  '............WWWWWS......'
  '...WWWWW....WWWWS.......'
  '..WWWWWWWWWWWWWWS.......'
  '.WWWDDDVVVWWWWWWS.......'
  'WWWDDDVVVYWWWWWWS.......'
  'WWWDDVVVYYWWWWWSS.......'
  'WWWDVVVYYYWWWWWS........'
  'WWWSVVYYYYSWWWSS........'
  'SWWSSSSSSSSSWWS.........'
  'SSSSSSSSSSSSSS..........'
  '.ZZSSSSSSSSSSS..........'
  '..ZZZZZZZZZZZ...........'
  '.....YY...YY............'
  '.....YY...YY............'
)

Save-EnemyGrid 'evil_goose_bogfeather' @( # draggled, ragged wing, venom drip
  '........................'
  '........................'
  '..............WWSW......'
  '.............SWWSWW.....'
  '.............WSWWWSW....'
  '............SWWSWWWW....'
  '............WSKWSWWWYYYY'
  '............WWWSWWWWYYYY'
  '............WSWWSWWW....'
  '............WWSWWS......'
  '............xWWWS.......'
  '...WSWWS....WSWWS.......'
  '..WSWWSWWSWWWWWWS.......'
  '.WxxcxxcxWSWWWWWS.......'
  'WxcccxcccxWWWWWWS.......'
  'WxccxcccxcWWWWWSS.......'
  'WxcccxcccxSWWWWS........'
  'WSxcxxcxxSSWWWSS........'
  'SWSSSSSSSSSSWWS.........'
  'SSSSSSSSSSSSSS..........'
  '.ZZSSSSSSSSSSS..........'
  '..ZZZZZZZZZZZ...........'
  '.....YY...YY..v.........'
  '.....YY...YY............'
)

# --- Bosses (36x36). A boss is not a big normal enemy: it gets a structural
# --- crown or crest that is part of the head, a planted stance wider than any
# --- 24x24 sprite can reach, and equipment sized to be absurd. ---

Save-EnemyGrid 'boss_battle' @(     # generic boss fallback: a crowned hulk
  '............Y...Y...Y...............'
  '...........YYY.YYY.YYY..............'
  '...........YYYYYYYYYYY..............'
  '..........YYYYYYYYYYYYY.............'
  '..........YGYGYGYGYGYGY.............'
  '...........mmmmmmmmmmm..............'
  '..........mmmmmmmmmmmmm.............'
  '..........mmDDmmmmmDDmm.............'
  '..........mmmmmmmmmmmmm.............'
  '..........mmmmmmmmmmmmm.............'
  '...........mmmNNNNNmmm..............'
  '...........mmmmmmmmmm...............'
  '........mmmmmmmmmmmmmmmm............'
  '.....mmmmmmmmmmmmmmmmmmmmm..........'
  '...mmmmmmmmmmmmmmmmmmmmmmmmm........'
  '..mmmmmmmmmmmmmmmmmmmmmmmmmmm.......'
  '.mmmmmmmmBBBBBBBBBBBmmmmmmmmm.......'
  '.mmmmmmmBBBBBBBBBBBBBmmmmmmmm.......'
  'nmmmmmmmBBBBVVVVVBBBBmmmmmmmm.......'
  'nnmmmmmmBBBBBBBBBBBBBmmmmmmmn.......'
  'nnnmmmmmmBBBBBBBBBBBmmmmmmmnn.......'
  'nnnnmmmmmmmmmmmmmmmmmmmmmnnn........'
  'nnnnnmmmmmmmmmmmmmmmmmmmnnnn........'
  '.nnnnnmmmmmmmmmmmmmmmmmnnnn.........'
  '..nnnnnmmmmmmmmmmmmmmmnnnn..........'
  '...nnnnnmmmmmmmmmmmmmnnnn...........'
  '....nnnnnmmmmmmmmmmmnnnn............'
  '.....nnnnnmmmmmmmmmnnnn.............'
  '......nnnnnnnnnnnnnnnn..............'
  '.......nnnnnn.nnnnnnn...............'
  '.......nnnnn...nnnnnn...............'
  '.......nnnnn...nnnnnn...............'
  '.......nnnnn...nnnnnn...............'
  '......bnnnnn...nnnnnnb..............'
  '......bbnnnn...nnnnnbb..............'
  '....................................'
)

Save-EnemyGrid 'boss_keep_warden' @( # brute: hunched, cleaver like a door
  '....Y...Y...Y.......................'
  '...YYY.YYY.YYY......................'
  '...YYYYYYYYYYY......................'
  '..YYYYYYYYYYYYY.....................'
  '..YGYGYGYGYGYGY.....................'
  '...hhhhhhhhhhh..........LLLLLLLLLL..'
  '..hhhhhhhhhhhhh.........LLLLLLLLLLL.'
  '..hhDDhhhhhDDhh.........LLLLLLLLLLL.'
  '..hhhhhhhhhhhhh.........LLLLLLLLLLL.'
  '...hhNhhhhhNhh..........LrrrrrrrrrL.'
  '....hhhhhhhhh...........Lrrrrrrrrrr.'
  '...ggggggggggggg........Lrrrrrrrrrr.'
  '..ggggggggggggggggg.....Lrrrrrrrrr..'
  '.ggggggggggggggggggggg..Lrrrrrrrr...'
  'gggggggggggggggggggggggg.Lrrrrrr....'
  'ggggghhhhhhhhhgggggggggg..sss.......'
  'gggghhhhhhhhhhhggggggggg....sss.....'
  'ggghhhhhhhhhhhhhgggggggg....sss.....'
  'ggghhhhhhhhhhhhhgggggggg....sss.....'
  'ggghhhhhhhhhhhhhggggggg.....sss.....'
  'gggghhhhhhhhhhhgggggggg.....sss.....'
  'ggggghhhhhhhhhggggggg.......sss.....'
  'yggggggggggggggggggg................'
  'yygggggggggggggggggg................'
  'yyyggggggggggggggggg................'
  'yyyygggggggggggggggg................'
  'yyyyyggggggggggggggg................'
  'yyyyyygggggggggggggg................'
  'yyyyyyyyggggggggggg.................'
  'yyyyyyyy....gggggg..................'
  'yyyyyyy.....gggggg..................'
  '.yyyyyy.....gggggg..................'
  '.yyyyyy.....gggggg..................'
  'tyyyyyyt...tggggggt.................'
  'ttyyyytt...ttgggggt.................'
  '....................................'
)

Save-EnemyGrid 'boss_crystal_sorcerer' @( # sorcerer: narrow column, staff shard
  '..........................CCC.......'
  '.........................CGGGC......'
  '........Y...Y...Y.......CGGGGGC.....'
  '.......YYY.YYY.YYY.....CGGGGGGGC....'
  '.......YYYYYYYYYYY.....CGGGGGGGC....'
  '......YYYYYYYYYYYYY....CGGGGGGGC....'
  '......YGYGYGYGYGYGY.....CGGGGGC.....'
  '.......ooooooooooo......CCGGGCC.....'
  '......ooooooooooooo......CCCCC......'
  '......ooCCooooCCooo.......CCC.......'
  '......ooooooooooooo........C........'
  '.......ooooooooooo.........s........'
  '....oooooooooooooooo.......s........'
  '...pooooooooooooooooo......s........'
  '..pooooooooooooooooooo.....s........'
  '.poooooooooooooooooooop....s........'
  '.pooooCCCCCCCCCCCoooooo....s........'
  '.poooCCGGGGGGGGGCCooooo....s........'
  '.poooCCGGGGGGGGGCCooooo....s........'
  '.pooooCCCCCCCCCCCoooooo....s........'
  '.poooooooooooooooooooop....s........'
  '..iooooooooooooooooooi.....s........'
  '..iiooooooooooooooooii.....s........'
  '..iioooooooooooooooiii.....s........'
  '...iiooooooooooooooii......s........'
  '...iiiooooooooooooiii......s........'
  '....iiioooooooooooii.......s........'
  '....iiiiooooooooiiii.......s........'
  '.....iiiiooooooiiii........s........'
  '.....iiiiiooooiiiii........s........'
  '......uiiiiiiiiiiu..................'
  '......uuiiiiiiiiuu..................'
  '.....uuuiiiiiiiiuuu.................'
  '....uuuuuiiiiiiuuuuu................'
  '...uuuuuuuuuuuuuuuuuu...............'
  '....................................'
)

Save-EnemyGrid 'boss_hollow_commander' @( # commander: banner and broadsword
  'DDDDDDDDDD..........................'
  'DDDDDDDDDDD.......Y...Y...Y.........'
  'DDDYYYYYDDD......YYY.YYY.YYY........'
  'DDYYYYYYYDD......YYYYYYYYYYY........'
  'DDDYYYYYDDD.....YYYYYYYYYYYYY.......'
  'DDDDDDDDDDD.....YGYGYGYGYGYGY.......'
  'DDDDDDDDDD.......eeeeeeeeeee........'
  'DDDDDDDDD.......eeeeeeeeeeeee.......'
  'sDDDDDDD........eeDDeeeeeDDee.......'
  's...............eeeeeeeeeeeee.......'
  's................eeeeeeeeeee...LLL..'
  's................eeeeeeeeee....LLL..'
  's............rrrrrrrrrrrrrrrr..LLL..'
  's..........rrrrrrrrrrrrrrrrrrr.LLL..'
  's.........rrrrrrrrrrrrrrrrrrrr.LLL..'
  's........rrrreeeeeeeeeeeerrrrr.LLL..'
  's........rrreeeeeeeeeeeeeerrrrLLLLL.'
  's........rrreeeeVVVVVeeeeerrrr.LLL..'
  's........rrreeeeeeeeeeeeeerrrr.LLL..'
  's........rrrreeeeeeeeeeeerrrrr.LLL..'
  's.........rrrrrrrrrrrrrrrrrrrr.LLL..'
  's..........rrrrrrrrrrrrrrrrrr..LLL..'
  's...........wwwwwwwwwwwwwwww...LLL..'
  's...........wwwwwwwwwwwwwwww...LLL..'
  's...........wwwwwwwwwwwwwwww...LLL..'
  's...........wwwwwwwwwwwwwwww....L...'
  's............wwwwwwwwwwwwww.........'
  's............wwwwwwwwwwwwww.........'
  's.............wwwwww.wwwwww.........'
  's.............wwwww...wwwww.........'
  's.............wwwww...wwwww.........'
  '..............wwwww...wwwww.........'
  '.............qwwwww...wwwwwq........'
  '.............qqwwww...wwwwqq........'
  '.............qqqqqq...qqqqqq........'
  '....................................'
)

Save-EnemyGrid 'boss_rush_tyrant' @( # rush: four arms, nothing but forward
  '..........Y...Y...Y.................'
  '.........YYY.YYY.YYY................'
  '.........YYYYYYYYYYY................'
  '........YYYYYYYYYYYYY...............'
  '........YGYGYGYGYGYGY...............'
  '.........mmmmmmmmmmm................'
  '........mmmmmmmmmmmmm...............'
  '........mmDDmmmmmDDmm...............'
  '........mmmmmmmmmmmmm...............'
  '.........mNNNNNNNNNm................'
  '..........mmmmmmmmm.................'
  'BB.....mmmmmmmmmmmmmmm.......BB.....'
  'BBB..mmmmmmmmmmmmmmmmmmm....BBB.....'
  'BBBBmmmmmmmmmmmmmmmmmmmmm.BBBB......'
  '.BBBBmmmmmmmmmmmmmmmmmmmBBBB........'
  '..BBBBmmmmmmmmmmmmmmmmmBBBB.........'
  '...BBBmmmmmmmmmmmmmmmmmBBB..........'
  'BB....mmmmmVVVVVVVmmmmm.....BB......'
  'BBB...mmmmmmmmmmmmmmmmm....BBB......'
  'BBBBmmmmmmmmmmmmmmmmmmmmm.BBBB......'
  '.BBBBmmmmmmmmmmmmmmmmmmmBBBB........'
  '..BBBmmmmmmmmmmmmmmmmmmmBBB.........'
  '....mmmmmmmmmmmmmmmmmmmmm...........'
  '....nmmmmmmmmmmmmmmmmmmmn...........'
  '....nnmmmmmmmmmmmmmmmmmnn...........'
  '.....nnmmmmmmmmmmmmmmmnn............'
  '......nnmmmmmmmmmmmmmnn.............'
  '.......nnmmmmmmmmmmmnn..............'
  '........nnnnnnnnnnnnn...............'
  '........nnnnnn.nnnnnn...............'
  '........nnnnn...nnnnn...............'
  '........nnnnn...nnnnn...............'
  '.......bnnnnn...nnnnnb..............'
  '.......bnnnnn...nnnnnb..............'
  '......bbnnnnn...nnnnnbb.............'
  '....................................'
)

Save-EnemyGrid 'boss_deep_king' @(  # brute: faceted colossus, bearded axe
  '.........Y...Y...Y..................'
  '........YYY.YYY.YYY.................'
  '........YYYYYYYYYYY.................'
  '.......YYYYYYYYYYYYY..ssLLLLLLLL....'
  '.......YGYGYGYGYGYGY..ssLLLLLLLL....'
  '........wwwwwwwwwww...ssLLLLLLLL....'
  '.......wwwwwwwwwwwww..ssLLLLLLLL....'
  '.......wwCCwwwwwCCww..ssLLLLLLLL....'
  '.......wwwwwwwwwwwww..ssLLLLLLL.....'
  '........wwwwwwwwwww...ssLLLLLL......'
  '.........wwwwwwwww....ssLLLLL.......'
  '....wwwwwwwwwwwwwwwww.ssLLLL........'
  '..wwwwwwwwwwwwwwwwwwwwss............'
  '.wwwwwwwwwwwwwwwwwwwwwss............'
  'wwwwwwwwwwwwwwwwwwwwwwss............'
  'wwwwwCCCCCCCCCCCCCwwwwss............'
  'wwwwCCGGGGGGGGGGGCCwwwss............'
  'wwwwCCGGGGGGGGGGGCCwwwss............'
  'wwwwwCCCCCCCCCCCCCwwwwss............'
  'wwwwwwwwwwwwwwwwwwwwwwss............'
  'qwwwwwwwwwwwwwwwwwwwwwss............'
  'qqwwwwwwwwwwwwwwwwwwwwss............'
  'qqqwwwwwwwwwwwwwwwwwwqss............'
  '.qqqwwwwwwwwwwwwwwwwqqss............'
  '..qqqwwwwwwwwwwwwwwqq.ss............'
  '...qqqwwwwwwwwwwwwqq..ss............'
  '....qqqwwwwwwwwwwqqq................'
  '.....qqqwwwwwwwwqqq.................'
  '......qqqqqqqqqqqq..................'
  '......qqqqq..qqqqq..................'
  '......qqqqq..qqqqq..................'
  '......qqqqq..qqqqq..................'
  '.....qqqqqq..qqqqqq.................'
  '.....wwwwww..wwwwww.................'
  '....qqqqqqq..qqqqqqq................'
  '....................................'
)

Save-EnemyGrid 'boss_blight_matron' @( # sorcerer: cauldron staff, spore veil
  '.........................vvv........'
  '........................vcccv.......'
  '........Y...Y...Y......vcHHHcv......'
  '.......YYY.YYY.YYY.....vcHHHcv......'
  '.......YYYYYYYYYYY.....vcHHHcv......'
  '......YYYYYYYYYYYYY.....vcccv.......'
  '......YGYGYGYGYGYGY......vvv........'
  '.......ccccccccccc........v.........'
  '......ccccccccccccc.......s.........'
  '......ccVVcccccVVcc.......s.........'
  '......ccccccccccccc.......s.........'
  '.......cccNNNNNccc........s.........'
  '....xxxxccccccccxxxx......s.........'
  '...xxxxxxxxxxxxxxxxxx.....s.........'
  '..xxxxxxxxxxxxxxxxxxxx....s.........'
  '.xxxxxxxxxxxxxxxxxxxxxx...s.........'
  '.xxxxVxxxxVxxxxVxxxxxx....s.........'
  '.xxxxxxxxxxxxxxxxxxxxx....s.........'
  '.xxxVxxxxVxxxxVxxxxVxx....s.........'
  '.xxxxxxxxxxxxxxxxxxxxx....s.........'
  '.xxxxxVxxxxVxxxxVxxxxx....s.........'
  '.zxxxxxxxxxxxxxxxxxxxz....s.........'
  '.zzxxxxVxxxxVxxxxVxxzz....s.........'
  '.zzzxxxxxxxxxxxxxxxzzz....s.........'
  '..zzzxxxxVxxxxVxxxzzz...............'
  '..zzzzxxxxxxxxxxxzzzz...............'
  '...zzzzxxxxVxxxxxzzz................'
  '...zzzzzxxxxxxxxzzzz................'
  '....zzzzzxxxxxxzzzz.................'
  '....zzzzzzxxxxzzzzz.................'
  '.....zzzzzzzzzzzzz..................'
  '.....zzzzzzzzzzzzz..................'
  '....zzzzzzzzzzzzzzz.................'
  '...zzzzzzzzzzzzzzzzz................'
  '..zzzzzzzzzzzzzzzzzzz...............'
  '....................................'
)

Save-EnemyGrid 'boss_sand_warlord' @( # brute: khopesh curved like a scythe
  '..........Y...Y...Y.................'
  '.........YYY.YYY.YYY......OOOOOO....'
  '.........YYYYYYYYYYY....OOOOOOOOOO..'
  '........YYYYYYYYYYYYY..OOOOOOOOOOOO.'
  '........YGYGYGYGYGYGY.OOOOOO...OOOO.'
  '.........fffffffffff..OOOOO.....OOO.'
  '........fffffffffffff.OOOO..........'
  '........ffDDfffffDDff.OOOO..........'
  '........fffffffffffff.OOOO..........'
  '.........ffNNNNNNNff..OOOOO.........'
  '..........fffffffff....OOOOO........'
  '.....dddddddfffffddddddd.OOOO.......'
  '...ddddddddddddddddddddd..OOO.......'
  '..dddddddddddddddddddddddd.OO.......'
  '.ddddddddddddddddddddddddd.s........'
  '.dddddffffffffffffffdddddd.s........'
  '.ddddfffffffffffffffddddd..s........'
  '.ddddffffffffffffffffdddd..s........'
  '.ddddfffffffffffffffddddd..s........'
  '.dddddffffffffffffffdddddd.s........'
  '.sddddddddddddddddddddddds.s........'
  '.ssdddddddddddddddddddddss.s........'
  '.sssddddddddddddddddddsss..s........'
  '..sssdddddddddddddddsss....s........'
  '...sssddddddddddddsss......s........'
  '....sssdddddddddsss.................'
  '.....sssdddddddsss..................'
  '......ssdddddddss...................'
  '.......ssssssssss...................'
  '.......sssss.sssss..................'
  '.......ssss...ssss..................'
  '.......ssss...ssss..................'
  '......assss...ssssa.................'
  '......aasss...sssaa.................'
  '.....aaaaaa...aaaaaa................'
  '....................................'
)

Save-EnemyGrid 'boss_frost_monarch' @( # sorcerer: throne of icicles, no legs
  '.....C....C....C....C....C..........'
  '.....CC..CC....CC..CC....CC.........'
  '......C...C.....C...C.....C.........'
  '........Y...Y...Y...................'
  '.......YYY.YYY.YYY..................'
  '.......YYYYYYYYYYY.....CCCCCCC......'
  '......YYYYYYYYYYYYY...CGGGGGGGC.....'
  '......YGYGYGYGYGYGY...CGGGGGGGC.....'
  '.......ooooooooooo....CGGGGGGGC.....'
  '......ooooooooooooo....CGGGGGC......'
  '......ooCCooooCCooo.....CCCCC.......'
  '......ooooooooooooo.......C.........'
  '.......ooNNNNNNNoo........C.........'
  '....ppppoooooooppppp......C.........'
  '..ppppppppppppppppppp.....C.........'
  '.pppppppppppppppppppppp...C.........'
  '.ppppCCCCCCCCCCCCCppppp...C.........'
  '.pppCCGGGGGGGGGGGCCpppp...C.........'
  '.pppCCGGGGGGGGGGGCCpppp...C.........'
  '.ppppCCCCCCCCCCCCCppppp...C.........'
  '.oppppppppppppppppppppo...C.........'
  '.ooppppppppppppppppppoo...C.........'
  '.ooopppppppppppppppooo..............'
  '..ooopppppppppppppooo...............'
  '...ooopppppppppppooo................'
  '....C.ooopppppooo.C.................'
  '...CC..oooooooooo..CC...............'
  '...C.C..oooooooo..C.C...............'
  '..CC..C..oooooo..C..CC..............'
  '..C....C..oooo..C....C..............'
  '.CC.....C..oo..C.....CC.............'
  '.C.......C....C.......C.............'
  'CC........C..C........CC............'
  'C..........CC..........C............'
  'C..........................C........'
  '....................................'
)

Save-EnemyGrid 'boss_obsidian_colossus' @( # brute: sheer black-glass slab
  '..........Y...Y...Y.................'
  '.........YYY.YYY.YYY................'
  '.........YYYYYYYYYYY................'
  '........YYYYYYYYYYYYY...............'
  '........YGYGYGYGYGYGY...............'
  '.........nnnnnnnnnnn................'
  '........nnnnnnnnnnnnn...............'
  '........nnVVnnnnnVVnn...............'
  '........nnnnnnnnnnnnn...............'
  '.........nnnnnnnnnnn................'
  'bbbb......nnnnnnnnn.......bbbb......'
  'bnnnb...nnnnnnnnnnnnn....bnnnb......'
  'bnnnb..nnnnnnnnnnnnnnn...bnnnb......'
  'bnnnb.nnnnnnnnnnnnnnnnn..bnnnb......'
  'bnnnb.nnnnnnnnnnnnnnnnn..bnnnb......'
  'bnnnbnnnnBBBBBBBBBnnnnnn.bnnnb......'
  'bnnnbnnnBBBBBBBBBBBnnnnn.bnnnb......'
  'bnnnbnnnBBBVVVVVBBBnnnnn.bnnnb......'
  'bnnnbnnnBBBBBBBBBBBnnnnn.bnnnb......'
  'bnnnbnnnnBBBBBBBBBnnnnnn.bnnnb......'
  'bnnnb.nnnnnnnnnnnnnnnnn..bnnnb......'
  'bnnnb.nnnnnnnnnnnnnnnnn..bnnnb......'
  'bnnnb.nnnnnnnnnnnnnnnnn..bnnnb......'
  'bbbbb.nnnnnnnnnnnnnnnnn..bbbbb......'
  '......nnnnnnnnnnnnnnnnn.............'
  '......nnnnnnnnnnnnnnnnn.............'
  '......nnnnnnnnnnnnnnnnn.............'
  '......bnnnnnnnnnnnnnnnb.............'
  '......bbnnnnnnnnnnnnnbb.............'
  '.......bnnnnnn.nnnnnnb..............'
  '........nnnnn...nnnnn...............'
  '........nnnnn...nnnnn...............'
  '.......bnnnnn...nnnnnb..............'
  '.......bnnnnn...nnnnnb..............'
  '......bbbnnnn...nnnnbbb.............'
  '....................................'
)

Save-EnemyGrid 'boss_hollow_sovereign' @( # commander: throne-backed revenant
  'rrr...........................rrr...'
  'rrrr.........Y...Y...Y.......rrrr...'
  'rrrr........YYY.YYY.YYY......rrrr...'
  'rrrr........YYYYYYYYYYY......rrrr...'
  'rrrr.......YYYYYYYYYYYYY.....rrrr...'
  'rrrr.......YGYGYGYGYGYGY.....rrrr...'
  'rrrr........NNNNNNNNNNN......rrrr...'
  'rrrr.......NNNNNNNNNNNNN.....rrrr...'
  'rrrr.......NNDDNNNNNDDNN.....rrrr...'
  'rrrr.......NNNNNNNNNNNNN.....rrrr...'
  'rrrr........NZZZZZZZZZN......rrrr...'
  'rrrr.........NNNNNNNNN.......rrrr...'
  'rrrr.....eeeeeeeeeeeeeee.....rrrr...'
  'rrrr...eeeeeeeeeeeeeeeeeee...rrrr...'
  'rrrreeeeeeeeeeeeeeeeeeeeeeeeerrrr...'
  'rrrreeeewwwwwwwwwwwwweeeeeeeerrrr...'
  'rrrreeewwwwwwwwwwwwwwweeeeeeerrrr...'
  'rrrreeewwwwwVVVVVwwwwwweeeeeerrrr...'
  'rrrreeewwwwwwwwwwwwwwweeeeeeerrrr...'
  'rrrreeeewwwwwwwwwwwwweeeeeeeerrrr...'
  'rrrr.eeeeeeeeeeeeeeeeeeeeeee.rrrr...'
  'rrrr..eeeeeeeeeeeeeeeeeeeee..rrrr...'
  'rrrr...eeeeeeeeeeeeeeeeeee...rrrr...'
  'rrrr....eeeeeeeeeeeeeeeee....rrrr...'
  'rrrr.....wwwwwwwwwwwwwww.....rrrr...'
  'rrrr.....wwwwwwwwwwwwwww.....rrrr...'
  'rrrr.....wwwwwwwwwwwwwww.....rrrr...'
  'rrrr.....wwwwwwwwwwwwwww.....rrrr...'
  'rrrr......wwwwww.wwwwww......rrrr...'
  'rrrr......wwwww...wwwww......rrrr...'
  'rrrr......wwwww...wwwww......rrrr...'
  'rrrr.....qwwwww...wwwwwq.....rrrr...'
  'rrrr.....qwwwww...wwwwwq.....rrrr...'
  'rrrr....qqqwwww...wwwwqqq....rrrr...'
  'rrrr..........................rrrr..'
  '....................................'
)

Save-EnemyGrid 'boss_abyssal_tyrant' @( # rush: maw for a torso, tusked
  '.........Y...Y...Y..................'
  '........YYY.YYY.YYY.................'
  '........YYYYYYYYYYY.................'
  '.......YYYYYYYYYYYYY................'
  '.......YGYGYGYGYGYGY................'
  '........hhhhhhhhhhh.................'
  '.......hhhhhhhhhhhhh................'
  '.......hhYYhhhhhYYhh................'
  '.......hhhhhhhhhhhhh................'
  '........hhhhhhhhhhh.................'
  'ttt......hhhhhhhhh.........ttt......'
  'tggt...gggggggggggggg.....tggt......'
  'tggt.gggggggggggggggggg...tggt......'
  'tggtgggggggggggggggggggg..tggt......'
  'tggggggggggggggggggggggggggggt......'
  'tgggggttttttttttttttttggggggt.......'
  'tgggttNtNtNtNtNtNtNtNtttgggt........'
  'tgggtNtNtNtNtNtNtNtNtNtgggt.........'
  'tggggtNtNtNtNtNtNtNtNtggggt.........'
  'tggggttNtNtNtNtNtNtNttggggt.........'
  'tgggggttttttttttttttggggggt.........'
  'tggggggggggggggggggggggggt..........'
  'ytgggggggggggggggggggggty...........'
  'yytggggggggggggggggggtyy............'
  'yyytgggggggggggggggtyyy.............'
  'yyyytgggggggggggggtyyy..............'
  'yyyyytgggggggggggtyy................'
  'yyyyyytgggggggggtyy.................'
  'yyyyyyyttttttttty...................'
  'yyyyyyy.....yyyy....................'
  'yyyyyy......yyyy....................'
  'yyyyyy......yyyy....................'
  'tyyyyt......tyyyt...................'
  'tyyyyt......tyyyt...................'
  'ttyytt......ttyyt...................'
  '....................................'
)

Save-EnemyGrid 'boss_dread_sovereign' @( # sorcerer: winged mantle of afflictions
  'VV......................VV..........'
  'VVV....Y...Y...Y.......VVV..........'
  'VVVV..YYY.YYY.YYY.....VVVV..........'
  'VVVVV.YYYYYYYYYYY....VVVVV..........'
  'VVVVVYYYYYYYYYYYYY..VVVVVV..........'
  'VVVVVYGYGYGYGYGYGY..VVVVVV..........'
  'VVVVVV.nnnnnnnnn...VVVVVVV..........'
  'VVVVVVnnnnnnnnnnn..VVVVVVV..........'
  'VVVVVVnnVVnnnVVnn..VVVVVVV..........'
  'VVVVVVnnnnnnnnnnn..VVVVVVV..........'
  'VVVVVVV.nnnnnnn...VVVVVVVV..........'
  'VVVVVVVVnnnnnnnnnVVVVVVVV...........'
  '.VVVVVnnnnnnnnnnnnnVVVVV............'
  '..VVVnnnnnnnnnnnnnnnVVV.............'
  '...VVnnnnnnnnnnnnnnnVV..............'
  '....nnnnBBBBBBBBBnnnnn..............'
  '....nnnBBBBBBBBBBBnnnn..............'
  '....nnnBBBVVVVVBBBnnnn..............'
  '....nnnBBBBBBBBBBBnnnn..............'
  '....nnnnBBBBBBBBBnnnnn..............'
  '....nnnnnnnnnnnnnnnnnn..............'
  '....bnnnnnnnnnnnnnnnnb..............'
  '....bbnnnnnnnnnnnnnnbb..............'
  '.....bnnnnnnnnnnnnnnb...............'
  '.....bbnnnnnnnnnnnnbb...............'
  '......bnnnnnnnnnnnnb................'
  '......bbnnnnnnnnnnbb................'
  '.......bnnnnnnnnnnb.................'
  '.......bbnnnnnnnnbb.................'
  '........bnnnnnnnnb..................'
  '........bbnnnnnnbb..................'
  '.........bnnnnnnb...................'
  '.........bbnnnnbb...................'
  '..........bnnnnb....................'
  '..........bbnnbb....................'
  '....................................'
)

Save-EnemyGrid 'boss_the_hollow_king' @( # the King: widest crown, orb sceptre
  '...Y....Y...Y...Y....Y..............'
  '..YYY..YYY.YYY.YYY..YYY.............'
  '..YYYYYYYYYYYYYYYYYYYYY.............'
  '.YYYYYYYYYYYYYYYYYYYYYYY............'
  '.YGYGYGYGYGYGYGYGYGYGYGY............'
  '.YYYYYYYYYYYYYYYYYYYYYYY............'
  '.....nnnnnnnnnnnnnnn......YYYY......'
  '....nnnnnnnnnnnnnnnnn....YYYYYY.....'
  '....nnnYYnnnnnnnYYnnn...YYDDDDYY....'
  '....nnnnnnnnnnnnnnnnn...YYDDDDYY....'
  '.....nnnnnnnnnnnnnnn.....YYYYYY.....'
  '......nnnnnnnnnnnnn.......YYYY......'
  '...VVVVVVVVVVVVVVVVVVV.....Y........'
  '..VVVVVVVVVVVVVVVVVVVVV....Y........'
  '.VVVVVVVVVVVVVVVVVVVVVVV...Y........'
  'VVVVnnnnnnnnnnnnnnnnnVVVV..Y........'
  'VVVnnnnnnnnnnnnnnnnnnnVVV..Y........'
  'VVVnnnnnYYYYYYYYYnnnnnVVV..Y........'
  'VVVnnnnYYYYYYYYYYYnnnnVVV..Y........'
  'VVVnnnnnYYYYYYYYYnnnnnVVV..Y........'
  'VVVVnnnnnnnnnnnnnnnnnVVVV..Y........'
  '.VVVnnnnnnnnnnnnnnnnnVVV...Y........'
  '..VVnnnnnnnnnnnnnnnnnVV....Y........'
  '...VnnnnnnnnnnnnnnnnnV.....Y........'
  '....nnnnnnnnnnnnnnnnn......Y........'
  '....nnnnnYYYYYYYnnnnn...............'
  '....nnnnnnnnnnnnnnnnn...............'
  '....nnnnnnnnnnnnnnnnn...............'
  '....bnnnnnnnnnnnnnnnb...............'
  '....bbnnnnnn.nnnnnnbb...............'
  '.....bnnnnn...nnnnnb................'
  '.....bnnnnn...nnnnnb................'
  '....YYYYYYY...YYYYYYY...............'
  '....bnnnnnn...nnnnnnb...............'
  '...bbbnnnnn...nnnnnbbb..............'
  '....................................'
)

Save-EnemyGrid 'boss_deadly_duck' @( # the Duck: no crown. Bulk, wings, pond.
  '....................................'
  '.......................WWWW.........'
  '......................WWWWWWW.......'
  '.....................WWWWWWWWW......'
  '....................WWWWWWWWWWW.....'
  '...................WWWWKWWWWWWWW....'
  '...................WWWWWWWWWWWWWYYYY'
  '...................WWWWWWWWWWWWWYYYY'
  '....................WWWWWWWWWWW.YYY.'
  '.....................WWWWWWWWS......'
  '......................WWWWWWS.......'
  '.......................WWWWS........'
  '.ZZZ....................WWWS........'
  'ZSSSZ...................WWWS........'
  'ZSWWSZ.................WWWWS........'
  'ZSWWWSZ...............WWWWWS........'
  'ZSWWWWSZ.....WWWWWWWWWWWWWS.........'
  'ZSWWWWWSZ.WWWWWWWWWWWWWWWWS.........'
  'ZSWWWWWWSWWWWWWWWWWWWWWWWS..........'
  'ZSWWWWWWWWWWWWWWWWWWWWWWWS..........'
  'ZSSWWWWWWWWWWWWWWWWWWWWWS...........'
  'ZZSSWWWWWWWWWWWWWWWWWWWWS...........'
  '.ZZSSSWWWWWWWWWWWWWWWWWS............'
  '..ZZSSSSSSSSSSSSSSSSSSS.............'
  '...ZZSSSSSSSSSSSSSSSSS..............'
  '....ZZSSSSSSSSSSSSSSS...............'
  '.....ZZZSSSSSSSSSSSS................'
  '.......ZZZZZZZZZZZZ.................'
  '.........YY....YY...................'
  '.........YY....YY...................'
  'iiiiiiiiiYYiiiiYYiiiiiiiiiiiiiiiiiii'
  'oiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiio'
  'ioooiiiiiioooiiiiiiiooooiiiiiiioooii'
  'iiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiii'
  'uuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuu'
  '....................................'
)

#<<<M73-ENEMY-SPRITES>>>
Write-Output 'Generating M27 service backgrounds...'

# Full-screen (426x240) service backgrounds. Legibility is the binding
# constraint (art_bible §7): each is a distinct dark tinted gradient + a
# low-alpha themed motif biased to the top/edges, so overlaid light text keeps
# its contrast. Graphics fills (fast) instead of per-pixel SetPixel. Appended
# and RNG-reseeded so existing/enemy PNGs stay byte-identical.
$script:rng = 27270000
$BW = 426; $BH = 240

function Ca([int]$a, [string]$hex) { $c = C $hex; [System.Drawing.Color]::FromArgb($a, $c.R, $c.G, $c.B) }
function New-Bg([string]$topHex, [string]$botHex) {
  $b = New-Object System.Drawing.Bitmap($BW, $BH, [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
  $g = [System.Drawing.Graphics]::FromImage($b); $g.SmoothingMode = 'None'
  $rect = New-Object System.Drawing.Rectangle(0, 0, $BW, $BH)
  $grad = New-Object System.Drawing.Drawing2D.LinearGradientBrush($rect, (C $topHex), (C $botHex), 90.0)
  $g.FillRectangle($grad, $rect); $grad.Dispose()
  $g.FillRectangle((New-Object System.Drawing.SolidBrush(Ca 70 '#0E0C14')), 0, $BH - 26, $BW, 26)
  return @($b, $g)
}
function GFill($g, [int]$a, [string]$hex, [int]$x, [int]$y, [int]$w, [int]$h) {
  $g.FillRectangle((New-Object System.Drawing.SolidBrush(Ca $a $hex)), $x, $y, $w, $h)
}
function GEll($g, [int]$a, [string]$hex, [int]$x, [int]$y, [int]$w, [int]$h) {
  $g.FillEllipse((New-Object System.Drawing.SolidBrush(Ca $a $hex)), $x, $y, $w, $h)
}
function Motes($b, [int]$n, [string]$hex) {
  for ($i = 0; $i -lt $n; $i++) { $x = [int]((Rnd) * $BW); $y = [int]((Rnd) * ($BH - 40)); P $b $x $y $hex }
}
function SaveBg($pair, [string]$name) { $pair[1].Dispose(); SaveImg $pair[0] "backgrounds/$name.png" }

# Inn - warm restful interior: lantern glow + a bed by the wall.
$r = New-Bg '#241A18' '#150F0D'
GEll $r[1] 30 '#8A6D48' 300 6 130 104; GEll $r[1] 46 $PAL.gold 332 20 82 66; GEll $r[1] 70 $PAL.gold 356 34 34 30
GFill $r[1] 40 $PAL.earth2 4 154 78 34; GFill $r[1] 60 $PAL.earth3 4 154 30 13
Motes $r[0] 34 $PAL.earth3; SaveBg $r 'inn'

# Item Shop - cool market stall: shelf bands + goods on the side walls.
$r = New-Bg '#16182A' '#0D0F18'
foreach ($sy in 40, 78, 116, 154) { GFill $r[1] 34 $PAL.wat2 0 $sy 70 4; GFill $r[1] 34 $PAL.wat2 356 $sy 70 4 }
foreach ($bx in 8, 22, 36, 50) { GFill $r[1] 46 $PAL.wat3 $bx 30 8 8; GFill $r[1] 46 $PAL.wat3 (368 + $bx - 8) 68 8 10 }
Motes $r[0] 26 $PAL.wat3; SaveBg $r 'item_shop'

# Equip Shop - steel forge: hanging arms up top + an anvil block low.
$r = New-Bg '#1E2028' '#101218'
foreach ($hx in 30, 70, 356, 396) { GFill $r[1] 40 $PAL.stone3 $hx 0 3 40; GFill $r[1] 46 $PAL.stone4 ($hx - 6) 34 15 10 }
GFill $r[1] 40 $PAL.stone3 176 196 74 22; GFill $r[1] 46 $PAL.stone2 168 190 90 8; GFill $r[1] 34 $PAL.stone4 200 176 26 16
GEll $r[1] 34 $PAL.danger 356 150 60 46; Motes $r[0] 22 $PAL.stone4; SaveBg $r 'equip_shop'

# Training Hall - martial dojo: faint crossed blades + a wall target.
$r = New-Bg '#241618' '#140D0F'
GFill $r[1] 26 $PAL.clsKnight 40 30 8 180; GFill $r[1] 26 $PAL.clsKnight 378 30 8 180
$r[1].TranslateTransform(213, 120); $r[1].RotateTransform(35)
GFill $r[1] 24 $PAL.stone4 -120 -4 240 8; $r[1].RotateTransform(-70); GFill $r[1] 24 $PAL.stone4 -120 -4 240 8
$r[1].ResetTransform()
GEll $r[1] 34 $PAL.maroon 348 18 66 66; GEll $r[1] 40 $PAL.danger 366 36 30 30; GEll $r[1] 55 $PAL.gold 378 48 6 6
Motes $r[0] 22 $PAL.maroon; SaveBg $r 'training_hall'

# Scoreboard - hall of honor: violet glow + flanking pillars.
$r = New-Bg '#1A1626' '#100C18'
GEll $r[1] 34 $PAL.violet 150 -40 126 110
foreach ($px in 18, 386) { GFill $r[1] 40 $PAL.stone3 $px 24 22 190; GFill $r[1] 50 $PAL.stone4 ($px - 4) 22 30 8; GFill $r[1] 50 $PAL.stone4 ($px - 4) 206 30 8 }
GFill $r[1] 40 $PAL.gold 24 26 10 186; GFill $r[1] 40 $PAL.gold 392 26 10 186
Motes $r[0] 24 $PAL.violet; SaveBg $r 'scoreboard'

# Guild - adventurers' lodge: hanging banner + map pins.
$r = New-Bg '#16201A' '#0D140E'
GFill $r[1] 40 $PAL.veg2 190 0 46 44; $r[1].FillPolygon((New-Object System.Drawing.SolidBrush(Ca 40 $PAL.veg2)), [System.Drawing.Point[]]@((New-Object System.Drawing.Point(190, 44)), (New-Object System.Drawing.Point(236, 44)), (New-Object System.Drawing.Point(213, 58))))
GFill $r[1] 55 $PAL.veg3 206 12 14 14
foreach ($p in @(@(60, 70), @(120, 150), @(330, 90), @(370, 170), @(90, 190))) { GEll $r[1] 45 $PAL.gold $p[0] $p[1] 6 6 }
Motes $r[0] 26 $PAL.veg3; SaveBg $r 'guild'

# ============================ M32 town-ladder variants ============================
# Per-town exterior tiles and per-town service interiors (owner: per-town
# interiors). Each higher town shades the town-1 base art progressively darker
# and more sinister via a ColorMatrix (fast, deterministic) - a palette
# progression, not new layouts. Town 1 keeps the base files unchanged, so this
# section only ADDS PNGs and existing outputs stay byte-identical.
Write-Output 'Generating M32 town-ladder variants...'

# Per-town shade: result = base*(darken*(1-blend)) + tint*blend. Darker and more
# tinted toward a sinister hue as the town index climbs (towns 2..7).
$townDarken = @{ 2=0.90; 3=0.84; 4=0.78; 5=0.72; 6=0.66; 7=0.60 }
$townBlend  = @{ 2=0.10; 3=0.15; 4=0.20; 5=0.26; 6=0.32; 7=0.40 }
$townTint   = @{ 2='#26364E'; 3='#2C3A2A'; 4='#1E2A44'; 5='#2E2442'; 6='#3A2030'; 7='#280E1A' }

function ShadeCopy([string]$srcRel, [string]$dstRel, [int]$town) {
  $srcPath = Join-Path $outRoot $srcRel
  $src = [System.Drawing.Bitmap]::FromFile($srcPath)
  $dst = New-Object System.Drawing.Bitmap($src.Width, $src.Height, [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
  $g = [System.Drawing.Graphics]::FromImage($dst)
  $g.SmoothingMode = 'None'; $g.PixelOffsetMode = 'Half'; $g.InterpolationMode = 'NearestNeighbor'
  $f = $townDarken[$town]; $b = $townBlend[$town]; $t = C $townTint[$town]
  $s = $f * (1.0 - $b)
  $cm = New-Object System.Drawing.Imaging.ColorMatrix
  $cm.Matrix00 = $s; $cm.Matrix11 = $s; $cm.Matrix22 = $s
  $cm.Matrix40 = ($t.R / 255.0) * $b; $cm.Matrix41 = ($t.G / 255.0) * $b; $cm.Matrix42 = ($t.B / 255.0) * $b
  $ia = New-Object System.Drawing.Imaging.ImageAttributes
  $ia.SetColorMatrix($cm)
  $rect = New-Object System.Drawing.Rectangle(0, 0, $src.Width, $src.Height)
  $g.DrawImage($src, $rect, 0, 0, $src.Width, $src.Height, [System.Drawing.GraphicsUnit]::Pixel, $ia)
  $g.Dispose(); $ia.Dispose(); $src.Dispose()
  SaveImg $dst $dstRel
}

# Defining exterior tiles (ground/grass/path/tree/building); water/door/flowers
# keep the base and fall back in-engine. Plus all six service interiors.
$townTiles = 'ground', 'grass', 'path', 'tree', 'building'
$townBgs = 'inn', 'item_shop', 'equip_shop', 'training_hall', 'scoreboard', 'guild'
foreach ($town in 2, 3, 4, 5, 6, 7) {
  foreach ($k in $townTiles) {
    ShadeCopy "environments/town_$k.png" "environments/town${town}_$k.png" $town
  }
  foreach ($p in $townBgs) {
    ShadeCopy "backgrounds/$p.png" "backgrounds/${p}_t${town}.png" $town
  }
}

# ============================ M34 black-market NPC ============================
# A hooded dealer overworld sprite (12x12), distinct from the brown dungeon
# merchant: dark cloak, violet trim, a glint of gold. Uses no speckle RNG, so it
# does not affect any other file's bytes.
Write-Output 'Generating M34 black-market NPC...'
$b = New-Img 12 12
FR $b 4 1 4 3 $PAL.bossBody; FR $b 4 3 4 1 $PAL.night1            # hood + face shadow
FR $b 3 4 6 5 $PAL.night3                                          # cloak
FR $b 3 4 6 1 $PAL.violet                                          # violet trim
P $b 5 5 $PAL.gold; P $b 6 6 $PAL.glint                            # coin glint
FR $b 4 9 2 2 $PAL.night1; FR $b 6 9 2 2 $PAL.night1              # boots
Outline $b; SaveImg $b 'actors/market_npc.png'

# --- M41 story NPCs (12x12 overworld actors; no speckle RNG, so they shift no
#     other file's bytes) ---
Write-Output 'Generating M41 story NPCs...'
$b = New-Img 12 12                                                # storyteller: wide-hat robed bard
FR $b 3 1 6 1 $PAL.earth4                                          # hat brim
FR $b 4 1 4 3 $PAL.earth3; FR $b 4 3 4 1 $PAL.night1             # hat crown + face shadow
FR $b 3 4 6 5 $PAL.earth2; FR $b 3 4 6 1 $PAL.gold               # robe + gold trim
FR $b 8 5 2 4 $PAL.earth4; P $b 9 6 $PAL.glint                   # a lute/staff at the side
FR $b 4 9 2 2 $PAL.night1; FR $b 6 9 2 2 $PAL.night1            # boots
Outline $b; SaveImg $b 'actors/bard_npc.png'

$b = New-Img 12 12                                                # jester: belled cap + two-tone motley
P $b 4 0 $PAL.gold; P $b 7 0 $PAL.gold                            # cap bells
P $b 4 1 $PAL.danger; P $b 7 1 $PAL.cyan                         # cap points
FR $b 4 2 4 2 $PAL.clsCleric                                       # face
FR $b 3 4 3 5 $PAL.danger; FR $b 6 4 3 5 $PAL.cyan              # motley: red left, cyan right
FR $b 4 9 2 2 $PAL.night1; FR $b 6 9 2 2 $PAL.night1            # boots
Outline $b; SaveImg $b 'actors/jester_npc.png'

# --- M69: town exteriors — five service facades (48x32, opaque, covering
# --- their 3x2 Building tiles exactly), the scoreboard stele, and the save
# --- crystal. Appended after every earlier section and reseeded, so all
# --- prior files stay byte-identical.
$script:rng = 69690000
Write-Output 'Generating town exteriors...'

# Shared south-facing facade: pitched shingle roof, plastered wall with timber
# posts on the tile seams, two warm-lit windows, and the door INTEGRATED into
# the middle tile — directly above the doorstep trigger tile below it.
function New-Facade([string]$roofHex, [string]$roofDarkHex, [string]$wallHex, [string]$trimHex) {
  $b = New-Img 48 32
  FR $b 0 11 48 21 $wallHex                                       # wall
  FR $b 0 30 48 2 $PAL.night2                                     # foundation
  FR $b 0 11 48 1 $PAL.night2                                     # eaves shadow
  FR $b 0 12 1 19 $trimHex; FR $b 47 12 1 19 $trimHex             # corner posts
  FR $b 16 12 1 19 $trimHex; FR $b 31 12 1 19 $trimHex            # seam posts
  FR $b 0 0 48 11 $roofHex                                        # roof
  FR $b 0 0 48 1 $roofDarkHex                                     # ridge cap
  FR $b 0 4 48 1 $roofDarkHex; FR $b 0 7 48 1 $roofDarkHex        # shingle rows
  FR $b 0 10 48 1 $roofDarkHex                                    # eaves edge
  foreach ($wx in @(5, 38)) {                                     # warm-lit windows
    FR $b $wx 15 8 8 $PAL.night1
    FR $b ($wx+1) 16 6 6 '#E8C56A'
    FR $b ($wx+4) 16 1 6 $PAL.night1; FR $b ($wx+1) 19 6 1 $PAL.night1
    FR $b $wx 23 8 1 $PAL.night2
  }
  FR $b 19 17 10 15 $PAL.night1                                   # door recess
  FR $b 20 18 8 14 $PAL.earth3                                    # door
  FR $b 20 18 8 1 $PAL.earth4                                     # lintel
  FR $b 22 19 1 13 $PAL.earth2; FR $b 25 19 1 13 $PAL.earth2      # planks
  P $b 26 25 $PAL.gold                                            # handle
  return $b
}
# Colored pennant hung on the right seam post, carrying the service emblem.
function Pennant($b, [string]$hex) {
  FR $b 30 11 7 1 $PAL.earth1                                     # rod
  FR $b 31 12 5 8 $hex
  P $b 31 20 $hex; P $b 35 20 $hex                                # swallowtail
}

$b = New-Facade $PAL.maroon $PAL.maroonD $PAL.clsCleric $PAL.earth2   # Inn: cream walls
Pennant $b $PAL.gold
P $b 33 14 $PAL.maroonD; P $b 32 15 $PAL.maroonD; P $b 33 16 $PAL.maroonD  # crescent
SaveImg $b 'environments/town_facade_inn.png'

$b = New-Facade $PAL.veg2 $PAL.veg1 $PAL.earth4 $PAL.earth1           # Item Shop
Pennant $b $PAL.danger
P $b 33 14 $PAL.clsCleric; FR $b 32 15 3 2 $PAL.clsCleric             # flask
SaveImg $b 'environments/town_facade_item_shop.png'

$b = New-Facade $PAL.stone3 $PAL.stone1 $PAL.stone4 $PAL.night3       # Equip Shop
Pennant $b $PAL.cyan
FR $b 33 13 1 4 $PAL.night1; FR $b 32 14 3 1 $PAL.night1              # sword
SaveImg $b 'environments/town_facade_equip_shop.png'

$b = New-Facade $PAL.bossBody $PAL.bossD $PAL.stone3 $PAL.gold        # Guild
Pennant $b $PAL.violet
P $b 33 14 $PAL.gold; P $b 32 15 $PAL.gold; P $b 34 15 $PAL.gold; P $b 33 16 $PAL.gold  # star
SaveImg $b 'environments/town_facade_guild.png'

$b = New-Facade $PAL.earth2 $PAL.earth1 $PAL.stone3 $PAL.clsGuardian  # Training Hall
Pennant $b $PAL.clsGuardian
FR $b 32 14 1 2 $PAL.night1; FR $b 34 14 1 2 $PAL.night1; FR $b 32 15 3 1 $PAL.night1  # dumbbell
SaveImg $b 'environments/town_facade_training_hall.png'

# Scoreboard stele (32x32, transparent, outlined): a great stone sheet on a
# plinth, its face engraved with score rows and crowned in gold.
$b = New-Img 32 32
FR $b 4 27 24 4 $PAL.stone1; FR $b 6 25 20 2 $PAL.stone2          # plinth
FR $b 8 3 16 23 $PAL.stone3                                       # slab
FR $b 9 2 14 1 $PAL.stone3                                        # arched top
FR $b 10 5 12 18 $PAL.stone4                                      # sheet face
foreach ($ly in @(8, 11, 14, 17, 20)) { FR $b 11 $ly 10 1 $PAL.night2 }  # engraved rows
FR $b 11 8 4 1 $PAL.gold                                          # the top entry shines
P $b 15 3 $PAL.gold; P $b 16 3 $PAL.gold; P $b 14 4 $PAL.gold; P $b 17 4 $PAL.gold  # crown
Outline $b; SaveImg $b 'props/scoreboard_stele.png'

# Save crystal (16x16, transparent, outlined): a cyan crystal on dark rock.
$b = New-Img 16 16
FR $b 4 13 8 3 $PAL.stone1; FR $b 5 13 6 1 $PAL.stone2            # rock base
FR $b 7 1 2 2 $PAL.cyan                                           # tip
FR $b 6 3 4 3 $PAL.cyan
FR $b 5 6 6 5 $PAL.cyan
FR $b 6 11 4 2 $PAL.cyan
FR $b 5 6 2 5 $PAL.glint; P $b 6 4 $PAL.glint; P $b 7 2 '#FFFFFF' # lit facet
FR $b 9 6 2 5 $PAL.wat2; P $b 9 11 $PAL.wat2                      # shaded facet
Outline $b; SaveImg $b 'props/save_crystal.png'

Write-Output 'Texture generation complete.'
