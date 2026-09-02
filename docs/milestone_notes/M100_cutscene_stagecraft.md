# M100 — Cutscene stagecraft & THE STRANGER "P"

**Status:** complete (approved 2026-09-02)
**Program:** M98–M108 "Are P Geese" (owner-authorized 2026-08-16). No
battle-rules or generation bump. Save schema additive-only:
`strangerJokesTold` (optional int; old saves → 0). The cutscene schema
grew a second shape ("joke_*" scenes) — additive, existing files load
unchanged.

## Scope (owner items)

The Goose character is named P (captioned THE STRANGER "P"); cutscene
text boxes should not need scrolling (taller box / higher stage); the
final cutscene must stop replaying endlessly — post-finale interactions
tell random dry jokes with no reward; cutscenes editable in CrystalForge
(verified — already true since M97; gaps closed below).

## What was built

- **The rename**: all 60 speaker/responseSpeaker values in
  data/cutscenes.json are now `THE STRANGER "P"`; the choice plaque reads
  "The Stranger \"P\" Offers"; the roadside nameplate is a dry `"P"`.
  The double quote is printable ASCII — inside the M87 glyph contract. A
  data test pins that no scene speaks as the un-named "The Stranger".
- **The taller stage** (`CutsceneState`): the feet line moved UP
  (kFloorY 150 → 106) and the dialogue panel's cap grew 3 → **6 lines**;
  the longest shipped beat (254 chars) wraps to five, so no authored beat
  scrolls anymore. The M87 viewport remains as the overflow net for
  translations. All three cutscene captures re-render on the new stage.
- **Finale-once + the joke pool**: the roadside trigger now plays the
  finale only until its keepsake choice is RECORDED (preserving the M97
  quit-mid-scene guard exactly — `cutsceneChoiceFor`, not seen-marking,
  is the gate). Afterwards each visit plays one joke from the authored
  pool: seven `joke_*` scenes (optionless, questionless, reward-less;
  the loader enforces that shape and rejects a joke smuggling options),
  cycled in sorted order by the persisted `strangerJokesTold` counter —
  every joke heard before any repeats, reload-honest. `joke_1` carries
  the owner's line verbatim ("...wild goose chase."). No jokes authored →
  the trigger falls back to retelling the finale rather than going mute.
- **Schema**: "joke_*" ids are known by prefix (forge users can author
  joke_8+ with no code change); `question` is optional and required only
  on story scenes; story scenes keep the exactly-2-options + question
  rules verbatim. CrystalForge's cutscene form marks the question
  optional and its docs say which shape is which; validation runs the
  real loader either way (C4 verified: id/question/beats with
  speaker/text/emote/staging, and options with label/heirloom/response
  are all editable — no gaps found).
- **Capture**: new scene `116_stranger_joke` (joke_1 on the raised
  stage); count 115 → 116.

## Verification

- Build: clean (VS2022 dev shell). Tests: full suite green after the run
  completes — two count pins updated deliberately (`cutsceneCount` 8 → 15;
  the shipped-arc test now counts story + joke pool separately). New
  tests: joke-shape loader rules, shipped-pool contract (≥7 jokes, all
  optionless, all captioned THE STRANGER "P", owner's line present),
  cycle determinism, rename pin.
- Capture: **116/116 scenes clean**.
- `CrystalForge --canonicalize`: 1 file rewritten (the hand-added jokes
  normalized), 0 content errors before and after.

## Deviations from the plan

- The finale gate keys on the RECORDED CHOICE, not on "seen" (the plan
  said "profile flag") — seen is marked before the push, so a seen-gate
  would eat the keepsake of a player who quits mid-first-play. The
  choice-gate preserves M97's protection and still satisfies the owner's
  "plays once" intent.
- The plan's "6–8 jokes" landed at 7.

## Manual owner checklist

Matrix rows **191–193**: the caption/nameplate rename everywhere; a
full-story read on the taller stage (no scrolling in any shipped beat);
finale once then the joke cycle (incl. save/reload mid-cycle and the
quit-mid-finale re-offer).

## Documentation updated

game_design (Hooded Goose passage: the name, the finale-once rule, the
joke pool, the taller stage), ledger row, matrix rows 191–193, this note.
