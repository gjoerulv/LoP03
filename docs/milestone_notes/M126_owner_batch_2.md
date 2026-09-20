# M126 — Owner batch 2

**Status:** complete (approved 2026-09-20)
**Authorization:** owner request 2026-09-20, made in the M120–M125 session
after that program's implementation closed ("make all this into as few
milestones as possible ... please go ahead and implement"). Branch `oyb12`,
baseline `d789f31` plus the uncommitted M120–M125 work.
**Version motion:** `dungeon::kGenerationVersion` **24 → 25** (the guarded
chest room). Nothing else: battle rules 19, save v1, settings v1, content v1,
manifest unchanged, `project(VERSION)` 0.9.0.

## Scope (the owner's six points)

1. Chest results should **list** everything received from the top, centred,
   gold included: one line of gold (the total, white), everything else in
   gold with duplicates as x2, x3 ...
2. Skills learned on a level-up wear their icons on the level-up panel.
3. The "C" left on the floor after a chest is taken goes away.
4. A chest guardian stands **in front of** its chest; a guarded chest is
   surrounded by walls; the guardian blocks the only way in and cannot be
   passed.
5. The Dragon's Attack needs no target (like an all-enemies skill) — but in
   the Jester and Chest patrol fights its attack must be single target.
6. The blackjack cards are twice as big.

## Re-audit findings that shaped the work

- The chest sentence ("Found N gold - and:") and the item tag row both live in
  `DungeonState`'s outcome panel, shared with the Miner's Cache, the reels,
  the Duck Peddler and the Sacrifice. The reels can pay the same piece on
  several spins, so the merging the owner asked for matters there too.
- The lingering "C" was the marker's text fallback: an opened chest dropped
  its sprite (`fallbackId = nullptr`) and fell through to the dark box with
  the glyph.
- The guard stood **beside** a chest the party could step onto; only the
  prompt ("Guarded - defeat the team first.") refused the take. Treasure
  rooms are always single-door dead ends, which makes a walled niche on the
  far wall always valid.
- The Dragon's sweep already ignored the chosen target (`Battle::attack` →
  `attackAll`), so the target pick was pure ceremony. In a decision patrol
  the sweep counted as "sweeping the field" — and the Dragon has **no
  skills**, so a Dragon (or a Dragonform party) could only ever be punished
  or wake the Mimic. That is what the owner's exception fixes.

## What was built

### Loot rows (`src/game/LootSummary.hpp`, new, pure)

- `LootSummary` totals gold (`addGold`; zero and losses add nothing) and
  merges items by id in first-received order (`addItem`); `rows()` yields the
  gold row first, then one `LootRow` per distinct piece; `LootRow::text()`
  is the wording — `"26 gold"`, `"Power Ring"`, `"Power Ring x2"`.
- The outcome panel's rows are `LootRow`s now. **Chest**: gold + piece as
  rows; the body is empty, or the trap's sentence. **Miner's Cache**: gold +
  find as rows, the body trimmed to "You clear the rockfall - battered, but
  richer." **Reels**: every spin feeds one summary — gold *won* (the 200 g
  consolation prizes, the 1000 g jackpot) is one total line, a piece paid
  twice is "x2"; the tax papers' *loss* is not a receipt and is not listed;
  the per-spin joke lines stay. The **Duck Peddler** and the **Sacrifice**
  keep their single row, through the same shape.
- Gold rows draw in the body white, everything else in the reward gold with
  its M81 icon, as before. A panel that is all rows closes up under them.

### The opened chest is gone

- `buildRoom` makes a chest marker only while the chest is unopened;
  `openChest` rebuilds the room and recomputes the interaction. No marker
  means no glyph, no brackets, no prompt. The "It is empty. It was empty the
  last time, too." line went with it (unreachable).

### Learned-skill icons (`src/ui/TagFlow.hpp`, new, pure)

- `LevelUpDiff::newSkillIcons` (parallel to `newSkillNames`) carries each
  learned skill's M121 kind icon id from `applySpoils`.
- The spoils panel draws "New:" followed by icon + name tags, flowed by
  `ui::flowTags` and wrapped under the label when a line fills; the frame is
  sized from the same flow it draws.

### The guarded vault (`src/dungeon/RoomLayout.{hpp,cpp}`) — generation v25

- With a guard, the chest keeps its place against the far wall; its two
  flank tiles become walls; the guard anchor is the tile one step toward the
  door. The guard's tile is the chest's only open neighbour. No RNG draw.
- The validator now checks such a room in both states: **guard standing** —
  the chest must be *unreachable* and the guard faceable; **guard fallen** —
  the chest must be reached. It also requires the guard to be the chest's
  single open side, so the old "beside" placement is rejected.
- `kGenerationVersion` 25 with its history line. Topology, teams, chests and
  events are byte-identical to v24; because the version feeds every
  room-local seed, room sizes and pillars re-roll, as on every bump.
- `DungeonState` moves a chest guard's danger label beside the guard when
  the chest is the tile right above it (it would have covered the chest).

### The Dragon's targeting

- `BattleState::onCommand`: a sweeping basic attack resolves at once — no
  target phase — unless a decision is pending.
- `decisionActionIsAoe` (`game/SpecialEncounter.hpp`): in a decision
  encounter **a basic attack is always one pick**; only a skill reaching
  several foes sweeps the field. `resolveDecision` and
  `uncontrolledDecisionFor` both read it. The battle engine is untouched.

### Blackjack

- Cards at 2× (36×48), rank font 20, the frame grown to hold two rows;
  a hand wider than the table fans.

## Decisions taken without asking (veto any of them)

1. **The generation bump.** The vault changes what a seed's rooms look like,
   so by the project's own rule the version moves (24 → 25), following the
   "owner direction" precedent of v22/v23 — the owner asked for the change
   itself. Consequence: older scoreboard entries read as older rules, as
   after every bump.
2. **"The Dragon's attacks" reads as "any sweeping basic attack".** The
   rule covers the Dragon class, a Dragonform party and a Rain of Arrows
   archer alike — one sentence, no special case per source. All-foes
   *skills* still sweep the field and are still punished.
3. **No battle-rules bump.** The decision rule lives above the battle
   engine (the M112/M117 precedent); `Battle::hostileTargetCount` keeps its
   meaning.
4. **An opened chest vanishes entirely**, including the "It is empty" quip.
5. **"And similar"** was read as every outcome panel that hands over gold
   or bag items (chest, cache, reels). Curios, tokens and map pieces are
   announcements, not bag loot, and keep their sentences. The battle spoils
   header ("+N XP each +N gold") is unchanged.

## Compatibility

No save, settings or content schema change; no new asset. New score entries
are tagged generation v25.

## Known limitations

- The flank walls use the theme's ordinary wall tile, so in a dark theme the
  niche reads as part of the far wall — intended, but the owner judges it.
- A fanned blackjack hand (8+ cards) overlaps cards slightly; the rank of a
  number card is centred and can be partly covered from ten cards up. Such
  hands are very rare.
- The level-up icon line was checked at the capture scene's worst case (four
  leveled members, two skills on one line); a member learning more skills
  than fit wraps to a second line and grows the frame by 12 px.

## Documentation updated

`docs/milestones.md`, this note, `docs/game_design.md` (Jester question,
Mimic, blackjack, chests, victory spoils, the Dragon), `docs/control_standard.md`,
`docs/technical_design.md` (generation restatements, scene count, §67),
`docs/ui_style_guide.md` (§13: received loot is listed),
`docs/manual_test_matrix.md` (rows 269–274; row 218's generation tag),
one-line pointers in the M16, M45, M68, M80, M104 and M112 notes.

## Completion report

### 1. Implementation summary

**M126 — Owner batch 2.** Complete: all six points.

### 2. Files changed

- **Source (new):** `src/game/LootSummary.hpp`, `src/ui/TagFlow.hpp`.
- **Source (changed):** `src/states/DungeonState.{hpp,cpp}`,
  `src/states/BattleState.cpp`, `src/states/BlackjackEventState.cpp`,
  `src/game/Spoils.hpp`, `src/game/SpecialEncounter.hpp`,
  `src/dungeon/RoomLayout.{hpp,cpp}`, `src/capture/CaptureRunner.cpp`.
- **Tests:** `tests/test_owner_batch_2.cpp` (new), `tests/test_danger.cpp`
  (the generation pin), `tests/CMakeLists.txt`.
- **Docs:** see "Documentation updated".

### 3. Plan deviations

None from the request; see "Decisions taken without asking".

### 4. Compatibility

See "Compatibility".

### 5. Automated validation

All run 2026-09-20 from the VS 2022 developer shell (amd64):

- `cmake --build --preset debug` - **succeeded** (game, CrystalForge, tests).
- `crystal_tests.exe "[m126]"` - **10 test cases, 4331 assertions, all
  passed** (the vault sweep checks 180 generated dungeons: every guarded
  chest sealed behind its guard and reached once it falls).
- `crystal_tests.exe "[roomlayout],[lore],[chest],[spoils],[danger],[gamble],[m117]"`
  - **59 test cases, 22444 assertions, all passed**.
- `ArePGeese.exe --capture <dir>` - **180/180 scenes clean** (three new:
  `178_chest_loot`, `179_vault_guarded`, `180_vault_open`; these and
  `83_battle_spoils`, `87_event_outcome`, `119_reels_icons`,
  `127_blackjack_cards` read by eye - which is how the guard's label was
  caught sitting on the chest, and moved).
- `cmake --build --preset release` - **succeeded**.
- `ctest --preset debug` - **961/961 passed** (307 s).
- `ctest --preset release` - **957/957 passed** (300 s).

### 6. Manual owner validation

Matrix rows **269–274**: the listed loot, the vanished chest, the learned-
skill icons, the walled-in guarded chest, the Dragon's Attack in ordinary
fights and in both decision patrols, the blackjack cards.

### 7. Known limitations

See "Known limitations".

### 8. Documentation updated

See "Documentation updated".

### 9. Final status

`complete (approved 2026-09-20)`
