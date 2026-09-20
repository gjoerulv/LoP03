# M122 — Party panel skills & field healing; scrolls taught from Items

**Status:** complete (approved 2026-09-20)
**Program:** M120–M125 (owner-authorized 2026-09-18 via the approved plan;
branch `oyb12`, baseline `d789f31`).
**No version motion:** rules, generation, save, settings, content and manifest
versions unchanged; no schema change; no new asset.

## Scope (plan section M122; the owner's point 11)

In the Party menu (Pause → Party), inspecting the party no longer teaches
scrolls; instead the member's learned skills can be inspected and **used to
heal** — in dungeons only, for the same MP cost. Scrolls are applied from the
Items menu (Pause → Items): pick the scroll, then the member, with the skill's
description (MP, text, ...) visible while picking. Owner ruling in the
planning interview: HP heals and revives only; Purify and the summons stay
battle-only.

## What was built

### One heal formula (`battle/HealMath.hpp`)

- `healBase`, `healWithCastBonus`, `reviveHp` — the battle's heal arithmetic,
  moved verbatim into a pure header that `Battle.cpp` now calls at the same
  three sites. Behavior-neutral (the battle suites pin it); it exists so the
  menu cast and the battle cast cannot drift.

### The rules (`game/FieldSkills.hpp`, pure)

- **What:** `isFieldSkill` — a Heal-category skill with power or a revive
  share, never a summon. Shipped: Mend, Group Mend, Greater Heal, Renew,
  Mending Wind, Honking Comfort, Generous Mending. Purify (a pure cleanse),
  Absolve (an uncurse) and Radiant Spring (a summon) are out.
- **Where:** inside a dungeon only (`inDungeon`, passed by the dungeon pause
  menu). In town every row is inspect-only.
- **How much:** `fieldHealAmount` = the battle's `power + Magic/2`, times the
  caster's Devotion share; a revive raises at the higher of the skill's share
  and the caster's Blessed Renew share. `Combatant::stats` is the
  `Character`'s own `stats`, so the two paths agree by construction — and a
  test casts the same Mend in a built battle and from the menu and compares.
- **Refusals** (nothing is ever spent on "no effect" — the M43/M90 rule), in
  the order a player would fix them: a summon / not a field skill / in town /
  the caster fallen / too little MP / nobody it would help; per target:
  already full, fallen (no revive share), or standing (a revive-only skill).
- A heal's buff rider and cleanse do nothing in the field: a `Character`
  carries no statuses.

### The Party panel (`PartyState`)

- `Browse → Skills → PickTarget`. Confirm on a member opens "<Name>'s
  skills": the member's MP, a scrollable list with the M121 kind icon, MP
  cost, milestone star and creature-named summons, focusable greyed rows; the
  lower panel shows the kind line, a healing skill's refusal, and the
  description as that member casts it. Details opens the M121 sheet.
- Casting: a whole-party heal lands on Confirm; a single-target heal turns
  the roster into the target pick — the slab follows the target, rows show
  HP (or "Fallen"), the caster's name is gold, the panel reads "Restores up
  to N HP." / "Raises the fallen." or the target's refusal. The pick opens on
  the first member the cast would help and stays open while another cast is
  possible. The banner reports the real gain.
- The Use-Scroll picker is gone from this panel.

### Scrolls from the Items screen (`InventoryState`)

- A scroll row is now live: Confirm opens the pupil pick when someone can
  still learn it (else "Everyone already knows that skill."). Rows read
  "can learn" / "knows it" (greyed, focusable — Confirm there refuses with
  the M64 reason and keeps the scroll). The hint reads "Choose who learns
  it." and the footer "Teach".
- The taught skill stays in view under the rows: kind icon, name, MP cost,
  kind line, and a two-line description **as the highlighted member would
  cast it** (a Blessed Renew holder sees Renew at 35% while being picked).
- Teaching runs the unchanged M64 rules (`scrollRefusal` / `learnScroll`),
  the M109 ledger seam and consumes the scroll, then returns to the bag.
- The two in-game pointers changed with it: the 20-floor trove's hint and the
  Stranger's scroll line now say "Teach it from the Items menu."

## Deviations from the plan

- None in scope. The plan's "in town every row is inspect-only with the
  reason": the reason line is shown for healing skills only — "its moment is
  in battle" under every attack would be noise — and every skill's sheet
  still opens.

## Tests

- `tests/test_field_skills.cpp` (new, `[fieldskills][m122]`, 6 cases): the
  seven shipped field skills and what is excluded; every refusal string and
  its order; MP spend and the HP cap; Devotion (+20 %) and Blessed Renew
  (35 % vs 20 %); the whole-party cast (skips the fallen and the full,
  charges once); menu == battle for the same caster and skill.
- The battle suites (unchanged) pin the `HealMath` extraction.
- Captures: `163_party_skills_dungeon`, `164_party_heal_target`,
  `165_party_skills_town`, `166_items_teach_scroll`.

## Compatibility

- **Saves / settings / scores / seeds / rules / schemas:** untouched.
  `battleRulesVersion` does not move: battle resolution is byte-identical and
  the field cast is outside battle (the M90 consumables precedent).
- **Balance note for the owner:** like M90's potions, heals cast between
  fights spend MP instead of battle turns. MP is the only price; the Inn
  remains the full restore. This is the deliberate trade the owner asked for
  and M23's playtests should judge it.

## Known limitations

- No cast animation in the menu — a banner and the heal sound only.
- The lifetime ledger does not count field casts (it has no such counter;
  adding one would be a ledger/summary change outside this milestone).
- Party-cycle keys do not switch member inside the skill list (Back, then
  Up/Down).

## Documentation updated

`docs/milestones.md`, this note, `docs/game_design.md` (§4 the M64 paragraph
re-homed + "The Party panel's skills and field healing"; §5 the M90 Items
paragraph; the trove and summon-scroll pointers), `docs/technical_design.md`
(§63; pointers in §21 and the M92 paragraph; the live scene count),
`docs/manual_test_matrix.md` (rows 253–256), pointers in
`M64_scrolls_party_panel.md` and `M90_party_menu_equip_items.md`.

## Completion report

### 1. Implementation summary

**M122 — Party panel skills & field healing; scrolls taught from Items.**
Complete. Players can open any member's skill list from the Party panel,
read every skill (greyed or not) and, inside a dungeon, spend MP on a heal or
a revive with exactly the battle's effect; scrolls are taught from the Items
screen with the skill in view. Engineering: one shared heal-math header, one
pure rules header, a three-phase `PartyState`, a scroll branch in
`InventoryState`.

### 2. Files changed

- **Source:** `src/battle/HealMath.hpp` (new), `src/battle/Battle.cpp`,
  `src/game/FieldSkills.hpp` (new), `src/states/PartyState.{hpp,cpp}`,
  `src/states/InventoryState.{hpp,cpp}`, `src/states/DungeonMenuState.cpp`,
  `src/states/DungeonState.cpp` and `src/states/ScrollChoiceState.cpp` (one
  hint string each), `src/capture/CaptureRunner.cpp`.
- **Tests:** `tests/test_field_skills.cpp` (new), `tests/CMakeLists.txt`.
- **Docs:** see "Documentation updated".

### 3. Plan deviations

None needing approval (see above).

### 4. Compatibility

See "Compatibility".

### 5. Automated validation

All run 2026-09-18 from the VS 2022 developer shell (amd64):

- `cmake --build --preset debug` - **succeeded** (game, CrystalForge, tests).
- `crystal_tests.exe "[m122],[fieldskills],[scroll],[battle],[milestone],[itemuse]"`
  - **130 test cases, 1388 assertions, all passed** (the battle tags pin the
  behavior-neutral `HealMath` extraction).
- `ArePGeese.exe --capture <dir>` - **166/166 scenes clean**;
  `163_party_skills_dungeon`, `164_party_heal_target` and
  `166_items_teach_scroll` were read back by eye.
- `cmake --build --preset release` - **succeeded**.
- `ctest --preset debug` - **924/924 passed** (1087 s).
- `ctest --preset release` - **920/920 passed** (1072 s).

No data file changed in this milestone, so no canonicalize run was needed.

### 6. Manual owner validation

Matrix rows **253–256**: the skill list's readability and flow; a cast's
numbers against the same cast in battle; the target pick; the town and
battle-only refusals; teaching from Items with the skill panel in view; and
the balance question above (is MP-only field healing the right price?).

### 7. Known limitations

See "Known limitations".

### 8. Documentation updated

See "Documentation updated".

### 9. Final status

`complete (approved 2026-09-20)`
