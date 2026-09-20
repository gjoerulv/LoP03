# M121 — Skill kinds, icons & milestone-aware skill text

**Status:** complete (approved 2026-09-20)
**Program:** M120–M125 (owner-authorized 2026-09-18 via the approved plan;
branch `oyb12`, baseline `d789f31`).
**No version motion:** rules, generation, save and settings versions
unchanged; one additive optional content field (`skillTexts` on a milestone);
the manifest gains twelve ids.

## Scope (plan section M121; the owner's points 3, 4, 5)

3. A skill's details always showed its original text. It must follow the
   milestone upgrades of the member who holds them — and only that member.
   (Owner ruling in the planning interview: *adjust the description to match
   the new effect, with a milestone icon attached*; generic boosts get the
   icon plus the milestone's own sentence.)
4. Skills that deal no damage should say so, minimally — and skills get
   icons: one per element, one for non-elemental damage, buff, debuff, heal,
   a special one for summons; assigned as the game starts up, not static.
5. An unusable skill must still take the cursor (too little MP, ...), and a
   used summon stays visible, greyed.

## What was built

### Kinds and icons (point 4)

- `content::SkillKind` — Fire, Ice, Lightning, Earth, Holy, Dark,
  NonElemental, Heal, Buff, Debuff, Summon. **Derived, never authored:**
  `skillKindFor` decides from the skill's own fields (a summon wins; then a
  restorative — the Heal category, or a Cleanse/Uncurse on allies; then
  damage by element, using the battle's own notion of "deals damage",
  including the M40 Support-with-power rule; the rest is a Debuff when aimed
  at enemies and a Buff otherwise), and `ContentDatabase::addSkill` stores the
  result as content loads. Nothing in `data/skills.json` names an icon and the
  editor never writes the field.
- Twelve hand-placed 10×10 glyphs (`ui.icon.skill.<id>` ×11 +
  `ui.icon.milestone`) through the M81 `Save-IconGrid` pipeline, in their own
  RNG-free section at the end of `generate_textures.ps1`. Shape first, colour
  second: flame, snowflake, bolt, boulder, sun disc, crescent, claw marks,
  green plus, up-chevrons, down-chevrons, sigil ring; a gold star for the
  milestone mark. A rerun added exactly those twelve files (byte-stable).
- The kind line heads every skill sheet: "Fire damage.", "Non-elemental
  damage.", "Healing - no damage.", "Buff - no damage.", "Debuff - no
  damage.", "Summon - Holy damage." / "Summon - healing, no damage."
- Battle skill list: the kind icon leads each row. A summon's row names its
  **creature** (`summonName`) — the icon already says "summon" and the full
  names ("Summon: Starfall Sentinel", 131 px at the body font) never fitted
  the column beside an MP cost, a latent clip since M95; the sheet keeps the
  full skill name.
- Geometry: `kInfoX` 186 → 206. The list column grew 20 px to absorb the
  13 px icon column; every shipped name now fits beside its cost, and every
  shipped description still previews within the info column's three lines
  (checked for all skills and consumables against the shipped font metrics).

### Milestone-aware text (point 3)

- New optional `skillTexts: [{skill, description}]` on a milestone
  (`data/milestones.json`). Five authored adjustments on four milestones:
  Purifying Light → Purify ("...mends everyone a little as it does"), Blessed
  Renew → Renew (35%), Intimidating Taunt → Taunt (also saps attack),
  Selective Generosity → Honking Comfort and Generous Mending (the enemy
  clause gone). Loader: shape, non-empty description, one entry per skill;
  `validateReferences`: the skill must exist. Editor: an object-array
  descriptor on the Milestones category.
- `game/SkillInfo.hpp` (pure): `skillTextFor(Character, SkillDef, db)` — the
  description as THAT member casts it, plus which held milestones touch the
  skill. `milestoneTouchesSkill` mirrors the battle's own conditions for the
  generic boosts (Arcane Edge: magic category; Devastation: magic +
  all-enemies; Devotion: a heal that is not a pure cleanse — unless the
  holder also has Purifying Light; Lingering Hex: any status but the
  turn-control pair; plus the by-name effects' predicates as the fallback for
  scroll-learned look-alikes). Effects keyed on the foe (execute,
  vs-afflicted, weakness, first strike), on basic attacks/sweeps, on stats or
  on passives mark nothing — they change every blow, not a skill.
- Where it shows: the battle preview (adjusted description), the skill sheet
  (adjusted description, then "Milestone - <name>" — with the milestone's own
  sentence when it did not supply the text), the party member sheet, the
  gold star after the name in the list (only when the row has room — a mark
  never clips a name) and beside the sheet's title.
- The Dragon's sweep effects and Lightbearer are basic-attack milestones: no
  skill row exists to restyle (they stay on the party panel's milestone
  list, as before).

### Focusable greyed skills (point 5)

- Root cause: `ui::Menu::step` skips disabled rows, so a greyed skill could
  never hold the cursor. New opt-in `Menu::setFocusDisabled(true)`: the
  cursor rests on greyed rows, `currentEnabled()` still says no. Only a menu
  that opts in changes; every other list renders and navigates exactly as
  before.
- The battle skill list opts in. A focused greyed row shows a grey slab and
  chevron; the info column says why through one shared sentence
  (`skillBlockLine`): "Not enough MP: N needed, M left.", "SIL: ...",
  "USED: a summon answers once per descent.", "Not here. ..."; Details opens
  the sheet with the same reason; Confirm plays the error beat (that branch
  was dead code before — the cursor never landed there).
- A spent summon was already kept in the list (M95) — it is now reachable.

## Deviations from the plan

- The plan drew the milestone mark as a second icon column; it became a
  trailing mark after the name, shown only when it fits, because a second
  column would have clipped "Everyone Is Welcome" on any Goose holding
  Selective Generosity.
- `kInfoX` moved (not in the plan) — the alternative was clipping names.
- Summon rows show the creature name (not in the plan) — see above; it also
  fixes the latent M95 clip.
- The kind enum and rule live in the content layer (`Enums.hpp`,
  `Definitions.hpp`) rather than `game/SkillInfo.hpp`, because the database
  assigns the kind at load; the text half stayed in `game/SkillInfo.hpp`.

## Tests

- `tests/test_skill_info.cpp` (new, `[skillinfo][m121]`, 7 cases): the kind
  table (30 named skills + the rule's invariants over every shipped skill);
  a hand-built skill; the kind lines; adjusted text only on the holder; the
  generic sentences and the sheet body; Devotion through Purifying Light;
  the predicates vs. non-skill effects across every skill; the loader's
  `skillTexts` rules including the dangling-id reference check.
- `tests/test_menu.cpp` (`[ui][m121]`): the focus mode, and that a classic
  menu still skips.
- `tests/test_presentation_lint.cpp` (`[lint][m121]`): every kind id and the
  milestone mark resolve to a shipped texture.
- The editor's descriptor-coverage test covers `skillTexts` (it fails on any
  shipped key without a descriptor).
- Captures: `160_battle_skill_icons` (icons, marks, a spent summon focused
  with its reason), `161_battle_skill_details_milestones`,
  `162_battle_skill_details_adjusted`.

## Compatibility

- **Saves / settings / scores / seeds / rules:** untouched — presentation
  and one presentation-only content field.
- **Content schema:** one additive optional milestone key.
- **Assets / manifest:** twelve new textures and ids; nothing else changed
  (byte-stability proven by `git status` after the generator run).

## Known limitations

- The star is omitted on a row whose name leaves no room (today only a
  marked summon); the sheet always shows it.
- Enemy skill sheets (team inspection) show the base text and no milestone
  lines — enemies hold no milestones.
- Icon legibility at 1x and under the CRT filter is an owner judgement.
- The CrystalForge guide does not yet describe `skillTexts`; M125 refreshes
  it.

## Documentation updated

`docs/milestones.md`, this note, `docs/game_design.md` (§4 "Skills at a
glance"), `docs/technical_design.md` (§62; the live scene count),
`docs/ui_style_guide.md` (§6 disabled state), `docs/control_standard.md`
(list navigation), `docs/asset_pipeline.md` (skill-kind icons),
`assets/credits.md`, `docs/manual_test_matrix.md` (rows 249–252).

## Completion report

### 1. Implementation summary

**M121 — Skill kinds, icons & milestone-aware skill text.** All three
points complete. Players see an icon on every skill, a one-line statement of
what kind of skill it is (and when it deals no damage), descriptions that
follow the caster's own milestones with a gold star on the touched skills,
and a skill list whose greyed rows can be focused, explained and inspected.
Engineering: a derived `SkillKind` assigned at content load, one optional
milestone field, a pure text module mirroring the battle's predicates, an
opt-in menu focus mode, a details-overlay icon overload.

### 2. Files changed

- **Source:** `src/content/Enums.hpp`, `src/content/Definitions.hpp`,
  `src/content/ContentDatabase.cpp`, `src/content/ContentLoader.cpp`,
  `src/game/SkillInfo.hpp` (new), `src/ui/Menu.{hpp,cpp}`,
  `src/ui/UiDraw.cpp`, `src/states/BattleState.{hpp,cpp}`,
  `src/states/DetailsOverlayState.{hpp,cpp}`, `src/states/PartyState.cpp`,
  `src/editor/CategoryDescriptors.cpp`, `src/capture/CaptureRunner.cpp`.
- **Data:** `data/milestones.json` (five `skillTexts` entries).
- **Assets:** `tools/asset_gen/generate_textures.ps1`, twelve PNGs under
  `assets/textures/ui/icons/`, `assets/manifest.json`, `assets/credits.md`.
- **Tests:** `tests/test_skill_info.cpp` (new), `tests/test_menu.cpp`,
  `tests/test_presentation_lint.cpp`, `tests/CMakeLists.txt`.
- **Docs:** see "Documentation updated".

### 3. Plan deviations

See "Deviations from the plan" — layout decisions inside the milestone's
scope; none changes a rule, a schema beyond the plan, or a save.

### 4. Compatibility

See "Compatibility".

### 5. Automated validation

All run 2026-09-18 from the VS 2022 developer shell (amd64):

- `tools\asset_gen\generate_textures.ps1` - **byte-stable**: `git status`
  showed only the twelve new PNGs (plus the intended script/manifest/credits
  edits); the icons were reviewed on an 8x contact sheet before acceptance.
- `cmake --build --preset debug` - **succeeded** (game, CrystalForge, tests).
- `CrystalForge.exe --canonicalize` - **0 files rewritten, 0 content errors**
  (the hand-edited `milestones.json` was already canonical).
- `crystal_tests.exe "[m121],[skillinfo],[ui],[lint],[editor],[milestone],[content]"`
  - **117 test cases, 95978 assertions, all passed**.
- `ArePGeese.exe --capture <dir>` - **162/162 scenes clean**;
  `160_battle_skill_icons`, `161_battle_skill_details_milestones` and
  `162_battle_skill_details_adjusted` were read back by eye.
- `ctest --preset debug` - **918/918 passed** (1069 s; it shared the machine
  with the Release build and suite).
- `cmake --build --preset release` - **succeeded**;
  `ctest --preset release` - **914/914 passed** (770 s).

### 6. Manual owner validation

Matrix rows **249–252**: the icons' readability and whether each reads as
its kind; the kind line's wording; the adjusted descriptions (five authored
texts — judge the prose) and the generic sentences; the star's visibility;
the greyed-row reasons and the error beat; the wider list column.

### 7. Known limitations

See "Known limitations".

### 8. Documentation updated

See "Documentation updated".

### 9. Final status

`complete (approved 2026-09-20)`
