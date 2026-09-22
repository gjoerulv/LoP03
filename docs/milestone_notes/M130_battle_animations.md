# M130 — Short attack & skill animations

**Status:** implemented, awaiting manual approval
**Authorization:** the M128–M130 "Peak 80s pixel art on Atari" program,
authorized 2026-09-22 by plan approval (see the M128 note for the program's
framing). Branch `oyb13`, baseline `5f6ff56` (v0.9.1).
**Version motion:** none. Battle rules 19, generation 25, save v1, settings
v1, content v1, manifest v2, `project(VERSION)` 0.9.1. No public schema
moved: the animation family and tier are DERIVED from the authored skill
fields (the M121 `SkillKind` way); `data/skills.json` names no animation.

## Scope (the owner's words, 2026-09-22)

> Make short animation for attacks and skills. The summons have long
> animations, we don't want that. They should be short, but the more
> powerful ones, like inferno, can be a bit longer. The fights should not be
> bogged down by animations though.

Rulings from the interview and the follow-up: the M107 summon apparitions
stay exactly as they are (2.0 s on message speed); **on Battle Speed Fast
the new animations are skipped entirely** (summons excepted), on Normal they
play, Instant keeps today's zero-length staging; Confirm still skips
everything.

## What was built

### The model (`src/render/ActionTier.hpp`, pure)

- **Family** = the M121 `content::SkillKind` already stored on every loaded
  skill (fire, ice, lightning, earth, holy, dark, non-elemental, heal, buff,
  debuff); a basic attack uses the weapon's damage kind (the same
  `damageKindFor` skills use); an item is a mender's sparks when it restores,
  chevrons when it applies a status; a summon has no ActionFx at all.
- **Tier** — `actionTierForSkill`: escalation applies to damage only, so a
  support skill never animates longer than a spell. **Grand** = all-enemies
  magic damage of power ≥ 10 (Inferno, Blizzard, Radiance, Chain Lightning,
  Sovereign's Cataclysm, the six Breaths) or single-target damage of power
  ≥ 20 (Meteor Dive, Eviscerate, Sovereign's Smite, Execute), and the
  Dragon's sweep; **Major** = other all-enemies damage (Whirlwind, Barrage,
  Cleave, Venom Mist) and single-target damage of power 14–19 (Aimed Shot,
  Arcane Burst, Power Smash, Shadow Bolt, Holy Ray, Fireball, Frost Lance…);
  **Minor** = everything else (Strike and every basic attack, small hits,
  heals, buffs, debuffs, items). Pinned over the shipped data by
  `tests/test_action_tier.cpp`.
- **The speed rule** lives in one function, `actionFxEnabled(speed)`:
  Normal → true; Fast and Instant → false.

### Timing (Normal speed; today was windup 0.18 + impact 0.14 + settle 0.90)

| Tier  | Windup | Impact | Burst frames | Tail (inside the settle) | Added per action |
|-------|--------|--------|--------------|--------------------------|------------------|
| Minor | 0.24 s | 0.14 s | 3            | 0.20 s                   | +0.06 s          |
| Major | 0.30 s | 0.14 s | 4            | 0.30 s                   | +0.12 s          |
| Grand | 0.42 s | 0.14 s | 6            | 0.45 s + one screen pulse| +0.24 s          |

The impact beat is never varied (it drives the hit flash and shake decay
everyone already knows). The burst keeps drawing through its tail inside
the 0.9 s settle that already exists, so no tail lengthens a turn — a test
pins every tier's burst (frames + tail) under the Normal settle. Frame step
0.07 s (≈14 fps, the 80s cadence). Fast: the tier collapses to the base
windup, the sequencer halves it as before, and nothing new is drawn — the
M91 element accent shows on Fast and Instant exactly as it did. Instant
short-circuits the staging as before.

### The sequencer (`render/BattleSequencer`)

`start(hasImpact, settleSeconds, params, windupSeconds = kWindupBase)` — one
defaulted parameter; `kWindupBase`/`kImpactBase` moved into the header. No
new stage, no elapsed-time getter: `BattleState` owns the effect clock
`actionFxT_` (reset when the sequence starts, advanced beside `seq_.update`,
so the capture freeze holds both) and the windup the tier chose
(`actionWindup_`, 0 when the action has no impact beat, so a buff's chevrons
count from the settle). Every existing sequencer test passes unmodified;
new cases pin the custom windup and its Fast/Instant scaling.

### The effects (`src/render/ActionFx.{hpp,cpp}` — procedural stepped pixels, the M91/M107 idiom)

Deterministic, no RNG, no textures; 2×2 pixel blocks so nothing is finer
than the sprites; high contrast collapses every motif to the palette's text
colour. During the **windup** a spell's caster rises 2 px and a stepped arc
of motes climbs over its head (`drawActionCast`); the acting unit's lunge is
the tier's reach (4 / 6 / 8 px, a Grand action holding its peak). From the
**impact beat** `drawActionBurst` plays on every unit the action moved
(hit, healed or status-touched — `fxTargets_`, staged with the floats), a
Grand action staggering its targets by 0.06 s so a chain hops across them:

- **non-elemental / basic attack** — a stepped slash band that breaks into
  chips; **fire** — flame columns rising from the feet to the belly, more
  per tier (the head stays readable); **ice** — shards falling in, then a
  splash of chips; **lightning** — a jagged bolt from the top edge, thick
  with a branch mid-burst, sparks at the feet; **earth** — rubble chips
  thrown up and falling back; **holy** — a plus expanding with a ring of
  pips; **dark** — a ring of wisps collapsing onto the unit; **heal** —
  sparks rising and a small cross; **buff / debuff** — three chevrons rising
  / sinking (a status-only action has no impact beat, so they play in the
  first fraction of the settle at no added time).
- **The Grand wash**: the M51 AoE tint mechanism with its cap raised from
  0.12 to 0.20 and the family's own hue (ember, ice, gold-white, violet…)
  during the impact beat — one decay pulse through the tested
  `aoeTintAlpha`, gated by the Battle Flash setting (Off → none, Reduced →
  half), never a strobe. Enemies' Grand breaths get the same.
- The M91 single-step accent is superseded on Normal (never both drawn) and
  untouched on Fast/Instant.

Wired beside the M91 element in **all five hit-dealing paths** —
`executePending` (attack, skill, item), `revealMimic`, `executeEnemy`,
`executeConfused`, `executeUncontrolled` — plus the decision resolution,
the status ticks and the element capture, each resetting to a neutral
animation first so nothing leaks between actions.

### Captures

`captureActionFxAt(skillId, atSeconds)` resolves a real action (the basic
attack for an empty id, taken by the first party member whose swing is a
single strike — the capture party's lead sweeps the field), runs the
sequencer AND the effect clock forward and freezes both; an ally-facing
skill wounds the party first so the heal has something to move. Four scenes:
`189_anim_minor_strike`, `190_anim_grand_inferno` (mid-columns with the
ember wash), `191_anim_grand_chain_lightning` (the bolts hopping),
`192_anim_heal` (sparks and the +20s). **192 scenes**.

## Decisions taken without asking (veto any of them)

1. **No new Settings row** — the Fast rule covers pacing; Battle Flash still
   gates the wash and the hit flash.
2. **No new SFX** — the hit, element, heal and status roles fire at the
   commit exactly as today.
3. **Items are Minor** (sparks for a restorative, chevrons for a draught);
   the plan's separate bottle glint was folded into these.
4. **Buffs and debuffs add no beat** — they had none; their chevrons live in
   the settle.
5. **Enemy basic attacks reuse the slash** (the same motif, mirrored by the
   attacker's side) rather than a separate claw.
6. **The bursts draw regardless of the Battle Flash level** (they are small
   pixel motifs, not flashes); only the Grand wash and the white hit flash
   are gated.
7. **Procedural over sprite strips** — no asset-pipeline growth, one review
   surface, byte-exact captures.

## Compatibility

No save, settings, content, generation or score change. The Simulator never
sees any of it; every pre-existing battle and simulator test passes
unmodified.

## Known limitations

- The motifs are shared per family: every fire skill is columns, every
  slash the same band — the tier, not the skill, sets the size. A per-skill
  motif would need an authored field (a schema decision the owner has not
  asked for).
- At Fast the M91 accent still shows, so Fast is not "no effects" — it is
  exactly what it was.
- The Grand stagger is 0.06 s per target; on a five-foe field the last bolt
  lands 0.24 s after the first, inside the tail.
- The chevrons of a single-target buff cast by an enemy AI rely on the AI's
  chosen target being recorded; a scripted status-all step marks the whole
  side.

## Documentation updated

`docs/milestones.md` (row + program section), this note,
`docs/game_design.md` (§8 the presentation paragraph: the tiers, the Fast
rule), `docs/technical_design.md` (the M18 sequencer paragraph, a new M130
section, the capture count), `docs/art_bible.md` (§8 effect conventions),
`docs/manual_test_matrix.md` (rows 285–287). `docs/ui_style_guide.md` and
the README restate nothing here.

## Completion report

### 1. Implementation summary

**M130 — Short attack & skill animations.** Complete: the derived model, the
sequencer parameter, the procedural effects on every action path, the Grand
wash, the capture hook and scenes, the tests, the docs.

### 2. Files changed

- **Source (new):** `src/render/ActionTier.hpp`, `src/render/ActionFx.hpp`,
  `src/render/ActionFx.cpp`.
- **Source (changed):** `src/render/BattleSequencer.{hpp,cpp}`,
  `src/states/BattleState.{hpp,cpp}`, `src/states/AoeTint.hpp`,
  `src/capture/CaptureRunner.cpp`, `CMakeLists.txt`.
- **Tests:** `tests/test_action_tier.cpp` (new), `tests/test_battle_sequencer.cpp`,
  `tests/CMakeLists.txt`.
- **Docs:** see "Documentation updated".

### 3. Plan deviations

The item glint folded into the heal/buff motifs (decision 3); the bursts
not gated by the flash level (decision 6). Nothing else.

### 4. Compatibility

See "Compatibility". Deterministic seeds: unchanged.

### 5. Automated validation

All run 2026-09-22 from the VS 2022 developer shell (amd64):

- `cmake --build --preset debug` — **succeeded**, no warnings.
- `crystal_tests.exe "[m130],[battleseq]"` — **10 test cases, 217
  assertions, all passed**.
- `ArePGeese.exe --capture <dir>` — **192/192 scenes clean** (four new).
  Read by eye: `189`–`192`, `61`, `17`.
- `ctest --preset debug` — **974/974 passed** (807 s, alongside the Release
  run).
- `cmake --build --preset release` — **succeeded**, no warnings.
- `ctest --preset release` — **970/970 passed** (707 s; the Release preset
  carries no capture-only cases).

### 6. Manual owner validation

Matrix rows **285–287**: the beats at Normal (a strike, a small spell, a
heal, a buff, Inferno and Chain Lightning, the Dragon's sweep), the Fast
rule (nothing new plays; the fight is exactly as quick as before), the
summons untouched, and above all the pacing — whether a long fight still
feels brisk. That judgement is the owner's.

### 7. Known limitations

See "Known limitations".

### 8. Documentation updated

See "Documentation updated".

### 9. Final status

`implemented, awaiting manual approval`
