# M61 — Goose Town & the Deadly Duck

Authorized 2026-07-25 ("I agree 100%" on the presented plan, covering every
recommendation including the achievement name). Implemented 2026-07-25 on the
working tree carrying M59/M60 (base checkout `7c8f32d`). The companion owner
concept — the geese scaring the King — was verified already shipped in M58
(approved) and needed no work; the owner chose to keep its polished wording.

## A. Status

**◑ implemented, awaiting manual approval** — set 2026-07-25. Evidence in §F.

## B. Goal (owner brief)

Defeating the King with at least one Goose in the party opens **Goose Town**
at the castle's own entrance. It works like the castle with a single
challenge: **the Deadly Duck** — first five Evil Goose minions (diverse cast,
each one weakness + two passives, 10% chance to just Quack), then the Duck:
all-party attacks that inflict statuses, immune to every bad status but
debuffable, Counter Attack + Thorns + Spell Ward, the highest stats of any
enemy or boss, **5000 effective HP** (minions ~500). A Goofy Jester tells the
Duck's legendary tale. Reward (owner Q&A): an achievement + a repeatable
best-turns record; the two fights are a no-heal gauntlet.

## C. As implemented

### Battle rules (v11 → v12, all schema-driven, no id branching)

- **`EnemyDef.doNothingPct` / `doNothingText`** — a per-own-turn chance the
  foe does nothing, decided by `battle::doesNothingThisTurn` (a pure seeded
  hash under its own salt, the King-scare shape: sim == live, `rollCursor`
  untouched) and hooked into `chooseEnemyAction` beside the scare. The
  authored line ("Quack.") shows as the turn's message.
- **`BossDef.attackHitsAll` / `attackStatuses`** — the M45 class machinery
  swung from the boss side; `buildBattle` resolves either source into the
  same Combatant fields, so the pure model never knows who authored them.
- **`BossDef.immuneToAfflictions`** — a new `isAffliction` classifier
  (poison + the control trio + the turn-takers; deliberately broader than a
  cleanse) guarded at the single `addStatus` chokepoint and folded into
  `isImmuneTo`, so display sites skip what cannot land. ATK-/DEF- debuffs
  still land ("can be debuffed"); relic stat-scaling (the Spoon) is not a
  status and lands too — that is the fight's obtainable counterplay.
- No pre-M61 content carries any field ⇒ every earlier battle resolves
  byte-identically; the loader gets range/semantic checks
  (`doNothingPct` 0..100; `doNothingText` requires the pct) and the
  class/boss `attackStatuses` readers were unified into one helper.

### Content

- Five **Evil Geese** (`bossOnly`, elite, authored as the Duck's `minions`
  so the pairing is content): Vanguard (bruiser; fire-weak;
  counter+iron_will), Hexwing (magic sniper; earth-weak;
  spell_ward+keen_senses), Mender (healer; lightning-weak; clarity+evasion),
  Trickster (disruptor; ice-weak; evasion+first_strike), Bogfeather
  (attrition; holy-weak; thorns+lifedrink). All base 90–100 HP → ~450–500
  effective at the arena scale; all `doNothingPct` 10, "Quack.".
- **The Deadly Duck** (`deadly_duck`): base 1000/40/46/40/28 — above the
  King's 750/36/44/36/26 — at **`kGooseTownScalePct` = 500%** ⇒ 5000
  effective HP and, per the pinned test, **every effective stat above every
  other foe in every authored context** (the unbounded endless rush stays
  excluded, the M52 precedent). No skills: his basic attack IS the kit —
  all-party, ATK-down + poison riders. Passives counter_attack, thorns,
  spell_ward; `immuneToAfflictions`; brute archetype (enrages). Excluded
  from the Boss Rush by the King's own rule (`bossRushOrder` skips
  `kDuckBossId`; he tops the bestiary at his arena scale via the
  `foeMaxScalePct` own-arena extension).
- **Story beat town 9** — the Goofy Jester's "Ballad of the Deadly Duck"
  (loader range 1..8 → 1..9; the towns-1..7 Loremaster mask untouched).

### Flow

- **Unlock:** the King-victory handler scans the party for the Goose class
  (the documented `kGooseClassId` constant, now exported in `Castle.hpp`
  beside `kKingBossId` and shared with the M58 scare rule) → sets
  `Party.gooseTownUnlocked` (optional save field) with a result-line
  announcement.
- **Entry:** town 7's north road — the castle's entrance — forks once Goose
  Town is open (`RoadForkState`, a small over-town prompt; Cancel steps
  back). The signpost text is unchanged (the fork reveals the choice).
- **`GooseTownState`** — a castle-style hub: Fight the Deadly Duck / Hear
  the Goofy Jester / Inn / Save / Leave, with a Pond Records panel
  (`duckBestTurns`, an optional save field on the castle records).
- **The gauntlet** — a new `CastleChallenge::DuckGauntlet` kind in the
  existing challenge state: wave 0 = `gooseWaveTeam` (plain start), wave 1 =
  `duckTeam` (the Duck ALONE — his court fell in fight 1 — with the Crystal
  Shatter intro and the King's battle theme), no-heal carry, the castle's
  1-HP defeat price, best-turns record via `duckImproved`.
- **Achievement `quackbane`** ("Fell the Deadly Duck.") — predicate reads
  `duckDefeated()`; toasts through the existing challenge-finish path.
- Editor descriptors for every new field (the M59 completeness sweep
  enforces them); hand-authored data entries confirmed byte-canonical by
  the round-trip battery.

## D. Files changed

- **Battle:** `Battle.hpp` (rules 12 + fields + `isAffliction`/`isImmuneTo` +
  `doesNothingThisTurn`), `Battle.cpp` (salt+rule, addStatus guard,
  buildBattle wiring, `kGooseClassId` moved out), `BattleState.cpp` (the
  Quack line beside the scare line).
- **Content:** `Definitions.hpp`, `ContentLoader.cpp` (+ shared
  `readAttackStatuses`), `data/enemies.json` (+5), `data/bosses.json` (+1),
  `data/story.json` (+1).
- **Game/flow:** `Castle.hpp/.cpp` (ids, scale, enum kind, records, team
  builders, rush exclusion), `Party.hpp`, `SaveSystem.cpp`,
  `Achievements.hpp/.cpp`, `CastleChallengeState.cpp`,
  `GooseTownState.{hpp,cpp}` (new), `RoadForkState.{hpp,cpp}` (new),
  `TownState.cpp` (fork hook), `BestiaryStats.hpp` + `BestiaryState.cpp`
  (own-arena scale), `CMakeLists.txt`.
- **Editor:** `CategoryDescriptors.cpp` (new fields, story town 9).
- **Tests/capture:** `tests/test_goose_town.cpp` (new, 13 cases),
  `tests/CMakeLists.txt`, `CaptureRunner.cpp` (+`76_goose_town`,
  `77_duck_battle`); `tests/test_castle.cpp` (the "deepest dungeon boss"
  sweep now skips the pond-only Duck exactly as it skips the King, and the
  King's superlative is scoped to the castle), `tests/test_kings_court.cpp`
  (the bossOnly roster is 7 — guards + geese — with a new generalized
  invariant: every bossOnly foe must be some boss's authored court).
- **Assets:** `assets/manifest.json` — six placeholder sprite rows mapping
  the Evil Geese and the Duck to the existing Goose actor sprite (the
  presentation lint requires every foe id to resolve; real art is an owner
  call, see §H).
- **Docs:** this note, `docs/milestones.md`, `docs/game_design.md`,
  `docs/technical_design.md`, `README.md`.

## E. Plan deviations

- **The sim-proof bar is the King's, not the plan's literal sentence.** The
  plan said "winnable by a maxed party"; the first battery showed a bare
  scripted party losing 0/5 — which is the owner-approved King philosophy
  (the M49/M54 record: castle fights are not tuned to what the itemless sim
  can beat; the King himself falls only WITH relic counterplay). The
  asserted bar therefore mirrors M44's King test: the gauntlet falls to a
  maxed party using the obtainable kit (one Deadly Spoon — stat scaling is
  not a status, so it pierces the immunity by design — plus
  elixirs/phoenix tears): **5/5 seeds win with counterplay; 0/5 bare**
  (recorded). Tax Sheets / Evil Goose relics bounce off the Duck by design.
- No other deviations.

## F. Automated validation (all run in this session, 2026-07-25)

- Clean single-toolset Debug rebuild (a stray VS18 toolchain flap plus a
  zombie game process were caught and eliminated first); zero warnings on a
  forced recompile of the new TUs.
- `[goose]` battery: **13 cases / 980 assertions green** — team shapes +
  effective numbers (Duck 5000 HP; geese 450–550; per-stat supremacy over
  every foe in every authored context), rush exclusion, quack determinism +
  party-never-quacks + `rollCursor == 0`, affliction-immunity semantics
  (poison refused, debuff lands, queries agree), save round-trip, Quackbane
  predicate, rules ≥ 12, and the counterplay gauntlet proof (5/5; bare 0/5
  recorded).
- `[editor]` battery: **19 cases / 3232 assertions green** (descriptor
  completeness over the new fields; the authored entries are
  byte-canonical).
- `--capture`: **77/77 scenes clean** (+`76_goose_town`, `77_duck_battle`).
- The full-suite passes surfaced six stale premises, all fixed and
  re-verified: the presentation lint demanded sprite rows for the six new
  foes (placeholder rows added); the kings-court battery pinned the bossOnly
  roster at literally two (generalized to "every bossOnly foe is some boss's
  authored court", now 7); the castle-outclass invariant swept the Duck into
  "bosses a dungeon can field" (he now shares the King's never-in-a-dungeon
  exclusion); the loader's entity-count pins (enemies 45 → 50, bosses
  13 → 14); the story-serial shape (8 → 9 beats, + a Goofy Jester identity
  check); and the Goofy Jester's tale wrapped to 14 dialog lines against the
  panel's 12 — the ballad was trimmed, not the panel.
- **Closing verification: 562/562 Debug and 558/558 Release tests green**
  (the 4-case gap is the debug-only god-mode battery); `--capture` **77/77**
  scenes clean; zero project-code warnings on the forced recompile of the
  new TUs.

## G. Manual owner checklist

1. Debug build. Fell the King with ≥1 Goose in the party (god mode + the
   debug menu make this quick) — the result screen announces Goose Town.
2. Walk town 7's north road: the fork prompt offers Castle / Goose Town;
   Cancel steps back; both destinations enter and leave cleanly.
3. In Goose Town: hear the Goofy Jester's tale (text fits), rest, save;
   reload that save — the town stays unlocked.
4. Fight the gauntlet. Watch for: geese occasionally spending a turn on
   "Quack." (roughly 1 turn in 10 per goose); the Crystal Shatter + King
   theme on the Duck; his all-party strikes applying ATK-down/poison; your
   afflictions (Evil Goose, Tax Sheets, poison, blind...) bouncing off him
   while the Deadly Spoon lands; Counter/Thorns/Spell Ward chips visible.
5. Lose once (the 1-HP castle price applies), win once (expect a REAL
   fight — bring the Spoon and healing; the itemless sim cannot beat him by
   design). The result shows the turns; Quackbane toasts; the Pond Records
   panel updates; the bestiary shows the Duck's 500% max stats.
6. Confirm the Duck never appears in the Boss Rush or dungeons, and the
   Evil Geese never appear in endless waves.
7. Sanity-check an ordinary dungeon battle and a King fight (rules v12 must
   change nothing pre-M61).
8. On failure: the result text, a save, and what you fought.

## H. Known limitations

- ~~The Duck and the Evil Geese render with sprite-catalog fallbacks~~ —
  **addressed in M62** (bespoke generated sprites).
- Goose Town's hub reuses the castle's music and its fights the Castle
  battle backdrop — a bespoke pond stage is presentation the owner may
  direct later. ~~The Duck fight borrows the King's theme~~ — **addressed in
  M62** (`MusicTrack::DuckBattle`).
- The road signpost still reads "^ Castle" at the fork (the prompt carries
  the choice); easy to retext on request.
- The gauntlet, like the castle, is deliberately beyond the itemless
  simulator: the balance bar is counterplay-assisted (see §E), and the
  owner's own fight is the real referee.

## I. Final status

`implemented, awaiting manual approval`
