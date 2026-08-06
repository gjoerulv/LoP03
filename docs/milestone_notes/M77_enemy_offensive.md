# M77 — Enemy & boss offensive pass: the new statuses in enemy hands

Authorized 2026-08-05 as part of the M75–M86 expansion program (see the
program section in `docs/milestones.md`). Implemented 2026-08-05 on the
post-M76 checkout (base commit `e73fed6`).

## A. Status

**◑ implemented, awaiting manual approval** — implemented 2026-08-05.
Evidence in §F; the balance-tuning ladder that produced the shipped numbers
is recorded in §E.

## B. Goal (owner brief)

Content authoring on the v15 engine: enemies and bosses actually use
Reflect, Sleep, Curse, MP damage and the trigger system; the King and the
Deadly Duck get their reworks; lackluster boss/elite AI gets triggers; boss
introduction text stops coaching the player.

## C. As implemented

Seven new enemy-side skills (`data/skills.json`), roster edits across
`data/enemies.json` / `data/bosses.json`, every boss telegraph rewritten
flavor-only, and two scoped v15 amendments the new content is the first to
reach (§E). No version motion: battle rules stay 15, generation stays 14,
schemas untouched.

### Roster-wide

- **MP damage** (`mpDamagePct: 25`, the owner's quarter rule): **Thought
  Thief** on the Hex Wisp (town 2) and Void Weaver; **Soul Tithe** on the
  Soul Render (town 6).
- **Reflect starters** (`initialStatuses`): the Rune Sentry and the Goose
  Trickster (enemies); the Crystal Sorcerer and the Deadly Duck (bosses).
  Mirrorbreak (M76) finally has marks.
- **Cursers**: **Leaden Hex** (support, wounds + Curse) on the Blight
  Matron, the Dread Sovereign, and the Throne Blade (the King's
  curse-and-cut minion).
- **Party-wide sleep**: **Veil of Slumber** on the Blight Chanter (town 4
  elite) and the Hollow Sovereign (town 5 boss) — their modest MP pools are
  the natural cadence limit. The King and Duck carry their own (below).
- **Dark weaknesses** (owner decision: a few enemies, exactly one boss):
  the Wisp, the Crystal Guardian, the Throne Blade; boss: the **Crystal
  Sorcerer** — whose new opening mirror makes the Rogue's physical Dark
  opener the town-1 counterplay story. A test pins the exact sets and that
  no foe anywhere is Dark-immune.
- **Triggers** on the lackluster: Troll Berserker (half HP → ATK +25%),
  Obsidian Colossus (every 4th hit taken → the attacker is stunned),
  Frost Monarch (half HP → Reflect), Deep King (quarter HP → DEF +50%),
  Sand Warlord (every 4th own turn → party-wide Blind), Abyssal Tyrant
  (first fallen follower → ATK +25% / SPD +20%). Every rule announces
  itself with an authored line.

### The King (castle)

- **Kit** (order is AI priority): the M44 control quartet leads —
  blinding_curse, silencing_word, maddening_gaze, withering_touch — then
  the new **Drowsing Verdict** (wounds, then sleeps) and **Royal Decree**
  (wounds, then stuns; authored duration 2 = exactly one stolen turn, the
  Tax Sheets precedent), then cataclysm and smite.
- **Party-wide sleep** is a TRIGGER, not a skill: every 5th of his own
  turns — *"The King hums an old cradle-song, and the whole hall grows
  heavy-lidded."* (§E for why.)
- `avoidSleepingTargets`: his single-target actions spare sleepers while
  anyone else stands; all-asleep and he picks someone off (waking them).
- **Throne Stave**: below 40% of its own HP it casts **Reflect on the
  King**, once (`status_boss`). **Throne Blade**: leads with Leaden Hex.

### The Deadly Duck (Goose Town)

- Opens **behind a mirror** (`initialStatuses` Reflect) and regains
  nothing — Mirrorbreak or patience.
- **Final Notice** — every 4th of his own turns the whole party is stunned
  (trigger), and his all-sweeping basic attack the same turn supplies the
  damage: the Tax-Sheet effect turned against the party.
- **Duck Down** — every 12th deliberate connecting hit the party lands
  shakes loose a blizzard of down: the whole party sleeps (hit-reactive
  trigger, so the party's own tempo pays for it).
- **Immune to the Deadly Spoon** (`immuneToStatScale`) — his one former
  counterplay closes; `itemAffects` keeps the relic from being wasted.
- **Manners**: `noStunWhileAllFoesSleep`, honoured on the trigger path too
  (§E) — while the whole party sleeps, the Notice's beat passes politely.
- His sweep rider is now **ATK-down only** — the old poison rider is
  retired (§E, the load-bearing balance deviation).
- **One chink in the feathers (owner decision 2026-08-06): a Curse
  lands.** His M61 blanket `immuneToAfflictions` became a bespoke
  `statusImmunities` list covering everything EXCEPT Curse, so the Evil
  Duckling finally has its worthy target — fight duck with duck. Every
  other affliction still bounces; stat debuffs still land.
- One goose curses the whole party (**Hexwing's Grudge**); the Trickster
  starts mirrored.

### Presentation

All 14 telegraphs rewritten as pure atmosphere in the same dry voice — no
thresholds, no mechanics, no imperatives. A lint test bans the old coaching
lexicon ("below half", "silence it first", "guard the first strike"…).

## D. Files changed

- **Engine (two scoped v15 amendments, §E):** `src/battle/Battle.cpp`
  (support-loop gate for enemy `all_enemies` support skills; trigger-stun
  manners in `applyTriggerAction`), `src/battle/Battle.hpp` (changelog
  note; version stays 15).
- **Data:** `data/skills.json` (+7: thought_thief, soul_tithe, leaden_hex,
  veil_of_slumber, royal_decree, drowsing_verdict, hexwings_grudge),
  `data/enemies.json` (11 entries touched), `data/bosses.json` (all 14
  telegraphs; 9 entries mechanically changed).
- **Tests:** `tests/test_enemy_offensive.cpp` (new, 13 cases,
  `[offense]`), `tests/CMakeLists.txt`, `tests/test_content_loader.cpp`
  (skills pin 65→72), `tests/test_goose_town.cpp` (the gauntlet battery
  rebuilt around the M76 counterplay — the Spoon is closed),
  `tests/test_royal_relics.cpp` (the King hook learns Mirrorbreak).
- **Docs:** this note, `docs/milestones.md`, `docs/game_design.md` §10,
  `docs/technical_design.md` §31, `docs/manual_test_matrix.md`.

## E. Plan deviations (and the tuning ladder that forced them)

1. **Two scoped engine amendments inside v15, no version bump.** (a) The
   enemy AI's support-skill gate treated `all_enemies` support skills like
   self-buffs — it checked the CASTER for the status, so a party-wide
   sleep/curse would re-cast every single turn. It now gates on the
   profiled party target. (b) Trigger-borne stuns honour
   `noStunWhileAllFoesSleep`. Both paths are provably unreachable by
   pre-M77 content (no earlier foe carries an all-enemies support skill or
   any trigger — a test pins the exact carrier sets), so no recorded v15
   battle changes; the version holds at 15. If the owner prefers the
   strict M61 reading (any engine motion bumps), say so and 15→16 lands
   in review.
2. **The King's and Duck's party-wide abilities are TRIGGERS, not
   skills.** The planned skill shape was built first and battery-tested:
   with AI-chosen AoE status skills the gate reopens before the boss's
   next action (sleep and stun are short by design), producing a
   zero-agency lockdown — the party acted roughly one round in three
   against the Duck, and the King, conversely, spent every turn on
   1-turn naps instead of his control quartet and LOST to an unaided
   party in 12 rounds. Triggers carry an authored cadence (5th turn /
   4th turn / 12th hit); both fights re-proved at their intended bars.
   The transient dev skills (`sovereigns_lullaby`, `final_notice`,
   `duck_down`) were removed; their lines live in the trigger texts.
3. **The Duck's poison sweep-rider is retired** (ATK-down stays). Under
   the v15 poison rule (magnitude + applier Magic/4, owner-approved in
   M75) his old authored 8 becomes 8 + 230/4 ≈ 65 → ~130 damage per
   member per turn ≈ 520/round for free — roughly TRIPLE his old total
   output, invisibly, from a rider authored under v14 math. With it, the
   gauntlet was arithmetically unbeatable (five seeds: party wiped by
   round 8 with the Duck at 43–70%); without it, 3/5 seeds fall at 9–11
   rounds and the losses die at 26–30% — a knife's-edge superboss instead
   of a wall. This is the closest-to-approved-feel adaptation of v14
   content to v15 rules; if the owner wants poison back in some form, it
   needs a different vehicle (say, a goose) — the Duck's own Magic makes
   any poison of his lethal.
4. **The Curse chink — offered 2026-08-05, owner-approved and shipped
   2026-08-06** ("Curse (Evil Duckling) should work on the Deadly Duck.
   Everything else stays as-is."). The Duck's blanket
   `immuneToAfflictions` became a `statusImmunities` list covering
   everything except Curse — schema-driven, no engine motion. The battery
   confirms the measured profile: 3/5 seeds win and the losses die at
   5–12% Duck HP (vs 26–30% without the chink); the gauntlet script
   spends its one held duckling on him the moment the fight opens.
5. The Duck-gauntlet battery's "obtainable kit" is now: Mirrorbreak and
   Absolve casts, Holy Taxes ×2, elixirs ×6, phoenix tears ×2, plus
   shield-bash/hamstring debuff upkeep — all currently obtainable; the
   Deadly Spoon is gone from it by design. (M78's caps will shrink the
   elixir count; that milestone re-proves under its own rules.)
6. Skill-count pin motion in dev: +10 authored, −3 converted to triggers
   → net +7 (pin 65→72).

## F. Automated validation (all run in this session, 2026-08-05)

- Debug build: clean, zero project-code warnings.
- **Full Debug suite: 654/654 tests green** (13 new `[offense]` cases).
- **Capture lint: 85/85 scenes clean.**
- **Goose gauntlet battery: 3/5 seeds clear the full gauntlet** (wave
  5/5); with the 2026-08-06 curse chink the two losses die at **5–12%**
  Duck HP (26–30% before it). Seeds recorded in the test.
- **King bars re-proved** (L99 maxed party): unaided **loses in 17
  rounds**; the modest relic plan (1 Tax Sheets + 1 Evil Goose + snacks
  + a Mirrorbreak for the Stave's mirror) **wins in 21 rounds with one
  survivor** — tenser than pre-M77 (15 rounds, 2–3 alive), bar intact.
  The Boss Rush remains uncleared by the sim floor; endless still
  bounded.
- **Blast radius pinned:** a test asserts exactly which foes carry
  triggers / initialStatuses / manners / all-enemies support skills —
  every other foe resolves its battles exactly as before M77.
- Telegraph de-hint lint green across all 14 bosses.
- `CrystalForge --canonicalize` run after the hand-edits (1 file
  rewritten, 0 content errors before and after) — the byte-stability
  test guards it.
- **Release build + suite: 650/650 tests green** (the 4-case gap to
  Debug is the debug-only god-mode battery, as always).
- First-pass honesty notes: (a) the first full-suite run failed the
  canonical byte-stability test — my hand-formatted trigger arrays were
  not canonical; fixed by running the canonicalizer itself. (b) The
  telegraph lint's first run caught my own banned-word list snagging the
  Keep WARDen's name ("ward" → "spells"). (c) The balance ladder that
  reshaped the King's kit order, moved both party-wide abilities onto
  triggers, and retired the Duck's poison rider is §E — each step was
  battery-measured, not guessed.

## G. Manual owner checklist

1. **The King.** Watch for: the cradle-song on his 5th/10th/… turns (whole
   party sleeps, one lost turn each); his singles sparing sleepers; Royal
   Decree (wound + one stolen turn) and Drowsing Verdict (wound + sleep);
   the Throne Stave's mirror closing around him when the Stave is nearly
   down (then Mirrorbreak or physicals); the Throne Blade's Leaden Hex
   (CRS chip — check Absolve/Holy Taxes lift it, double-cost MP while it
   lasts). Judge: difficulty, fairness, readability.
2. **The Duck.** He opens mirrored; Final Notice on his 4th/8th/… turns
   (all stunned + swept); the down-blizzard when your 12th blow lands;
   the Deadly Spoon is refused ("utterly unmoved" / kept in the bag); no
   Notice while the whole party sleeps. **Use the Evil Duckling on him**:
   the CRS chip lands (your approved chink — his sweeps halve while it
   holds) and the punchline rides the quip line; every other affliction
   still bounces. Judge whether the fight is a knife's-edge or a wall.
3. **Town bosses, spot-checks:** Crystal Sorcerer (town 1: opens mirrored,
   weak to Shadow Strike's Dark); Frost Monarch (mirror at half); Obsidian
   Colossus (your 4th hit rings back — attacker stunned); Sand Warlord
   (party-wide blind every 4th turn); Blight Matron / Dread Sovereign
   (Leaden Hex); Hollow Sovereign (Veil of Slumber).
4. **Trash with teeth:** Hex Wisp / Void Weaver / Soul Render drain MP
   (~¼ of the hit); Rune Sentry opens mirrored; Blight Chanter lullabies.
5. **Telegraphs** read as atmosphere, never advice — and still land in
   two lines.
6. On any failure: the battle log text, a save, and what you fought.

## H. Known limitations

- The Duck gauntlet's sim record is 3/5 by design margin — winnable but
  savage; the FEEL (and the §E.4 curse-chink decision) is the owner's.
- Veil-of-Slumber carriers are MP-starved by their own stats (1–3 casts a
  fight) — intended cadence, but worth feeling in play.
- Reflect on a foe bounces only MAGIC; the King's/court's support-category
  wounds pierce a future party mirror by the same rule (symmetric,
  documented in `game_design.md`).
- The M58 "10% per living goose" King-scare and the M61 quack rolls are
  untouched.

## I. Final status

`implemented, awaiting manual approval`
