# M84 — Guild Masters & town milestones

Authorized 2026-08-05 as part of the M75–M86 expansion program (see the
program section in `docs/milestones.md`). Implemented 2026-08-06 on the
post-M83 checkout (`d466e5b`). The plan allowed a systems/content split
"if review size demands"; it shipped as ONE milestone — the content is
formulaic on the M73/M77 authoring patterns and splitting would have
added an approval round-trip without shrinking any risky surface
(recorded per the note's own instruction).

## A. Status

**☑ complete (approved)** — implemented 2026-08-06; approved and
committed by the owner 2026-08-07 (`a047590`). Evidence in §F.

## B. Goal (owner brief)

Every town's Guild hides a Guild Master. Clearing a 4-floor dungeon in a
town unlocks "Fight the Guild Boss" there: a gauntlet at roughly depth-20
difficulty — one random wave of five town enemies, then a unique new boss
with 1–3 unique new minions. First victory grants a permanent **town
milestone**: pick one of two perks. Guild Masters also invade the Endless
Rush every tenth wave.

## C. What was built

### Unlock, records & the Guild screen

- `Party::guild` — `GuildRecords`, one `GuildTownRecord` per town
  (`unlocked` / `bestTurns` / `perkId`), all optional save fields
  (`guildUnlocked` 7-bit mask + flat `guildBest1..7`/`guildPerk1..7`).
  A 4-floor `completeDungeon` in town N sets the unlock.
- The Guild screen gains a **"Fight the Guild Boss"** row (kept
  navigable while locked — `ui::Menu` skips disabled rows, which would
  have hidden the goal; the row renders dim instead and Confirm refuses).
  Focusing it repurposes the stakes-banner line for the row's own story:
  the lock hint, the trial explainer, or the best-turns record (also
  shown gold on the row). The panel's paddings were trimmed 2 px so the
  new row fits without crowding the footer.

### The gauntlet

- Rides `CastleChallengeState` (new `CastleChallenge::GuildBoss` + a
  `guildTown` ctor param): persistent HP/MP, no free healing, the castle
  defeat price (`clampCastleDefeat`), no gold taken. Wave 1 = **the
  Guild Trial**: five picks from the town's unlocked pool (every
  non-bossOnly enemy with `minTown ≤ town`) hashed from the fixed
  `kGuildSeed` — the SAME trial every attempt, so best-turns is a
  reproducible measure (the kEndlessSeed philosophy). Wave 2 = the
  Master + authored court. Both at `guildScalePct(town)` =
  depth-20 composition scaling × the town ladder (190 % t1 → 570 % t7),
  derived from the generator's own rules, never a literal.
- Refightable; `guildImproved` mirrors the Duck's record rule. A win
  earns the M71 celebration (it rides the existing "cleared &&
  not-Endless" rule); boss music + the Plain backdrop (a town hall, not
  the throne room); the overlay is titled by the Master and returns to
  the Guild.

### Content: the seven Masters & their courts

Seven new bosses (one per town, `guildTown` 1–7) + twelve new `bossOnly`
minions, authored on the v15 vocabulary so each is mechanically distinct:
**Foreman Brakk** (brute; enrages at half HP; two Guild Clerks),
**Auditor Vess** (MP-draining `soul_tithe`; party silence every 4th turn;
Inkwing + Ledger Golem), **Chain-Mistress Karga** (attack-down riders;
stuns every 5th attacker; two Debt Hounds + a Chirurgeon healer),
**Warden Mole** (thorns tank; locks attackers; defense doubles at 40 %;
Vault Mimic + Key Rat), **The Cutlery Curator** (Spoon-immune
`immuneToStatScale`; reflect at half HP; party confusion; Fork Fiend +
Ladle Shade), **Grandmaster Ossia** (first-strike duelist; enrages on a
fallen ally; terrifies every 8th attacker; two Seat Wardens), and
**Registrar Null** (opens reflected; party-wide CURSE at half HP; holy
weakness as the counterplay door; Null Notary + The Final Clause).
Flavor-only telegraphs, original names throughout. 19 M73-style sprites
(36×36 Masters / 24×24 courts, RNG-free hand grids appended before the
M73 marker — no existing PNG's bytes moved), manifest + credits rows,
and a review contact sheet at `docs/sprite_review/guild_masters_*.png`.

### The town milestones (perks)

- `kGuildPerks[14]` — the owner's table, constexpr with stable ids;
  `GuildPerkChoiceState` (the M63 modal pattern: Confirm chooses
  permanently, Cancel postpones, town resume re-offers, one push drains
  every pending town). Pushed beneath the celebration/toasts on a first
  victory.
- Every effect is live at its hook: consumable caps (M78 capBonus, both
  shops), battle EXP and enemy gold (inside `applySpoils`, so the
  victory panel shows what was granted), chest gold at open time, the
  chest-trap wound percent, the M83 map-piece bonus (the inert hook goes
  live), the black-market chance (+10 pts on the 20 % stakes path only),
  the token price 3 → 1, and town 5's cryptic **Mind the Spoon**: a
  post-generation pure-hash pass (`applyGuildRelicOmen`, the M76 peddler
  precedent) that may upgrade one plain rolled event per relic-less
  floor into the Royal Relic event — the generator never sees party
  state, so generation stays byte-identical and v15.

### Achievement & Endless Rush

- **Guildbane** ("Defeat a Guild Master."), the 19th achievement.
- `endlessWaveTeam`: every 10th wave ((w+1) % 10 == 0) fields a seeded
  boss — the Boss Rush roster ∪ the Masters — with its authored minions,
  from `kEndlessSeed` on a salt far above the per-slot ones. Ordinary
  waves are byte-identical to pre-M84.

## D. Schema, save & version implications

- **`BossDef::guildTown`** (optional int, default 0; validated 0..7 with
  at-most-one-Master-per-town): identifies the Master AND excludes it
  from the generator's fallback boss sweep and the Boss Rush — without
  those two guards, adding the Masters would have changed what existing
  seeds generate and grown the rush. The §D "a gap found here goes back
  into the schema" clause anticipated exactly this. The CrystalForge
  descriptor shipped WITH the field (the M59 staleness sweep enforces
  it), so M86 owes nothing for M84.
- New optional save fields: `guildUnlocked` mask, `guildBest1..7`,
  `guildPerk1..7` — all defensive (unknown/wrong-town/unearned perk ids
  drop and re-offer; a recorded victory forces its unlock). No
  rules/generation/score version motion.

## E. Deviations & decisions

1. **No slice split** (allowed, not mandated): one reviewable milestone,
   reasoning in the header.
2. **Chest-gold perk credits the payout, not the score input** —
   `run_.treasureGold` keeps the generated amount so scoring stays
   comparable across parties (no score-rule motion).
3. **Black-market perk = +10 pts** on the M34 20 % path only (the plan
   said "chance up" without a number; conservative, one constant in the
   perk table).
4. **A guild win earns the M71 celebration** — it rides the existing
   castle-runner rule unchanged; suppressing it would have been MORE
   code for less delight.
5. **The locked row stays navigable** (dim + explained + polite refusal)
   rather than disabled, because `ui::Menu` skips disabled rows entirely
   and the feature's discoverability IS the row.
6. The endless king's-court lint was narrowed: `bossOnly` foes enter the
   endless arena ONLY inside a 10th-wave boss's court (the invariant now
   says exactly that).
7. **No new Dark weaknesses.** Two court minions were first drafted
   dark-weak, which would have silently widened the owner's pinned M77
   rule ("a few foes and exactly one boss"). The pin was respected: they
   ship fire-/earth-weak instead, and the M77 lint still passes
   unchanged.
8. **Guild-screen banner lines are one-liners by rule** — drawBanner
   grows downward and a wrapped second line lands under the footer; the
   92/93 capture review caught the first drafts doing exactly that.

## F. Automated validation (evidence)

- `[guild]` battery (new, 13 cases): the exact perk table incl. the
  cryptic-wording guard; record rules + pending-perk queue; effect sums
  with tamper degradation; caps + `applySpoils` integration; exactly one
  Master per town with a 1–3 bossOnly court; the derived threat bar
  (190/570 endpoints documented); trial determinism + town gating;
  Master exclusion from the rush, every theme, and generated dungeons
  (incl. the theme-less fallback); the every-10th-wave rule with
  ordinary waves untouched; the omen's no-op/upgrade/eligibility/
  idempotence; save round-trip + wrong-town tamper; Guildbane; and
  sim-backed clearability of ALL SEVEN gauntlets by the castle battery's
  maxed party — recorded: 4 / 4 / 4 / 6 / 7 / 9 / 13 rounds at
  190→570 % (a visibly escalating curve).
- Updated pins: enemyCount 50→62, bossCount 14→21, kAchievementCount
  18→19, the bossOnly census 7→19, the castle deepest-dungeon sweep now
  skips Masters, the endless lint narrowed as above.
- Debug build: **passed** (VS2022 dev shell). Full Debug suite:
  **711/711 passed**. Release build: **passed** (a transient
  "subcommand failed" on the first chain attempt rebuilt clean on
  retry — no source error); full Release suite on the fresh build:
  **707/707 passed**. Capture sweep: **95/95 scenes clean** — the four
  new scenes visually spot-checked (the review caught and fixed the
  wrapped guild-screen banners and the three-line cryptic perk text
  before sign-off). Three pre-existing M77 [offense] lints tripped on
  the new content and were resolved as §E.7 and the census update in
  §F above; the fixed [offense]/[elements]/[guild] trio re-ran green
  (1444 assertions / 47 cases) before the final full chains.

## G. Owner manual validation

1. Clear a 4-floor dungeon in town 1, open the Guild: the boss row goes
   live (before that, focus it and read the lock banner). Fight Foreman
   Brakk; judge the two-wave pacing and whether wave 1 feels like a
   trial rather than a wall.
2. Win, pick a milestone (try Cancel first — it must re-offer on the
   next town visit), and confirm the perk actually bites where it says
   (e.g. Deep Pockets: a shop row's MAX rises by one).
3. Refight the Master; the Guild row shows best turns and the rematch
   banner.
4. Fight a HIGH-town Master (5+) for difficulty and mechanical
   distinctness — the real balance question only play answers.
5. Sprite pass: the contact sheet
   (`docs/sprite_review/guild_masters_contact.png`) and one live fight —
   do the Masters read as bosses at native size?
6. Endless Rush to wave 10: the boss + court arrives; the run's feel
   with the interruption is an owner call.
7. Town 5's perk wording: cryptic enough, or too cryptic?
8. Load a pre-M84 save: every audience locked, nothing else disturbed.

## H. Known limitations

- Guild Masters enter the bestiary like any fought boss (deliberate).
- The Trial's five picks can repeat an enemy id (a hash over the pool;
  deliberate, matches Endless).
- No new music/backdrop art: boss anthem + the Plain stage (conservative;
  bespoke guild-hall dressing would be a new-asset owner call).
- The T5 omen announces nothing when it fires — that is the joke.

## I. Final status

`complete (approved)` — owner approval 2026-08-07, committed as
`a047590`.
