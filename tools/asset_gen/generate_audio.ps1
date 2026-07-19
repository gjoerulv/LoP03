# Crystal Dungeons — deterministic slice-music generator (M15).
# Synthesizes the three scene loops (square lead + triangle bass, art_bible §9)
# as 22050 Hz 16-bit mono WAVs under assets/audio/music/. Fully original.

$ErrorActionPreference = 'Stop'
$repo = Split-Path (Split-Path $PSScriptRoot -Parent) -Parent
$outDir = Join-Path $repo 'assets\audio\music'
New-Item -ItemType Directory -Force $outDir | Out-Null
$rate = 22050

function NoteHz([string]$n) {
  # "A4" style names; "-" = rest.
  if ($n -eq '-') { return 0.0 }
  $names = @{ 'C'=0; 'C#'=1; 'D'=2; 'D#'=3; 'E'=4; 'F'=5; 'F#'=6; 'G'=7; 'G#'=8; 'A'=9; 'A#'=10; 'B'=11 }
  $oct = [int]::Parse($n.Substring($n.Length-1))
  $key = $n.Substring(0, $n.Length-1)
  $semis = $names[$key] + ($oct + 1) * 12 - 69   # A4 = 440
  return 440.0 * [math]::Pow(2.0, $semis / 12.0)
}

# Renders one channel: sequence of @(note, beats) at $bpm into a float array.
function Render([object[]]$seq, [double]$bpm, [string]$wave, [double]$amp, [int]$total) {
  $out = New-Object double[] $total
  $pos = 0
  foreach ($step in $seq) {
    $hz = NoteHz $step[0]
    $len = [int]($rate * 60.0 / $bpm * $step[1])
    for ($i = 0; $i -lt $len -and ($pos + $i) -lt $total; $i++) {
      if ($hz -le 0) { continue }
      $t = $i / [double]$rate
      $phase = ($t * $hz) % 1.0
      $v = 0.0
      if ($wave -eq 'square') { $v = if ($phase -lt 0.25) { 1.0 } else { -1.0 } }        # 25% duty
      elseif ($wave -eq 'tri') { $v = 4.0 * [math]::Abs($phase - 0.5) - 1.0 }
      $env = [math]::Exp(-2.2 * $i / [double]$len)                                       # per-note decay
      $out[$pos + $i] += $v * $env * $amp
    }
    $pos += $len
  }
  return $out
}

function WriteWav([string]$name, [double[][]]$channels, [int]$total) {
  $mix = New-Object double[] $total
  foreach ($ch in $channels) { for ($i = 0; $i -lt $total; $i++) { $mix[$i] += $ch[$i] } }
  $peak = 0.0; foreach ($v in $mix) { $a = [math]::Abs($v); if ($a -gt $peak) { $peak = $a } }
  if ($peak -lt 0.0001) { $peak = 1.0 }
  $data = New-Object byte[] ($total * 2)
  for ($i = 0; $i -lt $total; $i++) {
    $s = [Int16]([math]::Round($mix[$i] / $peak * 0.82 * 32000))
    $data[$i*2] = [byte]($s -band 0xFF); $data[$i*2+1] = [byte](($s -shr 8) -band 0xFF)
  }
  $ms = New-Object System.IO.MemoryStream; $w = New-Object System.IO.BinaryWriter($ms)
  $w.Write([Text.Encoding]::ASCII.GetBytes('RIFF')); $w.Write([int](36 + $data.Length))
  $w.Write([Text.Encoding]::ASCII.GetBytes('WAVEfmt ')); $w.Write([int]16)
  $w.Write([Int16]1); $w.Write([Int16]1); $w.Write([int]$rate); $w.Write([int]($rate*2))
  $w.Write([Int16]2); $w.Write([Int16]16)
  $w.Write([Text.Encoding]::ASCII.GetBytes('data')); $w.Write([int]$data.Length); $w.Write($data)
  [IO.File]::WriteAllBytes((Join-Path $outDir $name), $ms.ToArray()); $w.Dispose()
  Write-Output "  $name ($([math]::Round($total/$rate,1))s)"
}

Write-Output 'Generating music loops...'

# --- Town: calm major arpeggios, 104 BPM, 16 beats (~9.2s) ---
$bpm = 104.0; $total = [int]($rate * 60.0 / $bpm * 16)
$lead = Render @(
  @('C5',0.5),@('E5',0.5),@('G5',0.5),@('E5',0.5), @('A4',0.5),@('C5',0.5),@('E5',0.5),@('C5',0.5),
  @('F4',0.5),@('A4',0.5),@('C5',0.5),@('A4',0.5), @('G4',0.5),@('B4',0.5),@('D5',0.5),@('B4',0.5),
  @('C5',0.5),@('E5',0.5),@('G5',0.5),@('C6',0.5), @('A4',0.5),@('E5',0.5),@('C5',0.5),@('A4',0.5),
  @('F4',0.5),@('C5',0.5),@('A4',0.5),@('F4',0.5), @('G4',0.5),@('D5',0.5),@('B4',1.0)
) $bpm 'square' 0.30 $total
$bass = Render @(
  @('C3',2),@('A2',2),@('F2',2),@('G2',2), @('C3',2),@('A2',2),@('F2',2),@('G2',2)
) $bpm 'tri' 0.26 $total
WriteWav 'town.wav' @($lead, $bass) $total

# --- Dungeon: sparse minor with drone, 80 BPM, 16 beats (~12s) ---
$bpm = 80.0; $total = [int]($rate * 60.0 / $bpm * 16)
$lead = Render @(
  @('A4',1.0),@('C5',1.0),@('E5',1.5),@('D5',0.5), @('C5',1.0),@('B4',1.0),@('A4',2.0),
  @('-',1.0),@('E4',1.0),@('G4',1.5),@('F4',0.5), @('E4',1.0),@('D4',1.0),@('E4',2.0)
) $bpm 'square' 0.22 $total
$bass = Render @(
  @('A2',4),@('A2',4),@('F2',4),@('E2',4)
) $bpm 'tri' 0.30 $total
WriteWav 'dungeon.wav' @($lead, $bass) $total

# --- Battle: driving minor, 138 BPM, 16 beats (~7s) ---
$bpm = 138.0; $total = [int]($rate * 60.0 / $bpm * 16)
$lead = Render @(
  @('E5',0.5),@('E5',0.25),@('E5',0.25),@('G5',0.5),@('E5',0.5), @('D5',0.5),@('B4',0.5),@('D5',0.5),@('E5',0.5),
  @('C5',0.5),@('C5',0.25),@('C5',0.25),@('E5',0.5),@('C5',0.5), @('B4',0.5),@('G4',0.5),@('B4',0.5),@('D5',0.5),
  @('E5',0.5),@('G5',0.5),@('A5',0.5),@('G5',0.5), @('E5',0.5),@('D5',0.5),@('B4',0.5),@('D5',0.5),
  @('C5',0.5),@('E5',0.5),@('D5',0.5),@('B4',0.5), @('E5',1.0),@('-',1.0)
) $bpm 'square' 0.30 $total
$bass = Render @(
  @('E2',0.5),@('E2',0.5),@('E2',0.5),@('E2',0.5),@('E2',0.5),@('E2',0.5),@('E2',0.5),@('E2',0.5),
  @('C2',0.5),@('C2',0.5),@('C2',0.5),@('C2',0.5),@('B1',0.5),@('B1',0.5),@('B1',0.5),@('B1',0.5),
  @('E2',0.5),@('E2',0.5),@('E2',0.5),@('E2',0.5),@('E2',0.5),@('E2',0.5),@('E2',0.5),@('E2',0.5),
  @('C2',0.5),@('C2',0.5),@('C2',0.5),@('C2',0.5),@('B1',0.5),@('B1',0.5),@('E2',1.0)
) $bpm 'tri' 0.30 $total
WriteWav 'battle.wav' @($lead, $bass) $total

Write-Output 'Audio generation complete.'
