# Presentation Audit — Defect Register (M11)

> **HISTORICAL — not an open defect list.** This register captured the M11
> baseline. The milestones that consumed it (M12–M22) are all
> `complete (approved)`; see `docs/milestones.md` for current status and
> `docs/manual_test_matrix.md` for what still needs an owner pass. Do not plan
> new work from the rows below without re-verifying against the current build.
>
> **M12 update (2026-07-19):** defects carry a `Fixed (M12)` / `Partial
> (M12)` marker where the text-safety milestone addressed them; verification
> is the owner's manual pass (rows in `docs/manual_test_matrix.md`).
> Post-migration evidence: `docs/screenshots/m12_after/`.
>
> Audited commit: `827187124f9123c7275abcbbb45d5a4aabedf92b` (2026-07-19).
> Build: MSVC 19.51 / Ninja, Debug. Tests: 125/125 passed. Executable launched
> and exited cleanly (code 0).
>
> **Evidence classes:** `[observed]` = seen in a live capture or verified at
> runtime this session; `[static]` = derived from source geometry/logic and
> not yet visually confirmed. Static findings with exact-arithmetic geometry
> are marked `[static-certain]`.
>
> Screenshots: `docs/screenshots/m11_baseline/` (7 captures at the default
> 1278×720 window; native-426×240 capture requires the M23 tooling — not
> available at this baseline). Remaining screens are covered by the owner
> checklist in `docs/manual_test_matrix.md`.

## 1. Screen and flow inventory

Every player-facing state, its entry path, dynamic inputs, and known risks.
"Shot" = baseline screenshot exists.

| # | Screen / state | Source | Entry path | Dynamic inputs | Shot | Key risks (defect IDs) |
|---|---|---|---|---|---|---|
| 1 | Title / main menu | `MainMenuState` | launch | content counts; Continue enabled iff any save | 01, 01b | UI-TEXT-010, UI-LAYOUT-009 |
| 2 | Controls / help | `HelpState` | title → Controls | static table | 02 | CTRL-006, CTRL-007, CTRL-021 |
| 3 | Party creation | `PartyCreationState` | title → New Game | class list from data; names ≤12 chars | 03 | UI-INPUT-001 |
| 4 | Name editing (modal) | `PartyCreationState` (editing) | party creation → Confirm on row | typed name, caret blink | 04 | UI-INPUT-001 |
| 5 | No-classes fallback | `PartyCreationState` (!ready) | New Game with broken `classes.json` | — | — | none (message fits) |
| 6 | Save slots (Save mode) | `SlotMenuState` | town → Save Point | 3 slots: empty/valid summaries | — | CTRL-011, UI-INFO-012a |
| 7 | Load slots (Continue) | `SlotMenuState` | title → Continue | 4 slots incl. autosave; disabled empties | — | UI-INFO-012a |
| 8 | Town exploration | `TownState` | new game / load / retreat | party names in HUD; door prompts | 05 | UI-TEXT-008, UI-LAYOUT-009, CTRL-007 |
| 9 | Town pause menu | `TownMenuState` | town → Menu/Cancel | — | 06 | CTRL-011 |
| 10 | Inn | `InnState` | town → Inn door | party rows (≤4), name/level/HP/MP | — | UI-TEXT-017 |
| 11 | Item shop | `ItemShopState` | town → Item Shop | 8 consumables + owned counts | — | UI-INFO-004, UI-TEXT-002 (future growth) |
| 12 | Equip shop: menu | `EquipShopState` (Menu) | town → Equip Shop | party emptiness | — | — |
| 13 | Equip shop: buy list | `EquipShopState` (Buy) | equip shop → Buy Gear | **25 equippable items** | — | **UI-TEXT-001**, UI-INFO-013 |
| 14 | Equip shop: char/slot/item | `EquipShopState` (3 phases) | equip shop → Equip Party | party, slots, inventory gear | — | UI-INFO-013 |
| 15 | Training hall | `TrainingHallState` | town → Training Hall | names/levels/costs; max-level disable | — | UI-TEXT-017 |
| 16 | Guild / dungeon select | `GuildState` | town → Guild | 3 themes; depth 1–20; 20-digit seed | — | CTRL-007 |
| 17 | Scoreboard | `ScoreboardState` | town → Scoreboard | 0..14+ entries, theme names | — | UI-TEXT-012, UI-INFO-012b |
| 18 | Dungeon exploration | `DungeonState` | guild → Enter Dungeon | room map, markers, danger labels, HUD, minimap | — | ROOM-015, UI-TEXT-008, UI-INFO-005 |
| 19 | Encounter prompt (gate/guard/boss) | `DungeonState` footer | face a team marker | tier, count, tags | — | **UI-INFO-005**, CTRL-007 |
| 20 | Chest prompts (open/guarded/empty) | `DungeonState` footer | stand on chest tile | rarity, gold, item name | — | — |
| 21 | Dungeon pause / retreat | `DungeonMenuState` | dungeon → Menu/Cancel | — | — | CTRL-011 (lesser: hint exists) |
| 22 | Battle: intro + telegraph | `BattleState` (Intro) | confirm on team | boss telegraph ≤55 chars | — | — |
| 23 | Battle: command menu | `BattleState` (Command) | intro → Confirm | 5 commands, disabled states | — | UI-INFO-004 |
| 24 | Battle: skill list | `BattleState` (ChooseSkill) | command → Skill | up to 5+ skills, MP costs | — | **UI-TEXT-002**, UI-INFO-004 |
| 25 | Battle: item list | `BattleState` (ChooseItem) | command → Item | up to 8 consumable types | — | **UI-TEXT-002**, UI-INFO-004 |
| 26 | Battle: target select | `BattleState` (ChooseTarget) | skill/attack/item | 1–5 enemies / 4 party | — | UI-LAYOUT-003 |
| 27 | Battle: resolve/messages | `BattleState` (Resolve) | after action | action messages, floating numbers, statuses | — | UI-LAYOUT-003 (status rows) |
| 28 | Battle: outcome | `BattleState` (Done) | victory/defeat/escape | outcome text | — | UI-INFO-014 (defeat) |
| 29 | Dungeon result / score | `DungeonResultState` | boss victory | 7–8 breakdown lines | — | UI-LAYOUT-018 |
| 30 | Defeat return (game over path) | via `DungeonState::onResume` | battle defeat | gold halved, heal, to town | — | **UI-INFO-014** |
| 31 | Degraded content startup | `Application` + title | malformed `data/` | reduced counts on title | — | — (logged, no crash — verified by tests) |

Maximum-content cases that drive the risks above: 12-char names ×4; 25-item
buy list; 5-enemy battles; multi-status combatants; 20-digit seeds; 55-char
descriptions/telegraphs (never rendered today — see UI-INFO-004); 14+
scoreboard entries; 999999 gold.

## 2. Defect register

Severity: **Blocker** / **High** / **Medium** / **Low** (definitions in
`docs/milestone_notes/M11_completion_baseline.md` §F). "Owner" = subsystem;
"→" = recommended owning milestone/slice.

### Blocker

**UI-INPUT-001 — Gamepad-only player soft-locks in name editing** `[static]`
**Fixed (M13):** text entry flows through the input layer; A/B/Enter/Esc all
finish editing, X/Backspace deletes; typing itself still needs a keyboard
(labeled in-game).
- Screen: party creation, editing mode. Repro: with only a gamepad, Confirm
  (A) on a name row → editing starts; editing polls raw
  `GetCharPressed`/`IsKeyPressed(KEY_BACKSPACE/ENTER/ESCAPE)` only
  (`PartyCreationState.cpp:94-105`), so no gamepad button types or exits.
- Expected: gamepad can exit editing (and ideally edit via an on-screen
  keyboard, or editing is keyboard-labeled and skippable).
- Frequency: every gamepad-only new game that touches a name row. Modes:
  gamepad-only.
- Owner: input / `PartyCreationState`. → **M13-1** (raw-input removal). If the
  owner wants an interim mitigation earlier, it needs separate approval (M11
  is no-code).

### High

**UI-TEXT-001 — Equip-shop buy list overflows the screen** `[static-certain]`
**Fixed (M12):** scrolling list (9 visible rows, arrows, selection followed).
- Screen: equip shop, Buy phase. 25 equippable items × 14px from y=60 ends at
  y≈410 — 170px past the 240px screen. ~12 rows visible; the cursor wraps
  through invisible rows (`Menu::step` wraps; `drawMenu` draws all).
- Expected: list viewport with scrolling and selection always visible.
- Frequency: every visit. Modes: all. Owner: `ui::drawMenu`/`EquipShopState`.
- → **M12-b** (list viewport); migration slice for shops in M12-d.

**UI-TEXT-002 — Battle skill/item lists overflow the command panel**
`[static-certain]`
**Fixed (M12):** panel redesign — lists scroll in 4 rows; see also
UI-TEXT-024 below (command menu itself was off-screen).
- Screen: battle, ChooseSkill/ChooseItem. Panel is 60px (y=176..236); lists
  start panelY+20 with 11px rows → 4th+ row draws below the panel edge; 6th+
  approaches screen bottom. Characters can know 4–6+ skills (starting skills +
  scrolls); 8 consumable types exist.
- Expected: scrolling list constrained to the panel, or a paged panel.
- Frequency: common mid-game. Modes: all. Owner: `BattleState`.
- → **M12-b/M12-d** (battle family), coordinated with M18.

**UI-LAYOUT-003 — 5-enemy battles collide with the command panel**
`[static-certain]`
**Fixed (M12):** 5-enemy columns start at y=20 (turn counter moved
top-right); enemy statuses render beside the unit instead of below it.
- Screen: battle field. Enemy rows at y=36+row·34; row 5 occupies y=172..188
  plus name (y=163) and status line (y=196) vs. panel top y=176.
- Expected: unit layout fits any legal encounter (enemy team size 1–5).
- Frequency: every 5-enemy encounter. Modes: all. Owner: `BattleState`.
- → **M12-d**/M18 (battle layout).

**UI-INFO-004 — Item/skill descriptions exist in data but are never shown**
`[static]` (verified: zero render sites reference `description`)
**Partial (M12):** descriptions now shown for the selected entry in the
battle skill/item lists and both shops (plus gear slot/stat summary). Full
contextual Details help remains M22.
- 36 items and 28 skills carry authored descriptions (≤55 chars); no screen
  renders them. Players buy gear and cast skills blind; MP cost appears only
  in the battle skill list.
- Expected: a Details/description surface in shops, inventory and battle.
- Owner: UI/states. → **M12-d** (description line in migrated lists) and
  **M22** (full Details help). Feeds CTRL standard (Details action, M13).

**UI-INFO-005 — Encounter display violates the design contract** `[static]`
**Fixed (M12):** the fight prompt now includes the team name (name + tier +
count + tags — the full game_design §6 contract).
- `docs/game_design.md` §6: visible teams show **team name**, danger, count,
  tags. The dungeon prompt shows tier + count + tags only
  (`DungeonState.cpp:516`); the team's generated name and member list are
  shown nowhere (M4's `EncounterPreviewState` was removed in M5).
- Expected: name + composition visible before Confirm (prompt or preview).
- Owner: `DungeonState`/design. → **M12-d** (dungeon family) with the owner
  deciding the presentation form; battle-side identity belongs to M18.

**CTRL-006 — "Left Stick" navigation is advertised but not implemented**
`[static]` (verify with a controller)
**Fixed (M13):** real analog support with hysteresis (0.5/0.35); docs/Help
re-advertise the stick honestly. Owner controller pass confirms.
- Help (`HelpState.cpp:19`) and README claim D-Pad / Left Stick; `InputMap`
  binds D-pad buttons only and `InputQuery` has no axis callbacks at all.
- Expected: stick navigation works (M13) or docs stop claiming it (interim).
- Modes: gamepad. Owner: input. → **M13-3** (deadzone/hysteresis axis
  support). Interim doc correction is an owner call.

**CTRL-007 — Prompts name abstract actions, not keys, and are hard-coded**
`[observed]` (shots 03, 05, 06)
**Fixed (M13):** every footer/hint derives from the live bindings via
`input::prompt` ("[Tab] Pause"); the Controls page is generated from the
`InputMap` and reflects remaps.
- "Menu: Pause" (town/dungeon footer) never says Tab/Start; every footer
  string is a hard-coded literal; after M13 remapping they would all lie.
- Expected: binding-derived labels for the active device.
- Owner: all states. → **M13-5**; string sites migrate with M12-d families.

### Medium

**UI-TEXT-008 — HUD backdrops are fixed-width; text can spill past them**
`[static]` (partially observed in shot 05)
**Fixed (M12):** town and dungeon HUD backdrops sized from measured text.
- Town party HUD and dungeon HUD draw a 150px backdrop, then unbounded text
  ("Party:" + four 12-char names ≈ 220px; theme+depth+gates line similar).
- Expected: backdrop sized from measured text (or text clipped to it).
- Owner: `TownState`/`DungeonState`. → **M12-a/M12-d**.

**UI-LAYOUT-009 — Debug overlay overlaps the party HUD and is on by default**
`[observed]` (shots 01, 05: overlay covers "Party:"/HUD corner)
**Fixed (M12):** overlay starts hidden; F1 still toggles it in debug builds
(overlap when toggled on is accepted as debug-only; Release exclusion is the
M24 check).
- Debug builds start with the overlay visible (`Application::debugOverlay_`),
  stacked on the same top-left corner as gameplay HUDs.
- Expected: overlay off by default; no overlap with gameplay HUD; excluded
  from Release (verify at M24).
- Owner: `core/Application`. → **M12-d** (HUD placement); default-off is a
  one-line owner-approved tweak best bundled with M12-a.

**UI-TEXT-010 — Stale "Milestone 8 - Presentation" label on the title**
`[observed]` (shot 01; `MainMenuState.cpp:105`)
**Fixed (M12):** label removed (content counts kept as load feedback).
- Also: raw content counts on the title footer are developer diagnostics.
- Expected: version/build string sourced from one place, or removed.
- → **M12-d** (title family). Trivial, but M11 changes no code.

**CTRL-011 — Destructive actions have no confirmation** `[static+observed]`
- Save-slot overwrite (SlotMenu Save) and town-menu "Quit to Title" (drops
  unsaved progress, shot 06) execute on a single Confirm. Dungeon Retreat at
  least shows a consequence hint.
- Expected: confirm step or undo for destructive actions.
- → **M22** (error/destructive-action handling); note for M12-d layouts to
  reserve space for confirm rows.

**UI-TEXT-012 — Scoreboard: misaligned columns, hard cap, missing context**
`[static]`
**Fixed (M12)** for (a) columns (right-aligned numerics), (b) cap (free
scrolling with range indicator), and depth display; seed display and the
comparability *policy* remain M19.
- (a) Column alignment uses space padding with a variable-width font (also
  `SlotMenuState` summaries — 012a); (b) display caps at 14 rows with no
  scroll or "more" indicator; (c) rows omit depth and seed (012b), so scores
  from depth 1 and depth 20 look identical in context — score-comparability
  gap that **M19** must resolve (display side: M12-d).
- Owner: `ScoreboardState`. → **M12-b/M12-d**; policy in **M19**.

**UI-INFO-013 — Gear is bought without seeing its stats** `[static]`
**Fixed (M12):** selected gear shows slot + stat bonus + description in the
shop detail line (side-by-side compare remains M22).
- Equip-shop Buy shows name+price only; no slot, no stat bonus, no compare.
- → **M12-d** (shop family detail line); full compare UI is **M22** Details.

**UI-INFO-014 — Defeat consequences are never communicated** `[static]`
- On defeat: battle says "The party has fallen..."; then the game silently
  heals the party, halves gold, and returns to town
  (`DungeonState::onResume`), with no game-over screen and no mention of the
  gold loss or run score forfeiture.
- Expected: an explicit defeat/return summary (mirror of `DungeonResultState`).
- **Fixed (M18):** the defeat outcome message now states the consequences
  (carried to town, half gold lost, run forfeit) before the return.
- → **M18** (battle presentation) or **M12-d** dungeon family — owner call.

**ROOM-015 — Rooms fill the whole exploration screen** `[observed+static]`
- 26×15×16px = 416×240 of 426×240 (10px dead band right). Known M16 driver;
  recorded here for completeness with severity tied to the roadmap.
- → **M16** (no earlier action).
- **Fixed (M16):** rooms are now compact archetype layouts (5–19 × 5–13
  tiles) realized from derived room-local seeds and drawn centered in the
  exploration viewport; captures in `docs/screenshots/m16_rooms/`.

**ART-016 — Placeholder art everywhere** `[observed]` (all shots)
- Rectangles/glyphs for actors, tiles, markers; code-generated colors.
- → **M15/M17** (no earlier action). Not inflated above Medium: readable, but
  it is the program's core quality gap.
- **Largely fixed (M17):** exploration is fully arted — animated player,
  tier-silhouette team markers, all three theme tile sets with accents,
  facing brackets (`docs/screenshots/m17_explore/`). Remaining: battle-scene
  presentation (M18) and final audio (M21).
- **Enabler (M14):** the asset manifest now makes every texture/font/audio
  role replaceable without C++ (proven live by `sfx.ui.confirm` shipping as a
  file); producing the real art/audio remains M15/M17/M21.
- **Partial (M15):** the vertical slice ships generated pixel art for the
  town, Ruined Keep, player, battle actors/enemies/boss, markers, UI frame,
  emblem, and three music loops (`docs/screenshots/m15_slice/`); direction
  pending the owner gate. Crystal Mine / Hollow Forest and per-enemy art
  remain M17.

### Low

**UI-TEXT-017 — Space-padded pseudo-columns in Inn/Training Hall** `[static]`
- `%-12s`/`%-10s` with a variable-width font; 11–12-char names break the pad
  width outright. → **M12-d** (town-services family).
- **Fixed (M12):** Inn uses real columns; Training Hall drops the padding.

**UI-LAYOUT-018 — Dungeon-result panel is 2px from overflow** `[static]`
- The 8-line case (escape penalty shown) ends at y≈172 vs footer at 174
  inside the 190px panel. Any added line overlaps. → **M12-d** (result
  family).
- **Fixed (M12):** row pitch 13 leaves 10px clearance in the 8-line case.

**UI-019 — Minimap visited state is alpha-only** `[static]`
- Unvisited rooms differ only by transparency; weak signal, no legend.
  → **M12-c** conventions / **M17** visuals.

**UI-020 — Title menu block sits off-center** `[observed]` (shot 01)
- Left-aligned labels at `w/2-40` read as off-center against the centered
  title. → **M12-d** (title family).
- **Fixed (M12):** centered on the measured widest label.

**UI-TEXT-024 — Battle command menu drew its last rows off-screen**
`[static-certain]` — *discovered during M12 implementation; missed by the
M11 audit (UI-TEXT-002 understated it).*
- At the reviewed baseline the 5-command menu rendered at panelY+22 with
  12px rows: "Guard" was clipped at the screen edge and "Escape" started at
  y=246 — entirely below the 240px screen — while remaining selectable via
  cursor wrap. Severity retroactively **High**.
- **Fixed (M12):** the command menu renders at panelY+5 with 11px rows; all
  five rows sit inside the panel.

**CTRL-021 — Dead `Pause` binding; Help omits it** `[static]`
- `InputAction::Pause` (P / gamepad Select) is bound and consumed nowhere;
  Help doesn't list it. → **M13-2** (action vocabulary cleanup).
- **Fixed (M13):** the Pause action was removed; Menu is the single
  pause/menu action.

**CTRL-022 — Battle lists treat Left/Right as Up/Down** `[static]`
- `up = MoveUp||MoveLeft` in every battle menu; harmless but inconsistent
  with list semantics elsewhere. → **M13** control standard.
- **Fixed (M13):** aliases removed; vertical lists use Up/Down only, and
  Left/Right stay reserved for value adjust.

**DATA-023 — No manual seed entry at the Guild** `[static]`
- Seeds can only be rerolled, never typed; sharing a seed with another player
  (or reproducing a bug) requires luck. Owner decision whether to add
  (suggest alongside **M13** text-input rework or **M20**).

## 3. Severity summary

| Severity | Count | IDs |
|---|---|---|
| Blocker | 1 | UI-INPUT-001 |
| High | 7 | UI-TEXT-001, UI-TEXT-002, UI-LAYOUT-003, UI-INFO-004, UI-INFO-005, CTRL-006, CTRL-007 |
| Medium | 9 | UI-TEXT-008, UI-LAYOUT-009, UI-TEXT-010, CTRL-011, UI-TEXT-012, UI-INFO-013, UI-INFO-014, ROOM-015, ART-016 |
| Low | 7 | UI-TEXT-017, UI-LAYOUT-018, UI-019, UI-020, CTRL-021, CTRL-022, DATA-023 |

By evidence: 8 observed (screenshot-backed), 3 static-certain (exact
geometry), 13 static (code-derived, owner/manual confirmation pending).

## 4. Recommended M12-a slice (per M11 note §F, slice M11-f)

The audit **confirms the default recommendation** with one addition:

> **M12-a:** central font-aware text measurement, bounded wrapping,
> long-token handling, overflow diagnostics; migrate **Help** plus the
> **battle bottom panel** as the representative "modal" (it owns three of the
> four geometry-certain overflows: UI-TEXT-002 partially, and sets up
> UI-LAYOUT-003), and bundle the two one-line fixes the owner may approve
> alongside it (UI-TEXT-010 stale label, UI-LAYOUT-009 overlay default-off).

Evidence for keeping text-measurement first: 3 geometry-certain overflow
defects (UI-TEXT-001/002, UI-LAYOUT-003) and 3 unbounded-text patterns
(UI-TEXT-008/012/017) all reduce to the same missing primitives — measured
bounds + list viewport. No prerequisite more severe than this was found; the
sole Blocker (UI-INPUT-001) is an input defect whose real fix is scheduled
with M13 and does not depend on M12 internals.
