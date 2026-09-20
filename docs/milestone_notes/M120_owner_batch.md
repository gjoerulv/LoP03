# M120 — Owner batch

**Status:** complete (approved 2026-09-20)
**Program:** M120–M125 (owner-authorized 2026-09-18 via the approved plan;
branch `oyb12`, baseline `d789f31`).
**No version motion:** rules, generation, save, settings and content versions
unchanged; one additive optional item field (`mpAmount`); no manifest change.

## Scope (plan section M120; the owner's points 1, 2, 7, 9, 10)

Five small, independent slices from the owner's 2026-09-18 ideas list:

1. The curio map's X on the minimap pulses to catch the eye, like the title
   phrase.
2. The Elixir also restores 50 MP.
3. Hidden: an Eternal run that got beyond floor 4 loses only 25 % gold on a
   wipe.
4. The settings resets ask for confirmation, and the "reset to defaults"
   banner disappears when leaving Settings or entering another category —
   for the settings, the tutorial prompts and the controls alike.
5. The title screen drops its Controls row for a Credits & Licenses page
   ("A game by SmettPlay" + the third-party licenses).

## What was built

### Slice 1 — The chart's pulse

- `DungeonState::renderMinimap` draws the M66 X from a three-step ramp
  (ember `176,150,64` → the pre-M120 gold `235,214,112` → glint
  `255,246,190`) indexed by `ui::motionPhase3()` — the title phrase's own
  ~1.5 Hz clock, so the two pulses read as one idiom. Colour only: no
  geometry, fog or layout change; the X still burns through the M93 fog.

### Slice 2 — The Elixir's MP rider

- New optional `ItemDef::mpAmount` (default 0). The loader accepts it on
  `heal` consumables only and reports it anywhere else (equipment, a
  `restore_mp` item — which already has `effectAmount` — or a negative).
- Battle: the one shared `Battle::useItem` path restores it after the heal.
  The King's `kingMpAmount` (M43) still replaces it in his fight *when
  authored*; an item without one keeps its rider there. Field Medic's potency
  share scales it exactly as it already scaled the King's MP amount.
- Out of battle (`game/ItemUse.hpp`): a heal with a rider is offered when HP
  **or** MP has room ("Already at full HP and MP." when neither has), and the
  line reads "… recovers N HP and M MP." A rider-free heal keeps its pre-M120
  wording byte for byte.
- Content: the Elixir gains `"mpAmount": 50` and reads "Restores a great
  deal of HP and 50 MP to one ally." `CrystalForge --canonicalize` reports the
  file already canonical. The editor's item descriptor carries the field now
  (the coverage test requires it; M125 audits the rest).

### Slice 3 — The hidden Eternal mercy

- `wipeGoldLoss(gold, eternal, eternalFloorsCleared)` in `game/Ledger.hpp` is
  the one wipe price: the M89 halving (`gold - gold / 2`, the odd coin stays),
  or `gold / 4` once an Eternal run has felled `kEternalMercyFloors` = 4
  floor-bosses — i.e. the party fell on floor five or deeper ("beyond floor
  4"). `DungeonState`'s Defeat branch calls it with `dungeon_.eternal` and
  `eternalFloorsCleared_`; the ledger records what was actually taken.
- Deliberately unexplained in-game (owner decision): the battle's defeat line
  still says "half your gold is lost". This is the one sanctioned exception
  to the M43 "state consequences truthfully" rule, recorded here and in
  `game_design.md` §6.

### Slice 4 — Resets ask first

- `SettingsState`: **Reset settings and bindings** and **Reset tutorial
  prompts** raise the shared `ConfirmPromptState` (danger frame, cursor on
  Cancel, Cancel/Back dismisses); the work moved into `resetAll()` /
  `resetTutorial()`, run by the prompt's confirm.
- `RemapState`: **Reset keyboard/gamepad to defaults** does the same. Its
  body says what the row's per-device label cannot — *every keyboard and
  gamepad binding* — because the reset has always covered both devices.
- The banner: every list switch now goes through `SettingsState::enterMode`,
  which clears `message_`; opening a prompt clears it too. Leaving Settings
  destroys the state (it is constructed per visit), so nothing survives the
  exit. `RemapState`'s banner was already timed and dies with its state.

### Slice 5 — Credits & Licenses

- Title menu: New Game / Continue / **Credits** / Settings / Quit.
- `CreditsState`: header band, the gold credit line between crystal pips,
  and the license prose in an inset, scrollable `ui::TextViewport` (policy
  C). Up/Down scroll, Confirm or Cancel leaves.
- One source of truth: `packaging/LICENSES.txt` (it gained the "A game by
  SmettPlay." line; `tools/package.ps1` ships the same file). CMake embeds it
  at configure time as `generated/core/Licenses.hpp` from
  `packaging/Licenses.hpp.in` — a single raw string literal, guarded against
  MSVC's ~16 KB literal limit and against the delimiter, and re-run when the
  text file changes. No runtime file, no path handling, identical in Debug,
  Release and capture.
- `game/Credits.hpp` (pure): `kCreditLine` and `bodyFromLicenses()`, which
  rejoins the file's hard-wrapped paragraphs and keeps headings, `Copyright`
  lines and numbered clauses on their own lines; the file's title, underline
  and the credit sentence are dropped because the page draws them itself.
- The Controls page (`HelpState`) is no longer on the title menu; it opens
  from **Settings → Controls → Controls overview** (reachable from the title
  and both pause menus). The page and its M117 pin are unchanged.
- License audit: `cmake/Dependencies.cmake` pins raylib, nlohmann/json and
  Catch2 (dev-only); `assets/credits.md` records every texture, font and
  audio file as produced by the project's own generators. Nothing else needs
  a notice, so `LICENSES.txt` was already complete apart from the credit.

## Deviations from the plan

- The plan named `ResetAll` / `ResetTutorial` "and any other reset row
  found": the re-audit found the remap screens' **Reset to defaults** and
  covered it (the owner's "controls").
- The plan's "confirm prompt" capture became `159_settings_reset_confirm`,
  raised by the row itself through a capture hook rather than re-typed text.
- No others.

## Tests

- `[m120]` — `tests/test_content_validation.cpp` (the `mpAmount` loader
  rules, 4 sections), `tests/test_inventory.cpp` (the rider out of battle:
  gating on either bar, both caps, the both-full refusal, the unchanged
  rider-free wording), `tests/test_battle_rules_v4.cpp` (the rider in an
  ordinary battle, capped, in the King's hall with and without a King
  amount), `tests/test_ledger.cpp` (the wipe-price table: the halving, the
  boundary at four floors, scored runs never eligible, the ledger records
  the quarter).
- `[credits]` — `tests/test_credits.cpp` (new): the embedded text names
  raylib + zlib/libpng, nlohmann/json + MIT and the credit line; every
  embedded character has a glyph; the reflow's structure on a synthetic file
  and on the shipped one.
- Captures: `156_credits`, `157_credits_scrolled`, `158_settings_controls`,
  `159_settings_reset_confirm` (new); `01_title` re-renders with the Credits
  row.

## Compatibility

- **Saves / settings / scores / seeds:** untouched. No battle-rules or
  generation bump — `mpAmount` is an inert-by-default content field (the
  `minTown` / `resistPct` precedent) and only the Elixir sets it. *The owner
  may still call for a rules bump; say so and it is a one-line change.*
- **Content schema:** one additive optional item key; older data loads
  unchanged.
- **Assets / manifest:** none.

## Known limitations

- The mercy is invisible by design, so the defeat line overstates the loss
  on an eligible Eternal wipe.
- The minimap pulse is time-driven like the title phrase; the capture suite
  never shows a chart, so its look is owner-validated only.
- The Credits page shows the working credit only; a fuller credits roll is
  future work when there is more to credit.

## Documentation updated

`docs/milestones.md` (program section, ledger rows), this note,
`docs/game_design.md` (§6 Eternal mercy, the M66 chart pulse, the Elixir,
the settings/credits bullets), `docs/technical_design.md` (§61 and the
HelpState pointer), `docs/manual_test_matrix.md` (rows 243–248; row 233's
path), `docs/control_standard.md`, `docs/release_hardening_manual_checklist.md`
and `README.md` (the Controls page's new home), pointers in
`M105_eternal.md` and `M117_owner_fix_batch.md`.

## Completion report

### 1. Implementation summary

**M120 — Owner batch.** Slices 1–5 as above, all complete. Players see a
pulsing chart X, an Elixir that also restores 50 MP, confirmation prompts on
every reset with a banner that no longer lingers, and a Credits & Licenses
page where the title's Controls row was (Controls moved under Settings).
Invisible: the Eternal quarter-loss mercy. Engineering: one pure wipe-price
rule, one optional item field, a configure-time embedded license header, a
pure credits reflow.

### 2. Files changed

- **Source:** `src/states/DungeonState.cpp`, `src/content/Definitions.hpp`,
  `src/content/ContentLoader.cpp`, `src/battle/Battle.cpp`,
  `src/game/ItemUse.hpp`, `src/game/Ledger.hpp`, `src/game/Credits.hpp`
  (new), `src/states/SettingsState.{hpp,cpp}`, `src/states/RemapState.cpp`,
  `src/states/CreditsState.{hpp,cpp}` (new), `src/states/MainMenuState.cpp`,
  `src/editor/CategoryDescriptors.cpp`, `src/capture/CaptureRunner.cpp`.
- **Build / packaging:** `CMakeLists.txt`, `packaging/Licenses.hpp.in`
  (new), `packaging/LICENSES.txt`, `tests/CMakeLists.txt`.
- **Data:** `data/items.json` (the Elixir).
- **Tests:** `tests/test_credits.cpp` (new), `tests/test_inventory.cpp`,
  `tests/test_content_validation.cpp`, `tests/test_battle_rules_v4.cpp`,
  `tests/test_ledger.cpp`.
- **Docs:** see "Documentation updated".

### 3. Plan deviations

See "Deviations from the plan" — the remap reset was added to the
confirmation slice; nothing needs approval beyond the plan.

### 4. Compatibility

See "Compatibility" — nothing breaks; no version moves.

### 5. Automated validation

All run 2026-09-18 from the VS 2022 developer shell (amd64), in this order:

- `cmake --preset msvc-debug` + `cmake --build --preset debug` - **succeeded**
  (game, CrystalForge and tests; the configure step generated
  `core/Licenses.hpp`).
- `crystal_tests.exe "[m120],[credits],[itemuse],[help],[editor]"` -
  **33 test cases, 8882 assertions, all passed**.
- `CrystalForge.exe --canonicalize` - **0 files rewritten, 0 content
  errors** (the hand-edited `items.json` was already canonical).
- `ArePGeese.exe --capture <dir>` - **159/159 scenes clean** (155 + the four
  new ones); `156_credits`, `157_credits_scrolled`,
  `159_settings_reset_confirm` and `01_title` were read back by eye.
- `ctest --preset debug` - **908/908 passed** (663 s; it shared the machine
  with the Release build).
- `cmake --preset msvc-release` + `cmake --build --preset release` -
  **succeeded**.
- `ctest --preset release` - **904/904 passed** (477 s).

Not run: `tools\package.ps1` (no packaging was asked for; it stages the same
`packaging/LICENSES.txt`, now with the credit line).

### 6. Manual owner validation

Matrix rows **243–248** (and row 233 for the Controls page's new path):
the pulse's visibility and pace; the Elixir in battle and from the bag; the
mercy on floor 5+ versus floors 1–4 and scored dungeons; the three reset
prompts and the banner's lifetime; the Credits page's readability at 1x and
under the CRT filter; the Controls overview from the title and both pause
menus.

### 7. Known limitations

See "Known limitations".

### 8. Documentation updated

See "Documentation updated".

### 9. Final status

`complete (approved 2026-09-20)`
