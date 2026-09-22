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
  # M128 foliage ramps (art bible section 2 - completed, not invented, like the
  # M73 ramps above): the town ladder's gold-leaf, ember and crimson-leaf
  # canopies, plus a named key for the already-shipped bark shadow (earth1 -1).
  goldleaf0='#6E5A1E'; goldleaf1='#A88A2A'; goldleaf2='#D4B43C'
  ember0='#6E3A1A';    ember1='#B0602A';    ember2='#DC8A3A'
  crimson0='#5A1E22';  crimson1='#8E2E32';  crimson2='#C04A44'
  barkShadow='#3A2E20'
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
# M128 ramps (usable by every grid section, sprites included): j/k/l gold-leaf,
# 4/5/6 ember, 7/8/9 crimson-leaf, 0 the bark shadow (earth1 -1).
GridKey 'j' $PAL.goldleaf0; GridKey 'k' $PAL.goldleaf1; GridKey 'l' $PAL.goldleaf2
GridKey '4' $PAL.ember0;    GridKey '5' $PAL.ember1;    GridKey '6' $PAL.ember2
GridKey '7' $PAL.crimson0;  GridKey '8' $PAL.crimson1;  GridKey '9' $PAL.crimson2
GridKey '0' $PAL.barkShadow

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
  # M129: the normal/elite canvas is bound the same way (24x24 - the formation
  # envelopes and the party's own sprites assume it).
  if (-not $name.StartsWith('boss_') -and ($b.Width -ne 24 -or $b.Height -ne 24)) {
    throw "Save-EnemyGrid: enemy '$name' is $($b.Width)x$($b.Height); must be 24x24."
  }
  Outline $b
  SaveImg $b "enemies/$name.png"
}

# --- Family: goblinoid & raider (bruisers with comically outsized arms) ---

Save-EnemyGrid 'goblin_grunt' @(    # crouched green grunt, studded club over the back shoulder
  '.....ffff...............'
  '....ffddddd.............'
  '...fdddddddd............'
  '...fddwdddwds...........'
  '...dddddddddss..........'
  '....sddddddss...........'
  '.....ssssss.............'
  '.......ss.....xxxx......'
  '.......ss...xxccccx.....'
  '.......ss..xcvvccccx....'
  '.....xxss..xcvcKcccccx..'
  '....xccs...xccccccccccx.'
  '....xcc....xcccKKKKccx..'
  '.....xcc...xxccWWccx....'
  '......xccccccccccccx....'
  '......xccvccccccccccx...'
  '......xcccccccccccccx...'
  '.......xcssssssssccx....'
  '........sssassssa.......'
  '.......xcc.....ccx......'
  '.......xcc.....ccx......'
  '.......xcc.....ccx......'
  '......xxxx....xxxx......'
  '........................'
)

Save-EnemyGrid 'kobold_scout' @(    # all ears: a scaled scout, dagger thrust ahead
  '........................'
  '....f.........f.........'
  '....fd.......fd.........'
  '....fdd.....fdd.........'
  '....fddd...fddd.........'
  '.....fddd.fddd..........'
  '.....fdddddddd..........'
  '....fddddddddddd........'
  '....fdddKdddddddd.......'
  '....fdddddddddddddd.....'
  '.....fdddddddKKddd......'
  '......fdddddddddd.......'
  '.......ddddddd.......W..'
  '......fddddddddd....LS..'
  '.....fdddddddddddd.LS...'
  '.....fdddsssddddddLS....'
  '.....fdddsssdddddda.....'
  '......dddsssdddd........'
  '......fdddddddd.........'
  '.......ddd..ddd.........'
  '......fdd....ddd........'
  '......sdd....dds........'
  '.....sssss..sssss.......'
  '........................'
)

Save-EnemyGrid 'bandit' @(          # slouch hat, maroon mask, sabre held out
  '........................'
  '.......aaaaaa...........'
  '......aassssaa..........'
  '.....aaaaaaaaaaa........'
  '....aaaaaaaaaaaaa.......'
  '.......hhhhhh...........'
  '.......hhKhhhh..........'
  '.......gggggggg.........'
  '........ggggggg.........'
  '.........sss............'
  '......sssssssss......LS.'
  '.....sssdssssssss...LS..'
  '.....ssssssssssssss.LS..'
  '....ssdssssssssssssLS...'
  '....ss.sssssssssssdd....'
  '....ss..ssssssss........'
  '........aaaaaaa.........'
  '........sssssss.........'
  '.......sss..sss.........'
  '.......sss..sss.........'
  '......0ss....ss0........'
  '......aaa....aaa........'
  '.....aaaa....aaaa.......'
  '........................'
)

Save-EnemyGrid 'dune_reaver' @(     # sand-cloth raider, scimitar held flat overhead
  '.....LLLSSSSSSSSSS......'
  '....LSSSSSSSSSSSSSS.....'
  '....L..........SSSS.....'
  '.....aa.................'
  '.....fda......ffff......'
  '.....fd......fddddf.....'
  '.....fd.....fdddKddf....'
  '......fd....fddddddd....'
  '......fd...sssssssss....'
  '.......fddsddddddd......'
  '.......fdddddddddd......'
  '.......fddddddddddd.....'
  '......fdddsdddddddd.....'
  '......fddddddddddddd....'
  '......fdddddddsdddd.....'
  '.......fdddddddddd......'
  '.......sssssssssss......'
  '.......ddddd.dddd.......'
  '......fddd...fddd.......'
  '......ddd.....ddd.......'
  '.....sdd.......dds......'
  '.....sdd.......dds......'
  '....aaaa.......aaaa.....'
  '........................'
)

Save-EnemyGrid 'ogre_marauder' @(   # elite: slab brute, studded log club grounded right
  '........................'
  '..........hhhhh.........'
  '.........hhhhhhh........'
  '.........hhKhhhKh.......'
  '.........hhhhhhhhh......'
  '..........hhWhhhWh......'
  '.......ggggggggggg......'
  '.....ggghhhhhhhhhggg....'
  '....ghhhhhhhhhhhhhhhg...'
  '....ghhhhhhhhhhhhhhhg...'
  '....ghhhggghhhhhhhhgg...'
  '....ghhhggghhhhhhhhg.aa.'
  '.....ghhhhhhhhhhhhhg.da.'
  '.....gghhhhhhhhhhgg.dda.'
  '......gghhhhhhhhhg..dwa.'
  '.......ssssssssss..fdda.'
  '.......ssssssssss..fdwa.'
  '.......ssssssssss..fdda.'
  '.......hhhh..hhhh...ss..'
  '......hhhh....hhhh......'
  '......hhhh....hhhh......'
  '.....ggggg....ggggg.....'
  '....aaaaaa....aaaaaa....'
  '........................'
)

Save-EnemyGrid 'troll_berserker' @( # elite: lanky, arms to the ground, one tusk
  '........................'
  '.........sssss..........'
  '........seeeees.........'
  '........seKeeees........'
  '........eeeeeeeeW.......'
  '.........eeKKKee........'
  '..........eeeee.........'
  '.......eeeeeeeeeee......'
  '.....eeeeeeeeeeeeeee....'
  '....eee.eeeeeeeee.eee...'
  '...eee...eeeeeee...eee..'
  '...ee....eeeeeee....ee..'
  '..ee.....eeeeeee.....ee.'
  '..ee......eeeee......ee.'
  '..ee......sssss......ee.'
  '..ee......sssss......ee.'
  '.eee......sssss.....eee.'
  '.eee.....ee...ee....eee.'
  '.eee.....ee...ee....eee.'
  '.wwww....ee...ee...wwww.'
  '.wwww...www...www..wwww.'
  '........www...www.......'
  '.......qqqq...qqqq......'
  '........................'
)

Save-EnemyGrid 'ironclad_reaver' @( # elite: riveted plate, axe grounded right
  '........................'
  '.........wwwww..........'
  '........wweeeew.........'
  '........weKKKew.........'
  '........wweeeew.........'
  '.........wwwww..........'
  '......wwwwwwwwwww.......'
  '.....wwerwwwwwrwww......'
  '....wwwwwwwwwwwwwww.....'
  '....wwqwwwwwwwwwqww.....'
  '....wwwwwwwwwwwwwww.aa..'
  '....wwwwwwwwwwwwwww.ss..'
  '....qwwwwwwwwwwwwwqeess.'
  '.....qwwwwwwwwwwwqreess.'
  '.....qwqqqqqqqqwq.reess.'
  '......wwwwwwwww...reess.'
  '......wwwwwwwww...reess.'
  '......wwwwwwwww....eess.'
  '......wwww.wwww.....ss..'
  '.....wwww...wwww....ss..'
  '.....wwww...wwww....ss..'
  '....qqqqq...qqqqq...ss..'
  '...qqqqqq...qqqqqq..aa..'
  '........................'
)

Save-EnemyGrid 'dread_knight' @(    # elite: black plate, violet visor, greatsword planted
  '........................'
  '........mmmmmm..........'
  '.......mBBmmmmm.........'
  '.......mmVVVmmm.........'
  '.......mmmmmmmm.........'
  '........mmmmmm..........'
  '.....mmmmmmmmmmmm.......'
  '....mBBmmmmmmmmmmm......'
  '...mmmmmmmmmmmmmmmm.....'
  '...mmnmmmmmmmmmmnmm.....'
  '...mmmmmmmmmmmmmmmm.r...'
  '...mmmmmmmmmmmmmmmm.q...'
  '....mmmmmmmmmmmmmmqqqq..'
  '....nmmmmmmmmmmmmn.LS...'
  '.....nmmmmmmmmmmn..LS...'
  '.....nmnnnnnnnnmn..LS...'
  '......mmmmmmmmm....LS...'
  '......mmmmmmmmm....LS...'
  '......mmmm.mmmm....LS...'
  '.....mmmm...mmmm...LS...'
  '.....mmmm...mmmm...LS...'
  '....nnnnn...nnnnn...S...'
  '...nnnnnn...nnnnnn..S...'
  '........................'
)

# --- Family: undead (bone showing through, negative space in the ribs) ---

Save-EnemyGrid 'skeleton_archer' @( # skull, rib holes, a bow taller than the archer
  '........................'
  '.......ZZZZ.......d.....'
  '......ZSSSZZ.....Sd.....'
  '......ZKZZKZ.....S.d....'
  '......ZZZZZZ.....S.d....'
  '.......ZKKZ......S..d...'
  '........ZZ.......S..d...'
  '......ZZZZZZZZ...S..d...'
  '.....ZZ.Z.Z.ZZZ..S..d...'
  '.....Z..Z.Z..ZZZZS..d...'
  '.....Z.ZZ.ZZ..ZZ.S..d...'
  '.....ZZ.Z.Z.ZZ...S..d...'
  '......ZZZZZZZ....S..d...'
  '.......ZZZZZ.....S.d....'
  '.......ZZ.ZZ.....S.d....'
  '......ZZ...ZZ....Sd.....'
  '......ZZ...ZZ.....d.....'
  '.....ZZ.....ZZ..........'
  '.....ZZ.....ZZ..........'
  '....ZZ.......ZZ.........'
  '....ZZ.......ZZ.........'
  '...ZZZ.......ZZZ........'
  '...ZZZZ.....ZZZZ........'
  '........................'
)

Save-EnemyGrid 'zombie' @(          # shambler: one arm out front, torn-open gut, dragging leg
  '........................'
  '........wwwwww..........'
  '.......wwwwwwww.........'
  '.......wKwwwwKw.........'
  '.......wwwwwwww.........'
  '........wwKKww..........'
  '.........wwww...........'
  '.......wwwwwwwww........'
  '......wwwwwwwwwwwwwwww..'
  '......wwwwwwwwwwwwwwwww.'
  '......wwwwwwwwwww.......'
  '......wwwwxxwwwww.......'
  '......wwwKKKKwwww.......'
  '......wwwtKKtwwww.......'
  '.......wwwttwwww........'
  '.......aaaaaaaaa........'
  '.......aaaaaaaaa........'
  '.......aaaa.aaaa........'
  '......aaaa...aaaa.......'
  '......aaaa....aaaa......'
  '......www......www......'
  '.....wwww......wwww.....'
  '....wwwww.......wwww....'
  '........................'
)

Save-EnemyGrid 'grave_wight' @(     # gaunt hooded shroud, long bone claws, torn hem
  '........................'
  '.........mmmmm..........'
  '........mmmmmmm.........'
  '........mKmmmKm.........'
  '........mmmmmmm.........'
  '.........mmmmm..........'
  '.......mmmmmmmmm........'
  '......mmmmmmmmmmm.......'
  '.....mmmmmmmmmmmmm......'
  '.....mmmmmmmmmmmmmm.....'
  '....mmm.mmmmmmm.mmmmS...'
  '....mm..mmmmmmm..mmmSS..'
  '...mmm..mmmmmmm...mmSSS.'
  '..SSm...mmmmmmm....SSSS.'
  '.SSSm...nmmmmmn.........'
  '.SSS....nmmmmmn.........'
  '........nmmmmmn.........'
  '........nm.mm.mn........'
  '........n..mm..n........'
  '.......nn..mm..nn.......'
  '.......n...mm...n.......'
  '...........mm...........'
  '..........mmmm..........'
  '........................'
)

Save-EnemyGrid 'corpse_hound' @(    # ribs that are HOLES, head low, jaw open
  '........................'
  '........................'
  '........................'
  '........................'
  '.gg.....................'
  '..gg....................'
  '...gg...................'
  '....gggggggggggg........'
  '....ggggggggggggggg.....'
  '....ggg.g.g.g.gggggg....'
  '....gg..g.g.g..ggggKgg..'
  '....ggg.g.g.g.ggggggggg.'
  '....gggggggggggggggWWWW.'
  '.....ggggggggggggKKKKK..'
  '.....gg.......ggggggggg.'
  '.....gg........gg..WWW..'
  '....ggg........gg.......'
  '....gg.........gg.......'
  '....gg.........gg.......'
  '...ggg........ggg.......'
  '...gg.........gg........'
  '..ggg........ggg........'
  '..gggg.......gggg.......'
  '........................'
)

Save-EnemyGrid 'grave_chanter' @(   # elite: hooded skull, an enormous open tome
  '........................'
  '.........nnnnn..........'
  '........nnnnnnn.........'
  '........nnZZZnn.........'
  '........nZKZKZn.........'
  '........nZZZZZn.........'
  '.........ZKKZ...........'
  '.......nnnnnnnnn........'
  '......nnnnnnnnnnn.......'
  '.....nnnnnnnnnnnnn......'
  '.....nnnnnnnnnnnnn......'
  '.....nnnnnnnnnnnnn......'
  '.dddddddddddddddddddd...'
  '.dSSSSSSSSSdSSSSSSSSd...'
  '.dSSSKSKSSSdSSKSKSSSd...'
  '.dSSSSSSSSSdSSSSSSSSd...'
  '.dSSKSSKSSSdSSSKSSKSd...'
  '.dSSSSSSSSSdSSSSSSSSd...'
  '.dddddddddddddddddddd...'
  '......nnnnnnnnnnn.......'
  '......nnnnnnnnnnn.......'
  '.....nnnnnnnnnnnnn......'
  '.....nnnnnnnnnnnnn......'
  '........................'
)

Save-EnemyGrid 'bone_colossus' @(   # elite: a wall of ribs, arms like pillars
  '........................'
  '.........ZZZZZ..........'
  '.........ZKZKZ..........'
  '.........ZZZZZ..........'
  '..........ZKZ...........'
  '....ZZZZZZZZZZZZZZZZ....'
  '...ZZZZZZZZZZZZZZZZZZ...'
  '..ZZZ.ZZ.ZZ.ZZ.ZZ.ZZZ...'
  '..ZZZ.ZZ.ZZ.ZZ.ZZ.ZZZ...'
  '..ZZZ.ZZ.ZZ.ZZ.ZZ.ZZZ...'
  '..ZZZ.ZZ.ZZ.ZZ.ZZ.ZZZ...'
  '..ZZZ.ZZ.ZZ.ZZ.ZZ.ZZZ...'
  '..ZZZ.ZZ.ZZ.ZZ.ZZ.ZZZ...'
  '..ZZZ.ZZ.ZZ.ZZ.ZZ.ZZZ...'
  '..ZZZ..ZZZZZZZZZZ.ZZZ...'
  '..ZZZ...ZZZZZZZZ..ZZZ...'
  '..ZZZ....ZZZZZZ...ZZZ...'
  '..ZZZ....ZZ..ZZ...ZZZ...'
  '..ZZZ...ZZZ..ZZZ..ZZZ...'
  '..ZZZ...ZZ....ZZ..ZZZ...'
  '.ZZZZ...ZZ....ZZ..ZZZZ..'
  '.ZZZZ..ZZZ....ZZZ.ZZZZ..'
  '.ZZZZ.ZZZZ....ZZZZZZZZ..'
  '........................'
)

Save-EnemyGrid 'soul_render' @(     # elite: legless wraith, a huge scythe, one cold eye
  '.......a................'
  '.......a.SSSSSSSSSS.....'
  '.......a.SLLLLLLLLLLS...'
  '.......a.......LLLLLLS..'
  '.......a.........LLLLL..'
  '.......a..nnnnnn........'
  '.......a.nnnnnnnn.......'
  '.......a.nnCnnnnn.......'
  '.......a.nnnnnnnn.......'
  '.......a..nnnnnn........'
  '......nnnnnnnnnnnnn.....'
  '.....nnnnnnnnnnnnnnn....'
  '.....nnnnnnnnnnnnnnn....'
  '.....nnnnnnnnnnnnnnn....'
  '......nnnnnnnnnnnnn.....'
  '......nnnnnnnnnnnnn.....'
  '.......nnnnnnnnnnn......'
  '.......nn.nnnnn.nn......'
  '........n..nnn..n.......'
  '........n..nnn..n.......'
  '...........nnn..........'
  '...........nn...........'
  '............n...........'
  '........................'
)

Save-EnemyGrid 'plague_bearer' @(   # elite: bloated gut, tiny head, weeping buboes
  '........................'
  '........................'
  '.........hhhh...........'
  '.........hKhKh..........'
  '.........hhhhh..........'
  '..........hhh...........'
  '.......ggggggggg........'
  '.....ggghhhhhhhggg......'
  '....ghhhhhhhhhhhhhg.....'
  '...ghhhhhhhhvhhhhhhg....'
  '...ghhhhhhhvvhhhhhhg....'
  '..ghhhvhhhhhvhhhhhhhg...'
  '..ghhvvhhhhhhhhhhvhhg...'
  '..ghhhvhhhhhhhhhvvhhg...'
  '..ghhhhhhhhhhhhhhvhhg...'
  '...ghhhhhhhvhhhhhhhg....'
  '...ghhhhhhvvhhhhhhhg....'
  '....gghhhhhvhhhhhgg.....'
  '.....ggghhhhhhhggg......'
  '.......gg.....gg........'
  '......ggg.....ggg.......'
  '......ggg.....ggg.......'
  '.....gggg.....gggg......'
  '........................'
)

# --- Family: beast (no weapons; the body plan itself is the silhouette) ---

Save-EnemyGrid 'cave_bat' @(        # wingspan three times the body, fingered membranes, fangs
  '........................'
  '........................'
  '........................'
  '........................'
  '........................'
  '..m...........m.........'
  '..mm.........mm.........'
  '..mmm..mmm..mmm.........'
  '..mmmm.mmmm.mmmm........'
  '..mmmmmmmmmmmmmmm.......'
  '.mmmmmmmmnnmmmmmmm......'
  '.mmmm.mmmnKnmmm.mmmm....'
  '.mmm..mmmnnnmmm..mmmm...'
  '.mm...mmmWnWmmm...mmm...'
  '.m....mm.nnn.mm....mm...'
  '......m..nnn..m.....m...'
  '.........nnn............'
  '..........n.............'
  '........................'
  '........................'
  '........................'
  '........................'
  '........................'
  '........................'
)

Save-EnemyGrid 'venom_spider' @(    # eight angular legs, a fat abdomen, green venom markings
  '........................'
  '........................'
  '........................'
  '........................'
  '........................'
  '........................'
  '....x.......x...........'
  '...xx......xx.....x.....'
  '...x.xxxxxxxx....xx.....'
  '..xx.xxvvvxxxxx.xx......'
  '..x.xxvvvvvxxxxxx.......'
  '..x.xxxvvvxxxxxxxKx.....'
  '.xx.xxxxxxxxxxxxxxxKx...'
  '.x..xxxxxxxxxxxxxxxxx...'
  '.x...xxxxxxxxxxxx.WxW...'
  '....x.xxxxxxxxx.x.......'
  '...xx.x.......xx.xx.....'
  '..xx..x........x..xx....'
  '..x..xx........xx..x....'
  '.xx..x..........x..xx...'
  '.x..xx..........xx..x...'
  '....x............x......'
  '...xx............xx.....'
  '........................'
)

Save-EnemyGrid 'wild_boar' @(       # bristle hump forward, tusks up, a running wedge
  '........................'
  '........................'
  '........................'
  '........................'
  '........................'
  '..........sss...........'
  '.......sssssssss........'
  '.....ssssssssssss.......'
  '....sssssssssssssss.....'
  '...sssssssssssssssss....'
  '...ssssssssssssssssss...'
  '...sssssssssssssdKddd...'
  '...sssssssssssssdddddd..'
  '....ssssssssssssddddWd..'
  '....ssssssssssssdddWW...'
  '.....ssssssssssssddd....'
  '.....ssss...sssssss.....'
  '......sss....sss........'
  '......ss.....sss........'
  '.....aaa......aaa.......'
  '.....aa........aa.......'
  '....aaa........aaa......'
  '....aaaa.......aaaa.....'
  '........................'
)

Save-EnemyGrid 'forest_wolf' @(     # lean, long legs, ears and tail up, a running diagonal
  '........................'
  '........................'
  '........................'
  '........................'
  '..w.............w.w.....'
  '..ww...........wwww.....'
  '...ww.........wwwwww....'
  '....ww.......wwKwwwww...'
  '.....wwwwwwwwwwwwwwwww..'
  '.....wwwwwwwwwwwwwwWww..'
  '......wwwwwwwwwwwwwWW...'
  '......wwwwwwwwwwwwww....'
  '......wwwwwwwwwwww......'
  '......wwwwwwwwwwww......'
  '......www.....www.......'
  '.....www......www.......'
  '....www.......www.......'
  '....ww.........ww.......'
  '...ww..........ww.......'
  '...ww...........ww......'
  '..ww............ww......'
  '..ww.............ww.....'
  '.qqq.............qqq....'
  '........................'
)

Save-EnemyGrid 'sand_lurker' @(     # a coiled, banded serpent, head raised and open
  '........................'
  '........................'
  '........................'
  '........................'
  '........................'
  '..............dddd......'
  '.............ddddddd....'
  '.............ddKdddddd..'
  '.............dddddddWd..'
  '..............dddWWWd...'
  '..............ddddd.....'
  '..............ddd.......'
  '..............ddd.......'
  '..............ddd.......'
  '......dddddddddd........'
  '....dddffdddffdd........'
  '...ddffdddffddddd.......'
  '...dddddddddddddd.......'
  '....ddffdddffdddd.......'
  '.....dddddddddddd.......'
  '....ffdddffdddffd.......'
  '...dddddddddddddddd.....'
  '..dffdddffdddffdddfd....'
  '........................'
)

Save-EnemyGrid 'mud_crawler' @(     # low armoured woodlouse, stepped plate ridges, many legs
  '........................'
  '........................'
  '........................'
  '........................'
  '........................'
  '........................'
  '........................'
  '........................'
  '........................'
  '.........sss............'
  '......sssdddsss.........'
  '....ssdddsssdddss.......'
  '...sdddsssdddsssddss....'
  '..sdddsssdddsssdddsss...'
  '..sssdddsssdddsssdddsK..'
  '..ssssssssssssssssssss..'
  '..ssssssssssssssssssss..'
  '..aa.aa.aa.aa.aa.aa.aa..'
  '.aa.aa.aa.aa.aa.aa.aa...'
  '........................'
  '........................'
  '........................'
  '........................'
  '........................'
)

Save-EnemyGrid 'frost_imp' @(       # slender, tall ice horns, sharp wings, a cold grin
  '........................'
  '.........C....C.........'
  '........CC...CC.........'
  '........Cp...pC.........'
  '.........ppppp..........'
  '........pppppppp........'
  '........ppKpppKp........'
  '........pppppppp........'
  '.........ppWWWp.........'
  '..........pppp..........'
  '....i.....pppp....i.....'
  '...iii...pppppp..iii....'
  '..iiiii.pppppppp.iiiii..'
  '..iiiiiippppppppiiiiii..'
  '...iiii.ppppppp.iiii....'
  '....ii..pppppp...ii.....'
  '........pppppp..........'
  '........pppppp..........'
  '........ppp.pp..........'
  '.......ppp..ppp.........'
  '......ppp....ppp........'
  '......pp......pp........'
  '.....ooo......ooo.......'
  '........................'
)

Save-EnemyGrid 'mire_imp' @(        # squat toad, wide toothy grin, drooping wings
  '........................'
  '........................'
  '........................'
  '........................'
  '........................'
  '........................'
  '........xxxxxxxx........'
  '.......xxKxxxxKxx.......'
  '......xxxxxxxxxxxx......'
  '.....xxxxxxxxxxxxxx.....'
  '....xxxxxxxxxxxxxxxx....'
  '...xxxKWKWKWKWKWKxxxx...'
  '...xxxxKKKKKKKKKKxxxx...'
  '..xxxxxxxxxxxxxxxxxxxx..'
  '.xxxxxxxxvvvvvvxxxxxxxx.'
  '.xx.xxxxvvvvvvvvxxxx.xx.'
  '.x..xxxxvvvvvvvvxxxx..x.'
  '.x...xxxvvvvvvvvxxx...x.'
  '.....xxxxvvvvvvxxxx.....'
  '......xxxxxxxxxxxx......'
  '......xxx......xxx......'
  '.....xxxx......xxxx.....'
  '....xxxxx......xxxxx....'
  '........................'
)

# --- Family: caster (robes are interchangeable, so the HEADGEAR carries the
# --- silhouette: cone hood / antlers / mitre / plague beak / orb / shard) ---

Save-EnemyGrid 'dark_acolyte' @(    # tall cone hood, one eye in the dark, orb floating at the hand
  '..........n.............'
  '.........nnn............'
  '.........nnn............'
  '........nnnnn...........'
  '........nnnnn...........'
  '.......nnnnnnn..........'
  '.......nnnnnnn..........'
  '......nnnnnnnnn.........'
  '......nnn1K1nnn.........'
  '......nnn111nnn.........'
  '.......nnnnnnn..........'
  '.....nnnnnnnnnnn........'
  '....nnnnnnnnnnnnn.......'
  '....nnnnnnnnnnnnnn..GV..'
  '....nnnnnnnnnnnnnnnVVVV.'
  '....nnnnnnnnnnnnnn.VVVV.'
  '....nnmnnnnnnnnnn...VV..'
  '....nnmnnnnnnnnnn.......'
  '.....nnnnnnnnnnn........'
  '.....nnnnnnnnnnn........'
  '.....nnnnnnnnnnn........'
  '....nnnnnnnnnnnnn.......'
  '....nnnnnnnnnnnnn.......'
  '........................'
)

Save-EnemyGrid 'bog_shaman' @(      # antler crown wider than the shaman, moss robe, gourd rattle
  '..d...........d.........'
  '..dd...d.d...dd.........'
  '...dd..d.d..dd..........'
  '....dd.ddd.dd...........'
  '.....ddddddd............'
  '......ddddd.............'
  '.......xxxx.............'
  '......xxKxKx............'
  '......xxxxxx............'
  '.......xxxx.............'
  '.....xxxxxxxxx..........'
  '....xxxcxxxxxxx.........'
  '...xxxxcxxxxxxxx....dd..'
  '...xxxxcxxxxxxxxx..dsdd.'
  '...xxxxxxxxxxxxxxxxdssd.'
  '...xxxxxxxxxxxxxx..dddd.'
  '....xxxxxxxxxxxx....dd..'
  '....xxxxxxxxxxxx........'
  '....xxxxxxxxxxxx........'
  '....xxxxxxxxxxxx........'
  '...xxxxxxxxxxxxxx.......'
  '...xxxxxxxxxxxxxx.......'
  '...xxxxxxxxxxxxxx.......'
  '........................'
)

Save-EnemyGrid 'gloom_priest' @(    # wide flat mitre, hollow hood, censer on a long chain
  '....eeeeeeeeeeeee.......'
  '....errrrrrrrrrre.......'
  '....eeeeeeeeeeeee.......'
  '.......nnnnnnn..........'
  '.......n11111n..........'
  '.......n1K1K1n..........'
  '.......n11111n..........'
  '........nnnnn...........'
  '......nnnnnnnnn.........'
  '.....nnnnnnnnnnn........'
  '....nnnnnnnnnnnnn.......'
  '....nnnnnnnnnnnnnr......'
  '....nnnnnnnnnnnnn.r.....'
  '....nnnnnnnnnnnnn.r.....'
  '....nnnnnnnnnnnnn.r.....'
  '....nnnnnnnnnnnnn.r.....'
  '....nnnnnnnnnnnnn.r.....'
  '....nnnnnnnnnnnnnddd....'
  '....nnnnnnnnnnnnndCd....'
  '....nnnnnnnnnnnnn.dd....'
  '...nnnnnnnnnnnnnn..C....'
  '...nnnnnnnnnnnnnn.......'
  '...nnnnnnnnnnnnnn.......'
  '........................'
)

Save-EnemyGrid 'blight_chanter' @(  # elite: plague beak, wide brim, censer vial of green
  '........................'
  '.......aaaaaaa..........'
  '......aaaaaaaaa.........'
  '.....aaaaaaaaaaa........'
  '..aaaaaaaaaaaaaaaaa.....'
  '.......qqqqqqq..........'
  '.......qKqqqqqZZZ.......'
  '.......qqqqqqqZZZZZ.....'
  '........qqqqqqZZZZZZZ...'
  '.........qqqqq..........'
  '......qqqqqqqqqqq.......'
  '.....qqqqqqqqqqqqq......'
  '....qqqqqqqqqqqqqqq.....'
  '....qqqqqqqqqqqqqqqq....'
  '....qqqqqqqqqqqqqqqqq...'
  '....qqqqqqqqqqqqqqq.q...'
  '....qqqqqqqqqqqqqqq.q...'
  '....qqqqqqqqqqqqqqqvvv..'
  '....qqqqqqqqqqqqqqqvHv..'
  '.....qqqqqqqqqqqqq.vvv..'
  '.....qqqqqqqqqqqqq......'
  '....qqqqqqqqqqqqqqq.....'
  '....qqqqqqqqqqqqqqq.....'
  '........................'
)

Save-EnemyGrid 'wisp' @(            # bodiless: a hard-edged cyan core, spikes of corona, tendrils
  '........................'
  '........................'
  '........................'
  '...........p............'
  '...........p............'
  '......p....p....p.......'
  '.......p..ppp..p........'
  '........pppCppp.........'
  '.........pCGCp..........'
  '..pppp..pCCCCCp..pppp...'
  '.........pCCCCp.........'
  '........ppCKCpp.........'
  '.......p..ppp..p........'
  '......p....p....p.......'
  '..........p.p...........'
  '.........p...p..........'
  '.........p...p..........'
  '........p.....p.........'
  '........................'
  '........................'
  '........................'
  '........................'
  '........................'
  '........................'
)

Save-EnemyGrid 'hex_wisp' @(        # a violet orb trailing a curtain of tendrils, one eye
  '........................'
  '........................'
  '........................'
  '........VVVVV...........'
  '.......VVGVVVV..........'
  '......VVVVVVVVV.........'
  '......VVVVKVVVV.........'
  '......VVVVVVVVV.........'
  '.......VVVVVVV..........'
  '........VVVVV...........'
  '......m.m.m.m.m.........'
  '......m.m.m.m.m.........'
  '.....m..m.m.m..m........'
  '.....m..m...m..m........'
  '.....m.m..m..m.m........'
  '....m..m..m..m..m.......'
  '....m..m..m..m..m.......'
  '....m.m...m...m.m.......'
  '...m..m...m...m..m......'
  '...m..m..m.m..m..m......'
  '...m.m...m.m...m.m......'
  '..m..m...m.m...m..m.....'
  '........................'
  '........................'
)

Save-EnemyGrid 'shardling' @(       # a crystal splinter, no body at all: facets and a cold eye
  '...........C............'
  '..........CC............'
  '..........CCp...........'
  '.........CCCp...........'
  '.........CCCpp..........'
  '........CCCCpp..........'
  '........CCCppp..........'
  '.......CCCCppp..........'
  '.......CCCKppp..........'
  '......CCCCpppp..........'
  '......CCCCpppp..........'
  '.....CCCCppppp..........'
  '.....CCCCppppp..........'
  '......CCCpppp...........'
  '......CCCpppp...........'
  '.......CCppp............'
  '.......CCppp............'
  '........Cpp.............'
  '........Cpp.............'
  '.........p..............'
  '...pp.........pp........'
  '..ppp........ppp........'
  '..pp..........pp........'
  '........................'
)

Save-EnemyGrid 'void_weaver' @(     # elite: thread-thin arms, unravelling hem, a violet heart
  '........................'
  '.........nnnnn..........'
  '........nnnnnnn.........'
  '........nnKnKnn.........'
  '........nnnnnnn.........'
  '.........nnnnn..........'
  '..........nnn...........'
  '.......nnnnnnnnn........'
  '......nnnnnnnnnnn.......'
  '.n....nnnnVnnnnnn....n..'
  '.n....nnnnnnnnnnn....n..'
  '..n...nnnnnnnnnnn...n...'
  '..n...nnnnnnnnnnn...n...'
  '...n..nnnnnnnnnnn..n....'
  '...n..nnnnnnnnnnn..n....'
  '....nnnnnnnnnnnnnnn.....'
  '......nnnnnnnnnnn.......'
  '......nnnnnnnnnnn.......'
  '......nn.nnnnn.nn.......'
  '......n..n.n.n..n.......'
  '.....n...n.n.n...n......'
  '.....n...n...n...n......'
  '....n....n...n....n.....'
  '........................'
)

# --- Family: construct & armoured protector (hard 90-degree geometry; the
# --- shield or the growth on the back does the silhouette work) ---

Save-EnemyGrid 'stone_golem' @(     # nothing but slabs, one glowing crack, arms hang clear
  '........................'
  '........eeeeee..........'
  '........ewwwwe..........'
  '........ewKKwe..........'
  '........eewwee..........'
  '....eeeeeeeeeeeeee......'
  '...eeewwwwwwwwwweee.....'
  '..eeewwwwwwwwwwwweee....'
  '..wwe.wwwwwwwwwwwe.ww...'
  '..ww..wwwwCwwwwww..ww...'
  '..ww..wwwwwCwwwww..ww...'
  '..ww..wwwwwwCwwww..ww...'
  '..ww..wwwwwwwwwww..ww...'
  '..ww..wqwwwwwwwqw..ww...'
  '..ww..qqqqqqqqqqq..ww...'
  '.www...wwwwwwwww...www..'
  '.www...wwwwwwwww...www..'
  '.......wwww.wwww........'
  '.......www...www........'
  '......wwww...wwww.......'
  '......wwww...wwww.......'
  '.....qqqqq...qqqqq......'
  '....qqqqqq...qqqqqq.....'
  '........................'
)

Save-EnemyGrid 'rune_sentry' @(     # a floating monolith with one great rune eye
  '........................'
  '.........wwww...........'
  '........wwwwww..........'
  '.......wwwwwwww.........'
  '.......wwwwwwww.........'
  '.......wwCCCCww.........'
  '.......wCCwwCCw.........'
  '.......wCCwKCCw.........'
  '.......wCCwwCCw.........'
  '.......wwCCCCww.........'
  '.......wwwwwwww.........'
  '.......wwwwwwww.........'
  '.......wwwwwwww.........'
  '.......wwwwwwww.........'
  '.......wwwwwwww.........'
  '.......wwwwwwww.........'
  '.......wwwwwwww.........'
  '........wwwwww..........'
  '.........qqqq...........'
  '........................'
  '..........C.............'
  '.........C.C............'
  '........................'
  '........................'
)

Save-EnemyGrid 'crystal_guardian' @(# elite: shards erupting from the back, a stone core
  '........................'
  '........C...............'
  '.......CC..C............'
  '......CCC.CC............'
  '.....pCCC.CC............'
  '.....ppCCCCC.wwww.......'
  '....pppCCCCCwwwwww......'
  '....ppppCCCwwwKwww......'
  '...pppppCCwwwwwwww......'
  '...ppppppwwwwwwwww......'
  '..ppppppwwwwwwwwwwww....'
  '..pppppwwwwwwwwwwwwww...'
  '..ppppwwwwwwwwwwwwwww...'
  '..pppwwwwwwwwwwwwwwww...'
  '...ppwwwwwwwwwwwwwww....'
  '....pwwwwwwwwwwwwww.....'
  '.....wwwwwwwwwwwww......'
  '.....wwwwwwwwwwwww......'
  '......wwwwww.wwwww......'
  '......wwwww...wwww......'
  '.....wwwww....wwwww.....'
  '.....qqqqq....qqqqq.....'
  '....qqqqqq....qqqqqq....'
  '........................'
)

Save-EnemyGrid 'iron_sentinel' @(   # elite: a tower shield front-on, only a helm above it
  '........................'
  '........LLLLLL..........'
  '........LeeeeL..........'
  '........LeKKeL..........'
  '........LLLLLL..........'
  '......LLLLLLLLLL........'
  '.....LrLLLLLLLLrL.......'
  '.....LLLLLLLLLLLL.......'
  '.....LLeeeeeeeeLL.......'
  '.....LLeLLLLLLeLL.......'
  '.....LLeLLLLLLeLL.......'
  '.....LrLLLLLLLLrL.......'
  '.....LLeLLLLLLeLL.......'
  '.....LLeLLLLLLeLL.......'
  '.....LLeeeeeeeeLL.......'
  '.....LLLLLLLLLLLL.......'
  '.....LrLLLLLLLLrL.......'
  '......LLLLLLLLLL........'
  '.......LLLLLLLL.........'
  '........LLLLLL..........'
  '.......ee....ee.........'
  '.......ee....ee.........'
  '......qqq....qqq........'
  '........................'
)

Save-EnemyGrid 'titan_guard' @(     # elite: squat colossus, a slab shield planted right
  '........................'
  '........................'
  '........................'
  '.........eeeee..........'
  '........eeeeeee.........'
  '........eeKKKee.........'
  '........eeeeeee.........'
  '....eeeeeeeeeeeee.rrrrr.'
  '...eeeeeeeeeeeeee.reeer.'
  '..eeeeeeeeeeeeeee.reeer.'
  '..eeeeeeeeeeeeeee.reKer.'
  '..eeeeeeeeeeeeeee.reeer.'
  '..eeeeeeeeeeeeeeeereeer.'
  '..eeeeeeeeeeeeeeeereeer.'
  '..eeeeeeeeeeeeeee.reeer.'
  '...eeeeeeeeeeeeee.reeer.'
  '...eqqqqqqqqqqqqe.reeer.'
  '...eeeeeeeeeeeeee.reeer.'
  '...eeeeee..eeeeee.reeer.'
  '...eeeeee..eeeeee.reeer.'
  '..eeeeeee..eeeeeee.rrr..'
  '..qqqqqqq..qqqqqqq......'
  '.qqqqqqqq..qqqqqqqq.....'
  '........................'
)

Save-EnemyGrid 'royal_guard_sword' @(# elite: pale plate, gold-leaf trim, blade held upright
  '........................'
  '.........LLLL......S....'
  '........LkkkkL.....S....'
  '........LLLLLL.....S....'
  '........LLKKLL.....S....'
  '........LLLLLL.....S....'
  '......LLLLLLLLLL...S....'
  '.....LkLLLLLLLLkL..S....'
  '....LLLLLLLLLLLLLL.S....'
  '....LLLLLLLLLLLLLL.S....'
  '....LLkkkkkkkkkkLL.S....'
  '....LLLLLLLLLLLLLLkkk...'
  '....LLLLLLLLLLLLLL.a....'
  '....LeLLLLLLLLLLeL.a....'
  '.....LLLLLLLLLLLL.......'
  '.....LLLLLLLLLLLL.......'
  '.....kkkkkkkkkkkk.......'
  '......LLLLLLLLLL........'
  '......LLLLLLLLLL........'
  '......LLLL..LLLL........'
  '.....LLLL....LLLL.......'
  '.....eLLL....LLLe.......'
  '....eeeee....eeeee......'
  '........................'
)

Save-EnemyGrid 'royal_guard_staff' @(# elite: the matched twin, a crowned stave with a gem
  '..................k.k...'
  '..................kkk...'
  '.........LLLL.....kVk...'
  '........LkkkkL....kkk...'
  '........LLLLLL.....a....'
  '........LLKKLL.....a....'
  '........LLLLLL.....a....'
  '......LLLLLLLLLL...a....'
  '.....LkLLLLLLLLkL..a....'
  '....LLLLLLLLLLLLLL.a....'
  '....LLLLLLLLLLLLLL.a....'
  '....LLkkkkkkkkkkLL.a....'
  '....LLLLLLLLLLLLLLLa....'
  '....LLLLLLLLLLLLLL.a....'
  '....LeLLLLLLLLLLeL.a....'
  '.....LLLLLLLLLLLL..a....'
  '.....LLLLLLLLLLLL..a....'
  '.....kkkkkkkkkkkk..a....'
  '......LLLLLLLLLL...a....'
  '......LLLLLLLLLL...a....'
  '......LLLL..LLLL...a....'
  '.....LLLL....LLLL..a....'
  '.....eLLL....LLLe..a....'
  '....eeeee....eeeee.a....'
)

Save-EnemyGrid 'archon_of_ruin' @(  # elite: a broken halo, feet that never land
  '.......kkk...kkk........'
  '......k.........k.......'
  '......k.........k.......'
  '.......k.......k........'
  '.........mmmmm..........'
  '........mmmmmmm.........'
  '........mmVmVmm.........'
  '........mmmmmmm.........'
  '.........mmmmm..........'
  '.......mmmmmmmmm........'
  '......mmmmmmmmmmm.......'
  '.....mmmmmmmmmmmmm......'
  '....mm.mmmBBBmmm.mm.....'
  '....mm.mmmBBBmmm.mm.....'
  '...mm..mmmmBmmmm..mm....'
  '...mm..mmmmmmmmm..mm....'
  '.......mmmmmmmmm........'
  '.......mmmmmmmmm........'
  '........mmmmmmm.........'
  '........mmm.mmm.........'
  '.........m...m..........'
  '........................'
  '..........V.V...........'
  '........................'
)

# --- Family: buffer & stalker (the buffers ARE their instrument) ---

Save-EnemyGrid 'war_drummer' @(     # a drum wider than the drummer, sticks raised
  '.......a..........a.....'
  '........a........a......'
  '.........a......a.......'
  '..........a....a........'
  '.........xxxxxxx........'
  '........xxKxxxKxx.......'
  '........xxxxxxxxx.......'
  '.........xxxxxxx........'
  '.......xxxxxxxxxxx......'
  '......xxxxxxxxxxxxx.....'
  '..dddddddddddddddddddd..'
  '..dSSSSSSSSSSSSSSSSSSd..'
  '..dSSSSSSSSSSSSSSSSSSd..'
  '..ssssssssssssssssssss..'
  '..dddddddddddddddddddd..'
  '..dfdddddfdddddfdddddd..'
  '..dddddddddddddddddddd..'
  '..ssssssssssssssssssss..'
  '..dddddddddddddddddddd..'
  '..dddddddddddddddddddd..'
  '...ssssssssssssssssss...'
  '......xxx......xxx......'
  '.....xxxx......xxxx.....'
  '........................'
)

Save-EnemyGrid 'standard_bearer' @( # a banner twice the soldier's height, planted right
  '..................a.....'
  '..................aggggg'
  '..................aggggg'
  '..................agglgg'
  '..................aglllg'
  '..................agglgg'
  '..................aggggg'
  '..................aggggg'
  '........wwwww.....agg.gg'
  '.......wwwwwww....ag..g.'
  '.......wwKKKww....a.....'
  '.......wwwwwww....a.....'
  '........wwwww.....a.....'
  '.....wwwwwwwwwww..a.....'
  '....wwwwwwwwwwwww.a.....'
  '....wwwwwwwwwwwwwwa.....'
  '....wwwwwwwwwwwww.a.....'
  '....wwwwwwwwwwwww.a.....'
  '.....wwwwwwwwwww..a.....'
  '.....wwww...wwww..a.....'
  '.....wwww...wwww..a.....'
  '....wwww.....wwww.a.....'
  '....qqqq.....qqqq.a.....'
  '........................'
)

Save-EnemyGrid 'war_caller' @(      # elite: a horn he can barely lift, cheeks puffed
  '........................'
  '........................'
  '..................ddd...'
  '.................ddfdd..'
  '................ddfffdd.'
  '...............ddfffddd.'
  '..............ddffddd...'
  '.............ddfddd.....'
  '............dddddd......'
  '...........ddddd........'
  '.......cccxdddd.........'
  '......ccKccxdd..........'
  '......ccccccxd..........'
  '......ccccccc...........'
  '.......ccccc............'
  '....ccccccccccc.........'
  '...ccccccccccccc........'
  '...ccccccccccccc........'
  '...ccccccccccccc........'
  '....ccccccccccc.........'
  '....cccc...cccc.........'
  '....cccc...cccc.........'
  '...xxxxx...xxxxx........'
  '........................'
)

Save-EnemyGrid 'shadow_stalker' @(  # elite: upright, twin daggers held wide, one violet eye
  '........................'
  '.........nnnnn..........'
  '........nnnnnnn.........'
  '........nnnVnnn.........'
  '........nnnnnnn.........'
  '.........nnnnn..........'
  '..........nnn...........'
  '.......nnnnnnnnn........'
  'S.....nnnnnnnnnnn.....S.'
  '.S...nnnnnnnnnnnnn...S..'
  '..S..nnnnnnnnnnnnn..S...'
  '...L.nnnnnnnnnnnnn.L....'
  '....nnnnnnnnnnnnnnn.....'
  '.....nnnnnnnnnnnnn......'
  '.....nnnnnnnnnnnnn......'
  '......nnnnnnnnnnn.......'
  '......nnnnnnnnnnn.......'
  '......nnnnnnnnnnn.......'
  '......nnnn...nnnn.......'
  '......nnn.....nnn.......'
  '......nnn.....nnn.......'
  '.....nnn.......nnn......'
  '.....nnn.......nnn......'
  '........................'
)

Save-EnemyGrid 'void_stalker' @(    # elite: four-legged prowler, a blade for a tail
  '........................'
  '........................'
  '........................'
  '........................'
  '........................'
  '..S.....................'
  '..SS....................'
  '...SS...................'
  '....SS..................'
  '.....SS.................'
  '......n.................'
  '......nnnnnnnnnnnnn.....'
  '.....nnnnnnnnnnnnnnnn...'
  '.....nnnnnnnnnnnnnnVnn..'
  '.....nnnnnnnnnnnnnnnnnn.'
  '.....nnnnnnnnnnnnnnWWW..'
  '......nnn.....nnnnnnn...'
  '......nnn......nnn......'
  '.....nnn.......nnn......'
  '.....nn.........nn......'
  '....nn...........nn.....'
  '....nn...........nn.....'
  '...nnn...........nnn....'
  '........................'
)

# The two generic tier fallbacks. `BattleState::drawUnit` only reaches these
# when a content id has no bespoke sprite, so they are deliberately anonymous:
# a shape that says "an enemy" and "a tougher enemy" and nothing more.
Save-EnemyGrid 'normal_battle' @(   # generic: a plain hunched beast on all fours
  '........................'
  '........................'
  '........................'
  '........................'
  '........................'
  '........................'
  '.........sssss..........'
  '.......sssssssss........'
  '......sssssssssss.......'
  '.....sssssssssssss......'
  '....ssssssssssssssss....'
  '....sssssssssssssssss...'
  '....ssssssssssssssKss...'
  '....ssssssssssssssssss..'
  '.....sssssssssssssWWs...'
  '.....sssssssssssssss....'
  '.....sss.....sssss......'
  '......ss......sss.......'
  '......ss......sss.......'
  '.....sss.......ss.......'
  '.....ss........ss.......'
  '....sss.......sss.......'
  '....ssss......ssss......'
  '........................'
)

# Same beast as `normal_battle`, but REARED UP: the tier difference is posture
# and height, not a recolour, so it survives the grayscale/colour-blind check.
Save-EnemyGrid 'elite_battle' @(    # generic: the same beast rearing, horned, a violet eye
  '........................'
  '........aa........aa....'
  '.........aa......aa.....'
  '..........sssssss.......'
  '.........sssssssss......'
  '.........ssssssVss......'
  '.........sssssssss......'
  '..........ssssWWs.......'
  '.......sssssssssss......'
  '......sssssssssssss.....'
  '.....ssssssssssssssss...'
  '.....sssssssssssss..s...'
  '.....ssssssssssssss.....'
  '.....sssssssssssss......'
  '......sssssssssss.......'
  '......sssssssssss.......'
  '......sssssssssss.......'
  '......ssss...ssss.......'
  '......sss.....sss.......'
  '......sss.....sss.......'
  '.....sss.......sss......'
  '.....ss.........ss......'
  '....ssss.......ssss.....'
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

# --- Bosses (36x36; rules in art_bible.md §5b — the boss-art repair). A boss
# --- is NOT a big normal enemy and there is NO universal crown: each is built
# --- from 2-3 readable masses with real negative space, one dominant thematic
# --- motif from its bosses.json entry, one deliberate asymmetry, and a face
# --- that is a slit/void/maw/mask — never a cute dot-eye pair. Crowns exist
# --- only where regalia IS the identity (the Hollow King; mineral/ice
# --- diadems for the Deep King / Frost Monarch), and each is unique. ---

Save-EnemyGrid 'boss_battle' @(     # generic boss fallback: a CHAINED VOID, maw and two eyes
  '....................................'
  '....................................'
  '............nnnnnnnnnnnn............'
  '..........nnnnnnnnnnnnnnnn..........'
  '........nnnnnnnnnnnnnnnnnnnn........'
  '.......nnnnnnnnnnnnnnnnnnnnnn.......'
  '......nnnnnnnnnnnnnnnnnnnnnnnn......'
  '.....nnnnnnnVVnnnnnnnnnVVnnnnnn.....'
  '.....nnnnnnnVVnnnnnnnnnVVnnnnnn.....'
  '....nnnnnnnnnnnnnnnnnnnnnnnnnnnn....'
  '....rr.nnnnnnnnnnnnnnnnnnnnnn.rr....'
  '...r..rnnnnnnnnnnnnnnnnnnnnnnr..r...'
  '...r..r.nnnnnnnnnnnnnnnnnnnn.r..r...'
  '....rr.rrnnnnnnnnnnnnnnnnnnrr.rr....'
  '.......r..rrnnnnnnnnnnnnrr..r.......'
  '.......r..r.rrnnnnnnnnrr.r..r.......'
  '....rr.rr.r..rrrrrrrrrr..r.rr.rr....'
  '...r..r..rr..nKKKKKKKKn..rr..r..r...'
  '...r..r...r.nnKWKWKWKWnn.r...r..r...'
  '....rr....r.nnKKKKKKKKnn.r....rr....'
  '..........rr.nnnnnnnnnn.rr..........'
  '.........r..rrnnnnnnnnrr..r.........'
  '.........r..r.rrnnnnrr.r..r.........'
  '........rr.rr.r.rrrr.r.rr.rr........'
  '.......r..r...r......r...r..r.......'
  '.......r..r...rr....rr...r..r.......'
  '........rr.....rr..rr.....rr........'
  '................rrrr................'
  '................r..r................'
  '...............r....r...............'
  '...............r....r...............'
  '..............rr....rr..............'
  '.............rrr....rrr.............'
  '............rrrr....rrrr............'
  '....................................'
  '....................................'
)

Save-EnemyGrid 'boss_keep_warden' @(# brute: built like the gate he guards - battlement shoulders, slab helm, keyhole chest, cleaver planted
  '....................................'
  '....................................'
  '.............eeeeeeeee..............'
  '............eeeeeeeeeee.............'
  '............eeeeeeeeeee.............'
  '............eeKKKKKKKee.............'
  '............eeeeeeeeeee.............'
  '.............eeeeeeeee..............'
  '...ww.ww.ww...eeeeee............aa..'
  '...wwwwwwwww..wwwwww............aa..'
  '...wwwwwwwwwwwwwwwwwwwww.ww.ww.waa..'
  '...wwwwwwwwwwwwwwwwwwwwwwwwwwwwwaa..'
  '...wwwwwwwwwwwwwwwwwwwwwwwwwwwwwaa..'
  '....qqqqqwwwwwwwwwwwwwwwwwwqqqqqaa..'
  '........wwwwwwwKKKKwwwwwwww.....aa..'
  '........wwwwwwKKKKKKwwwwwww...LSSSS.'
  '........wwwwwwKKKKKKwwwwwww..LSSSSS.'
  '........wwwwwwwKKKKwwwwwwww..LSSSSS.'
  '........2222222KKKK22222222..LSSSSS.'
  '........wwwwwwwwKKwwwwwwwww..LSSSSS.'
  '........wwwwwwwwKKwwwwwwwww..LSSSSS.'
  '........wwwwwwwwKKwwwwwwwww..LSSSSS.'
  '........2222222222222222222..LSSSSS.'
  '........wwwwwwwwwwwwwwwwwww..LSSSSS.'
  '........wwwwwwwwwwwwwwwwwww..LSSSSS.'
  '........qqqqqqqqqqqqqqqqqqq..LSSSSS.'
  '.........wwwwww.....wwwwww...LSSSSS.'
  '.........wwwwww.....wwwwww...LSSSSS.'
  '.........wwwwww.....wwwwww...LSSSSS.'
  '.........wwwwww.....wwwwww...LSSSSS.'
  '.........wwwwww.....wwwwww...LSSSSS.'
  '........wwwwwww.....wwwwwww..LLSSSS.'
  '........qqqqqqq.....qqqqqqq...LLLLS.'
  '.......qqqqqqqq.....qqqqqqqq....LLL.'
  '....................................'
  '....................................'
)

Save-EnemyGrid 'boss_crystal_sorcerer' @(# sorcerer: a brittle void-robed column, a crystal staff taller than it, shards in orbit
  '...........................CC.......'
  '..........................CCCC......'
  '..........................CCCC......'
  '..........................CGCC......'
  '...........nnnnn..........CCCC......'
  '..........nnnnnnn..........CC.......'
  '.........nnnnnnnnn.........pp.......'
  '.........nnnnnnnnn.........pp.......'
  '....C....nnnnnnnnn.........pp.......'
  '...CCC...nnn1C1nnn.........pp.......'
  '....C....nnn111nnn.........pp.......'
  '.........nnnnnnnnn.........pp.......'
  '..........nnnnnnn..........pp.......'
  '......C..nnnnnnnnn.........pp.......'
  '.....CCC.nnnnnnnnnnn.......pp.......'
  '......C.nnnnnnnnnnnnn......pp.......'
  '........nnnnnnnnnnnnnn.....pp.......'
  '........nnnnnnnnnnnnnnn....pp.......'
  '...C....nnnnnnnnnnnnnnnn...pp.......'
  '..CCC...nnnnnnnnnnnnnnnnnnnpp.......'
  '...C....nnnnnnnnnnnnnnnn...pp.......'
  '........nnnnnnnnnnnnnnn....pp.......'
  '........nnnnnnnnnnnnnn.....pp.......'
  '.....C..nnnnnnnnnnnnn......pp.......'
  '....CCC.nnnnnnnnnnnnn......pp.......'
  '.....C..nnnnnnnnnnnnn......pp.......'
  '........nnnnnnnnnnnnn......pp.......'
  '........nnnnnnnnnnnnn......pp.......'
  '........nnnnnnnnnnnnn......pp.......'
  '.......nnnnnnnnnnnnnnn.....pp.......'
  '.......nnnnnnnnnnnnnnn.....pp.......'
  '.......nn.nnnnnnnnn.nn.....pp.......'
  '.......n..nnnnnnnnn..n.....pp.......'
  '..........nnnnnnnnn........pp.......'
  '....................................'
  '....................................'
)

Save-EnemyGrid 'boss_hollow_commander' @(# commander: EMPTY animated armour - the gaps between the plates are the hollowness
  '.......gg.LLLLLLLL..................'
  '......ggg.LLLLLLLLLL................'
  '.....ggg..LLbbbbbbLL................'
  '......gg..LLbbbbbbLL..........S.....'
  '..........LLbbbbbbLL..........S.....'
  '..........LLLLLLLL............S.....'
  '..............................S.....'
  '..............................S.....'
  '......LLLLLLLLLLLLLLLLLL......S.....'
  '.....LLLLLLLLLLLLLLLLLLLL.....S.....'
  '.....LLLLLLLLLLLLLLggLLLL.....S.....'
  '.....LLLLLLLLLLLLggLLLLLL.....S.....'
  '.....LLLLLLLLLLLggLLLLLLL.....S.....'
  '.....LLLLLLLLLLggLLLLLLLL.....S.....'
  '..LL.LLLLLLLLLggLLLLLLLLL.LL..S.....'
  '..LL.LLLLLLLLggLLLLLLLLLL.LL..S.....'
  '..LL.LLLLLLLggLLLLLLLLLLL.LL..S.....'
  '..LL.LLLLLLggLLLLLLLLLLLL.LLLLLL....'
  '..LL.LLLLLggLLLLLLLLLLLLL.LLLLLLL...'
  '..LL..LLLLLLLLLLLLLLLLLL..LLLLLLL...'
  '..LL..LLLLLLLLLLLLLLLLLL...LLLLL....'
  '..LL......................LLLLL.....'
  '..LL......................LLLL......'
  '.LLLL...............................'
  '.LLLL...............................'
  '.......LLLLLL......LLLLLL...........'
  '.......LLLLLL......LLLLLL...........'
  '.......LLLLLL......LLLLLL...........'
  '.......LLLLLL......LLLLLL...........'
  '.......LLLLLL......LLLLLL...........'
  '....................................'
  '.......LLLLLL......LLLLLL...........'
  '......LLLLLLL......LLLLLLL..........'
  '......eeeeeee......eeeeeee..........'
  '....................................'
  '....................................'
)

Save-EnemyGrid 'boss_rush_tyrant' @(# rush: a quadruped CHARGE, weight thrown forward, tusks first
  '....................................'
  '....................................'
  '....................................'
  '....................................'
  '....................................'
  '....................................'
  '....................................'
  '....................................'
  '............gggg....................'
  '.........ggggggggg..................'
  '.......ggggggggggggg................'
  '.....ggggggggggggggggg..............'
  '....ggggggggggggggggggggg...........'
  '...gggggggggggggggggggggggg.........'
  '..ggggggggggggggggggggggggggg.......'
  '..gggggggggggggggggggggggggggggK....'
  '..ggggggggggggggggggggggggggggggg...'
  '...gggggggggggggggggggggggggggggggg.'
  '...ggggggggggggggggggggggggggWWgggg.'
  '....gggggggggggggggggggggggggWWWgg..'
  '.....ggggggggggggggggggggggggWWWW...'
  '......ggggggggggggggggggggggggg.....'
  '......gggg....gggggggggggggggg......'
  '......ggg......gggggg.gggggg........'
  '......ggg.......ggggg..gggg.........'
  '.....ggg........gggg....ggg.........'
  '.....ggg.......gggg.....ggg.........'
  '....ggg.......gggg......ggg.........'
  '....ggg......gggg.......ggg.........'
  '...ggg......gggg........ggg.........'
  '...ggg.....gggg.........ggg.........'
  '..ggg......ggg..........gggg........'
  '..yyyy....yyyy..........yyyy........'
  '.yyyyy...yyyyy.........yyyyy........'
  '....................................'
  '....................................'
)

Save-EnemyGrid 'boss_deep_king' @(  # brute: a GEOLOGICAL monarch - a mineral crest for a crown, a boulder fist
  '........C..........V................'
  '.......CC.C.......VV.V..............'
  '......CCC.CC.....VVV.VV.............'
  '......CCCCCC....VVVVVVV.............'
  '......wwwwwwwwwwwwwwwww.............'
  '.....wwwwwwwwwwwwwwwwwww............'
  '.....wwwwwwwwwwwwwwwwwww............'
  '.....wwwwKKwwwwwwwwKKwww............'
  '.....wwwwKKwwwwwwwwKKwww............'
  '.....wwwwwwwwwwwwwwwwwww............'
  '......wwwwwwwwwwwwwwwww.............'
  '.......wwwwwwwwwwwwwww..............'
  '...eeeeeeeeeeeeeeeeeeeeeeeee........'
  '..eeeeeeeeeeeeeeeeeeeeeeeeeee.......'
  '..eeeeeeeeeeeeeeeeeeeeeeeeeeee......'
  '..eeeeeeeeeeeeeeeeeeeeeeeeeeeeee....'
  '..eeeeeeeeeeeeeeeeeeeeeeeeeeeeeee...'
  '..eeeeeeeeeeeeeeeeeeeeeeeeeeeeeeee..'
  '..eeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeee.'
  '..eeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeee.'
  '..eeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeee.'
  '...eeeeeeeeeeeeeeeeeeeeeeeeeeeeeeee.'
  '...eeeeeeeeeeeeeeeeeeeeeeeeeeeeeee..'
  '....eeeeeeeeeeeeeeeeeeeeeeeeeeeeee..'
  '....qqqqqqqqqqqqqqqqqqqqqqqqqqqqq...'
  '....eeeeeeeeeeeeeeeeeeeeeeeeeeee....'
  '....eeeeeeeeeeeeeeeeeeeeeeeeeee.....'
  '.....eeeeeeeeeeeeeeeeeeeeeeeee......'
  '.....eeeeeeeee.......eeeeeeeee......'
  '.....eeeeeeeee.......eeeeeeeee......'
  '.....eeeeeeeee.......eeeeeeeee......'
  '....eeeeeeeeee.......eeeeeeeeee.....'
  '....qqqqqqqqqq.......qqqqqqqqqq.....'
  '...qqqqqqqqqqq.......qqqqqqqqqqq....'
  '....................................'
  '....................................'
)

Save-EnemyGrid 'boss_blight_matron' @(# sorcerer: a FUNGAL matron - a spotted cap, a veil of hyphae, a censer of spores
  '..............xxxxxxxxx.............'
  '...........xxxxxxxxxxxxxxx..........'
  '.........xxxxxvvxxxxxxxxxxxx........'
  '........xxxxxvvvvxxxxxvvxxxxx.......'
  '.......xxxxxxxvvxxxxxvvvvxxxxx......'
  '......xxxxxxxxxxxxxxxxvvxxxxxxx.....'
  '......xxxxxxxxxxxxxxxxxxxxxxxxx.....'
  '......xxxxxxxvvxxxxxxxxxxxxxxxx.....'
  '......xxxxxxvvvvxxxxxxxxxvvxxxx.....'
  '.......xxxxxxvvxxxxxxxxxvvvvxx......'
  '........xxxxxxxxxxxxxxxxxvvxx.......'
  '.........xxxxxxxxxxxxxxxxxxx........'
  '..........SSSSSSSSSSSSSSSS..........'
  '..........S.S.S.S.S.S.S.S...........'
  '..........S.S.SKS.SKS.S.S...........'
  '..........S.S.S.S.S.S.S.S...........'
  '..........S.S.S.S.S.S.S.S...........'
  '..........S.S.S.S.S.S.S.S...........'
  '..........S...S.S.S.S...S...........'
  '..........S...S.S.S.S...S...........'
  '..............S.S.S.S..........a....'
  '..............S.S.S.S..........a....'
  '..............S.S.S.S..........a....'
  '.............ddddddddd........vvv...'
  '.............ddddddddd.......vvvvv..'
  '.............ddddddddd.......vHvvv..'
  '.............ddddddddd.......vvvvv..'
  '.............ddddddddd........vvv...'
  '.............ddddddddd..............'
  '.............ddddddddd..............'
  '............ddddddddddd.............'
  '............ddddddddddd.............'
  '...........ddddddddddddd............'
  '...........ddddddddddddd............'
  '....................................'
  '....................................'
)

Save-EnemyGrid 'boss_sand_warlord' @(# brute: a wind-braced desert chieftain - layered plates, a scarf tail, a crescent blade
  '....................................'
  '....................................'
  '....................................'
  '.............ffffff.................'
  '............ffffffff................'
  '............ffKKKKff................'
  '............ffffffff................'
  '.............ffffff.................'
  'ss..........dddddddd..........LS....'
  'sss.......dddddddddddd.......LSS....'
  '.sss.....dddddddddddddd.....LSSS....'
  '..sss...dddddddddddddddd...LSSSS....'
  '...sss..ffffffffffffffff..LSSSS.....'
  '....sssddddddddddddddddddLSSSS......'
  '.....ssdddddddddddddddddLSSSS.......'
  '......sddddddddddddddddddSSS........'
  '.......ffffffffffffffffff.a.........'
  '.......dddddddddddddddddd.a.........'
  '.......dddddddddddddddddd...........'
  '.......ffffffffffffffffff...........'
  '.......dddddddddddddddddd...........'
  '.......dddddddddddddddddd...........'
  '........ffffffffffffffff............'
  '........dddddddddddddddd............'
  '........aaaaaaaaaaaaaaaa............'
  '........ddddddd..ddddddd............'
  '........ddddddd..ddddddd............'
  '........ddddddd..ddddddd............'
  '.......dddddddd..dddddddd...........'
  '.......dddddddd..dddddddd...........'
  '.......dddddddd..dddddddd...........'
  '......aaaaaaaaa..aaaaaaaaa..........'
  '.....aaaaaaaaaa..aaaaaaaaaa.........'
  '....................................'
  '....................................'
  '....................................'
)

Save-EnemyGrid 'boss_frost_monarch' @(# sorcerer: a THRONE-BOUND sovereign - the throne back fused to it, a brittle ice crest
  '.........C......C......C............'
  '.........C......C......C............'
  '........CC.....CCC.....CC...........'
  '........CC.....CCC.....CC...........'
  '....pppppppppppppppppppppppp........'
  '....pooooooooooooooooooooooop.......'
  '....po....................op........'
  '....po.....SSSSSSSS.......op........'
  '....po....SSSSSSSSSS......op........'
  '....po....SSKKSSSKKS......op........'
  '....po....SSSSSSSSSS......op........'
  '....po.....SSSSSSSS.......op........'
  '....po....ppppppppp.......op........'
  '....po...ppppppppppp......op........'
  '....po..ppppppppppppp.....op........'
  '....po..ppppppppppppp.....op........'
  '....po..ppppppppppppp.....op........'
  '....po..ppppppppppppp.....op........'
  '....po..ppppppppppppp.....op........'
  '....po..ppppppppppppp.....op........'
  '....po..ppppppppppppp.....op........'
  '....po..ppppppppppppp.....op........'
  '....pooooooooooooooooooooooop.......'
  '....pppppppppppppppppppppppp........'
  '....pp..pppppppppppp......pp........'
  '....pp..pppppppppppp......pp........'
  '....pp..pppppppppppp......pp........'
  '....pp..pppppppppppp......pp........'
  '....pp..pppppppppppp......pp........'
  '....pp..pppppppppppp......pp........'
  '....pp..pppppppppppp......pp........'
  '....pp...pppppppppp.......pp........'
  '....pp...pppppppppp.......pp........'
  '....oo....................oo........'
  '....................................'
  '....................................'
)

Save-EnemyGrid 'boss_obsidian_colossus' @(# brute: a MOUNTAIN, not a robot - stacked slabs, one glowing fissure
  '....................................'
  '....................................'
  '..................n.................'
  '................nnnnn...............'
  '..............mmmnnnnnn.............'
  '.............mmmnnnnnnnn............'
  '............mmmnnnnnnnnnn...........'
  '...........mmmnnnnnnnnnnnn..........'
  '..........mmmnnnnnnnnnnnnnn.........'
  '..........mmmnnnnnnnnnnnnnnn........'
  '.........mmmnnnnnnCnnnnnnn111.......'
  '.........mmmnnnnnnnnCnnnnnn111......'
  '........mmmnnnnnnnnnnCnnnnn111......'
  '........mmmn1111111111C1nnn111......'
  '.......mmmnnnnnnnnnnnnCnnnnn111.....'
  '.......mmmnnnnnnnnnnnnnCnnnn111.....'
  '......mmmnnnnnnnnnnnnnnCnnnnn111....'
  '......mmmnnnnnnnnnnnnnnnCnnnn111....'
  '.....mmmnnnnnnnnnnnnnnnnCnnnnn111...'
  '.....mmm1111111111111111111111nn111.'
  '....mmmnnnnnnnnnnnnnnnnnnnnnnnnnn111'
  '....nnnnnnnnnnnnnnnnnnnnnnnnnnnnn111'
  '....nnnnnnnnnnnnnnnnnnnnnnnnnnnnn111'
  '....nnnnnnnnnnnnnnnnnnnnnnnnnnnnn111'
  '.....nnnnnnnnnnnnnnnnnnnnnnnnnnnn111'
  '.....nnnn11111111111111111111nnn111.'
  '......nnnnnnnnnnnnnnnnnnnnnnnnnn111.'
  '......nnnnnnnnnnnnnnnnnnnnnnnnn111..'
  '.......nnnnnnnnnnnnnnnnnnnnnnn111...'
  '.......nnnnnnnnnnnnnnnnnnnnnnn111...'
  '........nn111111111111111111n111....'
  '........nnnnnnnnnnnnnnnnnnnnnnnn....'
  '.......nnnnnnnnnnnnnnnnnnnnnnnnnnnn.'
  '......nnnnnnnnnnnnnnnnnnnnnnnnnnnnnn'
  '....................................'
  '....................................'
)

Save-EnemyGrid 'boss_hollow_sovereign' @(# commander: a crowned revenant - a broken circlet fused to the skull, a host banner
  '....................................'
  '............l..l..l.................'
  '............l..l..l.................'
  '...........lllllllll................'
  '...........ZZZZZZZZZ................'
  '..........ZZZZZZZZZZZ...............'
  '..........ZZKKZZZKKZZ...............'
  '..........ZZZZZZZZZZZ...............'
  '...........ZZKKKKKZZ................'
  '............ZZZZZZZ.................'
  '..........nnnnnnnnnnn............a..'
  '........nnnnnnnnnnnnnnn..........a..'
  '.......nnnnnnnnnnnnnnnnn.........a..'
  '......nnnnnnnnnnnnnnnnnnn........a..'
  '......nnnnnnnnnnnnnnnnnnn.....7777a.'
  '......nnnnnnnnnnnnnnnnnnn.....7777a.'
  '......nnnnnnnnnnnnnnnnnnnnnnnn7l77a.'
  '......nnnnnnnnnnnnnnnnnnn.....7777a.'
  '......nnnnnnnnnnnnnnnnnnn.....7777a.'
  '......nnnnnnnnnnnnnnnnnnn.....77.7a.'
  '......nnnnnnnnnnnnnnnnnnn.....7..7a.'
  '.......nnnnnnnnnnnnnnnnn.........a..'
  '.......nnnnnnnnnnnnnnnnn.........a..'
  '.......nnnnnnnnnnnnnnnnn.........a..'
  '.......nnnnnnnnnnnnnnnnn.........a..'
  '.......nnnnnnnnnnnnnnnnn.........a..'
  '........nnnnnnnnnnnnnnn..........a..'
  '........nnnnnnnnnnnnnnn..........a..'
  '........nnnnnnnnnnnnnnn..........a..'
  '........nnn.nnnnnnn.nnn..........a..'
  '........nn..nnnnnnn..nn..........a..'
  '........n...nn.n.nn...n..........a..'
  '............nn...nn..............a..'
  '............nn...nn..............a..'
  '....................................'
  '....................................'
)

Save-EnemyGrid 'boss_abyssal_tyrant' @(# rush: an attack with a body attached - all maw and tusks, the rest trailing
  '....................................'
  '....................................'
  '....................................'
  '....................................'
  '....................................'
  '....................................'
  '....................................'
  '....................................'
  '....................................'
  '.........yyyy....i.......i..........'
  '.......yyyyyyyy..ii.....ii..........'
  '.....yyyyyyyyyyyyii....iii.....i....'
  '....yyyyyyyyyyyyyyyyi..iii....ii....'
  '...yyyyyyyyyyyyyyyyyyyyiii...iii....'
  '...yyyyyyyyyyyyyyyyyyyyyyyy.........'
  '..yyyyyyyyyyyyyyyyyyyyyyyyyyy.......'
  '..yyyyyyyyyyyyyyyyyyyyyyyyyyyyyy....'
  '..yyyyyyyyyyyyyyyyyyyyyyyKyyyyyyy...'
  '...yyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyW.'
  '...yyyyyyyyyyyyyyyyyyyyyyyyKKKKKKWW.'
  '....yyyyyyyyyyyyyyyyyyyyyKWKWKWKWW..'
  '....yyyyyyyyyyyyyyyyyyyyyKKKKKKKK...'
  '.....yyyyyyyyyyyyyyyyyyyyyyyWWWW....'
  '.....yyyyyyyyyyyyyyyyyyyyyyyyyy.....'
  '......yyyyyyyyyyyyyyyyyyyyyyy.......'
  '......yyyyy....yyyyyyyy.yyyy........'
  '......yyyy......yyyyyy...yyy........'
  '.....yyyy.......yyyyy....yyy........'
  '.....yyy........yyyy.....yyy........'
  '....yyy........yyyy......yyy........'
  '....yyy.......yyyy.......yyyy.......'
  '...yyy........yyy........yyyy.......'
  '..tttt.......tttt.......ttttt.......'
  '.ttttt......ttttt......tttttt.......'
  '....................................'
  '....................................'
)

Save-EnemyGrid 'boss_dread_sovereign' @(# sorcerer: elegant, hostile - a narrow core, a staff of thorns, afflictions in orbit
  '....................................'
  '.............mmmm..........9........'
  '............mmmmmm.........9........'
  '............mmVVmm........V9V.......'
  '............mmmmmm.........9........'
  '.............mmmm..........9........'
  '..............mm...........9........'
  '...........mmmmmmmm........9........'
  '..........mmmmmmmmmm.......9........'
  '..........mmmmmmmmmm.......9........'
  '..........mmmmmmmmmm.....v.9........'
  '..........mmmmmmmmmm......v9........'
  '..........mmmmmmmmmm.......9v.......'
  '....H.....mmmmmmmmmm.......9........'
  '...HHH....mmmmmmmmmm.....v.9........'
  '....H.....mmmmmmmmmm......v9........'
  '..........mmmmmmmmmmmmmmmmm9........'
  '..........mmmmmmmmmm.......9........'
  '..........mmmmmmmmmm.......9........'
  '.....C....mmmmmmmmmm.......9........'
  '....CCC...mmmmmmmmmm.......9........'
  '.....C....mmmmmmmmmm.......9........'
  '..........mmmmmmmmmm.......9........'
  '..........mmmmmmmmmm.......9........'
  '...........mmmmmmmm........9........'
  '...........mmmmmmmm........9........'
  '...........mmmmmmmm........9........'
  '...........mmmmmmmm........9........'
  '...........mmmmmmmm........9........'
  '..........mmmmmmmmmm.......9........'
  '..........mmmmmmmmmm.......9........'
  '..........mmm.mm.mmm.......9........'
  '..........mm..mm..mm.......9........'
  '..........m...mm...m.......9........'
  '....................................'
  '....................................'
)

Save-EnemyGrid 'boss_the_hollow_king' @(# the King: the ONE literal crown, a tower of a crown, a mantle, the sceptre
  '..........YY....YY....YY............'
  '..........YY....YY....YY............'
  '..........YYYYYYYYYYYYYY............'
  '..........YYYYYYYYYYYYYY............'
  '..........YYYYYYYYYYYYYY............'
  '...........YYYYYYYYYYYY.............'
  '...........bbbbbbbbbbbb.............'
  '...........bbbbbbbbbbbb.............'
  '...........bbVVbbbbVVbb.............'
  '...........bbbbbbbbbbbb.............'
  '...........bbbbbbbbbbbb.............'
  '............bbbbbbbbbb..............'
  '.........SSSSSSSSSSSSSSSS.....V.....'
  '.......SSmmmmmmmmmmmmmmmmSS..VGV....'
  '......mmmmmmmmmmmmmmmmmmmmmm..V.....'
  '.....mmmmmmmnmmmmmmmmmmnmmmmm.Y.....'
  '.....mmmmmmmnmmmmmmmmmmnmmmmm.Y.....'
  '.....mmmmmmmnmmmmmmmmmmnmmmmm.Y.....'
  '.....mmmmmmmnmmmmmmmmmmnmmmmm.Y.....'
  '.....mmmmmmmmmmmmmmmmmmmmmmmmmY.....'
  '.....mmmmmmmnmmmmmmmmmmnmmmmm.Y.....'
  '.....mmmmmmmnmmmmmmmmmmnmmmmm.Y.....'
  '.....mmmmmmmnmmmmmmmmmmnmmmmm.Y.....'
  '.....mmmmmmmnmmmmmmmmmmnmmmmm.Y.....'
  '.....mmmmmmmnmmmmmmmmmmnmmmmm.Y.....'
  '.....mmmmmmmnmmmmmmmmmmnmmmmm.Y.....'
  '.....mmmmmmmnmmmmmmmmmmnmmmmm.Y.....'
  '.....mmmmmmmnmmmmmmmmmmnmmmmm.Y.....'
  '.....mmmmmmmnmmmmmmmmmmnmmmmm.Y.....'
  '.....mmmmmmmnmmmmmmmmmmnmmmmm.Y.....'
  '.....mmmmmmmnmmmmmmmmmmnmmmmm.Y.....'
  '.....mmmmmmmnmmmmmmmmmmnmmmmm.Y.....'
  '.....mmmmmmmnmmmmmmmmmmnmmmmm.Y.....'
  '.....YYYYYYYYYYYYYYYYYYYYYYYY.Y.....'
  '....................................'
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

# --- M84: the seven Guild Masters (36x36) and their courts (24x24). Same
# --- rules as every boss above: 2-3 readable masses, one thematic motif from
# --- the bosses.json entry, one asymmetry, no crowns. All hand-placed — no
# --- random helper, so every earlier sprite stays byte-identical. ---

Save-EnemyGrid 'boss_guild_foreman_brakk' @(# brute: the First Chair himself - a stone bulk in a chair-back, the DENIED stamp raised
  '....................................'
  '....................................'
  '...aaaaaaaaaaaaaaa..................'
  '...a.a.a.a.a.a.a.a..................'
  '...aaaaaaaaaaaaaaa..................'
  '...aa...........aa......8888........'
  '...aa..eeeeeee..aa.....888888.......'
  '...aa.eeeeeeeee.aa.....899998.......'
  '...aa.eeKKeeKKe.aa.....888888.......'
  '...aa.eeeeeeeee.aa......8888........'
  '...aa.eeeeeeeee.aa.......aa.........'
  '...aa..eeeeeee..aa.......aa.........'
  '...aaeeeeeeeeeeeaa.......aa.........'
  '...aeeeeeeeeeeeeeaa......aa.........'
  '...aeeeeeeeeeeeeeeaa.....aa.........'
  '...aeeeeeeeeeeeeeeeaa....aa.........'
  '...aeeeeeeeeeeeeeeeeaa..eee.........'
  '...aeeeeeeeeeeeeeeeeeeeeeee.........'
  '...aeeeeeeeeeeeeeeeeeeeeee..........'
  '...aeeeeeeeeeeeeeeeeeeee............'
  '...aeeeeeeeeeeeeeeeeee..............'
  '...aeeeeeeeeeeeeeeeee...............'
  '...aeeeeeeeeeeeeeeeee...............'
  '...aqqqqqqqqqqqqqqqqq...............'
  '...aaaaaaaaaaaaaaaaaa...............'
  '...aa.eeeeee.eeeeee.................'
  '...aa.eeeeee.eeeeee.................'
  '...aa.eeeeee.eeeeee.................'
  '...aa.eeeeee.eeeeee.................'
  '...aa.eeeeee.eeeeee.................'
  '...aa.eeeeee.eeeeee.................'
  '...aa.eeeeee.eeeeee.................'
  '...aa.qqqqqq.qqqqqq.................'
  '...aaqqqqqqq.qqqqqqq................'
  '....................................'
  '....................................'
)

Save-EnemyGrid 'boss_guild_auditor_vess' @(# sorcerer: a narrow hooded column, the headsman's quill held like an axe
  '....................................'
  '...........nnnnnn.......8...........'
  '..........nnnnnnnn.....88...........'
  '..........nnnnnnnn....888...........'
  '..........nnnnnnnn...888............'
  '..........nn1111nn..8888............'
  '..........nn1K1Knn..888.............'
  '..........nn1111nn.888..............'
  '...........nnnnnn..888..............'
  '...........nnnnnn.888...............'
  '.........nnnnnnnnnn88...............'
  '........nnnnnnnnnnnn8...............'
  '........nnnnnnnnnnnnn...............'
  '........nnnnnnnnnnnnna..............'
  '........nnnnnnnnnnnnn.a.............'
  '........nnnnnnnnnnnnn..a............'
  '........nnnnnnnnnnnnn...a...........'
  '........nnnnnnnnnnnnn....a..........'
  '........nnnnnnnnnnnnn.....a.........'
  '........nnnnnnnnnnnnn......a........'
  '........nnnnnnnnnnnnn.......a.......'
  '........nnnnnnnnnnnnn...............'
  '........nnnnnnnnnnnnn...............'
  '........nnnnnnnnnnnnn...............'
  '........nnnnnnnnnnnnn...............'
  '........nnnnnnnnnnnnn...............'
  '........nnnnnnnnnnnnn...............'
  '........nnnnnnnnnnnnn...............'
  '........nnnnnnnnnnnnn...............'
  '.......nnnnnnnnnnnnnnn..............'
  '.......nnnnnnnnnnnnnnn..............'
  '.......nn.nnnnnnnnn.nn..............'
  '.......n..nnnnnnnnn..n..............'
  '..........nnnnnnnnn.................'
  '....................................'
  '....................................'
)

Save-EnemyGrid 'boss_guild_chain_mistress' @(# commander: crested enforcer, the collection chain and its ankle shackle swung wide
  '....................................'
  '.............ggg....................'
  '............ggggg...................'
  '...........gggggggg.................'
  '...........wwwwwwwg.................'
  '..........wwwwwwwwwg................'
  '..........wwKKKKKww.................'
  '..........wwwwwwwww.................'
  '...........wwwwwww..................'
  '........wwwwwwwwwwwww...............'
  '.......wwwwwwwwwwwwwww..............'
  '......wwwwwwwwwwwwwwwww.............'
  '......wwwwwwwwwwwwwwwwwww...........'
  '......wwwwwwwwwwwwwwwwwwwr..........'
  '......wwwwwwwwwwwwwwwww...r.........'
  '......wwwwwwwwwwwwwwwww....r........'
  '......wwwwwwwwwwwwwwwww...r.r.......'
  '.......wwwwwwwwwwwwwww...r...r......'
  '.......wwwwwwwwwwwwwww....r.r.......'
  '.......wwwwwwwwwwwwwww.....r........'
  '.......wwwwwwwwwwwwwww....r.r.......'
  '.......wwwwwwwwwwwwwww...r...r......'
  '.......wwwwwwwwwwwwwww....r.r.......'
  '.......wwqqqqqqqqqqqww.....r........'
  '.......wwwwwwwwwwwwwww....rrrr......'
  '.......wwwwwww.wwwwwww...rr..rr.....'
  '.......wwwwwww.wwwwwww...rr..rr.....'
  '.......wwwwwww.wwwwwww....rrrr......'
  '.......wwwwwww.wwwwwww..............'
  '......wwwwwwww.wwwwwwww.............'
  '......wwwwwwww.wwwwwwww.............'
  '......wwwwwwww.wwwwwwww.............'
  '......qqqqqqqq.qqqqqqqq.............'
  '.....qqqqqqqqq.qqqqqqqqq............'
  '....................................'
  '....................................'
)

Save-EnemyGrid 'boss_guild_warden_mole' @(# brute: a vault door with a pulse - the unturned keyring, the slot of an eye
  '....................................'
  '....................................'
  '....................................'
  '.......eeeeeeeeeeeeeeeeeeee.........'
  '......eeeeeeeeeeeeeeeeeeeeee........'
  '.....eerrrrrrrrrrrrrrrrrrrree.......'
  '.....eereeeeeeeeeeeeeeeeeeree.......'
  '.....eereeeeeeeeeeeeeeeeeeree.......'
  '.....eereeeeeKKKKKKKKeeeeeree.......'
  '.....eereeeeeeeeeeeeeeeeeeree.......'
  '.....eereeeeeeeeeeeeeeeeeeree.......'
  '.....eereeeeeeeeeeeeeeeeeeree.......'
  '.....eereeeeeeeeeeeeeeeeeeree...ll..'
  '.....eereeeeeeeeeeeeeeeeeeree..l..l.'
  '.....eereeeeeeeeeeeeeeeeeeree..l..l.'
  '.....eereeeeeeeeCCeeeeeeeeree...ll..'
  '.....eereeeeeeeeCCeeeeeeeeree....l..'
  '.....eereeeeeeeeeeeeeeeeeeree...ll..'
  '.....eereeeeeeeeeeeeeeeeeeree...l...'
  '.....eereeeeeeeeeeeeeeeeeeree..ll...'
  '.....eereeeeeeeeeeeeeeeeeeree...l...'
  '.....eereeeeeeeeeeeeeeeeeeree.......'
  '.....eereeeeeeeeeeeeeeeeeeree.......'
  '.....eereeeeeeeeeeeeeeeeeeree.......'
  '.....eerrrrrrrrrrrrrrrrrrrree.......'
  '......eeeeeeeeeeeeeeeeeeeeee........'
  '.......eeeeeeeeeeeeeeeeeeee.........'
  '.........eeeeee....eeeeee...........'
  '.........eeeeee....eeeeee...........'
  '.........eeeeee....eeeeee...........'
  '.........eeeeee....eeeeee...........'
  '........eeeeeee....eeeeeee..........'
  '........qqqqqqq....qqqqqqq..........'
  '.......qqqqqqqq....qqqqqqqq.........'
  '....................................'
  '....................................'
)

Save-EnemyGrid 'boss_guild_cutlery_curator' @(# sorcerer: a night-robed keeper, THE spoon held aloft like a relic
  '..........................SSS.......'
  '.........................SSSSS......'
  '.........................SSSSS......'
  '.........................SSSSS......'
  '..........................SSS.......'
  '...........33333...........r........'
  '..........3333333..........r........'
  '..........33K3K33..........r........'
  '..........3333333..........r........'
  '...........33333...........r........'
  '..........3333333..........r........'
  '.........333333333.........r........'
  '........33333333333........r........'
  '........33333333333........r........'
  '........33333333333........r........'
  '........333333333333.......r........'
  '........3333333333333......r........'
  '........33333333333333.....r........'
  '........333333333333333....r........'
  '........3333333333333333...r........'
  '........33333333333333333..r........'
  '........333333333333333333.r........'
  '........3333333333333333333r........'
  '........33333333333.................'
  '........33333333333.................'
  '........33333333333.................'
  '........33333333333.................'
  '........33333333333.................'
  '........33333333333.................'
  '.......3333333333333................'
  '.......3333333333333................'
  '.......333.3333333.33...............'
  '.......33..3333333..33..............'
  '...........3333333..................'
  '....................................'
  '....................................'
)

Save-EnemyGrid 'boss_guild_grandmaster_ossia' @(# commander: the one Master who still duels - an upright fencer, blade raised
  '..............................S.....'
  '..............................S.....'
  '..............................S.....'
  '............SSSS..............S.....'
  '...........SSSSSS.............S.....'
  '...........SSKKSS.............S.....'
  '...........SSSSSS.............S.....'
  '............SSSS..............S.....'
  '..........NNNNNNNN............S.....'
  '.........NNNNNNNNNN...........S.....'
  '........NNNNNNNNNNNN..........S.....'
  '........NNNNNNNNNNNN..........S.....'
  '........NNNNNNNNNNNNNN........S.....'
  '........NNNNNNNNNNNNNNNN......S.....'
  '........NNNNNNNNNNNNNNNNNN....S.....'
  '........NNNNNNNNNNNNNNNNNNNN..S.....'
  '........NNNNNNNNNNNN.....NNNNlll....'
  '........NNNNNNNNNNNN..........a.....'
  '........NNNNNNNNNNNN................'
  '........NNNNNNNNNNNN................'
  '........NNNNNNNNNNNN................'
  '........NNNNNNNNNNNN................'
  '........NNNlllllllNN................'
  '........NNNNNNNNNNNN................'
  '........NNNNNNNNNNNN................'
  '........NNNNNN.NNNNN................'
  '........NNNNN...NNNN................'
  '........NNNNN...NNNN................'
  '.......NNNNN.....NNNN...............'
  '.......NNNNN.....NNNN...............'
  '......NNNNN.......NNNN..............'
  '......NNNNN.......NNNN..............'
  '.....qqqqqq.......qqqqq.............'
  '....qqqqqqq.......qqqqqq............'
  '....................................'
  '....................................'
)

Save-EnemyGrid 'boss_guild_registrar_null' @(# sorcerer: not a person holding a ledger - a standing void ledger with a face of nothing
  '....................................'
  '....................................'
  '.......bbbbbbbbbbbbbbbbbbbbb........'
  '......bbbbbbbbbbbbbbbbbbbbbbb.......'
  '......bb.....................b......'
  '......bb.....................b......'
  '......bb.....................b......'
  '......bb.....V.......V.......b......'
  '......bb.....................b......'
  '......bb.....................b......'
  '......bb.....................b......'
  '......bb.....................b......'
  '......bb...1111111111111.....b......'
  '......bb.....................b......'
  '......bb...1111111111111.....b......'
  '......bb.....................b......'
  '......bb...1111111111111.....b......'
  '......bb.....................b......'
  '......bb...1111111111111.....b......'
  '......bb.....................b......'
  '......bb...1111111111111.....b......'
  '......bb.....................b......'
  '......bb...1111111111111.....b......'
  '......bb.....................b......'
  '......bb.....................b......'
  '......bb.....................b......'
  '......bb.....................b......'
  '......bb.....................b......'
  '......bb.....................b......'
  '......bb.....................b......'
  '......bbbbbbbbbbbbbbbbbbbbbbbb......'
  '.......bbbbbbbbbbbbbbbbbbbbbb.......'
  '..........bbbbb.......bbbbb.........'
  '..........bbbbb.......bbbbb.........'
  '....................................'
  '....................................'
)

Save-EnemyGrid 'guild_clerk' @(     # a paper stack with a clerk attached, one red stamp
  '........................'
  '......SSSSSSSSSSS.......'
  '......SZZZZZZZZZS.......'
  '......SSSSSSSSSSS.......'
  '......SZZZZZZZZZS.......'
  '......SSSSSSSSSSS.......'
  '......SZZZZZZZZZS.......'
  '......SSSSSSSSSSS.......'
  '......SZZZgZZZZZS.......'
  '......SSSSSSSSSSS.......'
  '......SZZZZZZZZZS.......'
  '......SSSSSSSSSSS.......'
  '.....aaaaaaaaaaaaa......'
  '.......hhhhh.aa.........'
  '.......hKhKh..aa........'
  '.......hhhhh...aa.......'
  '........hhh.............'
  '......qqqqqqqqq.........'
  '......qqqqqqqqq.........'
  '......qqqqqqqqq.........'
  '......qqqq.qqqq.........'
  '......qqq...qqq.........'
  '.....qqqq...qqqq........'
  '........................'
)

Save-EnemyGrid 'guild_inkwing' @(   # a bird made of spilled ink mid-swoop, drips trailing
  '........................'
  '........................'
  '.....n..................'
  '.....nn.................'
  '......nnn...............'
  '.......nnnn.............'
  '........nnnnn...........'
  '....n....nnnnnn.........'
  '....nn....nnnnnnnnn.....'
  '.....nnn...nnnnnnnnnn...'
  '......nnnnnnnnnnnWnnnn..'
  '.......nnnnnnnnnnnnnnl..'
  '........nnnnnnnnnnnn....'
  '.........nnnnnnnnn......'
  '..........nnnnnn........'
  '..........nn.nn.........'
  '.........nn...nn........'
  '..........n...n.........'
  '..........n.............'
  '..........n...n.........'
  '..............n.........'
  '........................'
  '........................'
  '........................'
)

Save-EnemyGrid 'guild_ledger_golem' @(# a walking backlog: six mismatched ledgers stacked
  '........................'
  '........................'
  '.......aaaaaaaaa........'
  '.......adddddddda.......'
  '.......aaaaaaaaa........'
  '......ssssssssss........'
  '......sfffffffffs.......'
  '......ssssssssss........'
  '.....aaaaaaaaaaaaa......'
  '.....addddddddddda......'
  '.....aaaaaaaaaaaaa......'
  '......gggggggggg........'
  '......ghhhhhhhhg........'
  '......gggggggggg........'
  '.....aaaaaaaaaaaa.......'
  '.....addddKKddddda......'
  '.....aaaaaaaaaaaa.......'
  '......ssssssssss........'
  '......sffffffffs........'
  '......ssssssssss........'
  '......aaaa..aaaa........'
  '......aaa....aaa........'
  '.....aaaa....aaaa.......'
  '........................'
)

Save-EnemyGrid 'guild_debt_hound' @(# all sprint: a lean hound with an invoice tag on its collar
  '........................'
  '........................'
  '........................'
  '........................'
  '........................'
  '........................'
  '...........qqqq.........'
  '...w......qqqqqqq.......'
  '....w....qqqqqqKqq......'
  '.....w..qqqqqqqqqqqq....'
  '......wqqqqqqqqqqqWW....'
  '......qqqqqqqqqqqqq.....'
  '......qqqqqqSSqqqq......'
  '......qqqqqqSlSqqq......'
  '.....qqqqqqqqSSqqq......'
  '.....qqq.....qqqq.......'
  '....qqq......qqq........'
  '...qqq.......qqq........'
  '..qqq.........qqq.......'
  '.qqq...........qqq......'
  '.qq.............qq......'
  '.qq.............qq......'
  '.qqq............qqq.....'
  '........................'
)

Save-EnemyGrid 'guild_chirurgeon' @(# the field medic: white robes, a green-cross satchel, a saw
  '........................'
  '.........SSSSS..........'
  '........SSSSSSS.........'
  '........SSKSKSS.........'
  '........SSSSSSS.........'
  '.........SSSSS..........'
  '.......SSSSSSSSS........'
  '......SSSSSSSSSSS.......'
  '.....SSSSSSSSSSSSS......'
  '.....SSSSSSSSSSSSS..L...'
  '.....SSSSSSSSSSSSS..LL..'
  '.....SSSSSSSSSSSSSSLL...'
  '.....SSSSSSSSSSSSS.LL...'
  '.....SSSSSSSSSSSSS.LL...'
  '.....SSSSSSSSSSSSS.L....'
  '.....SSSSSSSSSSSSS......'
  '....gggggSSSSSSSSS......'
  '....ggHggSSSSSSSSS......'
  '....gHHHgSSSSSSSSS......'
  '....ggHggSSSSSSSSS......'
  '....ggggg.SSSSSSS.......'
  '.........SSS.SSS........'
  '........SSSS.SSSS.......'
  '........................'
)

Save-EnemyGrid 'guild_vault_mimic' @(# the vault's oldest joke: a strongbox with legs and teeth
  '........................'
  '........................'
  '........................'
  '........................'
  '......aaaaaaaaaaaaa.....'
  '.....addddddddddddda....'
  '.....adddkkkkkkkddda....'
  '.....adddkdddddkddda....'
  '.....adddkkkkkkkddda....'
  '.....adddddddddddda.....'
  '.....aaaaaaaaaaaaaaa....'
  '.....aKWKWKWKWKWKWKa....'
  '.....aKKKKKKKKKKKKKa....'
  '.....aWKWKWKWKWKWKWa....'
  '.....aaaaaaaaaaaaaaa....'
  '.....addddddddddddda....'
  '.....adddkkkkkdddddda...'
  '.....addddddddddddda....'
  '.....aaaaaaaaaaaaaaa....'
  '.......aa......aa.......'
  '......aaa......aaa......'
  '......aaa......aaa......'
  '.....aaaa......aaaa.....'
  '........................'
)

Save-EnemyGrid 'guild_key_rat' @(   # the Warden's runner: a grey rat at full sprint, key in its teeth
  '........................'
  '........................'
  '........................'
  '........................'
  '........................'
  '........................'
  '........................'
  '........................'
  '..........ee............'
  '..........ee.eeee.......'
  '............eeeeeee.....'
  '......eeeeeeeeeKeee.....'
  '.....eeeeeeeeeeeeeee....'
  '....eeeeeeeeeeeeeeeel...'
  '.w..eeeeeeeeeeeeee.lll..'
  '..ww.eeeeeeeeeeeee..l...'
  '....wweeeeeeeeeee...l...'
  '......eee.....eee...ll..'
  '.....eee.......eee......'
  '....eee.........eee.....'
  '...eee...........eee....'
  '..eee.............eee...'
  '..ee...............ee...'
  '........................'
)

Save-EnemyGrid 'guild_fork_fiend' @(# an imp whose skull tapers into three tines
  '.......W....W....W......'
  '.......W....W....W......'
  '.......W....W....W......'
  '.......W....W....W......'
  '.......WWWWWWWWWWW......'
  '.........WWWWWWW........'
  '.........WWWWWWW........'
  '.........WhhhhhW........'
  '.........hhKhKhh........'
  '.........hhhhhhh........'
  '..........hhhhh.........'
  '........hhhhhhhhh.......'
  '.......hhhhhhhhhhh......'
  '......hhhhhhhhhhhhh.....'
  '......hhhhhhhhhhhhh.....'
  '......hh.hhhhhhh.hh.....'
  '......hh.hhhhhhh.hh.....'
  '.........hhhhhhh........'
  '.........hhhhhhh........'
  '.........hhh.hhh........'
  '........hhh...hhh.......'
  '.......hhh.....hhh......'
  '......hhh.......hhh.....'
  '........................'
)

Save-EnemyGrid 'guild_ladle_shade' @(# what is left when a soup ladle is haunted: a hood of it
  '........................'
  '........ZZZZZZZZ........'
  '.......ZZZZZZZZZZ.......'
  '......ZZZZZZZZZZZZ......'
  '......ZZZZZZZZZZZZ......'
  '.......ZZZZZZZZZZ.......'
  '........ZZZZZZZZ........'
  '..........nnnn..........'
  '.........nnnnnn.........'
  '.........nKnnKn.........'
  '.........nnnnnn.........'
  '.......nnnnnnnnnn.......'
  '......nnnnnnnnnnnn......'
  '.....nnnnnnnnnnnnnn.....'
  '.....nnnnnnnnnnnnnn.....'
  '.....nnnnnnnnnnnnnn.....'
  '......nnnnnnnnnnnn......'
  '......nnnnnnnnnnnn......'
  '.......nnnnnnnnnn.......'
  '.......nn.nnnn.nn.......'
  '........n..nn..n........'
  '...........nn...........'
  '............n...........'
  '........................'
)

Save-EnemyGrid 'guild_seat_warden' @(# honor guard of the Tenth Seat: plate with a chair-back shield
  '........................'
  '.........wwwww..........'
  '........wweeeew.........'
  '........weKKKew.........'
  '........wweeeew.........'
  '.........wwwww..........'
  '......wwwwwwwwww.aaaaa..'
  '.....wwwwwwwwwww.adada..'
  '....wwwwwwwwwwwwwadada..'
  '....wwwwwwwwwwwwwadada..'
  '....wwwwwwwwwwwwwadada..'
  '....wwwwwwwwwwwwwaaaaa..'
  '....wwwwwwwwwwwwwadada..'
  '....wwwwwwwwwwwwwadada..'
  '.....wwwwwwwwwww.aaaaa..'
  '.....wwwwwwwwwww.a...a..'
  '.....wwqqqqqqqww.a...a..'
  '......wwwwwwwww..a...a..'
  '......wwww.wwww.........'
  '......www...www.........'
  '.....wwww...wwww........'
  '.....qqqq...qqqq........'
  '....qqqqq...qqqqq.......'
  '........................'
)

Save-EnemyGrid 'guild_null_notary' @(# the clerk-of-record: a blank page for a face, a quill
  '........................'
  '.........SSSSS..........'
  '........SSSSSSS.........'
  '........SSSSSSS.........'
  '........SSSSSSS.........'
  '........SSSSSSS.........'
  '.........SSSSS..........'
  '.......qqqqqqqqq........'
  '......qqqqqqqqqqq....W..'
  '.....qqqqqqqqqqqqq...W..'
  '.....qqqqqqqqqqqqq..W...'
  '.....qqqqqqqqqqqqq..W...'
  '.....qqqqqqqqqqqqqqqW...'
  '.....qqqqqqqqqqqqq..a...'
  '.....qqqqqqqqqqqqq......'
  '.....qqqqqqqqqqqqq......'
  '.....qqqqqqqqqqqqq......'
  '......qqqqqqqqqqq.......'
  '......qqqqqqqqqqq.......'
  '......qqqqq.qqqqq.......'
  '......qqqq...qqqq.......'
  '.....qqqqq...qqqqq......'
  '.....qqqqq...qqqqq......'
  '........................'
)

Save-EnemyGrid 'guild_final_clause' @(# the contract that collects itself: an unrolling scroll
  '........................'
  '........................'
  '..........fff...........'
  '.........fSSSf..........'
  '.........fSSSf..........'
  '.........fSSSf..........'
  '........ffSSSff.........'
  '.......fSSSSSSSf........'
  '......fSSSKSSKSSf.......'
  '......fSSSSSSSSSf.......'
  '......fSSSSSSSSSf.......'
  '......fSSqqqqqSSf.......'
  '......fSSSSSSSSSf.......'
  '......fSSqqqqqSSf.......'
  '......fSSSSSSSSSf.......'
  '......fSSqqqqqSSf.......'
  '......fSSSSSSSSSf.......'
  '......fSSSSSSSSSf.......'
  '.......fSSSSSSSf........'
  '........ffSSSff.........'
  '.........fSSSf..........'
  '..........fff...........'
  '...........gg...........'
  '........................'
)

# --- M85/M115: the Last Dragon (36x36). M115 (owner direction): a DRAGON
# --- silhouette within the boss canvas - a horned head with a snout and
# --- exactly ONE open red eye (the telegraph), a neck, one raised wing with
# --- daylight between its membrane fingers (negative space, S5), foreclaws,
# --- a substantial torso with stone belly bands, a tail curling out the
# --- trailing side with a spade tip. Void ramp body, stone highlights, gold
# --- horns / claws / spines. Asymmetric, facing right. Smoke drifts from
# --- the nostril. RNG-free like every grid above; hand-placed rows.
Save-EnemyGrid 'boss_the_dragon' @(
  '...............Y.........Y.Y........'
  '.........Y.....B.....Y....B.Y.......'
  '..........B.....B...nB.....YY......1'
  '....Y.....B........nnB.....mYYmmmm.2'
  '....nB.....B....B.nnnB.....mmmmmmm2.'
  '.....nBB....B...BnnnBnn....mmmDDmmm.'
  '.....nnnB.nnnBnnnBnnBnn.....mmDGmKm.'
  '......nnnBnnnBnnnBnnBnn....mmmmmmmBB'
  '..........BnnnBnnBnnBnn....mmmmmmnnn'
  '...........BBnnBnBnnBnn...mmnYmmmnnn'
  '.............BnBnnBnBnn...mmnmm.....'
  '..............BnBnBnBn...mmnmmm.....'
  '...............BnBBBn...mmnYmm......'
  '................BBBBn...mYmmm.......'
  '..................BBn..Ymnmmm.......'
  '................mmmBmmmmnmmm........'
  '..............mmYmmmmmmmmm..........'
  '.............mYmmmmmmmmmmmm.........'
  '............Ymmmmmmmmmmmmmmm........'
  '..........mmmmmmmmmmmmmmmmmmm.......'
  '..........mmmmmmmmmmmmmmmmmmm.......'
  '..........mnnnnmmmmmmmmmmmmmm.......'
  '..........mnnnnmmmmmmmmmmmmmm.......'
  '..........mnnnnmmmmmmmmmmmmmm.......'
  '..........mnnBBBBBBBBBBBBmmmm.......'
  'Y.Y.......mnnqqqqqqqqqqqqmmmm.......'
  '.Y........mnnqqqqqqqqqmmmmmmm.......'
  '.n........mnnqqqqqqqqqmmmmmmm.......'
  '.nm.......mmnnnnnnnwwwmmmmmmm.......'
  '..n.......mmnnnnnnnwwwwmmmmmmm......'
  '..nm.....mmmnnnnnnnmmmmmmnnnnm......'
  '...mmmmmmmmmnnnnnnnmmmmmmnnnnm......'
  '...mmmmmmmmmnnnnnnn.....mnnnnmm.....'
  '....nnnnn...nnnnnnn.....mnnnnmY.....'
  '............Y.Y.Y........Y.Y.Y......'
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
# Scene helpers (M113, hoisted here in M119 so the interiors can use them).
function GPoly($g, [int]$a, [string]$hex, [int[]]$xy) {
  $pts = @()
  for ($i = 0; $i -lt $xy.Count; $i += 2) { $pts += (New-Object System.Drawing.Point($xy[$i], $xy[$i + 1])) }
  $g.FillPolygon((New-Object System.Drawing.SolidBrush(Ca $a $hex)), [System.Drawing.Point[]]$pts)
}
function Stars($b, [int]$n, [string]$hex, [int]$maxY) {
  for ($i = 0; $i -lt $n; $i++) { $x = [int]((Rnd) * $BW); $y = [int]((Rnd) * $maxY); P $b $x $y $hex }
}
function FloorSpeckle($b, [int]$n, [string]$hex) {
  for ($i = 0; $i -lt $n; $i++) { $x = [int]((Rnd) * $BW); $y = 110 + [int]((Rnd) * 100); P $b $x $y $hex }
}

# M119 (owner direction 2026-09-14): the six interiors redrawn as painted
# rooms on the M113 stage recipe - layered silhouettes with the composition
# weight in the side margins and the strips above and below the panels (the
# opaque header band covers y 0..24 and the frames the middle), the caption
# row (y 26..40) kept quiet at the source (the states also back their bare
# captions with a translucent strip), fills darker than the panel fills, no
# signal colours. The M32 town shades regenerate from these bases.

# Inn - a timbered common room: beams, a shuttered window with lamplight,
# a hearth at the right, a plank floor with a rug edge.
$r = New-Bg '#241A18' '#150F0D'
GFill $r[1] 110 $PAL.earth1 0 24 426 5                                     # the ceiling beam
foreach ($bx in 6, 62, 358, 414) { GFill $r[1] 90 $PAL.earth1 $bx 24 6 110 }   # posts
GFill $r[1] 120 $PAL.night3 10 46 22 36; GFill $r[1] 70 $PAL.gold 13 49 16 30   # the window + lamplight
GFill $r[1] 120 $PAL.earth1 20 49 1 30; GFill $r[1] 120 $PAL.earth1 13 63 16 1  # mullions
GEll $r[1] 36 $PAL.gold 330 22 90 60                                        # lantern glow, top right
GFill $r[1] 140 $PAL.stone2 386 118 36 70; GFill $r[1] 160 $PAL.night2 392 130 24 46   # the hearth
GEll $r[1] 90 $PAL.danger 396 146 16 22; GEll $r[1] 70 $PAL.gold 399 152 10 12       # its fire
GFill $r[1] 255 $PAL.earth1 0 190 426 50                                    # plank floor
foreach ($py in 198, 206, 214) { GFill $r[1] 60 $PAL.night2 0 $py 426 1 }
GFill $r[1] 120 $PAL.maroon 120 196 186 18; GFill $r[1] 80 $PAL.gold 120 196 186 1; GFill $r[1] 80 $PAL.gold 120 213 186 1   # the rug
Motes $r[0] 20 $PAL.earth3; SaveBg $r 'inn'

# Item Shop - jars and bottles up both walls, a herb bundle, a hanging sign,
# a counter edge.
$r = New-Bg '#16182A' '#0D0F18'
foreach ($sy in 42, 78, 114, 150) {
  GFill $r[1] 70 $PAL.wat2 0 $sy 62 3; GFill $r[1] 70 $PAL.wat2 364 $sy 62 3          # the shelves
  foreach ($jx in 6, 20, 34, 48) { GFill $r[1] 80 $PAL.wat3 $jx ($sy - 11) 8 10; P $r[0] ($jx + 2) ($sy - 9) $PAL.glint }
  foreach ($jx in 370, 384, 398, 412) { GFill $r[1] 80 $PAL.veg2 $jx ($sy - 11) 8 10; P $r[0] ($jx + 2) ($sy - 9) $PAL.glint }
}
GFill $r[1] 90 $PAL.veg1 396 24 3 12; GEll $r[1] 90 $PAL.veg2 384 32 26 16      # the herb bundle
GFill $r[1] 100 $PAL.earth2 26 24 44 12; GFill $r[1] 80 $PAL.gold 30 28 36 4     # a hanging sign
GFill $r[1] 255 $PAL.earth1 0 196 426 44; GFill $r[1] 120 $PAL.earth2 0 196 426 3   # the counter's edge
Motes $r[0] 16 $PAL.wat3; SaveBg $r 'item_shop'

# Equip Shop - the forge: racked arms on the left wall, chains above, the
# forge mouth at the right, an anvil low centre.
$r = New-Bg '#1E2028' '#101218'
foreach ($cx in 40, 120, 306, 386) { GFill $r[1] 90 $PAL.stone3 $cx 24 2 22 }   # chains
GFill $r[1] 110 $PAL.earth1 4 44 26 124                                     # the rack
foreach ($hy in 52, 74, 96, 118, 140) { GFill $r[1] 120 $PAL.earth3 8 $hy 6 2; GFill $r[1] 120 $PAL.clsKnight 14 $hy 12 2 }   # hafts + blades
GFill $r[1] 140 $PAL.stone2 386 98 36 84; GFill $r[1] 160 $PAL.night1 392 114 24 42   # the forge
GEll $r[1] 60 $PAL.danger 395 128 18 26; GEll $r[1] 45 $PAL.gold 399 136 10 12      # its glow
GFill $r[1] 255 $PAL.stone1 0 194 426 46; GFill $r[1] 60 $PAL.night2 0 210 426 1   # stone floor
GFill $r[1] 120 $PAL.stone3 176 200 74 14; GFill $r[1] 120 $PAL.stone2 168 196 90 6; GFill $r[1] 100 $PAL.stone4 196 184 30 12   # the anvil
Motes $r[0] 14 $PAL.stone4; SaveBg $r 'equip_shop'

# Training Hall - the dojo: a slatted paper screen, lanterns in the corners,
# crossed staves high left, a wall target at the right, a mat floor.
$r = New-Bg '#241618' '#140D0F'
GFill $r[1] 22 $PAL.white0 0 24 426 92                                      # the paper glow
foreach ($sx in 0, 24, 48, 72, 96, 120, 306, 330, 354, 378, 402) { GFill $r[1] 40 $PAL.earth4 $sx 24 2 92 }   # slats, clear of the middle
GEll $r[1] 40 $PAL.gold 28 24 44 26; GEll $r[1] 40 $PAL.gold 354 24 44 26     # corner lanterns
$r[1].TranslateTransform(30, 70); $r[1].RotateTransform(35)
GFill $r[1] 70 $PAL.earth3 -34 -2 68 4; $r[1].RotateTransform(-70); GFill $r[1] 70 $PAL.earth3 -34 -2 68 4
$r[1].ResetTransform()                                                      # the crossed staves
GEll $r[1] 90 $PAL.maroon 382 40 36 36; GEll $r[1] 100 $PAL.danger 391 49 18 18; GEll $r[1] 120 $PAL.gold 397 55 6 6   # the target
GFill $r[1] 255 $PAL.earth1 0 194 426 46; GFill $r[1] 60 $PAL.earth3 0 194 426 2   # the mat
foreach ($mx in 60, 180, 300) { GFill $r[1] 40 $PAL.night2 $mx 196 2 44 }
Motes $r[0] 14 $PAL.maroon; SaveBg $r 'training_hall'

# Scoreboard - the hall of honour: fluted pillars with banners at the
# margins, a crystal glow above, a flagstone floor.
$r = New-Bg '#1A1626' '#100C18'
GEll $r[1] 40 $PAL.violet 150 -30 126 90; GEll $r[1] 30 $PAL.cyan 190 -10 46 50   # the glow
foreach ($px in 14, 386) {
  GFill $r[1] 110 $PAL.stone3 $px 26 24 170; GFill $r[1] 120 $PAL.stone4 ($px - 4) 24 32 6; GFill $r[1] 120 $PAL.stone4 ($px - 4) 190 32 8   # pillar + caps
  GFill $r[1] 40 $PAL.night2 ($px + 6) 30 2 160; GFill $r[1] 40 $PAL.night2 ($px + 16) 30 2 160                                             # flutes
  GFill $r[1] 90 $PAL.gold ($px + 4) 42 16 56; GEll $r[1] 80 $PAL.violet ($px + 8) 58 8 8                                                    # a banner + its emblem
}
GFill $r[1] 255 $PAL.stone1 0 196 426 44                                    # flagstones
foreach ($fy in 204, 214, 224) { GFill $r[1] 60 $PAL.night2 0 $fy 426 1 }
foreach ($fx in 50, 150, 250, 350) { GFill $r[1] 50 $PAL.night2 $fx 196 1 8; GFill $r[1] 50 $PAL.night2 ($fx + 50) 204 1 10 }
Motes $r[0] 18 $PAL.violet; SaveBg $r 'scoreboard'

# Guild - the lodge: a beam ceiling, a pinned wall map left, notices right,
# a hearth low right, a plank floor with a rug.
$r = New-Bg '#16201A' '#0D140E'
GFill $r[1] 100 $PAL.earth1 0 24 426 4; GFill $r[1] 80 $PAL.earth1 4 24 6 160; GFill $r[1] 80 $PAL.earth1 416 24 6 160   # beams + posts
GFill $r[1] 60 $PAL.earth4 12 30 70 56; GFill $r[1] 50 $PAL.veg2 20 40 30 2; GFill $r[1] 50 $PAL.veg2 30 56 40 2; GFill $r[1] 50 $PAL.wat2 18 70 54 2   # the wall map
foreach ($pin in @(@(28, 38), @(58, 54), @(44, 72))) { GEll $r[1] 70 $PAL.danger $pin[0] $pin[1] 4 4 }
foreach ($np in @(@(346, 30), @(378, 36), @(356, 62), @(388, 68))) { GFill $r[1] 70 $PAL.white0 $np[0] $np[1] 22 16; P $r[0] ($np[0] + 11) $np[1] $PAL.gold }   # notices
GFill $r[1] 120 $PAL.stone2 372 128 48 62; GFill $r[1] 150 $PAL.night2 380 140 32 42   # the hearth
GEll $r[1] 80 $PAL.danger 386 154 20 22; GEll $r[1] 60 $PAL.gold 390 160 12 12
GFill $r[1] 255 $PAL.earth1 0 196 426 44; GFill $r[1] 80 $PAL.earth2 0 196 426 2   # plank floor
GFill $r[1] 90 $PAL.veg1 140 200 146 20; GFill $r[1] 60 $PAL.gold 140 200 146 1     # the rug
Motes $r[0] 16 $PAL.veg3; SaveBg $r 'guild'

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

# Defining exterior tiles (grass/path/building); water/door/flowers keep the
# base and fall back in-engine. Since M128 the trees and the ground are hand-
# placed per town in the M128 section (the tinted copies were removed). Plus
# all six service interiors.
$townTiles = 'grass', 'path', 'building'   # M128: tree and ground are authored per town below
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

# ============================ M97 the Hooded Goose ============================
# The story's narrator: a hooded stranger whose disguise is one goose wide.
# Two sprites, appended after every earlier section and using no speckle RNG,
# so all prior files stay byte-identical.
Write-Output 'Generating M97 hooded goose...'

# Overworld NPC (12x12, the finale's roadside stranger). Same silhouette
# family as the market dealer, but the hood shadow holds a white head and an
# unmistakable gold bill.
$b = New-Img 12 12
FR $b 4 1 4 3 $PAL.night3; FR $b 4 3 4 1 $PAL.night1              # hood + shadow
FR $b 5 2 2 2 $PAL.clsCleric                                       # white head in the hood
P $b 7 3 $PAL.gold; P $b 8 3 $PAL.gold                             # the bill pokes out
FR $b 3 4 6 5 $PAL.night3                                          # cloak
FR $b 3 4 6 1 $PAL.earth4                                          # weathered trim
P $b 8 8 $PAL.clsCleric                                            # a tail feather escapes
FR $b 4 9 2 2 $PAL.gold; FR $b 6 9 2 2 $PAL.gold                  # webbed feet, plainly
Outline $b; SaveImg $b 'actors/hooded_goose_npc.png'

# Stage actor (18x26, the cutscene's center): taller hood, draped cloak to the
# floor, the same honest bill and feet — mystery from the knees up only.
$b = New-Img 18 26
FR $b 6 1 6 6 $PAL.night3; P $b 8 0 $PAL.night3                    # peaked hood
FR $b 6 6 6 1 $PAL.night1                                          # hood shadow
FR $b 7 3 3 3 $PAL.clsCleric                                       # white head
P $b 10 4 $PAL.gold; P $b 11 4 $PAL.gold; P $b 12 4 $PAL.gold      # the bill, prominent
P $b 8 4 $PAL.night1                                               # one dark eye
FR $b 4 7 10 16 $PAL.night3                                        # cloak
FR $b 4 7 10 1 $PAL.earth4                                         # trim
FR $b 4 22 10 1 $PAL.night1                                        # hem shadow
FR $b 3 12 1 6 $PAL.night3; FR $b 14 12 1 6 $PAL.night3            # drape
FR $b 14 17 3 2 $PAL.clsCleric; P $b 16 16 $PAL.clsCleric          # tail feathers, escaping
FR $b 5 23 3 2 $PAL.gold; FR $b 10 23 3 2 $PAL.gold                # webbed feet
Outline $b; SaveImg $b 'actors/hooded_goose_stage.png'

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

# --- M81: gear icons (10x10 pixel grids) -----------------------------------
#
# One icon per gear category (`iconCategory` in data/items.json; the exact
# vocabulary is content::kIconCategoryIds and the presentation lint holds the
# two in lockstep). Drawn with the M73 grid idiom: explicit ASCII rows, the
# shared $GRIDC palette key, hand-placed pixels only.
#
# DETERMINISM: like the enemy section above, this section calls NO random
# helper, so it cannot shift any other generated file's bytes. (The Goosy
# Gauntlet section appended after it reseeds $script:rng before its own
# Speckle calls, so these icons stay byte-identical too.)
#
# 10x10 is the size the UI actually renders: menu rows are 14px tall at font
# 10 (equip shop, armory ghost) and the party panel's gear lines sit on a
# 10px pitch, so 10 is the largest square that fits every site at 1x. No
# Outline pass — these sit on dark Inset list panels where the light ramps
# carry the shape.
Write-Output 'Generating gear icons (M81 pixel grids)...'

function Save-IconGrid([string]$name, [string[]]$rows) {
  $b = Draw-Grid $rows
  if ($b.Width -ne 10 -or $b.Height -ne 10) {
    throw "Save-IconGrid: icon '$name' is $($b.Width)x$($b.Height); must be 10x10."
  }
  SaveImg $b "ui/icons/$name.png"
}

Save-IconGrid 'sword' @(     # diagonal blade, gold cross-guard, dark grip
  '.........W'
  '.......LS.'
  '......LS..'
  '.....LS...'
  '....LS....'
  '...LS.....'
  '..YYY.....'
  '..s.......'
  '.s........'
  'Y.........'
)

Save-IconGrid 'axe' @(       # crescent head with a flat right cutting edge
  '......rrS.'
  '.....rrrS.'
  '....dsrrS.'
  '.....rrrS.'
  '......rrS.'
  '....ds....'
  '....ds....'
  '....ds....'
  '....ds....'
  '..........'
)

Save-IconGrid 'dagger' @(    # short blade, wide guard, round pommel
  '..........'
  '......LS..'
  '.....LS...'
  '....LS....'
  '...YY.....'
  '..s.......'
  '.sY.......'
  '..........'
  '..........'
  '..........'
)

Save-IconGrid 'bow' @(       # left-bulging stave, straight string
  '.....dd...'
  '...dd.S...'
  '..d...S...'
  '.d....S...'
  '.d....S...'
  '.d....S...'
  '.d....S...'
  '..d...S...'
  '...dd.S...'
  '.....dd...'
)

Save-IconGrid 'staff' @(     # crystal-topped rod
  '....CG....'
  '...CCCG...'
  '....CC....'
  '....ss....'
  '....ds....'
  '....ds....'
  '....ds....'
  '....ds....'
  '....ss....'
  '..........'
)

Save-IconGrid 'mace' @(      # studded stone head on a straight handle
  '...www....'
  '..wrKrw...'
  '..wrrrw...'
  '...www....'
  '....ss....'
  '....ss....'
  '....ss....'
  '....ss....'
  '....ss....'
  '..........'
)

Save-IconGrid 'spear' @(     # long 2px shaft, bright steel point
  '........LS'
  '.......LS.'
  '......ss..'
  '.....ss...'
  '....ss....'
  '...ss.....'
  '..ss......'
  '.ss.......'
  'ss........'
  '..........'
)

Save-IconGrid 'shield' @(    # heater: bright rim, gold boss
  '.wwwwwww..'
  '.weeeeew..'
  '.weeYeew..'
  '.weeeeew..'
  '..weeew...'
  '..weeew...'
  '...wew....'
  '....w.....'
  '..........'
  '..........'
)

Save-IconGrid 'armor' @(     # cuirass: shoulders, rimmed torso, waist taper
  '..........'
  '.rr....rr.'
  '.rrreerrr.'
  '..reeeer..'
  '..reeeer..'
  '..rreerr..'
  '...reer...'
  '...rrrr...'
  '..........'
  '..........'
)

Save-IconGrid 'accessory' @( # gold ring, cyan gem
  '..........'
  '..........'
  '....CC....'
  '...YCCY...'
  '..Y....Y..'
  '..Y....Y..'
  '...Y..Y...'
  '....YY....'
  '..........'
  '..........'
)

Save-IconGrid 'relic' @(     # violet void-diamond with a glint core
  '..........'
  '....m.....'
  '...mBm....'
  '..mBVBm...'
  '.mBVGVBm..'
  '..mBVBm...'
  '...mBm....'
  '....m.....'
  '..........'
  '..........'
)


# ================= Rebrand follow-up + the Goosy Gauntlet's own art =========
# Owner direction 2026-08-17 (the M104/M106/M108 manual-pass fixes): the goose
# emblem that fronts the title screen and the exe icon, the Goosy Gauntlet's
# own tiles and its eleven battle sprites, and the seven reel icons. Appended
# after every earlier section; the grid sprites place every pixel by hand, and
# $script:rng is reseeded below before this section's only Speckle calls, so
# every previously shipped file stays byte-identical.
$script:rng = 20260817
Write-Output 'Generating rebrand + Goosy Gauntlet art...'

# --- Goose emblem (32x32): the title-screen mark and the .ico source. A
# --- proud right-facing goose riding still water; one cyan glint keeps the
# --- crystal-as-light thread. Silhouette-first so it survives 16px.
$b = Draw-Grid @(
  '................................'
  '...............WWWWWWWW.........'
  '..............WWWWWWWWWW........'
  '..............WWWWWWKWWW........'
  '..............WWWWWWWWWWYYYYYYY.'
  '..............WWWWWWWWWWYYYYY...'
  '..............WWWWWWWWW.........'
  '..............WWWWWW............'
  '.............WWWWWW.............'
  '.............WWWWWS.............'
  '.............WWWWSS.............'
  '............WWWWSS..............'
  '............WWWWSS..............'
  '............WWWWSS..............'
  '......WWWWWWWWWWSS..............'
  '.....WWWWWWWWWWWWSSS......v.....'
  '....WWWWWWWWWWWWWWSSS.....c.....'
  '...WWWWWWWWWWWWWWWSSSS....c.....'
  '...WWWWWWWWWWWWWWWSSSSS..vc.....'
  '..WWWWWWWWWWWWWWWWSSSSSS..c.....'
  '..WWWWWWWWWWWWWWWWSSSSSSS.c.....'
  '.WWWWWWWWWWWWWWWWWSSSSSSS.c.....'
  '.ZWWWWWWWWWWWWWWSSSSSSSS..c.....'
  '..ZZZSSSSSSSSSSSSSSSZZ....c.....'
  '..iiiiiiiiiiiiiiiiiiiiiiiiciii..'
  '..iioiiioiiiioiiiiioiiiioiiiii..'
  '...iiiiiiiCiiiiiiiiiiiiiiiiii...'
  '....uuuuuuuuuuuuuuuuuuuuuuuu....'
  '......uuuuuuuuuuuuuuuuuu........'
  '................................'
  '................................'
  '................................'
)
Outline $b; SaveImg $b 'ui/emblem_goose.png'

# --- Goosy Gauntlet tiles (16x16, opaque, no outer outline). Identity per
# --- art_bible SS8b: a FLOODED PEN - still dark water underfoot, woven reed
# --- palisade walls, parted-reed doorways, and a nest shrine accent. Shape
# --- language (thin verticals + horizontal water banding) separates it from
# --- the Hollow Forest's trunk masses in grayscale.
$b = New-Img 16 16; FR $b 0 0 16 16 $PAL.wat0
Speckle $b 0 0 16 16 $PAL.veg0 0.20; Speckle $b 0 0 16 16 $PAL.wat1 0.16
FR $b 2 5 5 1 $PAL.wat2; FR $b 9 11 5 1 $PAL.wat2
P $b 12 3 $PAL.wat3; P $b 5 13 $PAL.wat3
P $b 7 8 $PAL.white1
SaveImg $b 'environments/goosy_floor.png'

$b = New-Img 16 16; FR $b 0 0 16 16 $PAL.veg0
foreach ($x in 0, 3, 6, 9, 12, 15) { FR $b $x 0 2 16 $PAL.veg1; FR $b $x 0 1 16 $PAL.veg2 }
FR $b 0 3 16 2 $PAL.earth2; FR $b 0 4 16 1 $PAL.earth1
FR $b 0 10 16 2 $PAL.earth2; FR $b 0 11 16 1 $PAL.earth1
Speckle $b 0 0 16 16 $PAL.veg2 0.05
SaveImg $b 'environments/goosy_wall.png'

$b = New-Img 16 16; FR $b 0 0 16 16 $PAL.veg0
FR $b 0 0 3 16 $PAL.veg1; FR $b 13 0 3 16 $PAL.veg1
FR $b 2 0 1 16 $PAL.veg2; FR $b 13 0 1 16 $PAL.veg2
FR $b 4 0 8 16 $PAL.night1
for ($j = 0; $j -lt 4; $j++) { P $b (4+$j) $j $PAL.veg2; P $b (11-$j) $j $PAL.veg2 }
FR $b 4 13 8 2 $PAL.wat1; FR $b 4 13 8 1 $PAL.wat2
SaveImg $b 'environments/goosy_door.png'

$b = New-Img 16 16; FR $b 0 0 16 16 $PAL.wat0
Speckle $b 0 0 16 16 $PAL.veg0 0.20
FR $b 3 6 10 7 $PAL.earth2
FR $b 4 7 8 5 $PAL.earth1
FR $b 3 6 10 1 $PAL.earth3
P $b 4 8 $PAL.earth3; P $b 11 10 $PAL.earth3
FR $b 6 8 2 3 $PAL.white1; FR $b 9 9 2 2 $PAL.white1
P $b 6 8 $PAL.white2; P $b 9 9 $PAL.white2
P $b 7 9 $PAL.cyan
FR $b 1 12 2 3 $PAL.veg1; P $b 1 11 $PAL.veg2
SaveImg $b 'environments/goosy_accent.png'

# --- The Goosy Gauntlet flock (24x24 normals/elites). A DIFFERENT flock from
# --- the five Evil Geese: pond fowl. Same family language (white bodies, gold
# --- feet), identity carried above the neck and at the wing, never by hue.

Save-EnemyGrid 'pond_drake' @(       # chunky drake: shovel bill, tail curl, speculum
  '........................'
  '........................'
  '........................'
  '.............ooooo......'
  '............opooooo.....'
  '............opoKooo.....'
  '............ooooooYYYYYY'
  '............ooooo.dddddd'
  '............ooooo.......'
  '..ii........WWWWW.......'
  '.i..i......WWWWWW.......'
  '.i.iWWWWWWWWWWWWWW......'
  '.iiWWWWWWWWWWWWWWWW.....'
  '..WWWppooWWWWWWWWWW.....'
  '.WWpooooiWWWWWWWWWWW....'
  '.WWpooooiWWWWWWWWWS.....'
  '.WWooooiiWWWWWWWWSS.....'
  '.WWWooiiWWWWWWWWWS......'
  '.WWWWWWWWWWWWWWWSS......'
  '..WSSSSSSSSSSSWWS.......'
  '..SSSSSSSSSSSSSS........'
  '...ZZZZZZZZZZZZ.........'
  '......YY...YY...........'
  '......YY...YY...........'
)

Save-EnemyGrid 'reed_honker' @(      # periscope neck above a reed blind
  '........................'
  '............WWWWW.......'
  '............WWWKWYYY....'
  '............WWWWW.......'
  '.............WW.........'
  '.............WW.........'
  '.............WW.........'
  '.............WW.aa......'
  '......aa.....WW.aa......'
  '......aa.....WW.cc......'
  '......cc.....WW.cc......'
  '......cc..cc.WW.cc.cc...'
  '..cc..cc..cc.WWScc.cc...'
  '..cc..cc..cc.cc.cc.cc...'
  '..cc..xc..cc.cc.cc.xc...'
  '..xc..xc..xc.cc.cc.xc...'
  '..xc..xx..xc.xc.xc.xx...'
  '..xc..xx..xc.xc.xc.xx...'
  '..xx..xx..xx.xc.xx.xx...'
  '..xx..xz..xx.xx.xx.xz...'
  '..xz..zz..xz.xx.zz.zz...'
  '..zz..zz..zz.zz.zz.zz...'
  '..zz..zz..zz.zz.zz.zz...'
  '........................'
)

Save-EnemyGrid 'mallard_marauder' @( # raider: red bandana, comically big oar
  '........................'
  '..ff....................'
  '..fff...................'
  '..dfff..................'
  '...dff..................'
  '....dd......WWWWW.......'
  '.....dd.....DDDDW.......'
  '......dd...DWWKWWYYYY...'
  '.......dd...WWWWWYYYY...'
  '........dd..WWWWW.......'
  '.........dd.WWWWDD......'
  '..........ddWWWW.D......'
  '...WWWWW..WdWWWWW.......'
  '..WWWWWWWWWdWWWWS.......'
  '.WWWWWWWWWWWWWWWS.......'
  '.WWWWWWWWWWWWWWSS.......'
  '.WWWWWWWWWWWWWWS........'
  '.WWWWWWWWWWWWWSS........'
  '.WWWSSSSSSSWWWS.........'
  '.SWWSSSSSSSSWWS.........'
  '.SSSSSSSSSSSSS..........'
  '..ZZZZZZZZZZZ...........'
  '.....YY...YY............'
  '.....YY...YY............'
)

Save-EnemyGrid 'downfeather_witch' @( # the Hag: ragged shawl, crooked frost wand
  '........................'
  '........................'
  '..........mm............'
  '.........mmmm...........'
  '..........mmmm..........'
  '..........mmmmm.........'
  '..........mWWmmm........'
  '..........mWKWmmYYY.....'
  '..........mmWWmYYY.W....'
  '.........mmmWWm...pp....'
  '.........mmmWWmm.s......'
  '........mmmWWWmm.s......'
  '........mmWWWWm.s.......'
  '..WWWWmmmWWWWWm.s.......'
  '.WWWWWmmWWWWWWWWs.......'
  '.WWWWWWWWWWWWWWWS.......'
  '.WWWWWWWWWWWWWWS........'
  '.WWWWWWWWWWWWWSS........'
  '.WWWSSSSSSSWWWS.........'
  '.SWWSSSSSSSSWWS.........'
  '.SSSSSSSSSSSSS..........'
  '..ZZZZZZZZZZZ...........'
  '.....YY...YY............'
  '.....YY...YY............'
)

Save-EnemyGrid 'puddle_imp' @(       # a splash that got opinions
  '........................'
  '........................'
  '........................'
  '........................'
  '........................'
  '........................'
  '........................'
  '........................'
  '........................'
  '.........p....p.........'
  '....p....p...pp....p....'
  '....pp...pp..pp...pp....'
  '.....ppooppoopppoopp....'
  '.....ooooooooooooooo....'
  '....oooKKoooooKKooooo...'
  '....ooooooooooooooooo...'
  '...oioooooWWooooooooi...'
  '...iioooooooooooooiii...'
  '...iiioooooooooooiiii...'
  '....iiiiiiiiiiiiiiii....'
  '.....iiiuuuuuuuuiii.....'
  '......uuuuuuuuuuuu......'
  '....u..uu..uu..uu..u....'
  '........................'
)

Save-EnemyGrid 'gander_grenadier' @( # elite: bandolier and a bomb of pure hubris
  '........................'
  '........................'
  '........................'
  '...........WWWWW........'
  '..........WWWWWWW.......'
  '..........WWWKWWYYYY....'
  '..........WWWWWWYYYY....'
  '..........WWWWWW........'
  '..........WWWWS.........'
  '..........WWWS..........'
  '..WWWWW...WWWS.G........'
  '.WWWWWWWWWWWWWW.Y.......'
  '.WWWWaaWWWWWWWWWs.......'
  'WWWWWWaaWWWW22222.......'
  'WWWWWWWaaWW2233322......'
  'WWWWWWWWaaW2233322......'
  'WWWWWWWWWaW2223222......'
  'WWWWSSSSSaSS22222S......'
  'WWWSSSSSSSSSSWWSS.......'
  'SWWSSSSSSSSSSSWS........'
  'SSSSSSSSSSSSSSSS........'
  '.ZZZZZZZZZZZZZZ.........'
  '....YY....YY............'
  '....YY....YY............'
)

Save-EnemyGrid 'cob_knight' @(       # elite: a swan in a helm; shield on the wing
  '...........LLLL.........'
  '..........LLLLLL........'
  '..........LWWKLWYYY.....'
  '..........LLWWWW........'
  '...........WWW..........'
  '...........WW...........'
  '..........WW............'
  '.........WW.............'
  '.........WW.............'
  '..........WW............'
  '..........WWW...........'
  '...........WWWW.........'
  '..WWWWWW..LLLLL.........'
  '.WWWWWWWWWLqwwqL........'
  '.WWWWWWWWWLqwYwqL.......'
  '.WWWWWWWWWLqwwqL........'
  '.WWWWWWWWWLLqqL.........'
  '.WWWWWWWWWSLLL..........'
  '.WWWSSSSSSSWWWS.........'
  '.SWWSSSSSSSSWWS.........'
  '.SSSSSSSSSSSSS..........'
  '..ZZZZZZZZZZZ...........'
  '.....11...11............'
  '.....11...11............'
)

Save-EnemyGrid 'migration_herald' @( # elite: the war-horn and the V-banner
  '.....................Y..'
  '....................YY..'
  '...................dYY..'
  '............WWWW..ddY...'
  '...........WWWWWWYdd....'
  '...........WWWKWWd......'
  '...........WWWWWW.......'
  '...........WWWWS........'
  '....xx.....WWWS.........'
  '....xxx....WWWS.........'
  '....xWx....WWWS.........'
  '....xxx..WWWWWW.........'
  '....xx.WWWWWWWWW........'
  '....s.WWWWWWWWWWW.......'
  '....sWWWWWWWWWWWWW......'
  '....sWWWWWWWWWWWWS......'
  '....sWWWWWWWWWWWS.......'
  '....sWWWWWWWWWWSS.......'
  '....sWWSSSSSSSWWS.......'
  '....sSSSSSSSSSSSS.......'
  '.....SSSSSSSSSSS........'
  '......ZZZZZZZZZ.........'
  '.......YY...YY..........'
  '.......YY...YY..........'
)

# --- The three Goosy bosses (36x36, art_bible SS5b). Each is 2-3 readable
# --- masses, one motif from its bosses.json entry, one asymmetry, and a
# --- slit/void face - never a crowned toy.

Save-EnemyGrid 'boss_the_gray_gander' @(  # brute: unfolded to its full height
  '....................................'
  '..........................eee.......'
  '.........................eeeee......'
  '........................eeeeeee.....'
  '........................eeeeqqe.....'
  '........................eeeqKKeddd..'
  '........................eeeeeeeddddd'
  '.........................eeeeeddddd.'
  '..........................eeeee.....'
  '.........................eeeeee.....'
  '........................eeeeee..ww..'
  '.......................eeeeee..www..'
  '.......................eeeee..wwww..'
  '......................eeeeee.wwww...'
  '......................eeeee.wwww....'
  '....ee.......wwwwwwwwwwwwww.........'
  '...eee.....wwwwwwwwwwwwwwww.........'
  '...eeee...wwwwwwwwwwwwwwwww.........'
  '..eeeee..wwwwwwwwwwwwwwwwwww........'
  '..eee.e.wwwwwwwwwwwwwwwwwwww........'
  '..ee..ewwwwwwwwwwwwwwwwwwwww........'
  '..e..wwwwwwwwwwwwwwwwwwwwwww........'
  '....wwwwwwwwwwwwwwwwwwwwwwww........'
  '....wwwwwwwwwwwwwwwwwwwwwww.........'
  '.....wwwwwwwwwwwwwwwwwwwwww.........'
  '.....ZwwwwwwwwwwwwwwwwwwwwZ.........'
  '.....ZZwwwwwwwwwwwwwwwwwZZ..........'
  '......ZZZZZZZZZZZZZZZZZZ............'
  '.......ZSSSSSSSSSSSSSSZ.............'
  '.........dd........dd...............'
  '.........dd........dd...............'
  '........ddd.......ddd...............'
  '.......ddddd.....ddddd..............'
  '....................................'
  '....................................'
  '....................................'
)

Save-EnemyGrid 'boss_mother_of_ponds' @(  # sorcerer: veil, nest, orbiting cold
  '....................................'
  '....................................'
  '..................WW................'
  '.................WWWW...............'
  '................WWWWWW.....pp.......'
  '................WWWWWW....poop......'
  '...pp...........WSSSSWW...poop......'
  '..poop..........WSbbbSW....pp.......'
  '..poop..........WSbbbbSW............'
  '...pp...........WSbbbbYYY...........'
  '................WWSbbSWW............'
  '................WWSSSSWW............'
  '.................WWSSWW.............'
  '.................WWSSW..............'
  '..................WSSW..............'
  '..................WSSW..............'
  '..ppp.............WSSSW.............'
  '..pWp............WWSSSW.............'
  '..ppp...........WWSSSSWW............'
  '................WWSSSSWWWW..........'
  '...............WWSSSSSWWWWWW........'
  '...............WSSSSSSSWWWWWWW......'
  '..............WWSSSSSSSSWWWWWWW.....'
  '..............WSSSSSSSSSS.WWWWWW....'
  '..............WSSSSSSSSSS..WWWW.....'
  '..............SSSSSSSSSS............'
  '........aassWWSSSSSSSSWWssaa........'
  '.......asddssaddssaddssaddsa........'
  '......assddssddssddssddssddsa.......'
  '......asddssddssddssddssddssa.......'
  '.......aasssssssssssssssssaa........'
  '.........dddddddddddddddd...........'
  '....................................'
  '....................................'
  '....................................'
  '....................................'
)

Save-EnemyGrid 'boss_the_pondlord' @(     # commander: reed diadem, cattail standard
  '....................................'
  '............cv..c.............aa....'
  '............cc..c..c..........aa....'
  '...........cWWWWWWWc..........aa....'
  '...........WWWWWWWWW..........aa....'
  '...........WWWWWWWWW..........aa....'
  '...........WSKKSWWWWYYY.......ss....'
  '...........WWWWWWWWWYY........ss....'
  '............WWWWWW............ss....'
  '.............WWWW.......xxxxxxss....'
  '.............WWWW.......xWWxxxss....'
  '.............WWWW.......xxWWxxss....'
  '.............WWWW.......xxxWWxss....'
  '............WWWWWW......xxxxxxss....'
  '..........ZWWWWWWWWZ......xxx.ss....'
  '........ZZWWWWWWWWWWZZ.....xx.ss....'
  '.......ZWWWWWWWWWWWWWWZ......Sss....'
  '.......WWWWWWWWWWWWWWWWSSSSSSSss....'
  '......WWWWWSWWWWWWSWWWWW......ss....'
  '......WWWWWSWWWWWWSWWWWW......ss....'
  '......WWWWWSWWWWWWSWWWWW......ss....'
  '......WWWWWSWWWWWWSWWWWW......ss....'
  '......WWWWWSWWWWWWSWWWWW......ss....'
  '.......WWWWSWWWWWWSWWWW.......ss....'
  '.......WWWWSWWWWWWSWWWW.......ss....'
  '.......WWWSSWWWWWWSSWWW.......ss....'
  '........WWSSWWWWWWSSWW........ss....'
  '........ZZSSSWWWWSSSZZ........ss....'
  '.........ZZZZZZZZZZZZ.........ss....'
  '..........dd......dd..........ss....'
  '..........dd......dd..........ss....'
  '........dddd......dddd........dd....'
  '......xxxxxxxxxxxxxxxxxx......dd....'
  '....................................'
  '....................................'
  '....................................'
)

# --- Reel icons (12x12, no outline - they sit inside the dark outcome panel
# --- like the M81 gear icons). One per gamble::ReelSymbol, in enum order.
function Save-ReelGrid([string]$name, [string[]]$rows) {
  $b = Draw-Grid $rows
  if ($b.Width -ne 12 -or $b.Height -ne 12) {
    throw "Save-ReelGrid: icon '$name' is $($b.Width)x$($b.Height); must be 12x12."
  }
  SaveImg $b "ui/icons/reel_$name.png"
}

Save-ReelGrid 'tax_papers' @(  # the stack, the stamp
  '............'
  '...SSSSSS...'
  '..SSSSSSS...'
  '..SWWWWWW...'
  '..SW1111W...'
  '..SWWWWWW...'
  '..SW1111W...'
  '..SWWWDDW...'
  '..SWWWDDW...'
  '..WWWWWWW...'
  '............'
  '............'
)

Save-ReelGrid 'goose_head' @(  # the bird itself
  '............'
  '...WWWW.....'
  '..WWWWWW....'
  '..WWKWWWYYY.'
  '..WWWWWWYYY.'
  '..WWWWWW....'
  '...WWWW.....'
  '...WWW......'
  '...WWWS.....'
  '...WWWS.....'
  '..SWWWWS....'
  '............'
)

Save-ReelGrid 'spoon' @(       # the P-Spoon; mind the spoon
  '............'
  '....LLLL....'
  '...LSSSSL...'
  '...LSWWSL...'
  '...LSVWSL...'
  '...LSSSSL...'
  '....LLLL....'
  '.....LL.....'
  '.....LL.....'
  '.....LL.....'
  '....LLLL....'
  '............'
)

Save-ReelGrid 'crown' @(       # the Dragon Crown
  '............'
  '............'
  '..Y...Y...Y.'
  '..YY.YYY.YY.'
  '..YYYYYYYYY.'
  '..YGYYCYYGY.'
  '..YYYYYYYYY.'
  '...YYYYYYY..'
  '............'
  '............'
  '............'
  '............'
)

Save-ReelGrid 'red_x' @(       # the polite refusal
  '............'
  '..DD.....DD.'
  '...DD...DD..'
  '....DD.DD...'
  '.....DDD....'
  '.....DDD....'
  '....DD.DD...'
  '...DD...DD..'
  '..DD.....DD.'
  '............'
  '............'
  '............'
)

Save-ReelGrid 'bald_head' @(   # the bald-red-beard gentleman
  '............'
  '....hhhh....'
  '...hhhhhh...'
  '...hghhgh...'
  '...hKhhKh...'
  '...hhhhhh...'
  '..OhhhhhhO..'
  '..OOhhhhOO..'
  '..OOOOOOOO..'
  '...OOOOOO...'
  '....OOOO....'
  '............'
)

Save-ReelGrid 'seven' @(       # the one everyone is here for
  '............'
  '..YYYYYYYY..'
  '..YGGGGGYY..'
  '........YY..'
  '.......YY...'
  '......YY....'
  '.....YYY....'
  '.....YY.....'
  '....YYY.....'
  '....YY......'
  '............'
  '............'
)

# ============================================================================
# 2026-08-29 (owner request) - blackjack card faces. Six 18x24 hand-placed
# grids, RNG-free like every icon; the section reseed keeps the discipline.
# The dealer's hole card is the BACK; number ranks draw the blank FACE and
# the game letters the rank over it in the bitmap font; A/J/Q/K are full
# authored cards - the Jack a goose, the Queen a duck, the King a dark king
# (the game's own royalty gag, no real deck imitated). Corners stay
# transparent so the cards read rounded on any panel.
$script:rng = 20260829
Write-Output 'Generating blackjack cards...'

function Save-CardGrid([string]$name, [string[]]$rows) {
  $b = Draw-Grid $rows
  if ($b.Width -ne 18 -or $b.Height -ne 24) {
    throw "Save-CardGrid: card '$name' is $($b.Width)x$($b.Height); must be 18x24."
  }
  SaveImg $b "ui/cards/card_$name.png"
}

Save-CardGrid 'face' @(   # the blank card: cream face, shaded right/bottom edge
  '.KKKKKKKKKKKKKKKK.'
  'KWWWWWWWWWWWWWWWWK'
  'KWWWWWWWWWWWWWWWSK'
  'KWWWWWWWWWWWWWWWSK'
  'KWWWWWWWWWWWWWWWSK'
  'KWWWWWWWWWWWWWWWSK'
  'KWWWWWWWWWWWWWWWSK'
  'KWWWWWWWWWWWWWWWSK'
  'KWWWWWWWWWWWWWWWSK'
  'KWWWWWWWWWWWWWWWSK'
  'KWWWWWWWWWWWWWWWSK'
  'KWWWWWWWWWWWWWWWSK'
  'KWWWWWWWWWWWWWWWSK'
  'KWWWWWWWWWWWWWWWSK'
  'KWWWWWWWWWWWWWWWSK'
  'KWWWWWWWWWWWWWWWSK'
  'KWWWWWWWWWWWWWWWSK'
  'KWWWWWWWWWWWWWWWSK'
  'KWWWWWWWWWWWWWWWSK'
  'KWWWWWWWWWWWWWWWSK'
  'KWWWWWWWWWWWWWWWSK'
  'KWWWWWWWWWWWWWWWSK'
  'KSSSSSSSSSSSSSSSSK'
  '.KKKKKKKKKKKKKKKK.'
)

Save-CardGrid 'back' @(   # the hole card: night-blue weave, crystal pips
  '.KKKKKKKKKKKKKKKK.'
  'KnnnnnnnnnnnnnnnnK'
  'KnnCnnnCnnnCnnnCnK'
  'KnnnnnnnnnnnnnnnnK'
  'KCnnnCnnnCnnnCnnnK'
  'KnnnnnnnnnnnnnnnnK'
  'KnnCnnnCnnnCnnnCnK'
  'KnnnnnnnnnnnnnnnnK'
  'KCnnnCnnnCnnnCnnnK'
  'KnnnnnnnnnnnnnnnnK'
  'KnnCnnnCnnnCnnnCnK'
  'KnnnnnnnnnnnnnnnnK'
  'KCnnnCnnnCnnnCnnnK'
  'KnnnnnnnnnnnnnnnnK'
  'KnnCnnnCnnnCnnnCnK'
  'KnnnnnnnnnnnnnnnnK'
  'KCnnnCnnnCnnnCnnnK'
  'KnnnnnnnnnnnnnnnnK'
  'KnnCnnnCnnnCnnnCnK'
  'KnnnnnnnnnnnnnnnnK'
  'KCnnnCnnnCnnnCnnnK'
  'KnnnnnnnnnnnnnnnnK'
  'KnnnnnnnnnnnnnnnnK'
  '.KKKKKKKKKKKKKKKK.'
)

Save-CardGrid 'ace' @(    # A + the crystal shard (the deck's own suit)
  '.KKKKKKKKKKKKKKKK.'
  'KWWWWWWWWWWWWWWWWK'
  'KWWKWWWWWWWWWWWWSK'
  'KWKWKWWWWWWWWWWWSK'
  'KWKKKWWWWWWWWWWWSK'
  'KWKWKWWWWWWWWWWWSK'
  'KWKWKWWWWWWWWWWWSK'
  'KWWWWWWWWWWWWWWWSK'
  'KWWWWWWWWWWWWWWWSK'
  'KWWWWWWWCCWWWWWWSK'
  'KWWWWWWGCCCWWWWWSK'
  'KWWWWWGCCCCVWWWWSK'
  'KWWWWWGCCCCVWWWWSK'
  'KWWWWWCCCCCVWWWWSK'
  'KWWWWWCCCCVVWWWWSK'
  'KWWWWWWCCCVWWWWWSK'
  'KWWWWWWCCCVWWWWWSK'
  'KWWWWWWWCVWWWWWWSK'
  'KWWWWWWWCVWWWWWWSK'
  'KWWWWWWWWWWWWWWWSK'
  'KWWWWWWWWWWWWWWWSK'
  'KWWWWWWWWWWWWWWWSK'
  'KSSSSSSSSSSSSSSSSK'
  '.KKKKKKKKKKKKKKKK.'
)

Save-CardGrid 'jack' @(   # J - the goose, bill raised, unimpressed
  '.KKKKKKKKKKKKKKKK.'
  'KWWWWWWWWWWWWWWWWK'
  'KWWWKWWWWWWWWWWWSK'
  'KWWWKWWWWWWWWWWWSK'
  'KWWWKWWWWWWWWWWWSK'
  'KWKWKWWWWWWWWWWWSK'
  'KWWKWWWWWWWWWWWWSK'
  'KWWWWWWWWWWWWWWWSK'
  'KWWWWWWWWKKKKWWWSK'
  'KWWWWWWWKZZZZKWWSK'
  'KWWWWWWWKZKZZKYYSK'
  'KWWWWWWWKZZZZKYYSK'
  'KWWWWWWWKZZKWWWWSK'
  'KWWWWWWKZZKWWWWWSK'
  'KWWWWWKZZKWWWWWWSK'
  'KWWWWWKZZKWWWWWWSK'
  'KWWWWKZZZZKWWWWWSK'
  'KWWWKZZZZZZKWWWWSK'
  'KWWWKZZZZZZKWWWWSK'
  'KWWWKKKKKKKKWWWWSK'
  'KWWWWWWWWWWWWWWWSK'
  'KWWWWWWWWWWWWWWWSK'
  'KSSSSSSSSSSSSSSSSK'
  '.KKKKKKKKKKKKKKKK.'
)

Save-CardGrid 'queen' @(  # Q - the duck, ring-necked, mid-scheme
  '.KKKKKKKKKKKKKKKK.'
  'KWWWWWWWWWWWWWWWWK'
  'KWWKWWWWWWWWWWWWSK'
  'KWKWKWWWWWWWWWWWSK'
  'KWKWKWWWWWWWWWWWSK'
  'KWKWKWWWWWWWWWWWSK'
  'KWWKKWWWWWWWWWWWSK'
  'KWWWWWWWWWWWWWWWSK'
  'KWWWWWWWWKKKKWWWSK'
  'KWWWWWWWKvvvvKWWSK'
  'KWWWWWWWKvKvvKYYSK'
  'KWWWWWWWKvvvvKYYSK'
  'KWWWWWWWKZZKWWWWSK'
  'KWWWWWWKssKWWWWWSK'
  'KWWWWWKssKWWWWWWSK'
  'KWWWWWKssKWWWWWWSK'
  'KWWWWKssssKWWWWWSK'
  'KWWWKssssssKWWWWSK'
  'KWWWKssssssKWWWWSK'
  'KWWWKKKKKKKKWWWWSK'
  'KWWWWWWWWWWWWWWWSK'
  'KWWWWWWWWWWWWWWWSK'
  'KSSSSSSSSSSSSSSSSK'
  '.KKKKKKKKKKKKKKKK.'
)

Save-CardGrid 'king' @(   # K - the dark king, crowned, displeased
  '.KKKKKKKKKKKKKKKK.'
  'KWWWWWWWWWWWWWWWWK'
  'KWKWKWWWWWWWWWWWSK'
  'KWKKWWWWWWWWWWWWSK'
  'KWKKWWWWWWWWWWWWSK'
  'KWKWKWWWWWWWWWWWSK'
  'KWKWKWWWWWWWWWWWSK'
  'KWWWWWWWWWWWWWWWSK'
  'KWWWWWYWYWYWWWWWSK'
  'KWWWWWYYYYYWWWWWSK'
  'KWWWWKKKKKKKWWWWSK'
  'KWWWWKnnnnnKWWWWSK'
  'KWWWKnbbbbbnKWWWSK'
  'KWWWKnbDbDbnKWWWSK'
  'KWWWKnbbbbbnKWWWSK'
  'KWWWKnbbbbbnKWWWSK'
  'KWWKnnnnnnnnnKWWSK'
  'KWWKnnbnnnbnnKWWSK'
  'KWWKnnnnnnnnnKWWSK'
  'KWWKKKKKKKKKKKWWSK'
  'KWWWWWWWWWWWWWWWSK'
  'KWWWWWWWWWWWWWWWSK'
  'KSSSSSSSSSSSSSSSSK'
  '.KKKKKKKKKKKKKKKK.'
)


# ============================ M111 the Golden Goose ============================
# --- The patrol dispatcher's 10% prize: a goose reared and about to bolt,
# --- facing right like every foe. One raised wing with daylight between it
# --- and the neck (negative space, art_bible S5), a gold body on the reward
# --- ramp with warm earth shading below, a white sheen on the head, one dark
# --- eye, bill and feet in earth. A patrol foe, never a boss: the ordinary
# --- 24x24 canvas. Hand-placed, RNG-free, appended last so every earlier file
# --- stays byte-identical.
Save-EnemyGrid 'golden_goose' @(   # reared goose, wing up, about to bolt
  '........................'
  '.............KKKK.......'
  '............KYYYYK......'
  '...........KYYKWYYKK....'
  '...........KYYYYYYKffK..'
  '............KYYYYYKKfK..'
  '.............KYYYK..K...'
  '.............KYYYK......'
  '......KKK....KYYYK......'
  '.....KYYYKK..KYYYK......'
  '....KYYYYYYK.KYYYK......'
  '....KYYYYYYYKKYYYK......'
  '...KK.KKKKKYYYYYYYYK....'
  '..KYK..KYYYYYYYYYYYYK...'
  '.KYYKKYYYYYYYYYYYYYYYK..'
  '.KYYYYYYYYYYYYYYYYYYYK..'
  '.KfYYYYYYYYYYYYYYYYYK...'
  '..KffYYYYYYYYYYYYYYK....'
  '...KfffYYYYYYYYYYYK.....'
  '....KKffffffffffKK......'
  '......KKKKKKKKKK........'
  '........Kd...Kd.........'
  '........Kd...Kd.........'
  '.......KdddKKdddK.......'
)


# ================================ M112 the Mimic ================================
# --- The lying chest: a treasure chest whose lid has swung open into a MAW.
# --- Hinge at the back (left), the lid rising to the right so the mouth opens
# --- toward the party; a ring of white teeth on both rims, one red eye deep in
# --- the dark, a tongue of gold coins lolling out and two loose coins on the
# --- ground. Wood body on the earth ramp, gold bands with glints (S5b:
# --- lore-before-ornament - the chest reads as a chest first). 36x36, the
# --- boss canvas. Hand-placed, RNG-free, appended last.
Save-EnemyGrid 'boss_mimic' @(      # a chest open into a maw, coins for a tongue, the lid a brow
  '....................................'
  '....................................'
  '....................................'
  '....................................'
  '....................................'
  '....................................'
  '........ssssssssssssssssssss........'
  '.......sdddddddddddddddddddds.......'
  '......sddddddddddddddddddddddds.....'
  '......sddaaaaaaaaaaaaaaaaaaddds.....'
  '......sddaddddddddddddddddaddds.....'
  '......sddaddKKdddddddddKKdaddds.....'
  '......sddaddddddddddddddddaddds.....'
  '......sddaaaaaaaaaaaaaaaaaaddds.....'
  '......sdddddddddddddddddddddds......'
  '......ssssssssssssssssssssssss......'
  '......sKWKWKWKWKWKWKWKWKWKWKWs......'
  '......sKKKKKKKKKKKKKKKKKKKKKKs......'
  '......sKKKKKKKKKKKKKKKKKKKKKKs......'
  '......sKKKKKKKKKKllllllKKKKKKKs.....'
  '......sKKKKKKKKlllllllllKKKKKKKs....'
  '......sKWKWKWKlllYlllYllKWKWKWKs....'
  '......ssssssssslllllllllsssssss.....'
  '......sddddddddddllllllddddddds.....'
  '......sddddddddddddddddddddddds.....'
  '......sddaaaaaaaaaaaaaaaaaaddds.....'
  '......sddaddddddddddddddddaddds.....'
  '......sddaaaaaaaaaaaaaaaaaaddds.....'
  '......sdddddddddddddddddddddds......'
  '......ssssssssssssssssssssssss......'
  '........aaa..............aaa........'
  '.......aaaa..............aaaa.......'
  '.......aaaa..............aaaa.......'
  '......aaaaa..............aaaaa......'
  '....................................'
  '....................................'
)

# --- The closed treasure chest of the decision phase (three of them stand on
# --- the field before one is opened): domed lid, seam, two gold bands, a gold
# --- lock plate with a dark keyhole, feet. 24x24 on the ordinary footprint.
function Save-PropGrid([string]$name, [string[]]$rows) {
  $b = Draw-Grid $rows
  Outline $b
  SaveImg $b "props/$name.png"
}

Save-PropGrid 'chest_battle' @(   # closed chest, decision phase
  '........................'
  '........................'
  '........................'
  '........................'
  '....ssYYssssssssYYss....'
  '...sssYYssssssssYYsss...'
  '..ddddYYddddddddYYdddd..'
  '..ddddGYddddddddGYdddd..'
  '..ddddYYddYYYYddYYdddd..'
  '..ffffYYffYYYYffYYffff..'
  '..KKKKYYKKYKYYKKYYKKKK..'
  '..ffddYYddYKKYddYYdddd..'
  '..ffddYYddYYYYddYYdddd..'
  '..ffddYYddddddddYYdddd..'
  '..ffddYYddddddddYYdddd..'
  '..ffddYGddddddddYGdddd..'
  '..ffddYYddddddddYYdddd..'
  '..ffddYYddddddddYYdddd..'
  '..ffddYYddddddddYYdddd..'
  '..ffddYYddddddddYYdddd..'
  '..ffffYYffffffffYYffff..'
  '..ffffYYffffffffYYffff..'
  '...fff............fff...'
  '........................'
)


# ============================ M113 cutscene stages ============================
# --- Five full-screen (426x240) stages behind the cutscene actors, on the
# --- M27 background recipe (gradient + darkened bottom band + low-alpha
# --- motifs). Binding rules (art_bible S6): darker than the actors' ramps,
# --- no signal colours, and the ACTOR BAND (y 36..106) kept quiet - motifs
# --- sit above y 36 (sky, peaks, canopy, roof) and below y 106 (the floor),
# --- with only far, low-alpha silhouettes at the edges of the band. Own rng
# --- reseed so every earlier speckled file stays byte-identical.
$script:rng = 20260902
# (GPoly / Stars / FloorSpeckle moved up to the M27 block in M119: shared
# scene helpers. A function definition draws nothing, so no bytes moved.)

# Town panorama: night sky, three bands of mountains fading with distance,
# the keep and roofs on the near ridge at the edges, terraced paths, earth floor.
$r = New-Bg $PAL.night1 $PAL.night3
Stars $r[0] 44 $PAL.white0 30
GEll $r[1] 40 $PAL.white0 352 6 18 18                                     # a small moon, top right
GPoly $r[1] 70 $PAL.stone1 @(0,64, 40,38, 90,56, 140,34, 190,58, 240,40, 300,60, 350,36, 400,54, 426,44, 426,110, 0,110)   # far range
GPoly $r[1] 90 $PAL.night3 @(0,84, 60,66, 120,80, 200,62, 260,78, 330,60, 390,76, 426,68, 426,112, 0,112)                 # mid range
GFill $r[1] 110 $PAL.night2 0 96 426 14                                     # the near ridge line
GFill $r[1] 120 $PAL.stone2 8 72 12 26;  GFill $r[1] 120 $PAL.stone2 22 80 22 18;  GFill $r[1] 140 $PAL.stone3 6 70 16 3   # the keep, far left
GFill $r[1] 100 $PAL.earth2 386 84 18 14; GPoly $r[1] 110 $PAL.earth3 @(384,84, 395,74, 406,84)                            # a roof, far right
GFill $r[1] 100 $PAL.earth2 406 88 14 10; GPoly $r[1] 110 $PAL.earth3 @(404,88, 413,80, 422,88)
GFill $r[1] 255 $PAL.earth1 0 108 426 132                                   # the floor
GFill $r[1] 60 $PAL.earth2 0 118 426 3; GFill $r[1] 40 $PAL.earth2 0 134 426 2; GFill $r[1] 30 $PAL.earth2 0 152 426 2  # terraces
FloorSpeckle $r[0] 40 $PAL.earth2
SaveBg $r 'cutscene_panorama'

# Ruined Keep: broken parapets along the top, dark arches at the sides,
# a cracked slab floor with rubble.
$r = New-Bg $PAL.night1 $PAL.stone1
foreach ($px in 0, 32, 64, 96, 300, 332, 364, 396) { GFill $r[1] 120 $PAL.stone2 $px 0 20 22 }   # crenellations, gaps in the middle
GFill $r[1] 120 $PAL.stone2 0 22 426 8                                      # the parapet walk
GFill $r[1] 90 $PAL.stone1 0 30 426 4
GFill $r[1] 160 $PAL.void0 10 44 52 64; GEll $r[1] 160 $PAL.void0 10 30 52 40   # left arch (dark)
GFill $r[1] 90 $PAL.stone3 8 44 3 64;   GFill $r[1] 90 $PAL.stone3 61 44 3 64
GFill $r[1] 160 $PAL.void0 364 44 52 64; GEll $r[1] 160 $PAL.void0 364 30 52 40 # right arch
GFill $r[1] 90 $PAL.stone3 362 44 3 64; GFill $r[1] 90 $PAL.stone3 415 44 3 64
GFill $r[1] 255 $PAL.stone1 0 108 426 132                                   # slab floor
foreach ($sy in 124, 146, 170, 196) { GFill $r[1] 70 $PAL.night2 0 $sy 426 2 }
foreach ($sx in 70, 190, 330) { GFill $r[1] 60 $PAL.night2 $sx 108 2 132 }
GFill $r[1] 110 $PAL.stone2 40 112 14 6; GFill $r[1] 110 $PAL.stone2 372 116 18 5; GFill $r[1] 110 $PAL.stone2 200 128 10 4  # rubble
FloorSpeckle $r[0] 36 $PAL.stone2
SaveBg $r 'cutscene_keep'

# Crystal Mine: clusters hanging from the roof and rising at the sides,
# timber supports, rails across the floor.
$r = New-Bg $PAL.night1 $PAL.void0
foreach ($cx in 60, 150, 250, 340) { GPoly $r[1] 60 $PAL.wat3 @(($cx-10),0, ($cx+10),0, $cx,30); GPoly $r[1] 50 $PAL.cyan @(($cx-3),0, ($cx+3),0, $cx,22) }  # hanging clusters
GPoly $r[1] 60 $PAL.violet @(6,106, 22,70, 38,106); GPoly $r[1] 60 $PAL.wat3 @(30,106, 44,82, 58,106)        # left cluster, floor-rooted
GPoly $r[1] 60 $PAL.violet @(388,106, 404,68, 420,106); GPoly $r[1] 60 $PAL.wat3 @(366,106, 380,84, 394,106) # right cluster
GFill $r[1] 140 $PAL.earth1 66 16 8 92; GFill $r[1] 140 $PAL.earth1 352 16 8 92; GFill $r[1] 140 $PAL.earth1 60 14 306 6  # supports + beam
GFill $r[1] 255 $PAL.stone1 0 108 426 132                                   # rock floor
GFill $r[1] 120 $PAL.earth3 0 150 426 2; GFill $r[1] 120 $PAL.earth3 0 158 426 2   # rails
foreach ($tx in 20, 80, 140, 200, 260, 320, 380) { GFill $r[1] 90 $PAL.earth2 $tx 148 8 12 }  # sleepers
FloorSpeckle $r[0] 30 $PAL.wat2
SaveBg $r 'cutscene_mine'

# Hollow Forest: trunk columns at the sides, a dark canopy, roots across
# the floor, a few fireflies.
$r = New-Bg $PAL.night1 $PAL.veg0
foreach ($tx in 14, 52, 362, 400) { GFill $r[1] 170 $PAL.earth1 $tx 0 14 108; GFill $r[1] 60 $PAL.earth2 ($tx+3) 0 3 108 }  # trunks
foreach ($tx in 14, 52, 362, 400) { GFill $r[1] 80 $PAL.night2 $tx 40 14 3; GFill $r[1] 80 $PAL.night2 $tx 78 14 3 }        # bark bands
GEll $r[1] 120 $PAL.veg1 -40 -30 200 60; GEll $r[1] 120 $PAL.veg1 120 -36 220 62; GEll $r[1] 120 $PAL.veg1 290 -30 200 60  # canopy
GEll $r[1] 80 $PAL.veg0 60 -10 120 40; GEll $r[1] 80 $PAL.veg0 260 -12 130 42
GFill $r[1] 255 $PAL.veg0 0 108 426 132                                     # forest floor
GEll $r[1] 120 $PAL.earth1 -20 112 120 18; GEll $r[1] 120 $PAL.earth1 320 116 140 16; GEll $r[1] 100 $PAL.earth1 150 126 120 14  # roots
GFill $r[1] 80 $PAL.earth2 0 124 426 2
foreach ($f in @(@(120, 22), @(300, 14), @(200, 30))) { P $r[0] $f[0] $f[1] $PAL.veg3 }   # fireflies
FloorSpeckle $r[0] 34 $PAL.veg1
SaveBg $r 'cutscene_forest'

# Goosy Gauntlet: a pale moon, reeds at the sides, a mist band, still water
# for a floor with faint ripples.
$r = New-Bg $PAL.night2 $PAL.wat0
GEll $r[1] 70 $PAL.white0 34 4 26 26; GEll $r[1] 40 $PAL.white1 40 10 14 14   # the moon
Stars $r[0] 20 $PAL.white0 28
foreach ($rx in 8, 18, 30, 44, 56, 372, 384, 398, 410, 420) { GFill $r[1] 130 $PAL.veg2 $rx 58 2 50; GFill $r[1] 130 $PAL.veg2 ($rx-1) 54 4 8 }  # reeds + tufts
GFill $r[1] 30 $PAL.white0 0 98 426 10                                       # the mist band
GFill $r[1] 255 $PAL.wat1 0 108 426 132                                      # still water
foreach ($wy in 120, 134, 150, 172) { GFill $r[1] 40 $PAL.wat2 0 $wy 426 1 }
GFill $r[1] 40 $PAL.white0 60 116 30 1; GFill $r[1] 40 $PAL.white0 320 140 40 1  # moonlight on the water
FloorSpeckle $r[0] 18 $PAL.wat2
SaveBg $r 'cutscene_goosy'

# ===================== M118 event marker props (12x12) =====================
# Owner direction 2026-09-14: every event kind owns its own marker. Fifteen
# hand-placed 12x12 props in the M20 event-prop idiom (FR/P + Outline), one
# motif each, shape-distinct in silhouette, keyed to the art_bible §2 ramps
# (at most one glint). RNG-free and appended after the file's last reseed, so
# no other file's bytes shift. The seven M20/M30/M44 event props are untouched.
Write-Output 'Generating M118 event marker props...'

$b = New-Img 12 12                                                # armory ghost: a spectral helm
FR $b 3 2 6 3 $PAL.stone3; FR $b 3 2 6 1 $PAL.stone4              # dome + crown highlight
P $b 6 1 $PAL.violet                                               # plume
FR $b 2 5 8 1 $PAL.stone4                                          # brim
FR $b 4 6 4 3 $PAL.night2                                          # the empty face
P $b 5 7 $PAL.cyan                                                 # one glint eye
FR $b 4 9 4 1 $PAL.stone2; P $b 3 10 $PAL.stone1; P $b 8 10 $PAL.stone1   # trailing wisp
Outline $b; SaveImg $b 'props/event_armory_ghost.png'

$b = New-Img 12 12                                                # miner's cache: an ore sack + pick
FR $b 4 3 4 2 $PAL.earth3                                          # tied neck
FR $b 3 5 6 5 $PAL.earth2; FR $b 3 9 6 1 $PAL.earth1              # sack + shadow
P $b 5 7 $PAL.cyan; P $b 7 8 $PAL.violet; P $b 4 8 $PAL.glint      # gems showing through
FR $b 8 1 3 1 $PAL.stone4; P $b 9 2 $PAL.earth4; P $b 9 3 $PAL.earth4   # the pick leaning in
Outline $b; SaveImg $b 'props/event_miners_cache.png'

$b = New-Img 12 12                                                # elder root: a knot with a sprout
FR $b 2 6 8 3 $PAL.earth1                                          # the root
FR $b 4 5 4 1 $PAL.earth2; FR $b 5 4 2 1 $PAL.earth2               # the knot
P $b 1 8 $PAL.earth1; P $b 10 8 $PAL.earth1; P $b 2 9 $PAL.earth2; P $b 9 9 $PAL.earth2   # tips
P $b 6 3 $PAL.veg3; P $b 6 2 $PAL.veg2; P $b 5 2 $PAL.veg3; P $b 7 1 $PAL.veg3            # the sprout
Outline $b; SaveImg $b 'props/event_elder_root.png'

$b = New-Img 12 12                                                # duck peddler: a hooded pack-figure, a sickly duck aboard
FR $b 3 2 4 3 $PAL.night3; FR $b 4 4 2 1 $PAL.night1               # hood + face shadow
FR $b 3 5 4 4 $PAL.night3                                          # coat
FR $b 7 4 3 5 $PAL.earth1                                          # the pack
FR $b 8 2 2 2 $PAL.veg3; P $b 10 3 $PAL.gold; P $b 9 2 $PAL.night1  # the duck head, bill, eye
FR $b 3 9 1 2 $PAL.night1; FR $b 6 9 1 2 $PAL.night1               # boots
Outline $b; SaveImg $b 'props/event_duck_peddler.png'

$b = New-Img 12 12                                                # surveyor: a parchment with a red pin
FR $b 2 3 8 6 $PAL.earth4                                          # the sheet
FR $b 1 3 1 6 $PAL.earth3; FR $b 10 3 1 6 $PAL.earth3              # rolled ends
FR $b 4 5 4 1 $PAL.earth2; FR $b 4 7 3 1 $PAL.earth2               # map lines
FR $b 7 4 2 2 $PAL.danger; P $b 7 4 $PAL.glint                     # the pin
Outline $b; SaveImg $b 'props/event_surveyor.png'

$b = New-Img 12 12                                                # dragonform: a scaled snout, horns, a slit eye
FR $b 3 4 6 5 $PAL.maroon; FR $b 3 4 6 1 $PAL.flesh3               # head + highlight band
P $b 8 3 $PAL.earth4; P $b 9 2 $PAL.earth4; P $b 9 1 $PAL.earth4   # the curved horn
P $b 3 3 $PAL.earth4                                               # the second horn's stub
FR $b 5 6 2 2 $PAL.gold; P $b 6 6 $PAL.night1; P $b 6 7 $PAL.night1   # eye + slit
FR $b 4 9 4 1 $PAL.flesh2; P $b 5 9 $PAL.white1; P $b 7 9 $PAL.white1   # jaw + teeth
Outline $b; SaveImg $b 'props/event_dragonform.png'

$b = New-Img 12 12                                                # goose polymorph: a goose, a wand spark
FR $b 3 6 6 3 $PAL.white1                                          # body
FR $b 7 3 1 3 $PAL.white1; FR $b 7 2 2 2 $PAL.white1               # neck + head
P $b 9 3 $PAL.gold; P $b 8 2 $PAL.night1                           # bill, eye
P $b 4 9 $PAL.gold; P $b 7 9 $PAL.gold                             # feet
P $b 2 2 $PAL.violet; P $b 1 3 $PAL.glint; P $b 3 3 $PAL.glint; P $b 2 4 $PAL.glint   # the spark
Outline $b; SaveImg $b 'props/event_goose_polymorph.png'

$b = New-Img 12 12                                                # sacrifice: an anvil, a broken blade
FR $b 2 6 8 2 $PAL.stone3; P $b 10 6 $PAL.stone3                   # anvil top + horn
FR $b 4 8 4 1 $PAL.stone2; FR $b 3 9 6 1 $PAL.stone1               # waist + base
FR $b 5 2 3 1 $PAL.earth3                                          # the guard
FR $b 6 3 1 3 $PAL.clsKnight; P $b 6 3 $PAL.white2                 # the stub of the blade + glint
P $b 8 4 $PAL.clsKnight; P $b 9 5 $PAL.clsKnight                   # the chip flying off
Outline $b; SaveImg $b 'props/event_sacrifice.png'

$b = New-Img 12 12                                                # level altar: stepped stone, a rising spark
FR $b 2 9 8 1 $PAL.stone2; FR $b 3 8 6 1 $PAL.stone3; FR $b 4 7 4 1 $PAL.stone4   # three steps
FR $b 2 10 8 1 $PAL.night3                                         # base shadow
FR $b 5 5 2 2 $PAL.gold; P $b 6 4 $PAL.glint; P $b 6 3 $PAL.gold; P $b 6 2 $PAL.glint   # the spark rising
Outline $b; SaveImg $b 'props/event_level_altar.png'

$b = New-Img 12 12                                                # stranger story: the hooded Stranger
FR $b 4 1 4 3 $PAL.night3; FR $b 5 3 2 1 $PAL.night1               # hood + face shadow
FR $b 3 4 6 5 $PAL.night2; FR $b 3 4 6 1 $PAL.night3               # cloak + collar
P $b 6 3 $PAL.cyan                                                 # one glint eye
FR $b 4 9 2 1 $PAL.night1; FR $b 6 9 2 1 $PAL.night1               # feet
Outline $b; SaveImg $b 'props/event_stranger_story.png'

$b = New-Img 12 12                                                # token exchange: a balance, two tokens
FR $b 6 2 1 7 $PAL.stone4; FR $b 4 9 5 1 $PAL.stone3               # post + base
FR $b 2 3 9 1 $PAL.stone4                                          # the beam
P $b 2 4 $PAL.stone2; P $b 2 5 $PAL.stone2; P $b 10 4 $PAL.stone2; P $b 10 5 $PAL.stone2   # strings
FR $b 2 6 3 1 $PAL.stone3; FR $b 8 6 3 1 $PAL.stone3               # the pans
P $b 3 5 $PAL.gold; P $b 9 5 $PAL.violet                           # a gold coin, a violet token
Outline $b; SaveImg $b 'props/event_token_exchange.png'

$b = New-Img 12 12                                                # patrol reset: an hourglass
FR $b 2 1 8 1 $PAL.earth3; FR $b 2 10 8 1 $PAL.earth3              # caps
FR $b 2 2 1 8 $PAL.earth3; FR $b 9 2 1 8 $PAL.earth3               # posts
FR $b 3 2 6 1 $PAL.white0; FR $b 4 3 4 1 $PAL.white0               # upper glass
FR $b 5 4 2 1 $PAL.gold; P $b 5 5 $PAL.gold                        # sand in the neck
FR $b 5 6 2 1 $PAL.white0; FR $b 4 7 4 1 $PAL.white0               # lower glass
FR $b 4 8 4 1 $PAL.gold; FR $b 3 9 6 1 $PAL.gold                   # the heap
Outline $b; SaveImg $b 'props/event_patrol_reset.png'

$b = New-Img 12 12                                                # reels: a three-window machine, a lever
FR $b 2 2 8 1 $PAL.stone3; FR $b 2 3 8 7 $PAL.stone2               # cabinet
FR $b 3 5 1 2 $PAL.white1; FR $b 5 5 1 2 $PAL.white1; FR $b 7 5 1 2 $PAL.white1   # the windows
P $b 3 5 $PAL.danger; P $b 5 5 $PAL.gold; P $b 7 5 $PAL.cyan        # symbols in them
FR $b 4 8 4 1 $PAL.night2                                          # the coin slot
FR $b 10 3 1 4 $PAL.stone4; P $b 10 2 $PAL.danger                  # lever + knob
FR $b 2 10 8 1 $PAL.night3                                         # base
Outline $b; SaveImg $b 'props/event_reels.png'

$b = New-Img 12 12                                                # blackjack: two fanned cards
FR $b 2 3 5 7 $PAL.white2; P $b 3 4 $PAL.danger                    # the back card + a red pip
FR $b 5 2 5 7 $PAL.white1                                          # the front card
P $b 6 3 $PAL.night1; P $b 8 7 $PAL.night1; P $b 7 5 $PAL.cyan     # black pips + the crystal ace
Outline $b; SaveImg $b 'props/event_blackjack.png'

$b = New-Img 12 12                                                # goosy flock: five geese in a V
P $b 5 3 $PAL.white1; P $b 6 2 $PAL.white1; P $b 7 3 $PAL.white1   # the lead
P $b 2 6 $PAL.white1; P $b 3 5 $PAL.white1; P $b 4 6 $PAL.white1   # left wing
P $b 8 6 $PAL.white1; P $b 9 5 $PAL.white1; P $b 10 6 $PAL.white1  # right wing
P $b 1 9 $PAL.white0; P $b 2 8 $PAL.white0; P $b 3 9 $PAL.white0   # far left
P $b 8 9 $PAL.white0; P $b 9 8 $PAL.white0; P $b 10 9 $PAL.white0  # far right
Outline $b; SaveImg $b 'props/event_goosy_flock.png'

# ============================ M119 title scene ============================
# The title screen's own place (owner direction 2026-09-14): a still lake at
# night, reeds at the margins, the ridge low. Quiet behind the emblem row
# (y 0..40) and the plaque/phrase band (y 40..100; the state backs the phrase
# with a caption strip); the menu frame (y 106+) and the footer are opaque.
# Own rng reseed; every earlier file stays byte-identical.
Write-Output 'Generating M119 title scene...'
$script:rng = 20260914
$r = New-Bg $PAL.night1 $PAL.wat0
Stars $r[0] 26 $PAL.white0 34
GEll $r[1] 60 $PAL.white0 372 12 22 22; GEll $r[1] 40 $PAL.white1 377 17 12 12     # the moon, top right
GPoly $r[1] 80 $PAL.stone1 @(0,118, 50,96, 110,110, 170,92, 230,108, 290,90, 350,106, 426,94, 426,132, 0,132)   # the ridge
GPoly $r[1] 110 $PAL.night3 @(0,126, 80,112, 160,122, 240,110, 320,124, 426,114, 426,136, 0,136)             # the near shore
GFill $r[1] 255 $PAL.wat1 0 134 426 106                                     # the lake
foreach ($wy in 146, 160, 178, 200, 216) { GFill $r[1] 40 $PAL.wat2 0 $wy 426 1 }
GFill $r[1] 45 $PAL.white0 330 150 60 1; GFill $r[1] 35 $PAL.white0 344 164 34 1   # moonlight on the water
foreach ($rx in 8, 18, 30, 44, 56, 372, 384, 398, 410, 420) { GFill $r[1] 130 $PAL.veg2 $rx 108 2 60; GFill $r[1] 130 $PAL.veg2 ($rx - 1) 104 4 8 }   # reeds + tufts
GFill $r[1] 30 $PAL.white0 0 128 426 8                                       # a mist band on the shore
FloorSpeckle $r[0] 16 $PAL.wat2
SaveBg $r 'title'

# ======================= M119 painted battle stages =======================
# Corrected 2026-09-16 (owner brief "grounded battle stages"). A 426x150
# painting per themed BackdropStage - the WHOLE battle band (y 24 to two
# pixels above the command panel) - drawn unscaled under the M56 ink
# silhouettes and skipped in high contrast (render/BattleBackdrop). Three
# layers: the FAR strip (band y 0..12: wall base, ceiling, canopy, mist -
# the lowest contrast), the GROUND plane (y 12..150: ONE continuous surface
# every formation row stands on, lightening toward the near edge, with
# restrained perspective cues whose spacing grows downward) and the NEAR
# edge (the painting's bottom rows and the M56 rects). Binding rule: the
# action field (x 30..408, y 8..140 - render::actionField) is QUIET, NOT
# EMPTY. Major motifs are clipped out of it (Clip-Field: the margins, the
# skyline and the near strip only); the ground plane and its cues run
# straight through it; and Assert-StageGrounded proves before saving that
# (1) the ground reads as a plane distinct from the far strip, (2) every cue
# inside the field stays within a low-contrast band of the plane, (3) the
# cues cover a bounded share of the field, (4) no signal colour enters it,
# and (5) the plane is even along every party foot row. Own rng reseed; the
# section is the file's last, so every earlier PNG stays byte-identical.
Write-Output 'Generating M119 battle stages...'
$script:rng = 20260915
$BAW = 426; $BAH = 150; $HZ = 12
$field = New-Object System.Drawing.Rectangle(30, 8, 378, 132)
$footRows = @(28, 62, 96, 130)   # the party's sprite feet in band rows (screen 52/86/120/154 - 24)
function Ramp($g, [string]$topHex, [string]$botHex, [int]$y0, [int]$h) {   # a per-row value ramp (integer, deterministic)
  $a = C $topHex; $z = C $botHex
  for ($i = 0; $i -lt $h; $i++) {
    $t = 0.0; if ($h -gt 1) { $t = $i / ($h - 1) }
    $c = [System.Drawing.Color]::FromArgb(255, [int]($a.R + ($z.R - $a.R) * $t), [int]($a.G + ($z.G - $a.G) * $t), [int]($a.B + ($z.B - $a.B) * $t))
    $g.FillRectangle((New-Object System.Drawing.SolidBrush($c)), 0, ($y0 + $i), $BAW, 1) }
}
function New-Stage([string]$farTop, [string]$farBot, [string]$gndTop, [string]$gndBot) {
  $b = New-Object System.Drawing.Bitmap($BAW, $BAH, [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
  $g = [System.Drawing.Graphics]::FromImage($b); $g.SmoothingMode = 'None'
  Ramp $g $farTop $farBot 0 $HZ
  Ramp $g $gndTop $gndBot $HZ ($BAH - $HZ)
  return @($b, $g)
}
function Clip-Field($g) { $g.SetClip($field, [System.Drawing.Drawing2D.CombineMode]::Exclude) }
function Unclip-Field($g) { $g.ResetClip() }
function Snapshot($b) { New-Object System.Drawing.Bitmap($b) }
function Pixels($b) {   # the bitmap as ARGB ints, row-major
  $rect = New-Object System.Drawing.Rectangle(0, 0, $b.Width, $b.Height)
  $d = $b.LockBits($rect, [System.Drawing.Imaging.ImageLockMode]::ReadOnly, [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
  $n = $b.Width * $b.Height; $px = New-Object int[] $n
  [System.Runtime.InteropServices.Marshal]::Copy($d.Scan0, $px, 0, $n)
  $b.UnlockBits($d); return ,$px
}
function Lum([int]$argb) { (0.299 * (($argb -shr 16) -band 255)) + (0.587 * (($argb -shr 8) -band 255)) + (0.114 * ($argb -band 255)) }
$signal = @($PAL.cyan, $PAL.violet, $PAL.glint, $PAL.gold, $PAL.danger, $PAL.heal, $PAL.white1, $PAL.white2) | ForEach-Object { (C $_).ToArgb() }
function Assert-StageGrounded($b, $ref, [string]$name) {
  $px = Pixels $b; $rp = Pixels $ref; $w = $b.Width
  # (1) the ground is its own plane: the far strip and the ground differ in value across the field's span
  $farSum = 0.0; $farN = 0; $gndSum = 0.0; $gndN = 0
  for ($y = 2; $y -lt $HZ; $y++) { for ($x = $field.Left; $x -lt $field.Right; $x += 3) { $farSum += Lum $px[$y * $w + $x]; $farN++ } }
  for ($y = $HZ + 2; $y -lt $field.Bottom; $y += 2) { for ($x = $field.Left; $x -lt $field.Right; $x += 3) { $gndSum += Lum $px[$y * $w + $x]; $gndN++ } }
  $gndMean = $gndSum / $gndN; $step = [math]::Abs($gndMean - $farSum / $farN)
  if ($step -lt 6) { throw "M119 battle stage '$name': the ground does not separate from the far strip (value step $step < 6)" }
  # (2)(3)(4) inside the action field: cues are low-contrast, bounded in number, never a signal colour
  $changed = 0; $total = 0; $maxDelta = 0.0
  for ($y = $field.Top; $y -lt $field.Bottom; $y++) { for ($x = $field.Left; $x -lt $field.Right; $x++) {
    $i = $y * $w + $x; $v = $px[$i]; $total++
    if ($signal -contains $v) { throw "M119 battle stage '$name': a signal colour inside the action field at ($x,$y)" }
    if ($v -ne $rp[$i]) { $changed++; $d = [math]::Abs((Lum $v) - (Lum $rp[$i])); if ($d -gt $maxDelta) { $maxDelta = $d }
      if ($d -gt 26) { throw "M119 battle stage '$name': the cue at ($x,$y) is too strong for the action field (luminance delta $d > 26)" } } } }
  $frac = $changed / $total
  if ($frac -lt 0.02) { throw "M119 battle stage '$name': the action field is empty (cues on $([math]::Round($frac * 100, 1))% of it, under 2%)" }
  if ($frac -gt 0.35) { throw "M119 battle stage '$name': the action field is busy (cues on $([math]::Round($frac * 100, 1))% of it, over 35%)" }
  # (5) the plane is even along every party foot row: nothing pools or glows under a sprite's feet
  foreach ($fy in $footRows) { for ($x = $field.Left; $x -lt $field.Right; $x += 2) {
    $d = [math]::Abs((Lum $px[$fy * $w + $x]) - $gndMean)
    if ($d -gt 26) { throw "M119 battle stage '$name': the ground at ($x,$fy) strays from the plane under a foot row (delta $d > 26)" } } }
  Write-Output ("  battle_{0}: far/ground step {1:N1}, cues on {2:P1} of the field, strongest cue {3:N1}" -f $name, $step, $frac, $maxDelta)
}
function SaveStage($pair, $ref, [string]$name) {
  $pair[1].Dispose(); Assert-StageGrounded $pair[0] $ref $name; $ref.Dispose(); SaveImg $pair[0] "backgrounds/battle_$name.png"
}
function Courses($g, [int]$a, [string]$hex, [int[]]$ys) { foreach ($y in $ys) { GFill $g $a $hex 0 $y $BAW 1 } }   # 1px course lines across the ground
function Joints($g, [int]$a, [string]$hex, [int[]]$xyh) { for ($i = 0; $i -lt $xyh.Count; $i += 3) { GFill $g $a $hex $xyh[$i] $xyh[$i + 1] 1 $xyh[$i + 2] } }   # short 1px joints (x, y, length)
function Root($g, [int]$a, [string]$hex, [int]$x, [int]$y, [int]$dir, [int]$n) {   # a stepped 2px root arc: n segments of 4x2, each a row lower and four columns along (no overlap, so no double blend)
  for ($i = 0; $i -lt $n; $i++) { GFill $g $a $hex ($x + $dir * 4 * $i) ($y + $i) 4 2 } }
function GroundSpeckle($g, [int]$n, [string]$hex) {   # sparse blended single pixels on the ground rows, never in the far strip or the near edge
  for ($i = 0; $i -lt $n; $i++) { $x = [int]((Rnd) * $BAW); $y = $HZ + 6 + [int]((Rnd) * ($BAH - $HZ - 18)); GFill $g 90 $hex $x $y 1 1 } }

# Ruined Keep: coursed masonry in the far strip; a broken wall stump (left)
# and the tower's edge (right) in the margins, rubble stones at their feet;
# the ground ONE flagstone floor - courses at perspective spacing, staggered
# joints, two cracks (the M56 ink piles sit on top at the near edge).
$r = New-Stage $PAL.night1 $PAL.night2 $PAL.night3 $PAL.stone1
GFill $r[1] 70 $PAL.night1 0 5 $BAW 1; GFill $r[1] 70 $PAL.night1 0 9 $BAW 1    # masonry courses
GFill $r[1] 40 $PAL.stone2 0 11 $BAW 1                                          # the wall's base course, lit
$ref = Snapshot $r[0]
Clip-Field $r[1]
GPoly $r[1] 110 $PAL.stone1 @(0,12, 30,12, 30,40, 22,34, 14,46, 6,38, 0,48)     # the wall stump, ragged
GFill $r[1] 150 $PAL.night1 12 16 3 12                                          # its arrow slit
GFill $r[1] 110 $PAL.stone2 408 0 18 150; GFill $r[1] 90 $PAL.stone3 408 0 18 2  # the tower's edge and its lit cap
GFill $r[1] 120 $PAL.night1 414 20 3 14; GFill $r[1] 120 $PAL.night1 414 64 3 14  # two slits
GFill $r[1] 100 $PAL.stone2 4 118 8 4; GFill $r[1] 100 $PAL.stone2 16 128 10 4; GFill $r[1] 100 $PAL.stone2 412 122 8 4   # rubble stones
Unclip-Field $r[1]
Courses $r[1] 110 $PAL.night1 @(24, 42, 66, 96, 132)
Joints $r[1] 90 $PAL.night1 @(110,12,12, 200,12,12, 280,12,12, 150,24,18, 240,24,18, 390,24,18, 120,42,24, 210,42,24, 290,42,24, 170,66,30, 260,66,30, 100,66,30, 230,96,36, 140,96,36, 380,96,36)
foreach ($c in @(@(160,112), @(250,126))) { GFill $r[1] 120 $PAL.night1 $c[0] $c[1] 4 1; GFill $r[1] 120 $PAL.night1 ($c[0] + 4) ($c[1] + 1) 3 1; GFill $r[1] 120 $PAL.night1 ($c[0] + 7) ($c[1] + 2) 4 1; GFill $r[1] 120 $PAL.night1 ($c[0] + 11) ($c[1] + 3) 3 1 }   # cracks, stepped
GroundSpeckle $r[1] 24 $PAL.stone2
SaveStage $r $ref 'keep'

# Crystal Mine: the rock ceiling with a quiet crystal drip and a timber
# lintel in the far strip; timber supports and crystal clusters (a facet
# glint each) in the margins; the ground a rough rock floor - broad dark ore
# veins (part of the plane), seams, then the rails and sleepers across it.
$r = New-Stage $PAL.night1 $PAL.night2 $PAL.night3 $PAL.stone1
foreach ($cx in 60, 150, 240, 330) { GPoly $r[1] 45 $PAL.wat3 @(($cx - 6),0, ($cx + 6),0, $cx,7) }   # the drip line
GFill $r[1] 60 $PAL.earth1 0 10 $BAW 2                                          # the lintel
GPoly $r[1] 40 $PAL.wat1 @(80,20, 140,26, 190,40, 230,60, 260,90, 300,130, 240,132, 200,100, 150,60, 90,30)   # an ore vein sweeping toward the near edge
GPoly $r[1] 35 $PAL.wat1 @(340,40, 400,30, 404,60, 360,80)
$ref = Snapshot $r[0]
Clip-Field $r[1]
GFill $r[1] 150 $PAL.earth1 24 0 6 150; GFill $r[1] 150 $PAL.earth1 410 0 6 150   # timber supports
GPoly $r[1] 90 $PAL.violet @(0,150, 0,96, 10,72, 20,110, 22,150); GPoly $r[1] 90 $PAL.wat3 @(2,150, 8,118, 16,150)   # the left cluster
GPoly $r[1] 90 $PAL.violet @(416,150, 418,100, 426,88, 426,150)                 # the right cluster
P $r[0] 9 84 $PAL.cyan; P $r[0] 421 104 $PAL.cyan                              # facet glints (margins only)
Unclip-Field $r[1]
Courses $r[1] 90 $PAL.night1 @(30, 54, 84)
GFill $r[1] 70 $PAL.earth3 0 108 $BAW 2; GFill $r[1] 70 $PAL.earth3 0 122 $BAW 2   # the rails
foreach ($tx in 4, 50, 96, 142, 188, 234, 280, 326, 372) { GFill $r[1] 60 $PAL.earth2 $tx 104 6 22 }   # sleepers
GroundSpeckle $r[1] 20 $PAL.wat2
SaveStage $r $ref 'mine'

# Hollow Forest: the canopy's underside in the far strip; trunks with bark
# bands in the margins (two left, one right) and a firefly each side; the
# ground leaf-litter earth with a path widening toward the near edge (part
# of the plane), faint earth bands, stepped root arcs and leaf clusters.
$r = New-Stage $PAL.night1 $PAL.veg0 $PAL.veg0 $PAL.earth1
GEll $r[1] 60 $PAL.veg1 -40 -14 200 22; GEll $r[1] 60 $PAL.veg1 120 -16 220 24; GEll $r[1] 60 $PAL.veg1 280 -14 200 22   # the canopy
GPoly $r[1] 40 $PAL.earth2 @(150,12, 276,12, 366,150, 60,150)                    # the path
$ref = Snapshot $r[0]
Clip-Field $r[1]
foreach ($tx in 2, 18) { GFill $r[1] 170 $PAL.earth1 $tx 0 10 150; GFill $r[1] 60 $PAL.earth2 ($tx + 3) 0 3 150; GFill $r[1] 80 $PAL.night2 $tx 36 10 3; GFill $r[1] 80 $PAL.night2 $tx 92 10 3 }   # left trunks + bark bands
GFill $r[1] 170 $PAL.earth1 410 0 14 150; GFill $r[1] 60 $PAL.earth2 414 0 3 150; GFill $r[1] 80 $PAL.night2 410 50 14 3; GFill $r[1] 80 $PAL.night2 410 110 14 3   # the right trunk
P $r[0] 14 50 $PAL.veg3; P $r[0] 420 66 $PAL.veg3                               # fireflies
GFill $r[1] 110 $PAL.earth1 0 140 30 10; GFill $r[1] 110 $PAL.earth1 408 140 18 10   # the near roots' mass
Unclip-Field $r[1]
foreach ($y in 24, 42, 66, 96, 132) { GFill $r[1] 30 $PAL.night1 0 $y $BAW 1 }    # earth bands, faint
Root $r[1] 100 $PAL.night1 32 118 1 5; Root $r[1] 100 $PAL.night1 384 124 -1 5   # roots reaching in from the margins
Root $r[1] 100 $PAL.night1 150 128 1 4; Root $r[1] 100 $PAL.night1 262 106 -1 4  # and two in the open ground
foreach ($lx in 100, 172, 236, 300, 360) { GFill $r[1] 70 $PAL.veg1 $lx 60 2 1; GFill $r[1] 70 $PAL.veg1 ($lx + 30) 84 2 1 }   # leaf clusters
GroundSpeckle $r[1] 26 $PAL.veg1
SaveStage $r $ref 'forest'

# Castle: the dark hall with a plinth line in the far strip; pilasters at
# the outer edges and a pointed window below each banner in the margins;
# the ground a paved floor whose dais runway (part of the plane) widens
# toward the near edge - tile courses at perspective spacing and staggered
# joints (the banners and the throne arch are the M56 rects on top).
$r = New-Stage $PAL.night1 $PAL.void1 $PAL.void1 $PAL.stone1
GFill $r[1] 40 $PAL.stone2 0 10 $BAW 1                                          # the plinth line
GPoly $r[1] 45 $PAL.stone2 @(140,12, 286,12, 336,150, 90,150)                   # the dais runway
$ref = Snapshot $r[0]
Clip-Field $r[1]
GFill $r[1] 90 $PAL.stone2 0 0 6 150; GFill $r[1] 60 $PAL.stone3 5 0 1 150      # the left pilaster and its lit edge
GFill $r[1] 90 $PAL.stone2 420 0 6 150; GFill $r[1] 60 $PAL.stone3 420 0 1 150  # the right pilaster
foreach ($wx in 10, 408) { GFill $r[1] 60 $PAL.wat3 $wx 62 14 44; GFill $r[1] 40 $PAL.violet ($wx + 3) 66 8 36; GPoly $r[1] 60 $PAL.wat3 @($wx,62, ($wx + 7),55, ($wx + 14),62) }   # pointed windows below the banners
GFill $r[1] 110 $PAL.stone2 0 140 30 10; GFill $r[1] 110 $PAL.stone2 408 140 18 10   # the near edge's paving
Unclip-Field $r[1]
Courses $r[1] 100 $PAL.night1 @(24, 42, 66, 96, 132)
Joints $r[1] 80 $PAL.night1 @(130,12,12, 213,12,12, 296,12,12, 100,24,18, 180,24,18, 260,24,18, 380,24,18, 140,42,24, 220,42,24, 290,42,24, 110,66,30, 200,66,30, 270,66,30, 160,96,36, 240,96,36, 370,96,36)
GroundSpeckle $r[1] 12 $PAL.stone2
SaveStage $r $ref 'castle'

# Goosy Gauntlet: the far pond under a mist band in the far strip; reeds
# with tufts in the margins; the ground a muddy BANK between that water and
# the near water's edge (the bottom rows) - grass along the far edge, faint
# mud bands, puddles that grow toward the near edge with a ripple each. The
# party stands on the bank, never on open water.
$r = New-Stage $PAL.night1 $PAL.night2 $PAL.night3 $PAL.earth1                # the far pond is night-dark; the mist is its light
GFill $r[1] 36 $PAL.white0 0 2 $BAW 4; GFill $r[1] 40 $PAL.wat2 0 9 $BAW 1      # the mist, a far ripple
$ref = Snapshot $r[0]
Clip-Field $r[1]
foreach ($rx in 4, 12, 22, 412, 420) { GFill $r[1] 130 $PAL.veg2 $rx 14 2 136; GFill $r[1] 130 $PAL.veg2 ($rx - 1) 10 4 8 }   # reeds + tufts
GFill $r[1] 150 $PAL.wat1 0 140 30 10; GFill $r[1] 150 $PAL.wat1 408 140 18 10; GFill $r[1] 110 $PAL.wat1 30 142 378 8   # the near water's edge
Unclip-Field $r[1]
foreach ($tx in 40, 96, 150, 210, 262, 318, 372) { GFill $r[1] 80 $PAL.veg1 $tx 13 2 3 }   # grass along the far edge
foreach ($y in 26, 46, 72, 104) { GFill $r[1] 45 $PAL.night1 0 $y $BAW 1 }      # mud bands
GEll $r[1] 70 $PAL.wat1 100 40 40 6; GEll $r[1] 70 $PAL.wat1 230 74 56 8; GEll $r[1] 70 $PAL.wat1 130 116 70 10; GEll $r[1] 70 $PAL.wat1 340 100 50 8   # puddles, larger nearer
GFill $r[1] 60 $PAL.wat2 110 43 20 1; GFill $r[1] 60 $PAL.wat2 250 78 24 1; GFill $r[1] 60 $PAL.wat2 150 121 30 1; GFill $r[1] 60 $PAL.wat2 352 104 26 1   # ripples
foreach ($pb in @(@(84,58), @(196,52), @(292,66), @(120,92), @(272,98), @(386,84), @(212,136), @(60,128))) { GFill $r[1] 80 $PAL.stone1 $pb[0] $pb[1] 2 1 }   # pebbles
foreach ($gt in @(@(70,30), @(250,34), @(330,28), @(180,48), @(392,44))) { GFill $r[1] 70 $PAL.veg1 $gt[0] $gt[1] 1 2; GFill $r[1] 70 $PAL.veg1 ($gt[0] + 2) ($gt[1] + 1) 1 1 }   # grass tufts on the bank
GroundSpeckle $r[1] 16 $PAL.veg1
SaveStage $r $ref 'goosy'

# --- M121: skill-kind icons + the milestone mark (10x10 pixel grids) --------
#
# One icon per content::SkillKind (the exact vocabulary is
# content::kSkillKindIds; the presentation lint holds the two in lockstep)
# plus the mark a milestone-touched skill wears. The kinds are DERIVED when
# content loads - nothing here is authored per skill. Same idiom and size as
# the M81 gear icons (Save-IconGrid, the shared $GRIDC key, no Outline pass);
# each kind is a distinct SHAPE first and a colour second, so the row reads
# without colour (flame, snowflake, bolt, boulder, sun disc, crescent, claw
# marks, plus, up-chevrons, down-chevrons, sigil ring, star).
#
# DETERMINISM: hand-placed pixels only - NO random helper is called, so this
# appended section cannot shift any other generated file's bytes.
Write-Output 'Generating skill-kind icons (M121 pixel grids)...'

Save-IconGrid 'skill_fire' @(       # flame: red tongue, gold heart, white core
  '....D.....'
  '...DD.....'
  '...DDD.D..'
  '..DDYD.DD.'
  '..DYYDDD..'
  '.DDYYYDD..'
  '.DYYWYYD..'
  '.DYWWWYD..'
  '..DYWYD...'
  '...DDD....'
)

Save-IconGrid 'skill_ice' @(        # six-armed snowflake
  '....W.....'
  '..C.W.C...'
  '...CWC....'
  '.C..W..C..'
  'WWWWWWWWW.'
  '.C..W..C..'
  '...CWC....'
  '..C.W.C...'
  '....W.....'
  '..........'
)

Save-IconGrid 'skill_lightning' @(  # zigzag bolt, glint leading edge
  '......GY..'
  '.....GY...'
  '....GY....'
  '...GYYYY..'
  '......GY..'
  '.....GY...'
  '....GY....'
  '...GY.....'
  '..GY......'
  '..Y.......'
)

Save-IconGrid 'skill_earth' @(      # a lit boulder
  '..........'
  '....dd....'
  '...dffd...'
  '..dfffsd..'
  '.dffsssd..'
  '.dfssssad.'
  'dfssssaad.'
  'dsssaaaad.'
  '.aaaaaaa..'
  '..........'
)

Save-IconGrid 'skill_holy' @(       # sun disc with eight rays
  '....Y.....'
  '.Y..Y..Y..'
  '..Y...Y...'
  '...WWW....'
  'YY.WWW.YY.'
  '...WWW....'
  '..Y...Y...'
  '.Y..Y..Y..'
  '....Y.....'
  '..........'
)

Save-IconGrid 'skill_dark' @(       # crescent moon
  '...VVV....'
  '..VVm.....'
  '.VVm......'
  '.VVm......'
  '.VVm......'
  '.VVm......'
  '.VVVm...V.'
  '..VVVmmVV.'
  '...VVVVV..'
  '..........'
)

Save-IconGrid 'skill_neutral' @(    # three claw marks: plain, elementless force
  '...S..S..W'
  '..S..S..W.'
  '.S..S..W..'
  'S..S..W...'
  '..S..W....'
  '.S..W.....'
  'S..W......'
  '..W.......'
  '.W........'
  'W.........'
)

Save-IconGrid 'skill_heal' @(       # green plus, bright heart
  '..........'
  '....HH....'
  '....HH....'
  '....HH....'
  '.HHHWWHHH.'
  '.HHHWWHHH.'
  '....HH....'
  '....HH....'
  '....HH....'
  '..........'
)

Save-IconGrid 'skill_buff' @(       # two chevrons UP
  '..........'
  '....CC....'
  '...CCCC...'
  '..CC..CC..'
  '.CC....CC.'
  '....CC....'
  '...CCCC...'
  '..CC..CC..'
  '.CC....CC.'
  '..........'
)

Save-IconGrid 'skill_debuff' @(     # two chevrons DOWN
  '..........'
  '.DD....DD.'
  '..DD..DD..'
  '...DDDD...'
  '....DD....'
  '.DD....DD.'
  '..DD..DD..'
  '...DDDD...'
  '....DD....'
  '..........'
)

Save-IconGrid 'skill_summon' @(     # sigil: gold ring, glint diamond, white heart
  '...YYYY...'
  '..Y....Y..'
  '.Y..GG..Y.'
  'Y..G..G..Y'
  'Y.G.WW.G.Y'
  'Y.G.WW.G.Y'
  'Y..G..G..Y'
  '.Y..GG..Y.'
  '..Y....Y..'
  '...YYYY...'
)

Save-IconGrid 'milestone' @(        # the milestone mark: a gold star
  '..........'
  '....Y.....'
  '....Y.....'
  '...YYY....'
  'YYYYYYYYY.'
  '.YYYYYYY..'
  '..YYYYY...'
  '..YY.YY...'
  '.YY...YY..'
  '..........'
)

# --- M127: curio icons, the map-piece scrap, the four-piece treasure map -----
#
# Owner batch 3 (2026-09-20). THREE things, all hand-placed grids on the
# shared $GRIDC key - NO random helper is called anywhere in this section, so
# it cannot shift any other generated file's bytes:
#
#  1. One 10x10 icon per curio (game/Curios.hpp; id 'ui.icon.curio.<curio id>',
#     file 'curio_<curio id>.png'). Same idiom and size as the M81 gear and
#     M121 skill icons (Save-IconGrid, no Outline pass): each curio is a
#     distinct SHAPE first, so the Curios grid reads without colour.
#  2. The dungeon floor's Secret Map Piece: a 12x12 prop (Outline pass, like
#     every prop) - a torn parchment scrap carrying a trail and the X, the
#     same parchment the Maps screen shows.
#  3. The treasure map of the Maps screen: ONE 100x56 grid (drawn at 2x, so
#     200x112 on screen) cut into four pieces along two zigzag tears. Each
#     piece is saved on the full 100x56 canvas (transparent elsewhere), so
#     the screen draws every owned piece at the same origin and they fit by
#     construction. The tear's edge pixels are shaded one step (earth3) on
#     each piece - a torn rim alone, a faint crease when both halves meet.
#     Parchment on the earth ramp, ink in earth1, sea on the water ramp, the
#     wood on the vegetation ramp, the peaks on stone with a white cap, the X
#     in the danger red (art_bible S2: it marks a guarded treasure).
Write-Output 'Generating curio icons and the treasure map (M127 pixel grids)...'

Save-IconGrid 'curio_keep_crown_shard' @(        # a broken arc of a coronet: two points, a red stone, a snapped edge
  '..........'
  '.Y..Y.....'
  '.Y..Y..Y..'
  '.YY.YY.Y..'
  '.YYYYYYY..'
  '.YDYYWYY..'
  '.YYYYYY...'
  '.ddddd....'
  '..........'
  '..........'
)

Save-IconGrid 'curio_keep_banner' @(             # a pole and a tattered red pennant with a gold device
  '.S........'
  '.SDDDDDD..'
  '.SDDYDDDD.'
  '.SDYYYDD..'
  '.SDDYDDDD.'
  '.SDDDDD...'
  '.S.DD.D...'
  '.S........'
  '.S........'
  '.S........'
)

Save-IconGrid 'curio_keep_gate_key' @(           # a ring-bowed key gone to rust
  '..........'
  '..........'
  '.OOO......'
  'O...O.....'
  'O...OOOOOO'
  'O...O..O.O'
  '.OOO...s.s'
  '..........'
  '..........'
  '..........'
)

Save-IconGrid 'curio_keep_gargoyle_ear' @(       # a pointed stone ear, the hollow in shadow
  '.......r..'
  '......re..'
  '.....ree..'
  '....rewe..'
  '...rewwe..'
  '..rewqwe..'
  '..rewqwe..'
  '..reewe...'
  '...reee...'
  '....ee....'
)

Save-IconGrid 'curio_mine_singing_crystal' @(    # a cyan shard and the note it hums
  '....C.....'
  '...CWC..W.'
  '...CWC..WW'
  '..CCWCC.W.'
  '..CWWCC.W.'
  '..CWCCCWW.'
  '..CCCCCWW.'
  '...CCC....'
  '...oCo....'
  '....o.....'
)

Save-IconGrid 'curio_mine_lucky_lamp' @(         # an oil lamp: loop handle, spout, a small flame
  '.......Y..'
  '......YWY.'
  '.......Y..'
  '.......O..'
  '.OO..OOO..'
  'O..OOOOO..'
  'O..OOOOOO.'
  '.OOOOOOO..'
  '...OOOO...'
  '..OOOOOO..'
)

Save-IconGrid 'curio_mine_geode_heart' @(        # a split stone, violet crystal inside
  '..........'
  '...eeee...'
  '..eerree..'
  '.eeVGVVee.'
  '.eVGWGVVe.'
  '.eVVGCVVe.'
  '.eeVCVVee.'
  '..eeVVee..'
  '...eeee...'
  '..........'
)

Save-IconGrid 'curio_mine_vein_etching' @(       # a slab scratched with a branching vein
  '..........'
  '.rrrrrrrr.'
  '.eeeeCeee.'
  '.eCeeCeee.'
  '.eeCCeeCe.'
  '.eeeCeCee.'
  '.eeeCCeee.'
  '.eeCeeeee.'
  '.wwwwwwww.'
  '..........'
)

Save-IconGrid 'curio_forest_elder_acorn' @(      # cap, nut, a glint
  '....ss....'
  '....s.....'
  '..ssssss..'
  '.sdsdsdss.'
  '.ssssssss.'
  '..fffffd..'
  '..fWfffd..'
  '..ffffdd..'
  '...fffd...'
  '....fd....'
)

Save-IconGrid 'curio_forest_owl_quill' @(        # a barred feather, a gold nib
  '.......dSW'
  '......SSWd'
  '.....dSWS.'
  '....SSWdS.'
  '...dSWSS..'
  '..SSWdS...'
  '..SWSS....'
  '.SW.......'
  'SW........'
  'Y.........'
)

Save-IconGrid 'curio_forest_moss_idol' @(        # a stone idol the moss has given a beard
  '...eeee...'
  '..erreee..'
  '..eKeeKe..'
  '..eeeeee..'
  '..ecvvce..'
  '...cvvc...'
  '..eecvee..'
  '..eeecee..'
  '..eeeeee..'
  '.rrrrrrrr.'
)

Save-IconGrid 'curio_forest_firefly_lantern' @(  # a hanging jar, green light, two fireflies
  '....ee....'
  '...e..e...'
  '...eeee...'
  '..SHHHHS..'
  '..SHWHHS..'
  '..SHHHWS..'
  '..SHWHHS..'
  '..SHHHHS..'
  '...eeee...'
  '..........'
)

Save-PropGrid 'map_piece' @(   # the floor pickup: one torn piece, a trail and the X
  '............'
  '..fff.ffff..'
  '.ffffffffdf.'
  '.fsaffffffd.'
  '.ffsaffDfDd.'
  '..ffsaffDfd.'
  '.fffffaDfDd.'
  '.ffaffffffd.'
  '.fdfdffffdd.'
  '..ddd.dddd..'
  '............'
  '............'
)

$mapRows = @(
  '..ssssss.s.ssssss.s.ssssss.s.ssssss.s.ssssss.s.ssssss.s.ssssss.s.ssssss.s.ssssss.s.ssssss.s.ssssss..'
  '.sffffffss.sfffffsdsffffffss.sfffffsfsfffdffss.sfffffsfsffffffss.sfffffsfsffffffss.sfffdfsfsffffffs.'
  'sfffffffffsffdffffffffffffffsfffffffdfffffffffsffffffffffffdffffsfffffffffffffffffsffffffffffffffffs'
  '.sffffffdffffafafffffffffffffffdfffafaffffffffffffffffdffffffffffffffffffffffdfffffffffffffffffffffs'
  'sffdffffffffafafafffffffffdfffffffafafaffffffffffdffffffffffffffffffffffdffffffffffffffffffffffdfffs'
  '.sfffffffffffffffffffdffffpoopffffffffffffffdffffffffffffffffffffffdffffffffffffffffffffffdffffffffs'
  'sfffffpoopffffffdffffffffffffffffffffffdffffffffffffffffffffffdffffffffffffffffffffffdffffffffffffs.'
  'sffffffffffdffffffffffffffffffffffdffffffffffffffffffffffdffffffffffffffffffffffdffffffffffffffffffs'
  'sfffffdffffffffffffffffffffffdfffffffffpoopfffffffffdffffffffffffffffffffffdfaffffffffffffffffffffs.'
  'sdffffffffffffffpooopfffdffffffffffffffffffffffdffffffffffffffffffffffdfffffWWSffffffffffffffdfffffs'
  'sffffffffffffffffffdffffffffffffffffffffffdffffffffffffffffffffffdfffffffffWWWSSffffffffdffffffffffs'
  'sfffffffffffffdffffffffffffffffffffffdffffffffffffffffffffffdfffffffffffffWWWWSSSffdfffffffffffffffs'
  '.sfffffffdffffffffffffffffffffpooopffffffffffffffffffffdfffffffffffffffffarreeeewafffffffffffffffffs'
  'ssffdffffpoopffffffffffffffdffffffffffffffffffffffdfffffffffffffffffffffarrreeeewwafffffffffffffdffs'
  '..sfffffffffffffffffffdfffffffffffffffffffpopdffffffffffffffffffffffdffarrreeeeeewwafffffffdfffffffs'
  'ssfffffffffffffffdfffpopffffffffffffffffdffffffffffffffffffffffdffffffarrrreeeeeewwwafdfffffffffffs.'
  'sfffffffffffdfffffaaaafffffffffffffdffffffffafffffffffffffdafffffffffarrrrreeeeeeewwwaffffffffffffss'
  'sffffffdfffffffffafssfafffffffdfffffffffffaasaaafffffdffffaeafffffffarrrrreeeeeeeewwwwaffffffffffs..'
  'sfdfffffffffffffasffffsafdfffffffffffffffafsffssdffffffffareeafffffarrrrrreeeeeeeeewwwwafffffadfffss'
  'sffffffffffffffasfffdffsaaffaaafffffffffasfdffffffffffffareeewafffarrrrrrreeeeeeeeewwwwwadffaeaffffs'
  'sffffaaaaafffaadfffffffffsaassfaafffffdasffffffffffffffarreeeewafarrrrrrreeeeeeeeeeewwwwwafareeafffs'
  '.sfaasfssfaaassfffffffffffsffffssaafffafffffffffffffffarrreeeewwarrrrrrrreeeeeeeeeeewwwwwwareeewaffs'
  'sfffsdffffssffffffffffffaaffdfffffsaaasffffffffffffdfarrreeeeeearrrrrrrrreeeeeeeeeeeewwwwwwaeeeewafs'
  '.sfffffffffdfffffffffdfdfffffffffffsfsffffffffdfffffarrrreeeeearrrrrrrrreeeeeeeeeeeeewwwwwwwaeeewwas'
  'sfffffffffffffffffdffffffffaaffffffffffffdfffffffffarrrrreeeearrrrrrrrrreeeeeeeeeeeeeewwwwwwwaeeeww.'
  'sffffffffffffdffffffffffffffffffffffdffffffffffffffffffsaaasaaasaaasaaasaaasaaasaaasaaasaaasaaasaffs'
  'sfffffffdffffffffffffffffffffffdffffffffffffffffffffffdffffffffffffffffffffffdffffffffffffffffffffs.'
  'sffdfffffffdfffffffffdffffdfffaafffffffffffffffffdffffffffffffffffffffffdffffffffffffffffffffffdfffs'
  'sffffffffffffffffffffdffffffffffffffffffffffdffffffffffffffffffffffdffffffffffffffffffffffdffffffffs'
  'sfffffffffffffffdffffffffffffffffffffffdffffffffffffffffffffffdffffffffffffffffffffffdfffffDfffffffs'
  '.sfffffffffdfffffffffffffffffffaafdffffffffffffffffffffffdffffffffffffffffffffffdffffffffffafffffffs'
  'ssffffdffffffffvccfffffffffffdffffffffffffffffffffffdffffffffffffffffffffffdfffffffffffffffaffffffds'
  '..sfffffffffffvcccxfffffdvccfffffffffffffffffffdffffffffffffffffffffffdfffffffffffffffffffsasdfffffs'
  'ssffffvccfffffcccxxdffffvcccxffffaafffffffdffffffffffffffffffffffdffffffffffffffffffffffaaaaaaafffs.'
  'sffffvcccxffffdxxxffffffcccxxffffffffdffffffffffffffffffffffdffffffffffffffffffffffdffffffsasfffffss'
  'sffffcccxxffffffaffffffffxxxffffdffffffffffffffffffffffdffffffffffffffffffffffdffffffffffffafffffs..'
  'sfffdfxxxfffffffafffffffffadfffffffaafffffffffffffdfffffffffffaafaaffffffdfffffffffffffffffaffffdfss'
  'sffffffaffffffffffffffdfffaffffffffffffffffffdffffffffffaafaafffffffaafffffffffffffffffffffdfffffffs'
  'sffffffafffffffffdffffffffffffffffffffffdffffffffffffaaffffffffdfffffffaafffffffffffffdffffffffffffs'
  '.sffffffffffdffffffffffffffffffffffdffaafaafaafaafaaffffffdfffffffffffffffffffffKdfffffffffffKfffffs'
  'sffffffdffffffffffffffffffffffdffffffffffffffffffffffdffffffffffffffffffffaadffffKDDffffffDDKffffffs'
  '.sdfffffffffffffffffvccffdffffffffffffffffffffffdffffffffffffffffffffffdfffffaafffKDDffffDDKffdffffs'
  'sffffffffffffffffffvcccxfffffffffffffffffffdffffffffffffffffffffffdfdfffdfdffffffffKDDffDDKfffffffs.'
  'sffffffffvccfffdfffcccxxffffffffffffffdffffffffffffffffffffffdfffdfffffffffdffffffffKDDDDKfffffffffs'
  'sfffffffvcccxfffffffxxxffffffffffdfffffffvccffffffffffffdfffffffdfffffffffffdffdfffffKDDKfffffffffs.'
  'sffffdffcccxxffffffffaffffffdfffffffffffvcccxffffffdffffffffffffffffffffffdffffffffffDDDDffffffffdfs'
  'sffffffffxxxfffffffffafdfffffffvccffffffcccxxfdfffffffffffffffffdffffdffffffdfffffffDDKKDDffdffffffs'
  'sfffffffffafffffffdfffffffffffvcccxffffffxxxffffffffffffffffffffdffffffffffffffffffDDKfdKDDffffffffs'
  '.sffffffffaffdffffffffffffffffcccxxfdfffffaffffffffffffffffdffffdfffffffffffdfffffDDKffffKDDfffffffs'
  'ssffffffdffffffffffffffffffffffxxxffffffffafffffffffffdffffffffffdfffffffffdfdfffffKffffffKffffffffs'
  '..sdffffffffffffffffffffffdfffffaffffffffffffffffdffffffffffffffffdfdfffdfdffffffffffffffffffffdfffs'
  'ssfffffffffffffffffffdffffffffffafffffffffffdffffffffffffffffffffffdffffffffffffffffffffffdfffffffs.'
  'sfffffffffffffffdffffffffffffffffffffffdffffffffffffffffffffffdffffffffffffffffffffffdffffffffffffss'
  'sffffffffffdfsfffffffffffffffffsffdffffffffffffffsfffffffdfffffffffsffffffffffffdffffsfffffffffffs..'
  '.ssfsfdffffss.sfffffsfsffffffss.sfffffsfsffffffss.sfdfffsfsffffffss.sfffffsdsffffffss.sfffffsfsfffs.'
  '...s.ssssss.s.ssssss.s.ssssss.s.ssssss.s.ssssss.s.ssssss.s.ssssss.s.ssssss.s.ssssss.s.ssssss.s.sss..'
)
$mapFull = Draw-Grid $mapRows
if ($mapFull.Width -ne 100 -or $mapFull.Height -ne 56) { throw "M127 map grid is $($mapFull.Width)x$($mapFull.Height); must be 100x56." }
# The two tears: a column offset per row (period 8) and a row offset per column
# (period 10). Piece = (right of the vertical tear) + 2 * (below the horizontal).
$tearV = 0, 1, 2, 1, 0, -1, -2, -1
$tearH = 0, 1, 1, 2, 1, 0, -1, -1, -2, -1
function MapPieceOf([int]$x, [int]$y) {
  $p = 0
  if ($x -ge (50 + $tearV[$y % 8])) { $p += 1 }
  if ($y -ge (28 + $tearH[$x % 10])) { $p += 2 }
  return $p
}
$parchA = (C $PAL.earth4).ToArgb(); $parchB = (C $PAL.earth3).ToArgb(); $crease = C $PAL.earth3
$pieceNames = 'tl', 'tr', 'bl', 'br'
for ($piece = 0; $piece -lt 4; $piece++) {
  $b = New-Img 100 56
  for ($y = 0; $y -lt 56; $y++) { for ($x = 0; $x -lt 100; $x++) {
    $px = $mapFull.GetPixel($x, $y)
    if ($px.A -eq 0 -or (MapPieceOf $x $y) -ne $piece) { continue }
    $argb = $px.ToArgb()
    if ($argb -eq $parchA -or $argb -eq $parchB) {
      foreach ($d in @(@(-1,0),@(1,0),@(0,-1),@(0,1))) {
        $nx = $x + $d[0]; $ny = $y + $d[1]
        if ($nx -ge 0 -and $ny -ge 0 -and $nx -lt 100 -and $ny -lt 56 -and (MapPieceOf $nx $ny) -ne $piece) { $px = $crease; break }
      }
    }
    $b.SetPixel($x, $y, $px)
  } }
  SaveImg $b "ui/map/piece_$($pieceNames[$piece]).png"
}
$mapFull.Dispose()

# ============================ M128 tile redesign ============================
# The town ladder's trees - four hand-placed 16x16 variants per town, placed by
# the engine's position hash (render/TileVariant.hpp) - the seasonal ground
# (four layouts per town) and four wall variants per dungeon theme. Environment
# tiles are opaque and get NO outline pass (they tile edge to edge). RNG-free:
# every pixel is a grid character, so nothing here can shift another file's
# bytes. Grid keys added for the M128 ramps: j/k/l gold-leaf, 4/5/6 ember,
# 7/8/9 crimson-leaf, 0 bark shadow (registered with the palette key table
# above, so every grid section can use them). Authored in a PIL prototype and emitted
# from the same data - edit the grids here, never the PNGs.
Write-Output 'Generating M128 tiles (town trees, seasonal ground, wall variants)...'
function Save-TileGrid([string]$name, [string[]]$rows) {
  $b = Draw-Grid $rows
  if ($b.Width -ne 16 -or $b.Height -ne 16) {
    throw "Save-TileGrid: tile '$name' is $($b.Width)x$($b.Height); must be 16x16."
  }
  for ($y = 0; $y -lt 16; $y++) { for ($x = 0; $x -lt 16; $x++) {
    if ($b.GetPixel($x, $y).A -ne 255) { throw "Save-TileGrid: tile '$name' has a transparent pixel at $x,$y; tiles are opaque." } } }
  SaveImg $b "environments/$name.png"
}

Save-TileGrid 'town1_tree1' @(
  'aaaaaaaaaaaaaaaa'
  'aaaaazzzzzaaaaaa'
  'aazzzvvccczzaaaa'
  'aazvvvcccccczaza'
  'aazvvcccccccxzaa'
  'azvcccccccccxzaa'
  'azcccccccccxxzaa'
  'azcccccccxxxxzaa'
  'aazcccccxxxxzaaa'
  'aazzcccxxxxzzaaa'
  'aaazzzxxxzzzaaaa'
  'aaaaaz0dszaaaaaa'
  'aaaaaaadsaaaazaa'
  'azaaaaadsaaaaaaa'
  'aaaaaa0dss0aaaaa'
  'aaaazaaaaaaazaaa'
)

Save-TileGrid 'town1_tree2' @(
  'aaaaaaazzaaaaaaa'
  'aaazaazvvzaaaaaa'
  'aaaaazvvcczazaaa'
  'aaaaazvccczaaaaa'
  'aaaazvccccxzaaaa'
  'aazazcccccxzaaaa'
  'aaaazcccccxzaaaa'
  'aaaazccccxxzaaaa'
  'aaaazcccxxxzaaaa'
  'aaaaazcxxxzaazaa'
  'aaaaazzxxzzaaaaa'
  'aaaaaazdszaaaaaa'
  'aaaaaaadsaaaaaaa'
  'aaaaaaadsaaaaaaa'
  'azaaaa0ds0aaaaza'
  'aaaaaaaaaaaaaaaa'
)

Save-TileGrid 'town1_tree3' @(
  'aaaaaaaaaaaaaaaa'
  'azaaaaaazzzzaaza'
  'aaaaazzzvvcczaaa'
  'aaazzvvcccccczaa'
  'aazvvccccccccxza'
  'aazcccccccccxxza'
  'aazcccccccxxxxza'
  'aaazcccccxxxxzaa'
  'aaaazzccxxxzzaaa'
  'aaaaaazxxxzaaaaa'
  'aaaaaaazdszaaaaa'
  'aaaaaaaadsaaaaza'
  'azaaaaaadsaaaaaa'
  'aaaaaaa0ds0aaaaa'
  'aaaaaaaaaaaaazaa'
  'aazaaaaaaaaaaaaa'
)

Save-TileGrid 'town1_tree4' @(
  'aaaaaaaaaaaaaaaa'
  'aazzzaaaaaazzzaa'
  'azvvczaaaazvccza'
  'azvccczaazcccxza'
  'azccccczzzcccxza'
  'aazcccccccccxzaa'
  'aazcccccccxxxzaa'
  'aaazcccccxxxzaaa'
  'zaaazzccxxxzzaaa'
  'aaaaaazxxxzaaaaa'
  'aaaaaaz0dszaaaaa'
  'aaaaaaadsaaaaaaa'
  'azaaaaadsaaaaaza'
  'aaaaaa0dss0aaaaa'
  'aaaaaaaaaaaaaaaa'
  'aazaaaazaaaaazaa'
)

Save-TileGrid 'town1_ground1' @(
  'aaaaaaaaaaaaaaaa'
  'aaaaaaaaaaazaaaa'
  'aaaaaaaaaaaaaaaa'
  'aazaaaaaaaaaaaaa'
  'aaaaaaaaaaaaaaaa'
  'aaaaaaaaaaaaaaaa'
  'aaaaaaaaaaaaaaaa'
  'aaaaaazaaaaaaaaa'
  'aaaaaaaaaaaaaaaa'
  'aaaaaaaaaaaaaaaa'
  'aaaaaaaaaaaaaaza'
  'aaaasaaaaaaaaaaa'
  'aaaaaaaaaaaaaaaa'
  'aaaaaaaaazaaaaaa'
  'aaaaaaaaaaaaaaaa'
  'aaaaaaaaaaaaaaaa'
)

Save-TileGrid 'town1_ground2' @(
  'aaaaaaaaaaaaaaaa'
  'aaaaaaaaaaaaaaaa'
  'aaaaaaaaaaaaaaaa'
  'aaaaaaaaaaaaazaa'
  'aaaaaaaaaaaaaaaa'
  'aaaaaaaazaaaaaaa'
  'azaaaaaaaaaaaaaa'
  'aaaaaaaaaaaaaaaa'
  'aaaaaaaaaaaaaaaa'
  'aaaaaaaaaasaaaaa'
  'aaaaaaaaaaaaaaaa'
  'aaaaaaaaaaaaaaaa'
  'aaaaazaaaaaaaaaa'
  'aaaaaaaaaaaaaaaa'
  'aaaaaaaaaaaazaaa'
  'aaaaaaaaaaaaaaaa'
)

Save-TileGrid 'town1_ground3' @(
  'aaaaaaaaaaaaaaaa'
  'aaaaaaaaaaaaaaaa'
  'aazaaaaaaaaaaaaa'
  'aaaaaaaaaaaaaaaa'
  'aaaacaaaaaaaaaaa'
  'aaaxxcaaaaaaaaaa'
  'aaaxxaaaaaaazaaa'
  'aaaaaaaaaaaaaaaa'
  'aaaaaaaaaaaaaaaa'
  'aaaaaaaaaaacaaaa'
  'aaaaaaaaaaxxcaaa'
  'aaaaaaaaaaxxaaaa'
  'aaaaaaaaaaaaaasa'
  'aaaaaaazaaaaaaaa'
  'aaaaaaaaaaaaaaaa'
  'aaaaaaaaaaaaaaaa'
)

Save-TileGrid 'town1_ground4' @(
  'aaaaaaaaaaaaaaaa'
  'aaaaaaaaaaaaaaaa'
  'aaaaaaaaaaaaazaa'
  'aaaaaaaaaacaaaaa'
  'aaaaaaaaaxxcaaaa'
  'aaaaaaaaaxxaaaaa'
  'aaaaaaaaaaaaaaaa'
  'aaaaaaaaaaaaaaaa'
  'aaaaaaaaaaaaaaaa'
  'aaazaaaaaaaaaaaa'
  'aaaaacaaaaaaaaaa'
  'aaaaxxcaaaaaaaaa'
  'aaaaxxaaaaaaaaaa'
  'asaaaaaaaaaaaaaa'
  'aaaaaaaaaaaaaaaa'
  'aaaaaaaaazaaaaaa'
)

Save-TileGrid 'town2_tree1' @(
  'aaaaaaaaaaaaaaaa'
  'aaaaajjjjjaaaaaa'
  'aajjjllkkkjjaaaa'
  'aajlllkkkkkkjaja'
  'aajllkkkkkkkjjaa'
  'ajlkkkkkkkkkjjaa'
  'ajkkkkkkkkkjjjaa'
  'ajkkkkkkkjjjjjaa'
  'aajkkkkkjjjjjaaa'
  'aajjkkkjjjjjjaaa'
  'aaajjjjjjjjjaaaa'
  'aaaaaj0dsjaaaaaa'
  'aaaaaaadsaaaajaa'
  'ajaaaaadsaaaaaaa'
  'aaaaaa0dss0aaaaa'
  'aaaajaaaaaaajaaa'
)

Save-TileGrid 'town2_tree2' @(
  'aaaaaaajjaaaaaaa'
  'aaajaajlljaaaaaa'
  'aaaaajllkkjajaaa'
  'aaaaajlkkkjaaaaa'
  'aaaajlkkkkjjaaaa'
  'aajajkkkkkjjaaaa'
  'aaaajkkkkkjjaaaa'
  'aaaajkkkkjjjaaaa'
  'aaaajkkkjjjjaaaa'
  'aaaaajkjjjjaajaa'
  'aaaaajjjjjjaaaaa'
  'aaaaaajdsjaaaaaa'
  'aaaaaaadsaaaaaaa'
  'aaaaaaadsaaaaaaa'
  'ajaaaa0ds0aaaaja'
  'aaaaaaaaaaaaaaaa'
)

Save-TileGrid 'town2_tree3' @(
  'aaaaaaaaaaaaaaaa'
  'ajaaaaaajjjjaaja'
  'aaaaajjjllkkjaaa'
  'aaajjllkkkkkkjaa'
  'aajllkkkkkkkkjja'
  'aajkkkkkkkkkjjja'
  'aajkkkkkkkjjjjja'
  'aaajkkkkkjjjjjaa'
  'aaaajjkkjjjjjaaa'
  'aaaaaajjjjjaaaaa'
  'aaaaaaajdsjaaaaa'
  'aaaaaaaadsaaaaja'
  'ajaaaaaadsaaaaaa'
  'aaaaaaa0ds0aaaaa'
  'aaaaaaaaaaaaajaa'
  'aajaaaaaaaaaaaaa'
)

Save-TileGrid 'town2_tree4' @(
  'aaaaaaaaaaaaaaaa'
  'aajjjaaaaaajjjaa'
  'ajllkjaaaajlkkja'
  'ajlkkkjaajkkkjja'
  'ajkkkkkjjjkkkjja'
  'aajkkkkkkkkkjjaa'
  'aajkkkkkkkjjjjaa'
  'aaajkkkkkjjjjaaa'
  'jaaajjkkjjjjjaaa'
  'aaaaaajjjjjaaaaa'
  'aaaaaaj0dsjaaaaa'
  'aaaaaaadsaaaaaaa'
  'ajaaaaadsaaaaaja'
  'aaaaaa0dss0aaaaa'
  'aaaaaaaaaaaaaaaa'
  'aajaaaajaaaaajaa'
)

Save-TileGrid 'town2_ground1' @(
  'aaaaaaaaaaaaaaaa'
  'aaaaaaaaaaajaaaa'
  'aaaaaaaaaaaaaaaa'
  'aajaaaaaaaaaaaaa'
  'aaaaaaaaaaaaaaaa'
  'aaaaaaaaaaaaaaaa'
  'aaaaaaaaaaaaaaaa'
  'aaaaaajaaaaaaaaa'
  'aaaaaaaaaaaaaaaa'
  'aaaaaaaaaaaaaaaa'
  'aaaaaaaaaaaaaaja'
  'aaaasaaaaaaaaaaa'
  'aaaaaaaaaaaaaaaa'
  'aaaaaaaaajaaaaaa'
  'aaaaaaaaaaaaaaaa'
  'aaaaaaaaaaaaaaaa'
)

Save-TileGrid 'town2_ground2' @(
  'aaaaaaaaaaaaaaaa'
  'aaaaaaaaaaaaaaaa'
  'aaaaaaaaaaaaaaaa'
  'aaaaaaaaaaaaajaa'
  'aaaaaaaaaaaaaaaa'
  'aaaaaaaajaaaaaaa'
  'ajaaaaaaaaaaaaaa'
  'aaaaaaaaaaaaaaaa'
  'aaaaaaaaaaaaaaaa'
  'aaaaaaaaaasaaaaa'
  'aaaaaaaaaaaaaaaa'
  'aaaaaaaaaaaaaaaa'
  'aaaaajaaaaaaaaaa'
  'aaaaaaaaaaaaaaaa'
  'aaaaaaaaaaaajaaa'
  'aaaaaaaaaaaaaaaa'
)

Save-TileGrid 'town2_ground3' @(
  'aaaaaaaaaaaaaaaa'
  'aaaaaaaaaaaaaaaa'
  'aajaaaaaaaaaaaaa'
  'aaaaaaaaaaaaaaaa'
  'aaaalaaaaaaaaaaa'
  'aaakklaaaaaaaaaa'
  'aaakkaaaaaaajaaa'
  'aaaaaaaaaaaaaaaa'
  'aaaaaaaaaaaaaaaa'
  'aaaaaaaaaaalaaaa'
  'aaaaaaaaaakklaaa'
  'aaaaaaaaaakkaaaa'
  'aaaaaaaaaaaaaasa'
  'aaaaaaajaaaaaaaa'
  'aaaaaaaaaaaaaaaa'
  'aaaaaaaaaaaaaaaa'
)

Save-TileGrid 'town2_ground4' @(
  'aaaaaaaaaaaaaaaa'
  'aaaaaaaaaaaaaaaa'
  'aaaaaaaaaaaaajaa'
  'aaaaaaaaaalaaaaa'
  'aaaaaaaaakklaaaa'
  'aaaaaaaaakkaaaaa'
  'aaaaaaaaaaaaaaaa'
  'aaaaaaaaaaaaaaaa'
  'aaaaaaaaaaaaaaaa'
  'aaajaaaaaaaaaaaa'
  'aaaaalaaaaaaaaaa'
  'aaaakklaaaaaaaaa'
  'aaaakkaaaaaaaaaa'
  'asaaaaaaaaaaaaaa'
  'aaaaaaaaaaaaaaaa'
  'aaaaaaaaajaaaaaa'
)

Save-TileGrid 'keep_wall1' @(
  'wwwwwwwwwwwwwwww'
  'qq2wqqqqqq2wqqqq'
  'qq2qqqqqqq2qqqqq'
  '2222222222222222'
  'wqqqqq2wqqqqqq2w'
  'qqqqqq2qqqqqqq2q'
  'qq3qqq2qqqqq3q2q'
  '2222222222222222'
  'qq2wqqqqqq2wqqqq'
  'qq2qqqqqqq2qqqqq'
  'qq2qqq3qqq2qqqqq'
  '2222222222222222'
  'wqqqqq2wqqqqqq2w'
  'qqqqqq2qqqqqqq2q'
  'qqqq3q2qqqqqqq2q'
  '2222222222222222'
)

Save-TileGrid 'keep_wall2' @(
  'wwwwwwwwwwwwwwww'
  'qq2wq1qqqq2wqqqq'
  'qq2q31qqqq2qqqqq'
  '2222221222222222'
  'wqqqqq1wqqqqqq2w'
  'qqqqqq31qqqqqq2q'
  'qq3qqq21qqqq3q2q'
  '2222222212222222'
  'qq2wqqqq1q2wqqqq'
  'qq2qqqqq312qqqqq'
  'qq2qqq3qq12qqqqq'
  '2222222222122222'
  'wqqqqq2wqqqeeq2w'
  'qqqqqq2qqqqwq32q'
  'qqqq3q2qqqqqq32q'
  '2222222222222222'
)

Save-TileGrid 'keep_wall3' @(
  'wwwwwwwwwwwwwwww'
  'qq2wqqqqqq2wqqqq'
  'qq2qqqqeeq2qqqqq'
  '222222e113222222'
  'wqqqqqe11qqqqq2w'
  'qqqqqqe11qqqqq2q'
  'qq3qqqe11qqq3q2q'
  '222222e112222222'
  'qq2wqqe11q2wqqqq'
  'qq2qqqe11q2qqqqq'
  'qq2qqqe11q2qqqqq'
  '222222e113222222'
  'wqqqqqwwwwqqqq2w'
  'qqqqqq2qqqqqqq2q'
  'qqqq3q2qqqqqqq2q'
  '2222222222222222'
)

Save-TileGrid 'keep_wall4' @(
  'wwwwwwwwwwwwwwww'
  'qq2wqqqqqq2wqqqq'
  'qq2qqqxqvq2qqqqq'
  '2222222zxx222222'
  'wqqqqxzcxqqqqq2w'
  'qqqqxczxcqqqqq2q'
  'qq3qqzvqqqqq3q2q'
  '22222zc222222222'
  'qq2xzcxqqq2wqqqq'
  'qqxqzxqqqq2qqqqq'
  'qq2zvq3qqq2qqqqq'
  '222zc22222222222'
  'wvzcxq2wqqqqqq2w'
  'xczxqq2qqqqqqq2q'
  'xzczzq2qqqqqqq2q'
  '2zz2222222222222'
)

Save-TileGrid 'town3_tree1' @(
  'aaaaaaaaaaaaaaaa'
  'aaaaa44444aaaaaa'
  'aa4446655544aaaa'
  'aa46665555554a4a'
  'aa466555555544aa'
  'a4655555555544aa'
  'a4555555555444aa'
  'a4555555544444aa'
  'aa45555544444aaa'
  'aa44555444444aaa'
  'aaa444444444aaaa'
  'aaaaa40ds4aaaaaa'
  'aaaaaaadsaaaa4aa'
  'a4aaaaadsaaaaaaa'
  'aaaaaa0dss0aaaaa'
  'aaaa4aaaaaaa4aaa'
)

Save-TileGrid 'town3_tree2' @(
  'aaaaaaa44aaaaaaa'
  'aaa4aa4664aaaaaa'
  'aaaaa466554a4aaa'
  'aaaaa465554aaaaa'
  'aaaa46555544aaaa'
  'aa4a45555544aaaa'
  'aaaa45555544aaaa'
  'aaaa45555444aaaa'
  'aaaa45554444aaaa'
  'aaaaa454444aa4aa'
  'aaaaa444444aaaaa'
  'aaaaaa4ds4aaaaaa'
  'aaaaaaadsaaaaaaa'
  'aaaaaaadsaaaaaaa'
  'a4aaaa0ds0aaaa4a'
  'aaaaaaaaaaaaaaaa'
)

Save-TileGrid 'town3_tree3' @(
  'aaaaaaaaaaaaaaaa'
  'a4aaaaaa4444aa4a'
  'aaaaa44466554aaa'
  'aaa44665555554aa'
  'aa4665555555544a'
  'aa4555555555444a'
  'aa4555555544444a'
  'aaa45555544444aa'
  'aaaa445544444aaa'
  'aaaaaa44444aaaaa'
  'aaaaaaa4ds4aaaaa'
  'aaaaaaaadsaaaa4a'
  'a4aaaaaadsaaaaaa'
  'aaaaaaa0ds0aaaaa'
  'aaaaaaaaaaaaa4aa'
  'aa4aaaaaaaaaaaaa'
)

Save-TileGrid 'town3_tree4' @(
  'aaaaaaaaaaaaaaaa'
  'aa444aaaaaa444aa'
  'a46654aaaa46554a'
  'a465554aa455544a'
  'a45555544455544a'
  'aa455555555544aa'
  'aa455555554444aa'
  'aaa4555554444aaa'
  '4aaa445544444aaa'
  'aaaaaa44444aaaaa'
  'aaaaaa40ds4aaaaa'
  'aaaaaaadsaaaaaaa'
  'a4aaaaadsaaaaa4a'
  'aaaaaa0dss0aaaaa'
  'aaaaaaaaaaaaaaaa'
  'aa4aaaa4aaaaa4aa'
)

Save-TileGrid 'town4_tree1' @(
  'aaaaaaaaaaaaaaaa'
  'aaaaa77777aaaaaa'
  'aa7779988877aaaa'
  'aa79998888887a7a'
  'aa799888888877aa'
  'a7988888888877aa'
  'a7888888888777aa'
  'a7888888877777aa'
  'aa78888877777aaa'
  'aa77888777777aaa'
  'aaa777777777aaaa'
  'aaaaa70ds7aaaaaa'
  'aaaaaaadsaaaa7aa'
  'a7aaaaadsaaaaaaa'
  'aaaaaa0dss0aaaaa'
  'aaaa7aaaaaaa7aaa'
)

Save-TileGrid 'town4_tree2' @(
  'aaaaaaa77aaaaaaa'
  'aaa7aa7997aaaaaa'
  'aaaaa799887a7aaa'
  'aaaaa798887aaaaa'
  'aaaa79888877aaaa'
  'aa7a78888877aaaa'
  'aaaa78888877aaaa'
  'aaaa78888777aaaa'
  'aaaa78887777aaaa'
  'aaaaa787777aa7aa'
  'aaaaa777777aaaaa'
  'aaaaaa7ds7aaaaaa'
  'aaaaaaadsaaaaaaa'
  'aaaaaaadsaaaaaaa'
  'a7aaaa0ds0aaaa7a'
  'aaaaaaaaaaaaaaaa'
)

Save-TileGrid 'town4_tree3' @(
  'aaaaaaaaaaaaaaaa'
  'a7aaaaaa7777aa7a'
  'aaaaa77799887aaa'
  'aaa77998888887aa'
  'aa7998888888877a'
  'aa7888888888777a'
  'aa7888888877777a'
  'aaa78888877777aa'
  'aaaa778877777aaa'
  'aaaaaa77777aaaaa'
  'aaaaaaa7ds7aaaaa'
  'aaaaaaaadsaaaa7a'
  'a7aaaaaadsaaaaaa'
  'aaaaaaa0ds0aaaaa'
  'aaaaaaaaaaaaa7aa'
  'aa7aaaaaaaaaaaaa'
)

Save-TileGrid 'town4_tree4' @(
  'aaaaaaaaaaaaaaaa'
  'aa777aaaaaa777aa'
  'a79987aaaa79887a'
  'a798887aa788877a'
  'a78888877788877a'
  'aa788888888877aa'
  'aa788888887777aa'
  'aaa7888887777aaa'
  '7aaa778877777aaa'
  'aaaaaa77777aaaaa'
  'aaaaaa70ds7aaaaa'
  'aaaaaaadsaaaaaaa'
  'a7aaaaadsaaaaa7a'
  'aaaaaa0dss0aaaaa'
  'aaaaaaaaaaaaaaaa'
  'aa7aaaa7aaaaa7aa'
)

Save-TileGrid 'town5_tree1' @(
  'qqqqqqqqqqqqqqqq'
  'qqqqqqqqqqqqqqqq'
  'qqZqqWWSSSqqqqqq'
  'qqqWWWSSSSSSqqZq'
  'qqqWWSSSSSSSZqqq'
  'qqWSSSSSSSSSZqqq'
  'qqSSSSSSSSSZZqqq'
  'qqSSSSSSSZZZZqqq'
  'qqqSSSSSZZZZqqqq'
  'qqqqSSSZZZZqqqqq'
  'qqqqqqZZZqqqqqqq'
  'qqqqqq0dsqqqqqqq'
  'qqqqqqqdsqqqqZqq'
  'qZqqqqqdsqqqqqqq'
  'qqqqqq0dss0qqqqq'
  'qqqqZqqqqqqqZqqq'
)

Save-TileGrid 'town5_tree2' @(
  'qqqqqqqqqqqqqqqq'
  'qqqZqqqWWqqqqqqq'
  'qqqqqqWWSSqqZqqq'
  'qqqqqqWSSSqqqqqq'
  'qqqqqWSSSSZqqqqq'
  'qqZqqSSSSSZqqqqq'
  'qqqqqSSSSSZqqqqq'
  'qqqqqSSSSZZqqqqq'
  'qqqqqSSSZZZqqqqq'
  'qqqqqqSZZZqqqZqq'
  'qqqqqqqZZqqqqqqq'
  'qqqqqqqdsqqqqqqq'
  'qqqqqqqdsqqqqqqq'
  'qqqqqqqdsqqqqqqq'
  'qZqqqq0ds0qqqqZq'
  'qqqqqqqqqqqqqqqq'
)

Save-TileGrid 'town5_tree3' @(
  'qqqqqqqqqqqqqqqq'
  'qZqqqqqqqqqqqqZq'
  'qqqqqqqqWWSSqqqq'
  'qqqqqWWSSSSSSqqq'
  'qqqWWSSSSSSSSZqq'
  'qqqSSSSSSSSSZZqq'
  'qqqSSSSSSSZZZZqq'
  'qqqqSSSSSZZZZqqq'
  'qqqqqqSSZZZqqqqq'
  'qqqqqqqZZZqqqqqq'
  'qqqqqqqqdsqqqqqq'
  'qqqqqqqqdsqqqqZq'
  'qZqqqqqqdsqqqqqq'
  'qqqqqqq0ds0qqqqq'
  'qqqqqqqqqqqqqZqq'
  'qqZqqqqqqqqqqqqq'
)

Save-TileGrid 'town5_tree4' @(
  'qqqqqqqqqqqqqqqq'
  'qqqqqqqqqqqqqqqq'
  'qqWWSqqqqqqWSSqq'
  'qqWSSSqqqqSSSZqq'
  'qqSSSSSqqqSSSZqq'
  'qqqSSSSSSSSSZqqq'
  'qqqSSSSSSSZZZqqq'
  'qqqqSSSSSZZZqqqq'
  'ZqqqqqSSZZZqqqqq'
  'qqqqqqqZZZqqqqqq'
  'qqqqqqq0dsqqqqqq'
  'qqqqqqqdsqqqqqqq'
  'qZqqqqqdsqqqqqZq'
  'qqqqqq0dss0qqqqq'
  'qqqqqqqqqqqqqqqq'
  'qqZqqqqZqqqqqZqq'
)

Save-TileGrid 'town3_ground1' @(
  'aaaaaaaaaaaaaaaa'
  'aaaaaaaaaaa4aaaa'
  'aaaaaaaaaaaaaaaa'
  'aa4aaaaaaaaaaaaa'
  'aaaaaaaaaaaaaaaa'
  'aaaaaaaaaaaaaaaa'
  'aaaaaaaaaaaaaaaa'
  'aaaaaa4aaaaaaaaa'
  'aaaaaaaaaaaaaaaa'
  'aaaaaaaaaaaaaaaa'
  'aaaaaaaaaaaaaa4a'
  'aaaasaaaaaaaaaaa'
  'aaaaaaaaaaaaaaaa'
  'aaaaaaaaa4aaaaaa'
  'aaaaaaaaaaaaaaaa'
  'aaaaaaaaaaaaaaaa'
)

Save-TileGrid 'town3_ground2' @(
  'aaaaaaaaaaaaaaaa'
  'aaaaaaaaaaaaaaaa'
  'aaaaaaaaaaaaaaaa'
  'aaaaaaaaaaaaa4aa'
  'aaaaaaaaaaaaaaaa'
  'aaaaaaaa4aaaaaaa'
  'a4aaaaaaaaaaaaaa'
  'aaaaaaaaaaaaaaaa'
  'aaaaaaaaaaaaaaaa'
  'aaaaaaaaaasaaaaa'
  'aaaaaaaaaaaaaaaa'
  'aaaaaaaaaaaaaaaa'
  'aaaaa4aaaaaaaaaa'
  'aaaaaaaaaaaaaaaa'
  'aaaaaaaaaaaa4aaa'
  'aaaaaaaaaaaaaaaa'
)

Save-TileGrid 'town3_ground3' @(
  'aaaaaaaaaaaaaaaa'
  'aaaaaaaaaaaaaaaa'
  'aa4aaaaaaaaaaaaa'
  'aaaaaaaaaaaaaaaa'
  'aaaa6aaaaaaaaaaa'
  'aaaff6aaaaaaaaaa'
  'aaaffaaaaaaa4aaa'
  'aaaaaaaaaaaaaaaa'
  'aaaaaaaaaaaaaaaa'
  'aaaaaaaaaaa6aaaa'
  'aaaaaaaaaaff6aaa'
  'aaaaaaaaaaffaaaa'
  'aaaaaaaaaaaaaasa'
  'aaaaaaa4aaaaaaaa'
  'aaaaaaaaaaaaaaaa'
  'aaaaaaaaaaaaaaaa'
)

Save-TileGrid 'town3_ground4' @(
  'aaaaaaaaaaaaaaaa'
  'aaaaaaaaaaaaaaaa'
  'aaaaaaaaaaaaa4aa'
  'aaaaaaaaaa6aaaaa'
  'aaaaaaaaaff6aaaa'
  'aaaaaaaaaffaaaaa'
  'aaaaaaaaaaaaaaaa'
  'aaaaaaaaaaaaaaaa'
  'aaaaaaaaaaaaaaaa'
  'aaa4aaaaaaaaaaaa'
  'aaaaa6aaaaaaaaaa'
  'aaaaff6aaaaaaaaa'
  'aaaaffaaaaaaaaaa'
  'asaaaaaaaaaaaaaa'
  'aaaaaaaaaaaaaaaa'
  'aaaaaaaaa4aaaaaa'
)

Save-TileGrid 'town4_ground1' @(
  'aaaaaaaaaaaaaaaa'
  'aaaaaaaaaaa7aaaa'
  'aaaaaaaaaaaaaaaa'
  'aa7aaaaaaaaaaaaa'
  'aaaaaaaaaaaaaaaa'
  'aaaaaaaaaaaaaaaa'
  'aaaaaaaaaaaaaaaa'
  'aaaaaa7aaaaaaaaa'
  'aaaaaaaaaaaaaaaa'
  'aaaaaaaaaaaaaaaa'
  'aaaaaaaaaaaaaa7a'
  'aaaa0aaaaaaaaaaa'
  'aaaaaaaaaaaaaaaa'
  'aaaaaaaaa7aaaaaa'
  'aaaaaaaaaaaaaaaa'
  'aaaaaaaaaaaaaaaa'
)

Save-TileGrid 'town4_ground2' @(
  'aaaaaaaaaaaaaaaa'
  'aaaaaaaaaaaaaaaa'
  'aaaaaaaaaaaaaaaa'
  'aaaaaaaaaaaaa7aa'
  'aaaaaaaaaaaaaaaa'
  'aaaaaaaa7aaaaaaa'
  'a7aaaaaaaaaaaaaa'
  'aaaaaaaaaaaaaaaa'
  'aaaaaaaaaaaaaaaa'
  'aaaaaaaaaa0aaaaa'
  'aaaaaaaaaaaaaaaa'
  'aaaaaaaaaaaaaaaa'
  'aaaaa7aaaaaaaaaa'
  'aaaaaaaaaaaaaaaa'
  'aaaaaaaaaaaa7aaa'
  'aaaaaaaaaaaaaaaa'
)

Save-TileGrid 'town4_ground3' @(
  'aaaaaaaaaaaaaaaa'
  'aaaaaaaaaaaaaaaa'
  'aa7aaaaaaaaaaaaa'
  'aaaaaaaaaaaaaaaa'
  'aaaa8aaaaaaaaaaa'
  'aaadd8aaaaaaaaaa'
  'aaaddaaaaaaa7aaa'
  'aaaaaaaaaaaaaaaa'
  'aaaaaaaaaaaaaaaa'
  'aaaaaaaaaaa8aaaa'
  'aaaaaaaaaadd8aaa'
  'aaaaaaaaaaddaaaa'
  'aaaaaaaaaaaaaa0a'
  'aaaaaaa7aaaaaaaa'
  'aaaaaaaaaaaaaaaa'
  'aaaaaaaaaaaaaaaa'
)

Save-TileGrid 'town4_ground4' @(
  'aaaaaaaaaaaaaaaa'
  'aaaaaaaaaaaaaaaa'
  'aaaaaaaaaaaaa7aa'
  'aaaaaaaaaa8aaaaa'
  'aaaaaaaaadd8aaaa'
  'aaaaaaaaaddaaaaa'
  'aaaaaaaaaaaaaaaa'
  'aaaaaaaaaaaaaaaa'
  'aaaaaaaaaaaaaaaa'
  'aaa7aaaaaaaaaaaa'
  'aaaaa8aaaaaaaaaa'
  'aaaadd8aaaaaaaaa'
  'aaaaddaaaaaaaaaa'
  'a0aaaaaaaaaaaaaa'
  'aaaaaaaaaaaaaaaa'
  'aaaaaaaaa7aaaaaa'
)

Save-TileGrid 'town5_ground1' @(
  'qqqqqqqqqqqqqqqq'
  'qqqqqqqqqqqZqqqq'
  'qqqqqqqqqqqqqqqq'
  'qqZqqqqqqqqqqqqq'
  'qqqqqqqqqqqqqqqq'
  'qqqqqqqqqqqqqqqq'
  'qqqqqqqqqqqqqqqq'
  'qqqqqqZqqqqqqqqq'
  'qqqqqqqqqqqqqqqq'
  'qqqqqqqqqqqqqqqq'
  'qqqqqqqqqqqqqqZq'
  'qqqqwqqqqqqqqqqq'
  'qqqqqqqqqqqqqqqq'
  'qqqqqqqqqZqqqqqq'
  'qqqqqqqqqqqqqqqq'
  'qqqqqqqqqqqqqqqq'
)

Save-TileGrid 'town5_ground2' @(
  'qqqqqqqqqqqqqqqq'
  'qqqqqqqqqqqqqqqq'
  'qqqqqqqqqqqqqqqq'
  'qqqqqqqqqqqqqZqq'
  'qqqqqqqqqqqqqqqq'
  'qqqqqqqqZqqqqqqq'
  'qZqqqqqqqqqqqqqq'
  'qqqqqqqqqqqqqqqq'
  'qqqqqqqqqqqqqqqq'
  'qqqqqqqqqqwqqqqq'
  'qqqqqqqqqqqqqqqq'
  'qqqqqqqqqqqqqqqq'
  'qqqqqZqqqqqqqqqq'
  'qqqqqqqqqqqqqqqq'
  'qqqqqqqqqqqqZqqq'
  'qqqqqqqqqqqqqqqq'
)

Save-TileGrid 'town5_ground3' @(
  'qqqqqqqqqqqqqqqq'
  'qqqqqqqqqqqqqqqq'
  'qqZqqqqqqqqqqqqq'
  'qqqqqqqqqqqqqqqq'
  'qqqqWqqqqqqqqqqq'
  'qqqSSWqqqqqqqqqq'
  'qqqSSqqqqqqqZqqq'
  'qqqqqqqqqqqqqqqq'
  'qqqqqqqqqqqqqqqq'
  'qqqqqqqqqqqWqqqq'
  'qqqqqqqqqqSSWqqq'
  'qqqqqqqqqqSSqqqq'
  'qqqqqqqqqqqqqqwq'
  'qqqqqqqZqqqqqqqq'
  'qqqqqqqqqqqqqqqq'
  'qqqqqqqqqqqqqqqq'
)

Save-TileGrid 'town5_ground4' @(
  'qqqqqqqqqqqqqqqq'
  'qqqqqqqqqqqqqqqq'
  'qqqqqqqqqqqqqZqq'
  'qqqqqqqqqqWqqqqq'
  'qqqqqqqqqSSWqqqq'
  'qqqqqqqqqSSqqqqq'
  'qqqqqqqqqqqqqqqq'
  'qqqqqqqqqqqqqqqq'
  'qqqqqqqqqqqqqqqq'
  'qqqZqqqqqqqqqqqq'
  'qqqqqWqqqqqqqqqq'
  'qqqqSSWqqqqqqqqq'
  'qqqqSSqqqqqqqqqq'
  'qwqqqqqqqqqqqqqq'
  'qqqqqqqqqqqqqqqq'
  'qqqqqqqqqZqqqqqq'
)

Save-TileGrid 'town6_ground1' @(
  '0000000000000000'
  '00000000000a0000'
  '0000000000000000'
  '00a0000000000000'
  '0000000000000000'
  '0000000000000000'
  '0000000000000000'
  '000000a000000000'
  '0000000000000000'
  '0000000000000000'
  '00000000000000a0'
  '0000200000000000'
  '0000000000000000'
  '000000000a000000'
  '0000000000000000'
  '0000000000000000'
)

Save-TileGrid 'town6_ground2' @(
  '0000000000000000'
  '0000000000000000'
  '0000000000000000'
  '0000000000000a00'
  '0000000000000000'
  '00000000a0000000'
  '0a00000000000000'
  '0000000000000000'
  '0000000000000000'
  '0000000000200000'
  '0000000000000000'
  '0000000000000000'
  '00000a0000000000'
  '0000000000000000'
  '000000000000a000'
  '0000000000000000'
)

Save-TileGrid 'town6_ground3' @(
  '0000000000000000'
  '0000000000000000'
  '00a0000000000000'
  '0000000000000000'
  '0000s00000000000'
  '000aas0000000000'
  '000aa0000000a000'
  '0000000000000000'
  '0000000000000000'
  '00000000000s0000'
  '0000000000aas000'
  '0000000000aa0000'
  '0000000000000020'
  '0000000a00000000'
  '0000000000000000'
  '0000000000000000'
)

Save-TileGrid 'town6_ground4' @(
  '0000000000000000'
  '0000000000000000'
  '0000000000000a00'
  '0000000000s00000'
  '000000000aas0000'
  '000000000aa00000'
  '0000000000000000'
  '0000000000000000'
  '0000000000000000'
  '000a000000000000'
  '00000s0000000000'
  '0000aas000000000'
  '0000aa0000000000'
  '0200000000000000'
  '0000000000000000'
  '000000000a000000'
)

Save-TileGrid 'town7_ground1' @(
  'qqqqqqqqqqqqqqqq'
  'qqqqqqqqqqq3qqqq'
  'qqqqqqqqqqqqqqqq'
  'qq3qqqqqqqqqqqqq'
  'qqqqqqqqqqqqqqqq'
  'qqqqqqqqqqqqqqqq'
  'qqqqqqqqqqqqqqqq'
  'qqqqqq3qqqqqqqqq'
  'qqqqqqqqqqqqqqqq'
  'qqqqqqqqqqqqqqqq'
  'qqqqqqqqqqqqqq3q'
  'qqqqwqqqqqqqqqqq'
  'qqqqqqqqqqqqqqqq'
  'qqqqqqqqq3qqqqqq'
  'qqqqqqqqqqqqqqqq'
  'qqqqqqqqqqqqqqqq'
)

Save-TileGrid 'town7_ground2' @(
  'qqqqqqqqqqqqqqqq'
  'qqqqqqqqqqqqqqqq'
  'qqqqqqqqqqqqqqqq'
  'qqqqqqqqqqqqq3qq'
  'qqqqqqqqqqqqqqqq'
  'qqqqqqqq3qqqqqqq'
  'q3qqqqqqqqqqqqqq'
  'qqqqqqqqqqqqqqqq'
  'qqqqqqqqqqqqqqqq'
  'qqqqqqqqqqwqqqqq'
  'qqqqqqqqqqqqqqqq'
  'qqqqqqqqqqqqqqqq'
  'qqqqq3qqqqqqqqqq'
  'qqqqqqqqqqqqqqqq'
  'qqqqqqqqqqqq3qqq'
  'qqqqqqqqqqqqqqqq'
)

Save-TileGrid 'town7_ground3' @(
  'qqqqqqqqqqqqqqqq'
  'qqqqqqqqqqqqqqqq'
  'qq3qqqqqqqqqqqqq'
  'qqqqqqqqqqqqqqqq'
  'qqqqeqqqqqqqqqqq'
  'qqqwweqqqqqqqqqq'
  'qqqwwqqqqqqq3qqq'
  'qqqqqqqqqqqqqqqq'
  'qqqqqqqqqqqqqqqq'
  'qqqqqqqqqqqeqqqq'
  'qqqqqqqqqqwweqqq'
  'qqqqqqqqqqwwqqqq'
  'qqqqqqqqqqqqqqwq'
  'qqqqqqq3qqqqqqqq'
  'qqqqqqqqqqqqqqqq'
  'qqqqqqqqqqqqqqqq'
)

Save-TileGrid 'town7_ground4' @(
  'qqqqqqqqqqqqqqqq'
  'qqqqqqqqqqqqqqqq'
  'qqqqqqqqqqqqq3qq'
  'qqqqqqqqqqeqqqqq'
  'qqqqqqqqqwweqqqq'
  'qqqqqqqqqwwqqqqq'
  'qqqqqqqqqqqqqqqq'
  'qqqqqqqqqqqqqqqq'
  'qqqqqqqqqqqqqqqq'
  'qqq3qqqqqqqqqqqq'
  'qqqqqeqqqqqqqqqq'
  'qqqqwweqqqqqqqqq'
  'qqqqwwqqqqqqqqqq'
  'qwqqqqqqqqqqqqqq'
  'qqqqqqqqqqqqqqqq'
  'qqqqqqqqq3qqqqqq'
)

Save-TileGrid 'town6_tree1' @(
  '0000000000000000'
  '00a000000000a000'
  '00sa0000000as000'
  '000sa00000as0000'
  '0000sa000as00000'
  '00000sa0as000000'
  '000000sds0000000'
  '0000000ds0000000'
  '0000000ds0000a00'
  '000000ads0000000'
  '0000000ds0000000'
  '0a00000ds0000000'
  '000000adsa000000'
  '00000a0ds0a00000'
  '0000000000000000'
  '0000000000000000'
)

Save-TileGrid 'town6_tree2' @(
  '0000000000000000'
  '0a0000000000a000'
  '00sa000a000as000'
  '000sa00sa0as0000'
  '0000sa0dsas00000'
  '00000sadss000000'
  '000000dss0000000'
  '0000000ds0000000'
  '00a0000ds0000a00'
  '000sa00ds00as000'
  '0000sa0ds0as0000'
  '00000sadsas00000'
  '0000000ds0000000'
  '000000adsa00a000'
  '0a00000000000000'
  '0000000000000000'
)

Save-TileGrid 'town6_tree3' @(
  '0000000000000000'
  '0000000000a0a000'
  '000000000as0s000'
  '00000000as0s0000'
  '0000000dss000000'
  '00000a0ds0000000'
  '000000ads0000000'
  '0000000ds000000a'
  '000000ads0000000'
  '00000a0ds0000000'
  '0000000ds0000000'
  '000000ds00000000'
  '000000ds00000000'
  '00000adsa0000000'
  '0a00000000000000'
  '0000000000000000'
)

Save-TileGrid 'town6_tree4' @(
  '0000000000000000'
  '0000000000000000'
  '00000a0000000a00'
  '000000sa0000as00'
  '0000000sa00as000'
  '00000000sads0000'
  '00000000dss00000'
  '00000000ds000000'
  '0000000dss000000'
  '0000000dss000000'
  '000000ddss000000'
  '000000ddss00a000'
  '00000addssa00000'
  '0000a000000a0000'
  '0a00000000000000'
  '0000000000000000'
)

Save-TileGrid 'town7_tree1' @(
  'qqqqqqqqqqqqqqqq'
  'qqqqq22222qqqqqq'
  'qqqq2eeerre2qqqq'
  'qqq2eereeeee2qqq'
  'qqq2ee11e11e2qqq'
  'qqq2ee11e11e2qqq'
  'qqq2eeeeeeee2qqq'
  'qqqq2eee1ee2qqqq'
  'qqqq2ewewew2qqqq'
  'qqqqq2w2w2qqqqqq'
  'qqqqqq2a0qqqqqqq'
  'qqqqqqqa0qqq3qqq'
  'qqqqqqqa0qqqqqqq'
  'qqqqqq3a0aqqqqqq'
  'q3qqqqqqqqqqqqqq'
  'qqqqqqqqqqqqqqqq'
)

Save-TileGrid 'town7_tree2' @(
  'qqqqqqqqqqqqqqqq'
  'qqqaqqqqqqqaqqqq'
  'qqqq0q222q0qqqqq'
  'qqqq20eeee02qqqq'
  'qqq2eereeeee2qqq'
  'qqq2e11ee11e2qqq'
  'qqq2e11ee11e2qqq'
  'qqq2eeee1eee2qqq'
  'qqqq2ewewew2qqqq'
  'qqqqq2w2w2qqqqqq'
  'qqqqqq2a0qqqqqqq'
  'qqqqqqa0qqqq3qqq'
  'qqqqqa0qqqqqqqqq'
  'qqqq3a0aqqqqqqqq'
  'q3qqqqqqqqqqqqqq'
  'qqqqqqqqqqqqqqqq'
)

Save-TileGrid 'town7_tree3' @(
  'qqqqqqqqqqqqqqqq'
  'qqqqq2222qqqqqqq'
  'qqqq2eer1e2qqqqq'
  'qqq2eere1ee2qqqq'
  'qqq2e11e11ee2qqq'
  'qqq2e11e11ee2qqq'
  'qqq2eeeeeeee2qqq'
  'qqqq2ee1eee2qqqq'
  'qqqq2e2e2e2qqqqq'
  'qqqq2111112qqqqq'
  'qqqqq2w2w2qqqqqq'
  'qqqqqq2a0qqq3qqq'
  'qqqqqqqa0qqqqqqq'
  'qqqqqq3a0aqqqqqq'
  'q3qqqqqqqqqqqqqq'
  'qqqqqqqqqqqqqqqq'
)

Save-TileGrid 'town7_tree4' @(
  'qqqqqqqqqqqqqqqq'
  'qqqqqq222qqqqqqq'
  'qqqqq2eere2qqqqq'
  'qqqqq2e1e1e2qqqq'
  'qqqqq2e1e1e2qqqq'
  'qqqqq2eee1e2qqqq'
  'qqqqqq2ewe2qqqqq'
  'qqqqqqq2w2qqqqqq'
  'qqqqqqqa0qqaqqqq'
  'qqqqqqa0qqaqqqqq'
  'qqqqqa0qa0qq3qqq'
  'qqqqqa00aqqqqqqq'
  'qqqqqqa0qqqqqqqq'
  'qqqqq3a0aqqqqqqq'
  'q3qqqqqqqqqqqqqq'
  'qqqqqqqqqqqqqqqq'
)

Save-TileGrid 'mine_wall1' @(
  'ssssssssssssssss'
  'ssssssssssssssss'
  'aaaaaaaaaaaaaaaa'
  'aaq2qqqwqq2qqqaa'
  'aaqqq2qqqqqq2qaa'
  'aaqwqqqq2qqqqqaa'
  'aaqqqq2qqqwqqqaa'
  'aaq2qqqqqqqq2qaa'
  'aaqqqqCqqqqqqqaa'
  'aaqqwqqq2qqqqqaa'
  'aaqqqqqqqq2wqqaa'
  'aaq2qqqwqqqqqqaa'
  'aaqqqqqqqqVqq2aa'
  'aaqqq2qqqqqqqqaa'
  'aaqwqqqq2qqqwqaa'
  'aaqqqqqq2qqqqqaa'
)

Save-TileGrid 'forest_wall1' @(
  'xxcxxxxcxxxxxcxx'
  'xxxxcxxxxxcxxxxx'
  'ass0aass0aass0aa'
  'ass0aass0aass0aa'
  'ads0aass0aass0aa'
  'ass0aass0aads0aa'
  'ass0aass0aass0aa'
  'ass0aads0aass0aa'
  'ass0aass0aass0aa'
  'ass0aass0aass0aa'
  'ass0aass0a0ss0aa'
  'ads0aass0aass0aa'
  'ass0aass0aads00a'
  'ass00ass0aass0aa'
  'ass0aass0aass0a0'
  'ass0a0ss00ass0aa'
)

Save-TileGrid 'goosy_wall1' @(
  'cxzcxzcxzcxzcxzc'
  'cxccxzcxzcxzcxzc'
  'cxzcxzcxzcxzcxzc'
  'ssssssssssssssss'
  'aaaaaaaaaaaaaaaa'
  'cxzcxzcxzcxzcxzc'
  'cxzcxzcxzcxccxzc'
  'cxzcxzcxccxzcxzc'
  'cxzcxzcxzcxzcxzc'
  'cxzcxzcxzcxzcxzc'
  'ssssssssssssssss'
  'aaaaaaaaaaaaaaaa'
  'cxzcxzcxzcxzcxzc'
  'cxzcxzcxzcxzcxcc'
  'cxzcxccxzcxzcxzc'
  'cxzcxzcxzcxzcxzc'
)

Save-TileGrid 'mine_wall2' @(
  'ssssssssssssssss'
  'ssssssssssssssss'
  'aaaaaaaaaaaaaaaa'
  'aaq2qqqwqq2qpqaa'
  'aaqqq2qqqquVuqaa'
  'aaqwqqqq2qCuqqaa'
  'aaqqqq2qqpwqqqaa'
  'aaq2qqqquVqq2qaa'
  'aaqqqqqqCqqqqqaa'
  'aaqqwqup2qqqqqaa'
  'aaqqqqVuqq2wqqaa'
  'aaq2qCqwqqqqqqaa'
  'aaqqupqqqqVqq2aa'
  'aaquV2qqqqqqqqaa'
  'aauCqqqq2qqqwqaa'
  'aaqqqqqq2qqqqqaa'
)

Save-TileGrid 'forest_wall2' @(
  'xxcxxxxcxxxxxcxx'
  'xxxxcxxxxxcxxxxx'
  'ass0aass0aass0aa'
  'ass0aass0aass0aa'
  'ads0aass0aass0aa'
  'ass0aadd0aads0aa'
  'ass0aa11daass0aa'
  'ass0ad111aass0aa'
  'ass0ad111aass0aa'
  'ass0aad1daass0aa'
  'ass0aasd0a0ss0aa'
  'ads0aass0aass0aa'
  'ass0aass0aads00a'
  'ass00ass0aass0aa'
  'ass0aass0aass0a0'
  'ass0a0ss00ass0aa'
)

Save-TileGrid 'goosy_wall2' @(
  'cxzcxzcxzcxzcxzc'
  'cxccxzcxzcxzcxzc'
  'cxzcxzdffdxzcxzc'
  'sssssdf00sdsssss'
  'aaaaada00fdaaaaa'
  'cxzcxzdffdxzcxzc'
  'cxzcxzcxzcxccxzc'
  'cxzcxzcxccxzcxzc'
  'cxzcxzcxzcxzcxzc'
  'cxzcxzcxzcxzcxzc'
  'ssssssssssssssss'
  'aaaaaaaaaaaaaaaa'
  'cxzcxzcxzcxzcxzc'
  'cxzcxzcxzcxzcxcc'
  'cxzcxccxzcxzcxzc'
  'cxzcxzcxzcxzcxzc'
)

Save-TileGrid 'mine_wall3' @(
  'ssssssssssssssss'
  'ssssssssssssssss'
  'aaaaaaaaaaaaaass'
  'aaq2qqqwqq2qqaaa'
  'aaqqq2qqqqqq2qaa'
  'aaqwqqqq2qqqqqaa'
  'aaqqqq2qqqwqqdsa'
  'aaq2qqqqqqqq2d11'
  'aaqqqqCqqqqqqs11'
  'aaqqwqqq2qqqdq1s'
  'aaqqqqqqqq2wqqad'
  'aaq2qqqwqqqqqqaa'
  'aaqqqqqqqqVqq2aa'
  'aaqqq2qqqqqqqqaa'
  'aaqwqqqq2qqqwqaa'
  'aaqqqqqq2qqqqqaa'
)

Save-TileGrid 'forest_wall3' @(
  'xxcxxxxcxxxxxcxx'
  'xxxxcxxxxxcxxxxx'
  'ass0aass0aass0aa'
  'ass0aass0aass0aa'
  'ads0fass0aass0aa'
  'assfffss0aads0aa'
  'assddass0aass0aa'
  'ass0aads0aass0aa'
  'ass0afss0aass0aa'
  'assffffs0aass0aa'
  'assdddss0a0ss0aa'
  'ads0aass0aass0aa'
  'ass0aass0aads00a'
  'ass00ass0aass0aa'
  'ass0aass0aass0a0'
  'ass0a0ss00ass0aa'
)

Save-TileGrid 'goosy_wall3' @(
  'cxzcxzcxzcxzcxzc'
  'cxccxzcxzcxzcxzc'
  'cxzcxzcxzcxzcxzc'
  'ssssssssssssssss'
  'aaaaaaaaaaaaaaaa'
  'cxzcxxuuucxzcxzc'
  'cxzcxzuuicxccxzc'
  'cxzcxzupucxzcxzc'
  'cxzcxziiicxzcxzc'
  'cxzcxzuuuxxzcxzc'
  'ssssssssssssssss'
  'aaaaaaaaaaaaaaaa'
  'cxzcxzcxzcxzcxzc'
  'cxzcxzcxzcxzcxcc'
  'cxzcxccxzcxzcxzc'
  'cxzcxzcxzcxzcxzc'
)

Save-TileGrid 'mine_wall4' @(
  'ssssssssssssssss'
  'ssssssssssssssss'
  'aaaaaaaaaaaaaaaa'
  'aaq2qeqwqq2qqqaa'
  'aaqeeeqqqqqq2qaa'
  'aaqeffeq2qqqqqaa'
  'aadeWfedqqwqqqaa'
  'aadeffedqqqq2qaa'
  'aaqqeeCqqqqqqqaa'
  'aaqqddqq2qqqqqaa'
  'aaqqqqqqqq2wqqaa'
  'aaq2qqqwqqqqqqaa'
  'aaqqqqqqqqVqq2aa'
  'aaqqq2qqqqqqqqaa'
  'aaqwqqqq2qqqwqaa'
  'aaqqqqqq2qqqqqaa'
)

Save-TileGrid 'forest_wall4' @(
  'xxcxxxxcxxxxxcxx'
  'xxxxcxxxxxcxxxxx'
  'ass0xxss0xxss0xa'
  'asscxxss0xxss0xa'
  'ads0xcss0xcss0ca'
  'ass0cass0xads0va'
  'ass0vass0cass0aa'
  'ass0aads0vass0aa'
  'ass0aass0aass0aa'
  'ass0aass0aass0aa'
  'ass0aass0a0ss0aa'
  'ads0aass0aass0aa'
  'ass0aass0aads00a'
  'ass00ass0aass0aa'
  'ass0aass0aass0a0'
  'ass0a0ss00ass0aa'
)

Save-TileGrid 'goosy_wall4' @(
  'cxzcxzcxzcxzcxzc'
  'cxccxzcxzcxzcxzc'
  'cxzcxzcxzcxzcxzc'
  'ssssssssssssssss'
  'aaaaaaaaaaaaaaaa'
  'cxzcxzcxzcxzcxzc'
  'cxzcxzcxzcxccxzc'
  'cxzcxzcxccxzcxzc'
  'cxzcxzcxzssssszc'
  'cxzcxzcxsadSdasc'
  'ssssssssasdddsas'
  'aaaaaaaaaaaaaaaa'
  'cxzcxzcxzcxzcxzc'
  'cxzcxzcxzcxzcxcc'
  'cxzcxccxzcxzcxzc'
  'cxzcxzcxzcxzcxzc'
)

Write-Output 'Texture generation complete.'
