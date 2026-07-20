# Playtest Protocol (M23)

> Owner-run, observed, **uncoached** sessions. The point is evidence, not a
> demo: do not explain anything the game should teach. Findings feed the M23
> hardening pass; the milestone cannot complete without them.

## 1. Tester profiles (cover each at least once)

| # | Profile | What it tests |
|---|---|---|
| 1 | Unfamiliar with the project (never seen it) | onboarding, first-run comprehension |
| 2 | Experienced JRPG player | battle conventions, depth of tactics |
| 3 | Roguelite / score-chasing player | scoring comprehension, replay pull |
| 4 | Controller-first player | gamepad completeness and feel |
| 5 | Keyboard-first player | keyboard completeness and feel |
| 6 | Smaller display or low-vision setup (where available) | readability, High Contrast option |

One person may cover two profiles (e.g. controller-first AND JRPG-experienced);
note which. Profile 1 must be its own person and go first — they are the
M22 acceptance test ("completes a depth-1 run without external instruction").

## 2. Session rules

- Fresh state: delete `settings.json`, `tutorial.json`, and saves from the
  user data folder before profile-1 sessions (Settings > "Reset tutorial
  prompts" is not enough — bindings and options should be defaults too).
- Say only: "Play as you like; think aloud; I can't answer questions until
  the end." Never touch the input. If they are hard-stuck for 3+ minutes,
  note WHERE and WHY, then you may unstick them — that moment is a finding.
- 30–45 minutes per session, then a short debrief (§4).
- One session per row of the observation sheet; write during, not after.

## 3. Observation sheet (one per session)

```
Tester profile(s):            Date:            Input device:
Display/setup notes:

Time to begin a run (New Game -> entered dungeon):        min
Wrong-button / navigation errors (count + where):
Missed interactables (walked past chests/events/teams?):
Unread or clipped text (which screen?):
Tutorial prompts: read / dismissed instantly / annoyed?
Room navigation: hesitation, lost moments, backtracking confusion:
Battle decisions: understood commands? used Guard/skills/items? escaped?
Danger labels: did they read them? change behavior because of them?
Score screen: can they say what the score rewards? (ask at debrief)
Deaths/retreats and reaction:
Run duration + depth(s) attempted:
Dominant strategy observed (spam attack? one skill? shopping loop?):
Voluntary replay? (did they start another run unprompted?)
Quotes worth keeping:
```

## 4. Debrief questions (after play, in this order)

1. "What is this game about?" (loop comprehension)
2. "What does the score reward?" (must mention finishing + few turns)
3. "What would you do differently next run?" (depth of the loop)
4. "What annoyed you?" / "What felt good?"
5. Profile 6 only: "Was anything hard to read? Try Settings > High Contrast —
   better or worse?"

## 5. Triage (owner + Claude, after all sessions)

Bring the filled sheets back. Findings get triaged into:

- **Fix in M23** — repeated usability defects (2+ testers hit it), any
  score-comprehension failure, any stuck moment, any clipped/unread text.
- **Balance evidence** — pacing/difficulty observations, cross-checked
  against the sim report (`crystal_tests "[sim-report]" -s`).
- **Defer with reason** — logged in `docs/presentation_audit.md`; deferral
  needs a written why, not silence.

The M23 acceptance bar: all six profiles observed; repeated defects fixed
(not documented away); no dominant strategy trivializes representative runs.
