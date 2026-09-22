# M129 — Enemy & boss sprite redesign (the Atari-ST bar)

**Status:** implemented, awaiting manual approval
**Authorization:** the M128–M130 "Peak 80s pixel art on Atari" program,
authorized 2026-09-22 by plan approval (see the M128 note for the program's
framing). Branch `oyb13`, baseline `5f6ff56` (v0.9.1).
**Version motion:** none. Battle rules 19, generation 25, save v1, settings
v1, content v1, manifest v2, `project(VERSION)` 0.9.1. No id, path, manifest
row or canvas size changed — only pixels.

## Scope (the owner's words, 2026-09-22)

> Redesign all enemies, except: The final Dragon and all Duck/Goose enemies.
> They look a bit too primitive. Again. It should look impressive but
> primitive. Think Peak 80s pixel art on Atari.

Clarified in the interview: the **late-80s Atari ST look** (bold flat fills,
hard edges, three to five colours per sprite, strong silhouettes, chunky
readable detail, little or no dithering); the canvases stay **24×24 /
36×36**; the geese and the Last Dragon, which already read this way, are the
quality bar.

## What was built

**81 sprites redrawn, 19 byte-identical.** Redrawn: the 57 non-fowl enemies
(the goblinoid & raider, undead, beast, caster, construct & protector and
buffer & stalker families, the two Royal Guards and the twelve guild
minions), 21 bosses (the thirteen regional bosses, the Hollow King, the seven
Guild Masters, the Mimic) and the three generic tier fallbacks. Untouched,
proven by `git status`: the fourteen geese and ducks (the five Evil Geese,
the eight Goosy Gauntlet fowl, the Golden Goose), the four fowl bosses (the
Gray Gander, the Mother of Ponds, the Pondlord, the Deadly Duck) and
`boss_the_dragon`.

### The style sheet applied

- Three to five ramp steps plus the ink outline (the generator's `Outline`
  pass) and at most one accent per sprite; real three-band shading with the
  light top-left; hard edges, no dithering.
- The canvas used: a normal enemy stands 18–24 rows tall where the M73 set
  had many 12–14-row blobs (the venom spider was a green disc, the wisp a
  ring, the mire imp a green blob, the plague bearer a green mound).
- Head, torso, limbs and prop as separate masses with daylight between them;
  texture from a few placed pixels (rivets, ribs, scales, studs).
- Faces that carry the read — the art bible §4 amendment approved with the
  plan: one or two deliberate ink pixels (an eye, a socket pair, a maw, a
  visor slit), never a drawn face.
- Bosses under §5b unchanged: three masses, one motif from `data/bosses.json`,
  one asymmetry, the archetype's silhouette (brute low and broad; sorcerer a
  narrow core with the magic source apart; commander upright with banner or
  blade; rush a forward wedge), no crown except the Hollow King's.
- Signal colours stay reserved: the bandit's mask is the flesh-ramp maroon,
  the medic's cross the heal green (its meaning), the Foreman's stamp the
  crimson-leaf ramp, the Royal Guards' trim the gold-leaf ramp; only the
  Hollow King wears the reward gold, as regalia won nowhere else.

### The families, one line each

- **Goblinoid & raider** — a crouched green grunt with a studded club over
  the back shoulder; an eared kobold with a dagger thrust ahead; a slouch-hat
  bandit with a sabre; a sand-cloth reaver with a scimitar flat overhead; a
  slab ogre with a log club; a lanky troll with arms to the ground; a
  riveted ironclad with an axe grounded; a black-plate dread knight with a
  greatsword planted.
- **Undead** — a skeleton archer with rib holes and a bow taller than it; a
  shambler with a torn gut; a hooded wight with bone claws; a hound whose
  ribs are holes; a hooded skull reading an enormous tome; a wall of ribs
  with pillar arms; a legless wraith with a scythe; a bloated plague bearer
  with weeping buboes.
- **Beast** — a fingered-membrane bat; an angular-legged venom spider; a
  tusked boar wedge; a running wolf; a coiled banded serpent with its head
  raised; a plated woodlouse; a frost imp with ice horns; a toothy toad imp.
- **Caster** — a cone-hooded acolyte with a floating orb; an antlered bog
  shaman with a gourd rattle; a flat-mitred gloom priest with a censer on a
  chain; a beaked blight chanter with a vial; a hard-edged cyan wisp core;
  a violet hex-wisp trailing tendrils; a faceted shardling; a thin-armed void
  weaver with a violet heart.
- **Construct & protector** — a slab golem with one glowing crack; a floating
  rune monolith; a shard-backed crystal guardian; a tower-shield sentinel; a
  squat titan with a slab shield; the twin Royal Guards (blade upright,
  crowned stave); an archon under a broken halo, feet never landing.
- **Buffer & stalker + generics** — a drum wider than the drummer; a banner
  twice the soldier's height; a horn barely lifted; a stalker with twin
  daggers held wide; a four-legged void prowler with a blade tail; the
  generic beast on all fours, rearing and horned, and the chained-void boss
  fallback.
- **Guild minions** — a paper stack with a clerk attached; an ink bird
  mid-swoop; a walking backlog of ledgers; a sprinting debt hound with an
  invoice tag; a white-robed chirurgeon with a saw and a green-cross satchel;
  a toothed strongbox on legs; a key rat; a three-tined fork fiend; a ladle
  hood over a shade; a plate warden with a chair-back shield; a blank-page
  notary with a quill; the contract that collects itself.
- **Bosses** — the Keep Warden built like his gate (battlement shoulders,
  keyhole chest, cleaver planted); the Crystal Sorcerer as a brittle column
  with shards in orbit; the Hollow Commander as EMPTY armour with a plume
  and a sash; the Rush Tyrant as a charging quadruped wedge; the Deep King
  with a mineral crest for a crown; the Blight Matron as a spotted cap over a
  veil of hyphae; the Sand Warlord in layered plates with a scarf tail and a
  crescent blade; the Frost Monarch fused into a throne frame under an ice
  crest; the Obsidian Colossus as a faceted dark mountain with one cyan
  fissure; the Hollow Sovereign as a skull under a broken circlet with a host
  banner; the Abyssal Tyrant as a maw with dorsal fins; the Dread Sovereign
  as a narrow core with a staff of thorns and afflictions in orbit; the
  Hollow King with the one literal crown, a fur-trimmed mantle and a sceptre;
  the seven Guild Masters on their credited motifs (the DENIED stamp, the
  headsman's quill, the chain and shackle, the vault door with its unturned
  keyring, THE spoon, the duelling blade, the standing void ledger); the
  Mimic polished to hard planes.

### Process (the generator discipline)

Nine batches (families in the generator's own order, bosses last), each
prototyped in a scratchpad PIL module that renders the batch at 6–8× with a
solid-black silhouette row and at 1× on the native canvas — with the
generator's outline pass replicated so the review matched the shipped
pixels — then emitted from the same data and spliced IN PLACE into
`tools/asset_gen/generate_textures.ps1` (each `Save-EnemyGrid` block's rows
and comment replaced; nothing reordered; no `Rnd`). After every batch the
generator ran and `git status` listed exactly that batch's PNGs. Review
rounds: the Hollow Commander read as a robot (plume and sash added), the
Obsidian Colossus was drawn in the darkest night step and merged with its
outline (moved to the void ramp with lit facets), the Abyssal Tyrant shared
the Rush Tyrant's wedge silhouette (dorsal fins added). Hardening: the
generator now throws on any non-boss sprite that is not 24×24 (only the
boss canvas was guarded); the M128 ramp keys moved into the main key table
so every grid section can use them. The scratchpad prototypes die with the
session; the grids in the generator are the source of truth.

## Review artefacts

`tools/asset_gen/preview.ps1` regenerated `docs/sprite_review/sprites_*`
(all 100), `bosses_*` (the sixteen non-guild bosses) and `guild_masters_*`
(the seven Masters and their twelve minions); `baseline_*` (the M73
"before") and `dragon_*` are untouched. The silhouette sheets are the
binding artefact: no two sprites in a family read as one shape.

## Decisions taken without asking (veto any of them)

1. **The three generic tier fallbacks were redrawn** with the families
   (they never ship for a real id, but a stripped catalog shows them).
2. **The 24×24 guard** was added to `Save-EnemyGrid` (in-scope hardening;
   every shipped sprite already satisfied it).
3. **The Hollow King keeps the reward gold** for his crown and trim: his
   regalia is the game's one literal crown and is "won nowhere else".
4. **The medic's cross is the heal green** rather than red — a healer's
   mark, the colour's own meaning.
5. **The Foreman's stamp is crimson** (the M128 ramp), not the danger red.

## Compatibility

No save, settings, content, generation or score change; no manifest change.
The bestiary, team inspection, the save slots' party, the victory screen,
the King's clone (`Combatant.bossArt` is a struct copy) and every capture
pick the new pixels up unchanged.

## Known limitations

- Several 24×24 designs are at the edge of what the canvas holds (the
  goblin's club, the troll's arms, the archer's bow); the owner judges
  whether they read at 1× in play.
- The Abyssal and Rush Tyrants are both charging quadrupeds by archetype;
  the fins and the open maw separate them, but they remain the closest pair
  in the boss silhouette sheet.
- The excluded geese and the Dragon were drawn under the same rules and
  stand beside the new set without a visible seam; the owner may still want
  a touch on any single sprite — its grid is one block in the generator.

## Documentation updated

`docs/milestones.md` (row + program section), this note, `docs/art_bible.md`
(§4 face amendment, §5 rule 7, §11 production paragraph),
`docs/asset_pipeline.md` (the sprite lineage and the insertion convention),
`docs/manual_test_matrix.md` (rows 282–284), `assets/credits.md`,
`docs/sprite_review/` regenerated. `docs/game_design.md` and
`docs/technical_design.md` restate no sprite facts and need nothing.

## Completion report

### 1. Implementation summary

**M129 — Enemy & boss sprite redesign.** Complete: all 81 in-scope sprites;
the 19 exclusions untouched.

### 2. Files changed

- **Assets:** 81 `assets/textures/enemies/*.png` (pixels only);
  `tools/asset_gen/generate_textures.ps1` (81 grids replaced in place, the
  24×24 guard, the key table); `assets/credits.md`;
  `docs/sprite_review/sprites_*`, `bosses_*`, `guild_masters_*`.
- **Source / tests:** none.
- **Docs:** see "Documentation updated".

### 3. Plan deviations

None; see "Decisions taken without asking".

### 4. Compatibility

See "Compatibility".

### 5. Automated validation

All run 2026-09-22 from the VS 2022 developer shell (amd64):

- `tools/asset_gen/generate_textures.ps1` — **ran clean** after every batch;
  final `git status` on `assets/textures/enemies/` lists exactly the 81
  redrawn PNGs; none of the 19 excluded files changed.
- `tools/asset_gen/preview.ps1` — the three canonical sheet sets
  regenerated and read by eye.
- `cmake --build --preset debug` — **succeeded** (the tree already carried
  the M128 code; no source changed in M129).
- `crystal_tests.exe "[lint],[m128]"` — **21 test cases, 95 984 assertions,
  all passed**.
- `ArePGeese.exe --capture <dir>` — **188/188 scenes clean**. Read by eye:
  `17`, `18`, `34`, `107` (formation, boss court, the King between his
  guards, team inspection).
- `ctest --preset debug` — **970/970 passed** (523 s).
- `cmake --build --preset release` — no source changed in M129; the Release
  build and `ctest --preset release` run once at the end of the program.

### 6. Manual owner validation

Matrix rows **282–284**: the contact and silhouette sheets, then live
battles across the themes and towns, the castle court and the Guild trial.
Whether the 81 sprites read as "impressive but primitive" and hold the
Atari-ST bar beside the geese and the Dragon is the owner's judgement.

### 7. Known limitations

See "Known limitations".

### 8. Documentation updated

See "Documentation updated".

### 9. Final status

`implemented, awaiting manual approval`
