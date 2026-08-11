# Crystal Dungeons — Standard MIDI File exporter for the game music.
# Reads the same note tables the WAV renderer uses (music_data.ps1), so the
# .mid files can never drift from the shipped chiptunes. Output goes to
# docs/music/ — deliberately OUTSIDE assets/, which tools/package.ps1 stages
# wholesale into the release zip.
#
# Format: SMF type 1, 480 ticks per quarter note. Per song: a conductor track
# (tempo, time signature, loopStart/loopEnd markers on looping tracks), a
# square-lead track (GM program 80, "Lead 1 (square)") and a bass track
# (GM program 38, "Synth Bass 1"). Velocities derive from the renderer's
# channel amplitudes; notes gate at 90% of their written length to keep the
# staccato chiptune feel. Deterministic; reruns are byte-identical.

$ErrorActionPreference = 'Stop'
$repo = Split-Path (Split-Path $PSScriptRoot -Parent) -Parent
$outDir = Join-Path $repo 'docs\music'
New-Item -ItemType Directory -Force $outDir | Out-Null

. (Join-Path $PSScriptRoot 'music_data.ps1')

$TPQN = 480

function NoteNum([string]$n) {
  # "A4" style names to MIDI numbers (C4 = 60, A4 = 69); "-" = rest (-1).
  if ($n -eq '-') { return -1 }
  $names = @{ 'C'=0; 'C#'=1; 'D'=2; 'D#'=3; 'E'=4; 'F'=5; 'F#'=6; 'G'=7; 'G#'=8; 'A'=9; 'A#'=10; 'B'=11 }
  $oct = [int]::Parse($n.Substring($n.Length-1))
  $key = $n.Substring(0, $n.Length-1)
  return $names[$key] + ($oct + 1) * 12
}

function AddVlq([System.Collections.Generic.List[byte]]$buf, [int]$value) {
  # MIDI variable-length quantity, 7 bits per byte, high bit = continuation.
  if ($value -lt 0) { throw "negative delta $value" }
  $bytes = New-Object System.Collections.Generic.List[byte]
  $bytes.Add([byte]($value -band 0x7F))
  $value = $value -shr 7
  while ($value -gt 0) {
    $bytes.Add([byte](0x80 -bor ($value -band 0x7F)))
    $value = $value -shr 7
  }
  for ($i = $bytes.Count - 1; $i -ge 0; $i--) { $buf.Add($bytes[$i]) }
}

function AddBytes([System.Collections.Generic.List[byte]]$buf, [byte[]]$bytes) {
  foreach ($b in $bytes) { $buf.Add($b) }
}

function AddMeta([System.Collections.Generic.List[byte]]$buf, [int]$delta, [byte]$type, [byte[]]$payload) {
  AddVlq $buf $delta
  $buf.Add([byte]0xFF); $buf.Add($type)
  AddVlq $buf $payload.Length
  AddBytes $buf $payload
}

function TrackChunk([System.Collections.Generic.List[byte]]$events) {
  $out = New-Object System.Collections.Generic.List[byte]
  AddBytes $out ([Text.Encoding]::ASCII.GetBytes('MTrk'))
  $len = $events.Count
  $out.Add([byte](($len -shr 24) -band 0xFF)); $out.Add([byte](($len -shr 16) -band 0xFF))
  $out.Add([byte](($len -shr 8) -band 0xFF));  $out.Add([byte]($len -band 0xFF))
  AddBytes $out $events.ToArray()
  # The comma stops PowerShell unrolling the array into the pipeline.
  return ,($out.ToArray())
}

# One melodic track: program change, then the sequence as gated note on/offs.
# $semi transposes; $amp maps to velocity exactly as the renderer maps loudness.
function MelodicTrack([string]$name, [int]$channel, [int]$program,
                      [object[]]$seq, [double]$amp, [double]$semi) {
  $ev = New-Object System.Collections.Generic.List[byte]
  AddMeta $ev 0 0x03 ([Text.Encoding]::ASCII.GetBytes($name))
  AddVlq $ev 0
  $ev.Add([byte](0xC0 -bor $channel)); $ev.Add([byte]$program)
  $velocity = [Math]::Min(110, [Math]::Max(40, [int][Math]::Round($amp * 300)))
  $cursor = 0   # tick the last emitted event landed on
  $time = 0.0   # musical position in beats
  foreach ($step in $seq) {
    $note = NoteNum $step[0]
    $startTick = [int][Math]::Round($time * $TPQN)
    $durTicks = [int][Math]::Round([double]$step[1] * $TPQN)
    if ($note -ge 0) {
      $n = $note + [int]$semi
      $gate = [Math]::Max(1, [int][Math]::Round($durTicks * 0.9))
      AddVlq $ev ($startTick - $cursor)
      $ev.Add([byte](0x90 -bor $channel)); $ev.Add([byte]$n); $ev.Add([byte]$velocity)
      AddVlq $ev $gate
      $ev.Add([byte](0x80 -bor $channel)); $ev.Add([byte]$n); $ev.Add([byte]0x40)
      $cursor = $startTick + $gate
    }
    $time += [double]$step[1]
  }
  $endTick = [int][Math]::Round($time * $TPQN)
  AddMeta $ev ([Math]::Max(0, $endTick - $cursor)) 0x2F @()
  return ,$ev
}

function WriteSongMidi([string]$fileBase, [hashtable]$song,
                       [double]$bpmOverride, [string]$titleSuffix,
                       [double]$semi, [double]$leadAmpOverride) {
  $bpm = $song.bpm
  if ($bpmOverride -gt 0) { $bpm = $bpmOverride }
  $leadAmp = $song.leadAmp
  if ($leadAmpOverride -gt 0) { $leadAmp = $leadAmpOverride }
  $bassAmp = $song.bassAmp
  if ($semi -ne 0.0) { $bassAmp = 0.28 }   # the variants' fixed bass level
  $meterNum = 4
  if ($song.ContainsKey('meterNum')) { $meterNum = $song.meterNum }
  $totalTicks = [int][Math]::Round($song.beats * $TPQN)

  # Conductor track: name, tempo, meter, loop markers (loops only), EOT.
  $cond = New-Object System.Collections.Generic.List[byte]
  AddMeta $cond 0 0x03 ([Text.Encoding]::ASCII.GetBytes("Crystal Dungeons - $fileBase$titleSuffix"))
  $usPerQn = [int][Math]::Round(60000000.0 / $bpm)
  AddMeta $cond 0 0x51 @([byte](($usPerQn -shr 16) -band 0xFF),
                         [byte](($usPerQn -shr 8) -band 0xFF),
                         [byte]($usPerQn -band 0xFF))
  AddMeta $cond 0 0x58 @([byte]$meterNum, [byte]2, [byte]24, [byte]8)  # x/4 time
  if (-not $song.jingle) {
    AddMeta $cond 0 0x06 ([Text.Encoding]::ASCII.GetBytes('loopStart'))
    AddMeta $cond $totalTicks 0x06 ([Text.Encoding]::ASCII.GetBytes('loopEnd'))
    AddMeta $cond 0 0x2F @()
  } else {
    AddMeta $cond $totalTicks 0x2F @()
  }

  $leadTrack = MelodicTrack 'Lead (square)' 0 80 $song.lead $leadAmp $semi
  $bassTrack = MelodicTrack 'Bass (triangle)' 1 38 $song.bass $bassAmp $semi

  $file = New-Object System.Collections.Generic.List[byte]
  AddBytes $file ([Text.Encoding]::ASCII.GetBytes('MThd'))
  AddBytes $file @([byte]0,[byte]0,[byte]0,[byte]6)          # header length
  AddBytes $file @([byte]0,[byte]1)                          # format 1
  AddBytes $file @([byte]0,[byte]3)                          # three tracks
  $file.Add([byte](($TPQN -shr 8) -band 0xFF)); $file.Add([byte]($TPQN -band 0xFF))
  AddBytes $file (TrackChunk $cond)
  AddBytes $file (TrackChunk $leadTrack)
  AddBytes $file (TrackChunk $bassTrack)

  $path = Join-Path $outDir "$fileBase.mid"
  [IO.File]::WriteAllBytes($path, $file.ToArray())
  Write-Output "  $fileBase.mid ($([math]::Round($song.beats * 60.0 / $bpm, 1))s @ $([math]::Round($bpm,1)) BPM)"
}

Write-Output 'Exporting music as MIDI...'
foreach ($songName in $MusicSongs.Keys) {
  WriteSongMidi $songName $MusicSongs[$songName] 0 '' 0.0 0
}
$townSong = $MusicSongs['town']
foreach ($t in 2, 3, 4, 5, 6, 7) {
  $v = $TownVariants[$t]
  WriteSongMidi "town_$t" $townSong ($townSong.bpm * $v.tempo) " (town $t variant)" $v.semi $v.amp
}
Write-Output 'MIDI export complete.'
