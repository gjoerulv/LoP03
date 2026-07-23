# M46 — Presentation facelift: the procedural UI kit

## A. Status and authority

- **Status:** ☑ complete (approved) — merged to main by the owner 2026-07-23
  (`2d9d0da`, merge `e9f7a37`) after playtesting.
- **Provenance:** an owner-directed, presentation-only milestone executed
  from a written brief (not a pre-authored note); this file is the
  **as-implemented record**. The brief's binding constraints: preserve
  gameplay, content, controls, saves, the 426×240 resolution, sprites,
  raster backgrounds, and the bitmap font; no new image assets, shaders,
  external libraries, or doc rewrites during the task.
- **Approved direction (supersedes older mood wording for UI):** *"Humorous
  8-bit-plus fantasy micro-caricature: Atari-era geometric economy,
  early-console silhouette readability, a brighter storybook palette, and
  deliberately oversized equipment."*
- **Base checkout:** `671bc8d` ("M45 doc update"). Baseline 431/431 tests
  (426 + 5 new UI-kit tests written during the milestone), capture 49 → 51
  scenes.

## B. What shipped

- **Semantic palette** (`src/ui/UiStyle.*`): the pre-M46 8-role palette grew
  to **28 roles** (surfaces, borders, accents, meters, structure roles), all
  with high-contrast equivalents; the standard table was re-valued to the
  approved storybook foundation (ink `#0E0C14`, panels `#171522`/`#252235`,
  warm cream text `#F3EAD8`, gold `#E6BD6A`, crystal/magic/danger/success/
  resource accents). The legacy constants still equal the standard text
  roles (test-pinned); high-contrast keeps pure-white text, brightens
  borders, darkens fills.
- **Procedural kit** (`src/ui/UiDraw.*`): `FrameStyle` variants (Standard /
  Raised / Inset / Danger / Reward / Crystal / Overlay — one construction:
  hard 2px offset shadow, ink keyline, mid border, top-left light,
  bottom-right depth, 2px stepped ink corners); `drawTitlePlaque`,
  `drawHeaderBand` (+ gold badge), `drawSectionHeader`, `drawDivider`,
  `drawSelectionSlab`, `drawChevron`, `drawStepArrow`, `drawChip`,
  `drawKeycap`, `drawFooterHints` (device-derived keys, planned fit, compact
  fallback — never silent clipping), `drawBanner` (Danger/Success/Reward
  with shape icons), `drawMeter` (framed, capped, segmented),
  `drawFocusBrackets`, `drawCrystalPip`, `drawModalDim`; pure helpers
  `meterFillWidth`/`planHintGap`/`lighten`; `motionPhase()` — the sanctioned
  two-frame ~2.5 Hz clock (1px chevron/arrow nudges, pip glints, danger
  border pulse; layout and text never move).
- **Menus:** `drawMenu`/`drawMenuScrolled` upgraded in place — focused rows
  get slab + chevron + brighter text (three signals); chunky stepped scroll
  arrows; measurement/wrapping/suffix/overflow behavior unchanged.
- **Every screen migrated** to the kit (title, all services, Guild, dungeon
  HUD + pause, battle, town, Settings, Controls/Remap, party creation,
  save/load, confirmations, tutorial/details/story overlays, result,
  scoreboard, bestiary, achievements, castle). The legacy `drawPanel` was
  removed; `drawFramedPanel` (nine-patch) remains functional but no screen
  uses it — the procedural kit is the identity (`ui.frame.default` still
  loads and is manifest-replaceable).
- **Capture:** two new scenes (`50_dungeon_pause`, `51_battle_high_contrast`)
  → 51; verified overflow-clean, byte-stable, and clean with the frame
  texture deleted. **Tests:** `tests/test_ui_kit.cpp` (contrast floors for
  both palettes, border-luminance ordering, meter/hint-layout math,
  high-contrast brightness guarantees) → 431 total.

## C. Deviations / notes

1. The title tagline changed to "an 8-bit-plus dungeon-score roguelite"
   (brief-directed; M51 replaces it with randomized comedic phrases).
2. Small sanctioned layout shifts: battle list x 16→20, equip shop 9→8
   visible rows, scoreboard 11→10, Help columns 8px left, the dungeon idle
   footer says "Pause" (the retreat consequence lives in the pause modal).
3. Two tutorial texts were corrected in the same merge at the owner's
   request (post-facelift): the Royal Relic beat is now **cryptic** (effects
   implied through the storytellers' ballad, never stated), and the stakes
   beat quotes the real −30 %/−99 % numbers.
4. Town exits at tile row 14 (pixel y=224) sit under the new opaque footer
   strip — acknowledged; **M50 fixes this** as part of the town rework.

## D. Evidence

431/431 tests in Debug and Release; `--capture` 51/51 scenes overflow-clean
(standard + high-contrast + grayscale review); runs clean with
`ui.frame.default` missing; no gameplay, content, save, or schema change.
