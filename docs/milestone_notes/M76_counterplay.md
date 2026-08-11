# M76 — Counterplay: breaker skills, Holy Taxes & the Evil Duckling

Authorized 2026-08-05 as part of the M75–M86 expansion program (see the
program section in `docs/milestones.md`). Implemented 2026-08-05 on the
post-M75 checkout (base commit `2538a92`, M75 approved the same day).

## A. Status

**☑ complete (approved)** — implemented 2026-08-05; approved and
committed by the owner 2026-08-05 (`e73fed6`). Evidence in §F.

## B. Goal (owner brief)

Before enemies start using the M75 statuses (M77), the party gets its
counterplay: Ranger and Rogue can break Reflect, the Cleric (and a new item)
can lift Curse, the Knight gains Smite, and a new rare dungeon event sells
the party a cursed duck of its own.

## C. As implemented

Content on the v15 engine — **no version motion of any kind** (battle rules
stay 15, generation stays 14, save/settings schemas untouched).
`docs/technical_design.md` §30 carries the architecture record;
`docs/game_design.md` §10 ("Counterplay (M76)") the player-facing rules.

### Skills & learnsets

- **Mirrorbreak** (`mirrorbreak`) — physical single-enemy, power 10, MP 4,
  `control: break_reflect`. One shared skill id in **both** learnsets:
  **Rogue 11**, **Ranger 12** (the plan's ~L9–12 window). Physical by
  loader rule — a magic breaker would bounce off the very mirror it came
  to break.
- **Absolve** (`absolve`) — support single-ally, MP 6, `control: uncurse`.
  **Cleric 12** (between Purify 10 and Radiance 13).
- **Smite** added to the **Knight's learnset at 13** (the owner's "Holy
  Elemental" access; the skill itself is unchanged — the Cleric keeps it
  at 8).
- **Shadow Strike** gains `element: dark` — the element's one player-side
  exception (owner decision 2026-08-05; Dark *weapons* stay reserved). A
  `[counterplay]` test proves **no foe anywhere is Dark-immune**, so the
  Rogue's always-known opener honours the M48 never-a-trap rule. The Dark
  weaknesses arrive with M77.

### Items

- **Holy Taxes** (`holy_taxes`) — consumable, 200g, `minTown: 3`, heal 10
  + the M75 `curesCurse` flag. Stocking needed **zero shop code**: the
  M43 town-window machinery picks it up in item shops from town 3 and in
  the dungeon merchant/chest pools from there.
- **Evil Duckling** (`evil_duckling`) — consumable, `battleTarget: enemy`,
  a 2-authored-turn **Curse** rider (×2 duration mult ×1.5 curse rule =
  6 bearer-turns), and **value 0** — the M44 value-gate keeps it out of
  every shop, chest pool and merchant roll, so the peddler is its only
  source. Consumed on use; re-obtainable from a later peddler.
- **`ItemDef.useLine`** (new optional field, loader + editor descriptor):
  a one-liner delivered on the Jester-quip channel when the item is
  actually spent — the duckling's **Hilarious Punchline**: *"What do you
  call a duck that steals? A robber ducky."* Presentation-only; the
  battle model never reads it.

### The Duckling Peddler

- New `RoomEventKind::DuckPeddler`. A post-pass in `dungeon::generate`
  replaces **one plain rolled event** (Shrine / Spring / Merchant / Wager
  / Rest — never a theme rite, the Royal Relic, or an elite challenge)
  via `duckPeddlerSlot(seed, eligibleCount)` — a **pure `themeEventHash`
  under two fresh salts** that consumes no rng draw, so every other roll
  of a seed is byte-identical. Constants: **10%** of dungeons,
  **300 gold** flat, `kEvilDucklingItemId`.
- **One per customer** (the owner's rule), enforced at **interaction
  time**: while the party owns a duckling both the footer prompt and the
  resolution decline ("One per customer. Duck rules.") and the event
  stays **unresolved**, so the offer stands for a duckless return — and
  what a seed *generates* never depends on the party's bag (determinism).
- Minimap/room marker: a sickly-green **"D"** glyph (no new sprite, so no
  manifest motion). The event uses today's footer-prompt presentation;
  M80's centered flavor text will dress it.

## D. Files changed

- **Content model:** `src/content/Definitions.hpp` (`ItemDef.useLine`),
  `src/content/ContentLoader.cpp` (read it).
- **Dungeon:** `src/dungeon/DungeonModel.hpp` (the event kind),
  `src/dungeon/ThemeEvents.hpp/.cpp` (`kEvilDucklingItemId`,
  `kDuckPeddlerChancePct`, `kDuckPeddlerPriceGold`, `duckPeddlerSlot`),
  `src/dungeon/DungeonGenerator.cpp` (the pure-hash post-pass + the
  rolled-kind switch case).
- **States:** `src/states/DungeonState.cpp` (prompt, resolution, marker),
  `src/states/BattleState.cpp` (the use-line quip on spend).
- **Editor:** `src/editor/CategoryDescriptors.cpp` (`useLine`).
- **Data:** `data/skills.json` (+2, Shadow Strike element),
  `data/classes.json` (4 learnset entries), `data/items.json` (+2).
- **Tests:** `tests/test_counterplay.cpp` (new, 8 cases, `[counterplay]`),
  `tests/CMakeLists.txt`, `tests/test_content_loader.cpp` (count pins
  63→65 skills, 80→82 items), `tests/test_events.cpp` (the event-kind
  sweep now meets 8 kinds — honest premise update).
- **Docs:** this note, `docs/milestones.md`, `docs/game_design.md` §10
  (+ the M48 Dark paragraph), `docs/technical_design.md` §30,
  `docs/manual_test_matrix.md` (M76 header note).

## E. Plan deviations

- **The punchline is one authored line, not a seeded pool.** The planning
  note floated "seeded pick from several authored lines"; the shipped
  shape is a single `useLine` (the `doNothingText` precedent — "Quack."
  is one line and it is the Duck's whole comedy). A pool would need a
  new string-list editor field kind for one joke; trivially extendable
  later if the owner wants variety.
- **Generation stays v14 — recorded for the owner's veto.** Precedent
  cuts both ways: M65/M66 bumped generation for pure-hash additions,
  M52's high-stakes market did not. The approved M76 note promised "no
  generation motion" and the program reserves its one bump for M82, so
  the peddler ships bump-free: it consumes no rng draw, every other roll
  of a seed is byte-identical, and its score impact is a voluntary 300g
  spend. If the owner prefers the M65 reading (any content a seed
  produces tags comparability), a 14→15 bump can be added in review.
- **Holy Taxes heals 10 HP alongside the uncurse** (the Royal Snacks
  shape) so its use never logs "Nothing happens" before the lift — a
  presentation nicety, not a balance lever.
- No other deviations.

## F. Automated validation (all run in this session, 2026-08-05)

- Debug build: clean, zero project-code warnings.
- **Full Debug suite: 640/640 tests green** (the 8 new `[counterplay]`
  cases and 1 case the TRF/STN chip task added — see below).
- **Capture lint: 85/85 scenes clean.** The set grew to 85 between M75
  and M76: the owner ran the spawned "TRF/STN Details legend" background
  task and folded it into the M75 commit — it added the two legend
  entries and capture scene `85_battle_details` (the fullest Details
  overlay, overflow-checking the wrapped legend). Verified coherent with
  this milestone's RFL/SLP/CRS entries in the same string.
- **Release build + suite: 636/636 tests green**, zero warnings (the
  4-case gap to Debug is the debug-only god-mode battery, as always).
- Closing verification: **640/640 Debug and 636/636 Release tests green;
  `--capture` 85/85 scenes clean; zero project-code warnings.**
- First-pass honesty note: the initial run failed 4 tests — two were a
  bug in the new `[counterplay]` file itself (a nonexistent `goblin` foe
  id built a one-unit battle and indexing `units[1]` crashed — fixed
  with the shipped `goblin_grunt` and a roster REQUIRE), one was the
  duckling's description overflowing the 2-line detail lint (shortened),
  and one the event-kind sweep's 7-kind pin (now 8, honestly updated).

## G. Manual owner checklist

1. Debug build. Training Hall / level a Rogue to 11 (or debug-level): the
   skill list gains **Mirrorbreak**; a Ranger at 12 likewise; a Cleric at
   12 gains **Absolve**; a Knight at 13 gains **Smite**.
2. In battle, the Rogue's Shadow Strike now logs **"(Dark)"**.
3. In a town-3+ item shop: **Holy Taxes** on the shelf at 200g; buy one.
4. Roam dungeons until the **"D"** marker appears (~1 dungeon in 10; any
   town, any theme): the peddler asks **300g** for the **Evil Duckling**;
   buy it, walk back in — he now refuses ("One per customer. Duck
   rules."); the refusal reads well.
5. Use the duckling in a battle: the foe's **CRS** chip appears, its
   hits halve, and the **punchline** rides the gold quip line — judge
   the joke.
6. Debug-poke a Curse onto a party member (or wait for M77): **Absolve**
   and **Holy Taxes** lift it; **Purify** and a **Remedy** must NOT.
7. On failure: the battle log text, a save, and what you fought.

## H. Known limitations

- Reflect still has no shipped enemy source (M77) — Mirrorbreak is
  learnable now but has nothing to break until then (deliberate:
  counterplay ships before the threat).
- The peddler speaks through the footer prompt until M80's centered
  event flavor arrives.
- The punchline is a single line (see §E) — extendable to a pool on
  request.

## I. Final status

`complete (approved)` — owner approval 2026-08-05.
