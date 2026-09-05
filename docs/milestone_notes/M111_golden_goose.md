# M111 — The Golden Goose (scripted enemy actions, the enemy flee; rules v19)

**Status:** implemented, awaiting manual approval
**Program:** M109–M116 (owner-authorized 2026-09-02 via the approved plan).
**Battle rules 18 → 19** (`src/battle/Battle.hpp` history, the authority):
a data-driven scripted own-turn mechanism on enemies and a new outcome,
`EnemyFled`. No pre-M111 foe carries a script or can flee, so every
earlier battle resolves byte-identically (the [battle] suites ran
unmodified); the bump follows the M89/M95/M96 precedent of tagging a new
engine hook that ships with scored content. Generation stays v24, saves
stay v1, content schemas stay v1 (new optional keys only).

## Scope (plan section M111; owner sections 4, 5, 10–13, 29)

The patrol dispatcher's 10 % kind becomes a real encounter: the Golden
Goose — HP 45 / ATK 1 / MAG 0 / DEF 2 / SPD 40, Spell Ward + Evasion +
Iron Will + First Strike, Reflect at the start, three scripted own turns
(dust / cower / getaway), a 2000-gold bounty on defeat, the replaced
patrol's XP, nothing on its flight. Data-driven end to end: the engine
knows a script, not a goose (no `id == "golden_goose"` anywhere in
`battle::`).

## What was built

- **Content** (`data/enemies.json` `golden_goose`; enemy count 70 → 71):
  the owner's stats verbatim, tier normal, role disruptor, `specialOnly:
  true`, `goldReward 2000`, `xpReward 0` (the XP comes from the team
  override below), `initialStatuses: [reflect d2]`, and the script —
  `status_all_foes` (blind d3, silence d3, poison mag 5 d3), `guard`,
  `flee` — each with its own announcement line.
- **Schema** (`content::ScriptDo`, `content::ScriptStep`,
  `EnemyDef.script`, `EnemyDef.specialOnly`, `BossDef.specialOnly`; the
  `kScriptDos` id table + `parseScriptDo/toString/scriptDoIds`; the loader's
  `readScript`): `status_all_foes` requires at least one status, the other
  actions may carry none, a `flee` must be the last step; an unknown action
  is rejected. `specialOnly` on bosses is read now (M112's Mimic uses it)
  and is inert on every shipped boss. Editor: `scriptChildren()` +
  `objArr("script", …)` and the two `specialOnly` booleans, so the Forge
  edits the goose and the M59 completeness battery stays green.
- **Engine** (`battle::ScriptedAction`, `Combatant.script`/`fled`,
  `scriptedTurn(const Combatant&)`, `EnemyChoice.scripted`,
  `Battle::runScriptedStep(actor)`, `Outcome::EnemyFled`): the script is
  mirrored at `buildBattle` the attackStatuses way; `scriptedTurn` indexes
  it by `ownTurnsTaken` (1-based; a turn a control status steals still
  spends the step — the M89 lunge precedent); `chooseEnemyAction` reports
  a scripted turn right after the do-nothing tier and before any target is
  picked; the one shared executor lands `status_all_foes` on every living
  foe through `addStatus` (immunities, the M35 duration scaling, the M75
  poison magnitude off the applier's Magic, the M109 `StatusApplied`
  event), guards through the ordinary `guard`, and marks a flee. Both
  drivers (`Simulator::applyChoice`, `BattleState::executeEnemy`) call it
  first; no roll is consumed, so the rng cursor never moves. A fled unit is
  alive but gone: `sideAlive`, `aliveIndices`, `turnOrder`, the enemy AI's
  hurt-ally scan, the court-revival scan and `bodyguardFor` filter on
  `!fled`; `Battle::outcome()` returns `EnemyFled` when no foe stands and
  one has fled (Victory otherwise).
- **Screen** (`BattleState`): the scripted branch of `executeEnemy` shows
  the authored line, the dust wears the M51 debuff tint, a fled foe fades
  out like a fallen one, no jingle plays for a flight (the battle music runs
  into Done as a player escape does), `outcomeMessage` names it, and the
  spoils gate already pays nothing off Victory.
- **Team and spoils** (`dungeon::goldenGooseTeam`, `EnemyTeam.patrolPaysGold`,
  `EnemyTeam.xpOverride`, `teamSpoils`): the one goose at the scale
  `patrolTeam` gives this (town, depth), flagged to keep its authored gold,
  with `xpOverride` = the summed XP of the ordinary patrol derived with the
  same seed and index (reload-honest, never a better XP farm than the patrol
  it replaced). `patrolTeam` is untouched; every other patrol still zeroes
  its gold.
- **Dungeon** (`DungeonState::triggerPatrol` GoldenGoose case,
  `pendingPatrolKind_`, the `EnemyFled` onResume branch): `geeseMet` at the
  trigger, the team pushed and started as an ordinary Patrol battle (no
  boss intro), Victory → `geeseDefeated` + its own line, EnemyFled →
  `consumePatrol()` + `geeseEscaped` + its own line, no gate or chest
  touched, no escape recorded. The content lacking the foe falls back to a
  normal patrol.
- **Exclusions** (`specialOnly`): the theme pool predicate, `Castle.cpp`'s
  endless waves and elite vigils, `Guild.cpp`'s trial sweep.
- **Telemetry**: `BattleTelemetry::recordBattleEnd` counts only the turns
  for `EnemyFled`.
- **Art**: `enemy.golden_goose.battle` — a 24×24 hand-placed grid
  (`Save-EnemyGrid 'golden_goose'`, RNG-free, appended last): a goose
  reared and about to bolt, one raised wing with daylight between wing and
  neck, gold body on the reward ramp, earth bill and feet, a white sheen
  and one dark eye. Manifest row, credits row; the generator re-run wrote
  only the new PNG (every earlier file byte-identical, `git status`).
- **Capture**: `130_battle_golden_goose` (the lone foe centred on the
  Keep stage, Reflect in the status column).

## Deviations from the plan

- **The Duck's speed supremacy.** `test_goose_town` pinned the Deadly
  Duck's effective stats above every foe in its authored context; the
  owner-locked SPD 40 at the deepest patrol scale (228) tops the Duck's
  140. The goose exists to act first and run, so the sweep now skips
  `specialOnly` foes with the reason written beside it, and the game
  design records the exception ("the one place the Duck's rule yields").
  Flagged for the owner in the report — the stats stand as specified.
- `xpOverride` floors at 1 when the shadow patrol's XP sums to zero (no
  shipped context does) so the override is never mistaken for "unset".
- `ScriptDo::None` exists only as the struct default (never parsed, never
  authored) — the `TriggerDo` idiom.

## Tests

`tests/test_golden_goose.cpp` [goose][v19]: the content kit verbatim (and
`kBattleRulesVersion == 19`); the loader's shape rules (valid scripts
parse; a status-less `status_all_foes`, a `guard` carrying statuses, a
`flee` that is not last, an unknown action are rejected); buildBattle
mirrors the script, Reflect and the four passives, First Strike puts the
goose first; own turns one/two/three — the dust lands all three statuses
on every LIVING member only (durations ×kStatusDurationMult, poison
magnitude 5 off 0 Magic), the guard, the flight → `EnemyFled` with the
goose alive-but-gone, absent from `aliveIndices` and `turnOrder`, a fourth
turn finding no step; a stunned first turn consumes the step (turn two is
the guard); the flight through `BattleTelemetry` — no KO, no win, no
escape, no defeat entry, three turns; the simulator and the hand-driven
executor agree; the team pays 2000 gold and exactly the shadow patrol's XP
at the same scale, deterministically, while the ordinary patrol keeps its
zero gold; never generated by the patrol recipe (4 themes × 60 seeds),
the endless waves, the guild trials or the elite vigils. The
`[goose-report]` battery (`[!benchmark]`, run with `-s`) sims the standard
party across towns 1–7 × depths 1/5/10/20 and records catch rate; it
pins that the goose acts first in every context and never wins.
Pins moved honestly: enemy count 70 → 71 (`test_content_loader`),
`scriptDoIds().size() == 3` + `requireAllParse` (`test_editor_enum_lists`),
the Duck sweep's `specialOnly` skip (`test_goose_town`).

## Compatibility

- Battle rules 18 → 19; generation stays 24; saves stay v1.
- Content: one added enemy, three new optional keys (`script`,
  `specialOnly` on enemies and bosses), no version motion;
  `CrystalForge --canonicalize` normalized the hand-added entry.
- Manifest v2, one new id; the capture set grows to 130.

## Automated validation

See the completion report below.

## Manual owner checklist

Matrix rows **217–218**: the hunt (lone foe centred, acts first, the
one-action dust on every standing hero, the guard, the flight with no
jingle/XP/gold/escape; a kill inside three turns pays 2000 gold + the
patrol's XP, no drop) and the numbers (ledger tallies, a reload
reproducing the same goose and payout, v24 scoreboard tags). **Judge
whether the hunt is exciting and whether three turns is the right
window.**

## Documentation updated

`docs/milestones.md` (rows + section), `docs/game_design.md` (§6 the
Golden Goose passage), `docs/technical_design.md` (§53),
`docs/manual_test_matrix.md` (rows 217–218), `assets/credits.md`,
`src/battle/Battle.hpp` (the v19 history line), this note.

## Completion report

### 1. Implementation summary

- **Milestone:** M111 — The Golden Goose (scripted enemy actions, the
  enemy flee; battle rules v19).
- **Slices completed:** content + schema + loader + editor; the script
  engine, the flee and the outcome; the screen's presentation; the team and
  spoils rules; the dungeon trigger/resume paths; the exclusions; the art;
  the capture; tests; docs. All completed.
- **Player-facing changes:** ~10 % of patrols are now the Golden Goose
  hunt: a 2000-gold prize that dusts the party, cowers, and flees on its
  third turn unless caught.
- **Engineering changes:** `content::ScriptDo/ScriptStep`,
  `EnemyDef.script`, `specialOnly` on enemies and bosses,
  `battle::ScriptedAction`, `scriptedTurn`, `Battle::runScriptedStep`,
  `Outcome::EnemyFled`, `Combatant.fled`, `EnemyTeam.patrolPaysGold/
  xpOverride`, `dungeon::goldenGooseTeam`, `DungeonState::pendingPatrolKind_`.

### 2. Files changed

- **Source:** `src/content/{Enums.hpp,Enums.cpp,Definitions.hpp,
  ContentLoader.cpp}`, `src/battle/{Battle.hpp,Battle.cpp,Simulator.cpp}`,
  `src/game/{BattleTelemetry.hpp,Spoils.hpp,Castle.cpp,Guild.cpp}`,
  `src/dungeon/{DungeonModel.hpp,DungeonGenerator.hpp,DungeonGenerator.cpp}`,
  `src/states/{BattleState.cpp,DungeonState.hpp,DungeonState.cpp}`,
  `src/editor/CategoryDescriptors.cpp`, `src/capture/CaptureRunner.cpp`.
- **Tests:** `tests/test_golden_goose.cpp` (new), `tests/CMakeLists.txt`,
  `tests/{test_content_loader,test_editor_enum_lists,test_goose_town}.cpp`
  (pins).
- **Content/data:** `data/enemies.json` (+1), `assets/manifest.json` (+1),
  `assets/credits.md` (+1), `assets/textures/enemies/golden_goose.png`
  (new), `tools/asset_gen/generate_textures.ps1` (the appended grid).
- **Documentation:** as listed above.
- **Build/release configuration:** none.

### 3. Plan deviations

See "Deviations from the plan" — the Duck-speed exception is the one the
owner should glance at; the rest are routine.

### 4. Compatibility

See "Compatibility".

### 5. Automated validation

- **Build:** `cmake --build --preset debug` — clean, zero project-code
  warnings (VS 2022 developer shell).
- **Targeted tests:** `crystal_tests.exe "[goose],[v19],[battle],[editor],
  [content],[lint],[patrol],[lifetime],[telemetry],[data]"` — **236 cases,
  92 091 assertions green**; after the pin moves below, `"[offense],[goose],
  [v19],[content]"` — 80 cases, 4 395 assertions green.
- **Full suite:** `ctest --preset debug` (341 s) — one failure, the M77
  vocabulary pin (`test_enemy_offensive`: the goose joined the
  initial-status set), moved honestly and re-run green with its tag; the
  full run is repeated in the program's closing battery.
- **Balance battery** (`crystal_tests.exe "[goose-report]" -s`, 7 towns ×
  4 depths × 8 roll streams, the standard sim party): the goose acts first
  in **224/224** fights and never wins one; **plain** (no status
  protection) the party caught it in **0/224** — Blind's 75 % miss on every
  swing, Silence on every spell and the guard on turn two leave nothing
  landing before the getaway; **warded** (immune to the dust) the party
  caught it in **189/224**. The dust decides the hunt: a cleanse, a ward or
  a burst that lands before the third turn is the whole build question.
  Stats stand as the owner specified; this is the finding.
- **Content:** `CrystalForge --canonicalize` — 1 file rewritten (the
  hand-added goose normalized), 0 content errors before and after.
- **Art:** `generate_textures.ps1` re-run — `git status` shows only the new
  `golden_goose.png`; `preview.ps1 -Only golden_goose` sheets reviewed
  (docs/sprite_review updated).
- **Capture lint:** `ArePGeese.exe --capture <dir>` — exit 0, **130/130
  scenes clean** (`130_battle_golden_goose` new).
- **Release:** rides the program's closing battery.

### 6. Manual owner validation

Matrix rows 217–218.

### 7. Known limitations

- The `[goose-report]` finding (below) is a simulation with the standard
  sim party; the owner's own hunt judges the three-turn window.
- Lore and Chests still resolve as an ordinary patrol until M112.

### 8. Documentation updated

As listed above.

### 9. Final status

`implemented, awaiting manual approval`
