# M103 — Six new events

**Status:** complete (approved 2026-09-02)
**Program:** M98–M108 "Are P Geese" (owner-authorized 2026-08-16).
**Generation 18 → 19** (six new pure-hash event replacements move seeds'
event rolls where the hashes land; everything else byte-identical). NO
battle-rules bump (the polymorph changes battle INPUTS via the Dragonform
precedent; the double-XP and +100 score are post-battle bookkeeping). Save
schema unchanged (the polymorph stash and double-XP flag are runtime-only —
the entry autosave predates any event).

## Scope (owner events 1, 4, 5, 6, 7, 8 — all towns/themes per interview)

Goose polymorph (+100, random member, rest of run) · equipment sacrifice
(next battle double XP) · level-up with MP price · Stranger "P" story +
20 MP · legendary-token exchange · patrol reset.

## What was built

- **Placement** ([ThemeEvents](../../src/dungeon/ThemeEvents.hpp)): six new
  `RoomEventKind`s drawn sequentially after the peddler and Dragonform on
  the identical pure-hash replacement contract (own salt pairs, plain
  rolled slots only, at most one of each per dungeon; chances 6–8 %).
  A shared `recollect/apply` helper keeps the generator block flat.
  (v23, 2026-08-28: the sequential chain and the per-event chances are
  gone — all six roll the shared 8 % with uniform contention via
  `encounterRegistry()`; see the M55 note's adjustments.)
- **The Pond Spirit** (`GoosePolymorph`): a RANDOM non-goose member
  (seeded pick, reload-honest) enters **gooseform** —
  [game/Gooseform.hpp](../../src/game/Gooseform.hpp), the Dragonform
  pattern stretched run-long: bare Goose at the member's own level, the
  worn heirloom riding along (the M96 carve-out), vitals mapped by
  percentage, **XP and levels earned while goosed carried back** on
  restore. Restore lives at the run's one exit choke
  (`DungeonState::onExit`) so boss victory, retreat, wipe and quit all
  return the real member exactly once. +100 itemized ("Goose pact", the
  wager/dragonform channel). All-geese parties get a dry refusal and the
  event is not spent.
- **The Hungry Forge** (`Sacrifice`): bag-equipment-only pick via the new
  reusable [EventChoiceState](../../src/states/EventChoiceState.hpp)
  modal; the piece is removed, `party.doubleXpNext` arms, and
  `applySpoils` doubles exactly one award (panel-honest, cleared on use;
  a leftover clears at onExit).
- **The Level Altar** (`LevelAltar`): member pick → exactly one level
  (the Training-Hall `grantXp` rule) and MP to 0; at kMaxLevel the event
  is spent on a dry line instead (owner's spec).
- **A Familiar Hood** (`StrangerStory`): a seeded tale from the new
  "story_*" pool (3 authored, timing-neutral; the M100 optionless-tale
  loader rule now covers both prefixes) plays as a cutscene above the
  member-pick modal; the pick restores 20 MP.
- **The Token Changer** (`TokenExchange`): 1 legendary token → 3 rest
  tokens OR 1 map piece through the shared M83 `grantMapPiece` rule at
  this dungeon's own guard scale; tokenless parties get a refusal and
  keep the event.
- **The Cold Trail** (`PatrolReset`): `dangerSteps_` rewinds to 100.
- **Flavor**: six authored `event_flavor.json` panels (the M80 contract;
  the existing wrap-budget test lints them); footer trade-off lines for
  all six; outcome panels throughout.

## Verification

- New tests: placement determinism + at-most-one-per-dungeon + full-kind
  coverage over 400 seeds; the goose pact's +100 itemization; the
  sacrifice's double-once award; gooseform enter/leave (heirloom carry,
  XP continuity, KO both ways, refusal guards) in `test_gooseform.cpp`;
  the flavor lockstep list grew to 19. Count pins updated deliberately:
  cutscenes 15 → 18 (3 stories), `kEventFlavorIdCount` 13 → 19,
  generation pin 18 → 19.
- Build clean; full suite + capture recorded in the completion report.

## Deviations from the plan

- Event picks ride ONE reusable modal (`EventChoiceState`) instead of
  three bespoke states — less surface, same flows.
- The story pool is its own "story_*" prefix (3 scenes) rather than
  reusing the post-finale jokes, whose framing assumes the King already
  fell; the M100 loader rule simply grew a second prefix.

## Manual owner checklist

Matrix rows **197–199**: meet each event (debug reroll seeds or sweep
runs); accept the pond spirit and finish/retreat/wipe with a goosed
member; burn gear at the forge and read the doubled panel; level a member
at the altar (and a level-99 one); hear a story; trade a token both ways;
reset the patrol fuse. **Judge the flavor's dryness and the rates.**

## Documentation updated

game_design §6 (the six-event passage), ledger row, matrix rows 197–199,
this note.

## Post-implementation fix (2026-08-17, owner screenshot)

The Sacrifice's pick modal (`EventChoiceState`) listed one row per bag
piece and GREW with the list — a deep bag pushed the box past the 240px
screen with no way to see (or reach visibly) the rest. The modal's rows
now show through a fixed nine-row window that follows the cursor
(`ui::ScrollWindow` + `drawMenuScrolled`, whose arrows mark the hidden
remainder), and the title wraps to two lines so long flavor prompts stop
squeezing into one. Every M103/M104 modal shares the state, so the
Token Changer, Level Altar, blackjack bets and reels menus inherit the
same bounds. Capture `124_event_choice_scroll` pins a 20-row list.
