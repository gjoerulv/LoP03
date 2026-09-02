# M98 — Post-program approvals & the six-fix batch

**Status:** complete (approved 2026-09-02)
**Program:** M98–M108 "Are P Geese" (owner-authorized 2026-08-16; one plan,
one authorization; plan file with the exploration record and the owner
interview at `~/.claude/plans/new-milestones-and-keen-liskov.md`). No
battle-rules, generation, or save-version bump — every change here is
presentation, data, or post-battle-agnostic UI state.

## Scope (owner feedback batch, 2026-08-16)

The program preamble (recording the owner's M86–M97 batch approval and the
M95 veto resolution) plus six independently verifiable fixes from the
owner's manual pass: the tutorial-over-prologue interruption, the equip
shop's stale owned counts, Arcane Burst's missing element, the silent
heirloom equip band, the finale's wrong King sprite, and the
elemental-SFX audit.

## What was built

- **Preamble (docs only).** The ledger records M86–M97 `complete
  (approved)` (owner, 2026-08-16) with the program paragraph; each of the
  twelve notes' status lines flipped. The M95 flagged veto is RESOLVED:
  **the Hollow King CAN be Terrified by summons** — implemented behavior
  stands, no summon-specific immunity; game_design's King entry now says
  so beside his Blind/Silence/Confusion immunities.
- **Tutorial prompts defer to story scenes** (`TownState`): lifecycle
  hooks now queue beats (`queueTutorial` + `pendingBeats_`) and `update()`
  flushes ONE per frame only while the town is the active top state. The
  New Game prologue therefore plays uninterrupted with the welcome prompt
  following it, and the first-travel prompt follows a town's arrival scene
  the same way (both collision sites verified in code). Seen-set semantics
  unchanged — `takeBeat` still dedupes at fire time.
- **Equip shop owned counts refresh in place** (`EquipShopState`
  Phase::Buy): purchase now rebuilds the shelf immediately, restoring
  cursor + scroll and re-setting the "Bought ..." banner after the rebuild
  so the M58 message contract holds. No more stale "x N" until re-entry.
- **Arcane Burst is dark** (`data/skills.json`): `"element": "dark"` (+ a
  description nod). The M48 affinity math, M91 dark wisp accent, and the
  dark impact voice all key off the element field — no code change needed.
  Pinned by a content test (owner decision recorded in its name).
- **Heirloom candidates show their effect text** (`EquipShopState`
  Phase::EquipItem band, shop and M90 party mode alike — one state serves
  both): an heirloom candidate renders `name - description (+ resistance)`
  in a two-line body-font wrap (`"equipshop.heirloom"`) where the stat
  diff sat; non-heirloom candidates keep the M52 per-stat coloured diff
  untouched. New capture scene **115_equip_heirloom_text** stages the
  longest shipped composition (Hearthstone Chip, 109 chars) so the lint
  referees the budget; `captureEnterEquipItem` learned the fourth slot.
  A new content check requires every shipped heirloom to carry a
  description.
- **The finale stages the true King** (`CutsceneState.cpp`): the staged
  King sprite is `boss.the_hollow_king.battle` (the castle boss), not the
  `hollow_sovereign` dungeon boss it mistakenly showed. Dragon unchanged.
- **Elemental-SFX audit** (owner: "as far as I can tell they never play"):
  the chain was audited end to end and **no broken link was found** —
  enum/table alignment (21 roles), `elementHitSfx` mapping, manifest rows,
  distinct generated WAVs (hashes differ; six authored voices), presence
  in BOTH build dirs' asset copies, loader, and the live
  staging→impact-beat commit that plays case-2/4 through the element
  route. What WAS missing is observability, so M98 adds it:
  `AudioManager::applyManifest` now logs "elemental impact SFX loaded
  from files: N/6" every load, and `play()` logs once per role when an
  elemental request silently remaps to `hit_magic` (the M91 fallback that
  used to be invisible). **If the voices still read as absent in the
  manual pass, the finding is that the WAV designs are too subtle — an
  owner-directed audio redesign, not a code defect** (matrix row 187 is
  the ear test; the log line is the fact check).

## Deviations from the plan

None of substance. The plan's "tutorial fires when TownState is top" is
implemented as a general pending-queue (covers first-travel and
carried-out/return beats too, not just the welcome), and the SFX "fix"
resolved to diagnostics + an explicit ear-test handoff because the audit
found every link sound (in both senses).

## Verification

- Build: `cmake --build --preset debug` — clean (VS2022 dev shell).
- Tests: `ctest --test-dir build-msvc` — **778/778 pass**, including the
  two new checks (Arcane Burst is dark; every heirloom describes itself).
- Capture: `CrystalDungeons.exe --capture` — **115/115 scenes clean**
  (114 + the new heirloom-band scene), no `[ui-overflow]`.
- Release preset: not rebuilt this milestone (no build-system change);
  the program's later milestones rebuild it before hand-off.

## Manual owner checklist

Matrix rows **185–188**: prologue-before-prompts; shop count + heirloom
band; Arcane Burst dark + the six elemental voices (the ear test, with
the new log line as the fact check); the finale's true King.

## Documentation updated

`docs/milestones.md` (approvals, program paragraph, M98 row);
`docs/game_design.md` (King-terror ruling; tutorial deferral; heirloom
equip text); the twelve M86–M97 note status lines; the M95 veto section;
`docs/manual_test_matrix.md` rows 185–188; this note.
