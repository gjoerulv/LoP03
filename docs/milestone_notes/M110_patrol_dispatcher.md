# M110 — Patrol dispatcher, debug tools & the Stranger's patrol scenes

**Status:** implemented, awaiting manual approval
**Program:** M109–M116 (owner-authorized 2026-09-02 via the approved plan).
**Generation 23 → 24** (`src/dungeon/RoomLayout.hpp` history, the
authority): room, event and team rolls are byte-identical to v23, but what
the Nth patrol of a seed PRODUCES changed, so new scoreboard entries tag v24
and old entries keep theirs. No battle-rules or save motion. Content: four
new `patrol_*` scenes in `data/cutscenes.json` (the optionless-tale shape;
count pin 18 → 22).

## Scope (plan section M110; owner sections 2, 3, 8, 9)

The M93 counter stays; at 0 a pure dispatcher decides the patrol's kind —
65 % normal patrol, 10 % Golden Goose, 5 % lore question, 15 % treasure
chests, 5 % a Stranger "P" scene — deterministically from (run seed, patrol
index). Every outcome consumes the patrol. This milestone ships the
dispatcher and its bookkeeping, the four dedicated P scenes and their cycle,
the debug one-shots, the generation bump, and the explicit economy
amendment in the docs. The Golden Goose, Lore and Chest kinds resolve as an
ordinary patrol until M111/M112 land (interim; invisible at program end).

## What was built

- **`dungeon/PatrolDispatch.hpp`** (pure): `PatrolKind`, the mixture table
  `kPatrolKindPct = {65, 10, 5, 15, 5}`, `patrolKindForRoll` (cumulative
  thresholds), `patrolKindFor(runSeed, index)` (`blackMarketHash` under its
  own salt — distinct from the M93 team salt, no rng-stream draw),
  `patrolKindCount` (the per-kind ordinal of earlier patrols — the lore
  pool walk and the P cycle key off it), `patrolKindName`.
- **`DungeonState::triggerPatrol(forcedKind)`** replaces the inline M93
  block: both the tile trigger and the debug one-shot call it; it tallies
  the M109 ledger (`patrols.total`, the town's patrols, the kind's own
  counter) and dispatches — Normal → the unchanged `patrolTeam` battle;
  StrangerP → `game::patrolSceneFor` and a `CutsceneState(replay=true)`
  pushed after `consumePatrol()` (zero turns, nothing paid, nothing
  fought; an empty pool falls back to a normal patrol); Goose/Lore/Chests
  → the ordinary patrol until their milestones. `consumePatrol()` is the
  one place the counter rewinds and the index advances (the two onResume
  paths use it).
- **The P pool** (`game/Cutscenes.hpp`): `content::kPatrolCutscenePrefix`
  joins the loader's optionless-tale rule (no question, no options — a
  patrol scene smuggling options is rejected like a joke would be);
  `strangerPatrolIds` (sorted) and `patrolSceneFor(db, runSeed, told)` — a
  Fisher-Yates order hashed from the run seed under its own salt, walked to
  the end before any repeat; never touches `strangerJokesTold` or the story
  pick. Four original scenes: `patrol_1..4` — THE STRANGER "P" in the
  corridor, dry, two to three beats each, no staging (valid before or after
  the King).
- **Debug** (`core/DebugCheats.hpp`, `DebugMenuState`, dungeon rows, all
  dev-only): `Next patrol: < Random | Normal | Golden Goose | Lore | Chests
  | P >` (`cheats.nextPatrolKind`, consumed by the next trigger — it
  replaces the RESOLVED kind for that trigger; the hash sequence beneath
  never moves), `Trigger patrol now` (replaces "Patrol on next step";
  `requestPatrolNow` is consumed at the top of `DungeonState::update` by
  calling the dispatcher directly — no tile walk), and `Next event: <
  Random | … >` over the pure `dungeon::debugSubstitutableEventKinds()`
  (HealingSpring, ScoreWager, RestToken, ArmoryGhost, Dragonform,
  GoosePolymorph, Sacrifice, LevelAltar, StrangerStory, TokenExchange,
  PatrolReset, Reels, Blackjack, GoosyFlock — exactly the kinds whose
  resolution reads nothing baked at generation). The event override
  applies the moment a plain, unresolved event marker is FACED: the room's
  runtime `RoomEvent` takes the chosen kind (baked fields cleared) so the
  marker glyph, the footer prompt and the resolution all agree, and the
  cheat is consumed; Shrine / Merchant / Elder Root / Surveyor / Miner's
  Cache / Duck Peddler (baked `goldCost`/`itemId`), the Elite Challenge
  (needs a team) and the Royal Relic (its own table) can never be
  substituted in either direction. Forced patrols run the real path and
  record telemetry like walked ones (dev-only; Release has no overlay).
- **Ledger** (M109 fields): `patrols.total`, `patrols.normal`,
  `patrols.strangerScenes`, the town's `patrols` — incremented here.

## Deviations from the plan

- The event override is applied by substituting the room's RUNTIME event
  kind at facing time rather than through an "effective kind" accessor
  threaded into three readers — the same one-shot semantics, one field
  every reader already consults, and the generator/autosave are still
  untouched (the dungeon model is runtime state; the entry autosave
  predates any event). Recorded as a routine implementation choice.
- The P scene is pushed without a stage parameter; M113 adds the theme
  stage.

## Tests

`tests/test_patrol_dispatch.cpp` [patrol][dispatch]: the thresholds
verbatim (64/65/74/75/79/80/94/95/99), hash determinism, a 20 000-sample
census within two points of every weight, the M93 team roll untouched
(and the salts distinct), the per-kind ordinal, the four-scene cycle
(every scene before a repeat, seed-shuffled, stable, empty-pool fallback)
and the shipped pool's shape (four dedicated optionless, questionless,
unstaged scenes, disjoint from jokes and stories). The M93 patrol test
and the generation pin move with the milestone.

## Compatibility

- Generation 23 → 24: new scoreboard entries tag v24; old entries keep
  v23 (the M19 comparability rule). Structural generation is untouched.
- Saves unchanged; the debug one-shots are never saved.
- Content: four added cutscenes; the loader's prefix rule grew; no schema
  version motion; `CrystalForge --canonicalize` normalizes the hand-added
  scenes.

## Automated validation

Filled by the completion report below.

## Manual owner checklist

Matrix rows **214–216**: the mixture in play (P scenes appear now and
then, nothing fought, counter back at 100, reload-honest order); the debug
patrol selector (forced once, Random afterwards, the sequence beneath
unchanged); the debug event selector (glyph, prompt and resolution agree;
relic/challenge rooms never hijacked). Judge the four tales' dryness.

## Documentation updated

`docs/milestones.md` (row + section), `docs/game_design.md` (§6 patrol
passage rewritten + the economy amendment), `docs/technical_design.md`
(§52), `docs/manual_test_matrix.md` (rows 214–216), the M93 note (a
superseded-in-part pointer), this note.

## Completion report

### 1. Implementation summary

- **Milestone:** M110 — Patrol dispatcher, debug tools & the Stranger's
  patrol scenes.
- **Slices completed:** the pure dispatcher; the DungeonState trigger and
  consume seams; the P pool + cycle + four scenes; the three debug
  one-shots; the generation bump; tests; captures; docs. All completed.
- **Player-facing changes:** ~5 % of patrols are now a short Stranger
  scene instead of a fight (zero turns); the other special kinds arrive
  with M111/M112. New scoreboard entries tag generation v24.
- **Engineering changes:** `dungeon/PatrolDispatch.hpp` (new),
  `DungeonState::triggerPatrol/consumePatrol`, `game::strangerPatrolIds/
  patrolSceneFor`, `dungeon::debugSubstitutableEventKinds`, the loader's
  `patrol_` prefix, `DebugCheats::nextPatrolKind/nextEventKind`.

### 2. Files changed

- **Source:** `src/dungeon/PatrolDispatch.hpp` (new),
  `src/dungeon/{RoomLayout,ThemeEvents}.hpp`, `src/content/{Definitions.hpp,
  ContentLoader.cpp}`, `src/game/Cutscenes.hpp`, `src/core/DebugCheats.hpp`,
  `src/states/{DungeonState,DebugMenuState}.{hpp,cpp}`,
  `src/capture/CaptureRunner.cpp`.
- **Tests:** `tests/test_patrol_dispatch.cpp` (new), `tests/CMakeLists.txt`,
  the generation-version pin.
- **Content/data:** `data/cutscenes.json` (+4 scenes).
- **Documentation:** as listed above.
- **Build/release configuration:** none.

### 3. Plan deviations

See "Deviations from the plan" — routine local decisions.

### 4. Compatibility

See "Compatibility".

### 5. Automated validation

- **Build:** `cmake --build --preset debug` — clean, zero project-code
  warnings (VS 2022 developer shell).
- **Targeted tests:** `crystal_tests.exe "[patrol],[cutscene],[content],
  [danger],[events],[glyphs],[editor],[lint]"` — **113 cases, 121 084
  assertions green** (the `[dispatch]` set alone: 7 cases). Count pins
  moved honestly: cutscenes 18 → 22, generation 23 → 24.
- **Content:** `CrystalForge --canonicalize` — 1 file rewritten (the
  hand-added scenes normalized), 0 content errors before and after.
- **Capture lint:** `ArePGeese.exe --capture <dir>` — exit 0, **129/129
  scenes clean** (`128_debug_patrol_rows`, `129_patrol_scene` new).
- **Full suite / Release:** ride the program's closing battery (the M109
  full run passed on the build immediately before this milestone).

### 6. Manual owner validation

Matrix rows 214–216.

### 7. Known limitations

- Golden Goose / Lore / Chests resolve as an ordinary patrol until
  M111/M112 (interim by design).
- A forced "Next event" stays armed until a substitutable event is faced.

### 8. Documentation updated

As listed above.

### 9. Final status

`implemented, awaiting manual approval`
