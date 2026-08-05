# M84 — Guild Masters & town milestones

Authorized 2026-08-05 as part of the M75–M86 expansion program (see the
program section in `docs/milestones.md`). Re-audit this note against the
then-current checkout before implementation begins. The largest milestone
of the program — may be split into a systems slice and a content slice at
implementation time if review size demands (recorded here if so).

## A. Status

☐ planned

## B. Goal (owner brief)

Every town's Guild hides a Guild Master. Clearing a 4-floor dungeon in a
town unlocks "Fight the Guild Boss" there: a gauntlet at roughly depth-20
difficulty — one random wave of five town enemies, then a unique new boss
with 1–3 unique new minions. First victory grants a permanent **town
milestone**: pick one of two perks. Guild Masters also invade the Endless
Rush every tenth wave.

## C. Scope (planned)

### Unlock & records

- Completing a **4-floor** dungeon in town N sets that town's persistent
  `guildUnlocked` flag; until then the Guild option shows
  **disabled-with-help-text** (the pattern M85's Dragon menu reuses).
- **Refightable** (owner decision 2026-08-05): per-town best-turns record
  + defeated flag; the milestone perk is granted **once, on first
  victory**.

### The gauntlet

- Castle-challenge machinery (persistent HP/MP, no free healing between
  waves, the castle defeat price): wave 1 = **five seeded-random enemies
  from that town's unlocked pool** (deterministic from a committed
  challenge seed); wave 2 = the town's **Guild Master + 1–3 minions**.
- Scaled to ~**depth-20** threat plus the town's own scaling; balance bar
  is the castle standard (counterplay-assisted; battery evidence with
  seeds recorded).

### Content: 7 Guild Masters + minions

- Seven unique new bosses (one per town, themed to their town's identity)
  with 1–3 unique new minions each (all-new enemies, `bossOnly`), authored
  on the v15 vocabulary — triggers, initialStatuses, the new statuses —
  so each Master is mechanically distinguishable. M73-style hand-authored
  sprites through the manifest; flavor-only telegraphs; original names.

### Town milestones (pick 1 of 2, M63 modal pattern, stored per town)

| Town | Option A | Option B |
|------|----------|----------|
| 1 | Max Items +1 | EXP after battle +10% |
| 2 | Map-piece chance +5% | Max Items +1 |
| 3 | Enemy gold +10% | Black-market appearance chance up |
| 4 | Trap damage −5% | Chest gold +15% |
| 5 | Deadly-Spoon-event chance +5% (worded cryptically) | Map-piece chance +5% |
| 6 | Max Items +1 | EXP after battle +10% |
| 7 | Legendary token price 3 → 1 | EXP after battle +15% |

- Hooks: M78 `capBonus` (ceiling 9), `grantPartyXp`, enemy-gold payout,
  chest-gold and trap-damage computation, the black-market and
  relic-event rolls, M83 map chance, the black-market token price.
- Perks are replayability hooks on the same loop, not a narrative tree
  (the roadmap's focused-game bar; scoreboards already tag `partyLevel` —
  no new comparability tag needed, recorded reasoning).

### Achievement & Endless Rush

- New achievement: defeat a Guild Master (original name authored here).
- **Endless Rush**: every 10th wave becomes a seeded-random boss — a
  normal dungeon boss or a Guild Master — **with its usual minions**,
  derived from `kEndlessSeed` + wave index (deterministic, reproducible).

## D. Schema, save & version implications

- New optional save fields: per-town guild flags/records and the chosen
  perk ids (shape decided at implementation; defensive drops). No
  rules/generation/score version motion (content rides v15; the Endless
  change derives from the fixed seed).
- New content entries only; any new boss-schema need should already exist
  from M75 — a gap found here goes back into the schema with validation.

## E. Out of scope

The Dragon (M85). CrystalForge descriptors (M86).

## F. Dependencies

M82 (4-floor completion unlock), M83 (map-chance perk hook), M75–M77
(vocabulary + counterplay precedent), M78 (capBonus hook).

## G. Acceptance criteria

- Unlock/record/perk state persists per town across saves; perk effects
  measurably apply at every hook; the modal offers exactly the table above.
- Each Master is beatable at the castle bar (evidence recorded) and
  mechanically distinct; wave composition is deterministic per seed.
- Endless waves 10/20/30… field the boss + minions deterministically;
  the achievement fires on any Master's first fall.

## H. Automated validation

A new [guild] battery (team shapes, scaling, determinism, refight rules,
perk math at every hook, save round-trip); Endless wave tests; capture
scenes for the guild menu, perk modal, and a Master fight. Full suite
green.

## I. Owner manual validation

Unlock and fight at least two Masters (low and high town): difficulty,
distinctness, perk choice weight, the cryptic T5 wording, and an Endless
run to wave 10+. Confirm a pre-M84 save loads with everything locked.
