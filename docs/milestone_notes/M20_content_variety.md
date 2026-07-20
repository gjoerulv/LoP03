# M20 — Encounter and Dungeon-Content Variety

## A. Status and authority

- **Status:** complete (approved) — approved by the owner on 2026-07-20
  after manual testing (see §N).
- **Last reviewed repository commit:** `cce5a41` (M19, 2026-07-19). Re-audit:
  Brute bosses already enrage in the sim (damage ×1.5 below half HP,
  unannounced); Sorcerer/Commander differ only by data; saves never persist
  dungeons, so generation-shape changes are save-safe; `EnemyDef` has
  tags/tier/skills but no role field; events have no model anywhere.
- **Owner decisions (2026-07-19):**
  1. **Approved in full:** `role` field on enemies; versioned
     `data/composition.json` (team-size/elite depth curves, support caps,
     damage minimums, boss-minion bounds) **plus depth stat scaling**
     (+6%/depth past depth 5, capped +90%, flowing into danger);
     `kGenerationVersion` → 3.
  2. **All six room events ship:** shrine trade-off, healing spring,
     trapped treasure, merchant exchange, optional elite challenge, and a
     score-wager modifier (comparability-reviewed, visible in the score
     breakdown).
  3. **One sim mechanic per boss archetype** (rule changes approved):
     Brute enrage announced at trigger; Sorcerer empowers as minions fall;
     Commander rallies fallen minions once below half HP. Determinism
     preserved and tested.
- **Relationship to `docs/milestones.md`:** single authoritative detailed scope
  for the M20 ledger entry; the ledger holds status. On conflict, follow the
  authority order in `CLAUDE.md`.
- **Planning fidelity:** bounded specification. Role gaps and event candidates
  must be identified from the actual post-M19 game; slices below are
  provisional and this note must be refreshed against the then-current
  repository before implementation begins.
- **Input from the M19 audit (2026-07-19):** the depth difficulty curve
  **plateaus past depth ~6** — `makeTeam` size caps at `min(5, 2+depth)`
  (saturated at depth 3) and elite chance at `min(50, depth*12)` (saturated
  by depth ~4), so depth 8–10 dungeons clear at the same party level as
  depth 4–5 (evidence table in the M19 note §N). Fixing this changes what
  published seeds generate, so it belongs to this milestone's composition
  work and needs the generation-version policy applied (bump
  `kGenerationVersion`, owner sign-off).

## B. Problem statement

Content is complete by count (20 enemies, 4 bosses, 3 themes) but teams are
composed by depth/theme pools without role constraints, so generated
encounters can be tactically samey or incoherent (e.g. all-support teams);
rooms have no events or decisions beyond fight/loot; and bosses vary mostly by
stats, one enrage, and fixed minions (dynamic summons/true waves were
deferred at M7). The roadmap directs deepening replay through *meaningful
tactical combinations*, not raw content count.

## C. Goals

- An encounter-role taxonomy (bruiser, glass cannon, healer, buffer/debuffer,
  protector, poison/attrition, speed disruption, anti-magic/anti-physical,
  elite specialist) assigned to existing and new enemies in data.
- Externalized composition constraints (depth/theme pools, incompatible
  roles, max support count, elite frequency, team size, boss-minion rules)
  that generation cannot violate — validated, versioned content data.
- A deliberately small room-event set on the M16 room system (candidates:
  shrine trade-off, limited healing source, trapped treasure,
  merchant/exchange, optional elite challenge, score-risk modifier) — every
  event a real decision with risk/reward visible **before** confirmation.
- Boss identity improvements: unique mechanic, telegraph, escalation,
  counterplay, silhouette, reward identity — not inflated HP.
- New content only where role-coverage gaps are identified.

## D. Non-goals

- No quantity-first content production.
- No new game modes, story content, or meta-progression.
- No topology changes; events occupy M16 room markers.
- No scoring changes beyond what the M19 policy already defines (event
  score-risk modifiers must fit it).

## E. Dependencies

- M16 rooms (events need real room composition).
- M17/M18 presentation (events and bosses need readable presentation).
- M19 scoring policy (score-affecting events must be comparability-safe).
- **Owner approval required for:** the event set (player-experience
  decision), content-schema additions (roles, constraints, events), and any
  boss mechanic that changes battle rules.

## F. Proposed slices (provisional — refine before starting)

1. **Role taxonomy + constraints** — schema addition, data annotation of the
   existing roster, generator constraint enforcement, validation tests.
2. **Gap-driven content** — add enemies/elites only for identified role gaps.
3. **Room events** — owner-approved subset, implemented one at a time with
   visible-trade-off UI.
4. **Boss identity pass** — per-boss mechanic/telegraph/escalation review and
   improvements; deferred M7 items (dynamic summons, true waves) only if the
   owner elevates them.

## G. Expected systems affected

- `data/` — enemies/bosses annotations; new composition/event data (versioned
  schema additions, owner-gated).
- `src/content/` — loaders/validators for the additions.
- `src/dungeon/DungeonGenerator` — constraint-aware team composition; event
  placement via room markers.
- `src/battle/` — only if an approved boss mechanic requires it (rule change
  ⇒ owner-gated; determinism preserved).
- `src/states/` — event interaction UI.
- `tests/` — constraint validation, generation, event logic.

## H. Data, schema, and save compatibility

- **Content-schema additions expected** (roles, constraints, events):
  versioned, validated, owner-approved; old content files must remain loadable
  or be migrated in the same change.
- Deterministic seeds: generation changes here alter what seeds produce —
  coordinate with the M16 generation-version policy.
- Saves/settings: expected no impact; scoring only through the M19 policy.

## I. Automated validation

- Composition-constraint tests: generated teams across large seed samples
  never violate authored rules.
- Event logic tests (pure decision/outcome paths).
- Content validation for all schema additions; reference checks.
- Determinism tests under the generation-version policy; full suite green.

## J. Manual validation for the owner

- Representative runs across themes/depths: do encounters pose distinct
  tactical questions?
- Trigger every event: is the trade-off clear before confirming? Are
  consequences honest?
- Fight each boss: blind-distinguishable mechanics, readable telegraphs,
  real counterplay.

## K. Acceptance criteria

- Generated teams cannot violate authored composition constraints (tested at
  scale).
- Events create real decisions with risk/reward visible before confirmation.
- Representative bosses are mechanically distinguishable in blind play notes.
- New content maps to identified role gaps, not quantity targets.
- Content data remains valid, versioned, and reference-checked.

## L. Risks and failure modes

- **Variety theater** — events that are decorative text without decisions;
  the visible-trade-off rule is acceptance-relevant.
- **Constraint over-fitting** — rules so tight that generation becomes samey
  in the other direction; sample-based review.
- **Boss rule creep** — mechanics quietly changing combat rules; owner gate
  plus determinism tests.
- **Seed churn** — every generator change shifts published seeds; the
  generation-version policy must absorb it.
- **Second-order:** events interacting with score integrity (M19) and pacing
  (M22); score-risk events need explicit comparability review.

## M. Required documentation updates on completion

- `docs/milestones.md`; this note (as-implemented + deviations).
- `docs/game_design.md` — roles, events, boss identities.
- `docs/technical_design.md` — schema additions and generator constraints.
- Content-authoring documentation for the new schemas.
- Completion report per `docs/milestone_completion_template.md`.

## N. As-implemented record (2026-07-20)

**Roles & composition (owner decision 1, in full).** `EnemyRole` (7 roles)
required on every enemy; the 20-enemy roster annotated; `data/composition.json`
v1 externalizes size/elite curves, support cap (1), damage minimum (1), boss
minion bounds (1–3), and depth stat scaling (+6%/depth past 5, cap +90%)
carried by `EnemyTeam.statScalePct` into both `buildBattle` and the danger
formula. Constraint-aware `makeTeam` filters sorted candidate pools per
slot; an unsatisfiable elite pool falls back to normals rather than
violating a rule (a hole the scale test caught: Crystal Mine's elites were
all protectors). `kGenerationVersion` = 3.

**Depth-plateau fix verified** (economy battery, worst of 3 seeds):
clearing level by depth is now 1/1/1/1/3/5/9/11 at depths 1–6/8/10 (was
flat at 3 past depth 6). Depths 1–4 became slightly easier (all clear at
L1) — constraint-composed teams replace lucky all-elite rolls; flagged for
M23 play-tuning (`elitePctPerDepth` is the lever).

**Gap-driven content (identified, not quantity):** the roster had **zero
healers or buffers**. Added `bog_shaman` (normal healer, mine+forest),
`war_drummer` (normal buffer, keep), `grave_chanter` (elite healer,
keep+forest), plus `troll_berserker` into the mine's elite pool (its elites
had no damage role). Enemy AI now casts Support skills whose status the
target lacks — shadow_stalker's `weaken` was dead content before this.

**Boss mechanics (owner decision 3):** Brute enrage announcement (once);
Sorcerer +25% magic per fallen ally ("empowered" in the log); Commander
rallies fallen minions at half HP, once, below half HP (implemented in
`tickStatuses`, which play and simulator share — no play/sim divergence);
Rush doubles its first action. Telegraphs rewritten to state mechanic and
counterplay. All headless-tested on hand-built battles.

**All six events (owner decision 2):** 2–3 dead-end `RoomType::Event`
rooms per dungeon (kinds unique per dungeon), realized as the M16-deferred
`EventChamber` archetype with a solid anchor; every trade-off shown in the
footer before Confirm: shrine (40+20·depth gold → heal half of missing HP),
healing spring (40% HP, single use), merchant (one consumable at 130%
value), elite challenge (1–2 elites, double danger credit through the
normal battle flow), omen wager (`ScoreBreakdown.wager`: +150 completed
no-death / −100 completed with deaths; result panel grows a line), and
trapped chests (35% of unguarded chests; +25·depth+15 gold; claiming wounds
the party 25% max HP, never fatally; red-tinted with a "T" glyph fallback).
Five generated event props (shrine/spring/merchant/totem/omen) shipped
through the manifest with glyph fallbacks.

**Evidence:** 228/228 tests (12 new across composition/boss/events): 1000+
teams constraint-checked at scale, scaling reaches fight and label, curve
helpers, all four boss mechanics, support-AI gating, event well-formedness
(dead ends, unique kinds, valid payloads, traps only on unguarded chests),
wager scoring paths, theme role coverage. In-game captures show the
challenge totem + danger label live in Crystal Mine
(`docs/screenshots/m20_events/`). All pre-existing PNGs byte-identical
after the generator rerun.

**Deviations / notes:**
1. Blind scripted play did not trigger each event end-to-end; §J's owner
   checklist covers that explicitly (the matrix M20 note lists every
   check).
2. Dynamic mid-battle unit ADDITION was avoided: the Commander rally
   revives its fallen minions rather than summoning new units — same
   fantasy, no turn-order model change.
3. Merchant sells exactly one item per visit (single-confirm prompt), not
   an in-dungeon shop UI.
4. Battle presentation for revived minions reuses the M18 KO-fade restore
   path (`commitPresentation` resets the fade when a shown-dead unit
   returns).
