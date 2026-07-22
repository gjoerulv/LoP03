# M44 — Royal Relics & the doubled King

## A. Status and authority

- **Status:** ◑ implemented, awaiting manual approval (2026-07-22) — the second
  milestone of the **King's Gambit program**. See §J for the as-implemented
  record and the deviations, including the owner's mid-implementation decision to
  take `kKingScalePct` from 420 % to 340 %. M45 is **not** authorized by this.
- **Authority:** `CLAUDE.md` → this note → `docs/milestones.md` →
  `docs/game_design.md` → `docs/technical_design.md`. M45 is direction only.
- **Base checkout:** `65c9a47` ("M43"), working tree clean at authorization.
- **Baseline:** 387/387 tests, `--capture` 44/44 scenes overflow-clean.
- **Terminal state for this session:** `implemented, awaiting manual approval`.

## B. Re-audit against `65c9a47`

| Plan claim | Verified |
|---|---|
| Event placement is a shuffled unique-kind list | `std::array<RoomEventKind, 6>` shuffled per dungeon, 2–3 events placed on main-path side rooms ([DungeonGenerator.cpp:428](../../src/dungeon/DungeonGenerator.cpp:428)) |
| Events resolve at interaction time | `DungeonState::resolveEvent` / `eventPromptText` / the Event marker's sprite switch ([DungeonState.cpp:301](../../src/states/DungeonState.cpp:301), :369, :859) |
| A seeded one-shot hash exists | `blackMarketHash(seed, salt)` ([BlackMarket.hpp:52](../../src/game/BlackMarket.hpp:52)); `Dungeon.seed` is on the model ([DungeonModel.hpp:83](../../src/dungeon/DungeonModel.hpp:83)) |
| The King's current stats | `the_hollow_king` hp **560**, atk 18 / mag 22 / def 18 / spd 13 ([bosses.json:165](../../data/bosses.json:165)); fought at `kKingScalePct` **420 %** ([Castle.hpp:47](../../src/game/Castle.hpp:47)) — so its *effective* stats are 4.2× these |
| Battle items are ally-only | `battle::itemTargets` filters by effect within one side (M43) ([Battle.cpp](../../src/battle/Battle.cpp)); `BattleState::onItemChosen` passes `Side::Party` |
| The shared forced-action rule exists | `battle::confusedChoice` used by `BattleState`, `chooseEnemyAction`, and the Simulator's `choosePartyAction` (M43) |
| Statuses | `StatusType` = None/Poison/ATK±/DEF±/Confusion/Silence/Blind; `addStatus` scales every authored duration by `kStatusDurationMult` (2×); `tickStatuses` decrements at the **start** of the bearer's turn, before it acts |
| Versions | `kBattleRulesVersion` **4**, `kGenerationVersion` **9**, `kSaveVersion` **1** |

Two facts the plan did not state, found at re-audit and driving decisions below:

1. **Statuses tick before the bearer acts**, and `addStatus` doubles authored
   durations. A turn-control status therefore needs an exact, unscaled duration
   of **2** to cost its bearer exactly one turn (§D4).
2. **The King is fought at a 420 % stat scale**, so "double the King" means
   doubling a number that is then multiplied by 4.2. §D6 records what the sim
   says about that.

## C. Goals

A rare event that yields four unique consumable relics, and the King re-statted
to need them. Rules **4 → 5**, generation **9 → 10**. No save bump (the relics
are ordinary inventory items; the event lives in the generated dungeon).

## D. Slices

### D1 — The Royal Relic event (generation)

`RoomEventKind::RoyalRelic`. During event placement, an eligible rolled event is
**replaced** by the relic event on a seeded roll from the generator's own stream
(the same stream that already chooses events):

- eligible when **town ≥ 2 and depth ≥ 2**;
- chance **3 %** (town 2), **5 %** (towns 3–6), **7 %** (town 7), **+5 %pts when
  depth ≥ 20**;
- **at most one per dungeon** (note-time decision: the event's whole identity is
  rarity; two in one dungeon would read as a bug).

Pure `relicEventChancePct(town, depth)` so the table is unit-tested directly.

### D2 — The seeded, reload-proof grant (resolution)

On interaction, one relic is granted from
{Evil Goose 40, Tax Sheets 40, Dragon Crown 15, Deadly Spoon 5}:

- relics already **owned** (inventory count ≥ 1) are excluded and the remaining
  weights renormalized proportionally;
- **all four owned → the base 40/40/15/5 roll**, granting a duplicate (owner
  decision; `ItemStack.count` already stacks);
- the pick is `blackMarketHash(dungeon seed, room index)`, so reloading and
  re-entering the room with the same ownership reproduces it exactly.

### D3 — Battle items may target enemies

`ItemDef.battleTarget` (`ally` default / `enemy`). `battle::itemTargets` takes
the actor's side and returns living enemies for an enemy-target item; the M43
effect filter still governs ally items. The battle screen's target cursor already
handles either side.

### D4 — The four relics (schema-driven, no hardcoded item behavior)

New `ItemDef` fields, all optional and inert by default:

- `statuses`: a list of `{type, magnitude, duration}` applied to the target;
- `requiresBossId`: the item only does anything to that boss id;
- `statScalePct`: scales the target's ATK/MAG/DEF/SPD for the rest of the battle.

New `StatusType::Terrified` (forced Guard) and `StatusType::Stunned` (turn
skipped). Both are **turn-control** statuses: `addStatus` applies them at their
authored duration **unscaled** (the 2× multiplier would silently double a skipped
turn into two), and because statuses tick before their bearer acts, the relics
author duration **2** = exactly one lost turn.

| Relic | w | Effect | Text |
|---|---|---|---|
| Evil Goose | 40 | `Terrified` — target is forced to Guard on its next turn | "A terrifying goose." |
| Tax Sheets | 40 | `Stunned` — target does nothing on its next turn | "Busy your enemies with taxes." |
| Dragon Crown | 15 | ATK- and DEF- **on the King only** (`requiresBossId`); nothing on anyone else, and **not consumed** then | "The real Dragon Crown." |
| Deadly Spoon | 5 | `statScalePct` 50 — halves ATK/MAG/DEF/SPD for the rest of the fight | "Most deadly thing known to man." |

The King is **not** immune to any of them — they are the intended counterplay.

### D5 — Forced actions stay in shared `battle::` code (the M43 rule)

`battle::ForcedAction { None, BasicAttack, Guard, Skip }` +
`forcedActionFor(const Combatant&)`. `EnemyChoice` carries the forced kind, and
`chooseEnemyAction`, the Simulator's `choosePartyAction`, and `BattleState` all
consult exactly this one function — so a terrified or stunned unit behaves
identically in live play and in the sim by construction. `Simulator::applyChoice`
is exported so a scripted battery can drive real turns without re-implementing
the applier.

### D6 — The King re-statted

`the_hollow_king`: hp 560 → **750**, atk 18 → **36**, mag 22 → **44**, def 18 →
**36**, spd 13 → **26** (the plan's doubling).

**This is the milestone's real risk.** The King fights at `kKingScalePct` 420 %,
so doubling the base doubles the *effective* fight: ~2352 → ~3150 effective HP
and roughly double the incoming damage per turn. The acceptance bar is a sim in
which a **maxed party using obtainable counterplay** (Royal Snacks + the two 40 %
relics, never requiring the 5 % Spoon) wins, and the un-helped party does not.
A scripted battery (§G) measures exactly that. **If the doubled King proves
unbeatable even with the counterplay, that is an owner decision, not a silent
re-tune** — the note will report the numbers and recommend an option rather than
quietly changing `kKingScalePct` or the stats.

### D7 — Presentation & onboarding

Event prop sprite (`prop.event.relic`) from the deterministic generator + manifest
row + `assets/credits.md`; presentation lint extended; a relic-event tutorial
prompt (M20's rule: risk/reward visible before confirming — here it is pure
reward, stated plainly); capture scenes for the relic room and a relic in the
battle item menu.

## E. Determinism & compatibility

- Event placement rolls come from the generator's seeded `Rng`; the item pick
  comes from `blackMarketHash(dungeon seed, room index)`. No wall-clock, no
  unseeded RNG, nothing re-rollable by reloading.
- Saves: relics are ordinary inventory items; no `kSaveVersion` bump.
- Content schema: `ItemDef.battleTarget/statuses/requiresBossId/statScalePct` and
  two new `StatusType` values — all optional/additive.
- `kBattleRulesVersion` **4 → 5**, `kGenerationVersion` **9 → 10**.

## F. Out of scope

The three unlockable classes and the cross-save unlock (**M45**); any change to
another boss; M23/M24.

## G. Acceptance criteria

1. The replacement-chance table matches per town and depth, and never fires at
   town 1, depth 1, or twice in one dungeon.
2. Picks are deterministic, reload-proof, correctly renormalized while 1–3 are
   owned, and fall back to the base roll (duplicate allowed) when all four are.
3. Each relic effect is proven in the sim **and** on the live path, including the
   Crown's no-op (and non-consumption) against anything but the King.
4. A terrified/stunned unit loses exactly one turn, identically in sim and live.
5. The doubled King is sim-beatable with obtainable counterplay and materially
   harder without it — or the numbers are reported to the owner with options.
6. Full suite green, capture overflow-clean, presentation lint green.

## H. Manual validation for the owner

Whether the event feels like a discovery; whether the relics are dramatic in the
King fight; whether the doubled King is hard-but-fair; relic text tone.

## I. Required documentation updates on completion

`docs/game_design.md` (event odds, relic effects, King stats),
`docs/technical_design.md`, `docs/manual_test_matrix.md`, `assets/credits.md`,
`docs/milestones.md`, this note's as-implemented record, and the completion
report per `docs/milestone_completion_template.md`.

## J. As-implemented record (2026-07-22)

Implemented against `65c9a47`. **405/405 tests** green (387 baseline + 18 new),
`--capture` **46/46 scenes overflow-clean** (two new scenes), presentation lint
green, Debug and Release both build with no project warnings.

**What shipped**

- `game/Relics.hpp` — the pure table, weights, and the seeded, ownership-aware
  draw. `RoomEventKind::RoyalRelic` replaces a rolled event (at most one per
  dungeon); `DungeonState::resolveEvent` grants the relic at interaction time.
- Item schema: `battleTarget`, `statuses[]`, `requiresBossId`, `statScalePct`;
  plus the rule that a `value <= 0` item is never stocked or dropped, so the
  relics exist only through their event.
- `StatusType::Terrified` / `Stunned`, applied unscaled, plus `ForcedAction` /
  `forcedActionFor` / `forcedChoice` consulted by all three turn-deciders, and
  `applyChoice` exported so a forced turn resolves identically everywhere.
- `itemAffects` keeps an item that did nothing (the Crown off-King), with a
  "(kept)" note in the battle log.
- Presentation: `prop.event.relic` (generated, manifest + credits), the
  `first_relic` tutorial beat, capture scenes `45_relic_event` and
  `46_battle_relics`, and TRF / STN status tags.
- `the_hollow_king` re-statted to 750 / 36 / 44 / 36 / 26.

**Deviations from the plan / note**

1. **At most one relic event per dungeon** (§D1) — a note-time decision, not in
   the plan text.
2. **A valueless item is never sold or dropped.** The plan did not say how the
   relics stay out of shops, chests and dungeon merchants; this is the general
   rule chosen for it.
3. **The Crown applies ATK-/DEF- at 40 % for 4 turns** (doubled to 8 by the
   normal status scaling). The plan set no magnitude.
4. **`ItemDef.statuses` is a list**, rather than a single status field, so the
   Crown's two debuffs need no bespoke code path.

**The King's stat scale: raised as a question, decided by the owner**

The plan's acceptance bar was "beatable by a maxed party using **snacks + the two
40 % relics**, never requiring the 5 % Spoon". At the owner's doubled stats **and
the M40 420 % challenge scale**, that bar was **not met**: the doubling compounds
with the multiplier into an effective 3150 HP / 151 ATK / 184 MAG / 151 DEF /
109 SPD King, which fell only to three copies of each 40 % relic *plus* the 15 %
Crown (win in 24 rounds, one survivor at 12 % HP) — several reliquary runs of
farming, and the rare relics load-bearing.

Nothing was re-tuned unilaterally. The measured sweep (plan counterplay =
1 Tax Sheets + 1 Evil Goose + 20 snacks) was put to the owner:

| `kKingScalePct` | effective HP | no counterplay | plan counterplay |
|---|---|---|---|
| 420 (was) | 3150 | loss, 11 | loss, 18 |
| 380 | 2850 | loss, 13 | loss, 21 |
| **340 (chosen)** | 2550 | **loss, 17** | **win, 18 rounds, 2 of 4 alive** |
| 300 | 2250 | loss, 15 | win, 14 rounds, 4 alive |
| 260 | 1950 | **win, 7** | win, 11 |

**Owner decision 2026-07-22: `kKingScalePct` 420 → 340** ([Castle.hpp:47](../../src/game/Castle.hpp:47)),
with the 750 / 36 / 44 / 36 / 26 base stats untouched. Re-measured at 340 %:

| counterplay | result |
|---|---|
| nothing | **loss**, 17 rounds |
| 30 snacks only | win, 21 rounds, 2 alive at 14 % |
| 1 Sheets + 1 Goose + 20 snacks (**the bar**) | **win**, 18 rounds, 2 alive at 25 % |
| 2 + 2 + 30 snacks | win, 18 rounds, 3 alive |
| 3 + 3 + 30 snacks + Crown | win, 18 rounds, 4 alive at 64 % |
| + the 5 % Spoon | win, 10 rounds, 4 alive |

The relics now read as a difficulty dial rather than a toll: each extra copy buys
survivors, and the Spoon halves the fight. Honest caveat: **30 Royal Snacks alone
(7500 g) also wins**, barely — snacks are King-specific counterplay by design, so
this was left as a legitimate (expensive, grindy) alternative line rather than
nerfed. The acceptance test now asserts the approved bar directly.

**Known items for owner validation**

Whether 340 % feels right in play (the sim is not a player); whether the reliquary
reads as a discovery; relic drama and text tone; whether the snacks-only line
should stay open.
