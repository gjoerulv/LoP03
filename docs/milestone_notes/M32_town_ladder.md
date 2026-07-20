# M32 — Town Ladder (7 Towns, Travel, Scaling, Score Bonus)

## A. Status and authority

- **Status:** in progress — authored / re-audited 2026-07-21 against HEAD
  `09cf458` ("M31"). Owner authorized beginning M32 on 2026-07-21 after approving
  M31; §D decisions confirmed via Q&A the same day (see §D). As-implemented
  record in §K.
- **Authority:** `CLAUDE.md` > this note > `docs/milestones.md` >
  `docs/game_design.md` > `docs/technical_design.md`. Second milestone of the
  **M31–M34 expansion program**; **the structural spine** — M33 (stakes) reads
  the `(town, depth)` vector this milestone introduces, and M34 (black market)
  spawns in the current town and is balanced against town-7 difficulty.
- **Scope guard:** M33/M34 are direction only. This note implements the town
  ladder and nothing downstream of it.

## B. Problem statement (verified at re-audit, HEAD `09cf458`)

- **One town, one difficulty band.** `TownState` builds a single fixed 26×15
  layout from `town::buildTown()` ([TownData.hpp:33](src/town/TownData.hpp:33))
  in its constructor; there is no town index anywhere. The town is a single
  persistent state — pushed once from `PartyCreationState`/`SlotMenuState`
  ([PartyCreationState.cpp:84](src/states/PartyCreationState.cpp:84),
  [SlotMenuState.cpp:82](src/states/SlotMenuState.cpp:82)) — and every dungeon
  return **pops back to it** (`onResume`), so there is exactly one town to make
  town-aware, plus the transient capture instances.
- **Difficulty is depth-only.** Enemy stat scaling is
  `team.statScalePct = 100 + comp.statScalePct(depth)` at three generator sites
  ([DungeonGenerator.cpp:152](src/dungeon/DungeonGenerator.cpp:152), `:339`,
  `:467`). Base is **100 (×1.00)**; a town multiplier composes on top of it, and
  because `statScalePct` already feeds both combatant build **and** danger
  assessment, the danger label scales for free. `generate(seed, depth, db,
  themeId)` takes no town.
- **Scoring is pure and town-blind.** `scoreBreakdown(RunSummary)` sums fixed
  components and clamps to ≥0 ([Scoring.cpp:22](src/score/Scoring.cpp:22));
  `ScoreEntry` already carries three optional comparability tags
  (`generationVersion`, `partyLevel`, `battleRulesVersion`) written with no
  format bump ([ScoreEntry.hpp](src/score/ScoreEntry.hpp)). The boss-victory
  path builds the `RunSummary`, writes the `ScoreEntry`, and returns to town in
  `DungeonState`.
- **Save carries optional party state.** `Party { members, inventory, gold,
  restTokens }` ([Party.hpp:17](src/game/Party.hpp:17)); `kSaveVersion` = 1, and
  `restTokens` is the established optional-field precedent
  (`optIntMin("restTokens", 0, 0)`).
- **Music/art are role-keyed.** `MusicTrack::Town` → `music.town`
  ([AudioRoles.hpp:68](src/audio/AudioRoles.hpp)); town tiles are
  `tiles.town.<kind>` and service interiors are `bg.<place>` textures
  ([manifest.json](assets/manifest.json)); both are resolved by convention with
  a safe fallback, and the deterministic `tools/asset_gen/` generators reseed
  their RNG and append so prior PNGs stay byte-identical.

## C. Goals

- A chain of **seven towns**, each harder and higher-scoring; bottom-right exit
  → next town, bottom-left → previous. Travel between **unlocked** towns is free;
  the highest unlocked town persists.
- **Difficulty and score climb together and honestly:** a per-town enemy-stat
  multiplier (folded into `statScalePct`, multiplicative with depth) and a
  per-town **visible** score bonus. Danger labels stay absolute (§D.3).
- **Determinism & compatibility are inviolable:** town 1 output **byte-identical**
  to today (`generationVersion` stays 6), battle rules untouched
  (`battleRulesVersion` stays 1), simulator ↔ live agreement held, and every new
  save/score field optional with a safe default (old saves → town 1/1).
- **Per-town identity:** each town reads as its own place through exterior and
  **interior** art and progressively darker music, without a name label.

## D. Material design decisions (owner-confirmed 2026-07-21)

The three assumptions the plan reserved for note-time veto were put to the owner
via Q&A. Results:

### D.1 — Reward scaling → **flat (confirmed, = recommendation)**

Dungeon XP/gold rewards **do not** scale with town. Score is the sole reward for
climbing, so a higher town never becomes the better farm — the property that
makes the M33 stakes system meaningful. Only enemy **stats** and the **score
bonus** scale with town.

### D.2 — Service interiors → **per-town (confirmed; owner overrode the "shared" recommendation)**

Every service interior (Inn, Item Shop, Equip Shop, Training Hall, Scoreboard,
Guild) gets its **own per-town art**, not just the town exterior. Backgrounds
resolve `bg.<place>.<town>` with fallback to the base `bg.<place>` (town 1). This
is a deliberately larger art lift (6 services × 6 higher towns = 36 new interior
backgrounds), accepted by the owner for stronger per-town identity.

### D.3 — Danger tiers → **absolute (confirmed, = recommendation)**

Tiers are read straight from stats, so a town-7 team genuinely reads *Deadly*.
`statScalePct` already feeds the danger formula, so this is the honest reading
**and** free — no per-town threshold path.

### D.4 — Routine decisions taken locally (documented, reversible)

- **Ladder values (owner-decided at program planning):** enemy stats
  ×{100,125,150,175,200,250,300}% (towns 1–7 = +0/25/50/75/100/150/200%); score
  bonus +{0,10,20,30,40,50,100}%. Both live as one small constant table in pure
  code, unit-tested; town 1 = ×1.00 / +0% → byte-identical, unchanged scores.
- **Session-state storage:** `currentTown` and `highestUnlockedTown` are added
  to **`Party`** (beside `restTokens`), the established optional-save-field
  precedent — minimal churn versus threading a new `WorldState` through
  `AppContext` and the save path. Semantically world-state, but the save owner is
  the same. Reversible.
- **Exterior art:** per-town **tile palette variants** for the town's defining
  tiles (ground, grass, path, tree, building), resolving `tiles.town.<N>.<kind>`
  → base `tiles.town.<kind>`; water/door/flowers keep the base. A per-town
  palette *tint at render time* was considered and rejected as reading cheaper
  than genuine per-town tiles, given D.2's appetite for real per-town art.
- **Town-indexed music:** `MusicTrack::Town` resolves `music.town.<N>` (N>1)
  with fallback to `music.town` (town 1, unchanged). Implemented as a small
  `AudioManager` town field set on town enter, not an enum explosion.
- **Travel model:** taking an exit updates `currentTown` and **rebuilds the town
  layout in place** (fade + music change), rather than replacing the base state —
  the town is the persistent base state that dungeon returns pop back to.

## E. Proposed design & slices

Implemented as reviewable slices; each keeps the suite green.

1. **Session/save state.** `Party.currentTown` (1–7, default 1),
   `Party.highestUnlockedTown` (1–7, default 1). `SaveSystem` writes both and
   reads them as optional clamped fields (`optIntMin`, old saves → 1/1). Pure
   helpers: `townScalePct(town)`, `townScoreBonusPct(town)`, `unlockOnClear` in a
   small raylib-free `game/WorldLadder.hpp` (headless-tested).
2. **Difficulty.** `generate(seed, depth, db, themeId, town=1)`; each of the
   three `statScalePct` assignments becomes
   `combinedStatScale(comp, depth, town) = (100 + comp.statScalePct(depth)) *
   townScalePct(town) / 100`. Town 1 ⇒ identity (byte-identical). No new RNG
   draws, so the topology stream is unchanged for every town.
3. **Scoring.** `RunSummary.townBonusPct`; `ScoreBreakdown.townBonus =
   max(0, subtotal) * townBonusPct / 100` applied to the full subtotal (incl.
   wager), `total = max(0, subtotal + townBonus)`. Pct 0 ⇒ identical to today.
   `ScoreEntry.townIndex` optional tag (0 = legacy → shown as town 1); scoreboard
   row + Details + result screen show it within existing layout budgets.
4. **Travel & unlock.** `buildTown(town, hasPrev, nextUnlocked)` adds
   bottom-left / bottom-right exit tiles (present only when that neighbour
   exists; the next-town exit renders locked until unlocked, with a hint).
   Walking onto an exit prompts, then switches town. `GuildState` generates with
   `party.currentTown`. On boss victory `DungeonState` sets `highestUnlockedTown
   = max(…, currentTown+1)` (cap 7) and writes `townIndex`/`townBonusPct`. Every
   `pushState(TownState)` site audited to respect `currentTown`.
5. **Presentation.** Per-town exterior tiles + per-town interiors + 6 darker town
   tracks via `tools/asset_gen/` (append + RNG-reseed; existing PNGs/WAVs
   byte-identical); manifest rows; `assets/credits.md`. Town-indexed resolution
   with base fallback in `TownState`, the six service states, and `AudioManager`.
   Presentation lint extended: every town index resolves its exterior tiles,
   its six interiors, and its music. First-exit M22 tutorial prompt.
6. **Balance.** Extend the sim battery with a (town × depth) clearability grid:
   town 7 clearable by a strong endgame party; town 2 by a modest one. Battle and
   simulator tests themselves stay **unmodified**.

## F. Determinism & save compatibility

- **`kGenerationVersion` stays 6.** Town 1 is byte-identical (identity
  multiplier, no RNG change); towns 2–7 change only the post-hoc `statScalePct`
  value, not the seed stream. A determinism test asserts town-1 output equals the
  pre-M32 output and that (seed, depth, town) is reproducible.
- **`battleRulesVersion` stays 1.** No battle-rule change; battle/simulator tests
  unmodified.
- **No `kSaveVersion` bump.** `currentTown`/`highestUnlockedTown` and
  `ScoreEntry.townIndex` are optional; old saves/scoreboards load with town 1.
- **Scores stay comparable, never normalized (M19).** `townBonus`/`townIndex`
  are visible and tagged; ranking logic is untouched.

## G. Out of scope

Stakes penalty (M33); black market / legendary tokens (M34); scaling XP/gold
with town (D.1); per-town **dungeon** theme art (dungeon themes stay the three
existing ones — towns scale difficulty, not retheme dungeons); any battle-rule,
save-format, or virtual-resolution change; a world map (towns are a linear chain
onto the same loop).

## H. Balance

Acceptance: the (town × depth) grid shows a monotone difficulty climb; town 7 is
clearable only by a strong, well-geared endgame party and town 2 by a modest
one; no town/depth combination is unwinnable for an appropriately levelled party;
the score bonus rewards the harder town without making a low-depth high-town run
dominate a high-depth low-town run of equal skill (the visible bonus + M33's
future penalty are the intended levers).

## I. Manual validation for the owner

Travel feel (walking between towns, the locked-exit hint, fade + music change);
per-town distinctness of exteriors **and** interiors without reading a label;
the music growing more sinister town to town; the difficulty of towns 2–3 in
actual play; that the score bonus reads clearly on the result and scoreboard;
that loading an old save starts in town 1 with everything intact.

## J. Required documentation updates on completion

This note (as-implemented + deviations); `docs/milestones.md` (status + M32
as-implemented summary); `docs/game_design.md` (town ladder, travel, unlock,
score bonus — player-facing rules); `docs/technical_design.md` (`generate` town
param + `combinedStatScale`, `townBonus`/`townIndex` scoring, `Party` town
fields + save, town-indexed art/music resolution); `docs/asset_pipeline.md` /
art + audio docs for the new generator output; `assets/credits.md` provenance;
`docs/manual_test_matrix.md` rows (travel, per-town art, score bonus).

## K. As-implemented record (2026-07-21)

- **Status:** implemented, awaiting manual approval. §D decisions confirmed by
  the owner 2026-07-21: **flat rewards**, **per-town interiors** (owner overrode
  the "shared" recommendation), **absolute danger tiers**.
- **Ladder rules (pure).** `game/WorldLadder.hpp`: `kTownCount=7`, `clampTown`,
  `townScalePct` (+0/25/50/75/100/150/200 %), `townScoreBonusPct`
  (+0/10/20/30/40/50/100 %), `combineTownScale` (identity at town 1),
  `unlockAfterClear`. Headless-tested.
- **Session/save.** `Party.currentTown` + `Party.highestUnlockedTown` (default
  1/1), written by `SaveSystem` and read as optional clamped fields; a loaded
  `currentTown` is clamped to `[1, highestUnlockedTown]`. No `kSaveVersion` bump.
- **Difficulty.** `dungeon::generate(seed, depth, db, theme, town=1)` and
  `makeTeam(..., town)` apply `combineTownScale` at all three `statScalePct`
  sites; `Dungeon.town` records the run's town. Town 1 is byte-identical (proven
  by a test asserting town-1 team scale == `100 + comp.statScalePct(depth)` and
  identical enemy ids/topology to the default). `kGenerationVersion` stays 6.
- **Scoring.** `RunSummary.townBonusPct` → `ScoreBreakdown.townBonus`
  (`max(0,subtotal)*pct/100`, total `= max(0, subtotal+townBonus)`; pct 0 ⇒
  unchanged). `ScoreEntry.townIndex` optional tag written by `DungeonState` and
  round-tripped by `Scoreboard`. Result screen shows a "Town bonus (+N%)" row;
  the scoreboard tags the fitted Theme column with "T#" for town ≥ 2 (columns
  shifted ~16px left to make room); Details text and legend updated.
- **Travel & unlock.** `town::buildTown(town, hasPrev, hasNext, nextUnlocked)`
  carves bottom-left/right road exits (a `Door` gate + `Path`), recorded as
  `TownExit`s. `TownState` builds for `party.currentTown`, shows exit signposts
  ("< Town n" / "Town n >" / "Locked"), a "Town n/7" indicator, and a travel/
  locked prompt; stepping on an unlocked exit + Confirm switches town in place
  (rebuild + fade + music). `GuildState` generates for `currentTown`;
  `DungeonState` on boss victory sets `highestUnlockedTown =
  unlockAfterClear(...)` and writes `townIndex`/`townBonusPct`. Every
  `pushState(TownState)` site respects `currentTown` (they read it live).
- **Audio.** `AudioManager::setTown(n)` rebinds the `MusicTrack::Town` stream to
  `music.town.<n>` (n>1) with fallback to base `music.town`; `TownState` calls
  it before `setMusic(Town)` so the right variant plays on enter and on switch.
- **Presentation assets.** `tools/asset_gen/generate_textures.ps1` appends a
  per-town shading pass (deterministic ColorMatrix, progressively darker/more
  sinister) producing 30 exterior tiles (towns 2–7 × ground/grass/path/tree/
  building) + 36 service interiors (6 places × towns 2–7);
  `generate_audio.ps1` appends 6 transposed/slowed town tracks. 72 new asset
  files; existing PNGs/WAVs byte-identical (verified via git). Manifest rows +
  `assets/credits.md` provenance added. Per-town resolution: `TownState` tiles
  (`tiles.town.<n>.<kind>`), the town-aware `ui::drawSceneBackground` overload
  in the six service states (`bg.<place>.<n>`), and `AudioManager` music — all
  fall back to the base id. First-exit `kFirstTravel` tutorial beat added.
- **Balance.** `test_balance.cpp` gains a town battery: threat rises with town at
  fixed depth; a modest party (L10) clears a town-2 boss; a strong endgame party
  (L50, geared) clears a town-7 boss. Battle/simulator tests unmodified.
- **Validation (this session, VS 2022 / MSVC 14.44, `build-msvc`).** Debug build
  clean; full suite **293/293** (30949 assertions), incl. 10 new M32 cases and
  the extended presentation lint; battle & simulator tests **unmodified and
  green**. `--capture` **25/25** overflow-clean (new `25_town_ladder` scene;
  result town-bonus row; scoreboard `T7` tag). Release build clean. Generators
  reran byte-identical apart from the 72 intended new files.

### Deviations from the plan / note

1. **Session state lives on `Party`, not a separate `WorldState`** (the §D.4
   reversible choice), matching the `restTokens` precedent — least save/context
   churn.
2. **Exterior variety is per-town tile palette shading, not new tile layouts or
   prop density** (§D.4). Prop-density progression would need new town layouts,
   out of this milestone's scope; the palette progression + per-town music +
   per-town interiors already give clear per-town identity.
3. **Scoreboard town tag is a "T#" prefix on the Theme column** (for town ≥ 2),
   with the numeric columns shifted left to fit, rather than a dedicated column —
   the 92px theme column had no room otherwise and a new column would have
   reflowed the whole tight layout. Town/legacy (town 1 / townIndex 0) rows read
   exactly as before. Because a town-≥2 run always carries current battle rules,
   the "T#" and "~" (older-rules) tags never coexist, so the widened column fits
   the worst real case.
4. **Score Details text was rewritten more compactly** (not just extended) to add
   the town-bonus explanation while still fitting the Details overlay's line
   budget.

### Post-implementation fixes (owner manual testing, 2026-07-21)

Two unlock-flow bugs found during the owner's first manual pass, both fixed and
re-validated (293/293 tests, capture 25/25 clean):

1. **New Game did not reset world state.** `PartyCreationState::begin()` reset
   members/gold/inventory but not the session fields, so a New Game inherited
   `currentTown`/`highestUnlockedTown` (and `restTokens`) from a party still in
   memory from a prior Continue/play — letting a fresh game start with a later
   town already reachable. Fixed: `begin()` now resets `currentTown = 1`,
   `highestUnlockedTown = 1`, `restTokens = 0` (a New Game is a clean slate).
2. **An unlock earned mid-session was not reflected on return to town.**
   `TownState` builds its exits once (construction); `onResume` (fired when a
   dungeon/shop pops back) refreshed music but never recomputed the exits'
   `locked` flags, so clearing a dungeon updated `highestUnlockedTown` in memory
   yet the next-town road still read Locked. Fixed: `onResume` now recomputes the
   next-town exit's locked state from the current `highestUnlockedTown` in place
   (no rebuild, so the player is not teleported off a shop door). `currentTown`
   cannot change while a sub-state is on top, so only the lock flag needs
   refreshing; `travelTo` still does a full rebuild.

Note (unchanged, consistent with gold/XP): an unlock is held in the live party
and persists on the next save/autosave (autosave fires on dungeon entry), so
quitting to title immediately after a clear without saving does not persist the
unlock. This matches how a run's gold and XP already behave and was not part of
the report.
