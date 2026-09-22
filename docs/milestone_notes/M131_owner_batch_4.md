# M131 — Owner batch 4: the roadside Stranger, no Dragons in Iron Man, the Alarm

**Status:** implemented, awaiting manual approval
**Authorization:** owner request 2026-09-22, made in the M128–M130 session
after that program's implementation closed ("All this can be M131."). One
milestone at the owner's word; it sits on the same working tree as the
uncommitted M128–M130 work (branch `oyb13` over `5f6ff56`, v0.9.1), which
stays `implemented, awaiting manual approval` beneath it.
**Version motion:** none. Battle rules 19, generation 25, save v1, settings
v1, content v1 (`alarm` is a new value of the existing `effect` enum —
additive, the M120 `mpAmount` precedent), manifest v2, `project(VERSION)`
0.9.1.

## Scope (the owner's three points, 2026-09-22)

1. **A bug in the endgame.** THE STRANGER "P" appeared in town 7 the moment
   a **new game** reached it; P should only appear after beating the King.
   "Please examine this thoroughly, and fix any bugs related to this."
2. **Iron Man: the Dragon class is not allowed.** "This should not matter
   for games loaded from older versions of the game, but having at least
   one dragon in the party will prevent the Iron Man specific
   Achievements."
3. **A new consumable, the Alarm.** Consumable only from the item menu in
   dungeons (Pause menu → Items). Used, it triggers the next patrol
   immediately; what type of patrol follows the normal rules. Costs 200
   gold, follows the normal consumable quantity rule, only sold in town 4
   and after.

## Re-audit findings that shaped the work

- **The roadside gate read the wrong flag.** `TownState::gooseNpcHere` was
  `currentTown == 7 && profile.data.kingDefeated` — the **cross-save
  profile flag** M45 introduced for the reward classes. M97 chose it on
  purpose ("the stranger waits for anyone who knows"), so on any profile
  that had ever beaten the King a brand-new party found P at the eastern
  road on arrival, with the finale — King and Dragon staged behind it —
  one Confirm away before the King was even met. That is the report. The
  save's own castle record (`castleRecords.kingDefeated`, written by the
  castle on the kill and carried by every save that ever won) is the right
  gate, and every other P surface already keys on per-save state: the
  finale plays until its keepsake is recorded, the joke cycle and the
  End-game Summary follow the recorded choice. Nothing else about P was
  wrong: the **town-7 arrival tale** is the M97 first-arrival scene every
  town 2–7 has (approved story, unchanged), and the patrol tales are the
  M110 5 % kind.
- **The profile flag has two other jobs**, both left as designed: the
  three reward classes at New Game (M45, its purpose) and the post-King
  lore questions of the Jester's patrol (M112 `makeLoreEncounter`).
- **"Older versions" is moot for Iron Man by construction:** an Iron Man
  party is never saved (M123), so none can ever be loaded with a Dragon in
  it. The achievement bar is therefore belt and braces over the creation
  rule, exactly as the owner framed it; a Normal save is untouched.
- **The debug "Trigger patrol now" (M110) already fires the real
  dispatcher** through a one-shot consumed by `DungeonState::update` once
  the menus have closed. The Alarm reuses that shape with its own runtime
  flag — never the `DebugCheats` struct, whose every reader is compiled out
  of Release.
- **Pool precedent cuts both ways:** M76's Holy Taxes entered the town-3+
  chest and peddler pools without a generation bump (recorded for veto);
  M102 removed scrolls from the pools with one. The Alarm is kept out of
  the pools altogether (decision 1 below), so no seed's loot moves and the
  question does not arise.

## What was built

### 1. P waits on THIS save's King (`game/Cutscenes.hpp`, `TownState`)

- `game::strangerAtRoadside(party)` — pure, tested: town 7 (a stored index
  clamped onto the ladder) **and** `party.castleRecords.kingDefeated`.
- `TownState::gooseNpcHere` returns it; the sprite, the `"P"` nameplate,
  the "Speak with the stranger" prompt and the Confirm handler all hang off
  that one predicate, so the road is simply empty until this save's King
  falls. `TownState` no longer includes the profile store (its only use).
- Captures: `188_town_7` (M128) now shows the road without P, as a fresh
  party sees it; `148_stranger_choice` and the finale scenes are unchanged
  (they never depended on the gate).

### 2. No Dragons in Iron Man (`game/IronMan.hpp`, `PartyCreationState`, `Achievements`)

- The rule lives once, beside the other Iron Man rules: `kBarredClassId`
  (`"dragon"`), `classBarred(id)`, `hasBarredMember(party)`, the capsule
  suffix `" (Not allowed)"` and the roster note `"Iron Man: the Dragon
  class is not allowed - choose another."`. `kRules` grew to **five**; the
  fourth paragraph reads *"No Dragons: the Dragon class cannot join the
  party."* (shortened to one line so the Danger frame's line budget holds —
  the capture lint caught the two-line version).
- **Party creation** (Iron Man only): the Dragon stays listed and cycles
  like every class (the M45 "never hidden" rule), reads **Dragon (Not
  allowed)** greyed like a locked class with its sprite dimmed, its details
  sheet ends with the note, the note shows under the roster while any slot
  holds it, and Begin buzzes with the M45 error tone instead of starting.
  Jester and Goose are untouched. A Normal game still allows the Dragon.
- **Achievements:** `iron_crown`, `iron_scales`, `iron_bill` require
  `ironMan && !hasBarredMember(party)`; Kingslayer, Wyrmbane and Quackbane
  are unaffected.
- Captures: `193_class_select_iron_man_dragon` (new); `169`/`170` (the
  rules page) re-read with the fifth paragraph.

### 3. The Alarm (`data/items.json`, `ConsumableEffect::Alarm`, the Items screen, `DungeonState`)

- **Content:** `alarm` — "Alarm", consumable, uncommon, **200 g**,
  `minTown: 4`, `effect: alarm`, no `maxHeld` (so the ordinary cap of two,
  raised by the pockets perks like any other). Description: *"A hand bell
  of ungentle volume. Rung in a dungeon, the next patrol comes at once -
  whatever it is."* (the two-line detail lint bounded it: 7 px/char, 386 px,
  two wrapped lines) `CrystalForge --canonicalize` rewrote
  nothing (the row is canonical) and reports zero content errors; the
  editor lists the value through `consumableEffectIds()`.
- **Loader:** `alarm` is valid on a consumable only; a `battleTarget` or a
  `statuses` rider on it is an error (it targets nothing).
- **Shop:** stocked by the ordinary town window — towns 4–7, never 1–3 —
  and ranked an oddity, so it closes the shelf beside Royal Snacks and Holy
  Taxes (`itemShopCategoryRank`). Buying past two is refused at the till
  like any consumable (`capFor`).
- **Never loot:** `buildPools` skips it — no chest ever holds one and no
  peddler ever offers one — so every seed's chests and merchant offers are
  byte-identical to before the item existed; a `[m131][dungeon]` test
  sweeps four towns × five theme settings × ten seeds to pin it.
- **Use path:** `InventoryState` gained `inDungeon` (true only when the
  **dungeon** pause menu opens it). Confirm on the Alarm there spends one,
  sets the runtime one-shot `AppContext::alarmRung`, plays the confirm tone
  and pops both menus; `DungeonState::update` consumes the flag on its
  next tick and calls `triggerPatrol()` — the M110 dispatcher, the seeded
  kind for this patrol index, the M109 lifetime tallies, the counter
  rewound and the index advanced by the same `consumePatrol` — so a rung
  patrol is indistinguishable from a walked-down one (an ordinary team, the
  Golden Goose, the Jester's question, the chests or a P scene, by the
  mixture). In town the same Confirm answers *"The Alarm is for a dungeon -
  nothing here answers."* on the Danger banner and spends nothing. The
  dungeon constructor clears a stale flag.
- **Battle:** `BattleState::consumableIds` never lists it, and
  `Battle::useItem` treats it as `None` ("Nothing happens.") should content
  ever hand one to a battle. `itemUseRefusal` answers *"The Alarm is rung,
  not taken."* for any member, so the M43 pick can never accept it.
- Captures: `194_items_alarm_dungeon` (the bag with the Alarm's two-line
  description), `195_items_alarm_town` (the refusal banner). **195 scenes.**

## Decisions taken without asking (veto any of them)

1. **Town shelves only.** The Alarm is never a chest reward and never a
   peddler offer — "only sold in town 4 and after" read as the shop rule it
   is, and keeping it out of the pools keeps every seed's loot byte-identical
   (no generation bump to ask for). Revert = one `continue` in `buildPools`
   (and then a generation decision).
2. **An oddity on the shelf** (rank 4, after the revives), sorted with the
   other oddities by value.
3. **Rung, the menus close at once** — no confirmation prompt, no extra
   message: the patrol arriving IS the feedback (the "Trigger patrol now"
   idiom the owner already knows).
4. **In town it is refused with the reason** on Confirm (the M43 idiom),
   not hidden from the bag and not greyed at a glance.
5. **The Dragon stays listed** at Iron Man creation, greyed "(Not
   allowed)" like a locked class, and is not skipped when cycling — the M45
   rule that the roster is never hidden.
6. **A fifth rule paragraph**, one line, rather than folding the bar into
   the accomplishments paragraph.
7. **The profile flag keeps its other jobs** (class unlocks, post-King lore
   questions); only P's gate moved.
8. **No new SFX and no icon** — the ring uses the confirm tone; consumables
   carry no icons in this game.
9. **No "alarms rung" statistic** — the lifetime ledger counts a rung
   patrol like any patrol; a new counter would touch the saved ledger
   object.

## Compatibility

- **Saves:** v1, unchanged. An older save shows P exactly when its own
  castle record says the King fell — every save that beat him carries the
  record. Inventories are keyed by id, so the Alarm is simply absent from
  older bags until bought.
- **Content:** v1; one additive enum value. A CrystalForge built before
  M131 would reject the shipped `items.json` (an unknown `effect`), as with
  any new enum value.
- **Deterministic seeds:** generation 25; chest and merchant pools are
  byte-identical (the Alarm never enters them). **Scores:** unaffected.
- **Battle rules 19**: the Alarm never reaches a battle.

## Known limitations

- The Alarm cannot choose what answers: the kind is the run's seeded
  sequence for that patrol index — by the owner's own "follows normal
  rules".
- In town the Alarm's row looks like any other until Confirm explains.
- The bar keys on the class id `dragon` (a shipped content id, like the
  creation screen's class order); a renamed class would need the constant
  moved with it.
- The M97 note's "anyone who knows" framing is superseded; it carries a
  dated pointer here rather than a rewrite.

## Documentation updated

`docs/milestones.md` (row 131 + section; the M23 section's scene count),
this note, `docs/game_design.md` (the Hooded Goose paragraph, the Iron Man
rules and accomplishments, the achievements count paragraph, the danger
counter, the shop list), `docs/technical_design.md` (the M97 trigger line,
the scene count, a new §71), `docs/editor_guide.md` (the `alarm` value),
`docs/manual_test_matrix.md` (rows 191/258/262 amended, rows 288–290 new),
`docs/milestone_notes/M97_hooded_goose.md` and
`docs/milestone_notes/M123_playtime_iron_man.md` (dated pointers). README,
`docs/control_standard.md`, `docs/asset_pipeline.md` and
`assets/credits.md` restate nothing here (no asset was added).

## Completion report

### 1. Implementation summary

**M131 — Owner batch 4.** All three points complete: the roadside gate, the
Dragon bar (creation + accomplishments + the rules page), the Alarm
(content, loader, shop, pools, bag, dungeon hook, battle exclusion).
Player-facing: a new game no longer meets P at town 7's road before its own
King kill; Iron Man refuses the Dragon; the Alarm is on the town-4+ shelf.

### 2. Files changed

- **Source:** `src/game/Cutscenes.hpp`, `src/game/IronMan.hpp`,
  `src/game/Achievements.cpp`, `src/game/ItemUse.hpp`,
  `src/content/Enums.{hpp,cpp}`, `src/content/Definitions.hpp`,
  `src/content/ContentLoader.cpp`, `src/core/AppContext.hpp`,
  `src/dungeon/DungeonGenerator.cpp`, `src/battle/Battle.cpp`,
  `src/states/TownState.cpp`, `src/states/PartyCreationState.{hpp,cpp}`,
  `src/states/InventoryState.{hpp,cpp}`, `src/states/DungeonMenuState.cpp`,
  `src/states/DungeonState.cpp`, `src/states/BattleState.cpp`,
  `src/states/ItemShopFilter.hpp`, `src/states/GameModeState.cpp` (a
  comment), `src/capture/CaptureRunner.cpp`.
- **Tests:** `tests/test_owner_batch_4.cpp` (new), `tests/CMakeLists.txt`,
  `tests/test_editor_enum_lists.cpp`, `tests/test_content_loader.cpp`
  (the item-count pin).
- **Content:** `data/items.json` (+1 row).
- **Documentation:** see "Documentation updated".
- **Build configuration:** none beyond the test list.

### 3. Plan deviations

There was no written plan; the nine decisions above are the judgement
calls. One in-flight correction: the fifth rule paragraph and the town
refusal were each shortened to one line after the first capture (the rule
overflowed the Danger frame; the banner wrapped onto the footer hint), and
the Alarm's description was trimmed to the two-line detail lint after the
first Release suite flagged it.

### 4. Compatibility

See "Compatibility".

### 5. Automated validation

All run 2026-09-22 from the VS 2022 developer shell (amd64):

- `cmake --build --preset debug` — **succeeded**, no project warnings.
- `CrystalForge.exe --canonicalize` — 0 files rewritten, 0 content errors
  before and after.
- `crystal_tests.exe "[m131],[ironman],[itemshop],[caps],[content],[cutscene],[achievement],[editor],[inventory]"`
  — **120 test cases, 14 640 assertions, all passed**.
- `ArePGeese.exe --capture <dir>` — **195/195 scenes clean** (three new).
  Read by eye: `169`, `188`, `193`, `194`, `195`.
- `ctest --preset debug` — **981/981 passed** (1 609 s, alongside the
  Release run). A first full run failed only the two-line description lint
  on the Alarm's original text; the text was shortened and the whole suite
  re-run.
- `cmake --build --preset release` — **succeeded**, no project warnings.
- `ctest --preset release` — **977/977 passed** (987 s; the Release preset
  carries no capture-only cases).

### 6. Manual owner validation

Matrix rows **288–290**: the empty road on a fresh save that reaches town
7 before its King (and P back after the kill); the Iron Man rules page and
roster with the Dragon; the Alarm's shop gating, the town refusal, the ring
in a dungeon (each patrol kind through the dev build's next-kind one-shot),
its absence from the battle bag and from chests and peddlers. Rows 191,
258 and 262 carry the amended expectations.

### 7. Known limitations

See "Known limitations".

### 8. Documentation updated

See "Documentation updated".

### 9. Final status

`implemented, awaiting manual approval`
