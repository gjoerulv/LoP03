# M28 — Enmity, AI Diversity & Control Skills

## A. Status and authority

- **Status:** implemented, awaiting manual approval — design approved and
  implemented 2026-07-20.

### Owner decisions (2026-07-20, all recommendations accepted)

1. **Threat scope:** one **global** threat value per party member.
2. **Profiles:** **derived from the existing `EnemyDef.role`** (no schema change;
   optional override reserved for M29).
3. **Variance:** **small, seeded tie-break** jitter — profile score dominates.
4. **Control-skill delivery:** **added to class starting kits** (Guardian→taunt,
   Mage→fade, Knight→redirect/intercept).
- **Authored / audited:** 2026-07-20 against the current checkout (HEAD is the
  M27 approval commit; working tree clean).
- **Authority:** `CLAUDE.md` > this note > `docs/milestones.md` >
  `docs/game_design.md` > `docs/technical_design.md`.
- **Authorization:** the owner authorized beginning M28 on 2026-07-20 after
  approving M27. This is the **highest-risk milestone**: it changes the outcome
  of every battle, invalidates the tuned balance battery, and makes prior
  scores incomparable. Implementation waits on the §D decisions.

## B. Problem statement (verified at audit)

`battle::chooseEnemyAction` (`src/battle/Battle.cpp:459`) is a **pure,
deterministic** function shared by live play (`BattleState::executeEnemy`) and
the headless `Simulator` — this shared purity is what keeps live and sim in
exact agreement, and M28 must preserve it. Today the enemy always heals a hurt
ally if it can, else targets the **lowest-HP living party member** with an index
tie-break. There is no enmity concept anywhere. The owner's reported
consequence: playing a mage efficiently turns the mage into the party's tank,
because chip damage lowers its HP and the AI piles onto the lowest HP.

## C. Goals

- Replace predictable lowest-HP targeting with a **threat model** the player can
  read and influence (via play and control skills) but not trivially exploit.
- Give enemies **distinct targeting behaviour** so different foes feel different.
- Keep the sim and live play in **exact agreement**, and every battle **fully
  reproducible** given the same inputs.
- Add **enmity-control skills** so aggro is a deliberate lever.
- Tag scores with a **battle-rules version** so pre-M28 runs aren't presented as
  comparable.

## D. Material design decisions (owner input required)

These four forks change the implementation and the player experience; each has a
recommendation. `I agree 100%` accepts all recommendations.

1. **Threat scope.** *Recommend: one global threat value per party member* (all
   enemies read the same table), accrued from damage dealt / healing done /
   skill use and decaying each round. Simpler and readable; taunt/fade move it
   directly. Alternative: a per-(enemy, party-member) table (closer to an MMO
   aggro model, more state, harder to read). Consequence of global: all enemies
   weigh the same threat numbers, but their **profiles** still make them target
   differently.

2. **Behaviour profiles source.** *Recommend: derive the targeting profile from
   the existing `EnemyDef.role`* (Bruiser→aggressive, Sniper/Disruptor→
   opportunist, Healer/Buffer→tactician, Protector→protector,
   Attrition→spread) — **no schema change, no data edits to 27 enemies**, with
   an optional `targeting` override field reserved for M29. Alternative: a new
   required `targeting` field on every `EnemyDef`.

3. **Targeting variance.** *Recommend: small, seeded tie-break jitter* — the
   profile score dominates, and only near-ties are broken by a deterministic
   per-battle RNG (seeded from the battle's initial state, so sim and live agree
   and a given encounter always resolves the same way). Alternatives: (a) **no**
   variance — most predictable, best for pure fewest-turns solving, but keeps
   targeting fully deducible; (c) **meaningful** variance — least predictable,
   but can frustrate turn-count optimization. This is a score-game feel call.

4. **Control-skill delivery in M28.** *Recommend: add the three control skills to
   the relevant classes' `startingSkills`* (Guardian→**taunt**, a squishy caster
   →**fade**, a defender→**redirect/intercept**) so enmity is playable and
   testable now; the M29 learnset later moves skill delivery onto a curve.
   Alternatives: ship them as chest **scrolls** only, or build only the sim
   mechanic and defer all player access to M29 (enmity unplayable in M28).

## E. Proposed design (assuming the §D recommendations)

- **Threat state in `Battle`.** A `std::vector<int> threat` per party member (or
  a small struct), living in `Battle` so it is mutated only through the shared
  `attack`/`useSkill` paths and therefore identical in live and sim. Accrual:
  damage dealt to an enemy adds threat ∝ damage; healing adds threat ∝ heal;
  a bare skill cast adds a small flat amount. Decay: multiply toward zero at
  each round boundary via a new `Battle::beginRound()` called at the top of the
  round in **both** the `Simulator` loop and `BattleState`'s round advance.
- **Profiles.** A pure `targetScore(profile, enemy, candidate, threat, rng)`
  weighs threat, low-effective-HP (kill pressure), and role priority (e.g.
  tactician favours party healers/casters; protector favours whoever most
  threatens its allies). `chooseEnemyAction` picks the max-scoring living party
  member instead of lowest HP; the heal/support branches are retained.
- **Variance** (per §D.3): a `Battle` RNG seeded deterministically from the
  initial roster; advanced only inside `chooseEnemyAction`, so the call order
  (identical in sim and live) keeps it in lock-step.
- **Control skills** (`data/skills.json` + sim): **taunt** (spikes the caster's
  threat for a few rounds / forces targeting), **fade** (sharply drops the
  caster's threat), **redirect/intercept** (the caster takes hits aimed at
  allies for a round). Delivered per §D.4. Mechanics live in the pure sim so
  live and sim agree.
- **`ScoreEntry.battleRulesVersion`** (owner pre-approved): new optional int
  beside `generationVersion`/`partyLevel` (default 0 = pre-M28; M28 writes
  `battle::kBattleRulesVersion = 1`). Written by `DungeonState` on a completed
  run, read via `optIntMin` (no scoreboard format bump), shown on the scoreboard
  and used so different rules versions are visibly distinguished, never silently
  ranked together.

## F. Determinism & sim/live agreement (the binding constraint)

All new state (threat, RNG) lives in `Battle` and is mutated only by shared
methods called in the same order by `Simulator::simulate` and `BattleState`.
Existing battle-determinism tests are **extended, never weakened**; new tests
assert: identical inputs reproduce a battle exactly; the sim and a scripted live
sequence agree; taunt raises and fade lowers who gets hit (measurable
redirection); and profiles target differently on the same board.

## G. Data, schema, save compatibility

- `ScoreEntry`: +`battleRulesVersion` (optional, no format bump; pre-field = 0).
- `data/skills.json`: +3 control skills (and, per §D.4, edits to a few classes'
  `startingSkills`). Loader/validator coverage added.
- `EnemyDef`: no change under the recommended §D.2 (targeting derived from role);
  an optional `targeting` field only if the owner picks the alternative.
- No save-format bump; deterministic seed behaviour for **dungeon generation** is
  untouched (this changes battle resolution, tagged by `battleRulesVersion`, not
  `generationVersion`).

## H. Balance

The new AI changes every simulated outcome, so the `[economy]` difficulty-curve
assertions (`every depth clearable`, `difficulty never inverts`) must be
**re-validated and, if needed, re-tuned** (data-driven values only), and the
`[.][economy-report]` / `[.][sim-report]` batteries regenerated and reviewed.
Acceptance: no degenerate single-tank strategy survives; taunt/redirect
measurably redirect targeting; all depths remain clearable and monotonic.

## I. Out of scope

New enemies/bosses (M29); boss mechanic rewrites beyond what enmity needs;
learnset-based skill delivery (M29); economy changes (M30).

## J. Manual validation for the owner

Whether battles feel less predictable without feeling arbitrary; whether enmity
is legible in play (the M25 target panel shows the stats; a threat read may be
added); whether taunt/fade feel impactful; difficulty balance.

## K. Required documentation updates on completion

This note (as-implemented + deviations); `docs/milestones.md`;
`docs/game_design.md` (enmity/targeting rules, control skills);
`docs/technical_design.md` (threat state in `Battle`, determinism contract);
content-authoring notes for the new skills; balance-report expectations.

## L. As-implemented record (2026-07-20)

- **Enmity model.** `Battle` gained `std::vector<long> threat` (global per unit;
  only party units accrue) and a `rngSeed`. Threat accrues inside the shared
  `attack`/`useSkill` from **damage dealt to enemies and healing done** (see
  deviation 1). `Battle::beginRound()` decays threat ×3/4 and is called at each
  round start by **both** `Simulator::simulate` and `BattleState`
  (`beginTurns`/`advanceTurn`), keeping decay identical.
- **Profiles.** `chooseEnemyAction` now derives a `TargetProfile`
  (Aggressive/Opportunist/Tactician/Protector/Spread) from `EnemyDef.role`
  (bosses from `BossArchetype`) and picks the party member with the best
  `targetScore` — a weighted mix of threat, kill pressure (missing HP), and
  backline weight (magic) — plus a small seeded tie-break. The heal/support
  branches are unchanged.
- **Variance.** A pure hash (`SplitMix64` of `rngSeed`+round+actor+candidate) in
  `[0,24)`, so `chooseEnemyAction` stays `const`/pure — no evolving RNG state to
  keep in sync. It only breaks near-ties; clear signals dominate.
- **Control skills.** New `content::SkillEffect { None, Taunt, Fade, Intercept }`
  on `SkillDef` (JSON key `control`), handled in `useSkill`: **taunt** sets the
  caster's threat above the party max, **fade** quarters it, **redirect**
  (Intercept) sets `Combatant.intercepting` (cleared in `clearGuard`) so an
  enemy's single-target hit aimed at an ally is taken by the interceptor
  (`redirectTarget`). Added `taunt`/`fade`/`redirect` to `data/skills.json` and
  to Guardian/Mage/Knight `startingSkills`.
- **Scoreboard.** `ScoreEntry.battleRulesVersion` (optional, default 0 = pre-M28)
  written by `DungeonState` as `battle::kBattleRulesVersion = 1`, round-trips via
  `optIntMin` (no format bump). The scoreboard flags pre-M28 entries with a `~`
  and a legend line; ranking is untouched.
- **Validation (this session):** full suite **265/265** (`ctest --preset
  debug`), including 10 new `[enmity]` tests (threat targeting, per-profile
  divergence, taunt/fade/redirect, purity/reproducibility, decay) and a
  `battleRulesVersion` round-trip; Debug **and** Release build clean;
  `--capture` **23/23** overflow-clean; the `[economy]` difficulty-curve
  assertions (every depth clearable, difficulty never inverts) **still pass**
  under the new AI, so no re-tune was required.

### Deviations from the plan / note

1. **Threat accrues from damage and healing, not every skill cast.** Buffs and
   debuffs generate no threat (simpler and readable); damage/heal are the
   meaningful sources. Control skills set threat explicitly.
2. **Variance is a pure per-battle hash tie-break**, not a mutable RNG object —
   equivalent determinism, but keeps `chooseEnemyAction` a pure query and
   removes any RNG-sync fragility between sim and live.
3. **The Simulator's party AI does not use the control skills** (it only heals
   and attacks). The balance battery therefore measures a party *without*
   taunt/fade/redirect against the smarter enemy AI — a conservative (harder)
   estimate that still clears every depth; real players who use control skills
   will find it easier. Party-AI use of control skills was out of scope.
4. **Five profiles** (added `Spread` for Attrition and folded Disruptor into
   Tactician) rather than the plan's four example names; derived from the
   existing role taxonomy per the owner decision.
