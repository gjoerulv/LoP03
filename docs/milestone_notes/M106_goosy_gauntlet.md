# M106 — The Goosy Gauntlet (town-7 theme)

**Status:** implemented, awaiting manual approval
**Program:** M98–M108 "Are P Geese" (owner-authorized 2026-08-16).
**Generation 20 → 21** (the new theme + its rite + the grown boss rosters
move guard picks where the roster grew; classic-theme generation is
otherwise untouched). No battle-rules or save change.

## Scope (owner items + interview)

A new town-7 dungeon theme "loosely based on geese and ducks", with new
enemies and bosses (bosses also in Boss Rush), and the unique rite: the
ENTIRE team turns to Geese for one fight, +300 score.

## What was built

- **The theme** (`goosy_gauntlet`, `dungeon_themes.json`): reeds-and-ponds
  register; 5 new normals (Pond Drake, Reed Honker, Mallard Marauder,
  Downfeather Witch, Puddle Imp — all `minTown 7`, 5 % "Honk." idle
  chance) + swamp-adjacent classics (Mire Imp, Bog Shaman); 3 new elites
  (Gander Grenadier, Cob Knight, Migration Herald) + shared late elites;
  3 new bosses. Stats sit in the shipped town-7 family (the Dread
  Sovereign band); skills reuse the existing pool — no new battle content.
- **Theme gating**: `DungeonThemeDef.minTown` (new optional field, default
  1; loader + forge descriptor) — the Guild lists a theme only from its
  minTown, so towns 1–6 keep exactly three themes.
- **The bosses** (The Gray Gander/brute, The Mother of Ponds/sorcerer,
  The Pondlord/commander — all `minTown 7`, courts drawn from the M61
  Evil Geese, who are `bossOnly` and finally have day jobs): they
  **auto-join Boss Rush and the Endless pool** via `bossRushOrder` (the
  owner's ask; the rush is now 15 fights and its records note it). The
  **treasure-guard roster is gated**: only minTown-7 bosses are excluded
  below town 7, so every historical guard pick keeps its eligibility.
- **The rite** (`RoomEventKind::GoosyFlock`, on the M55 first event slot
  as built; since the 2026-08-17 leveling it rolls at 8% per floor like
  every rite — see the M55 note's adjustment): the WHOLE party fights its next battle
  as Geese — the Dragonform machinery verbatim (same stash, same vital
  mapping, same restore; heirlooms ride along) with the sign flipped:
  **+300 score**, itemized as "Flock battles". When dragonform and the
  flock are both armed, scales take the first fight, feathers the next.
- **Presentation (INTERIM, owner judgment requested)**: the new enemies
  and bosses are manifest-mapped to the existing goose/duck art (the five
  Evil Goose sprites and the Deadly Duck), and the theme borrows the
  Hollow Forest tiles, music and ambience — real art on screen everywhere,
  ZERO placeholders, but repeats across ids. A dedicated Goosy art +
  audio pass (sprites, tiles, its own track) is the natural follow-up if
  the owner wants distinct looks; the manifest makes that a pure asset
  swap. **Because no new generated art entered the pipeline, the owed
  art-bible §2 palette reconciliation is NOT discharged here** — it stays
  owed before that real art pass (deviation, honestly flagged).

## Verification

- Tests: guard-roster gating (low towns never meet minTown-7 bosses),
  rush count 15, theme minTown loader default, goosy generation + rite
  placement + flock enter/leave + the +300 itemization (see [goosy] and
  updated [castle]/[treasure] tags). Suite + capture in the completion
  report.

## Fix round (2026-08-17 — owner manual-pass verdict: interim art REJECTED)

The owner ruled the theme must have its own graphics, music, ambience and
tiles. Delivered:

- **Palette reconciliation first** (the owed art-bible §2 debt): all 68
  generator hex literals censused and attributed; §2 now records the 25
  survivors as sanctioned groups. Nothing retinted; debt closed.
- **11 hand-authored sprites** (`generate_textures.ps1`, ASCII grids,
  RNG-free): 5 normals (shovel-billed Pond Drake, Reed Honker as a
  periscope neck in a reed blind, oar-armed Mallard Marauder, the shawled
  Downfeather Hag, the Puddle Imp splash), 3 elites (bomb-cradling Gander
  Grenadier, helmed swan Cob Knight, horn-and-banner Migration Herald),
  and 3 bosses per §5b (the Gray Gander unfolded to full height, the
  veiled Mother of Ponds on her nest with orbiting frost eggs, the
  Pondlord with reed diadem and cattail standard). Reviewed via contact +
  silhouette sheets before acceptance; every previously shipped PNG
  byte-identical (git-verified).
- **Own tile set** (`goosy_{floor,wall,door,accent}.png`): still-water
  floor, woven reed-palisade walls, parted-reed doorway, nest shrine —
  the "flooded pen" identity recorded in art_bible §8b.
- **Own battle backdrop**: `BackdropStage::Goosy` (reed-top skyline,
  cattail clumps, the pond line with a phase-stepped ripple glint) —
  goosy battles no longer fall to Plain.
- **Own music**: `dungeon_goosy.wav` — the waddling strut, G dorian
  116 BPM, sectioned A/B/breakdown/A (~29 s), from `music_data.ps1`
  (MIDI exported too); `MusicTrack::DungeonGoosy` appended.
- **Own ambience**: `goosy.wav` — the occupied-wetland bed (reed rustle,
  still-water swell, distant double honks kept below the birdcall band
  per the M74 rule, water plops, one wing flap);
  `AmbienceTrack::Goosy` appended. Existing WAVs byte-identical.
- Manifest retargeted (11 enemy/boss ids + 4 tiles to their own files;
  music + ambience entries added); capture scenes `118_dungeon_goosy`,
  `120_battle_goosy`, `121_battle_goosy_boss` added; backdrop/audio test
  pins extended.

## Deviations from the plan

- ~~Interim reused art~~ — resolved 2026-08-17 (fix round above).
- ~~Palette reconciliation deferred~~ — discharged 2026-08-17.
- Roster: 5+3 new enemies (plan said "6–8") plus the five reused geese as
  boss courts.

## Manual owner checklist

Matrix row **202** (re-test after the fix round): the theme appears only at
town 7; the dungeon shows the reed/water tiles; battles stage on the reed
backdrop with the NEW sprites (no evil-goose repeats); the strut plays and
the wetland bed honks; the rite arms and pays +300 with the whole party
honking; the three bosses fight distinctly (brute/sorcerer/commander) with
goose courts; Boss Rush now runs 15; a town-1 dig never meets a goose
guard. Row 202 carries the full art/audio expectations.

## Documentation updated

game_design §5/§6 (the fourth theme + rite), art_bible §2/§5/§7/§8b/§9b,
assets/credits.md, ledger row, matrix row 202, this note.
