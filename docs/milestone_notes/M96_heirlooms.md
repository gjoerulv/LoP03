# M96 — Heirlooms: worn memories (battle rules v18)

**Status:** complete (approved 2026-08-16)
**Program:** M88–M97 (authorized 2026-08-14). Battle rules **17 → 18**.
Save schema additive-only (`equippedHeirloom` optional gear field; old
saves load with the slot empty; unknown ids are dropped on load, never a
crash). No generation change.

## Scope (owner item 9, part 1)

A new equipment concept — **"Heirloom"**, the owner's chosen name (M44's
Royal Relics untouched) — worn keepsakes with triggered battle effects,
including the owner's two named examples: +15% HP the first time the
wearer drops below 50%, and +25% ATK while at 25% HP or less. Every
firing visibly announced. Editable in CrystalForge. Story acquisition
(the M97 cutscene choices) arrives next milestone; until then the debug
menu grants them.

## What was built

- **Type & slot**: `ItemType::Heirloom` + `EquipSlot::Heirloom` — a
  fourth worn slot walked by the whole equip flow (shop and M90 Equip
  Party: slot list, stat-diff panel, detail lines). Value 0, never in
  any Buy category. `Character.equippedHeirloom` persists like the other
  three gear ids.
- **Effects = the M75 trigger engine, attached party-side for the first
  time** (the rules bump): `ItemDef.triggers` reuses `TriggerDef`
  verbatim (loader forbids clones on items); `buildBattle` resolves the
  worn heirloom's triggers onto the wearer and absorbs its element
  resists. One engine addition: `TriggerDo::HealSelfPct` (heals N% of
  max HP, alive-only, capped, "+N HP" logged) — the owner's first
  example rides `FirstTimeHpBelowPct 50`.
- **The conditional edge**: `lowHpThresholdPct` + `lowHpAttackPct`
  (paired, heirloom-only, 0..100) compile into Combatant fields read by
  ONE public rule — `battle::lowHpEdgeActive()` — applied at damage time
  exactly like the Brute enrage and announced once through the same
  three announce sites (`lowHpText`). Heal back over the line and the
  edge sheathes; drop again and it returns (announce fires once per
  battle). Simulator and live play share the rule by construction.
- **Sixteen heirlooms** (`data/items.json`, two per M97 story choice):
  the Emberwake Locket and the Lastlight Band (the owner's examples) plus
  fourteen more spanning first-time braces, ally-felled surges,
  every-Nth rhythms, and two further conditional edges. All value 0,
  type heirloom, slot heirloom, no stat lines — an heirloom's worth is
  what it does.
- **Anyone may wear one**: `canEquipSlot` returns true for the slot for
  every class. The Goose's "equips nothing" joke reads as arms and
  armor — a memory is not equipment — so no M97 reward is ever dead on a
  Goose party (**owner-approved 2026-08-16**; game_design's Goose entry
  carries the carve-out).
- **Debug**: "Grant 1x each heirloom". **Forge**: item descriptors
  gained `triggers` and the lowHp pair; type/slot pickers follow the
  extended content tables.

## Deviations

- **Indicators ride the existing message + log flow** (the M75 trigger
  text channel), not the plan's separate floating quip: every firing is
  announced in the battle message and the log; the extra floating quip
  and settings-gated sprite pulse were simplified away as redundant with
  the channel that already exists. Judge readability at matrix 183.
- **Icon**: heirlooms reuse the "relic" keepsake glyph; a dedicated
  heirloom icon is deferred to an art pass.
- **DEF/SPD conditional variants** (plan's "optionally") were not
  authored — attack-only keeps the first sixteen legible; the schema
  extends later without a rules bump only if a new field stays additive.
- **CrystalForge's Sim Lab party spec has no heirloom column** (recorded
  in the 2026-08-16 doc audit): trigger effects are not quick-simmable
  from the Forge. Never in this milestone's scope; the `[heirloom]`
  battery covers them headlessly.

## Compatibility

Old saves: slot empty, byte-identical battles (a party wearing nothing
resolves exactly as v17 — the pins prove it). Scoreboard: heirlooms are
equipment; gear is not tagged (M19 reasoning). Item count 105 → 121
(pin updated with the M-reference).

## Automated validation

- Debug + Release builds clean; capture **111/111** scenes clean.
- Targeted batteries green: `[heirloom]` + `[content]` — 831 assertions
  in 42 cases. The follow-up full suites surfaced three stale pins
  OUTSIDE those batteries (`test_editor_enum_lists` counts,
  `test_arms_icons`' non-gear branch, `test_equip_shop_filter`'s
  stocked-partition count); all three were test-side updates to this
  milestone's recorded design and were fixed in the M97 session (see
  M97_hooded_goose.md, "M96 stragglers"). The re-run full suite rides
  the session completion report.
- New `test_heirlooms.cpp`: authoring shape (the ==16 pin, both owner
  examples asserted by id), trigger attach + FirstTime one-shot firing
  through `beginUnitTurn`, HealSelfPct cap + alive-only, the conditional
  edge (query-level truth table PLUS a real damage-delta comparison at
  500% foe scale), fourth-slot save round-trip, unknown-id drop, and
  goose-can-wear.

## Manual owner checklist

Matrix row **183** — includes judging effect strength against
accessories and the announcement readability call.

## Documentation updated

`docs/milestones.md` (M96 row) · `docs/game_design.md` (Heirlooms
paragraph before the King's classes) · `docs/technical_design.md` (§49)
· `docs/manual_test_matrix.md` (row 183).

## Final status

`implemented, awaiting manual approval`
