# M29 — Content Expansion & Class Learnsets

## A. Status and authority

- **Status:** ☑ complete (approved by the owner 2026-07-20 after manual
  testing; committed). Authored / audited 2026-07-20 against the then-current
  checkout (HEAD `872782c` "M28"). §D decisions approved (`I agree 100%`,
  2026-07-20); as-implemented record in §L.
- **Authority:** `CLAUDE.md` > this note > `docs/milestones.md` >
  `docs/game_design.md` > `docs/technical_design.md`.
- **Authorization:** the owner authorized beginning M29 on 2026-07-20 after
  approving M25–M28. Dependencies satisfied: **M26** (per-id enemy/boss art
  language) and **M28** (enmity behaviour profiles) are `complete (approved)`.

## B. Problem statement (verified at audit)

Current content is thin and progression-flat:

- **6 classes**, each with only `startingSkills` (3–4 ids). There is **no
  level-based skill learning** anywhere. A character's usable skills are derived
  *entirely* from its class at battle-build time —
  `buildBattle` sets `u.skillIds = cls->startingSkills`
  ([Battle.cpp:524](src/battle/Battle.cpp:524)). `Character` stores **no** skill
  list and the save format persists **none** (skills are derived like stats).
- **Scrolls are a dead mechanic.** `ItemDef.grantsSkill` exists and three scroll
  items ship (`scroll_fireball`, `scroll_greater_heal`, `scroll_whirlwind`), but
  **no runtime code consumes a scroll to teach a skill** — grep confirms
  `grantsSkill` is read only by the loader/validator, never applied. `whirlwind`
  is therefore a skill no class or enemy can currently obtain in play.
- **23 enemies** (16 normal + 7 elite), **4 bosses**, **31 skills**. Enemy roles
  in use: bruiser ×5, sniper ×5, protector ×4, attrition ×3, buffer ×2,
  healer ×2, disruptor ×2. Content fills the M20 taxonomy but is not deep.

Consequence: no sense of a character *growing* — every skill it will ever have,
it has at level 1; the battle menu is static from creation; and the M28 enmity
profiles have a small, repetitive enemy cast to express themselves through.

## C. Goals

- A **per-class learnset**: skills granted at set levels, so characters visibly
  grow and the battle menu expands with progression.
- **More enemies and bosses** with declared M28 behaviour profiles (roles) and
  their own M26-standard sprites, filling role/theme gaps.
- **New skills** so the learnsets have meaningful content to grant.
- **Zero save breakage**; deterministic battle behaviour preserved (sim ↔ live
  agreement); battle menus stay within M12 layout budgets as counts grow.

## D. Material design decisions (owner input required)

Each fork changes the implementation and/or player experience and carries a
recommendation. `I agree 100%` accepts all recommendations.

### D.1 — Learnset storage model → *Recommend: derive-from-level (no new save field)*

Skills are already **derived, not stored** (like `stats`/`maxHp`). Known skills
become `startingSkills` ∪ `{ e.skill : e.level ≤ character.level }` from the
class learnset, recomputed wherever a skill list is needed (battle build; any
"skills learned" UI). **No `Character` field, no save-format change, no
migration — automatically backward-compatible**, and a loaded old save
immediately reflects the learnset for its level. This matches the existing
"derived so it never goes stale" architecture exactly.

- *Alternative (persist a `learnedSkills` vector on `Character`):* required only
  if one-off grants (scrolls, relics) must stick independent of level/class.
  Adds a save field + migration and a source-of-truth split (some skills
  derived, some stored). Rejected unless the owner wants scroll-teaching wired
  now (see D.4).

**Consequence of the recommendation:** a class's learned set is a pure function
of its level; the "level-up hook in `game/Party`" from the ledger scope becomes
a *notification* concern (what to surface as "learned X"), not a *storage* one —
the data is always derivable.

### D.2 — Learnset schema shape → *Recommend: `learnset: [{skill, level}]` on ClassDef; keep `startingSkills`*

Add an optional `learnset` array to each class (`data/classes.json`) beside the
existing `startingSkills` (which stays the level-1 set). Loader parses it,
validator reference-checks every `skill` id and requires `level ≥ 1`, and a new
reference test covers it. Public-schema change — **already owner-approved in
principle** (ledger, 2026-07-20: "classes gain a level-based learnset";
expanding `startingSkills` was explicitly rejected). `kContentSchemaVersion`
stays 1 (additive optional field; old-shaped `classes.json` still loads).

- *Alternative:* fold level into `startingSkills` entries (`{id, level}`),
  breaking the current plain-string array shape and every test that reads it.
  Rejected — needless churn.

### D.3 — Content volume → *Recommend the concrete target below*

A focused expansion sized to fill gaps, not to chase quantity (the M13 north
star). Every number here is the **decision to confirm**:

- **New skills: ~12** (so each class reaches **~6–7 known skills by the level
  cap**, up a smooth curve). Mix of damage/heal/support/status across the six
  classes; `whirlwind` (already shipped, currently orphaned) folded into a
  martial learnset so no skill is scroll-only.
- **New normal enemies: +6** (to 22), **new elites: +2** (to 9), chosen to
  round out under-represented roles and give each of the 3 themes a fuller pool.
- **New bosses: +2** (to 6) — two additional archetype expressions (the four
  `BossArchetype`s stay as-is; no new archetype, which would be an M28-scope
  combat change). Each boss gets one deterministic telegraphed mechanic built
  from existing hooks.
- **Sprites:** every new enemy/boss gets a **bespoke, deterministic per-id
  sprite** via `tools/asset_gen/generate_textures.ps1` (same pipeline as M26) —
  **forced by the M26 lint**, which fails the suite on any content id lacking
  `enemy.<id>.battle` / `boss.<id>.battle`. Names ≤16 chars and telegraphs ≤2
  lines (the lint enforces both).

- *Alternative (smaller):* learnsets only, no new enemies/bosses — cheaper,
  faster, but leaves the M28 profiles with the same small cast and does not
  deliver the "more enemies, more bosses" ledger scope. *Alternative (larger):*
  more content — higher art cost and balance-tuning surface for little
  additional readability. The recommended middle keeps the milestone reviewable.

### D.4 — Scrolls / one-off skill grants → *Recommend: leave scrolls as-is, out of scope*

Under D.1 (derive-from-level, no persisted skills), a scroll cannot durably
teach a skill without adding the persisted vector D.1 rejects. Since scrolls are
**already non-functional** (grant unwired) and their three skills all live in a
class kit or learnset after D.3, the scroll items become harmless flavor/no-ops.
Recommend **documenting them as a known unwired mechanic** and **not** wiring
scroll-teaching in M29 (it would drag in the persisted-skills model and a save
migration — a separate, deliberate decision).

- *Alternative:* wire scroll-teaching now → pulls in D.1's persisted
  `learnedSkills` + save version bump. Larger, and orthogonal to the learnset
  goal. Defer to a future milestone if the owner wants consumable skill grants.

## E. Proposed design (assuming the §D recommendations)

- **Schema/loader.** `ClassDef` gains `std::vector<LearnEntry>` where
  `LearnEntry { std::string skill; int level; }`. `parseClasses` reads an
  optional `learnset` array (each `{ "skill": id, "level": n≥1 }`);
  `validateReferences` checks each `skill` id exists. Reference/loader tests
  extended; `test_content_loader` count assertions updated.
- **Derivation.** A pure helper `content::knownSkillsFor(const ClassDef&, int
  level)` returns `startingSkills` plus every learnset entry with `level ≤ N`,
  de-duplicated, order stable (startingSkills first, then learnset by ascending
  level then declaration order). `buildBattle` calls it with `c.level` instead
  of copying `startingSkills` directly — the **single** behavioural change to
  combat, and it is deterministic and identical in sim and live (the simulator
  builds battles through the same `buildBattle`).
- **Level-up surfacing (optional, low-risk).** `grantXp` already recomputes
  stats on level-up; it can additionally compute which learnset skills became
  available this level for a "Learned <skill>!" message. Because the data is
  derivable, this is purely presentational and carries no persistence risk. If
  it adds UI scope, it is deferred to owner-validated polish, not required for
  the mechanic.
- **New content.** New rows in `data/{skills,enemies,bosses}.json` with valid
  roles/archetypes; theme pools (`dungeon_themes.json`) extended to include the
  new enemies/bosses so they actually spawn. New sprite recipes appended to
  `generate_textures.ps1` (append-only, RNG-reseed discipline so existing
  sprites stay byte-identical) + manifest rows + `assets/credits.md` provenance.
- **Balance.** Learnsets make parties *stronger* over levels; the `[economy]`
  difficulty-curve battery and `[.][economy-report]`/`[.][sim-report]` are
  re-run and reviewed. The simulator's party AI will now have more skills to
  choose among (it uses `u.skillIds`), so this exercises the new content. Tuning,
  if needed, is data-only.

## F. Determinism & sim/live agreement (binding constraint)

The only combat-path change is `buildBattle` sourcing skills from
`knownSkillsFor(cls, level)` instead of `cls->startingSkills`. Both the live
`BattleState` and the headless `Simulator` build battles through `buildBattle`,
so they receive identical skill lists. `knownSkillsFor` is pure and
level-deterministic. No RNG, no new mutable state. Existing battle-determinism
and `[enmity]` tests are **extended, never weakened**; a new test asserts a
higher-level character exposes its learnset skills and a level-1 character does
not, and that sim and live see the same set.

## G. Data, schema, save compatibility

- `data/classes.json`: +optional `learnset` per class (schema v1 unchanged).
- `data/skills.json`, `data/enemies.json`, `data/bosses.json`,
  `data/dungeon_themes.json`: additive new rows.
- **No `Character`/save-format change; no version bump; old saves load and
  immediately gain the learnset for their level** (D.1). Deterministic
  *dungeon-generation* seed behaviour is untouched — new enemies enlarge theme
  pools, which changes *what a given seed spawns*, so **`dungeon::kGenerationVersion`
  must bump** (owner-gated per the skill's gotcha 12; scoreboard tags it so
  published pre-M29 seeds stay comparable). This bump is the one generation-side
  escalation for M29 — flagged in §D-adjacent decisions and confirmed by
  `I agree 100%`.

## H. Balance

The learnset raises party power along the level curve; new enemies/bosses add
targets. Acceptance: every depth stays clearable and difficulty never inverts
(the existing `[economy]` assertions); no new skill is a strict dominator; new
enemies respect composition rules; battle menus stay within the scrolled
`kListRows` budget (already handles >4). Re-tune data only if an assertion fails.

## I. Out of scope

The enmity model itself (M28); economy/inn changes (M30); new `BossArchetype`s
or combat rules; wiring scroll-teaching / persisted one-off grants (D.4); new
game modes or story.

## J. Manual validation for the owner

Whether skill pacing across levels feels satisfying (not too front/back-loaded);
whether new enemies/bosses feel varied and read at 426×240; whether the new
sprites hold the M26 art language; difficulty balance across depths/themes.

## K. Required documentation updates on completion

This note (as-implemented + deviations); `docs/milestones.md` (status);
`docs/game_design.md` (§4 party/skills — learnset; §13 content target counts);
`docs/technical_design.md` (ClassDef `learnset`, `knownSkillsFor`, generation
version bump); `assets/credits.md` (new sprite provenance); content-authoring
notes for the new skills/enemies/bosses; balance-report expectations.

## L. As-implemented record (2026-07-20)

- **Status:** complete (approved 2026-07-20). Owner approved all §D
  recommendations (`I agree 100%`, 2026-07-20).
- **Schema + derivation.** `ClassDef` gained `std::vector<LearnEntry> learnset`
  (`LearnEntry { std::string skill; int level; }`). `parseClasses` reads the
  optional `learnset` array (`{ "skill", "level"≥1 }` via `reqIntMin`);
  `validateReferences` reference-checks each `skill`. `knownSkillsFor(cls,
  level)` (in `ContentDatabase`) returns `startingSkills` + level-satisfied
  learnset entries, stably ordered (starting first, then ascending level,
  declaration order on ties) and de-duplicated. `buildBattle`
  (`Battle.cpp`) now sets `u.skillIds = content::knownSkillsFor(*cls, c.level)`
  — the **only** combat-path change; live play and the simulator both route
  through it, so they agree exactly. No `Character` field, no save bump.
- **New skills (12).** `shield_bash, execute, aimed_shot, barrage, hamstring,
  rend, eviscerate` (physical), `arcane_burst, smite, radiance` (magic),
  `intimidate, iron_will` (support). Six previously-orphaned skills
  (`whirlwind` — formerly scroll-only — plus `stone_edge, inferno, holy_ray,
  renew, focus`) were folded into learnsets so they are reachable in play.
  Skill count 31 → **43**.
- **Learnsets.** Each class grows to 6–7 known skills by the cap along an
  early-to-mid curve (grants at levels 4–18): knight 7, ranger 7, mage 7,
  cleric 7, rogue 6, guardian 7.
- **New content.** +6 normal enemies (`corpse_hound, sand_lurker, gloom_priest,
  rune_sentry, standard_bearer, mire_imp`) and +2 elites (`bone_colossus,
  void_weaver`) → **31 enemies**; +2 bosses (`deep_king` brute, `blight_matron`
  sorcerer) → **6 bosses**. Casters were given enough `magic` to actually pay
  their listed skills, and debuffers use single-enemy statuses (which the AI
  re-checks against the real target) rather than spammy all-enemy ones. Theme
  pools (`dungeon_themes.json`) were extended so all new content spawns;
  `dungeon::kGenerationVersion` **4 → 5** (owner-approved).
- **Sprites.** 8 enemy (24×24) + 2 boss (32×32) bespoke recipes appended to
  `generate_textures.ps1` (M26 helpers/conventions, RNG reseeded) + manifest
  rows + `assets/credits.md`. Regeneration verified: the 10 new PNGs appear and
  every pre-existing texture stays byte-identical.
- **Validation (this session):** full suite **274/274** (`crystal_tests.exe`,
  30361 assertions), incl. 9 new `[learnset]` tests (derivation order/dedup/
  gating/clamp, loader parse + reference + level≥1 validation, and shipped-data
  growth: knight lacks `whirlwind` at lv1, gains it at lv10; every class gains
  breadth by the cap). Presentation lint passes (every new enemy/boss resolves
  its own sprite; names ≤16, telegraphs ≤2 lines). `--capture` **23/23**
  overflow-clean. Debug **and** Release build clean. The `[economy]`
  difficulty-curve assertions still pass; the report shows every depth clearable
  at levels 1–9 (deep clears a touch earlier than pre-M29 because the sim's
  party AI now wields the learnset skills — a conservative-harder estimate), so
  **no re-tune was required**.

### Deviations from the plan / note

1. **The optional "Learned <skill>!" level-up message was not added.** Because
   skills are derived, a mid-run level-up already unlocks the new options in the
   next battle's menu with no code; the toast is pure presentation and was
   deferred as low-risk polish (owner may request it at validation). No
   persistence or mechanic depends on it.
2. **Four orphaned skills remain class-unreachable** (`blizzard, shadow_bolt,
   venom_mist, sunder`) but are used by enemies/bosses, so none is dead content;
   the M29 goal (no *scroll-only* skill, each class 6–7) is met.
3. **Scrolls stay unwired** per §D.4 — documented, out of scope; their three
   skills are all now in class kits/learnsets.
