# Crystal Dungeons - deterministic bitmap-font generator (M25 slice 0).
# Produces an ORIGINAL proportional pixel font (printable ASCII 32-126) as a
# PNG atlas + AngelCode BMFont (.fnt) descriptors under assets/fonts/. Rerunning
# reproduces byte-identical files. Requires Windows PowerShell (System.Drawing).
#
# One 5x7 glyph design is emitted as three descriptors so raylib's DrawTextEx
# (which scales glyphs by requestedSize / font.baseSize, and where LoadBMFont
# sets baseSize = the .fnt "lineHeight") keeps the game's dominant sizes crisp:
#   font_small.fnt  lineHeight 8   -> base for HUD/caption text (size 8)
#   font_main.fnt   lineHeight 10  -> base for body/menu text   (size 10)
#   font_title.fnt  lineHeight 20  -> base for headings/title   (2x atlas)
# The small/main descriptors share the 1x atlas (identical glyphs, different
# base); the title descriptor uses a nearest-neighbour 2x atlas of the same
# glyphs, so the type reads as a single original typeface at three scales.

$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing

$repo = Split-Path (Split-Path $PSScriptRoot -Parent) -Parent
$outRoot = Join-Path $repo 'assets\fonts'
New-Item -ItemType Directory -Force $outRoot | Out-Null

# Glyph design: each block is "G <codepoint>" then exactly 7 rows of a fixed
# width using '#' (ink) and '.' (blank). Widths are trimmed to ink extent at
# generation time, so spacing is proportional. Space (32) is all-blank.
$glyphData = @'
G 32
...
...
...
...
...
...
...
G 33
#
#
#
#
#
.
#
G 34
#.#
#.#
...
...
...
...
...
G 35
.#.#.
.#.#.
#####
.#.#.
#####
.#.#.
.#.#.
G 36
..#..
.####
#.#..
.###.
..#.#
####.
..#..
G 37
##...
##..#
...#.
..#..
.#...
#..##
...##
G 38
.##..
#..#.
#.#..
.#...
#.#.#
#..#.
.##.#
G 39
#
#
.
.
.
.
.
G 40
.#
#.
#.
#.
#.
#.
.#
G 41
#.
.#
.#
.#
.#
.#
#.
G 42
.....
#.#.#
.###.
#####
.###.
#.#.#
.....
G 43
.....
..#..
..#..
#####
..#..
..#..
.....
G 44
..
..
..
..
..
.#
#.
G 45
....
....
....
####
....
....
....
G 46
.
.
.
.
.
.
#
G 47
....#
....#
...#.
..#..
.#...
#....
#....
G 48
.###.
#...#
#..##
#.#.#
##..#
#...#
.###.
G 49
..#..
.##..
..#..
..#..
..#..
..#..
.###.
G 50
.###.
#...#
....#
..##.
.#...
#....
#####
G 51
####.
....#
....#
.###.
....#
....#
####.
G 52
...#.
..##.
.#.#.
#..#.
#####
...#.
...#.
G 53
#####
#....
####.
....#
....#
#...#
.###.
G 54
.###.
#....
#....
####.
#...#
#...#
.###.
G 55
#####
....#
...#.
..#..
.#...
.#...
.#...
G 56
.###.
#...#
#...#
.###.
#...#
#...#
.###.
G 57
.###.
#...#
#...#
.####
....#
....#
.###.
G 58
.
.
#
.
.
#
.
G 59
..
..
.#
..
..
.#
#.
G 60
...#
..#.
.#..
#...
.#..
..#.
...#
G 61
....
....
####
....
####
....
....
G 62
#...
.#..
..#.
...#
..#.
.#..
#...
G 63
.###.
#...#
....#
..##.
..#..
.....
..#..
G 64
.###.
#...#
#.###
#.#.#
#.###
#....
.###.
G 65
.###.
#...#
#...#
#####
#...#
#...#
#...#
G 66
####.
#...#
#...#
####.
#...#
#...#
####.
G 67
.###.
#...#
#....
#....
#....
#...#
.###.
G 68
###..
#..#.
#...#
#...#
#...#
#..#.
###..
G 69
#####
#....
#....
####.
#....
#....
#####
G 70
#####
#....
#....
####.
#....
#....
#....
G 71
.###.
#...#
#....
#.###
#...#
#...#
.###.
G 72
#...#
#...#
#...#
#####
#...#
#...#
#...#
G 73
.###.
..#..
..#..
..#..
..#..
..#..
.###.
G 74
..###
...#.
...#.
...#.
#..#.
#..#.
.##..
G 75
#...#
#..#.
#.#..
##...
#.#..
#..#.
#...#
G 76
#....
#....
#....
#....
#....
#....
#####
G 77
#...#
##.##
#.#.#
#.#.#
#...#
#...#
#...#
G 78
#...#
##..#
#.#.#
#..##
#...#
#...#
#...#
G 79
.###.
#...#
#...#
#...#
#...#
#...#
.###.
G 80
####.
#...#
#...#
####.
#....
#....
#....
G 81
.###.
#...#
#...#
#...#
#.#.#
#..#.
.##.#
G 82
####.
#...#
#...#
####.
#.#..
#..#.
#...#
G 83
.####
#....
#....
.###.
....#
....#
####.
G 84
#####
..#..
..#..
..#..
..#..
..#..
..#..
G 85
#...#
#...#
#...#
#...#
#...#
#...#
.###.
G 86
#...#
#...#
#...#
#...#
#...#
.#.#.
..#..
G 87
#...#
#...#
#...#
#.#.#
#.#.#
#.#.#
.#.#.
G 88
#...#
#...#
.#.#.
..#..
.#.#.
#...#
#...#
G 89
#...#
#...#
.#.#.
..#..
..#..
..#..
..#..
G 90
#####
....#
...#.
..#..
.#...
#....
#####
G 91
##
#.
#.
#.
#.
#.
##
G 92
#....
#....
.#...
..#..
...#.
....#
....#
G 93
##
.#
.#
.#
.#
.#
##
G 94
..#..
.#.#.
#...#
.....
.....
.....
.....
G 95
.....
.....
.....
.....
.....
.....
#####
G 96
#.
.#
..
..
..
..
..
G 97
.....
.....
.###.
....#
.####
#...#
.####
G 98
#....
#....
####.
#...#
#...#
#...#
####.
G 99
.....
.....
.###.
#....
#....
#....
.###.
G 100
....#
....#
.####
#...#
#...#
#...#
.####
G 101
.....
.....
.###.
#...#
#####
#....
.###.
G 102
..##.
.#...
.#...
###..
.#...
.#...
.#...
G 103
.....
.####
#...#
#...#
.####
....#
.###.
G 104
#....
#....
####.
#...#
#...#
#...#
#...#
G 105
..#..
.....
.##..
..#..
..#..
..#..
.###.
G 106
...#.
.....
..##.
...#.
...#.
#..#.
.##..
G 107
#....
#....
#..#.
#.#..
##...
#.#..
#..#.
G 108
.##..
..#..
..#..
..#..
..#..
..#..
.###.
G 109
.....
.....
##.#.
#.#.#
#.#.#
#...#
#...#
G 110
.....
.....
####.
#...#
#...#
#...#
#...#
G 111
.....
.....
.###.
#...#
#...#
#...#
.###.
G 112
.....
####.
#...#
#...#
####.
#....
#....
G 113
.....
.####
#...#
#...#
.####
....#
....#
G 114
.....
.....
#.##.
##..#
#....
#....
#....
G 115
.....
.....
.####
#....
.###.
....#
####.
G 116
.#...
.#...
###..
.#...
.#...
.#..#
..##.
G 117
.....
.....
#...#
#...#
#...#
#...#
.####
G 118
.....
.....
#...#
#...#
#...#
.#.#.
..#..
G 119
.....
.....
#...#
#...#
#.#.#
#.#.#
.#.#.
G 120
.....
.....
#...#
.#.#.
..#..
.#.#.
#...#
G 121
.....
#...#
#...#
#...#
.####
....#
.###.
G 122
.....
.....
#####
...#.
..#..
.#...
#####
G 123
.##
.#.
.#.
#..
.#.
.#.
.##
G 124
#
#
#
#
#
#
#
G 125
##.
.#.
.#.
..#
.#.
.#.
##.
G 126
.....
.....
.##.#
#.##.
.....
.....
.....
'@

# --- Parse the glyph blocks into an ordered list of glyph records ---
$glyphs = New-Object System.Collections.Generic.List[object]
$lines = $glyphData -split "`r?`n"
$i = 0
while ($i -lt $lines.Count) {
  $line = $lines[$i]
  if ($line -match '^G (\d+)$') {
    $code = [int]$Matches[1]
    $rows = @()
    for ($r = 1; $r -le 7; $r++) { $rows += $lines[$i + $r] }
    $i += 8
    $boxW = ($rows | Measure-Object -Property Length -Maximum).Maximum
    # Ink extent (proportional trim).
    $minC = $boxW; $maxC = -1
    foreach ($row in $rows) {
      for ($c = 0; $c -lt $row.Length; $c++) {
        if ($row[$c] -eq '#') { if ($c -lt $minC) { $minC = $c }; if ($c -gt $maxC) { $maxC = $c } }
      }
    }
    $glyphs.Add([pscustomobject]@{ Code = $code; Rows = $rows; MinC = $minC; MaxC = $maxC })
  } else { $i++ }
}

# --- Lay glyphs out left-to-right (1px gap), compute atlas geometry ---
$cellH = 7
$cursor = 0
foreach ($g in $glyphs) {
  if ($g.MaxC -lt 0) {
    # Blank glyph (space): reserve a small transparent box, wider advance.
    $w = 3; $adv = 4
  } else {
    $w = $g.MaxC - $g.MinC + 1; $adv = $w + 1
  }
  $g | Add-Member -NotePropertyName X -NotePropertyValue $cursor
  $g | Add-Member -NotePropertyName W -NotePropertyValue $w
  $g | Add-Member -NotePropertyName Adv -NotePropertyValue $adv
  $cursor += $w + 1
}
$atlasW = $cursor

# --- Render an atlas bitmap at an integer scale (white ink on transparent) ---
function New-Atlas([int]$scale) {
  $bmp = New-Object System.Drawing.Bitmap(($atlasW * $scale), ($cellH * $scale),
    [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
  $white = [System.Drawing.Color]::FromArgb(255, 255, 255, 255)
  foreach ($g in $glyphs) {
    if ($g.MaxC -lt 0) { continue }
    for ($r = 0; $r -lt 7; $r++) {
      $row = $g.Rows[$r]
      for ($c = $g.MinC; $c -le $g.MaxC; $c++) {
        if ($c -lt $row.Length -and $row[$c] -eq '#') {
          $px = ($g.X + ($c - $g.MinC)) * $scale
          $py = $r * $scale
          for ($sy = 0; $sy -lt $scale; $sy++) {
            for ($sx = 0; $sx -lt $scale; $sx++) {
              $bmp.SetPixel($px + $sx, $py + $sy, $white)
            }
          }
        }
      }
    }
  }
  return $bmp
}

function Save-Png($bmp, [string]$rel) {
  $path = Join-Path $outRoot $rel
  $bmp.Save($path, [System.Drawing.Imaging.ImageFormat]::Png)
  $bmp.Dispose()
  Write-Output "  $rel"
}

# --- Emit a BMFont .fnt descriptor (LF newlines, UTF-8 no BOM) ---
function Save-Fnt([string]$rel, [int]$lineHeight, [int]$base, [int]$scale, [string]$page) {
  $sb = New-Object System.Text.StringBuilder
  [void]$sb.Append("info face=`"CrystalPixel`" size=$($cellH * $scale) bold=0 italic=0 unicode=0`n")
  [void]$sb.Append("common lineHeight=$lineHeight base=$base scaleW=$($atlasW * $scale) scaleH=$($cellH * $scale) pages=1 packed=0`n")
  [void]$sb.Append("page id=0 file=`"$page`"`n")
  [void]$sb.Append("chars count=$($glyphs.Count)`n")
  foreach ($g in $glyphs) {
    $x = $g.X * $scale; $w = $g.W * $scale; $h = $cellH * $scale; $adv = $g.Adv * $scale
    [void]$sb.Append("char id=$($g.Code) x=$x y=0 width=$w height=$h xoffset=0 yoffset=0 xadvance=$adv page=0 chnl=15`n")
  }
  $path = Join-Path $outRoot $rel
  $enc = New-Object System.Text.UTF8Encoding($false)
  [System.IO.File]::WriteAllText($path, $sb.ToString(), $enc)
  Write-Output "  $rel"
}

Write-Output 'Generating original bitmap font...'
$a1 = New-Atlas 1; Save-Png $a1 'font_atlas.png'
$a2 = New-Atlas 2; Save-Png $a2 'font_atlas_2x.png'
Save-Fnt 'font_small.fnt' 8  7  1 'font_atlas.png'
Save-Fnt 'font_main.fnt'  10 7  1 'font_atlas.png'
Save-Fnt 'font_title.fnt' 20 14 2 'font_atlas_2x.png'
Write-Output "Font generation complete ($($glyphs.Count) glyphs, atlas ${atlasW}x${cellH})."
