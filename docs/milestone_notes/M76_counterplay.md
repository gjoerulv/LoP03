# M76 — Counterplay: breaker skills, Holy Taxes & the Evil Duckling

Authorized 2026-08-05 as part of the M75–M86 expansion program (see the
program section in `docs/milestones.md`). Re-audit this note against the
then-current checkout before implementation begins.

## A. Status

☐ planned

## B. Goal (owner brief)

Before enemies start using the M75 statuses (M77), the party gets its
counterplay: Ranger and Rogue can break Reflect, the Cleric (and a new item)
can lift Curse, the Knight gains Smite, and a new rare dungeon event sells
the party a cursed duck of its own.

## C. Scope (planned)

### Skills (`data/skills.json` + learnsets in `data/classes.json`)

- One new shared **Reflect-breaker** skill (original name; uses the M75
  `SkillEffect` reflect-breaker) added to **both** the Ranger and Rogue
  learnsets (~level 9–12; exact levels tuned against the existing curves).
- New **Cleric Uncurse** skill (original name; the M75 Uncurse effect,
  ally-targeted) — with Holy Taxes, the *only* ways to remove Curse.
- **Knight learns `smite`** (the existing Cleric holy skill) at an
  appropriate learnset level (owner: "Holy Elemental").
- **`shadow_strike` becomes Dark-elemental** (owner decision 2026-08-05;
  the one player-side Dark exception — Dark weapons stay reserved).
- Verified by test: Purify and Remedy now also lift Sleep but never Curse.

### Items (`data/items.json`)

- **Holy Taxes** — consumable, removes Curse from an ally. Buyable in town
  item shops from ~town 3 (~200g) and in the chest pool; generic cap 2
  (cap enforcement itself lands in M78).
- **Evil Duckling** — consumable, `battleTarget: enemy`, applies Curse.
  Max **1** held; consumed on use (re-obtainable later via the event).
  Using it displays a **Hilarious Punchline**: a data-driven line, seeded
  pick from several authored punchlines (presentation-only, no RNG-stream
  motion — the quip/jest-line precedent), shown as a banner + log line.

### The Evil Duckling event

- New **rare dungeon room event**: an opportunity to buy the Evil Duckling
  for **300 gold**. Never triggers while the party owns one (checked at
  generation/roll time). Rides the existing seeded event machinery (the
  RoyalRelic replacement-roll precedent) so reload cannot farm it.
- Presentation uses the current event prompt; it inherits the centered
  flavor-text treatment automatically when M80 lands.

## D. Schema, save & version implications

- No rules/generation/save version motion (all on the v15 engine).
- New optional item field(s) for the punchline text (e.g. `useLines`),
  validated; new event kind in the room-event enum.
- Item/skill reference checks extended for the new ids.

## E. Out of scope

Enemy usage of the statuses (M77); cap enforcement (M78); centered event
flavor presentation (M80); CrystalForge descriptors (M86).

## F. Dependencies

M75 (statuses + skill effects + curse rules).

## G. Acceptance criteria

- Ranger and Rogue can strip an enemy's Reflect; the Cleric skill and Holy
  Taxes are the only Curse removals (proven by test).
- The Knight has Smite in play; `shadow_strike` shows Dark in the battle log.
- The duckling event: seeded, rare, 300g, suppressed while owned; the
  punchline shows on use and the Curse lands.

## H. Automated validation

Learnset/loader/reference tests; curse-removal exclusivity tests; event
trigger/suppression determinism tests; full suite + relevant batteries.

## I. Owner manual validation

Buy and use the counterplay in a real run: break a (debug-applied) Reflect,
lift a Curse both ways, meet the duckling event, judge the punchline and
event pacing, confirm Smite on the Knight feels right.
