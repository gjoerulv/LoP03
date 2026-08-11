# M75 — Battle rules v15: Reflect, Sleep, Curse & the trigger engine

Authorized 2026-08-05 as the opening milestone of the M75–M86 expansion
program (one plan, one approval — see the program section in
`docs/milestones.md`). Implemented 2026-08-05 on the post-M74 checkout
(base commit `fe6c9ae`).

## A. Status

**☑ complete (approved)** — implemented 2026-08-05; approved and
committed by the owner 2026-08-05 (`2538a92`). Evidence in §F.

## B. Goal (owner brief)

One atomic battle-rules revision (v14 → v15) that lands every engine-level
combat change of the program, so later milestones are content authoring on a
stable rule set: three new statuses (Reflect, Sleep, Curse), poison that
scales into the endgame, buffs/debuffs that matter, MP damage, battle-start
statuses, a generic deterministic boss/elite trigger system, sleep-aware
enemy targeting, the equipment element-resist hook, and an element line in
the battle log.

## C. As implemented

`battle::kBattleRulesVersion` **14 → 15** — the program's only rules bump;
its header comment in `src/battle/Battle.hpp` is the authoritative change
list. `docs/technical_design.md` §29 carries the full architecture record;
`docs/game_design.md` §10 ("Battle rules v15") the player-facing rules.
Highlights and the decisions taken:

### The three statuses

- **Reflect** bounces hostile *magic-category* skills back onto their caster
  — damage, the `mpDamagePct` drain and the status rider alike — resolved in
  `useSkill`'s target loop *before* the Spell Ward roll as a pure branch:
  `rollCursor` never moves for a mirror. Wears off naturally.
- **Sleep** is a `ForcedAction::Skip` (between Stunned and Terrified in
  `forcedActionFor`); the wake rule sits at the `applyDamage` chokepoint
  beside the M35 confusion snap, so the poison tick — which bypasses that
  chokepoint — deliberately does **not** wake (the owner's rule). Cleanses
  and Cure items lift it; it wears off naturally.
- **Curse** halves outgoing damage as the last attacker-side modifier in
  `dealPhysical`/`dealMagic` (and the counter-attack site) and doubles MP
  costs through the new shared `battle::mpCostFor` — used by `useSkill`'s
  deduction, both AIs and the skill menu, so a cursed caster can never pick
  what it cannot pay for in one driver but not the other. Duration ×1.5
  (`kCurseDurationPct` at the `addStatus` chokepoint; authored 2 → 6
  effective ticks vs 4 for anything else). Exactly two removers, as the
  owner specified: the new `SkillEffect::Uncurse` and the new
  `ItemDef.curesCurse` flag — `clearAfflictions` (Purify) and
  `clearNegativeStatuses` (Remedy) both keep a Curse (and keep Reflect,
  which is beneficial).
- `isAffliction` gained Sleep and Curse (so the Deadly Duck's M61 blanket
  will refuse both); `SkillEffect::BreakReflect` strips Reflect from enemy
  targets (loader-forbidden on magic skills — a magic breaker would bounce
  off the very mirror it came to break).

### The rebalances (the deliberate v15 behavior changes)

- **Poison scales**: applied magnitude = authored + applier MAG /
  `kPoisonMagicDiv` (4), snapshotted at application via
  `statusMagnitudeFor` — skills, attack riders and triggers scale;
  item-applied statuses keep authored numbers (an item has no caster craft
  behind it).
- **ATK±** now scales the whole `(attack + power)` physical term; **DEF±**
  scales the **final** damage taken on both damage paths (DEF+30 ≈ −23%,
  DEF−30 ≈ +43%). ATK± still does not touch magic damage (its historical
  shape). With no statuses every formula reduces exactly to its v14 result
  — proven by test — so only battles where poison/ATK±/DEF± appear resolve
  differently, which is the point of the bump.

### New schema (all optional, all inert until authored — the M61 precedent)

- `SkillDef.mpDamagePct` (0–100, damaging categories only): the target also
  loses that percent of the dealt HP damage as MP. The owner's ¼ rule is an
  authored 25.
- `initialStatuses[]` on enemies and bosses (applied at `buildBattle`
  through `addStatus`, so durations scale and immunities hold).
- `statusImmunities[]` per-status immunity lists (the future Dragon's
  matrix); blocked at `addStatus` and honoured by `isImmuneTo`. The legacy
  immunity FLAGS keep their historical store-but-ignore behaviour — changing
  that would have altered v14 battles (a stored inert status feeds Keen
  Senses).
- `avoidSleepingTargets` / `noStunWhileAllFoesSleep` AI manners in
  `chooseEnemyAction` (sleepers skipped in target scoring while anyone else
  stands — multi-hit sweeps exempt by nature; stun-rider skills shelved
  while every foe sleeps).
- `BossDef.immuneToStatScale` — the Deadly-Spoon shrug; `itemAffects`
  returns false for a scale-only relic on such a boss, so the caller keeps
  the item (the M44 rule).
- `ItemDef.resistElements` / `resistPct` (equipment/relic, both-or-neither):
  resolved per party member at `buildBattle` into
  `Combatant.elementResist`, applied inside `elementModifier` as
  `mod × (100 − resist) / 100` after the weak/immune decision (immunity's 0
  stays 0; a 50% resist takes a weak 150 to 75). Content arrives in M81.

### The trigger framework

`TriggerWhen` × `TriggerDo` (content enums with parse/toString/ids tables →
CrystalForge dropdowns), authored as `triggers[]` on enemies and bosses,
mirrored into pure `battle::TriggerRule`s at `buildBattle`. Conditions:
`every_nth_hit_taken` (fires in `dealPhysical`/`dealMagic` after a
deliberate connecting hit — thorns, counters and poison never count),
`first_time_hp_below_pct`, `every_nth_own_turn`, `first_time_ally_felled`
(all three evaluated in `beginUnitTurn`, the M49 revive-clock seam both
drivers already call; the clock body moved to `reviveCourtRule` and
composes). Actions: status to self / the attacker / every foe / the
bearer's own boss (the King's-minion shape), `scale_stats_self` (the Spoon
shape, upward — the Dragon's ATK/SPD doubling), `drain_foe_mp` (the
Dragon's one-shot), and `summon_clone`. **The clone is prebuilt dead at
`buildBattle`** (`Combatant.summonSlot`, the bearer's combat identity minus
its triggers, revive clock and battle-start statuses; maxHp = cloneHpPct of
the bearer's) so the unit roster never grows mid-battle — the trigger
simply raises it, the existing revive presentation fades it in, and
`BattleState` zeroes `koFade_` for units that start dead so the slot never
ghosts. No condition or action ever consumes a roll: sim == live by
construction. Loader semantics validate every condition/action pairing
(`status_attacker` only with the hit condition, `summon_clone` boss-only
with a 1–100 percent, scale percents 1–400, etc.).

### Presentation

Battle log names an attack's element (owner decision 2026-08-05 — "(Fire)"
on the attack/skill line); status chips gained RFL/SLP/CRS; the battle
Details legend explains all three; the skill menu shows the cursed doubled
cost through the same `mpCostFor` rule; the Skip beat says "fast asleep"
when Sleep (not a stun) took the turn.

## D. Files changed

- **Content model:** `src/content/Enums.hpp/.cpp` (StatusType +3,
  SkillEffect +2, the TriggerWhen/TriggerDo enums + tables + ids),
  `src/content/Definitions.hpp` (SkillDef.mpDamagePct, TriggerDef,
  EnemyDef/BossDef/ItemDef fields), `src/content/ContentLoader.cpp`
  (readStatusList/readStatusImmunities/readTriggers + per-category reads +
  semantic rules).
- **Battle:** `src/battle/Battle.hpp` (version 15, constants, TriggerRule,
  Combatant fields, new queries + `mpCostFor`, trigger member decls),
  `src/battle/Battle.cpp` (all rules — see §C), `src/battle/Simulator.cpp`
  (cursed costs in the party AI).
- **Presentation:** `src/states/BattleState.cpp` (chips, menu cost, sleep
  lines, koFade for dead-start slots, Details legend).
- **Editor:** `src/editor/CategoryDescriptors.cpp` (triggerChildren + every
  new field; the M59 completeness sweep enforces descriptor coverage, so
  this could not wait for M86 — M86 still owns the new content *files* and
  the final audit).
- **Tests:** `tests/test_rules_v15.cpp` (new, 24 cases, `[v15]`),
  `tests/CMakeLists.txt`, `tests/test_status.cpp` + `tests/test_status_v2.cpp`
  (two stale v14 poison pins updated to the scaled values, honestly),
  `tests/test_editor_enum_lists.cpp` (pins extended to the new enums).
- **Docs:** this note, `docs/milestones.md`, `docs/game_design.md` §10,
  `docs/technical_design.md` §29.
- **Data:** none — M75 ships no content; every new field waits for M76+.

## E. Plan deviations

- **Editor descriptors landed here, not M86.** The M59
  descriptor-completeness battery fails the build the moment a loader key
  lacks a descriptor, so the rows shipped with the fields. M86 keeps the
  new content files (event_flavor, curio_lore) and the closing audit.
- **The trigger action set dropped `castSkill`.** The four action families
  cover every behavior the owner named (the King-minion reflect is
  `status_boss`, the Dragon's MP burn is `drain_foe_mp`); a cast-skill
  action would have needed the content database inside `beginUnitTurn` for
  no named use. Recorded for M77/M84/M85 authoring.
- **The byte-identity bar was stated too broadly in the planning note.**
  v15 deliberately changes any battle containing poison or ATK±/DEF± (the
  rebalance is the point); the proven guarantee is: a battle with none of
  the new fields *and* none of those statuses resolves identically.
- No other deviations.

## F. Automated validation (all run in this session, 2026-08-05)

- Debug configure/build: clean, **zero project-code warnings** (Ninja,
  VS2022 dev shell).
- **Full Debug suite: 631/631 tests green** (607 legacy + the 24 new
  `[v15]` cases; ~200 s). The only pre-existing tests that needed touching
  were the two poison pins and the enum-list pin — all three updated to
  assert the v15 rules, none weakened.
- The in-suite balance proofs all held unchanged under v15: the M61
  counterplay gauntlet proof (the Duck falls 5/5 with the kit, 0/5 bare),
  the castle/king batteries, and the danger calibration.
- **Capture lint: 84/84 scenes clean** (byte-run against the fixed scene
  list; no scene stages the new statuses yet — M77 content will).
- **Release configure/build/suite: 627/627 tests green**, zero warnings
  (the 4-case gap to Debug is the debug-only god-mode battery, as always).
- Closing verification: **631/631 Debug and 627/627 Release tests green;
  `--capture` 84/84 scenes clean; zero project-code warnings.**

## G. Manual owner checklist

M75 ships engine + schema with **no content using it**, so ordinary play
should feel identical. The checklist is therefore half regression, half
debug-poke:

1. Debug build. Run one ordinary dungeon battle with poison/buff skills in
   play (a Ranger's Venom Fang, a Guardian's Bulwark): poison should
   visibly bite harder than before and DEF+ visibly blunt hits; the log
   should name elements on elemental swings ("(Fire)" etc.).
2. Cast Silence/nothing-new fights: confirm nothing else feels different
   (v15 is inert without the new statuses).
3. In the battle Details overlay, confirm the new RFL/SLP/CRS legend line
   reads well and fits.
4. Optional debug-poke (or wait for M76/M77 to meet them in play): the
   three statuses have no shipped source yet — if you want to see them
   now, CrystalForge can author a test enemy with
   `initialStatuses: [{"type":"reflect","duration":2}]` or a
   `triggers` entry; otherwise judge them when M76/M77 land the content.
5. On failure: the battle log text, a save, and what you fought.

## H. Known limitations

- The three statuses, triggers, MP damage, resists and manners are
  **unreachable in normal play until M76/M77/M81 author content** — by
  design (engine first).
- A boss carrying BOTH a revive clock and a clone would revive the clone
  as court (the clone is a non-boss same-side unit). No such boss is
  planned; recorded as an authoring caveat for M77/M84/M85.
- The `attackerElementMod` weakness-override (M63 Elemental Attunement)
  keys on the exact 150 value, so it does not compose with a defender
  resist — irrelevant today (only party members wear resist and party
  members have no weaknesses), recorded for completeness.
- Item-applied poison keeps authored magnitude (no caster scaling) — a
  deliberate scope line, revisit only if an item poison ever ships.

## I. Final status

`complete (approved)` — owner approval 2026-08-05, committed as `2538a92`.
