# M58 — Fixes: equip message, Deadly Spoon once, geese scare the King

> Owner-directed fixes, authorized 2026-07-24. Runs after M57 and before the
> M23/M24 release track. Three items: one UI bug (item 1), one item bug (item 2),
> one new King-fight mechanic (item 3). **Battle rules `10 → 11`** — the only
> version change; save (1), generation (11), settings (1), achievements (1) all
> unchanged.
>
> Authority: `CLAUDE.md` > this note > `docs/milestones.md` >
> `docs/game_design.md` > `docs/technical_design.md`. §J is the as-implemented
> record.

## A. Base and baseline

- **Base checkout:** `3181aa8` + the M53–M57 working tree (uncommitted at
  authorization). Verified clean-of-conflicts against the current files before
  editing.
- **Baseline:** 530/530 Debug, 526/526 Release, `--capture` 75/75 clean (post-M57).
- **Versions:** rules **10 → 11** (items 2 & 3 change King-fight outcomes);
  everything else unchanged.

## B. ⚠ Escalation and owner decisions

Items 2 and 3 change what a King fight produces for a given seed, which the
contract treats as a combat-rule change requiring a `kBattleRulesVersion` bump —
both owner-gated. The clarifying questions (rules bump / per-Goose odds / scare
scope) were put to the owner and **dismissed without an answer**. Because the
recommended options were the conservative, literal reading of the request and the
milestone stops at `implemented, awaiting manual approval`, the recommended
defaults were implemented and are flagged for approval:

| Decision | Implemented default | Alternative if owner prefers |
|---|---|---|
| Rules version | **bump 10 → 11** | keep 10 (breaks scoreboard comparability — not advised) |
| Per-Goose odds | **additive 10 % × living Geese** (4 Geese ⇒ 40 %) | diminishing `1−0.9^N`; or flat 10 % if any Goose |
| Which Geese | **living Geese only** | include KO'd Geese |
| What is skipped | **the King's own action only** (court still acts) | the whole enemy team skips |
| Cadence | **re-rolled each King turn** | once per battle |
| Wording | *"The geese scare the King…"* (grammar-polished) | verbatim *"The geese scares the king…"* |

Each is a one-line change. Nothing was retuned to compensate for the new
counterplay — whether the King needs rebalancing against a Goose party is an
owner call at approval (the `[king-report]` battery is the place to judge it).

## C. Item 1 — the equip refusal message lingered (UI only)

**Symptom.** In the Equipment Shop, trying to equip a slot a class is barred from
(e.g. a Dragon into Armor) raised *"A Dragon cannot equip Armor."* — and it stayed
on screen forever, across characters and phases.

**Cause.** `EquipShopState::message_` was set on the refusal (and on buy
feedback) and rendered whenever non-empty, but never cleared on navigation.

**Fix.** Clear `message_` (and `messageIsError_`) at the top of `rebuild()`, which
runs on every phase change (Confirm forward, Cancel back, a completed equip). The
message now fades the moment you leave that character's / list's menu. It never
wipes a just-set message: every path that sets `message_` `break`s **without**
calling `rebuild` in the same step (verified). No rules/version/schema impact.

## D. Item 2 — Deadly Spoon stacked (battle behaviour)

**Symptom.** The Deadly Spoon relic (`statScalePct: 50`) halves a foe's
ATK/MAG/DEF/SPD "for the rest of the battle", but using it again on the same foe
halved again (→ a quarter, an eighth, …). The live path applies it to one
selected foe per use (`itemTargets` lists all foes, the player picks one), so the
stacking came from repeated **uses**.

**Fix.** A new battle-only `Combatant::statDiminished` flag. In `Battle::useItem`,
the stat-scale block applies only when the target is not already diminished, then
sets the flag; a second spoon appends *"… is already diminished."* and changes no
stats. Idempotent per foe for the whole battle. Battle-only state (never
persisted). Changes battle outcomes ⇒ covered by the rules bump.

## E. Item 3 — geese scare the King (new combat rule)

**Rule.** Each of the Hollow King's own turns, if no status already took his turn,
he has a **10 % × (living Goose-class party members)** chance to do nothing. Only
his own action is skipped — his court/minions act normally.

**Where.** Decided in the shared, seeded battle model so the Simulator, the enemy
AI, and live play agree (the gotcha-13 rule):
- `battle::geeseScaringKing(b)` — count of living `sourceId == "goose"` party
  units in a King fight (0 otherwise).
- `battle::kingScaredThisTurn(b, actor)` — true when `actor` is the boss of a King
  fight and `targetJitter(rngSeed ^ kSaltGooseScare, turnsTaken, actor, …) % 100 <
  10 × geese`. A **pure hash** (its own salt, never advances `rollCursor`) so it is
  identical in the Simulator and on screen — but, UNLIKE a Jester quip, it feeds
  `chooseEnemyAction`, which returns a `ForcedAction::Skip` when it fires, so it
  **does** change how a King fight resolves (⇒ the rules bump).

**Presentation.** `BattleState::executeEnemy`, on a `Skip`, distinguishes the
scare from the Tax-Sheets "buried in paperwork" skip with the exact condition
`chooseEnemyAction` used (`forcedActionFor(self) == None && kingScaredThisTurn`),
so the flavour is right even if the King is also Stunned. It reuses the Jester
quip channel — a gold line above the panel via `jestLine_`/`jestTimer_` — showing
*"The geese scare the King…"*, while the resolve line reads *"<King> loses its
turn!"*. Matches the requested "similar manner to the Jester's dry jokes".

## F. Tests

- `tests/test_royal_relics.cpp` (+2 cases):
  - *Deadly Spoon diminishes a foe only once* — halves once, a second use leaves
    the stats unchanged and reports "already diminished".
  - *geese scare the King* — `geeseScaringKing` counts living Geese only and is 0
    outside a King fight; no geese ⇒ never scared; the scare rate over 5 000 turns
    is 10 % × Geese ± 5 % for 1/2/4 Geese and is deterministic; `chooseEnemyAction`
    returns `Skip` **exactly** when `kingScaredThisTurn` is true (sim == live).
- `tests/test_comforts.cpp` — the exact `kBattleRulesVersion == 10` pin relaxed to
  `>= 10` (M52 drove it to 10; M58 to 11); the M58 test file is the current
  exact-version anchor via the rate/wiring cases.
- Item 1 is UI state (no headless seam) — covered by the manual matrix.

## G. Documents updated

`docs/game_design.md` (the geese-scare King mechanic; the spoon "once"),
`docs/technical_design.md` (the shared seeded scare rule + the rules bump),
`docs/manual_test_matrix.md` (three M58 rows), `docs/milestones.md`, this note.

## J. As-implemented record

Implemented 2026-07-24 on base `3181aa8` + M53–M57. **Battle rules 10 → 11**; save
1 / generation 11 / settings 1 / achievements 1 unchanged.

**Files changed**
- **Source:** `src/battle/Battle.hpp` (rules 11; `Combatant::statDiminished`;
  `geeseScaringKing`/`kingScaredThisTurn` decls), `src/battle/Battle.cpp` (spoon
  idempotency; scare salt/const/helpers; the scare gate in `chooseEnemyAction`),
  `src/states/BattleState.cpp` (the geese-scare flavour line),
  `src/states/EquipShopState.cpp` (clear the message on `rebuild`).
- **Tests:** `tests/test_royal_relics.cpp` (+2), `tests/test_comforts.cpp` (version
  pin relaxed).
- **Docs:** this note + the four listed in §G.
- **No** data/JSON change (the Deadly Spoon's description "for the rest of the
  battle" is still accurate); **no** CMake change; **no** capture-scene change.

**Validation (this session, VS2022 dev shell)**
- Build debug + release — OK, **no project warnings**.
- `ctest --preset debug` → **532/532** (530 + 2). `ctest --preset release` →
  **528/528** (526 + 2). `[relics],[king],[comforts]` alone: 33 cases, 20 996
  assertions.
- `CrystalDungeons.exe --capture` → **75/75 clean**.

**Deviations from the request:** none in substance. The display wording is lightly
grammar-corrected (*"The geese scare the King…"*), and the three owner-gated
choices use the recommended defaults after the clarifying questions were dismissed
(see §B) — all trivially reversible before approval.

**Known limitations / owner-validation needs**
- Whether N = 10 %/Goose and King-action-only feel right in live play, and whether
  the King needs rebalancing against a Goose party, are owner sign-off items.
- Item 1's fix and item 3's on-screen presentation are runtime UI the headless
  suite can't assert — matrix rows cover them.
