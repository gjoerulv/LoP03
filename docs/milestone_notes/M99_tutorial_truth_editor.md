# M99 — Tutorial truth & the Forge tutorial editor

**Status:** complete (approved 2026-09-02)
**Program:** M98–M108 "Are P Geese" (owner-authorized 2026-08-16). No
battle-rules, generation, or save-version bump; `data/tutorials.json` is a
new OPTIONAL content file (the third), so old installs and forge-less
checkouts behave exactly as before.

## Scope (owner items)

"Please audit all tutorial text if they are true and useful" and "Are
tutorial text editable by Crystal Forge? If not they should be."

## What was built

- **The truth audit** covered all SIXTEEN beats (the docs' "nine" was the
  M22 count; seven joined between M32 and M89). Six were stale and are
  rewritten in both the new data file and the constexpr fallback:
  - `dungeon_first` — "nothing ambushes you here" died with the M93
    patrols; the beat now teaches the Patrol counter instead of denying it.
  - `town_return` — the Inn has charged gold since M30; the beat now says
    gold-or-rest-token.
  - `first_travel` — the "bottom" roads died with M50; the beat teaches
    the east/west edge walk-through.
  - `first_castle` — "three challenges" became four at M85; the Dragon is
    nodded at without spoiling ("one more legend...").
  - `result_first` — the M32 town tag joined "depth and level".
  - `guild_prepare` — the M82/M92 Floors picker joined the preparation
    list.
  The other ten beats were verified accurate and left verbatim.
- **Tutorial text is data** (`data/tutorials.json`, schema v1, id/title/
  body rows): loaded by `loadAll` as the third optional file (event-flavor
  terms), parsed by `content::parseTutorialTexts` (shape + duplicates; the
  beat-id table stays code-owned in `tutorial::kBeats`, so known-ness and
  full coverage are TEST-enforced — the curio-lore layering precedent).
  `maybeTutorialPrompt` resolves data first, constexpr fallback second, so
  a missing file or entry can never silence onboarding.
- **CrystalForge edits it**: `Category::Tutorials` (14th category;
  `kCategoryCount` 13 → 14 with the M86 count-pin test agreeing), the
  event-flavor field set (id / Title / Body), validation through the real
  loader, title-keyed entity labels, and canonical InlineEntities
  formatting (`styleForFile` learned `tutorials.json` — the byte-stability
  test refereed this and caught the initial omission).
- **Capture**: scene `20_tutorial_prompt` now stages the longest RESOLVED
  body (data overlay, fallback aware) so the lint referees what players
  actually see.

## Tests

782/782 green. New: shipped-file lockstep (all 16 beats authored, nothing
else), the same conservative wrap lint the constexpr table passes, the
truth-pin case (patrol taught / free-inn and bottom-roads phrasing banned /
town-depth-level comparison present / "three challenges" gone), and loader
shape/duplicate cases. Capture 115/115 clean.

## Deviations from the plan

None. (The plan said "9 beats"; the audit found and covered 16.)

## Known notes

- `town_welcome` still says "Welcome to Crystal Dungeons" on purpose — the
  M108 rebrand owns every title string sweep.
- A forge user adding a NEW id authors dead text until code fires it; the
  descriptor comment says so.

## Manual owner checklist

Matrix rows **189–190**: read the six rewritten prompts in play (fresh
save or Settings → reset tutorials) and judge truth + usefulness; edit a
beat in CrystalForge, save, relaunch, and see the edit in-game.

## Documentation updated

game_design §12b (data-driven text + truth pass), technical_design content
files (fourteen; also corrected the stale "twelve" that predated M97's
cutscenes), editor_guide canonical-formatting list, ledger row, matrix
rows 189–190, this note.
