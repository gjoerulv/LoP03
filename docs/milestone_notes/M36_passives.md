# M36 — Passive Skills

## A. Status and authority

- **Status:** complete (approved) — approved by the owner 2026-07-21 after manual
  testing; committed. Authored / re-audited and implemented 2026-07-21 against HEAD
  `94c79a1` ("M35"). The note-time catalog + price decisions were confirmed (all 10
  passives, flat 1000 g). As-implemented record in §K.
- **Authority:** `CLAUDE.md` > this note > `docs/milestones.md` >
  `docs/game_design.md` > `docs/technical_design.md`. Second milestone of the
  M35–M42 endgame program; builds directly on M35's seeded to-hit layer (Evasion
  reuses it). **This is the program's last battle-rules change**
  (`kBattleRulesVersion` 2 → 3); M37+ change generation, not rules.

## B. Problem statement (verified at re-audit, HEAD `94c79a1`)

- **No passive-skill concept exists.** Combat power is stats + equipment +
  learnset skills only. A character has no persistent, always-on trait.
- **The hooks this milestone needs all exist as precedent.** Boss mechanics are
  resolved into per-`Combatant` flags at `buildBattle`
  ([Battle.cpp:507](src/battle/Battle.cpp)) — `enrages`, `empowersOnAllyFall`,
  `ralliesMinions`, `rushOpener` — and read directly in the pure `attack`/
  `useSkill`, so the sim stays db-free and deterministic. Passives follow the
  same pattern: resolve each unit's passive(s) into Combatant fields at
  `buildBattle`, read them in the shared methods. M35 added the **seeded roll
  stream** (`Battle.rollCursor`/`nextRandom`) that Evasion/Spell Ward reuse, so
  new chance-based passives stay sim/live-identical.
- **`Character` already round-trips optional fields defensively**
  (`weapon`/`armor`/`accessory`, [SaveSystem.cpp:181](src/save/SaveSystem.cpp);
  unknown ids dropped on load). `ownedPassives` + `equippedPassive` extend that
  exact pattern — no `kSaveVersion` bump.
- **The Training Hall** ([TrainingHallState.cpp](src/states/TrainingHallState.cpp))
  is a per-character gold-spend menu — the natural home for buying/equipping
  passives; it gains a second flow.
- **Content loading** is one `parse*` per file wired into `loadAll`
  ([ContentLoader.cpp](src/content/ContentLoader.cpp)) with cross-reference
  validation; `passives.json` adds one parser + a `PassiveDef` map on
  `ContentDatabase` and a reference check (character/enemy/boss passive ids).
- **Target-info** ([BattleState.cpp](src/states/BattleState.cpp) ChooseTarget)
  already surfaces a foe's stats/statuses — the place to reveal a foe's passive.
- **Baseline:** 321/321 tests, 27/27 capture, `kBattleRulesVersion` 2,
  `kGenerationVersion` 6, `kSaveVersion` unchanged.

## C. Goals

- A **`passives.json`** content type (id, name, description, hook, magnitude,
  price) + loader/validator/reference-check.
- **Own many, equip one** (owner decision): the Training Hall sells passives per
  character; owned passives persist; the single equipped passive swaps freely.
- **Deterministic sim hooks** for the owner-approved catalog (§D), resolved into
  `Combatant` fields at `buildBattle` and read in the pure `attack`/`useSkill`/
  turn-order paths. Evasion uses M35's to-hit roll.
- **Enemy/boss passives**: `EnemyDef`/`BossDef` gain an optional passive list; the
  target-info panel reveals a foe's passive.
- `battle::kBattleRulesVersion` **2 → 3** (last rules bump of the program);
  balance battery re-tuned.

## D. Owner decisions (note-time) & routine choices

**Note-time owner decisions (the plan defers these here):**

- **Catalog (final set).** The plan's proposed 10, first two owner-specified.
  Confirmed set + per-passive hook and magnitude:

  | # | Passive | Hook | Effect (magnitude) |
  |---|---------|------|--------------------|
  | 1 | **Counter Attack** | Counter | after surviving a physical hit, retaliate with a basic attack (once per round) |
  | 2 | **Evasion** | Evasion | 25 % of physical attacks miss you; a **Blind** attacker **always** misses you (owner rule) |
  | 3 | **Spell Ward** | SpellWard | 25 % of hostile magic fizzles on you (no damage/status) |
  | 4 | **Thorns** | Thorns | a physical attacker takes 20 % of its dealt damage back |
  | 5 | **Lifedrink** | Lifedrink | heal 15 % of the physical damage you deal |
  | 6 | **Clarity** | Clarity | +3 MP at the start of each round; immune to Silence |
  | 7 | **Iron Will** | IronWill | survive a lethal blow at 1 HP (once per battle) |
  | 8 | **First Strike** | FirstStrike | act first in round 1; +50 % damage on your first damaging action |
  | 9 | **Bodyguard** | Bodyguard | 25 % of damage aimed at the lowest-HP ally is redirected to you |
  | 10 | **Keen Senses** | KeenSenses | immune to Blind; +10 % damage vs a debuffed target |

- **Prices (Training Hall, gold) — owner-confirmed 2026-07-21:** a **flat
  1000 gold** per passive, stored in each passive's `price` field so it stays
  data-tunable. (Owner chose flat over the tiered proposal.)
- **Catalog — owner-confirmed 2026-07-21:** **all 10** as specified above.

Routine, reversible choices taken here (owner validates feel/balance at approval):

- **Storage/schema.** `content::PassiveDef { id, name, description, hook,
  magnitude, price }`; `PassiveHook` enum (10 values) with parse/toString; a
  `PassiveDef` map on `ContentDatabase`. `Character` gains `std::vector<std::string>
  ownedPassives` + `std::string equippedPassive` (optional save fields, old →
  empty). `EnemyDef`/`BossDef` gain `std::vector<std::string> passives` (optional).
- **Sim resolution.** `buildBattle` resolves each unit's passive ids (a party
  member's single equipped passive, or an enemy/boss's list) into `Combatant`
  passive fields (mirroring the boss flags). The pure methods read the fields —
  **no db in the hot path**, determinism preserved.
- **Evasion/Blind interaction.** Physical miss chance for a defender =
  `blindAttacker && evasionDefender ? 100 : max(blind?75:0, evasion?25:0)`, one
  seeded roll off `rollCursor`. Keen Senses / Clarity set `blindImmune` /
  `silenceImmune` so those statuses simply don't apply/act on the holder.
- **Foe passive legibility.** The target-info panel and Details **always** show a
  foe's passive name when it has one (matches how stats are always shown); the
  "seen" discovery gate is left to M42's bestiary. Flagged for owner veto.
- **Counter Attack safety.** A counter is a basic attack that never itself
  triggers a counter, and each unit counters at most once per round
  (`counteredThisRound`, reset in `beginRound`) — no recursion/ping-pong.
- **First Strike turn order.** In round 1 only, `turnOrder` floats First-Strike
  units to the front (ties broken as today); the +50 % applies to the holder's
  first damaging action (a per-battle `firstStrikeUsed` flag).

## E. Proposed design & slices

1. **Content type.** `PassiveHook` enum + `PassiveDef` + `ContentDatabase`
   passive map/count; `parsePassives` + `loadAll` wiring + reference check
   (character equipped/owned, enemy/boss lists resolve); `data/passives.json`
   with the 10 rows. Headless loader/validation tests.
2. **Sim hooks.** `Combatant` passive fields; `buildBattle` resolution (party =
   equipped passive; enemy/boss = list); the 10 hooks in `attack`/`useSkill`/
   `beginRound`/`turnOrder`/`applyDamage` as in §D. `kBattleRulesVersion` 2 → 3.
   `[passive]` tests for every hook incl. determinism and a status-free/no-passive
   battle staying byte-identical to v2.
3. **Acquisition + save.** `Character.ownedPassives`/`equippedPassive` (optional
   save fields, New Game empty); a Training Hall passive flow (browse per
   character → buy owned / equip one / unequip), priced from the def. Save
   round-trip + defensive-drop tests.
4. **Legibility.** Target-info + Details show a foe's passive; the party/status
   screen shows each member's equipped passive; Details explains it. Non-color.
5. **Enemy/boss passives.** A conservative pass giving fitting foes a passive
   (e.g., a Thorns golem, an Evasion stalker, a Counter bruiser); bosses may take
   one or two where the archetype fits (the King's three are M40). Balance-checked.
6. **Tests/capture/battery.** Full `[passive]` suite; a capture scene showing the
   Training Hall passive flow and a battle where a foe's passive is revealed;
   re-tuned `[economy-report]`/balance battery (last rules change).

## F. Determinism & save compatibility

- **`kBattleRulesVersion` 2 → 3** (passives change how battles resolve). A battle
  where no unit has a passive is byte-identical to v2. `kGenerationVersion`
  unchanged (passives are not a generation input). Scoreboard tags the version
  (mechanism exists).
- **No `kSaveVersion` bump.** `ownedPassives`/`equippedPassive` are optional
  character fields (old saves → empty); a passive id the content no longer knows
  is dropped on load (as gear is). Enemy/boss passive lists are content, not save.
- **Sim/live agreement**: passive fields resolved once at `buildBattle`; all
  chance-based hooks draw from the shared `rollCursor`; counters/regen run in the
  shared methods both drivers call. No new unseeded RNG.

## G. Out of scope

Per-town equipment/enemies (M37/M38); boss drops (M39); the castle & the King's
three passives (M40); a "seen"-gated bestiary (M42); any generation change; new
passive hooks beyond the 10.

## H. Balance / acceptance

Every hook is deterministic and reproducible; a no-passive battle is byte-
identical to v2; Evasion honours the Blind-always-misses rule; own-many-equip-one
round-trips through saves; the Training Hall flow prices from the data; the
re-tuned battery keeps the town × depth grid clearable (passives must not trivialize
runs — they are a per-character choice, one equipped); capture overflow-clean;
presentation lint green (new sprites, if any, and the passives content).

## I. Manual validation for the owner

Whether passives feel impactful without being mandatory; whether own-many-equip-one
reads clearly at the Training Hall; whether a foe's revealed passive is legible and
changes targeting decisions; overall balance with passives in play.

## J. Required documentation updates on completion

This note (§K); `docs/milestones.md` (status); `docs/game_design.md` (passives as a
player-facing system + the catalog); `docs/technical_design.md` (`PassiveDef`/
`PassiveHook`, Combatant fields, buildBattle resolution, `kBattleRulesVersion` 3,
the save fields); `assets/credits.md` + `docs/asset_pipeline.md` if any sprite is
added; `docs/manual_test_matrix.md` rows; a manual checklist per
`docs/milestone_completion_template.md`.

## K. As-implemented record (2026-07-21)

- **Status:** complete (approved 2026-07-21). Implemented / audited against
  HEAD `94c79a1`. Owner confirmed the note-time decisions: **all 10** passives, a
  **flat 1000 gold** price each (chosen over the tiered proposal).
- **Content type.** `content::PassiveHook` (10 values + inert `None`) with
  parse/toString; `PassiveDef { id, name, hook, magnitude, price, description }`;
  a `PassiveDef` map on `ContentDatabase` (`addPassive`/`findPassive`/`passives`/
  `passiveCount`); `parsePassives` + `loadAll` wiring (loaded before enemies/
  bosses so references validate); reference checks for enemy/boss passive lists;
  `data/passives.json` with the 10 rows at 1000 g.
- **Sim hooks.** `Combatant` gained the passive effect fields; `applyPassives`
  resolves ids → fields at `buildBattle` (party = the single `equippedPassive`;
  enemy/boss = the def list). All 10 hooks implemented in the pure paths:
  `dealPhysical`/`dealMagic` (new helpers centralizing first-strike/keen bonuses,
  the Bodyguard split, Thorns/Lifedrink/Counter), `physicalMissPct` (Blind+Evasion
  with the 100 %-vs-evader rule), Spell Ward (`kSaltWard` roll), Clarity/Counter in
  `beginRound`, Iron Will in `applyDamage`, First Strike in `turnOrder`.
  `isBlinded`/`isSilenced` now honour `blindImmune`/`silenceImmune`. **A battle
  with no passive draws no roll and is byte-identical to v2** — the pre-existing
  battle/enmity/boss/status/balance tests pass unmodified.
  `battle::kBattleRulesVersion` **2 → 3**.
- **Acquisition + save.** `Character.ownedPassives` + `equippedPassive` (optional
  save fields; unknown ids and an unowned equipped id dropped on load — no
  `kSaveVersion` bump). The Training Hall became a 3-phase flow (Members →
  Char menu [Train / Manage Passives] → Passives), buying/equipping/unequipping
  per character, priced from the def; auto-equips a fresh purchase.
- **Legibility.** The battle target-info panel and Details show a unit's passive
  by name (foe or ally); the Training Hall members list shows each member's
  equipped passive.
- **Enemy/boss passives.** A conservative pass: elites `troll_berserker`
  (Counter), `crystal_guardian` (Spell Ward), `shadow_stalker` (Evasion),
  `iron_sentinel` (Thorns), `bone_colossus` (Iron Will), `void_weaver` (Keen
  Senses); bosses `crystal_sorcerer` (Spell Ward), `hollow_commander` (Iron Will),
  `deep_king` (Thorns), `blight_matron` (Keen Senses). The two tutorial-tier
  bosses and all normal enemies stay pure (early-game clearability); no base stats
  changed.
- **Validation (this session, VS 2022 / MSVC 14.44, `build-msvc`).** Debug build
  clean; full suite **334/334** (321 M35 baseline + 12 new `[passive]` cases in
  `test_passives.cpp` + 1 new `[save]` passive round-trip). `--capture` **29/29**
  overflow-clean (new `28_training_passives` and `29_battle_passive_reveal`
  scenes). Release build + Release suite **334/334** clean. `[economy-report]`
  clearability grid (worst of 3 seeds, depths 1/2/3/4/5/6/8/10): clearing levels
  **1/4/3/3/5/8/8/10**, all depths clearable — the economy guards pass; the small
  rises from the enemy/boss passives are flagged for the owner's balance read.

### Deviations from the plan / note

1. **Flat 1000 g pricing** (owner choice) rather than the tiered proposal.
2. **Bodyguard soaks magic too** (any damage aimed at the weakest ally), not only
   physical — the plan said "damage aimed at the lowest-HP ally"; magic is damage.
3. **First Strike uses `!actedOnce` for turn priority** rather than a round
   counter, because the Simulator never advances `turnsTaken` (M35 finding) — this
   is driver-independent and reads as "acts first at the start of the battle".
4. **Foe passives are always shown** in the target panel (no "seen" gate) — the
   discovery gate is deferred to M42's bestiary (flagged for owner veto).
5. **A party status/passive screen was not added** separately; the Training Hall
   members list and the battle target-info/Details cover passive visibility.

### Known items for owner validation

- Balance feel of the enemy/boss passives (grid rises at depths 4/5/10); whether
  1000 g per passive feels right; whether own-many-equip-one reads clearly; whether
  each of the 10 hooks is legible in play.
