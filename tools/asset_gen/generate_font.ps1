# Crystal Dungeons - deterministic bitmap-font generator (M25 slice 0; M87
# Latin extension). Produces an ORIGINAL proportional pixel font (printable
# ASCII 32-126 plus the M87 Latin localization set: every Latin-1 Supplement
# letter and the inverted marks / guillemets — the authority is
# src/ui/GlyphCoverage.hpp) as a PNG atlas + AngelCode BMFont (.fnt)
# descriptors under assets/fonts/. Rerunning reproduces byte-identical files.
# Requires Windows PowerShell (System.Drawing).
#
# M114 (the readability redesign): the same typeface redrawn on a 6x9
# master cell - cap height 7 (rows 0-6), x-height 5 (rows 2-6), descenders
# 2 (rows 7-8), accents in rows 0-1 (the M87 convention, so accented
# capitals keep their compressed 5-row bodies). Clearer lowercase (two-storey
# a, open c vs closed e, distinct r/n/m arches, pointed v vs round u),
# serifed I / tailed l / flagged 1, dotted 0 vs O, barred G. Every glyph is a
# hand-placed grid; nothing is traced or rasterized from any typeface.
#   - accents: acute ..#/.#.  grave #../.#.  circumflex .#./#.# (3-wide;
#     wider bodies get wider hats)  diaeresis (row 1) #.#  tilde .##/##.
#     ring a 2x2 blob;
#   - cedilla hangs in the descender rows under c/C (no compression).
#
# One glyph design is emitted as three descriptors so raylib's DrawTextEx
# (which scales glyphs by requestedSize / font.baseSize, and where LoadBMFont
# sets baseSize = the .fnt "lineHeight") keeps the game's dominant sizes crisp:
#   font_small.fnt  lineHeight 9   -> base for HUD/caption text (size 9, the
#                                     M114 floor; 8px text no longer exists)
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

# Glyph design: each block is "G <codepoint>" then exactly 9 rows of a fixed
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
.
.
G 34
#.#
#.#
...
...
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
.....
.....
G 36
..#..
.####
#.#..
.###.
..#.#
####.
..#..
.....
.....
G 37
##...
##..#
...#.
..#..
.#...
#..##
...##
.....
.....
G 38
.##..
#..#.
#.#..
.#...
#.#.#
#..#.
.##.#
.....
.....
G 39
#
#
.
.
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
..
..
G 41
#.
.#
.#
.#
.#
.#
#.
..
..
G 42
.....
#.#.#
.###.
#####
.###.
#.#.#
.....
.....
.....
G 43
.....
.....
..#..
..#..
#####
..#..
..#..
.....
.....
G 44
..
..
..
..
..
.#
.#
#.
..
G 45
....
....
....
....
####
....
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
.
.
G 47
....#
...#.
...#.
..#..
.#...
.#...
#....
.....
.....
G 48
.###.
#...#
#...#
#.#.#
#...#
#...#
.###.
.....
.....
G 49
..#..
.##..
#.#..
..#..
..#..
..#..
#####
.....
.....
G 50
.###.
#...#
....#
...#.
..#..
.#...
#####
.....
.....
G 51
#####
...#.
..#..
...#.
....#
#...#
.###.
.....
.....
G 52
...#.
..##.
.#.#.
#..#.
#####
...#.
...#.
.....
.....
G 53
#####
#....
####.
....#
....#
#...#
.###.
.....
.....
G 54
..##.
.#...
#....
####.
#...#
#...#
.###.
.....
.....
G 55
#####
....#
...#.
..#..
.#...
.#...
.#...
.....
.....
G 56
.###.
#...#
#...#
.###.
#...#
#...#
.###.
.....
.....
G 57
.###.
#...#
#...#
.####
....#
...#.
.##..
.....
.....
G 58
.
.
#
.
.
.
#
.
.
G 59
..
..
.#
..
..
.#
.#
#.
..
G 60
....
...#
..#.
.#..
..#.
...#
....
....
....
G 61
....
....
....
####
....
####
....
....
....
G 62
....
#...
.#..
..#.
.#..
#...
....
....
....
G 63
.###.
#...#
....#
..##.
..#..
.....
..#..
.....
.....
G 64
.###.
#...#
#.###
#.#.#
#.###
#....
.####
.....
.....
G 65
.###.
#...#
#...#
#####
#...#
#...#
#...#
.....
.....
G 66
####.
#...#
#...#
####.
#...#
#...#
####.
.....
.....
G 67
.###.
#...#
#....
#....
#....
#...#
.###.
.....
.....
G 68
####.
#...#
#...#
#...#
#...#
#...#
####.
.....
.....
G 69
#####
#....
#....
####.
#....
#....
#####
.....
.....
G 70
#####
#....
#....
####.
#....
#....
#....
.....
.....
G 71
.###.
#...#
#....
#.###
#...#
#...#
.####
.....
.....
G 72
#...#
#...#
#...#
#####
#...#
#...#
#...#
.....
.....
G 73
###
.#.
.#.
.#.
.#.
.#.
###
...
...
G 74
..###
...#.
...#.
...#.
...#.
#..#.
.##..
.....
.....
G 75
#...#
#..#.
#.#..
##...
#.#..
#..#.
#...#
.....
.....
G 76
#....
#....
#....
#....
#....
#....
#####
.....
.....
G 77
#...#
##.##
#.#.#
#.#.#
#...#
#...#
#...#
.....
.....
G 78
#...#
##..#
##..#
#.#.#
#..##
#..##
#...#
.....
.....
G 79
.###.
#...#
#...#
#...#
#...#
#...#
.###.
.....
.....
G 80
####.
#...#
#...#
####.
#....
#....
#....
.....
.....
G 81
.###.
#...#
#...#
#...#
#.#.#
#..#.
.##.#
.....
.....
G 82
####.
#...#
#...#
####.
#.#..
#..#.
#...#
.....
.....
G 83
.####
#....
#....
.###.
....#
....#
####.
.....
.....
G 84
#####
..#..
..#..
..#..
..#..
..#..
..#..
.....
.....
G 85
#...#
#...#
#...#
#...#
#...#
#...#
.###.
.....
.....
G 86
#...#
#...#
#...#
#...#
#...#
.#.#.
..#..
.....
.....
G 87
#...#
#...#
#...#
#.#.#
#.#.#
##.##
#...#
.....
.....
G 88
#...#
#...#
.#.#.
..#..
.#.#.
#...#
#...#
.....
.....
G 89
#...#
#...#
.#.#.
..#..
..#..
..#..
..#..
.....
.....
G 90
#####
....#
...#.
..#..
.#...
#....
#####
.....
.....
G 91
##
#.
#.
#.
#.
#.
##
..
..
G 92
#....
.#...
.#...
..#..
...#.
...#.
....#
.....
.....
G 93
##
.#
.#
.#
.#
.#
##
..
..
G 94
.#.
#.#
...
...
...
...
...
...
...
G 95
.....
.....
.....
.....
.....
.....
.....
#####
.....
G 96
#.
.#
..
..
..
..
..
..
..
G 97
....
....
.##.
...#
.###
#..#
.###
....
....
G 98
#...
#...
#...
###.
#..#
#..#
###.
....
....
G 99
....
....
.##.
#...
#...
#...
.##.
....
....
G 100
...#
...#
...#
.###
#..#
#..#
.###
....
....
G 101
....
....
.##.
#..#
####
#...
.##.
....
....
G 102
.##
.#.
###
.#.
.#.
.#.
.#.
...
...
G 103
....
....
.###
#..#
#..#
#..#
.###
...#
.##.
G 104
#...
#...
#...
###.
#..#
#..#
#..#
....
....
G 105
.#.
...
##.
.#.
.#.
.#.
###
...
...
G 106
..#
...
.##
..#
..#
..#
..#
..#
##.
G 107
#...
#...
#..#
#.#.
##..
#.#.
#..#
....
....
G 108
##.
.#.
.#.
.#.
.#.
.#.
.##
...
...
G 109
.....
.....
##.#.
#.#.#
#.#.#
#...#
#...#
.....
.....
G 110
....
....
###.
#..#
#..#
#..#
#..#
....
....
G 111
....
....
.##.
#..#
#..#
#..#
.##.
....
....
G 112
....
....
###.
#..#
#..#
###.
#...
#...
#...
G 113
....
....
.###
#..#
#..#
.###
...#
...#
...#
G 114
....
....
#.##
##..
#...
#...
#...
....
....
G 115
....
....
.###
#...
.##.
...#
###.
....
....
G 116
...
.#.
###
.#.
.#.
.#.
.##
...
...
G 117
....
....
#..#
#..#
#..#
#..#
.###
....
....
G 118
.....
.....
#...#
#...#
#...#
.#.#.
..#..
.....
.....
G 119
.....
.....
#...#
#...#
#.#.#
#.#.#
.#.#.
.....
.....
G 120
....
....
#..#
#..#
.##.
#..#
#..#
....
....
G 121
....
....
#..#
#..#
#..#
.###
...#
...#
.##.
G 122
....
....
####
...#
.##.
#...
####
....
....
G 123
.##
.#.
.#.
#..
.#.
.#.
.##
...
...
G 124
#
#
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
...
...
G 126
.....
.....
.....
.##.#
#.##.
.....
.....
.....
.....
G 161
.
.
#
.
#
#
#
#
#
G 171
.....
.....
..#.#
.#.#.
#.#..
.#.#.
..#.#
.....
.....
G 187
.....
.....
#.#..
.#.#.
..#.#
.#.#.
#.#..
.....
.....
G 191
.....
.....
..#..
.....
..#..
.#...
#....
#...#
.###.
G 192
.#...
..#..
.###.
#...#
#####
#...#
#...#
.....
.....
G 193
...#.
..#..
.###.
#...#
#####
#...#
#...#
.....
.....
G 194
..#..
.#.#.
.###.
#...#
#####
#...#
#...#
.....
.....
G 195
.##.#
#.##.
.###.
#...#
#####
#...#
#...#
.....
.....
G 196
.....
.#.#.
.###.
#...#
#####
#...#
#...#
.....
.....
G 197
..##.
..##.
.###.
#...#
#####
#...#
#...#
.....
.....
G 198
.####
#.#..
#.#..
#.###
###..
#.#..
#.###
.....
.....
G 199
.###.
#...#
#....
#....
#....
#...#
.###.
..#..
.##..
G 200
.#...
..#..
#####
#....
####.
#....
#####
.....
.....
G 201
...#.
..#..
#####
#....
####.
#....
#####
.....
.....
G 202
..#..
.#.#.
#####
#....
####.
#....
#####
.....
.....
G 203
.....
.#.#.
#####
#....
####.
#....
#####
.....
.....
G 204
#..
.#.
###
.#.
.#.
.#.
###
...
...
G 205
..#
.#.
###
.#.
.#.
.#.
###
...
...
G 206
.#.
#.#
###
.#.
.#.
.#.
###
...
...
G 207
...
#.#
###
.#.
.#.
.#.
###
...
...
G 208
####.
#...#
#...#
###.#
#...#
#...#
####.
.....
.....
G 209
.##.#
#.##.
#...#
##..#
#.#.#
#..##
#...#
.....
.....
G 210
.#...
..#..
.###.
#...#
#...#
#...#
.###.
.....
.....
G 211
...#.
..#..
.###.
#...#
#...#
#...#
.###.
.....
.....
G 212
..#..
.#.#.
.###.
#...#
#...#
#...#
.###.
.....
.....
G 213
.##.#
#.##.
.###.
#...#
#...#
#...#
.###.
.....
.....
G 214
.....
.#.#.
.###.
#...#
#...#
#...#
.###.
.....
.....
G 216
.###.
#..##
#.#.#
#.#.#
#.#.#
##..#
.###.
.....
.....
G 217
.#...
..#..
#...#
#...#
#...#
#...#
.###.
.....
.....
G 218
...#.
..#..
#...#
#...#
#...#
#...#
.###.
.....
.....
G 219
..#..
.#.#.
#...#
#...#
#...#
#...#
.###.
.....
.....
G 220
.....
.#.#.
#...#
#...#
#...#
#...#
.###.
.....
.....
G 221
...#.
..#..
#...#
.#.#.
..#..
..#..
..#..
.....
.....
G 222
#....
####.
#...#
#...#
####.
#....
#....
.....
.....
G 223
.##.
#..#
#.#.
#..#
#..#
#.#.
#...
....
....
G 224
.#..
..#.
.##.
...#
.###
#..#
.###
....
....
G 225
..#.
.#..
.##.
...#
.###
#..#
.###
....
....
G 226
.##.
#..#
.##.
...#
.###
#..#
.###
....
....
G 227
.#.#
#.#.
.##.
...#
.###
#..#
.###
....
....
G 228
....
#..#
.##.
...#
.###
#..#
.###
....
....
G 229
.##.
.##.
.##.
...#
.###
#..#
.###
....
....
G 230
.....
.....
###.#
..#.#
.####
#.#..
.####
.....
.....
G 231
....
....
.##.
#...
#...
#...
.##.
..#.
.##.
G 232
.#..
..#.
.##.
#..#
####
#...
.##.
....
....
G 233
..#.
.#..
.##.
#..#
####
#...
.##.
....
....
G 234
.##.
#..#
.##.
#..#
####
#...
.##.
....
....
G 235
....
#..#
.##.
#..#
####
#...
.##.
....
....
G 236
#..
.#.
##.
.#.
.#.
.#.
###
...
...
G 237
..#
.#.
##.
.#.
.#.
.#.
###
...
...
G 238
.#.
#.#
##.
.#.
.#.
.#.
###
...
...
G 239
...
#.#
##.
.#.
.#.
.#.
###
...
...
G 240
#..#.
.##..
..#..
.###.
#...#
#...#
.###.
.....
.....
G 241
.#.#
#.#.
###.
#..#
#..#
#..#
#..#
....
....
G 242
.#..
..#.
.##.
#..#
#..#
#..#
.##.
....
....
G 243
..#.
.#..
.##.
#..#
#..#
#..#
.##.
....
....
G 244
.##.
#..#
.##.
#..#
#..#
#..#
.##.
....
....
G 245
.#.#
#.#.
.##.
#..#
#..#
#..#
.##.
....
....
G 246
....
#..#
.##.
#..#
#..#
#..#
.##.
....
....
G 248
.....
.....
.###.
#..##
#.#.#
##..#
.###.
.....
.....
G 249
.#..
..#.
#..#
#..#
#..#
#..#
.###
....
....
G 250
..#.
.#..
#..#
#..#
#..#
#..#
.###
....
....
G 251
.##.
#..#
#..#
#..#
#..#
#..#
.###
....
....
G 252
....
#..#
#..#
#..#
#..#
#..#
.###
....
....
G 253
..#.
.#..
#..#
#..#
#..#
.###
...#
...#
.##.
G 254
#...
#...
###.
#..#
#..#
###.
#...
#...
#...
G 255
....
#..#
#..#
#..#
#..#
.###
...#
...#
.##.
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
    for ($r = 1; $r -le 9; $r++) { $rows += $lines[$i + $r] }
    $i += 10
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
$cellH = 9
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
    for ($r = 0; $r -lt 9; $r++) {
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
  [void]$sb.Append("info face=`"CrystalPixel`" size=$($cellH * $scale) bold=0 italic=0 unicode=1`n")
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
Save-Fnt 'font_small.fnt' 9  7  1 'font_atlas.png'
Save-Fnt 'font_main.fnt'  10 7  1 'font_atlas.png'
Save-Fnt 'font_title.fnt' 20 14 2 'font_atlas_2x.png'
Write-Output "Font generation complete ($($glyphs.Count) glyphs, atlas ${atlasW}x${cellH})."
