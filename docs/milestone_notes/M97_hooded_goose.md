# M97 — The Hooded Goose: a red thread in eight scenes

**Status:** complete (approved 2026-08-16)
**Program:** M88–M97 (authorized 2026-08-14) — the program's final
milestone. No battle-rules, generation, or save-version bump: the two
story fields (`seenCutscenes`, `heirloomChoices`) are additive optional
save fields, and cutscenes never touch battle or generation.

## Scope (owner item 9, part 2)

The story that hands out the M96 heirlooms: a red thread from New Game
plus each new town, narrated by a hooded goose; choices granting relics;
a post-King finale NPC at the would-be Town-8 exit; party visible; the
goose acting goose-like; the finale showing King and Dragon; the story
Claude-authored (owner decision 3), editable in CrystalForge.

## What was built

- **Content category** (`data/cutscenes.json` — REQUIRED, it grants
  gameplay items): eight scenes on the fixed vocabulary `new_game`,
  `town_2`..`town_7`, `finale`. A scene = 6–9 dialogue beats (speaker,
  text with `{member1}..{member4}` name tokens, goose emote
  idle/waddle/jump/panic, King/Dragon staging flags) + a **mandatory
  two-option choice** — 8 × 2 = the sixteen M96 heirlooms, each granted
  by exactly one option anywhere (a `validateReferences` rule, alongside
  exists-and-IS-an-heirloom). The file rides the canonical writer, the
  glyph lint, and the loader's defensive rules like every other.
- **The story** (original, dry): the realm ran on *kept things*, carried
  between towns by the wild flocks; the King hollowed himself outlawing
  the one thing he could not command; the Dragon slept on the deep fire
  so the rot could not take it; the Duck, given a pond, chose empire
  (we do not discuss the Duck). The narrator is a scholar of migrations
  whose hood fools no one — it waddles when excited, panics near bread,
  slips into "we" and corrects itself, and honks exactly once, at the
  confession. The finale stages King and Dragon and resolves all three
  threads; the last two keepsakes close the arc.
- **CutsceneState**: a full-screen stage — party battle sprites left,
  the hooded goose center (new generated `actor.hooded_goose.stage`),
  staged guests dimmed behind. Dialogue on a bottom panel with an M87
  3-line viewport; Cancel offers "Skip scene?" whose skip lands ON the
  choice (the choice is never skippable and cannot be cancelled); the
  choice modal shows the highlighted keepsake's own description. Grants
  fire only while the scene has no recorded choice — replays retell,
  never re-grant.
- **Triggers**: New Game after party creation (before the first town
  breathes); first physical arrival at towns 2–7 (marked seen BEFORE
  the push — the M41 storyMet idiom, so reloads cannot replay); and the
  finale NPC at town 7's eastern roadside (`kGooseNpcTileX/Y` — exactly
  where a Town-8 road would begin), standing whenever the profile-level
  `kingDefeated` is set, staying for replays.
- **Persistence**: `Party.seenCutscenes` + `Party.heirloomChoices`
  ("scene:heirloom" entries). The save reader drops unknown scene ids,
  unknown keepsakes, and malformed entries; old saves load a fresh
  story; a New Game clears both.
- **Assets**: two RNG-free sprites appended to the texture generator
  (12×12 roadside NPC, 18×26 stage actor — a disguise that deliberately
  fails: white head, gold bill, escaping tail feathers, webbed feet);
  manifest + credits rows; **every prior texture byte-identical**.
- **Debug**: "Play cutscene" (Left/Right cycles the eight; plays
  replay-only — no grant, no seen-mark) and "Reset story progress"
  (clears both fields; granted keepsakes stay in the bag). **Forge**:
  `Category::Cutscenes` (12 → 13) with beats and options as
  modal-edited object arrays.
- **Captures**: `112_cutscene_dialogue`, `113_cutscene_choice`,
  `114_cutscene_finale` — the set is now **114 scenes**.

## Deviations

- **An option's response is one authored line** (responseSpeaker +
  responseText), not the plan's "response beats": CrystalForge's
  ObjectArray descriptors nest one level, and a response-beat array
  inside an option would be two. The closing line carries the wrap-up;
  the schema can grow additively later if a scene ever needs more.
- **Goose emotes are micro-motion**, not free animation: offsets of
  1–3px on the sanctioned M46 motion clocks (`motionPhase`/`motionPhase3`
  — the chevron's own rhythm), so captures are deterministic and the
  reduced-effect settings need no new gate. "Jump" is a 3px hop on the
  slow clock; "panic" jitters on both. Judge whether it reads (row 184).
- **The prologue pushes over the fresh town** (town music underneath)
  rather than a bespoke pre-town music state — deliberate simplicity.
- **Debug "Reset story progress" does not confiscate** already-granted
  keepsakes; a reset-then-replay can therefore duplicate them. Debug
  tool, debug rules; the row's message says so.

## M96 stragglers fixed here

The full suites surfaced three stale pins outside the batteries M96 ran
(`[heirloom]`,`[content]`): `test_editor_enum_lists`
(item-type/equip-slot/trigger-do counts 4/4/7 → 5/5/8),
`test_arms_icons` (heirlooms are gear — they carry the relic glyph, and
the "non-gear never has an icon" branch now excludes them, with a spot
pin), and `test_equip_shop_filter` (heirlooms are equippable but NEVER
shop-stocked — excluded from the partition count like legendaries, plus
a never-in-a-buy-category assertion). All three were test-side updates
to M96's already-recorded design; no engine change.

## Compatibility

Old saves: fresh story, everything else untouched. Old parties mid-save
in a town ≥ 2 will meet the missed scenes only on their NEXT first
arrival at a new town (first-arrival is the trigger, deliberately —
nothing replays, nothing force-fires on load). Content counts: +1
category, cutscenes 8 (pinned).

## Automated validation

- Debug + Release builds clean; captures **114/114** clean (the three
  new scenes overflow-linted with the longest beats).
- `[cutscene],[editor],[arms],[content]`: **6949 assertions in 76 test
  cases**, all green. Full `ctest --preset debug` launched after the
  batch; the result rides the session completion report.
- New `test_cutscenes.cpp`: loader shape/vocabulary (unknown scene id,
  unknown emote, missing beats, wrong option count, duplicates), the
  heirloom cross-file rules (unknown / non-heirloom / double-grant),
  the shipped arc's pins (8 scenes, ≥6 beats each, all 16 heirlooms
  exactly once, finale stages both principals, the owner's two named
  heirlooms open the prologue), the pure progress rules (town mapping,
  idempotent seen-marks, record-once choices, encode/decode, token
  substitution incl. defensive cases), and the save round-trip with
  hand-edited junk degrading to unseen.

## Manual owner checklist

Matrix row **184** — the full New Game → climb → King → finale
playthrough. The story, the goose acting, and the pacing are owner
judgment calls no test covers.

## Documentation updated

`docs/milestones.md` (M97 row) · `docs/game_design.md` (Hooded Goose
paragraph) · `docs/technical_design.md` (§50 + capture count) ·
`docs/manual_test_matrix.md` (row 184) · `assets/credits.md` (sprite
row).

## Final status

`implemented, awaiting manual approval`
