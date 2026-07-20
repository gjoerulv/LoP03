# Crystal Dungeons — application icon generator (M24).
# Builds packaging/crystal.ico from the approved crystal emblem
# (assets/textures/ui/emblem_crystal.png): nearest-neighbor upscales at
# 16/32/48/256 stored as classic uncompressed 32bpp DIB entries (accepted
# by rc.exe and every Windows shell). Fully original; rerun-safe.

$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing

$repo = Split-Path (Split-Path $PSScriptRoot -Parent) -Parent
$srcPng = Join-Path $repo 'assets\textures\ui\emblem_crystal.png'
$outIco = Join-Path $repo 'packaging\crystal.ico'
New-Item -ItemType Directory -Force (Split-Path $outIco -Parent) | Out-Null

$src = [System.Drawing.Bitmap]::FromFile($srcPng)

# One ICO image as an uncompressed BGRA DIB: BITMAPINFOHEADER (height
# doubled for the AND mask), bottom-up pixel rows, then an all-zero mask
# (alpha carries transparency at 32bpp).
function DibBytes([int]$size) {
  $bmp = New-Object System.Drawing.Bitmap($size, $size)
  $g = [System.Drawing.Graphics]::FromImage($bmp)
  $g.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::NearestNeighbor
  $g.PixelOffsetMode = [System.Drawing.Drawing2D.PixelOffsetMode]::Half
  $g.DrawImage($src, 0, 0, $size, $size)
  $g.Dispose()

  $maskStride = [int](([math]::Ceiling($size / 32.0)) * 4)
  $ms = New-Object System.IO.MemoryStream
  $w = New-Object System.IO.BinaryWriter($ms)
  $w.Write([UInt32]40)                       # biSize
  $w.Write([Int32]$size)                     # biWidth
  $w.Write([Int32]($size * 2))               # biHeight (XOR + AND)
  $w.Write([UInt16]1)                        # biPlanes
  $w.Write([UInt16]32)                       # biBitCount
  $w.Write([UInt32]0)                        # biCompression BI_RGB
  $w.Write([UInt32]($size * $size * 4 + $maskStride * $size))
  $w.Write([Int32]0); $w.Write([Int32]0)     # ppm
  $w.Write([UInt32]0); $w.Write([UInt32]0)   # colors
  for ($y = $size - 1; $y -ge 0; $y--) {     # bottom-up BGRA rows
    for ($x = 0; $x -lt $size; $x++) {
      $p = $bmp.GetPixel($x, $y)
      $w.Write([Byte]$p.B); $w.Write([Byte]$p.G); $w.Write([Byte]$p.R); $w.Write([Byte]$p.A)
    }
  }
  $zero = New-Object byte[] ($maskStride * $size)  # AND mask (alpha rules)
  $w.Write($zero)
  $bmp.Dispose()
  $bytes = $ms.ToArray()
  $w.Dispose()
  return ,$bytes
}

$sizes = @(16, 32, 48, 256)
$blobs = New-Object System.Collections.ArrayList
foreach ($s in $sizes) { [void]$blobs.Add([byte[]](DibBytes $s)) }
$src.Dispose()

$ms = New-Object System.IO.MemoryStream
$w = New-Object System.IO.BinaryWriter($ms)
$w.Write([UInt16]0); $w.Write([UInt16]1); $w.Write([UInt16]$sizes.Count)  # ICONDIR
$offset = 6 + 16 * $sizes.Count
for ($i = 0; $i -lt $sizes.Count; $i++) {
  $dim = $sizes[$i]; if ($dim -ge 256) { $dim = 0 }
  $w.Write([Byte]$dim); $w.Write([Byte]$dim); $w.Write([Byte]0); $w.Write([Byte]0)
  $w.Write([UInt16]1); $w.Write([UInt16]32)
  $blob = [byte[]]$blobs[$i]
  $w.Write([UInt32]$blob.Length); $w.Write([UInt32]$offset)
  $offset += $blob.Length
}
foreach ($b in $blobs) { $w.Write([byte[]]$b) }
[System.IO.File]::WriteAllBytes($outIco, $ms.ToArray()); $w.Dispose()
Write-Output "  crystal.ico ($($sizes -join '/') px, $((Get-Item $outIco).Length) bytes)"
