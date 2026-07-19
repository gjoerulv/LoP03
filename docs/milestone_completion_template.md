# Milestone Completion Report Template

> Copy this template into the milestone completion report when a milestone's
> implementation is finished. Fill in every section honestly; write "none" or
> "not applicable" rather than deleting a section. The final status a Claude
> session may set is `implemented, awaiting manual approval` — only the project
> owner may later change it to `complete (approved)` after manual testing.

---

## 1. Implementation summary

- **Milestone:** MXX — <title>
- **Slices completed:** <list each approved slice and whether it was completed,
  partially completed, or dropped>
- **Player-facing changes:** <what a player would notice; "none" for pure
  engineering milestones>
- **Engineering changes:** <new/changed systems, interfaces, invariants>

## 2. Files changed

Group by category; list exact paths.

- **Source:** <src/... files created/modified/deleted>
- **Tests:** <tests/... files created/modified/deleted>
- **Content/data:** <data/... and assets/... files>
- **Documentation:** <docs/..., README.md, CLAUDE.md, skill files>
- **Build/release configuration:** <CMakeLists.txt, cmake/, presets, packaging>

## 3. Plan deviations

For each deviation from the approved milestone note:

- **What changed:** <deviation>
- **Why:** <reason discovered during implementation>
- **Impact:** <scope/risk/test/document impact>
- **Owner approval required?:** <yes — obtained when/how; or no — why it was a
  routine local decision>

If there were no deviations, state that explicitly.

## 4. Compatibility

State the impact (or explicitly "no impact") on each of:

- **Save files:** <version, old-save loading behavior>
- **Settings files:** <version, malformed-fallback behavior>
- **Content schemas:** <data/*.json version or field meaning changes>
- **Deterministic seeds:** <same seed still yields the same dungeon/battle?
  generation-version implications>
- **Score records:** <are existing scoreboard entries still valid/comparable?>
- **Packaged assets:** <manifest, directory layout, or packaging changes>

## 5. Automated validation

- **Configure command:** `<exact command run>`
- **Build command:** `<exact command run>`
- **Test command:** `<exact command run>`
- **Results:** <configure/build outcome; test count passed/failed>
- **Warnings:** <project-code warnings introduced or observed; "none">
- **Skipped validation and reason:** <anything not run, why, the exact commands
  the owner should run, and the expected successful output>

Never claim a build or test passed unless the command actually ran and
succeeded in this session.

## 6. Manual owner validation

A concrete checklist the owner can execute. Include, where relevant:

- exact launch steps and build configuration;
- screens/flows to inspect, with representative seeds or scenarios;
- window sizes/shapes to try;
- keyboard checks and gamepad checks;
- text-overflow / long-content cases to look at;
- audio checks;
- what to send back on failure (console output, screenshots, description).

## 7. Known limitations

List unresolved issues, partial implementations, and accepted trade-offs
honestly. "None known" only if genuinely nothing is known.

## 8. Documentation updated

List every document updated as part of this milestone (and any checked but
intentionally unchanged):

- `docs/milestones.md` — <status + section updated>
- `docs/milestone_notes/MXX_*.md` — <updated to describe the actual
  implementation and deviations>
- <docs/game_design.md, docs/technical_design.md, docs/completion_roadmap.md,
  README.md, content-authoring / control / asset-pipeline / save-schema /
  manual-test docs — as affected>

## 9. Final status

`implemented, awaiting manual approval`

Then stop and wait for the owner's manual acceptance decision.
