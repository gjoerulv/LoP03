# Are P Geese - M128 tile review harness.
#
# Composites the hand-placed 16x16 environment tiles under
# assets/textures/environments/ into two labelled sheets in docs/sprite_review/:
#   tiles_towns.png  - one row per town: its four trees, then its four ground
#                      layouts, magnified, plus the town's tree ring at 1x and 3x
#                      placed by the engine's own position hash (render/TileVariant.hpp)
#   tiles_walls.png  - one row per dungeon theme: the four wall variants, then a
#                      room perimeter at 1x and 3x placed the same way
# Nothing here writes to assets/ - it only reads the generated PNGs. Requires
# Windows PowerShell (System.Drawing), like preview.ps1.
#
# Usage:
#   powershell tools/asset_gen/preview_tiles.ps1
#   powershell tools/asset_gen/preview_tiles.ps1 -Zoom 10

[CmdletBinding()]
param(
  [int]$Zoom = 6
)

$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing

$repo = Split-Path (Split-Path $PSScriptRoot -Parent) -Parent
$envDir = Join-Path $repo 'assets\textures\environments'
$outDir = Join-Path $repo 'docs\sprite_review'
New-Item -ItemType Directory -Force $outDir | Out-Null

# render/TileVariant.hpp, replicated in C# (Windows PowerShell reads 64-bit hex
# literals as signed and never wraps its arithmetic): the SplitMix64 finalizer
# over the spatial primes, then a weighted bucket walk.
Add-Type -TypeDefinition @'
public static class TileVariantHash {
  public static ulong Hash(ulong seed, int x, int y, ulong salt) {
    unchecked {
      ulong v = seed;
      v ^= (ulong)(long)x * 73856093UL;
      v ^= (ulong)(long)y * 19349663UL;
      v ^= (salt + 1UL) * 0xD1B54A32D192ED03UL;
      v ^= v >> 30; v *= 0xBF58476D1CE4E5B9UL;
      v ^= v >> 27; v *= 0x94D049BB133111EBUL;
      v ^= v >> 31;
      return v;
    }
  }
  public static int Variant(ulong seed, int x, int y, ulong salt, int[] weights) {
    ulong total = 0;
    foreach (int w in weights) { total += (ulong)(w > 0 ? w : 0); }
    if (total == 0) { return 0; }
    ulong r = Hash(seed, x, y, salt) % total;
    for (int i = 0; i < weights.Length; i++) {
      ulong w = (ulong)(weights[i] > 0 ? weights[i] : 0);
      if (r < w) { return i; }
      r -= w;
    }
    return weights.Length - 1;
  }
}
'@
function Tile-Variant([uint64]$seed, [int]$x, [int]$y, [uint64]$salt, [int[]]$weights) {
  return [TileVariantHash]::Variant($seed, $x, $y, $salt, $weights)
}

function Load-Tile([string]$name) {
  $path = Join-Path $envDir "$name.png"
  if (-not (Test-Path $path)) { throw "missing tile $path (run generate_textures.ps1 first)" }
  return [System.Drawing.Bitmap]::FromFile($path)
}
function New-Sheet([int]$w, [int]$h) {
  $bmp = New-Object System.Drawing.Bitmap($w, $h)
  $g = [System.Drawing.Graphics]::FromImage($bmp)
  $g.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::NearestNeighbor
  $g.PixelOffsetMode = [System.Drawing.Drawing2D.PixelOffsetMode]::Half
  $g.Clear([System.Drawing.Color]::FromArgb(255, 30, 28, 38))
  return @($bmp, $g)
}
$font = New-Object System.Drawing.Font('Consolas', 9)
$ink = New-Object System.Drawing.SolidBrush([System.Drawing.Color]::White)

# --- towns ---------------------------------------------------------------
$treeW = [int[]](30, 25, 25, 20); $groundW = [int[]](45, 25, 15, 15); $wallW = [int[]](55, 20, 15, 10)
$ringW = 24; $ringH = 12
$cell = 16 * $Zoom + 6
$mockW = $ringW * 16 * 3
$rowH = [Math]::Max($cell, $ringH * 16 * 3) + 22
$sheetW = 8 * $cell + 16 + $mockW + 16 + $ringW * 16 + 16
$s = New-Sheet $sheetW ($rowH * 7 + 8); $sheet = $s[0]; $g = $s[1]
for ($town = 1; $town -le 7; $town++) {
  $y0 = 4 + ($town - 1) * $rowH
  $g.DrawString("town $town", $font, $ink, 4, $y0 + $rowH - 20)
  $i = 0
  foreach ($kind in 'tree', 'ground') {
    for ($v = 1; $v -le 4; $v++) {
      $img = Load-Tile "town${town}_${kind}${v}"
      $g.DrawImage($img, 8 + $i * $cell, $y0, 16 * $Zoom, 16 * $Zoom)
      $img.Dispose(); $i++
    }
  }
  # the ring at 3x, then at 1x: trees on the border, ground inside, by the hash
  $mock = New-Object System.Drawing.Bitmap(($ringW * 16), ($ringH * 16))
  $mg = [System.Drawing.Graphics]::FromImage($mock)
  for ($ty = 0; $ty -lt $ringH; $ty++) { for ($tx = 0; $tx -lt $ringW; $tx++) {
    $ring = ($tx -eq 0 -or $tx -eq $ringW - 1 -or $ty -eq 0 -or $ty -eq $ringH - 1)
    if ($ring) { $name = "town${town}_tree$((Tile-Variant 0 $tx $ty $town $treeW) + 1)" }
    else { $name = "town${town}_ground$((Tile-Variant 0 $tx $ty $town $groundW) + 1)" }
    $img = Load-Tile $name; $mg.DrawImage($img, $tx * 16, $ty * 16, 16, 16); $img.Dispose()
  } }
  $mg.Dispose()
  $x3 = 8 + 8 * $cell + 8
  $g.DrawImage($mock, $x3, $y0, $mock.Width * 3, $mock.Height * 3)
  $g.DrawImage($mock, $x3 + $mockW + 8, $y0, $mock.Width, $mock.Height)
  $mock.Dispose()
}
$g.Dispose()
$out = Join-Path $outDir 'tiles_towns.png'
$sheet.Save($out, [System.Drawing.Imaging.ImageFormat]::Png); $sheet.Dispose()
Write-Output "  docs/sprite_review/tiles_towns.png (7 towns x 8 tiles at $($Zoom)x + rings)"

# --- walls ---------------------------------------------------------------
$themes = @(
  @{ file = 'keep';   floor = 'keep_floor' },
  @{ file = 'mine';   floor = 'mine_floor' },
  @{ file = 'forest'; floor = 'forest_floor' },
  @{ file = 'goosy';  floor = 'goosy_floor' }
)
$roomW = 19; $roomH = 13
$mockW = $roomW * 16 * 3
$rowH = [Math]::Max($cell, $roomH * 16 * 3) + 22
$sheetW = 4 * $cell + 16 + $mockW + 16 + $roomW * 16 + 16
$s = New-Sheet $sheetW ($rowH * $themes.Count + 8); $sheet = $s[0]; $g = $s[1]
for ($t = 0; $t -lt $themes.Count; $t++) {
  $th = $themes[$t]; $y0 = 4 + $t * $rowH
  $g.DrawString($th.file, $font, $ink, 4, $y0 + $rowH - 20)
  for ($v = 1; $v -le 4; $v++) {
    $img = Load-Tile "$($th.file)_wall$v"
    $g.DrawImage($img, 8 + ($v - 1) * $cell, $y0, 16 * $Zoom, 16 * $Zoom); $img.Dispose()
  }
  $mock = New-Object System.Drawing.Bitmap(($roomW * 16), ($roomH * 16))
  $mg = [System.Drawing.Graphics]::FromImage($mock)
  $floor = Load-Tile $th.floor
  [uint64]$salt = [Convert]::ToUInt64('2B7E151628AED2A6', 16) + 5   # DungeonState's kSaltWallVariant, room 5
  for ($ty = 0; $ty -lt $roomH; $ty++) { for ($tx = 0; $tx -lt $roomW; $tx++) {
    $ring = ($tx -eq 0 -or $tx -eq $roomW - 1 -or $ty -eq 0 -or $ty -eq $roomH - 1)
    if ($ring) {
      $img = Load-Tile "$($th.file)_wall$((Tile-Variant 424242 $tx $ty $salt $wallW) + 1)"
      $mg.DrawImage($img, $tx * 16, $ty * 16, 16, 16); $img.Dispose()
    } else { $mg.DrawImage($floor, $tx * 16, $ty * 16, 16, 16) }
  } }
  $floor.Dispose(); $mg.Dispose()
  $x3 = 8 + 4 * $cell + 8
  $g.DrawImage($mock, $x3, $y0, $mock.Width * 3, $mock.Height * 3)
  $g.DrawImage($mock, $x3 + $mockW + 8, $y0, $mock.Width, $mock.Height)
  $mock.Dispose()
}
$g.Dispose()
$out = Join-Path $outDir 'tiles_walls.png'
$sheet.Save($out, [System.Drawing.Imaging.ImageFormat]::Png); $sheet.Dispose()
Write-Output "  docs/sprite_review/tiles_walls.png (4 themes x 4 walls at $($Zoom)x + rooms)"
