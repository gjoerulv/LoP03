# M43 — Balance pass & audit fixes

## A. Status and authority

- **Status:** ☑ complete (approved) — approved by the owner 2026-07-22 after
  manual testing; committed as `65c9a47`. The first milestone of the **King's
  Gambit program**, M43 → M44 → M45. See §J for the as-implemented record and
  deviations (all three accepted with the approval). The owner authorized **M44**
  at the same time.
- **Authority:** `CLAUDE.md` → this note → `docs/milestones.md` →
  `docs/game_design.md` → `docs/technical_design.md`. The program's approved
  implementation plan lives in the owner's plan file; this note is its
  just-in-time, re-audited M43 slice and is the authoritative scope while M43
  runs.
- **Base checkout:** `9dff867` ("Pre M43 doc update"), working tree clean at
  authorization. The plan's code facts were verified one commit earlier at
  `bc10511`; §B records the re-audit against `9dff867`.
- **Baseline:** 368/368 tests, `--capture` 44/44 scenes overflow-clean.
- **Terminal state for this session:** `implemented, awaiting manual approval`.
  Only the owner sets `complete (approved)`.

## B. Re-audit of the plan's facts against `9dff867`

Every fact the plan asserted was re-checked in this checkout. All hold; nothing
had drifted between `bc10511` and `9dff867` (that commit is documentation only).

| Plan claim | Verified at `9dff867` |
|---|---|
| Confusion is enforced only on the live-party side | `Battle::attack` redirects a confused attacker ([Battle.cpp:552](../../src/battle/Battle.cpp:552)); `chooseEnemyAction` ([Battle.cpp:963](../../src/battle/Battle.cpp:963)) and the Simulator's `choosePartyAction` ([Simulator.cpp:27](../../src/battle/Simulator.cpp:27)) never check it; `useSkill` does not redirect; the party lockout lives in `BattleState::startActorTurn` ([BattleState.cpp:336](../../src/states/BattleState.cpp:336)) |
| Castle defeat claims a gold loss that never happens | `outcomeMessage()` returns the "half your gold is lost" line for every defeat ([BattleState.cpp:649](../../src/states/BattleState.cpp:649)); `CastleChallengeState::finish(false)` neither heals nor charges ([CastleChallengeState.cpp:92](../../src/states/CastleChallengeState.cpp:92)) |
| Boss-rush progress text hardcodes 12 | "of 12 bosses" at [CastleChallengeState.cpp:113](../../src/states/CastleChallengeState.cpp:113); `bossRushOrder` derives the roster ([Castle.hpp:108](../../src/game/Castle.hpp:108)) |
| Battle items target any party member incl. KO'd | [BattleState.cpp:491](../../src/states/BattleState.cpp:491); `useItem` Heal/Revive on the wrong state is a "No effect" no-op that still spends the item ([Battle.cpp:780](../../src/battle/Battle.cpp:780)) |
| Skill ally targeting is alive-only | [BattleState.cpp:477](../../src/states/BattleState.cpp:477) (`aliveIndices`) |
| Prices / skills / passive to change | `antidote` "Remedy" 15, `ether` 60, `hi_ether` 150, `phoenix_tear` 150 with `effectAmount` 50 ([items.json:7](../../data/items.json:7)–:10); `purify` heal 12 + cleanse, all_allies ([skills.json:59](../../data/skills.json:59)); `renew` heal 26 / 7 MP ([skills.json:27](../../data/skills.json:27)); `clarity` magnitude 3 ([passives.json:9](../../data/passives.json:9)) |
| The King | `the_hollow_king` hp **560**, atk 18 / mag 22 / def 18 / spd 13, `immuneToConfusion` ([bosses.json:165](../../data/bosses.json:165)); `kKingBossId` ([Castle.hpp:67](../../src/game/Castle.hpp:67)). **M44 re-stats it — untouched here.** |
| Item shop has no town gating | `ItemShopState::rebuild()` lists every consumable ([ItemShopState.cpp:34](../../src/states/ItemShopState.cpp:34)); `ItemDef::minTown` exists (M37) but only the equip shop ([EquipShopFilter.hpp:33](../../src/states/EquipShopFilter.hpp:33)) and the generator pools honor it |
| Repricing moves generation | the dungeon Merchant event prices its offer at `item.value * 75 / 100` ([DungeonGenerator.cpp:479](../../src/dungeon/DungeonGenerator.cpp:479)), so a reprice changes what a seed's merchant charges → generation bump |
| Versions | `battle::kBattleRulesVersion` 3, `dungeon::kGenerationVersion` 8, `kSaveVersion` 1 |

## C. Goals

Features 5–9 of the owner's request plus the four 2026-07-22 audit findings,
landed at **one** battle-rules bump (3 → 4) and **one** generation bump (8 → 9).
No save-version bump: every schema addition is an optional field with a
backward-compatible default.

## D. Slices

### D1 — Confusion forces a basic attack (audit #1, #6)

A confused unit may not act deliberately. The rule moves into shared `battle::`
code so live play and the Simulator agree **by construction**, which is the
structural reason this program exists:

- New pure `battle::confusedChoice(const Battle&, int actor)` returns an
  `EnemyChoice` with `useSkill == false` and a nominal living opposing target
  (`attack()` then performs the existing seeded same-side redirect).
- `chooseEnemyAction` returns it first thing when `isConfused(self)` — a
  confused enemy can no longer heal, buff, or cast.
- The Simulator's `choosePartyAction` gets the same guard, closing the
  sim-fidelity gap (the sim previously let a confused party member cast).
- `BattleState::executeConfused` picks its nominal target through the same
  helper, so there is exactly one rule.

Immunity is already respected: `isConfused` honours `confusionImmune`, so the
King is unaffected. `targetJitter` is a pure hash and `confusedChoice` draws no
randomness, so the early return does not disturb `rollCursor`.

### D2 — Truthful castle defeat (audit #2, #3)

- `BattleState` gains a constructor flag `castleChallenge` (default false).
  When set, the defeat line states the truth — the party is carried back to the
  castle gates, no gold is lost, the challenge ends.
- `CastleChallengeState::finish(false)` **fully heals the party at the gates and
  charges nothing** (plan recommendation; the owner validates the feel). It also
  says so, so the screen and the battle message agree.
- The boss-rush defeat line derives its total from `bossRushOrder(db).size()`
  instead of the literal 12.

### D3 — Repricing (feature 5)

| Item | value | change |
|---|---|---|
| Remedy (`antidote`) | 15 → **100** | a cure is no longer pocket change |
| Ether | 60 → **150** | MP becomes a real resource |
| Hi-Ether | 150 → **500** | |
| Phoenix Tear | 150 → **300**, `effectAmount` 50 → **25** | costs more, gives less |

Descriptions follow the numbers ("Revives a fallen ally with 25% HP").

### D4 — Clarity nerf (feature 6)

`passives.json` `clarity` magnitude 3 → 2 (+2 MP each round; Silence immunity
unchanged), description updated. Affects the King too — it carries `clarity`;
M44's re-stat accounts for that.

### D5 — Purify is cure-only (feature 7)

`purify` power 12 → 0, description rewritten. Data only: the `Cleanse` control
effect already runs independently of the heal branch.

### D6 — Renew revives (feature 8)

New **schema capability**, not a hardcoded skill: `SkillDef` gains
`reviveHpPct` (0 = not revive-capable; 1–100 = revive a KO'd ally at that
percentage of max HP). One field carries both the capability and its magnitude.

- `Battle::useSkill`'s Heal branch: a dead target with `reviveHpPct > 0` is
  revived at `max(1, maxHp * pct / 100)`; a living target is healed as before; a
  dead target of a non-revive heal is still skipped.
- Ally target lists for a revive-capable skill include KO'd allies — through a
  new pure `battle::skillAllyTargets(...)` the battle screen calls, so the rule
  is testable headlessly.
- Loader validation: `reviveHpPct` is 0–100 and may only be set on a
  `heal`/`single_ally` skill (an authoring mistake is a load error, not a
  surprise at runtime).
- **Tuning (note-time decision):** `renew` power 26 → **12**, mpCost 7 → **9**,
  `reviveHpPct` **20**. Rationale: Renew stops being the Cleric's efficient
  mid-tier heal (Mend 20/5 and Greater Heal 40/10 keep that job) and becomes the
  emergency button; 20 % is below the repriced Phoenix Tear's 25 %, so the item
  stays worth its 300 g, and under fewest-turns scoring a revive is already a
  tempo loss. Reversible data tuning — the owner can veto the numbers without
  touching code.

### D7 — Battle item targeting by effect (feature 9)

New pure `battle::itemTargets(const Battle&, Side, const ItemDef&)`:

- `Heal` / `RestoreMp` / `Cure` / `None` → **living** party members only;
- `Revive` → **KO'd** party members only.

`BattleState::onItemChosen` uses it, and refuses the item with a message when no
valid target exists (a Phoenix Tear with nobody down no longer opens a target
cursor, and no item can be wasted on a "No effect" no-op).

### D8 — Royal Snacks (feature 4)

New consumable: **Royal Snacks**, 250 g, description **"Bring Snacks!"**.

- Normally: **10 HP** to one ally and cures **ATK-/DEF-**.
- In the King's fight: **100 HP + 10 MP** (and the same cure).

Supporting schema, all optional with inert defaults:

- `ItemDef.maxTown` (0 = unbounded) completes the town window `minTown..maxTown`,
  exposed as `ItemDef::availableAtTown(town)`. The **item shop** now honours the
  window (new pure `itemShopBuyIds(content, town)`, mirroring
  `equipShopBuyIds`), as do the generator's chest/merchant pools and the equip
  shop — one definition of "available at this town". Every existing item has
  `minTown` ≥ 1 and no `maxTown`, so nothing else changes.
- `ItemDef.curesDebuffs` — the item also lifts ATK-/DEF- (stat debuffs only, not
  poison/blind/silence/confusion; that is what `Cure` is for).
- `ItemDef.kingEffectAmount` / `ItemDef.kingMpAmount` — used **instead of** the
  normal amounts when the fight is the King's. Bespoke King fields follow the
  established M40 precedent (`BossDef.immuneToConfusion`).
- `Battle.kingBattle` is set in `buildBattle` when `team.bossId == kKingBossId`,
  so the sim and the live screen see the same flag from the same construction
  path. No wall-clock or unseeded input is involved.

Royal Snacks is authored `minTown: 1, maxTown: 1` — sold only in town 1, and
(consistently) only reachable from town-1 dungeon merchants/chests. The player
must go back to where the game started before facing the King.

### D9 — Version bumps and bookkeeping

- `battle::kBattleRulesVersion` 3 → **4** (confusion enforcement, Renew revive,
  snacks, item targeting all change how a battle resolves for identical inputs).
- `dungeon::kGenerationVersion` 8 → **9** (merchant prices derive from the
  repriced values; the town-1 item pools gain Royal Snacks).
- `docs/technical_design.md` gains the sim-fidelity rule (every forced/automatic
  action rule lives in shared `battle::` code).

## E. Determinism & compatibility

- No new randomness of any kind. `confusedChoice` and the target filters are
  pure; the existing seeded confusion redirect in `attack()` is unchanged; the
  King flag is derived from content, not rolled.
- Saves: no schema change, `kSaveVersion` stays 1. Old saves load unchanged; a
  party holding Royal Snacks round-trips through the existing inventory field.
- Scoreboard: existing entries keep their recorded `battleRulesVersion` /
  `generationVersion` and are flagged as older-rules by the existing M19
  comparability display. Nothing is normalized or discarded.
- Content schema: three optional `ItemDef` fields and one optional `SkillDef`
  field, all defaulting to the pre-M43 behavior; `data/*.json` version stays 1.

## F. Out of scope

The relic event and the four relic items, and the King's re-stat (**M44**);
the three unlockable classes (**M45**); anything from M23/M24.

## G. Acceptance criteria

1. A confused unit takes a basic attack in **both** the live screen and the
   Simulator, for both sides, proven by a sim = live test.
2. No defeat message claims a gold loss that does not happen; the party leaves a
   lost castle challenge fully healed.
3. The boss-rush defeat line counts the real roster.
4. Repriced economy passes the affordability battery.
5. Renew revives a KO'd ally and heals a living one; the loader rejects a
   mis-authored `reviveHpPct`.
6. Heal/MP/cure items cannot target the KO'd; revive items target only the KO'd.
7. Royal Snacks behaves per context, and is buyable only in town 1.
8. Full suite green, `--capture` overflow-clean, presentation lint green.

## H. Manual validation for the owner

Castle-defeat feel (the free full heal); whether the reprices sting
appropriately; snack usefulness in and out of the King fight; whether Renew at
12 power / 9 MP / 20 % revive is the right emergency button; Purify as a pure
cure; the Clarity nerf.

## I. Required documentation updates on completion

`docs/game_design.md` (prices, item/skill rules, castle defeat, town-1 snacks),
`docs/technical_design.md` (shared forced-action rule, new schema fields,
version bumps), `docs/manual_test_matrix.md`, `docs/milestones.md`, this note's
as-implemented record, and the completion report per
`docs/milestone_completion_template.md`.

## J. As-implemented record (2026-07-22)

Implemented against `9dff867` (working tree otherwise clean). **387/387 tests**
green in Debug and Release (368 baseline + 19 new), `--capture` **44/44 scenes
overflow-clean**, presentation lint green (it runs inside the suite), Debug and
Release both build with no project warnings.

**Shared code (the milestone's structural point)**

- `battle::confusedChoice(b, actor)` (Battle.hpp/cpp) is the single forced-action
  rule. `chooseEnemyAction` returns it first; the Simulator's `choosePartyAction`
  returns it first; `BattleState::executeConfused` takes its nominal target from
  it. `Simulator.hpp` now exports `choosePartyAction` so a test can assert
  sim = live directly.
- `battle::itemTargets` / `battle::skillAllyTargets` are the pure target rules the
  battle screen calls.

**Data**

- `items.json`: Remedy 15→100, Ether 60→150, Hi-Ether 150→500, Phoenix Tear
  150→300 with revive 50→25 % (description updated), **Royal Snacks** added
  (250 g, `minTown`/`maxTown` 1, heal 10, `curesDebuffs`, `kingEffectAmount` 100,
  `kingMpAmount` 10, "Bring Snacks!").
- `skills.json`: `purify` power 12→0; `renew` power 26→12, mpCost 7→9,
  `reviveHpPct` 20, description rewritten.
- `passives.json`: `clarity` magnitude 3→2.

**Deviations from the plan / note**

1. **An unusable item is greyed with its reason instead of refused after
   selection.** The plan said "refuse the item"; the item list had no disabled
   state, so a refusal message would have been drawn in the column the item
   description owns and would have been easy to miss. Greying the row and
   printing the reason beside it matches the existing silenced-skill ("SIL")
   convention and M22's non-color-alone rule. Same outcome — the item is never
   spent — with better legibility. Routine local decision.
2. **`ItemDef::maxTown` was implemented as a general availability window
   (`availableAtTown`) applied everywhere `minTown` already was** — item shop,
   equip shop, and the generator's chest/merchant pools — rather than as an
   item-shop-only gate. One predicate means Royal Snacks cannot be town-1-only in
   the shop and worldwide in a chest. Consequence: in **town 1** a dungeon
   merchant or chest may also offer Royal Snacks; from town 2 on they are
   unavailable everywhere. Covered by the generation bump. Owner can veto by
   restricting the window to shops.
3. **`renew` mpCost 7 → 9** in addition to the planned power drop, so the
   revive-capable heal is not also the cheapest one. Reversible data tuning,
   flagged for owner validation (§H).
4. `battle/Battle.cpp` now includes `game/Castle.hpp` for `kKingBossId`. It
   already included `game/Party.hpp` and `dungeon/DungeonModel.hpp`, so no new
   layering direction was introduced.

**Evidence**

- Affordability battery (new `[economy-report]` row, one clear's gold ÷ price):

  | depth | gold | Remedy 100 | Ether 150 | Hi-Ether 500 | Tear 300 | Snacks 250 |
  |---|---|---|---|---|---|---|
  | 1 | 437 | 4 | 2 | 0 | 1 | 1 |
  | 3 | 662 | 6 | 4 | 1 | 2 | 2 |
  | 5 | 847 | 8 | 5 | 1 | 2 | 3 |
  | 8 | 1848 | 18 | 12 | 3 | 6 | 7 |
  | 10 | 2070 | 20 | 13 | 4 | 6 | 8 |

  Potions (25 g) are untouched, so HP stays cheap while MP, cures and
  resurrection are decisions. Hi-Ether is deliberately a deep-run luxury.
- Castle battery after the Clarity nerf (`[castle-report]`): King cleared in 9
  rounds with 3 alive at 44 % HP; Boss Rush 12/12 in 36 rounds; Endless best wave
  10. Nothing became unclearable.
- Determinism: no new randomness. The only behavioral rolls remain the existing
  seeded confusion redirect and to-hit draws; `confusedChoice` and both target
  filters are pure `const` queries, and `kingBattle` is derived from the team's
  boss id.

**Known items for owner validation**

Castle-defeat feel (the free full heal); the reprices; Renew at 12 / 9 MP / 20 %;
snack usefulness; the Clarity nerf; whether Royal Snacks appearing from a **town-1**
merchant/chest (deviation 2) is acceptable.
