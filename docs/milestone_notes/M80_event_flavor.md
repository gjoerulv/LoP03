# M80 — Event flavor text: dungeon events find their voice

Authorized 2026-08-05 as part of the M75–M86 expansion program (see the
program section in `docs/milestones.md`). Re-audit this note against the
then-current checkout before implementation begins.

## A. Status

☐ planned

## B. Goal (owner brief)

Dungeon events stop being a bare footer prompt: triggering one brings up
lore-flavored, dry-humor text in the center of the screen (then the usual
confirmation/choice), and the text is data-driven so it is editable in JSON
and, later, in CrystalForge.

## C. Scope (planned)

- New `data/event_flavor.json` (schema v1): one entry per
  `RoomEventKind` — the classic events, the three theme rites, the
  RoyalRelic event and the M76 Evil Duckling event — carrying an authored
  title and body line(s) in the game's dry-humor register, plus the
  existing trade-off wording. Validated by the content loader with the
  usual defensive behavior: a missing/malformed file or absent entry falls
  back to today's footer prompt (never a crash, never a blocked event).
- Presentation: when the player triggers an event, a **centered panel**
  presents the flavor text and the trade-off, with confirm/decline handled
  in place (footer keeps the binding hints). Cost/risk stays visible
  *before* confirmation — the M20 "visible trade-off" bar is unchanged.
- The text must fit the panel at maximum authored length (overflow policy:
  wrap within the panel; the capture lint referees).
- CrystalForge editing of the new file is deferred to M86 (JSON-editable
  from day one).

## D. Schema, save & version implications

- New public JSON content file (schema addition approved with this
  program). No rules/generation/save version motion — presentation and
  text only; event mechanics, costs and seeded rolls are untouched.

## E. Out of scope

New events (the duckling event landed in M76); event mechanics changes;
CrystalForge category support (M86).

## F. Dependencies

M76 (so the duckling event exists to receive an entry). Otherwise
independent.

## G. Acceptance criteria

- Every event kind shows its centered flavor + trade-off on trigger;
  decline/confirm both work; costs remain visible before commitment.
- Editing `data/event_flavor.json` changes the text with no rebuild.
- Deleting the file leaves every event fully playable via fallback.

## H. Automated validation

Loader validation tests (complete coverage per event kind, malformed file,
missing entry fallback); capture scenes for a representative event at
maximum text length. Full suite green.

## I. Owner manual validation

Trigger several events across themes; judge the voice (dry humor), pacing,
and readability; confirm an edited line shows up next run.
