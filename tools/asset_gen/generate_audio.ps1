# Crystal Dungeons — deterministic audio generator (M15 slice music; M21 full
# soundscape). Synthesizes all music loops and jingles (square lead + triangle
# bass, art_bible §9), the four ambience beds (shaped noise), and the fifteen
# SFX roles as 22050 Hz 16-bit mono WAVs under assets/audio/. Fully original;
# reruns are byte-identical.

$ErrorActionPreference = 'Stop'
$repo = Split-Path (Split-Path $PSScriptRoot -Parent) -Parent
$outDir = Join-Path $repo 'assets\audio\music'
$sfxDir = Join-Path $repo 'assets\audio\sfx'
$ambDir = Join-Path $repo 'assets\audio\ambience'
New-Item -ItemType Directory -Force $outDir | Out-Null
New-Item -ItemType Directory -Force $sfxDir | Out-Null
New-Item -ItemType Directory -Force $ambDir | Out-Null
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

# Normalizes $mix to $peakTarget and writes a PCM16 mono RIFF WAV at $path.
function SaveWav([string]$path, [double[]]$mix, [double]$peakTarget) {
  $total = $mix.Length
  $peak = 0.0; foreach ($v in $mix) { $a = [math]::Abs($v); if ($a -gt $peak) { $peak = $a } }
  if ($peak -lt 0.0001) { $peak = 1.0 }
  $data = New-Object byte[] ($total * 2)
  for ($i = 0; $i -lt $total; $i++) {
    $s = [Int16]([math]::Round($mix[$i] / $peak * $peakTarget * 32000))
    $data[$i*2] = [byte]($s -band 0xFF); $data[$i*2+1] = [byte](($s -shr 8) -band 0xFF)
  }
  $ms = New-Object System.IO.MemoryStream; $w = New-Object System.IO.BinaryWriter($ms)
  $w.Write([Text.Encoding]::ASCII.GetBytes('RIFF')); $w.Write([int](36 + $data.Length))
  $w.Write([Text.Encoding]::ASCII.GetBytes('WAVEfmt ')); $w.Write([int]16)
  $w.Write([Int16]1); $w.Write([Int16]1); $w.Write([int]$rate); $w.Write([int]($rate*2))
  $w.Write([Int16]2); $w.Write([Int16]16)
  $w.Write([Text.Encoding]::ASCII.GetBytes('data')); $w.Write([int]$data.Length); $w.Write($data)
  [IO.File]::WriteAllBytes($path, $ms.ToArray()); $w.Dispose()
  Write-Output "  $(Split-Path $path -Leaf) ($([math]::Round($total/$rate,1))s)"
}

function WriteWav([string]$name, [double[][]]$channels, [int]$total) {
  $mix = New-Object double[] $total
  foreach ($ch in $channels) { for ($i = 0; $i -lt $total; $i++) { $mix[$i] += $ch[$i] } }
  SaveWav (Join-Path $outDir $name) $mix 0.82
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

# --- Dungeon (Ruined Keep): sparse minor with drone, 80 BPM, 16 beats (~12s) ---
$bpm = 80.0; $total = [int]($rate * 60.0 / $bpm * 16)
$lead = Render @(
  @('A4',1.0),@('C5',1.0),@('E5',1.5),@('D5',0.5), @('C5',1.0),@('B4',1.0),@('A4',2.0),
  @('-',1.0),@('E4',1.0),@('G4',1.5),@('F4',0.5), @('E4',1.0),@('D4',1.0),@('E4',2.0)
) $bpm 'square' 0.22 $total
$bass = Render @(
  @('A2',4),@('A2',4),@('F2',4),@('E2',4)
) $bpm 'tri' 0.30 $total
WriteWav 'dungeon_keep.wav' @($lead, $bass) $total

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

# ============================ M21 music ============================

# --- Title: stately major with suspensions, 96 BPM, 16 beats (~10s) ---
$bpm = 96.0; $total = [int]($rate * 60.0 / $bpm * 16)
$lead = Render @(
  @('G4',1.0),@('C5',1.0),@('E5',1.0),@('D5',0.5),@('C5',0.5),
  @('D5',1.0),@('B4',1.0),@('G4',2.0),
  @('A4',1.0),@('C5',1.0),@('E5',1.0),@('D5',0.5),@('B4',0.5),
  @('C5',1.5),@('G4',0.5),@('C5',2.0)
) $bpm 'square' 0.28 $total
$bass = Render @(
  @('C3',4),@('A2',4),@('F2',4),@('G2',4)
) $bpm 'tri' 0.28 $total
WriteWav 'title.wav' @($lead, $bass) $total

# --- Guild: preparation march in D dorian, 112 BPM, 16 beats (~8.6s) ---
$bpm = 112.0; $total = [int]($rate * 60.0 / $bpm * 16)
$lead = Render @(
  @('D5',0.5),@('F5',0.5),@('A5',0.5),@('F5',0.5), @('E5',0.5),@('D5',0.5),@('E5',0.5),@('F5',0.5),
  @('D5',0.5),@('F5',0.5),@('G5',0.5),@('A5',0.5), @('C5',0.5),@('D5',0.5),@('E5',0.5),@('C5',0.5),
  @('D5',0.5),@('A4',0.5),@('D5',0.5),@('F5',0.5), @('E5',0.5),@('C5',0.5),@('B4',0.5),@('C5',0.5),
  @('D5',1.0),@('A4',1.0),@('D5',2.0)
) $bpm 'square' 0.28 $total
$bass = Render @(
  @('D3',2),@('D3',2),@('C3',2),@('C3',2), @('D3',2),@('F2',2),@('G2',2),@('A2',2)
) $bpm 'tri' 0.28 $total
WriteWav 'guild.wav' @($lead, $bass) $total

# --- Dungeon (Crystal Mine): glinting arpeggios over drone, 92 BPM, 16 beats ---
$bpm = 92.0; $total = [int]($rate * 60.0 / $bpm * 16)
$lead = Render @(
  @('E5',0.5),@('G5',0.5),@('B5',0.5),@('G5',0.5), @('E5',0.5),@('B5',0.5),@('G5',0.5),@('E5',0.5),
  @('D5',0.5),@('F#5',0.5),@('A5',0.5),@('F#5',0.5), @('D5',0.5),@('A5',0.5),@('F#5',0.5),@('D5',0.5),
  @('E5',0.5),@('G5',0.5),@('B5',0.5),@('E6',0.5), @('B5',0.5),@('G5',0.5),@('E5',0.5),@('B4',0.5),
  @('C5',0.5),@('E5',0.5),@('A5',0.5),@('E5',0.5), @('B4',1.0),@('E5',1.0)
) $bpm 'square' 0.20 $total
$bass = Render @(
  @('E2',8),@('D2',4),@('E2',4)
) $bpm 'tri' 0.30 $total
WriteWav 'dungeon_mine.wav' @($lead, $bass) $total

# --- Dungeon (Hollow Forest): minor waltz, 102 BPM, 18 beats (6 bars of 3) ---
$bpm = 102.0; $total = [int]($rate * 60.0 / $bpm * 18)
$lead = Render @(
  @('A4',1.0),@('C5',0.5),@('E5',0.5),@('C5',1.0),
  @('B4',1.0),@('D5',0.5),@('F5',0.5),@('D5',1.0),
  @('C5',1.0),@('E5',0.5),@('A5',0.5),@('E5',1.0),
  @('B4',1.0),@('D5',0.5),@('G5',0.5),@('D5',1.0),
  @('A4',1.0),@('C5',0.5),@('E5',0.5),@('C5',1.0),
  @('B4',1.5),@('E4',0.5),@('A4',1.0)
) $bpm 'square' 0.24 $total
$bass = Render @(
  @('A2',3),@('G2',3),@('A2',3),@('E2',3),@('F2',3),@('E2',3)
) $bpm 'tri' 0.28 $total
WriteWav 'dungeon_forest.wav' @($lead, $bass) $total

# --- Boss: driving chromatic minor, 152 BPM, 16 beats (~6.3s) ---
$bpm = 152.0; $total = [int]($rate * 60.0 / $bpm * 16)
$lead = Render @(
  @('A4',0.5),@('A4',0.25),@('A4',0.25),@('C5',0.5),@('A4',0.5), @('D#5',0.5),@('D5',0.5),@('C5',0.5),@('A4',0.5),
  @('A4',0.5),@('A4',0.25),@('A4',0.25),@('C5',0.5),@('E5',0.5), @('F5',0.5),@('E5',0.5),@('D#5',0.5),@('E5',0.5),
  @('G5',0.5),@('F5',0.5),@('E5',0.5),@('D5',0.5), @('C5',0.5),@('B4',0.5),@('C5',0.5),@('D#5',0.5),
  @('E5',0.5),@('C5',0.5),@('A4',0.5),@('G#4',0.5), @('A4',2.0)
) $bpm 'square' 0.30 $total
$bass = Render @(
  @('A2',0.5),@('A2',0.5),@('A2',0.5),@('A2',0.5), @('A2',0.5),@('A2',0.5),@('G2',0.5),@('G#2',0.5),
  @('A2',0.5),@('A2',0.5),@('A2',0.5),@('A2',0.5), @('F2',0.5),@('F2',0.5),@('E2',0.5),@('E2',0.5),
  @('A2',0.5),@('A2',0.5),@('A2',0.5),@('A2',0.5), @('A2',0.5),@('A2',0.5),@('G2',0.5),@('G#2',0.5),
  @('F2',0.5),@('F2',0.5),@('E2',0.5),@('E2',0.5), @('A2',2.0)
) $bpm 'tri' 0.32 $total
WriteWav 'boss.wav' @($lead, $bass) $total

# --- Victory: one-shot fanfare, 128 BPM, 7.5 beats (~3.5s, no loop) ---
$bpm = 128.0; $total = [int]($rate * 60.0 / $bpm * 7.5)
$lead = Render @(
  @('G4',0.25),@('C5',0.25),@('E5',0.25),@('G5',0.75),
  @('E5',0.25),@('G5',0.25),@('C6',1.0),
  @('B5',0.5),@('A5',0.5),@('G5',0.5),@('C6',3.0)
) $bpm 'square' 0.30 $total
$bass = Render @(
  @('C3',1.5),@('E3',1.5),@('F3',0.75),@('G3',0.75),@('C3',3.0)
) $bpm 'tri' 0.28 $total
WriteWav 'victory.wav' @($lead, $bass) $total

# --- Defeat: one-shot dirge, 60 BPM, 4.5 beats (~4.5s, no loop) ---
$bpm = 60.0; $total = [int]($rate * 60.0 / $bpm * 4.5)
$lead = Render @(
  @('A4',1.0),@('G4',1.0),@('F4',1.0),@('E4',1.5)
) $bpm 'square' 0.22 $total
$bass = Render @(
  @('A2',1.0),@('E2',1.0),@('F2',1.0),@('E2',1.5)
) $bpm 'tri' 0.30 $total
WriteWav 'defeat.wav' @($lead, $bass) $total

# --- Result: calm reflection, 84 BPM, 16 beats (~11.4s) ---
$bpm = 84.0; $total = [int]($rate * 60.0 / $bpm * 16)
$lead = Render @(
  @('E5',1.0),@('C5',1.0),@('D5',1.0),@('G4',1.0),
  @('A4',1.0),@('C5',1.0),@('B4',2.0),
  @('E5',1.0),@('D5',1.0),@('C5',1.0),@('A4',1.0),
  @('G4',1.0),@('B4',1.0),@('C5',2.0)
) $bpm 'square' 0.20 $total
$bass = Render @(
  @('C3',4),@('F2',4),@('G2',4),@('C3',4)
) $bpm 'tri' 0.24 $total
WriteWav 'result.wav' @($lead, $bass) $total

# ============================ M21 ambience ============================

# Shaped-noise beds: xorshift white noise through a one-pole lowpass, with a
# whole-cycle amplitude LFO (seam-consistent) and optional deterministic sine
# chirps (birds/drips), normalized quiet.
function WriteAmbience([string]$name, [double]$seconds, [double]$lp, [double]$lfoCycles,
                       [double]$lfoDepth, [object[]]$chirps, [uint32]$seed) {
  $total = [int]($rate * $seconds)
  $mix = New-Object double[] $total
  $state = $seed; $y = 0.0
  for ($i = 0; $i -lt $total; $i++) {
    $state = $state -bxor ($state -shl 13); $state = $state -band 0xFFFFFFFF
    $state = $state -bxor ($state -shr 17)
    $state = $state -bxor ($state -shl 5);  $state = $state -band 0xFFFFFFFF
    $x = (($state -band 0xFFFF) / 32768.0) - 1.0
    $y += $lp * ($x - $y)
    $t = $i / [double]$total
    $lfo = 1.0 - $lfoDepth * 0.5 + $lfoDepth * 0.5 * [math]::Sin(2.0 * [math]::PI * $lfoCycles * $t)
    $mix[$i] = $y * $lfo
  }
  foreach ($c in $chirps) {
    $start = [int]($rate * $c[0]); $f0 = $c[1]; $f1 = $c[2]
    $len = [int]($rate * $c[3]); $amp = $c[4]
    for ($i = 0; $i -lt $len -and ($start + $i) -lt $total; $i++) {
      $frac = $i / [double]$len
      $f = $f0 + ($f1 - $f0) * $frac
      $env = [math]::Exp(-4.0 * $frac)
      $mix[$start + $i] += $amp * $env * [math]::Sin(2.0 * [math]::PI * $f * $i / $rate)
    }
  }
  SaveWav (Join-Path $ambDir $name) $mix 0.5
}

Write-Output 'Generating ambience beds...'
# Town: light breeze + two bright bird chirps.
WriteAmbience 'town.wav' 10.0 0.12 3 0.4 @(
  @(2.8, 2200, 1800, 0.12, 0.5), @(6.3, 1900, 2400, 0.10, 0.4)
) 0x21B2C3D4
# Ruined Keep: hollow low wind, slow swell.
WriteAmbience 'keep.wav' 12.0 0.03 2 0.6 @() 0x3EEFCAFE
# Crystal Mine: faint hum with sparse echoing drips.
WriteAmbience 'mine.wav' 12.0 0.05 2 0.3 @(
  @(1.7, 1400, 900, 0.09, 0.7), @(1.9, 1400, 900, 0.07, 0.3),
  @(4.9, 1600, 1000, 0.09, 0.6), @(5.1, 1600, 1000, 0.07, 0.25),
  @(8.6, 1300, 850, 0.09, 0.65), @(8.8, 1300, 850, 0.07, 0.28)
) 0x0DDBA11
# Hollow Forest: leaf rustle with low bird calls.
WriteAmbience 'forest.wav' 10.0 0.18 5 0.5 @(
  @(3.4, 1400, 1100, 0.14, 0.35), @(7.2, 1250, 1500, 0.12, 0.3)
) 0x70E57000

# ============================ M21 SFX ============================

# Each SFX is a sequence of segments @(wave, f0, f1, seconds, amp, decay)
# rendered back to back; wave is sine/square/tri/noise, decay shapes an
# exponential envelope over the segment.
function WriteSfx([string]$name, [object[]]$segs, [double]$peak) {
  $samples = New-Object System.Collections.Generic.List[double]
  foreach ($s in $segs) {
    $wave = $s[0]; $f0 = $s[1]; $f1 = $s[2]
    $len = [int]($rate * $s[3]); $amp = $s[4]; $decay = $s[5]
    $state = [uint32]0x1234567
    for ($i = 0; $i -lt $len; $i++) {
      $frac = $i / [double]$len
      $f = $f0 + ($f1 - $f0) * $frac
      $t = $i / [double]$rate
      $v = 0.0
      if ($wave -eq 'noise') {
        $state = $state -bxor ($state -shl 13); $state = $state -band 0xFFFFFFFF
        $state = $state -bxor ($state -shr 17)
        $state = $state -bxor ($state -shl 5); $state = $state -band 0xFFFFFFFF
        $v = (($state -band 0xFFFF) / 32768.0) - 1.0
      } elseif ($wave -eq 'square') {
        $phase = ($t * $f) % 1.0
        $v = -1.0; if ($phase -lt 0.5) { $v = 1.0 }
      } elseif ($wave -eq 'tri') {
        $phase = ($t * $f) % 1.0
        $v = 4.0 * [math]::Abs($phase - 0.5) - 1.0
      } else {
        $v = [math]::Sin(2.0 * [math]::PI * $f * $t)
      }
      $env = [math]::Exp(-$decay * $frac)
      $samples.Add($v * $env * $amp)
    }
  }
  SaveWav (Join-Path $sfxDir $name) $samples.ToArray() $peak
}

Write-Output 'Generating SFX...'
WriteSfx 'move.wav'      (,@('sine', 880, 880, 0.045, 0.5, 5.0)) 0.55
WriteSfx 'confirm.wav'   @(@('sine', 523, 523, 0.06, 0.5, 3.0), @('sine', 784, 784, 0.09, 0.5, 4.0)) 0.65
WriteSfx 'cancel.wav'    (,@('sine', 440, 300, 0.10, 0.5, 4.0)) 0.6
WriteSfx 'error.wav'     @(@('square', 165, 160, 0.06, 0.4, 2.0), @('square', 155, 150, 0.08, 0.4, 3.0)) 0.5
WriteSfx 'hit.wav'       @(@('noise', 0, 0, 0.05, 0.6, 3.0), @('sine', 180, 90, 0.08, 0.7, 5.0)) 0.7
WriteSfx 'hit_magic.wav' @(@('sine', 1400, 500, 0.12, 0.5, 4.0), @('tri', 700, 250, 0.06, 0.4, 5.0)) 0.6
WriteSfx 'heal.wav'      @(@('sine', 523, 523, 0.06, 0.4, 2.0), @('sine', 659, 659, 0.06, 0.45, 2.0), @('sine', 784, 784, 0.12, 0.5, 3.0)) 0.6
WriteSfx 'status.wav'    @(@('sine', 587, 880, 0.06, 0.45, 2.0), @('sine', 880, 587, 0.07, 0.4, 3.0)) 0.55
WriteSfx 'ko.wav'        @(@('sine', 330, 90, 0.22, 0.6, 3.0), @('noise', 0, 0, 0.08, 0.25, 5.0)) 0.65
WriteSfx 'victory.wav'   @(@('sine', 523, 523, 0.08, 0.5, 1.5), @('sine', 659, 659, 0.08, 0.5, 1.5), @('sine', 1046, 1046, 0.22, 0.55, 3.0)) 0.65
WriteSfx 'defeat.wav'    (,@('sine', 220, 90, 0.40, 0.55, 2.5)) 0.6
WriteSfx 'chest.wav'     @(@('sine', 784, 784, 0.06, 0.5, 2.0), @('sine', 988, 988, 0.06, 0.5, 2.0), @('sine', 1175, 1175, 0.12, 0.55, 3.0)) 0.65
WriteSfx 'step.wav'      (,@('noise', 0, 0, 0.028, 0.3, 6.0)) 0.3
WriteSfx 'door.wav'      @(@('sine', 180, 120, 0.08, 0.6, 3.0), @('noise', 0, 0, 0.03, 0.2, 6.0)) 0.55
WriteSfx 'interact.wav'  (,@('sine', 659, 880, 0.09, 0.5, 3.0)) 0.55

Write-Output 'Audio generation complete.'
