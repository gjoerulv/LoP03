# M81 — Arms, elements & icons: resist accessories, elemental weapons, gear icons

Authorized 2026-08-05 as part of the M75–M86 expansion program (see the
program section in `docs/milestones.md`). Re-audit this note against the
then-current checkout before implementation begins.

## A. Status

☐ planned

## B. Goal (owner brief)

The party gets an elemental defense layer (per-element half-damage
accessories plus two new legendaries), more elemental weapons filling the
town tiers, and every piece of equipment gets a categorized icon wherever
it is listed.

## C. Scope (planned)

### Equipment schema

- `elementResist` — element → percent, read by the M75 damage-calc hook
  (50 = half damage of that element taken by the wearer).
- `iconCategory` — sword / axe / dagger / bow / staff / mace / spear /
  shield / armor / accessory / relic (final list matched to the real
  catalog); optional, validated, with a category-derived default where the
  name makes it obvious.

### New accessories (`data/items.json`)

- **Six resist accessories** — one per element (Fire/Ice/Lightning/Earth/
  Holy/Dark), 50% resist each; buyable across the town ladder (minTown
  spread) and present in the chest pool. Dark-resist ships even though Dark
  stays enemy-only offensively — enemies do deal Dark.
- **Legendary +200 SPD accessory** and **legendary all-elemental −50%
  accessory** — black-market/boss-drop tier, priced with the existing
  legendary economy.

### New weapons

- New **Fire/Ice/Lightning/Earth/Holy** weapons filling town-tier gaps,
  priced/statted consistently with the M54 rebalance curve (no Dark
  weapons — owner ruling 2026-08-05; `shadow_strike` is the lone Dark
  exception, done in M76).

### Coverage audit

- Verify enough enemies/bosses actually deal elemental damage for resists
  to matter across the ladder; add elemental skills to a few thin spots
  (content-only; the M48 "sparse means sparse" bar still applies — resists
  should matter, not demand a coverage matrix).

### Icons

- Hand-authored pixel-grid icons via the asset manifest (the M73 pipeline
  + `tools/asset_gen/preview.ps1` review harness; grids consume no RNG).
- Rendered wherever equipment is listed: equip-shop buy/equip lists, the
  party panel's equipment lines, and the black market offer.
- Lint: every equipment id resolves an icon (category fallback allowed in
  code, shipping content relying on it is a lint failure — the M25
  distinctness precedent).

## D. Schema, save & version implications

- Optional content fields only (`elementResist`, `iconCategory`); loader
  validation; **no battle-rules bump** (the hook shipped inside M75's v15;
  resist-less content resolves identically). No generation/save motion.

## E. Out of scope

Enemy-side element authoring beyond the coverage audit (M77 owns enemy
kits); CrystalForge descriptors (M86); the Dragon's breaths (M85).

## F. Dependencies

M75 (resist hook + battle-log element line). M78 recommended first (shop
gates final before new stock lands) but not blocking.

## G. Acceptance criteria

- Wearing a resist accessory halves matching incoming elemental damage
  (visible in the log with the element line); the two legendaries work and
  sit in the legendary economy.
- New weapons appear in their towns at sane prices; sim batteries show no
  tier inversion against the M54 curve.
- Every listed equipment row shows a correct icon at native resolution.

## H. Automated validation

Resist-math tests (per-element, all-element, stacking rules defined and
tested); schema validation; icon lint; economy battery re-run; captures of
the icon-bearing screens. Full suite green.

## I. Owner manual validation

Shop the new gear across towns, fight elemental enemies with and without
resists, and review icon readability (grayscale check per the art bible).
