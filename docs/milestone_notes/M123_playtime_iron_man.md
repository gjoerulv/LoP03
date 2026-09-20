# M123 — Save-slot play time & the Iron Man rules

**Status:** complete (approved 2026-09-20)
**Program:** M120–M125 (owner-authorized 2026-09-18 via the approved plan;
branch `oyb12`, baseline `d789f31`).
**No version motion:** rules, generation, save, settings, content and manifest
versions unchanged; no schema change; no new asset. `Party::ironMan` is
runtime-only and never serialized; `achievements.json` gains three ids under
its unchanged version (unknown ids were always kept).

## Scope (plan section M123; the owner's points 6 and 8)

- **Point 6:** the save slots show the party's total play time as seven
  digits, `HHH:MM:SS`, with the hour digits not reached yet greyed
  (`001:03:56` greys its two leading zeros).
- **Point 8:** **Iron Man**, selectable when starting a New Game: saving and
  autosaving are disabled, so the game is one sitting; three new
  accomplishments for felling the King, the Last Dragon and the Deadly Duck
  in Iron Man. Owner rulings from the planning interview (2026-09-18):
  **permadeath** on every real wipe (dungeons, castle challenges, treasure
  digs; never sparring; fleeing is not a wipe); **escaping a battle** forfeits
  all gold and the whole bag (worn gear and heirlooms kept) and leaves
  everyone standing at 1 HP / 0 MP, the fallen staying fallen; and all of it
  is **explained when the mode is chosen**.
- The run's send-off scene, its summary and the record of fallen runs are
  **M124** — M123 ends a run with a plain notice.

## What was built

### The slot clock (`game/PlayTime.hpp`, `SlotMenuState`)

- `formatSlotPlayTime(seconds)` → `"HHH:MM:SS"` plus how many leading hour
  digits to grey (3 below one hour, 2 below ten, 1 below a hundred, else 0).
  Negative reads as zero; the clock **stops at 999:59:59**, it never wraps.
- `SlotSummary::playSeconds` comes from the loaded party's M109 ledger
  (`lifetime.explore.playSeconds`; a pre-M109 save reads 0).
- `SlotMenuState` draws the clock right-aligned at body size on the Save and
  Load lists, the greyed digits in the disabled colour; an empty slot has no
  clock. The label's fitted width now stops short of the clock. One
  `summary()` per slot per rebuild feeds the label, the King title and the
  clock (it used to be loaded once for each).

### The mode page (`GameModeState`)

- Title → **New Game** now opens a page with two rows, **Normal** (under the
  cursor) and **Iron Man**, and a panel explaining the highlighted mode. For
  Iron Man the panel (a Danger frame) carries the whole rule set — four
  paragraphs from `ironman::kRules`. Confirm on Iron Man asks once more
  ("Begin an Iron Man run?", cursor on Back); then party creation runs as
  always and marks the party `ironMan` **after** `resetForNewGame`.

### The rules (`game/IronMan.hpp`, pure)

- **No saves.** `SaveSystem::save` refuses an Iron Man party — the one choke
  point manual saves and the autosave share. The Guild skips its entry
  autosave and its caption reads "Choose a dungeon - Iron Man: nothing is
  saved."; all three Save entry points (the town Save Point, the Castle and
  Goose Town menus) open the Save list with every slot greyed and the refusal
  on a Danger banner, Back being the only hint.
- **The escape price.** `applyEscape` = `forfeitOnEscape` (gold to zero
  through `loseGold`, so the M109 ledger books it as a loss; the bag emptied
  of everything that is not an heirloom — unknown ids included) +
  `clampEscapeVitals` (standing members 1 HP / 0 MP, the fallen untouched,
  idempotent). It lands in `BattleState::writeBackParty`, the one place every
  fight's result reaches the party, when the outcome is `Escaped` and the
  fight is a real one (`ironManStakes()`: an Iron Man party **and** a
  lifetime hook — the spar and the capture scenes pass none, so their escapes
  stay free). The Escape command asks first (Danger prompt naming the price,
  cursor on "Keep fighting"); the flee line and the Done line state the loss.
  `DungeonState` re-pins the vitals after a dragonform / flock restore, whose
  percentage mapping could otherwise round 1 HP up.
- **Permadeath.** `beginIronManFall(stack, context, FallenInfo{place, foes})`
  is the one door every real wipe leaves through: the dungeon's Defeat branch
  (no carry-out, no gold halving; the ledger still counts the wipe), a lost
  castle challenge (King, Dragon, Duck, Boss Rush, Endless, Guild gauntlet)
  and a lost treasure dig. An **escape** from any of them takes the ordinary
  path. The sparring mirror never calls it. It clears the stack and shows
  `IronManFallState`: "The run ends here", *Fell at* / *Beaten by* / *Play
  time*, then Confirm returns to the title. The battle's defeat line reads
  "The party has fallen... Iron Man: the run ends here."
- **Where and who.** `fallenPlaceDungeon` ("Town 3 - Crystal Mine, floor 2 of
  4" / "..., Eternal floor 9"), castle and dig place lines built at the call
  sites, and `fallenFoes` (the team's name, else its boss, else its first
  enemy "and company"; a decision patrol's wipe names the Mimic or "The
  riddling Jester").
- **Honest warnings.** Both pause menus' Quit prompt reads "Iron Man: nothing
  is saved. Quitting ends this run for good." and an `IRON MAN` chip rides
  the pause screens (top-right in town, top-left in a dungeon where the
  minimap owns the right corner).

### Accomplishments 20 → 23

- `iron_crown` **Iron Crown**, `iron_scales` **Iron Scales**, `iron_bill`
  **Iron Bill**: the kingslayer / wyrmbane / quackbane predicates **and**
  `Party::ironMan`. An Iron Man party is never loaded, so its castle records
  were all earned inside the one sitting. The Achievements page now runs
  twelve rows a column; the description gap tightened by 6 px.

## Deviations from the plan

- **Save entry points.** The plan greyed the Castle and Goose Town "Save"
  rows and gave the town Save Point a refusal line. Those menus have no
  reason line and `TownState` has no message surface, so a greyed row would
  have been silent. Instead every entry point opens the Save list, which
  refuses in one place with the reason visible. Same rule, one choke point.
- **Mode select** is its own page (`GameModeState`) rather than a
  `ConfirmPromptState`: a mode choice is not a danger question, and the
  rules needed a panel, not a prompt body. The final Iron Man confirm is
  still the Danger prompt the plan named.
- The ledger has no "items lost" counter, so the forfeited bag is not
  tallied (`EscapeForfeit::items` is returned for the caller but only the
  gold reaches the ledger).

## Assumptions accepted with the plan (owner may overrule)

- The pause menu's **Retreat to Town** keeps today's rule in Iron Man.
- Counters that are not bag items — rest tokens, legendary tokens, map
  pieces, curios — survive an escape.
- **Every** battle Escape in a real fight pays the price - including
  backing out of a decision patrol (the riddling Jester, the three chests).
  The prompt warns first, so it is never a surprise.
- The Inn has **no poverty rule** (`restCost` ≥ 20 g): after an escape the
  way back is a rest token, selling worn gear, or winning a fight at 1 HP.
  This is the mode's teeth, but it is yours to judge.

## Tests

- `tests/test_iron_man.cpp` (new, `[m123]`, 12 cases): the clock's format,
  grey-digit table and clamps; `SlotSummary::playSeconds`; the save and
  autosave refusal (no file written) and that a load never brings the flag
  back; `resetForNewGame` clears it; the escape price (gold + ledger loss,
  bag emptied, heirloom kept, worn gear and the non-bag counters untouched,
  1 HP / 0 MP, fallen untouched, idempotent, a penniless party); what the
  bag keeps; the three predicates (normal party: never; each its own kill);
  the fall text; and that the rules page spells out every owner ruling.
- `tests/test_achievements.cpp` (the roster bound) and
  `tests/test_curios.cpp` (the exact count pin): 20 → 23.
- Captures: `167_slot_menu_playtime`, `168_new_game_mode`,
  `169_new_game_iron_man`, `170_new_game_iron_man_confirm`,
  `171_battle_iron_man_escape`, `172_save_iron_man_refused`,
  `173_pause_iron_man`, `174_iron_man_fall`.

## Compatibility

- **Saves:** untouched — no new field; the flag is runtime-only and refused
  at the writer. **Settings / scores / seeds / rules / schemas:** untouched.
  `battleRulesVersion` does not move: battle resolution is unchanged; the
  escape price is applied to the party after the battle ends.
- **Achievements file:** three new ids under the same version.
- **Scoreboard:** an Iron Man clear posts like any other (no mode column).

## Known limitations

- No Iron Man marker outside the pause menus (no HUD badge).
- Quitting the game mid-run simply loses the run (it is warned); it is not
  recorded as a fall.
- The fall notice is deliberately plain — M124 replaces it with the send-off
  scene, the summary and the Hall of Shame record. *(Done 2026-09-19:
  `IronManFallState` no longer exists; see `M124_iron_mans_fall.md`. The
  `174_iron_man_fall` capture now shows the send-off.)*
- The debug menu has no Iron Man toggle; test from a New Game.

## Documentation updated

`docs/milestones.md`, this note, `docs/game_design.md` (§12 "Iron Man", the
save-slot clock, the accomplishment count), `docs/technical_design.md` (§64,
the live scene count), `docs/control_standard.md` (New Game flow, the Escape
question), `docs/manual_test_matrix.md` (rows 257–262),
`docs/release_hardening_manual_checklist.md` (the no-save check), `README.md`
(How to play).

## Completion report

### 1. Implementation summary

**M123 — Save-slot play time & the Iron Man rules.** Complete. Every slot
row shows a fixed-width `HHH:MM:SS` clock with un-reached hour digits greyed.
New Game offers Normal or Iron Man with the full rules on screen; an Iron Man
party cannot be saved, pays the escape price, ends for good on any real
wipe, and can earn three new accomplishments.

### 2. Files changed

- **Source (new):** `src/game/PlayTime.hpp`, `src/game/IronMan.hpp`,
  `src/states/GameModeState.{hpp,cpp}`, `src/states/IronManFall.{hpp,cpp}`.
- **Source (changed):** `src/game/Party.hpp`, `src/game/Achievements.{hpp,cpp}`,
  `src/save/SaveSystem.{hpp,cpp}`, `src/states/SlotMenuState.{hpp,cpp}`,
  `src/states/MainMenuState.cpp`, `src/states/PartyCreationState.{hpp,cpp}`,
  `src/states/BattleState.{hpp,cpp}`, `src/states/DungeonState.cpp`,
  `src/states/CastleChallengeState.{hpp,cpp}`,
  `src/states/TreasureFightState.cpp`, `src/states/GuildState.cpp`,
  `src/states/TownMenuState.cpp`, `src/states/DungeonMenuState.cpp`,
  `src/states/AchievementsState.cpp`, `src/capture/CaptureRunner.cpp`,
  `CMakeLists.txt`.
- **Tests:** `tests/test_iron_man.cpp` (new), `tests/test_achievements.cpp`,
  `tests/test_curios.cpp`, `tests/CMakeLists.txt`.
- **Docs:** see "Documentation updated".

### 3. Plan deviations

See "Deviations from the plan" — none changes a rule the owner approved.

### 4. Compatibility

See "Compatibility".

### 5. Automated validation

All run 2026-09-18 from the VS 2022 developer shell (amd64):

- `cmake --build --preset debug` - **succeeded** (game, CrystalForge, tests).
- `crystal_tests.exe "[m123],[achievement],[save],[ledger]"` - **51 test
  cases, 774 assertions, all passed**; after the pin fix below,
  `"[curio],[achievement],[m123]"` - **21 cases, 1052 assertions, all passed**.
- `ArePGeese.exe --capture <dir>` - **174/174 scenes clean**; the eight new
  scenes (`167`-`174`) plus `39_achievements` and `11_slot_menu_save` were
  read back by eye (the clock's grey digits checked on a 6x crop). The first
  run caught a real overflow on the fall notice (a 234 px place line in a
  222 px column); the panel was widened and the specimen kept.
- `cmake --build --preset release` - **succeeded**.
- First full run: `ctest --preset debug` and `--preset release` each failed
  **one** test - `curios: the Curator achievement fires on the full dozen`,
  a second accomplishment-count pin (`== 20`) this milestone makes stale.
  Pin updated to 23, both configs rebuilt, both suites re-run in full:
  - `ctest --preset debug` - **936/936 passed** (1853 s).
  - `ctest --preset release` - **932/932 passed** (1743 s).

No data file changed in this milestone, so no canonicalize run was needed.

### 6. Manual owner validation

Matrix rows **257–262**: the clock's readability on both lists; the mode
page and whether the rules read clearly before committing; that nothing can
be saved; the escape question, its price and the 1 HP / 0 MP aftermath
(including whether recovery without an Inn poverty rule is fair); permadeath
from a dungeon, a castle challenge and a dig, and that fleeing or sparring
never triggers it; the three Iron accomplishments.

### 7. Known limitations

See "Known limitations".

### 8. Documentation updated

See "Documentation updated".

### 9. Final status

`complete (approved 2026-09-20)`

> **M127 (2026-09-20):** the slot rows also show the party's sprites left of
> the clock, hopping while highlighted. See `M127_owner_batch_3.md`.
