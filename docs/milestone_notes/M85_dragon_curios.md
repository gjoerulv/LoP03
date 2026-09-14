# M85 — The Dragon & curio lore

> **Superseded in part (M115, 2026-09-02):** the Dragon's sprite was redrawn
> as a dragon silhouette and its clone now wears the same art
> (`Combatant.bossArt`); see `docs/milestone_notes/M115_last_dragon.md`.

Authorized 2026-08-05 as part of the M75–M86 expansion program (see the
program section in `docs/milestones.md`). Implemented 2026-08-07 on the
post-M84 checkout (`a047590`).

## A. Status

**☑ complete (approved)** — implemented 2026-08-07; approved and
committed by the owner 2026-08-07 (`f562d5c`). Evidence in §F.

## B. Goal (owner brief)

The twelve curios finally talk — each inspectable for funny, Deadly-Duck-
related lore — and completing the collection unlocks the King's nemesis at
the castle: **the Dragon**, a Duck-grade gauntlet introduced by a new
jester with dry lore.

## C. What was built

### Curio inspection

- `data/curio_lore.json` — the SECOND optional content file (the M80
  terms): 12 dry Duck-mythology entries, one per curio; `CurioLoreDef` +
  `findCurioLore` on the database; the loader owns shape/duplicates while
  known-ness and 12/12 coverage are `[dragon]`-battery lints (the curio
  table lives a layer above the content loader — recorded reasoning).
- The Maps screen's curio grid gained a **cursor** (4×3, clamped edges)
  and an **inspect** action: Confirm on an owned curio opens a modal lore
  panel (wrap-checked, `maps.lore`); an unfound curio keeps its secret; a
  missing lore entry falls back to the curio's own M66 description.

### Debug aids (owner request, 2026-08-07)

- The debug menu (pause → Debug, `CRYSTAL_DEBUG_OVERLAY` builds only)
  gained two rows for §G's "debug-assisted" steps: **"Grant map piece"**
  (routes through the REAL M65/M83 `grantMapPiece`, so a fourth piece
  fires a genuine reveal with a seeded guard roster — the debug path can
  never drift from the shipped rule; the row's suffix shows the pouch or
  the standing reveal) and **"Grant next curio"** (the next unowned in
  table order; twelve presses open the Dragon's gate; suffix `N/12`).
  Structurally absent from Release, like the rest of the menu.

### The Dragon at the castle

- The castle menu gained **"Fight the Dragon"** (row 4; the pitch
  tightened 18 → 16 px for the eighth row). The row stays enabled while
  gated (the M84 guild-row rule): under 12/12 curios, Confirm pushes the
  **Pale Jester's** counting refusal — the funny help text, with your
  actual count. At 12/12 the gauntlet begins.
- **The Pale Jester** (original character; "a paler one counts on the
  stair" joins the hub's flavor line): his authored tale — the part the
  other jester leaves out — is story beat 10 (`kDragonJesterBeat`; the
  story loader's town range widened 1..9 → 1..10) and sits on top of the
  gauntlet's opening fight until the Dragon first falls.
- **Gauntlet** (`CastleChallenge::Dragon` on the castle runner): three
  seeded elite **vigil waves** (four picks each from the whole elite
  roster, never a bossOnly court, pure-hashed from the fixed
  `kDragonSeed` — the same gauntlet every attempt), then the Dragon
  **alone**, all at the Duck's 500 % reference scale. Persistent HP/MP,
  no free healing, the castle defeat price, best-turns record
  (`castleDragonBestTurns`, optional save field) on the castle records
  panel. Boss anthem, castle backdrop; the M71 celebration on a win;
  **Wyrmbane** ("Fell the Last Dragon.") on the first.

### The Dragon itself (`the_dragon`, all on the v15 vocabulary)

- **The largest fight in the game**: base **1400 HP** → 7,000 effective
  (the Duck: 5,000). Sim-tuned DOWN from the brief's ~2000 (§E.2).
- **Immunity matrix as briefed**: immune to confusion, silence, blind,
  terrified, stunned and sleep — **ATK−, DEF−, Curse and Poison land**
  (the counterplay doors; the M75 stat-scaled poison is real damage
  against 7,000 HP). Immune to **Fire** (§E.1). **Spoon-proof**
  (`immuneToStatScale` — the §G question, answered with the Duck
  precedent: a halvable superboss is no superboss). Never uses Reflect.
- **Six breaths**, one per element, each all-party magic (`mpCost` 20
  paces them: an opening barrage of ~6 breaths from his scaled MP pool,
  then the bites — recorded tuning). Passives: First Strike, Lifedrink,
  Spell Ward.
- **Triggers**: below 50 % — `drain_foe_mp` 100 % (once; the party's
  whole MP, as briefed); below 10 % — ATK & SPD ×2; at 5 % —
  `summon_clone` at 5 % of full HP (the clone gag: at the brink, there
  are suddenly two endings).
- Excluded from the Boss Rush and the generator's fallback boss sweep
  (both guards added BEFORE the content landed, so generation and the
  rush stayed byte-identical), and thereby from the Endless boss draw.
- Sprite: a 36×36 hand grid — one coiled hill-mass of void, stone
  belly-bands, gold horns/claws, a spiked wing ridge, ONE open red eye
  (review sheet `docs/sprite_review/dragon_contact.png`).

## D. Schema, save & version implications

- New optional content file `curio_lore.json`; new boss + six breath
  skills; story town range 1..10; new optional save field
  `castleDragonBestTurns`. **No rules/generation/score version motion**:
  the one engine change (§E.1) is provably inert for every pre-M85
  battle — the [elements] lint has always guaranteed no shipped foe
  carried a fire/holy immunity, so the new branch could never fire
  before the Dragon existed (the M75 inert-hook precedent).

## E. Deviations & decisions

1. **The fire-immunity conflict, resolved by honoring both owner
   decisions.** The M85 brief (2026-08-05) specs the Dragon fire-immune;
   the later-approved M81 narrowing (2026-08-06) says INTRINSIC fire/holy
   attack elements "may never meet an immunity anywhere", and its lint
   swept every boss. Shipped resolution: the Dragon is fire-immune
   against all **wielded and skill** fire (the informed trade the
   narrowing permits — shop chip, bestiary, "Immune" float), while a
   **milestone-granted intrinsic** element now resolves at neutral 100 %
   against ANY immunity — carried by the engine
   (`Combatant::elementIntrinsic` + one branch in `attackerElementMod`,
   with the immune mark/log/rider gated on the same effective modifier,
   so damage and presentation always agree). Thematically: thrown fire
   fizzles; dragonfire answers dragonfire. **Veto path**: say the word
   and either the immunity becomes absolute (revert the engine branch)
   or is dropped (delete one JSON field) — both one-line changes.
2. **HP sim-tuned 2000 → 1400** (the brief's "~2000, sim-tuned, value
   recorded here"): at 2000 (10,000 effective) the castle battery's
   maxed party could not finish after the 50 % MP deplete — the fight
   out-lasted every sustain the sim can script. 1400 keeps him the
   game's largest fight by a clear margin (7,000 vs the Duck's 5,000)
   and clears at the castle counterplay bar. Owner judgment on the felt
   difficulty is §G's real question.
3. **Breath pacing via mpCost 20** (~6 breaths, then bites) — same
   sim-tuning pass, recorded.
4. The vigil-wave shape (3 × 4 elites) is the conservative reading of
   "three seeded-random elite waves"; sizes are constants
   (`kDragonWaveCount`/`kDragonWaveSize`) if the owner wants them moved.
5. **No new music** — the Dragon fights to the ordinary boss anthem
   (a bespoke anthem is a new-asset owner call, §H).
6. The Pale Jester's tale repeats before every attempt UNTIL the first
   victory, then never again (deterministic from the record — no new
   "heard" flag).
7. **The Duck cedes exactly one supremacy.** M61's lint held that the
   Duck's effective stats top every authored context; the M85 brief
   gives the Dragon "the highest HP in the game". The lint now exempts
   the Dragon and asserts the inversion explicitly (Dragon 7,000 > Duck
   5,000 effective HP); the Duck keeps every other crown.

## F. Automated validation (evidence)

- `[dragon]` battery (new, 10 cases): the exact immunity matrix incl.
  the poison door and the Spoon answer; breath coverage (6 elements,
  all-party, magic); the three triggers + the prebuilt clone slot;
  vigil determinism/size/tier and the past-the-end sentinel; every
  exclusion (rush, themes, generated dungeons incl. the theme-less
  fallback, the Endless draw); records + save round-trip; lore
  coverage both directions + the defensive miss; the Pale Jester's
  beat; Wyrmbane; and the gauntlet sim-cleared by the maxed party —
  recorded: vigils 3 + 3 + 3 rounds, the Dragon 13, **22 rounds total
  at 500 %**.
- New engine test in [elements]: "an intrinsic element is never
  nullified" (neutral damage, no immune float/log, the rider lands, the
  weakness path untouched); the content lint narrowed to pin the Dragon
  as the game's ONE authored fire immunity.
- Updated pins: skillCount 72→78, bossCount 21→22, kAchievementCount
  19→20, the castle deepest-dungeon sweep and the [offense] censuses
  gained the Dragon.
- Debug build: **passed**. Full Debug suite: **722/722 passed**.
  Release build: **passed**; full Release suite: **718/718 passed**.
  Capture sweep: **97/97 scenes clean** — `33_castle_hub` (grown menu +
  Dragon records row), `96_curio_lore`, `97_dragon_jester` visually
  spot-checked (the castle menu pitch fix came from this review). Two
  pre-existing lints tripped by design and were updated with their
  reasoning pinned in place: the story census (9 → 10 beats) and the
  Duck-supremacy sweep (§E.7).

## G. Owner manual validation

1. Open Maps with some curios owned: walk the grid, inspect a few — is
   the Duck-mythology register right? Confirm an unfound curio refuses.
2. At the castle with fewer than 12 curios: Confirm "Fight the Dragon"
   and read the Pale Jester's refusal (the count must be yours).
3. Complete the dozen (debug-assisted), return: the tale plays, then the
   vigils, then the Dragon. Judge difficulty vs the Duck — the breath
   barrage, the mid-fight MP theft, the sub-10 % panic, the clone gag at
   the end. This is the milestone's real question (§E.2).
4. A Dragon-class hero with no weapon: its fire bite must still deal
   plain damage to the Dragon (no "Immune" float); a fire WEAPON on
   anyone else must fizzle with the float.
5. Refight after winning: no tale, records update, Wyrmbane toasted once.
6. Load a pre-M85 save: the Dragon row present but gated, Maps
   unchanged except the new cursor.

## H. Known limitations

- No bespoke Dragon anthem or backdrop (owner call; boss track + castle
  stage today).
- The Pale Jester has no sprite/NPC body — he lives in the flavor line
  and his dialogs (the castle hub is a menu, not a walkable scene).
- The clone inherits the 5 %-HP snapshot the M75 machinery defines;
  felling the original with the clone standing still ends the fight the
  way any boss battle ends (victory needs every foe down — both
  Dragons).
- CrystalForge cannot edit `curio_lore.json` until M86 (in its scope).
  *(Resolved: M86 made it an editor category.)*

## I. Final status

`complete (approved)` — owner approval 2026-08-07, committed as
`f562d5c`.
