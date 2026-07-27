# M63 — Class level milestones

Second milestone of the M62–M66 program (plan + the 54-entry bonus table
approved 2026-07-27 after two owner review rounds — five rows rebalanced at
the owner's direction, the Goose scare-doubling option cut as a protected
secret). Implemented 2026-07-27.

## A. Status

**◑ implemented, awaiting manual approval** — set 2026-07-27. Evidence in §F.

## B. Goal (owner brief)

Each class gets level milestone bonuses at 10/20/30: a selection of two per
tier, only one choosable, permanent — affecting skills or granting a
passive-like trait. All 9 classes (owner Q&A); the choice prompts **at the
level-up moment**; the approved table is in the plan file and shipped
verbatim in `data/milestones.json`.

## C. As implemented

### Content (`data/milestones.json`, schema v1 — approved via the plan)

54 one-line entries `{id, classId, level, option, name, description, effect,
magnitude}`. `MilestoneEffect` is a 37-value enum (`Enums.hpp` — the
PassiveHook pattern: one value per behaviour, one magnitude). The loader
(`parseMilestones`) validates tier ∈ {10,20,30} and option ∈ {a,b};
`validateReferences` checks the classId and that every (class, tier) forms a
**complete a/b pair**. The editor gains a Milestones category (descriptors,
canonical one-line style, real-parser validation — the M59 completeness
sweep enforces coverage).

### Choice flow

`Character.milestone10/20/30` (optional save fields). A load keeps an id
only when the content still knows it AND it belongs to that class+tier —
anything else drops so the tier simply re-asks. `pendingMilestoneTier`
(pure, `game/Milestones.hpp`) finds the oldest reached-but-unchosen tier;
`MilestoneChoiceState` is a modal (two named, described options; Confirm
chooses permanently and re-derives stats; **Cancel postpones** — never a
hostage screen) that drains every pending choice across the party in one
visit. Prompt sites — the level-up moments: the dungeon's post-battle
XP award, the Elder Root's XP grant, the Training Hall's level purchase,
and town arrival (old saves / fresh loads).

### Resolution

Stat effects (`stat_*_pct`) apply in `refreshCharacter` — after class and
gear, so menus show the truth. Battle effects resolve at `buildBattle` via
`applyMilestones` (the applyPassives pattern), layered after the equipped
passive so a grant can only match-or-raise it. New engine rules, all in
shared `battle::` code (sim == live by construction): basic/magic/AoE/heal
percents, execute and vs-afflicted bonuses, an attacker weakness override,
status-duration bonus (never turn-control), guard-block override, double
strike and reduced-strength sweeps (an `attackOne(scalePct)` extension),
first-hit immunity, Iron Will healing, revive-share override, the
purify-heals opt-back, taunt curse, item potency, gold bonus (applied at
the dungeon's award site from STANDING members), suppression of
`alsoBuffsEnemies`, and on-kill / on-death triggers (the death chokepoint +
the poison tick, the god-mode precedent). **`kBattleRulesVersion` 13 → 14**
— a party with no chosen milestones resolves byte-identically.

## D. Files changed

`Enums.hpp/.cpp`, `Definitions.hpp`, `ContentLoader.hpp/.cpp`,
`ContentDatabase.hpp/.cpp`, `data/milestones.json` (new),
`game/Character.hpp`, `game/Milestones.hpp` (new), `game/Party.cpp`,
`battle/Battle.hpp/.cpp`, `save/SaveSystem.cpp`,
`states/MilestoneChoiceState.{hpp,cpp}` (new), `states/TownState.cpp`,
`states/TrainingHallState.cpp`, `states/DungeonState.cpp` (prompts + gold
bonus), `editor/FieldDescriptor.hpp`, `editor/CategoryDescriptors.cpp`,
`editor/EditorValidation.cpp`, `editor/CanonicalJson.cpp`,
`capture/CaptureRunner.cpp` (+`78_milestone_choice`), `CMakeLists.txt`,
`tests/CMakeLists.txt`, `tests/test_milestones.cpp` (new), docs.

## E. Deviations

- None from the approved table. One engineering choice: the gold bonus is
  read from the PARTY (standing members) at the dungeon's award site rather
  than from the battle — the award happens after HP write-back, so
  "while the Rogue stands" is honest and there is exactly one rule.
- Cancel on the modal postpones instead of forcing a choice; prompts are
  event-driven (XP gains + town arrival), so a postponed choice returns
  without ever looping.

## F. Automated validation (2026-07-27)

- `[milestone]` battery: **16 cases / 289 assertions green** — table shape
  (9 × 3 × a/b), loader rejections, pending/chosen logic, refreshCharacter
  stats, buildBattle resolution + inertness, and per-effect engine cases
  (execute, deep guard, first-hit glance, Iron Constitution surge, double
  strike, reduced sweep, Purifying Light, Blessed Renew, Lingering Hex,
  Intimidating Taunt, Selective Generosity, Standing Ovation, Last Laugh —
  incl. the poison-death path, weakness override, gold-bonus helper),
  sim==live parity, save round-trip with class/tier-mismatch drops.
- Closing verification: **579/579 Debug and 575/575 Release tests green**
  (the 4-case gap is the debug-only god-mode battery); `--capture` **78/78**
  scenes clean (+`78_milestone_choice`); the M59 canonical and descriptor
  sweeps accepted the hand-authored `milestones.json` byte-canonical; zero
  project-code warnings.

## G. Manual owner checklist

1. Level a member to 10 (Training Hall is quickest): the choice modal
   appears at once; Cancel postpones and it returns on the next level-up or
   town entry; Confirm locks the choice (verify it appears nowhere to
   change).
2. Load an old save with members past level 10/20/30 — the modal prompts on
   town arrival, oldest tier first, for every member, in one visit.
3. Pick a stat bonus (+HP/+SPD/+MP/+DEF) and watch the menus update
   immediately; pick battle bonuses and verify them in a fight (e.g. the
   Guardian's first-hit glance, the Ranger's double shot, the Cleric's
   Purify healing again).
4. Verify a Goose with Selective Generosity no longer buffs enemies, and a
   fallen Cutpurse pays no gold bonus.
5. Balance: the table is owner-approved but numbers are one playthrough's
   guess — flag any row that dominates or disappoints.
6. CrystalForge: the Milestones category lists all 54, edits validate
   through the real loader, and a saved file stays byte-canonical.

## H. Known limitations

- Milestone choices are permanent by design (no respec); a respec service
  would be a new owner decision.
- The choice modal shows two options only (per the brief); if a class ever
  ships more options per tier, the loader's pair rule and the modal both
  need widening.

## I. Final status

`implemented, awaiting manual approval`
