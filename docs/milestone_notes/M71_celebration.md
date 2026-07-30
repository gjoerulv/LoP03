# M71 — The victory celebration screen

Owner feature (2026-07-29): a celebration after flawless-stakes dungeon
clears and the three great challenge victories. Implemented 2026-07-29.

## A. Status

**◑ implemented, awaiting manual approval** — set 2026-07-29. Evidence in §D.

## B. As implemented

- **`CelebrationState`** (new, presentation-only): a full-screen beat —
  the "Victory!" plaque, ONE headline number in large gold (the score, no
  breakdown), deterministic falling confetti, and the party celebrating:
  every standing member jumps to their own rhythm and height (per-slot
  frequency/amplitude/phase tables), the **MVP on a raised pedestal** at
  centre (jumping ~40% higher, gold-named, an "MVP" chip above), and any
  **KO'd member lying where they fell** — horizontal, dimmed, not
  jumping. One Confirm/Cancel continues. Reads the party, writes nothing.
- **The punchline** (owner follow-up, same day): one dry, lore-friendly
  line under the score, picked at random per celebration from
  `states/CelebrationPhrases.hpp` — twelve original lines in the
  TitlePhrases idiom ("The dungeon files no complaint.", "The Guild
  counted every turn. Twice.", "Somewhere, a goose approves.", …).
  `GetRandomValue`, so live play varies while the pinned capture seed
  keeps scene 84 reproducible; presentation-only, never the gameplay Rng.
  A headless lint (test_presentation_options) pins the pool: non-empty,
  genre-word-free, every line fits the screen at the drawn font.
- **When it shows** (owner rules; the score/escape gates tightened by the
  owner after the post-M71 audit):
  - after a dungeon completion with **zero stakes penalty**, a **positive
    score**, and **zero escapes** — pushed above the reckoning, so the
    order is: celebration (score only) → the detailed result screen →
    (any milestone choice) → town. Any stakes penalty, a 0-or-less score,
    or a single escape goes straight to the reckoning.
  - after beating **the King**, **the Deadly Duck**, and **the Boss
    Rush** — pushed above the challenge's result overlay, headline
    "Cleared in N turns!". The **Endless Rush is excluded** (it has no
    "beating"). Losses never celebrate.
- **MVP**: the M42 rule — most damage dealt over the run
  (`RunStats::mvpMember`). The castle challenges previously tracked no
  stats; `CastleChallengeState` now owns a `RunStats` and passes it to its
  battles, so the pedestal is honest there too. No MVP data (-1) → no
  pedestal, the row spreads evenly.
  **Assumption**: the brief's "MVS" was read as the MVP (the game's
  existing most-valuable metric). Say the word if it meant something else.
- The overflow lint rejected per-member name labels at fan spacing
  (12-char worst-case names cannot fit 64px), so only the MVP is named —
  the class sprites carry everyone else's identity.

## C. Files changed

`states/CelebrationState.{hpp,cpp}` (new),
`states/CelebrationPhrases.hpp` (new), `states/DungeonState.cpp`
(the stakes-gated push in `completeDungeon`),
`states/CastleChallengeState.{hpp,cpp}` (`RunStats stats_` into its
battles; the celebration push for cleared King/Duck/BossRush),
`capture/CaptureRunner.cpp` (+`84_celebration`), `CMakeLists.txt`,
`tests/test_presentation_options.cpp` (the phrase lint), docs.
No save/schema/rules/generation changes; no data changes.

## D. Automated validation (2026-07-29)

- Debug build: zero project warnings (one iteration: the capture lint
  caught 4 name-label overflows at worst-case names — resolved by the
  MVP-only naming above; honest catch, kept).
- `--capture` **84/84 scenes clean** (+`84_celebration`: max score width,
  a 12-char MVP name on the pedestal, one fallen member).
- The punchline pool lint (`[options][celebration]`): non-empty,
  genre-word-free, every line fits the screen at the drawn font — the one
  headless test this milestone adds (the states themselves are
  render-only; the manual checklist below is their verification).
- Closing verification: **607/607 Debug and 603/603 Release tests green**;
  `--capture` **84/84 scenes clean**; zero project-code warnings.

## E. Manual owner checklist

1. Clear a dungeon with NO stakes penalty: celebration first (score only,
   big and gold, a dry punchline beneath — different lines across
   celebrations), Confirm → the usual detailed reckoning → town. The team
   jumps at visibly different rhythms/heights; the MVP stands centred on
   the pedestal with the chip and their name; confetti falls.
2. Clear one WITH a stakes penalty, one with an escape, and (if you can
   manage it) a zero-score completion: none of the three celebrates —
   straight to the reckoning.
3. Clear a run where a member ended KO'd: they lie horizontal, dimmed,
   not jumping. If the KO'd member IS the damage MVP, they lie in state
   ON the pedestal — chip, gold name, and all (deliberate comedy,
   owner-confirmed).
4. Beat the King, the Duck gauntlet, and the Boss Rush: the same screen
   with "Cleared in N turns!"; then the challenge's own result overlay.
   The Endless Rush never celebrates; neither does any loss.
5. Feel: jump rhythm variety, pedestal look, confetti density — owner
   judgment.

## F. Known limitations

- Only the MVP is named (lint-driven; see §B).
- The MVP measures damage dealt — a pure-healer carry never takes the
  pedestal (the M42 metric, unchanged; flag if you want a broader rule).
- A KO'd MVP **keeps the pedestal, the chip, and the gold name** and lies
  in state on it, horizontal and dimmed (owner rule, explicitly
  confirmed: "It's too funny not to have it like that").

## G. Final status

`implemented, awaiting manual approval`
