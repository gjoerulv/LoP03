# Crystal Dungeons — deterministic audio generator (M15 slice music; M21 full
# soundscape). Synthesizes all music loops and jingles (square lead + triangle
# bass, art_bible §9), the four ambience beds (layered: noise bed + drones +
# recurring events, each place with its own identity), and the fifteen
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
# $semi transposes every note by that many semitones (M32 darker town variants);
# it defaults to 0 so all pre-existing calls render byte-identically.
function Render([object[]]$seq, [double]$bpm, [string]$wave, [double]$amp, [int]$total, [double]$semi = 0.0) {
  $out = New-Object double[] $total
  $pos = 0
  foreach ($step in $seq) {
    $hz = NoteHz $step[0]
    if ($semi -ne 0.0 -and $hz -gt 0.0) { $hz *= [math]::Pow(2.0, $semi / 12.0) }
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

# All music note tables live in music_data.ps1 — one source of truth shared
# with generate_midi.ps1, which exports the same songs as editable .mid files.
# Rendering stays here: square lead + triangle bass per song, buffer sized
# from bpm x beats. Adding a song = adding a data entry; no other file's
# bytes shift (the old append-only discipline, now structural).
. (Join-Path $PSScriptRoot 'music_data.ps1')

foreach ($songName in $MusicSongs.Keys) {
  $song = $MusicSongs[$songName]
  $total = [int]($rate * 60.0 / $song.bpm * $song.beats)
  $lead = Render $song.lead $song.bpm $song.leadWave $song.leadAmp $total
  $bass = Render $song.bass $song.bpm 'tri' $song.bassAmp $total
  WriteWav "$songName.wav" @($lead, $bass) $total
}

# The M32 town-ladder variants (towns 2..7): the town song re-rendered darker
# per $TownVariants (transposed down, slowed, softer; tri lead in the deepest
# towns; variant bass always tri at 0.28). Town 1 keeps town.wav.
Write-Output 'Generating M32 town-ladder music variants...'
$townSong = $MusicSongs['town']
foreach ($t in 2, 3, 4, 5, 6, 7) {
  $v = $TownVariants[$t]
  $tbpm = $townSong.bpm * $v.tempo
  $ttotal = [int]($rate * 60.0 / $tbpm * $townSong.beats)
  $tlead = Render $townSong.lead $tbpm $v.wave $v.amp $ttotal $v.semi
  $tbass = Render $townSong.bass $tbpm 'tri' 0.28 $ttotal $v.semi
  WriteWav "town_$t.wav" @($tlead, $tbass) $ttotal
}

# (Title, guild, both remaining dungeon themes, boss, the jingles, result,
# and the M40 castle/king pair all render from music_data.ps1 above.)

# ============================ M21 ambience ============================

# Layered ambience (M27): each bed is defined by its own recognizable elements
# (a distinct drone character plus recurring, above-the-bed events), not just a
# filtered-noise variant, so the four places are clearly different to the ear.

# Filtered xorshift-noise bed. hpAmt>0 subtracts a slow component (brighter -
# wind/rustle); lfo is a whole-cycle amplitude swell; fast is a quicker flutter
# (leaf rustle). Returns a float[] scaled to $level.
function AmbNoise([int]$total, [uint32]$seed, [double]$lp, [double]$hpAmt,
                  [double]$lfoCyc, [double]$lfoDep, [double]$fastCyc, [double]$fastDep, [double]$level) {
  $out = New-Object double[] $total
  $state = $seed; $y = 0.0; $ys = 0.0
  for ($i = 0; $i -lt $total; $i++) {
    $state = $state -bxor ($state -shl 13); $state = $state -band 0xFFFFFFFF
    $state = $state -bxor ($state -shr 17)
    $state = $state -bxor ($state -shl 5);  $state = $state -band 0xFFFFFFFF
    $x = (($state -band 0xFFFF) / 32768.0) - 1.0
    $y += $lp * ($x - $y)
    $v = $y
    if ($hpAmt -gt 0.0) { $ys += 0.02 * ($y - $ys); $v = $y - $hpAmt * $ys }
    $t = $i / [double]$total
    $env = 1.0 - $lfoDep * 0.5 + $lfoDep * 0.5 * [math]::Sin(2.0 * [math]::PI * $lfoCyc * $t)
    if ($fastDep -gt 0.0) { $env *= (1.0 - $fastDep * 0.5 + $fastDep * 0.5 * [math]::Sin(2.0 * [math]::PI * $fastCyc * $t)) }
    $out[$i] = $v * $env * $level
  }
  return , $out  # comma keeps it a Double[]; a bare return unrolls to Object[],
                 # which the [double[]] event params would then copy (losing the
                 # in-place drone/bird/drip/hoot mixing).
}

# Seam-consistent drone (frequency snapped to whole cycles over the loop).
function AmbDrone([double[]]$mix, [double]$freq, [double]$amp, [string]$wave) {
  $total = $mix.Length; $seconds = $total / [double]$rate
  $cyc = [math]::Max(1, [math]::Round($freq * $seconds)); $f = $cyc / $seconds
  for ($i = 0; $i -lt $total; $i++) {
    $ph = ($f * $i / $rate) % 1.0
    $v = if ($wave -eq 'tri') { 4.0 * [math]::Abs($ph - 0.5) - 1.0 } else { [math]::Sin(2.0 * [math]::PI * $ph) }
    $mix[$i] += $amp * $v
  }
}

# One gliding tone with soft attack + exponential decay and a half-strength
# octave, mixed in at $sec. The building block for the event synths below.
function AmbTone([double[]]$mix, [double]$sec, [double]$f0, [double]$f1, [double]$dur,
                 [double]$amp, [double]$atk, [double]$harm) {
  $total = $mix.Length; $start = [int]($rate * $sec); $len = [int]($rate * $dur)
  for ($i = 0; $i -lt $len -and ($start + $i) -lt $total -and ($start + $i) -ge 0; $i++) {
    $frac = $i / [double]$len
    $f = $f0 + ($f1 - $f0) * $frac
    $a = if ($frac -lt $atk) { $frac / $atk } else { [math]::Exp(-3.5 * ($frac - $atk)) }
    $ph = $f * $i / $rate
    $s = [math]::Sin(2.0 * [math]::PI * $ph) + $harm * [math]::Sin(4.0 * [math]::PI * $ph)
    $mix[$start + $i] += $amp * $a * $s
  }
}

# Bright three-note bird whistle (town).
function AmbBird([double[]]$mix, [double]$sec, [double]$amp, [double]$b) {
  AmbTone $mix  $sec           $b          ($b * 0.92) 0.09  $amp        0.15 0.3
  AmbTone $mix ($sec + 0.12)  ($b * 1.18) ($b * 1.05) 0.08 ($amp * 0.9) 0.12 0.3
  AmbTone $mix ($sec + 0.22)  ($b * 0.86) ($b * 0.98) 0.10 ($amp * 0.8) 0.12 0.25
}

# --- Mine events (M74) ---------------------------------------------------
# The mine's old `AmbDrip` was a 2300->1500 Hz glide with a second glided note
# 110 ms later. That is the same band, the same downward glide and the same
# note spacing as `AmbBird` above, so the owner correctly heard birds in the
# Crystal Mine. The replacements below are unpitched or inharmonic and never
# glide, which is exactly what separates "stone and mineral" from "call".

# One short filtered-noise transient - the physical basis of stone on stone.
# Broadband and unpitched by construction, so it cannot read as a whistle.
# Small $lp = darker/heavier; the decay is steep.
function AmbClack([double[]]$mix, [double]$sec, [double]$dur, [double]$amp,
                  [double]$lp, [uint32]$seed) {
  $total = $mix.Length; $start = [int]($rate * $sec); $len = [int]($rate * $dur)
  $state = $seed; if ($state -eq 0) { $state = 1 }
  $y = 0.0
  for ($i = 0; $i -lt $len -and ($start + $i) -lt $total -and ($start + $i) -ge 0; $i++) {
    $state = $state -bxor ($state -shl 13); $state = $state -band 0xFFFFFFFF
    $state = $state -bxor ($state -shr 17)
    $state = $state -bxor ($state -shl 5);  $state = $state -band 0xFFFFFFFF
    $x = (($state -band 0xFFFF) / 32768.0) - 1.0
    $y += $lp * ($x - $y)
    $mix[$start + $i] += $amp * [math]::Exp(-14.0 * $i / [double]$len) * $y
  }
}

# A fall of loose rock: a scatter of dark stone clacks at irregular spacing (a
# tumble, never a rhythm), answered a third of a second later by the cavern at
# a third of the level and duller, since the walls eat the highs.
function AmbRockfall([double[]]$mix, [double]$sec, [double]$amp, [uint32]$seed) {
  $taps = @(0.000, 0.055, 0.088, 0.151, 0.178, 0.262)
  $lvls = @(1.00,  0.62,  0.78,  0.45,  0.30,  0.22)
  for ($k = 0; $k -lt $taps.Count; $k++) {
    AmbClack $mix ($sec + $taps[$k]) 0.075 ($amp * $lvls[$k]) 0.30 `
             ([uint32](($seed + 7919 * ($k + 1)) -band 0xFFFFFFFF))
    AmbClack $mix ($sec + 0.335 + $taps[$k] * 1.2) 0.085 ($amp * $lvls[$k] * 0.30) 0.17 `
             ([uint32](($seed + 104729 * ($k + 1)) -band 0xFFFFFFFF))
  }
}

# A crystal facet giving way: an INHARMONIC strike - plate/bar partials at
# 1 : 2.76 : 5.40 : 8.93, not a harmonic series - struck with a near-instant
# attack, no glide at all, and one dimmer echo. Inharmonic + glide-free is what
# makes this read as struck brittle mineral rather than as a voice.
function AmbShard([double[]]$mix, [double]$sec, [double]$amp, [double]$f) {
  foreach ($p in @(@(1.00, 1.00), @(2.76, 0.55), @(5.40, 0.26), @(8.93, 0.11))) {
    AmbTone $mix $sec ($f * $p[0]) ($f * $p[0]) 0.070 ($amp * $p[1]) 0.004 0.0
  }
  foreach ($p in @(@(1.00, 1.00), @(2.76, 0.45), @(5.40, 0.18))) {
    AmbTone $mix ($sec + 0.213) ($f * $p[0]) ($f * $p[0]) 0.055 ($amp * 0.26 * $p[1]) 0.004 0.0
  }
}

# Low two-note owl hoot (forest).
function AmbHoot([double[]]$mix, [double]$sec, [double]$amp) {
  AmbTone $mix  $sec          360 340 0.18  $amp        0.25 0.2
  AmbTone $mix ($sec + 0.30)  330 315 0.22 ($amp * 0.9) 0.30 0.2
}

Write-Output 'Generating ambience beds...'

# TOWN - airy daytime settlement: a soft bright breeze, defined by frequent
# melodic bird whistles that sit clearly above the bed.
$tt = [int]($rate * 10.0)
$town = AmbNoise $tt 0x21B2C3D4 0.16 0.7 3 0.4 0 0 0.055
foreach ($s in 1.3, 3.1, 4.9, 6.5, 8.2) { AmbBird $town $s 0.7 (2200 + 260 * [math]::Sin($s * 1.7)) }
SaveWav (Join-Path $ambDir 'town.wav') $town 0.5

# RUINED KEEP - cavernous ruin: a loud, hollow, slowly beating low drone
# (audible fundamental + a detuned partner + sub-octave) with distant moans.
$kt = [int]($rate * 12.0)
$keep = AmbNoise $kt 0x3EEFCAFE 0.02 0.0 1 0.7 0 0 0.13
AmbDrone $keep 98.0 0.34 'sine'
AmbDrone $keep 99.0 0.30 'sine'
AmbDrone $keep 49.0 0.20 'sine'
AmbTone  $keep 4.5 150 70 2.6 0.26 0.3 0.15
AmbTone  $keep 9.2 135 62 2.4 0.22 0.3 0.15
SaveWav (Join-Path $ambDir 'keep.wav') $keep 0.5

# CRYSTAL MINE (M74) - a dead, enclosed excavation: the steady metallic hum
# (fundamental + octave + bright partial) carries the bed, with a low settling
# rumble underneath and only four events across twelve seconds - two falls of
# loose rock and two crystal facets giving way, each answered by the cavern.
# Deliberately sparse and low against the drone so it reads as a place rather
# than as a sound effect; the ear should have to notice it.
# Every event finishes its decay by ~10.9 s, so the 12 s loop seam is silent.
$mt = [int]($rate * 12.0)
$mine = AmbNoise $mt 0x0DDBA11 0.05 0.0 2 0.2 0 0 0.050
AmbDrone $mine 146.8 0.11 'sine'
AmbDrone $mine 293.7 0.06 'sine'
AmbDrone $mine 440.0 0.03 'tri'
AmbTone  $mine 0.8 58 44 3.2 0.100 0.35 0.10      # distant settling rumble
AmbTone  $mine 6.9 52 40 3.0 0.085 0.35 0.10
AmbRockfall $mine 2.6  0.40 0x51A7E1
AmbShard    $mine 5.1  0.30 1180
AmbRockfall $mine 7.9  0.32 0xC4A5711
AmbShard    $mine 10.2 0.24 970
SaveWav (Join-Path $ambDir 'mine.wav') $mine 0.5

# HOLLOW FOREST - living woods: a busy, bright, fluttering leaf rustle with low
# owl hoots and the odd high insect tick.
$ft = [int]($rate * 10.0)
$forest = AmbNoise $ft 0x70E57000 0.24 0.85 6 0.45 47 0.5 0.13
foreach ($s in 2.1, 5.4, 8.3) { AmbHoot $forest $s 0.6 }
AmbTone $forest 3.9 3200 3600 0.05 0.28 0.05 0.2
AmbTone $forest 7.1 3400 3000 0.05 0.24 0.05 0.2
SaveWav (Join-Path $ambDir 'forest.wav') $forest 0.5

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

# ============================ M91 elemental impacts ============================
# Six distinct voices, one per element, each still reading as a "hit":
# fire crackles and roars down, ice rings glassy and high, lightning cracks,
# earth thuds low, holy blooms upward in pure fifths, dark breathes a detuned
# descent. Deterministic like everything above; reruns are byte-identical.
WriteSfx 'hit_fire.wav'      @(@('noise', 0, 0, 0.05, 0.5, 2.5), @('sine', 420, 140, 0.11, 0.6, 4.0), @('noise', 0, 0, 0.04, 0.25, 6.0)) 0.65
WriteSfx 'hit_ice.wav'       @(@('sine', 2200, 1400, 0.07, 0.45, 4.0), @('tri', 1760, 1320, 0.06, 0.4, 5.0), @('sine', 2637, 2637, 0.05, 0.3, 6.0)) 0.55
WriteSfx 'hit_lightning.wav' @(@('noise', 0, 0, 0.03, 0.6, 5.0), @('square', 1800, 300, 0.07, 0.45, 5.0), @('sine', 240, 120, 0.06, 0.4, 5.0)) 0.6
WriteSfx 'hit_earth.wav'     @(@('noise', 0, 0, 0.06, 0.5, 3.0), @('sine', 110, 55, 0.16, 0.7, 4.0)) 0.65
WriteSfx 'hit_holy.wav'      @(@('sine', 660, 660, 0.06, 0.4, 2.0), @('sine', 990, 990, 0.07, 0.45, 2.5), @('sine', 1320, 1320, 0.10, 0.5, 3.5)) 0.6
WriteSfx 'hit_dark.wav'      @(@('tri', 300, 150, 0.10, 0.5, 3.0), @('sine', 233, 110, 0.10, 0.45, 3.5), @('noise', 0, 0, 0.05, 0.15, 6.0)) 0.55

# ============================ M107 summon arrival ==============================
# One shared fanfare for the three legends: a rising major stack that blooms
# and lingers - "epic relative to the game's feel", deterministic like all.
WriteSfx 'summon.wav'        @(@('sine', 392, 392, 0.10, 0.5, 2.0), @('sine', 523, 523, 0.12, 0.5, 2.0), @('sine', 659, 784, 0.26, 0.55, 3.0), @('tri', 1046, 1046, 0.14, 0.35, 4.0)) 0.65

# ============================ M32 town-ladder music ============================
Write-Output 'Audio generation complete.'
