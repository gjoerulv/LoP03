# M25 — UI Corrections and Battle HUD

## A. Status and authority

- **Status:** implemented, awaiting manual approval
- **Last reviewed repository commit:** `a9960b4` (working tree clean; code
  unchanged since `aa5a8aa`). Re-audited 2026-07-20 against the current
  checkout: every code reference in §B/§F/§G was confirmed still accurate, and
  the build/test/capture baseline was reproduced green (252 tests, 22/22
  capture scenes overflow-clean).
- **Authority:** `CLAUDE.md` > this note > `docs/milestones.md` >
  `docs/game_design.md` > `docs/technical_design.md`.
- **Authorization:** the polish program (M25–M30) was authorized on 2026-07-20;
  the owner authorized beginning M25 implementation this session, including the
  added font-replacement slice and the three decisions recorded in §A.1.

### A.1 Amendment 2026-07-20 — font replacement slice + owner decisions

This note was amended before implementation to add **Slice 0 — font
replacement** (§F.0) and to record three owner decisions taken by Q&A this
session:

1. **Font sourcing.** Replace raylib's default font with an **original bitmap
   font** generated in-project by a new `tools/asset_gen/generate_font.ps1`
   (PNG atlas pages + AngelCode BMFont `.fnt` descriptors), delivered through
   the existing manifest `AssetType::Font` support and a centralized
   `DrawTextEx` path in `ui/UiDraw`. No external/downloaded font file; this
   matches the in-project generated-sourcing policy from M15/M17/M21.
2. **Version stamp gating.** The **title-screen version stamp stays in Release**
   (players reporting bugs need a version). The separate **diagnostic content
   line** (`Content: N classes …`) is removed from Release via the existing
   `CRYSTAL_SHIPPING_BUILD` guard.
3. **Slice ordering.** Slice 0 lands first and the `--capture` run must be
   overflow-clean **before** slices 1–4 begin — the new font changes every text
   width, so layout fixes made against the old font would be wasted.

Hard constraints for the session: no git operations; battle and simulator
tests pass **unmodified** (M25 is zero simulation risk); the default-font
fallback must keep working (a missing font asset must never crash); do **not**
add a value field to `ui::MenuItem` without escalating.

## B. Problem statement

Four independent readability defects reported by the owner. All are presentation
only: none of them touches the battle simulation, dungeon generation, scoring,
or save data.

1. **Title screen text overlaps.** The version stamp is drawn at `(4, h-12)` at
   font size 8; the content-summary line is drawn at `(6, h-16)` at font size
   10 (`src/states/MainMenuState.cpp`). Their rectangles intersect on both axes.
   The content line is also drawn unconditionally — it is developer diagnostic
   information that should not appear in a Release build at all.
2. **Battle name labels bury the sprites.** `BattleState.cpp` draws
   `c.name` above every combatant on both sides, every frame, unconditionally.
   With a full party and five enemies this is ten labels competing with the art
   the player is trying to read.
3. **MP has no numerals.** Party members get an MP *bar*, but the only numeric
   readout is HP (`%d/%d` of `shownHp`/`maxHp`). MP is the resource that gates
   skill use, so its exact value matters more than a bar conveys.
4. **Guild values are detached from their controls.** `GuildState::render`
   draws Theme at y=78, Depth at y=96, and Seed at y=114, while the menu whose
   rows adjust them is drawn at y=140. The player reads a value roughly 60px
   above the row they are changing.

## C. Goals

- Remove every unintended text overlap on the title screen, and keep developer
  diagnostics out of Release builds.
- Make battle sprites the primary visual element, with identifying information
  available on demand rather than permanently overlaid.
- Show MP with the same numeric clarity as HP.
- Put adjustable values next to the controls that adjust them.

## D. Non-goals

- Any change to targeting *rules*, enemy AI, or battle resolution — that is M28.
  This milestone changes what is drawn, never what is decided.
- New art or assets (M26/M27).
- Restyling screens beyond the four defects above.
- Changing the 426×240 virtual resolution.

## E. Dependencies

None. M25 can begin immediately.

**M28 depends on this milestone.** An enmity model is unreadable if the player
cannot inspect a target, so the target-info panel introduced here is a
prerequisite for the AI work, not a parallel nicety.

## F. Proposed slices (provisional — refine before starting)

0. **Font replacement (lands first; capture must be overflow-clean before
   slices 1–4).**
   - **Asset generation.** New `tools/asset_gen/generate_font.ps1` (deterministic,
     System.Drawing, byte-identical reruns, same style as
     `generate_textures.ps1`) draws an **original** proportional pixel glyph set
     for printable ASCII 32–126 (95 glyphs, 5×7 body in an 8-tall cell with
     descenders) and emits:
     - `assets/fonts/font_atlas.png` (×1) shared by two `.fnt` descriptors —
       `font_small.fnt` (`lineHeight`/base 8) and `font_main.fnt`
       (`lineHeight`/base 10);
     - `assets/fonts/font_atlas_2x.png` (×2, nearest-neighbour of the same
       glyphs) with `font_title.fnt` (`lineHeight`/base 20).
     - **Why three descriptors from one design.** raylib's `LoadBMFont` sets
       `font.baseSize = lineHeight`, and `DrawTextEx` scales glyphs by
       `requestedSize / baseSize`. Three base sizes keep the game's dominant
       sizes crisp (8 → small ×1.0, 10 → main ×1.0, 20 → title ×1.0) while
       intermediate sizes scale from the nearest base with `TEXTURE_FILTER_POINT`
       (blocky, on-brand). The atlases share one glyph design so the type reads
       as a single original typeface at three scales.
   - **Manifest.** Three `AssetType::Font` rows — `font.ui.small` (fontSize 8),
     `font.ui.main` (fontSize 10), `font.ui.title` (fontSize 20). Provenance in
     `assets/credits.md`.
   - **ResourceManager.** `LoadFontEx` handles TTF/OTF only; extend
     `ResourceManager::font` to dispatch by extension (`.fnt` → `LoadFont`,
     `.ttf/.otf` → `LoadFontEx`), set `TEXTURE_FILTER_POINT`, and keep the
     default-font fallback (guard against BMFont's internal
     revert-to-default so the shared default font is never wrapped/unloaded).
   - **Rendering centralization.** `ui::setFonts(small, main, title)` installs
     the active fonts once after each manifest (re)load; before that (and when a
     font is missing) the wrappers fall back to `GetFontDefault()`. UiDraw's 13
     internal `DrawText`/`MeasureText` calls and the ~28 direct call sites in
     `states/*.cpp` and `core/Application.cpp` route through
     `ui::drawText` / `ui::drawTextCentered` / `ui::drawTextRight` /
     `ui::measureText` (all `DrawTextEx`/`MeasureTextEx`, one spacing convention
     in one place). `ui::lineHeight(fontSize) = fontSize + 3` is retained; the
     capture overflow check is the arbiter for any metric change.
   - **Wiring.** `ui::setFonts` is called from `Application::loadAssets` (startup
     and F5 reload) and from `CaptureRunner` after `resources.reload()`, so the
     capture overflow check validates the real font.

1. **Title screen.** Give the version stamp and content line non-overlapping
   rectangles using measured bounds (`ui::measureText`), not adjusted constants.
   Gate the content line behind the existing `CRYSTAL_SHIPPING_BUILD` guard so
   Release omits it. Decide with the owner whether Release keeps the version
   stamp — recommendation: **yes**, players reporting bugs need a version.
2. **Battle target info.** Remove the unconditional name label. Add a target
   panel showing name plus the basic stats needed to judge a target, rendered
   only for the currently targeted unit. Must reuse the M12 fitted/wrapped text
   helpers so it participates in overflow diagnostics. Placement must not
   collide with the existing status-effect lines, which already differ by side
   (party statuses below, enemy statuses to the right).
3. **MP numerals.** Add current/max MP text for party members alongside the
   existing HP numerals, within the same layout budget.
4. **Guild inline values.** Compose Theme/Depth values into their menu labels on
   rebuild, following `SettingsState::volumeLabel`. Note that `ui::MenuItem`
   holds only `label` and `enabled` — there is no value field, and adding one is
   a wider UI change that should be escalated rather than assumed. Seed is not
   an adjustable row and can stay a separate readout.

## G. Expected systems affected

- `tools/asset_gen/generate_font.ps1` — **new** original bitmap-font generator.
- `assets/fonts/*` + `assets/manifest.json` + `assets/credits.md` — font assets,
  three manifest rows, provenance.
- `src/resource/ResourceManager.cpp` — `.fnt`/TTF dispatch + `TEXTURE_FILTER_POINT`.
- `src/ui/UiDraw.{hpp,cpp}` — `ui::setFonts` registry, `ui::drawText`, and
  `DrawTextEx`/`MeasureTextEx` conversion of every wrapper and menu draw.
- `src/core/Application.cpp` — install fonts after each manifest load; migrate
  the debug-overlay text.
- `src/states/*.cpp` — migrate direct `DrawText`/`MeasureText` sites to the
  wrappers (BattleState, GuildState, MainMenuState, PartyCreationState,
  ScoreboardState, DungeonResultState, EquipShopState, InnState,
  TrainingHallState).
- `src/states/MainMenuState.cpp` — title layout, Release gating (slice 1).
- `src/states/BattleState.cpp` — label removal, target panel, MP numerals (2/3).
- `src/states/GuildState.cpp` — label composition (slice 4).
- `src/capture/CaptureRunner.cpp` — install fonts; add a battle-targeting scene
  so the new target panel is covered by the overflow check.
- Possibly `src/ui/Menu.hpp` **only if** the owner approves a value field;
  default is to avoid it.

## H. Data, schema, and save compatibility

No changes. No content schema, save format, settings field, or scoreboard field
is touched. No generation-version or battle-rules-version implication.

## I. Automated validation

- Full suite green (current baseline: 252 tests).
- `--capture` run stays overflow-clean, extended to cover a targeting state.
- Presentation lint continues to pass.

## J. Manual validation for the owner

- Title screen: no overlap; version legible; content line present in Debug and
  absent in Release.
- Battle with a full party and five enemies: sprites readable; target info
  appears only for the targeted unit and is legible; status lines uncollided.
- MP numerals correct and readable, including at max and at zero.
- Guild: Theme/Depth values sit beside their rows and update as you adjust.

## K. Acceptance criteria

- No unintended text overlap in any capture scene at 426×240.
- Content diagnostics absent from a Release build.
- No permanent name labels above combatants; target information reachable.
- Current/max MP visible as numerals for every party member.
- Guild values adjacent to the controls that change them.
- No change to any battle outcome: existing battle and simulator tests pass
  **unmodified**.

## L. Risks and failure modes

- **Target panel crowding.** The battle screen is already dense. If the panel
  cannot fit without collision, escalate rather than shrinking text below the
  established minimum size.
- **Removing names loses information.** If targeting alone proves insufficient
  to identify enemies in play, the fallback is a name shown on the acting unit
  as well — an owner call after seeing it, not a preemptive hedge.
- **Scope creep into restyling.** Four specific defects. A screen that merely
  looks dated is not in scope.

## M. Required documentation updates on completion

- This note: as-implemented record, deviations.
- `docs/milestones.md`: status → `implemented, awaiting manual approval`.
- `docs/ui_style_guide.md`: typography roles now backed by the real bitmap font
  (supersede the "raylib default font" wording); target-panel and inline-value
  conventions.
- `docs/asset_pipeline.md`: font generation + BMFont manifest usage.
- `assets/credits.md`: font provenance row.
- `README.md`: known-limitations line (generated fonts join the generated
  art/audio) if warranted.
- `docs/control_standard.md`: only if prompt or action semantics change.
- `docs/manual_test_matrix.md`: rows for the font swap and the four defects.

## N. As-implemented record (2026-07-20)

Implemented on top of `a9960b4`. Build/tests/capture all reproduced green in a
VS2022 developer shell:

- Debug build (`cmake --build --preset debug`) and Release build
  (`cmake --build --preset release`) both link the exe and tests with no
  project warnings.
- Full suite: **252/252** (`ctest --preset debug`) — battle and simulator tests
  unmodified, as required (zero simulation risk).
- Capture: **23/23 scenes overflow-clean** (`--capture`), including the new
  `23_battle_targeting` scene.
- `tools/asset_gen/generate_font.ps1` reruns byte-identical (5 files).

### Slice 0 — font replacement

- New generator `tools/asset_gen/generate_font.ps1` emits an original 5×7
  proportional glyph design (printable ASCII 32–126) as `assets/fonts/`:
  `font_atlas.png` (×1) shared by `font_small.fnt` (base 8) and `font_main.fnt`
  (base 10), plus `font_atlas_2x.png` with `font_title.fnt` (base 20). Three
  manifest `font` rows (`font.ui.small/main/title`, key `size`). Provenance in
  `assets/credits.md`.
- `ResourceManager::font` dispatches `.fnt`→`LoadFont`, `.ttf/.otf`→`LoadFontEx`,
  forces `TEXTURE_FILTER_POINT`, and guards against BMFont's revert-to-default
  so the shared default font is never wrapped/unloaded. Default-font fallback
  verified (missing `.fnt` → default font, no crash).
- `ui::setFonts` registry + `ui::drawText`; UiDraw's internals and every direct
  `DrawText`/`MeasureText` site in `states/*.cpp` and `core/Application.cpp`
  route through `DrawTextEx`/`MeasureTextEx` (spacing 0, baked into glyph
  advance). Installed from `Application::loadAssets` (startup + F5) and
  `CaptureRunner` after `resources.reload()`.

### Slices 1–4

1. **Title** (`MainMenuState`): version stamp and content line on separate rows
   (no overlap); content line wrapped in `#ifndef CRYSTAL_SHIPPING_BUILD`
   (absent from Release); version stamp kept in Release.
2. **Battle target panel** (`BattleState`): removed the always-on per-sprite
   name label; the `ChooseTarget` phase renders a target-info panel (name,
   HP + MP for allies, ATK/MAG/DEF/SPD, statuses) for the targeted unit in the
   bottom panel via the M12 fitted helpers, clear of the status lines.
3. **MP numerals** (`BattleState`): party rows show `HP c/m` and `MP c/m`
   numerals (HP gained an `HP ` prefix for parity with the new MP line).
4. **Guild inline values** (`GuildState`): `rebuild()` composes Theme/Depth
   values into their menu labels (the `SettingsState::volumeLabel` idiom); the
   seed is a separate readout. `ui::MenuItem` unchanged (no value field).

### Deviations from the plan

- **Three font descriptors, not two.** The plan named `font.ui.main` /
  `font.ui.small` "e.g."; a third `font.ui.title` (2× atlas, base 20) was added
  so headings/title (sizes 16–22) upscale from a nearest base rather than a
  large fractional scale of the 10px base. In scope, no new dependency; the
  atlases share one glyph design. Recorded in §A.1/§F.0.
- **`HelpState` gamepad column widened by 4px** (`col3W`): the new font makes
  "D-Pad Right or Stick" 1px wider than the old default font; the rightmost
  column now takes the full width to the safe margin. Routine font-forced layout
  fix; the capture overflow check is the arbiter.
- **Capture-only seam `BattleState::captureEnterTargeting()`** (guarded by
  `CRYSTAL_CAPTURE`, absent from Release): drives the battle deterministically
  into target selection for the new capture scene, instead of fragile scripted
  input. Not a production API.
- No value field was added to `ui::MenuItem` (constraint honored); no battle,
  scoring, save, schema, or determinism behavior changed.
