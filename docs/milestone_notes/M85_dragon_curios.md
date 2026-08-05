# M85 — The Dragon & curio lore

Authorized 2026-08-05 as part of the M75–M86 expansion program (see the
program section in `docs/milestones.md`). Re-audit this note against the
then-current checkout before implementation begins.

## A. Status

☐ planned

## B. Goal (owner brief)

The twelve curios finally talk — each inspectable for funny, Deadly-Duck-
related lore — and completing the collection unlocks the King's nemesis at
the castle: **the Dragon**, a Duck-grade gauntlet introduced by a new
jester with dry lore.

## C. Scope (planned)

### Curio inspection

- New `data/curio_lore.json` (schema v1): one entry per curio (12), each
  an original dry-humor lore text tied to the Deadly Duck mythology.
  Validated (complete coverage), defensive fallback (missing entry ⇒ the
  curio simply shows its name as today).
- The Maps screen's curio collection grid gains an **inspect** action:
  select a curio → a lore panel (wrap-safe, capture-linted).

### The Dragon at the castle

- The castle hub gains **"Fight the Dragon"** beside the King's
  challenges — **visible but disabled** until `ownedCurios == 12`, with a
  help text in the funny register ("Find all the collectibles first" or
  better); the disabled-with-help pattern shared with M84's guild option.
- A **new jester NPC** (original character, distinct from the Goofy
  Jester) introduces the fight with a dry lore tale (story machinery,
  text fits the panel).
- **Gauntlet**: Duck-style challenge — **three seeded-random elite waves,
  then the Dragon** (four fights total; persistent HP/MP, no free healing,
  castle defeat price, best-turns record; ~the Duck's difficulty band,
  500% scale).

### The Dragon itself (authored on the v15 vocabulary)

- **Highest HP in the game** — base ~2000 (above the Duck's 1000;
  sim-tuned, value recorded here).
- Immune to **every status except ATK−, DEF−, Curse and Poison**; never
  uses Reflect; **immune to Fire** (element immunity).
- **Breath attacks, one per element**, each hitting the entire party
  (magic-category, so party Reflect play is moot by design — he has none
  to bounce; the M81 resist accessories are the counterplay).
- Triggers: **once below 50% HP — depletes the party's MP**; **below 10%
  HP — ATK & SPD double**; **can clone itself at 5% of its full HP**
  (the M75 `summonClone` action; exact firing condition authored here and
  recorded).
- Passives: **First Strike, Lifedrink, Spell Ward**.
- Save: dragon defeated / best-turns records (optional fields).
- New **achievement** for felling the Dragon (original name).
- Sprite: M73-style hand-authored grid (a boss silhouette that reads as
  the King's nemesis); flavor-only telegraph.

## D. Schema, save & version implications

- New content file `data/curio_lore.json`; new boss/enemy entries; new
  optional save fields (dragon records). No rules/generation/score
  version motion — everything rides the v15 vocabulary.

## E. Out of scope

CrystalForge support for the lore file (M86). Any change to how curios
are found (M66 mechanics untouched).

## F. Dependencies

M75/M77 (vocabulary + precedents), M81 (elemental breaths meet the resist
layer), M84 (the disabled-option pattern and the gauntlet conventions).

## G. Acceptance criteria

- All 12 curios inspectable with fitting text; the collection screen
  stays overflow-clean.
- The Dragon option is visible-but-disabled pre-completion with the funny
  help text, and opens at 12/12.
- The gauntlet runs 3 elite waves + the Dragon with persistent HP; every
  §C mechanic observable and deterministic; beatable at the castle
  counterplay bar (battery evidence, seeds recorded).
- The immunity matrix holds: ATK−/DEF−/Curse/Poison land, everything else
  (incl. the Spoon? — no: the Spoon is stat-scaling, not a status; decide
  and record whether the Dragon honors or refuses it) bounces.

## H. Automated validation

A new [dragon] battery: immunity matrix, breath coverage (one per
element, all-party), the three triggers (one-shot MP deplete, the <10%
double, the clone), gauntlet flow/records, unlock gate, achievement
predicate, lore-file validation. Captures for the disabled menu, the
jester tale, and the fight. Full suite green.

## I. Owner manual validation

Complete the curios (debug-assisted), read a few lore texts, hear the new
jester, and fight the Dragon to the end: judge difficulty vs the Duck,
the breath/MP-deplete drama, the sub-10% panic phase, and the clone gag.
