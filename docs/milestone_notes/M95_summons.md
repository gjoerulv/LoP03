# M95 — Summons: the realm's legends answer (battle rules v17)

**Status:** complete (approved 2026-08-16)
**Program:** M88–M97 (authorized 2026-08-14). Battle rules **16 → 17**.
No generation or save-schema change (`usedSummons` is runtime-only by
design — a reload of the entry autosave starts the ledger fresh, like
every other run-runtime state).

## Scope (owner item 8)

New Map scrolls include **Summons**: very strong once-per-run attacks or
heals, an epic lore-friendly creature (the owner named "Mighty G. Goose"),
very high MP costs, able to Terrify (existing immunes stay immune), debug
support.

## What was built

- **Three legends** (`data/skills.json`): Mighty G. Goose (physical
  all-foe 42, 80 MP, Terrified), the Starfall Sentinel (holy magic
  all-foe 38, 70 MP, Terrified), the Radiant Spring (all-ally heal 90,
  100 MP). All `oncePerRun` with `summonName` (new SkillDef fields,
  loader-validated, CrystalForge descriptors updated).
- **Acquisition — the Map economy** (the owner's words): the treasure-dig
  pool grew 6 → 9; after the six Lost Scrolls the digs pay the three
  summon scrolls in award order (the M64 learn-on-the-spot flow and the
  token+gold fallback unchanged).
- **One shared once-per-run gate** (the rules bump):
  `Party.usedSummons` → `Battle.usedSummons` at build;
  `battle::summonSpent()` feeds the battle menu (USED + grey), all three
  enemy-AI skill loops (no foe ever answers a call — belt over the
  content rule), and `useSkill`'s refusal on the silence pattern (refuse
  before any mutation; record the cast the moment it is real). The
  writeback carries the ledger with HP/MP; resets at dungeon, castle-
  challenge, guild-boss (same state), and treasure-fight entry; the
  spar's whole-party restore covers itself.
- **Presentation**: "<name> answers the call!" on the quip channel, plus
  the M51 all-target tint and the M91 element accents.
- **Debug**: "Grant summon scrolls", "Reset used summons" (with a
  used-count readout).

## Re-audit decision (RESOLVED 2026-08-16: the King can cower)

The plan recommended adding Terrified immunity to the **Hollow King**.
Dropped on re-audit: M44 (owner-approved) states **"The King is NOT
immune to any of them: that is the whole point"** — the Evil Goose relic
terrifies him BY DESIGN, and a summon's terror is the same status through
the same chokepoint. Making him immune would silently break the M44
counterplay puzzle. "Some bosses immune" stands satisfied by the Deadly
Duck (blanket affliction immunity) and the Last Dragon (listed), plus
every already-immune foe. If the owner wants the King summon-proof
anyway, that needs a distinct mechanism (a summon-specific immunity) and
an M44 design amendment — a deliberate decision, not a data flag.

**Owner resolution (2026-08-16, recorded in the M98 preamble):** the King
**can** be Terrified by summons — the implemented behavior stands and no
summon-specific immunity will be added. M44's counterplay design is
reaffirmed; game_design's King entry now records it.

## Deviations

- **The Radiant Spring heals only** (no cleanse rider): a heal+cleanse
  composite risked the M47/M62 Purify rules; recorded for the owner.
- **The creature silhouette is deferred**: the summon's beat is the named
  quip + tint + element accent. Sprite sheets can land later without
  touching the wiring; judge whether the banner carries it (matrix 182).

## Compatibility

Saves/settings/generation untouched. Scoreboard entries tag rules v17
going forward; old entries keep their versions. Content counts moved:
81 skills, 105 items, 19 scrolls (pins updated with M-references).

## Automated validation

- Debug + Release builds clean; capture **111/111** scenes clean.
- Targeted batteries green (summons/treasure/scroll/content: 1049
  assertions). Full `ctest --preset debug` launched after the batch —
  the prior full run (M88–M94 binary) was **763/763**; the M95 result
  rides the session completion report.
- New `test_summons.cpp`: authoring brief, pool order + award-after-six,
  the shared gate (cast → spent → refused unspent → next-battle
  persistence → fresh-run reset), terror lands/shrugs correctly
  (plain boss vs Duck/Dragon), and the AI-never-answers belt.

## Manual owner checklist

Matrix row **182** — including the King-terror veto point and the
deferred-visual judgment.

## Documentation updated

`docs/milestones.md` (M95 row) · `docs/game_design.md` (§10 summons
paragraph) · `docs/technical_design.md` (§48) ·
`docs/manual_test_matrix.md` (row 182).

## Final status

`complete (approved 2026-08-16)` — the ledger is authoritative; this line
lagged at the approval flip and was corrected 2026-09-02.
