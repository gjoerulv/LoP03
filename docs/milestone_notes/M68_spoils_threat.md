# M68 — Battle spoils panel + party-relative threat rating

Owner feedback batch (2026-07-28): a proper post-battle results showing —
FF1-Pixel-Remaster-inspired, "not a new screen, not disruptive" — and the
threat-preview accuracy complaint ("most battles are marked as deadly, but
are still easy"). The owner chose the **party-relative** rating via the
in-session Q&A.

## A. Status

**☑ complete (approved by the owner 2026-08-05)** — implemented 2026-07-28. Evidence in §E.

## B. Scope (owner brief)

1. After a fight, a battle result showing **XP and gold received**, and a
   **diff for any level-ups** (FF1 PR inspiration). No new screen; a simple,
   easy, non-disruptive process.
2. Fix the threat preview: tiers must be accurate. (Owner decision:
   **party-relative** — "Deadly" means deadly for THIS party.)

## C. As implemented

- **Spoils rule** (`game/Spoils.hpp`, pure + header-only like Scrolls):
  `teamSpoils` (every enemy's reward + the boss's) and `applySpoils` — gold
  with the M63 standing-member bonuses, party-wide XP, and per-member
  **LevelUpDiff** (level motion, derived-stat deltas, newly unlocked skill
  names). The ONE rule; the panel can never disagree with the award.
- **The panel**: on Victory, the battle itself applies the spoils at the
  Done beat (HP write-back first, so "standing" is honest and the level-up
  heal survives — `finish()` no longer clobbers it) and draws a Reward
  panel over the settled battlefield: `+N XP each  +N gold`, then one
  compact block per leveled member — `Name  Lv.a > b`, the non-zero stat
  deltas, `New: <skills>` in gold. **The same single Confirm that always
  ended a battle dismisses it** — zero added inputs; without level-ups it
  is one slim line. Threaded as an optional `BattleSpoils*` through
  `BattleState` and `BossIntroState`; castle/gauntlet/treasure fights pass
  none and keep their old banner. `DungeonState::onResume` no longer
  grants (the old award block — and its `(+XP, +g)` message suffix — moved
  into the battle); the M63 level-up modal still prompts right after the
  battle pops, now with the diff already seen.
- **Party-relative danger tiers** (`danger/DangerRating`): the depth-only
  baseline (50 + 25/depth) never learned about the M32 town ladder while
  the team side scaled with it (up to ×3.7) — by town 3+ nearly everything
  crossed "Deadly" and the label carried no information. Now
  `partyThreat` (the same stat weights over each member's derived stats —
  gear and milestones included, max HP not current) is the baseline, and
  `tierFor` maps the team/party ratio through bands **<20 / <40 / <70 /
  <110 %** (Trivial/Easy/Fair/Dangerous, else Deadly). Calibrated against
  the simulator's clearing levels with town-shelf gear (`[danger-report]`
  prints the matrix): a party that barely clears reads the typical team
  Dangerous-to-Deadly; ~8–16 levels of geared headroom bring the typical
  read down to Fair. Tiers are snapshotted **once at dungeon entry**, so
  the Guild-to-boss labels and the danger-defeated score credit always
  agree within a run.
- **Generation 13 → 14**: layouts, teams, and events are byte-identical to
  v13 — the bump tags scoreboard comparability, because the danger credit
  follows the new tiers (the convention the owner approved in the Q&A).

## D. Files changed

`game/Spoils.hpp` (new), `danger/DangerRating.{hpp,cpp}` (party-relative),
`dungeon/RoomLayout.hpp` (gen 14), `states/BattleState.{hpp,cpp}` (spoils
apply + panel + `writeBackParty` latch + capture hook),
`states/BossIntroState.{hpp,cpp}` (spoils pass-through),
`states/DungeonState.{hpp,cpp}` (owns `pendingSpoils_`; award block
removed; entry-snapshot tiers), `editor/SimLab.cpp` (party-relative label),
`capture/CaptureRunner.cpp` (+`83_battle_spoils`), `tests/CMakeLists.txt`,
`tests/test_spoils.cpp` (new), `tests/test_danger.cpp` (rewritten +
`[danger-report]`), `tests/test_balance.cpp` (call site),
`tests/test_curios.cpp` (pin relaxed), docs. No battle-rules or save
changes; no data-file changes.

## E. Automated validation (2026-07-28)

- `[spoils]` battery: team payout sums (boss included, bad ids pay zero),
  applySpoils gold + party-wide XP, the level-up diff against the
  character's real before/after (new skills = exactly the learnset
  additions), the Cutpurse standing/KO'd gold cases, and the
  level-cap no-diff case.
- `[danger]` battery: threat monotonicity, party-relative band pins,
  stronger-party-never-raises-a-tier sweep, Boss override, determinism,
  the gen-14 exact pin, plus data-driven anchors (a maxed party reads an
  entry dungeon ≤ Easy; a fresh party at town 7 depth 20 reads everything
  Deadly; growth is monotonic against a real mid-world dungeon).
- Targeted run: `[spoils],[danger],[milestone],[scroll],[balance]` —
  **49 cases / 487 assertions green**.
- `--capture` **83/83 scenes clean** (+`83_battle_spoils`: four leveled
  12-char members, multi-skill learn lines, max XP/gold widths).
- Closing verification: **601/601 Debug and 597/597 Release tests green**
  (the 4-case gap is the debug-only god-mode battery); `--capture`
  **83/83 scenes clean**; zero project-code warnings.

## F. Manual owner checklist

1. Win an ordinary gate fight: the results panel shows `+XP each / +gold`
   over the battlefield; ONE Confirm continues into the dungeon (no extra
   presses vs before). The dungeon HUD message no longer repeats the
   reward numbers.
2. Level a member in battle: the panel adds `Name Lv.a > b`, the stat
   deltas, and any `New:` skills; if the level crossed 10/20/30, the M63
   choice modal appears right after the battle (boss kills: after the
   run's result screen). Cross-check a diff against the Party panel.
3. Win a boss fight: spoils panel → Continue → dungeon reckoning →
   (any milestone choice) → town.
4. Castle challenges, the Duck gauntlet, and the treasure-dig fight are
   unchanged (flat rewards, no spoils panel).
5. Threat labels: with your current strong party, walk a dungeon you find
   easy — most teams should now read **Trivial/Easy/Fair**, with
   Dangerous/Deadly reserved for the genuinely nasty compositions; take a
   fresh/weak party somewhere hard and watch the same seeds read
   Dangerous/Deadly. **Judge the calibration** — the bands live in
   `DangerRating.cpp` and re-tuning is one constant per band.
6. Scoreboard: new runs tag generation v14; old entries keep v13 and
   remain listed.

## G. Known limitations

- The danger snapshot is taken at dungeon entry: leveling mid-run does not
  soften the labels until the next run (deliberate — the preview, the
  in-run labels, and the score credit stay consistent).
- The calibration battery gears its parties from the town shop shelf only
  (no legendaries/milestones), so a maxed-out endgame party will read
  content one band easier than the battery predicts — direction, not
  defect.
- Escaped and lost fights pay nothing and show no panel (unchanged rules).

## H. Final status

`complete (approved 2026-08-05)`
