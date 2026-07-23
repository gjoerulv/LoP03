# M48 — Elements

## A. Status and authority

- **Status:** ◑ implemented, awaiting manual approval (2026-07-23) — authored
  just-in-time on 2026-07-23 from the
  owner-approved M47–M51 "Court & Comfort" plan. Second milestone of that
  program; M49 → M50 → M51 follow, **then** M23 → M24. See §J for the
  as-implemented record.
- **Authority:** `CLAUDE.md` → this note → `docs/milestones.md` →
  `docs/game_design.md` → `docs/technical_design.md`.
- **Base checkout:** `91b2335` ("M47"), working tree clean at authorization.
- **Baseline:** 442/442 tests, `--capture` 52/52 scenes overflow-clean.
- **Versions at baseline:** `battle::kBattleRulesVersion` **7**,
  `dungeon::kGenerationVersion` **10**, `kSaveVersion` **1**,
  `kSettingsVersion` **1**.
- **Terminal state for this session:** `implemented, awaiting manual approval`.

## B. Re-audit against `91b2335`

| Plan claim | Verified? |
|---|---|
| `Element` enum already exists (None, Fire, Ice, Lightning, Earth, Holy, Dark) | ✅ [Enums.hpp:11](../../src/content/Enums.hpp:11); `parseElement`/`toString` both present ([Enums.cpp:31](../../src/content/Enums.cpp:31)) |
| `SkillDef.element` is already parsed but **never used in damage** | ✅ [Definitions.hpp:19](../../src/content/Definitions.hpp:19), parsed at [ContentLoader.cpp:75](../../src/content/ContentLoader.cpp:75); no other reference anywhere in `src/` |
| `EnemyDef`, `BossDef`, `ItemDef` have **no** element/weakness fields | ✅ confirmed by reading all three structs |
| Damage lives in `physicalDamage`/`magicDamage`; guarding halves | ✅ [Battle.cpp:47–61](../../src/battle/Battle.cpp:47) |
| **"the single damage-resolution choke point"** | ❌ **There is no single one.** Base damage is computed by the two pure helpers at **five** call sites (`attackOne`, the Counter Attack inside `dealPhysical`, and the skill Physical / Magic / Support-with-power branches), then applied through `dealPhysical`/`dealMagic`. §E2 resolves this by putting the modifier **inside the two pure helpers**, which every one of those five sites already goes through. |
| `std::max(1, …)` damage floor | ⚠️ Not in the plan, and decisive: the floor is *inside* `physicalDamage`/`magicDamage`, so the element multiplier must be applied **after** it or an immune hit would deal 1, not 0. |
| Enemies have no weapons, so their basic attacks stay `None` | ✅ nothing gives an enemy `Combatant` a weapon; only `Character.weapon` exists |
| `buildBattle` resolves content flags onto `Combatant` once | ✅ [Battle.cpp:1003](../../src/battle/Battle.cpp:1003) — the M45 precedent (`attackHitsAll`, `attackStatuses`, `uncontrolled`); the pure model never touches the content DB |
| `lastMissed` is the per-action presentation channel for floats | ✅ [Battle.cpp:637](../../src/battle/Battle.cpp:637) → [BattleState.cpp:298](../../src/states/BattleState.cpp:298). Weak/Immune floats copy this pattern exactly. |
| The battle target-info panel can host affinity lines | ⚠️ Partly. The panel is **60 px** with four occupied rows (+5 heading, +22 vitals, +34 stats, +47 status/passive) — there is **no free row**. §E4 uses `drawChipRight` on the vitals row instead, which is both free space and the M46 idiom (shape + text, never color alone). |
| The bestiary detail panel can take affinity lines | ✅ [BestiaryState.cpp:120](../../src/states/BestiaryState.cpp:120) is a flowing `y` layout; an affinity block slots in after passives, before flavor |
| Loader/validator patterns for optional fields and string arrays | ✅ `optEnum`, `optStringArray`, and the "parse each string, report unknown" loop used by `equipBans` / `tags` |
| Sim uses the same damage path | ✅ `applyChoice` → `b.useSkill` / `b.attack`; no damage arithmetic exists outside `Battle.cpp` |
| The M19/M43 comparability mechanism handles a rules bump with no new work | ✅ `ScoreEntry.battleRulesVersion` + `ScoreboardState`'s flag |

Content inventory taken at note time (the curation in §E3 is sized against it):
**43 enemies** (22 normal, 21 elite; `minTown` 1–7), **13 bosses**, **25 weapons**
(`minTown` 1–7 plus 5 legendaries), **57 skills** of which 32 damage.

## C. Goals

An element weakness/immunity layer at one battle-rules bump (**7 → 8**):
optional schema, one pure modifier in shared `battle::` code applied to skills
**and** weapon-elemental basic attacks, sparse curated content tags, and reveal
UI so a player can see an affinity before spending a turn on it.

Owner decisions already taken (2026-07-23): **weakness = ×150 %, immunity = 0**;
affinities are **shown for encountered foes** (bestiary + battle target panel);
floats read **"Weak!" / "Immune"**.

No generation, save, or settings version bump. Out of scope: the Royal Guards
and the revive clock (M49), town travel (M50), settings/CRT/tint (M51).

## D. Constraints

- The modifier is **pure** and lives in `battle::`, called from inside the two
  damage helpers that both drivers already share — sim == live by construction.
- **No new rolls.** `elementModifier` is a lookup, not a draw; `rollCursor` is
  untouched, so a seed reproduces the same battle stream.
- All new UI goes through the M46 kit and `palette()` roles.
- Every schema addition is optional with a defensive loader; absent fields mean
  "no affinity", which is every pre-M48 content file.

## E. Slices

### E1 — Schema (all optional, validated)

| Def | Field | Meaning |
|---|---|---|
| `EnemyDef` | `weaknesses[]` | element names; each takes ×150 % damage |
| `EnemyDef` | `immunities[]` | element names; each deals 0 |
| `BossDef` | `weaknesses[]` / `immunities[]` | same |
| `ItemDef` | `element` | the weapon's element (basic attacks carry it) |

Validation (errors, not warnings):

1. an unknown element name is an error (the `tags`/`equipBans` pattern);
2. `weaknesses ∩ immunities` must be **empty** — a foe cannot be both;
3. `element` on a **non-weapon** item is a content error (`slot != weapon`);
4. `none` in either array is an error (it means nothing and hides a typo).

### E2 — The pipeline (shared `battle::` code)

```
int elementModifier(const Combatant& defender, content::Element e);  // 0 | 100 | 150
```

Pure, declared in `Battle.hpp` so it is directly unit-testable. `Element::None`
always returns 100. Applied **as the last step inside `physicalDamage` and
`magicDamage`**, i.e. *after* their `std::max(1, …)` floor — so an immune hit is
genuinely 0 rather than the floor's 1 — and after the guard halving, so guarding
and affinity compose multiplicatively. Because both helpers take the element as a
parameter, all five existing call sites are covered by construction:

| Call site | Element used |
|---|---|
| `attackOne` (basic attack) | the attacker's `weaponElement` |
| Counter Attack (inside `dealPhysical`) | the **counter-attacker's** `weaponElement` — a counter is a basic attack, so it carries the same steel |
| skill `Physical` branch | `skill.element` |
| skill `Magic` branch | `skill.element` |
| M40 Support-with-power branch | `skill.element` |

`Combatant.weaponElement` is resolved once in `buildBattle` from the character's
equipped weapon (`db.findItem(c.weapon)->element`) — the M45 precedent, so the
pure model still never reads the content database. Enemies and bosses have no
weapon, so their basic attacks stay `None` and every pre-M48 fight resolves
byte-identically.

**Immunity blocks riders, not just damage.** An immune hit applies **no**
attack-statuses (`applyAttackStatuses`) and **no** skill status; the check is the
same pure `elementModifier` call at those two sites. An immune hit is *not* a
miss: it still connects, still draws enmity through the existing zero-damage
path, and still reads differently in the log ("… is immune!" vs "… misses!").

**Presentation channel.** `Battle` gains `lastWeak` / `lastImmune` (unit index
lists, cleared at the start of every action exactly like `lastMissed`).
Presentation-only — nothing in the model reads them.

### E3 — Content curation

Sparse and deliberate. **Skills (9)** — only where the name says the element:

| Element | Skills |
|---|---|
| Fire | `fireball`, `inferno` |
| Ice | `frost_lance`, `blizzard` |
| Lightning | `spark` |
| Earth | `stone_edge` |
| Holy | `holy_ray`, `smite`, `radiance` |

**Dark stays reserved** (per the plan): `shadow_bolt` is left untagged so the
element exists for later content without shipping an untested seventh affinity.
The King's `sovereigns_cataclysm` / `sovereign_smite` are left untagged — the
King's fight belongs to M49.

**Weapons (5)**, spanning the whole ladder so the system is met early and stays
relevant: `holy_mace` (Holy, town 1), `dragonfang` (Fire, town 6),
`sunforged_greatblade` (Fire, town 7), `celestial_staff` (Holy, town 7),
`dawnforged_blade` (Holy, legendary).

**Foes (8 enemies + 3 bosses)** — at most one weakness and one immunity each:

| Foe | Weak | Immune |
|---|---|---|
| `zombie` (t1) | Holy | — |
| `venom_spider` (t1) | Fire | — |
| `stone_golem` (t1) | Lightning | Earth |
| `frost_imp` (t1) | Fire | Ice |
| `grave_wight` (t3) | Holy | Ice |
| `crystal_guardian` (elite) | Earth | Lightning |
| `bone_colossus` (elite) | Holy | Ice |
| `titan_guard` (t7 elite) | Lightning | Earth |
| `frost_monarch` (boss, t3) | Fire | Ice |
| `obsidian_colossus` (boss, t4) | Ice | Earth |
| `blight_matron` (boss) | Holy | — |

**The no-dead-weapon rule (a design decision this note makes explicit).**
Immunities use **only Ice / Earth / Lightning**; weapon elements use **only Fire
/ Holy**. The two sets are disjoint *by construction*, so **no basic attack can
ever be reduced to 0**. This matters because a class can be skill-less: the
Dragon (M45) has no skills at all, so a Fire-immune foe met by a Dragon holding a
Fire weapon would be an unwinnable fight, not an interesting one. A skill can
always be swapped for another; a basic attack cannot. A **test pins the rule**
(no shipped weapon's element appears in any shipped foe's `immunities`), so
future content cannot quietly create that trap.

Party members have no affinities — the layer is one-directional (foes only), so
enemy elemental skills resolve exactly as they do today.

### E4 — Reveal UI (M46 kit)

- **Battle target panel:** two `drawChipRight` chips on the **vitals row**
  (which is short and otherwise empty on the right) — `Weak Fire` in
  `palette().gold`, `Immune Ice` in `palette().danger`. Chips are outline +
  fill + accent bar + label, so the information is shape **and** text, never
  color alone. Shown only for the targeted unit; absent when the foe has no
  affinity, so nothing moves for untagged fights.
- **Bestiary detail:** an `Affinity` block after the passives block —
  `Weak: Fire` / `Immune: Ice` — for **known** foes only (an unmet foe reveals
  nothing, preserving the M42 masking rule).
- **Battle floats:** `Weak!` (gold) and `Immune` (coral) from the new
  `lastWeak`/`lastImmune` lists, beside the damage number. `FloatNumber` gains a
  `kind` enum (Damage / Heal / Miss / Weak / Immune) replacing the bare `heal`
  bool; existing colors are preserved exactly.

### E5 — Balance evidence

The tagged fights are re-run through the existing batteries
(`test_balance.cpp`, `[economy-report]`, `[castle-report]`, `[king-report]`) with
the results tabulated in §J. The bar: no tagged fight becomes degenerate in
either direction — a weakness must not trivialize a boss, and an immunity must
not make one unclearable.

## F. Tests

New `tests/test_elements.cpp`:

- `elementModifier` math: None → 100; untagged → 100; weak → 150; immune → 0;
  an element that is neither → 100.
- Immunity zeroes damage **through a real `Battle`** (skill and basic attack),
  and the `max(1, …)` floor does not resurrect it.
- Weakness multiplies a real hit by 150 % after guard.
- **Immune blocks riders:** no attack-status and no skill status lands on an
  immune target.
- Weapon-element basic attacks: a tagged weapon carries its element; an untagged
  one is `None`; enemies always `None`.
- Counter Attack carries the counter-attacker's weapon element.
- Sim == live on a tagged fight (`applyChoice` vs direct `useSkill`, same HP and
  `rollCursor`).
- Loader/validators: unknown element name, `weaknesses ∩ immunities ≠ ∅`,
  `element` on a non-weapon, `none` in an array — each a reported error.
- **Shipped-content lint:** every tagged foe has disjoint sets, and **no weapon
  element appears in any foe's immunities** (the §E3 rule).
- `kBattleRulesVersion == 8`.

## G. Capture

Three scenes (52 → **55**): a weak-hit float, an immune float, and a bestiary
entry showing the affinity block. Suite stays overflow-clean.

## H. Documents to update

`docs/game_design.md` (an elements section), `docs/technical_design.md`
(rules-v8: the modifier, its placement after the damage floor, the no-dead-weapon
rule), `docs/content_authoring.md` if it documents the enemy/item schemas,
`docs/manual_test_matrix.md` (new rows), `docs/milestones.md`, `README.md`,
this note's §J.

## I. Acceptance criteria

1. `elementModifier` is pure, lives in `battle::`, and is the only place the
   ×0/×1/×1.5 decision is made; sim == live proven on a tagged fight.
2. An immune hit deals exactly 0, applies no rider, and reads differently from a
   miss; a weak hit deals 150 % after guard.
3. Loader rejects unknown elements, overlapping sets, and `element` on a
   non-weapon; absent fields load as "no affinity".
4. No shipped weapon element collides with any shipped immunity (pinned by test).
5. Affinities are visible for encountered foes in the bestiary and for the
   targeted unit in battle; floats say Weak!/Immune with shape+text.
6. Balance battery shows no degenerate tagged fight (evidence table in §J).
7. `kBattleRulesVersion` 8; generation/save/settings versions unchanged.
8. Full suite green in Debug **and** Release; capture 55/55 clean.

## J. As-implemented record

All slices landed. Verified 2026-07-23 on `91b2335`: **462/462 tests** (442
baseline + 20 new) green in **Debug and Release**, `--capture` **56/56** scenes
overflow-clean, no warnings introduced.

### Deviations from this note

1. **No skill tagging was needed — the shipped skills were already tagged.**
   `data/skills.json` already carried `element` on eleven skills (the nine in
   §E3 **plus** `shadow_bolt` and `bewilder`, both `dark`). The field was parsed
   and ignored; M48 simply gave it effect. Consequence: **`data/skills.json` was
   not touched at all**, and the two Dark skills now have a real element that no
   shipped foe reacts to — inert, exactly as "Dark stays reserved" intends.
2. **A fourth capture scene** (`56_battle_target_affinity`) was added beyond the
   planned three. The first three cover the two floats and the bestiary block;
   none of them shows the **target panel** chips, which are the reveal a player
   actually uses while deciding. 52 → **56 scenes**.
3. **Element display names got a shared table.** `content::elementDisplayName`
   ("Fire", not the JSON id "fire") so the bestiary and the battle panel cannot
   drift. Small addition to `Enums.hpp/.cpp` beside `toString`.
4. **The "Immune" float sits a row above the unit**, like "Weak!", not on the
   unit like "Miss!". The first capture put it straight over the sprite it had
   failed to hurt and the word was unreadable. Miss keeps its original position.
5. **`test_rules_v7_flow.cpp`'s version pin was relaxed** from `== 7` to `>= 7`,
   with the exact pin now living in the newest rules milestone's suite
   (`test_elements.cpp`, `== 8`). Otherwise every rules bump edits every past
   milestone's tests.
6. **`Battle::clearActionMarks()`** replaced the six scattered
   `lastMissed.clear()` calls, so a future presentation mark cannot be added to
   one action path and forgotten in another.

### Balance evidence (§E5)

| Battery | Result | Baseline |
|---|---|---|
| `[balance]` (15 cases) | all green | unchanged |
| `[castle]` (16 cases) | all green | unchanged |
| `[castle-report]` Boss Rush | **cleared 12/12, 36 rounds** | M43 recorded 36 rounds — **identical** |
| `[castle-report]` King @340 % | relic-less maxed party still loses (17 rounds) | unchanged; the King is untagged, and M49 owns his fight |
| `[castle-report]` Endless | best wave 11 | unchanged |
| `[economy]`, `[king-report]`, `[classes-report]` | green | unchanged |

The Boss Rush includes both tagged bosses and its round count did not move: the
sim's party AI picks the *highest-power* affordable skill, which for a maxed
mage is the untagged `arcane_burst`, so the tags change how a **player** fights
those bosses far more than how the sim does. No fight became degenerate in
either direction. The end-to-end test "shipped tags reach the battle model
through buildBattle" is the evidence that the curation is genuinely wired:
Blizzard on the Frost Monarch deals exactly 0, Fireball exactly 150 % of its
untagged self.

### Files changed

- **Source:** `src/content/Definitions.hpp` (`ElementAffinity`, `ItemDef::element`),
  `src/content/Enums.hpp/.cpp` (`elementDisplayName`),
  `src/content/ContentLoader.cpp` (`readAffinity` + the weapon-only rule),
  `src/battle/Battle.hpp/.cpp` (`elementModifier`, `isImmuneToElement`,
  `weaponElement`/`weaknesses`/`immunities` on `Combatant`, the five call sites,
  `lastWeak`/`lastImmune`, `clearActionMarks`, `buildBattle` resolution, rules 8),
  `src/states/BattleState.hpp/.cpp` (`FloatKind`, the element floats, the target
  chips, `captureElementHit`), `src/states/BestiaryState.hpp/.cpp` (affinity
  block), `src/capture/CaptureRunner.cpp` (4 scenes).
- **Tests:** **new** `tests/test_elements.cpp` (20 cases),
  `tests/test_rules_v7_flow.cpp` (version pin), `tests/CMakeLists.txt`.
- **Content:** `data/items.json` (5 weapons), `data/enemies.json` (8 foes),
  `data/bosses.json` (3 bosses). **`data/skills.json` untouched** (see §J.1).
- **Docs:** this note, `docs/milestones.md`, `docs/game_design.md`,
  `docs/technical_design.md`, `docs/manual_test_matrix.md` (rows 118–122,
  row 117 amended), `README.md`.

### Known limitations

- **The enemy AI is element-blind.** Foes never prefer a spell the party is weak
  to — the party has no affinities at all, so there is nothing for them to
  exploit. If affinities ever become two-directional, the AI must be revisited.
- **The sim's party AI is element-blind too** (highest power wins), so simulated
  balance under-represents how much a *player* gains from aiming at a weakness.
  Stated rather than papered over; it is why the evidence table shows "no
  change" rather than "elements measurably help".
- Dark is authored on two skills and reacted to by nothing. Deliberate.
