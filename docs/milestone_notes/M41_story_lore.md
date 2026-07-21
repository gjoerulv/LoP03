# M41 — Story NPCs & Lore

## A. Status and authority

- **Status:** implemented, awaiting manual approval — authored / re-audited and
  implemented 2026-07-21 against the post-M40 checkout. Owner authorized beginning
  M41 after approving M40. As-implemented record in §K. Only the owner sets
  `complete (approved)`.
- **Authority:** `CLAUDE.md` > this note > `docs/milestones.md` >
  `docs/game_design.md` > `docs/technical_design.md`. Seventh milestone of the
  M35–M42 program. **Content + presentation only** — no battle, generation, or
  scoring change; `kBattleRulesVersion` (3), `kGenerationVersion` (8), and
  `kSaveVersion` all unchanged (the read-progress flag is an additive optional save
  field).

## B. Problem statement (verified at re-audit)

- **The game has no narrative flavor.** There is a castle and a King (M40) but no
  story tying the climb together, and the castle's **Jester spot is a reserved
  placeholder** (`CastleState` renders "No jester has yet come to court" — M40 §G/§K).
- **The NPC-interaction pattern exists** — the M34 black-market dealer is a TownState
  overlay: a sprite drawn at a fixed tile, an `onXTile()` position check, a
  bottom-of-screen prompt, and `Confirm` → `pushState` ([TownState.cpp:169](src/states/TownState.cpp:169)
  `blackMarketHere`/`onBlackMarketTile`, draw + prompt). A story NPC reuses this
  exactly, at a fixed plaza tile present in every town.
- **The dialog-overlay pattern exists** — `TutorialPromptState` dims the frozen scene
  below (`rendersBelow`), draws a titled wrapped-text panel (`ui::wrapText` /
  `drawTextWrapped`, the M12 helpers), and dismisses on Confirm/Cancel
  ([TutorialPromptState.cpp](src/states/TutorialPromptState.cpp)). The story dialog is
  a near-clone with a speaker footer instead of the tutorial footer.
- **Content is loaded/validated by a uniform pipeline** — `parse*` + `validateReferences`
  in `ContentLoader`, defs in `Definitions.hpp`, a `ContentDatabase` accessor, each
  file `{ "version": 1, "<plural>": [...] }`. A new `story.json` slots in the same way.
- **The town map is a fixed 26×15** ([TownData.cpp:20](src/town/TownData.cpp)); the
  open plaza (rows 5–9) has free Ground tiles. The 5 black-market tiles are
  `{5,7},{20,7},{9,8},{16,6},{12,6}`; the player spawn is `(12,8)`. **`(6,9)` is a
  free plaza tile** (no building/door/exit/spawn/market collision) — the storyteller's
  fixed spot in every town.
- **The castle is a menu hub** (`CastleState`, M40) — the Jester is a new menu entry
  there, not a walkable tile.
- **Baseline:** 361/361 tests, 35/35 capture, no content/version changes since M40.

## C. Goals

- An **original, light-hearted, funny 8-part serial** (the hard originality
  constraint binds every word): parts 1–7 told by a **wandering storyteller** who
  appears in each town (towns 1–7), part 8 the **Jester's punchline** at the castle.
- **Data-driven** `data/story.json` (validated like other content) with one beat per
  town 1–7 plus the Jester beat (town = `kCastleTown`); a **story dialog overlay**
  reusing the M12 wrapped-text helpers; the storyteller reuses the black-market NPC
  interaction pattern (fixed tile + prompt + `pushState`).
- The **Jester's beat unlocks after hearing all 7** town installments (recommended,
  so the punchline lands); before that the Jester gives a teaser. Read-progress
  persists (an optional Party save field).
- NPC sprites (storyteller + Jester) via the generators, with manifest rows +
  credits; a content lint that the beats fit the dialog panel.

## D. Owner decisions & routine choices

Locked at program planning: one story NPC per town (1–7) + the Jester; an 8-part
serial with a real punchline; the Jester unlocks after all 7 (recommended); all
writing original.

Routine choices taken here (owner validates tone / whether the jokes land at
approval — writing is a manual-validation item like art and audio):

- **Premise: "The Ballad of the Hollow King."** A single **wandering storyteller**
  (one recurring character, one sprite) tells escalating, increasingly absurd rumours
  of the fearsome King as you climb the seven towns; the **Jester** at the castle —
  who actually serves the King — delivers the deflating, affectionate punchline (the
  dread King is a bored, tired, goose-fearing man named Gerald who mostly wants a
  nap and a good challenge). Ties to the M40 King without undercutting the *fight*
  (the Jester frames him as powerful-but-lonely, so the epic battle still lands).
- **One recurring storyteller sprite** (a wandering bard) reused in every town, plus
  one Jester sprite — two new NPC sprites, matching the single-sprite black-market
  precedent and keeping the focus on the writing. Distinct per-town NPCs/sprites are
  a possible polish follow-up (flagged).
- **Storyteller at a fixed plaza tile `(6,9)` in every town**, always present (unlike
  the seeded market). The beat shown is `findStoryBeat(currentTown)`. Reading it sets
  the town's bit in `Party.storyMet` (a 7-bit mask, optional save field).
- **The Jester is a `CastleState` menu entry** ("Speak with the Jester"): the
  punchline beat once all 7 are heard, else a short teaser. No walkable castle needed
  (the castle is a menu hub, M40).
- **No version bumps.** Story is content + presentation; `storyMet` is additive/
  optional (old saves → nothing read); no battle/generation/scoring effect.

## E. Proposed design & slices

1. **Content type.** `StoryBeat { int town; std::string speaker, title, body; }`
   in `Definitions.hpp`; `parseStory` + validation in `ContentLoader` (town 1..8,
   non-empty fields, unique towns, the 7 town beats + the Jester beat present);
   `ContentDatabase::story()` + `findStoryBeat(town)`. `data/story.json` (the 8
   original beats).
2. **Dialog overlay.** `StoryDialogState(speaker, title, body)` — a titled wrapped
   panel over the dimmed scene, speaker in the footer, Confirm/Cancel dismisses.
3. **Town storyteller.** `TownState`: draw the bard at `(6,9)`, `onBardTile()` +
   `nearBard_`, a "Hear the storyteller" prompt, `Confirm` → push `StoryDialogState`
   with the current town's beat and set the `storyMet` bit.
4. **Castle Jester.** `CastleState`: a "Speak with the Jester" menu entry → the
   Jester beat (all 7 heard) or a teaser; replace the placeholder flavor line.
5. **Save + progress.** `Party.storyMet` (optional int mask) + `game/Story.hpp`
   pure helpers (`kStoryTownCount = 7`, `storyAllHeard`); SaveSystem round-trip.
6. **Art + docs + tests + capture.** Two NPC sprites (appended last to
   `generate_textures.ps1`) + manifest + credits; `[story]`/`[lint]` tests (load,
   validation, panel-fit wrapping, save round-trip, mask helpers); capture scenes
   (a storyteller beat, the Jester punchline); `game_design.md` / `technical_design.md`
   / README / manual test matrix.

## F. Determinism & save compatibility

- **No determinism surface.** Story is static content; nothing seeds or resolves a
  battle/dungeon. Sim ↔ live unaffected.
- **Save.** `storyMet` is an additive optional field (old saves → 0, nothing read);
  no `kSaveVersion` bump. Loading still returns the party to town.

## G. Out of scope

Enrichment: bestiary / victory stats / achievements (M42); any branching or
choice-driven narrative; voiced/animated cutscenes; per-town distinct NPC sprites
(one recurring storyteller here); any battle/generation/scoring change.

## H. Balance / acceptance

Story validates and renders within the dialog panel budget (a `[lint]`/`[story]`
wrap check); the serial reads in town order (installments 1–7 by town, the Jester
last); the Jester unlocks only after all 7 are heard; `storyMet` round-trips through
saves; all writing original; full suite green from the 361 baseline; capture
overflow-clean with the new dialog scenes; lint green.

## I. Manual validation for the owner

Tone and whether the jokes land (a manual-validation item like art and audio);
whether the storyteller is easy to find in each town; whether the Jester punchline
pays off the climb; readability of the dialog panels.

## J. Required documentation updates on completion

This note (§K); `docs/milestones.md` (status); `docs/game_design.md` (the story
serial as player-facing flavor); `docs/technical_design.md` (the story content type
+ dialog overlay + storyteller/Jester); `assets/credits.md` + `docs/asset_pipeline.md`
(two NPC sprites); README (story line); `docs/manual_test_matrix.md` rows; a manual
checklist per `docs/milestone_completion_template.md`.

## K. As-implemented record (2026-07-21)

- **Status:** implemented, awaiting manual approval. The §D routine choices were
  taken as written (recurring wandering storyteller + Jester, two sprites; the
  "Ballad of the Hollow King" premise; the storyteller at `(6,9)`; the Jester as a
  castle menu entry gated on all 7).
- **Content.** `StoryBeat` (`Definitions.hpp`); `parseStory` + `loadAll` wiring +
  `optBool`-free validation (town 1..8, non-empty speaker/title/body, one per town)
  in `ContentLoader`; `ContentDatabase::addStory`/`story()`/`findStoryBeat`.
  `data/story.json` — **8 original beats**: the wandering storyteller's seven
  escalating verses (towns 1–7) and the Jester's punchline (town = `kCastleTown`).
- **Presentation.** `StoryDialogState` (title + speaker + wrapped body + Continue,
  dims the frozen scene) modeled on `TutorialPromptState`.
- **Storyteller.** `TownState` overlay at `(6,9)` in every town (fixed plaza tile,
  clear of buildings/doors/exits/spawn/market): sprite draw + `onBardTile()` +
  `nearBard_` + "Hear the storyteller" prompt → `StoryDialogState` for
  `findStoryBeat(currentTown)` and sets `Party.storyMet |= storyBit(town)`.
- **Jester.** `CastleState` "Speak with the Jester" menu entry → the punchline once
  `storyAllHeard`, else a teaser; the M40 placeholder flavor line replaced. Mask
  helpers in `game/Story.hpp` (`kStoryTownCount = 7`, `storyBit`, `storyHeard`,
  `storyAllHeard`).
- **Save.** `Party.storyMet` (optional 7-bit mask) round-trips (SaveSystem); old
  saves → 0. No `kSaveVersion` bump.
- **Art.** Two 12×12 overworld NPC sprites (`actor.bard.overworld`,
  `actor.jester.overworld`) appended last to `generate_textures.ps1` (no speckle
  RNG → **2 new / 0 modified** PNGs) + manifest + credits.
- **Validation (this session, VS 2022 / MSVC, `build-msvc`).** Debug **364/364**
  (+3 `[story]`: content/panel-fit lint/mask helpers; +1 `[save]` storyMet
  round-trip folded into the castle test). `--capture` **37/37** overflow-clean
  (new `36_story_dialog`, `37_jester`). Release build + Release suite **364/364**
  clean. The shipped `story.json` loads with zero errors (the content-loader
  smoke test). `kSaveVersion` / `kBattleRulesVersion` (3) / `kGenerationVersion` (8)
  all unchanged.

### Deviations from the plan / note

1. **One recurring storyteller (two sprites total), not seven distinct town NPCs.**
   The plan's "one story NPC per town" is realized as a single wandering storyteller
   who appears in every town telling the next verse, plus the Jester — matching the
   single-sprite black-market precedent and keeping the focus on the writing.
   Per-town distinct NPCs/sprites are a possible polish follow-up (flagged for owner
   veto).
2. **The Jester lives as a `CastleState` menu entry**, not a walkable tile — because
   the castle is a menu hub (M40 deviation 1), not a walkable place.

### Known items for owner validation

- Tone and whether the jokes land (writing is a manual-validation item like art and
  audio); whether the storyteller is easy to find; whether the Jester punchline pays
  off the climb; whether the King wink undercuts the boss fight (intended not to).
