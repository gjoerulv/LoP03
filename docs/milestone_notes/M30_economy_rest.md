# M30 — Economy: Paid Rest & Rest-Token Event

## A. Status and authority

- **Status:** complete (approved) — approved by the owner 2026-07-20 after
  manual testing. Authored / audited 2026-07-20 against the then-current
  checkout (HEAD `2eaff28` "M29"). §D
  decisions approved (`I agree 100%`, 2026-07-20); as-implemented record in §K.
- **Authority:** `CLAUDE.md` > this note > `docs/milestones.md` >
  `docs/game_design.md` > `docs/technical_design.md`.
- **Authorization:** the owner authorized beginning M30 on 2026-07-20 after
  approving M29. This is the **final milestone** of the M25–M30 polish program.
  Dependencies satisfied: M28 (battle outcomes → income) and M29 (content →
  income) are `complete (approved)`, so cost tuning is against a settled economy.

## B. Problem statement (verified at audit)

- **The inn is free.** `InnState::onEnter` calls `healFull(context_.party)` and
  plays a chime; any key pops ([InnState.cpp:17](src/states/InnState.cpp:17)).
  There is no cost, no confirmation, no decision — full HP/MP restoration is a
  formality, so gold has no recovery sink and attrition never bites.
- **Room events are a proven, extensible system (M20).**
  `RoomEventKind { None, Shrine, HealingSpring, Merchant, EliteChallenge,
  ScoreWager }` ([DungeonModel.hpp:18](src/dungeon/DungeonModel.hpp:18)); the
  generator shuffles the five real kinds and places 2–3 per dungeon
  ([DungeonGenerator.cpp:406](src/dungeon/DungeonGenerator.cpp:406)); each is
  previewed with its full trade-off and resolved in `DungeonState::resolveEvent`
  with a confirm ([DungeonState.cpp:297](src/states/DungeonState.cpp:297)).
- **Save carries optional party state without version bumps.** `Party { members,
  inventory, gold }` ([Party.hpp:17](src/game/Party.hpp:17)); `kSaveVersion` = 1
  and prior milestones added `inventory`/equipment as **optional** fields read
  defensively, never bumping it ([SaveSystem.hpp:18](src/save/SaveSystem.hpp:18)).

## C. Goals

- Make resting a **real economic decision**: a gold cost that stays meaningful as
  income grows, shown clearly, with a confirm; declining is **never a soft-lock**.
- Reward exploration with **relief** from that cost: a new room event granting a
  **one-time free-rest token** that persists across save/load and grants exactly
  one free full rest.
- **Simulation-backed** evidence that the cost is affordable-but-meaningful
  across depths, and that a broke, wounded party can always recover.
- No existing save breaks; deterministic battle behaviour untouched.

## D. Material design decisions (owner input required)

Each fork has a recommendation. `I agree 100%` accepts all recommendations.

### D.1 — Inn cost scaling rule → *Recommend: scale with highest party level*

`restCost(party) = kBase + kPerLevel * (highestLevel - 1)`, clamped to
`[kBase, kMax]`. **Provisional constants `kBase = 20`, `kPerLevel = 12`,
`kMax = 500`** (level 1 → 20g, level 10 → 128g, level 20 → 248g, cap 500g),
**final values tuned against the affordability sim** within this form. Rationale:
income scales with depth/level (the M29 economy report shows per-run gold ~440
at depth 1 up to ~1900 at depth 10; a training level costs 280–1240), so a
level-tied rest cost stays a live trade-off at every stage instead of going
trivial. Full HP **and** MP restored, as today.

- *Alternatives:* **flat** (simple, but trivial once income climbs); **depth-
  scaled** (the inn is in town with no active depth — awkward to source);
  **missing-HP-scaled** (punishes hard-won attrition and is unreadable). The
  level form is the most legible "the richer you are, the more a full rest
  costs."

### D.2 — Rest-token storage → *Recommend: a dedicated `Party.restTokens` counter*

A new `int restTokens = 0` on `Party` beside `gold`, saved as an **optional**
field (`optIntMin("restTokens", 0, 0)`) — **no `kSaveVersion` bump, old saves
load with zero**, exactly the pattern inventory/equipment already use.

- *Alternative (an inventory item `rest_token`):* reuses the inventory save path
  but needs an `ItemDef`, and a non-battle, non-shop item risks leaking into the
  battle item menu and the item shop unless specially filtered everywhere. A
  plain counter is cleaner, cannot leak, and the inn is the only consumer.

### D.3 — Free-rest event + generation version → *Recommend: new `RoomEventKind::RestToken`; bump `kGenerationVersion` 5 → 6*

Add `RestToken` to `RoomEventKind` and to the generator's event pool; resolving
it (a simple confirm — "Rest here and pocket a free-rest token for town?") grants
one token, persists via D.2, and is previewed with its full effect like every
other event. Because adding a kind changes the shuffle/placement RNG for **every
seed**, `dungeon::kGenerationVersion` must bump **5 → 6** (owner-gated per the
comparability policy; the scoreboard tags it, so pre-M30 seeds stay comparable).

- *Consequence:* the token is redeemed at the inn (D.4) for one free full rest.
  It is not consumed in the dungeon; it survives to town and across save/load.

### D.4 — Inn flow & soft-lock guard → *Recommend: a small inn menu*

Replace the auto-heal with a menu: **"Rest (N gold)"**, **"Use free-rest token
(k held)"** (shown only when `restTokens > 0`), **"Leave"**. Resting deducts gold
(or one token), restores full HP/MP, plays the chime. The cost and token count
are always on screen. **Declining is never a soft-lock:** HP/MP persist, and a
wounded, broke party can still enter an easy dungeon and win gold from battles
(victory grants gold even without resting), then afford a rest — so recovery is
always reachable. If gold is insufficient and no token is held, "Rest" is
disabled with a clear reason, never a dead end.

## E. Proposed design (assuming the §D recommendations)

- **`restCost`** — a pure free function (headless-testable) implementing D.1,
  reading `highestLevel(party)`; constants in one place for sim tuning.
- **`InnState`** — menu-driven (reuse `ui::Menu`); Confirm on "Rest" spends gold
  or a token and heals; on insufficient funds the row is disabled. No auto-heal
  in `onEnter`.
- **Token** — `Party.restTokens` (D.2); `SaveSystem` writes/reads it optionally;
  a small helper to grant/spend. `RoomEventKind::RestToken` generated (D.3) and
  resolved in `DungeonState` by incrementing `restTokens` with a confirm+preview.
- **Balance sim** — extend the `[economy]` battery with an affordability check:
  across depths, per-run income comfortably exceeds a rest at the run's clearing
  level (so rest is affordable), yet the cost is a non-trivial fraction of a
  potion/gear purchase (so it is a real decision). Assert a broke L-party can
  recover. Tune `kBase/kPerLevel/kMax` (data-like constants) if needed.

## F. Determinism & save compatibility

- Battle resolution is **unchanged** — this milestone touches town economy and
  dungeon event generation only. No `battleRulesVersion` change.
- `kGenerationVersion` **5 → 6** (D.3), the one deterministic-seed change, tagged
  on new score entries exactly as prior bumps were.
- **No `kSaveVersion` bump** — `restTokens` is an optional field; old saves load
  with zero tokens; new saves round-trip. A save-compat test covers both.

## G. Out of scope

Wider shop or reward-curve rebalancing (only the inn cost + token are added);
new mechanics beyond the token; any battle-rule or content change; a general
"quest item" system (the token is a single typed counter, not a framework).

## H. Balance

Acceptance: a rest at the clearing level of each depth is affordable from that
depth's typical income but is a meaningful fraction of a shopping trip; declining
never soft-locks; the token grants exactly one free rest and cannot be duplicated
or banked into infinite rests (one token = one rest, decremented atomically).

## I. Manual validation for the owner

Whether the cost changes decision-making without becoming a grind; the event's
discovery rate feels rewarding; the inn menu reads clearly; a broke, wounded
party never feels stuck.

## J. Required documentation updates on completion

This note (as-implemented + deviations); `docs/milestones.md` (status);
`docs/game_design.md` (§5 town — paid inn; §6 room events — rest-token; §12
progression — recovery economy); `docs/technical_design.md` (`restCost`,
`Party.restTokens` save field, `RoomEventKind::RestToken`, gen-version 6);
balance-report expectations; `docs/manual_test_matrix.md` rows.

## K. As-implemented record (2026-07-20)

- **Status:** complete (approved 2026-07-20). Owner approved all §D
  recommendations (`I agree 100%`, 2026-07-20).
- **Cost model.** `restCost(party)` (pure, in `game/Party`) =
  `kRestCostBase(20) + kRestCostPerLevel(12)*(highestLevel-1)`, clamped to
  `[20, kRestCostMax(500)]`. Constants live in `Party.hpp` for tuning; the
  affordability sim (below) validated them, so no change was needed.
- **Token.** `Party.restTokens` (int beside `gold`), saved as an **optional**
  field (`SaveSystem` write + `optIntMin("restTokens", 0, 0)` read) — **no
  `kSaveVersion` bump; old saves load with 0**.
- **Inn.** `InnState` rewritten as a menu (no auto-heal in `onEnter`): row 0 is
  the paid rest — disabled with a reason when unaffordable **or** already fully
  rested (so gold is never wasted); a token row appears only when tokens are
  held. Confirm spends gold or one token and `healFull`s; Cancel leaves. Gold,
  tokens, and the party roster are always shown.
- **Event.** `RoomEventKind::RestToken` added to the enum and the generator's
  event pool (now 6 kinds shuffled, 2–3 per dungeon); resolving it (a simple
  previewed confirm) grants one token. Its dungeon marker is a distinct
  campfire sprite `prop.event.rest` (glyph "R"), generated by
  `generate_textures.ps1` + manifest + `credits.md`, and added to the
  presentation-lint required-id list. `dungeon::kGenerationVersion` **5 → 6**
  (owner-approved).
- **Validation (this session):** full suite **279/279** (`crystal_tests.exe`,
  30502 assertions), incl. 4 new `[rest]` tests (cost formula, highest-level
  drives it, clamp/underflow, monotonicity), a `restTokens` save round-trip +
  backward-compat, the `[events]` guard updated to require all 6 kinds incl.
  RestToken, and a new `[economy]` **affordability** test proving a single clear
  at each depth's clearing level funds ≥3 full rests (so a broke party always
  recovers — no soft-lock), the cost stays ≥ base, and a rest is cheaper than a
  party training level. `--capture` **23/23** overflow-clean (the inn scene now
  holds a token to exercise both rows). Debug **and** Release build clean.
  Generator reruns **byte-identical** (only `event_rest.png` added).

### Deviations from the plan / note

1. **"Leave" is Cancel/Back, not a menu row** (the house idiom used by every
   other town service, e.g. `TrainingHallState`), keeping the menu to the rest
   options. Still never a soft-lock.
2. **The paid-rest row is disabled when the party is already fully rested**
   (beyond the §D.4 "disabled when unaffordable"), so a player cannot waste gold
   healing at full — a small UX safeguard, not a scope change.
