# M20 — Encounter and Dungeon-Content Variety

## A. Status and authority

- **Status:** planned
- **Last reviewed repository commit:**
  `a316f244e870718aa27d9995dc871e11572ad429` (2026-07-19).
- **Relationship to `docs/milestones.md`:** single authoritative detailed scope
  for the M20 ledger entry; the ledger holds status. On conflict, follow the
  authority order in `CLAUDE.md`.
- **Planning fidelity:** bounded specification. Role gaps and event candidates
  must be identified from the actual post-M19 game; slices below are
  provisional and this note must be refreshed against the then-current
  repository before implementation begins.

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
