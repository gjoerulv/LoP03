# M89 — Battle flow: the Dragon & the carry-out (rules v16)

**Status:** complete (approved 2026-08-16)
**Program:** M88–M97 (authorized 2026-08-14). Battle rules **15 → 16**.

## Scope (owner items 11 and 7)

- **Item 11:** the Last Dragon stops using its breath moves early — it runs
  out of MP. Give it more MP, but have it make normal attacks at rare
  moments too.
- **Item 7:** a dungeon wipe should no longer fully heal the party: MP
  stays, a single member returns at 1 HP. Owner Q&A: the half-gold loss
  STAYS; only the free heal goes.

## Root cause & what was built

- **Diagnosis:** enemy MP derives 1:1 from scaled Magic. The Dragon's
  MAG 26 at its 500 % arena gave ~130 MP; each breath costs 20 → six
  breaths, then plain swipes for the rest of the game's largest fight.
- **The authored pool:** optional `maxMp` on EnemyDef/BossDef (0 = derived,
  every pre-M89 foe). A positive value replaces the base and scales like
  Magic. Dragon: `maxMp: 80` → **400 MP / twenty breaths** at the arena.
- **The lunge:** BossDef `basicAttackEveryNth` (+ optional
  `basicAttackText`, loader-validated). `Combatant.ownTurnsTaken` advances
  in `beginUnitTurn` (the M49 revive-clock seam — driver-identical by
  construction); pure `battle::basicAttackTurn()` feeds both
  `chooseEnemyAction` (the swing replaces the cast) and BattleState's
  flavour line ("The Dragon's breath catches. It lunges instead." — the
  doNothingText presentation pattern). Dragon: every **4th** own turn.
- **The carry-out:** `DungeonState` defeat now calls the M47
  `clampCastleDefeat` (reused as-is: survivors→1 HP, fallen stay fallen,
  MP untouched, a wipe leaves exactly one member standing) and keeps
  `gold /= 2`. New one-time tutorial beat `kCarriedOut` fires from
  `TownState::onResume` on the first arrival with fallen members.
- **CrystalForge:** Enemies + Bosses descriptors updated in the same
  milestone (maxMp; basicAttackEveryNth; basicAttackText).

## Balance consequence (deliberate, owner-visible)

A Dragon that never runs dry is strictly harder. Sim evidence
(`test_dragon.cpp` gauntlet battery): the pre-M89 plain-accessory maxed
loadout now **loses** in 15 rounds; with the DESIGNED counter — the
elemental defense layer the design doc always named ("six party-wide
elemental breaths … met by the M81 ward charms") — the maxed party wearing
**Motley Aegis** clears the gauntlet in **22 rounds** (vigils 3+3+3, the
Dragon 13). The clearability bar now carries that counterplay, exactly as
the King's bar carries relics and snacks. Whether the difficulty FEELS
right is the owner's manual judgment (matrix row 171).

## Plan deviations

None of substance. The rare-attack cadence is the planned every-4th rule;
`maxMp` landed on 80 (scaled 400) rather than the plan's "≈240 absolute"
sketch because the implementation scales the authored base like Magic —
recorded here; same intent (breaths span the whole fight).

## Compatibility

- **Saves/settings/scores:** untouched; scoreboard entries tag rules v16
  going forward, old entries keep their version (never renormalized).
- **Content schemas:** three additive optional fields (absent = pre-M89
  behavior byte-identical; a battle whose content carries none of them and
  never wipes resolves as before — the version bump marks the Dragon fight
  and the defeat rework honestly).
- **Deterministic seeds:** generation untouched (no gen bump).

## Automated validation

- `cmake --build --preset debug` (VS2022 shell) — clean; `--preset release`
  — clean.
- `ctest --preset debug` — **751/751 pass** (two new dragon test cases:
  the scaled authored pool + the every-4th lunge walked through the real
  per-turn seam). One pre-existing pin updated: `test_rules_v15.cpp`'s
  `== 15` became `>= 15` (the version moved by design).
- Capture — **107/107 scenes clean**.

## Manual owner checklist

Matrix rows **171** (Dragon: breath cadence, the lunge line, difficulty
judgment — bring wards/Aegis) and **172** (carry-out: wipe → one member at
1 HP, MP kept, half gold gone, one-time prompt, Inn recovery). Also worth
a glance: a castle-challenge defeat still behaves exactly as before (same
shared helper, no double application).

## Known limitations

- The lunge flavour line shows in live play only (the Simulator ignores
  presentation text by design; outcomes are identical).
- No dedicated "defeat screen": the carry-out communicates through the
  one-time prompt and the party's visible state, as scoped.

## Documentation updated

- `docs/milestones.md` — M89 row.
- `docs/game_design.md` — M47 castle-stakes section (dungeon-defeat line
  superseded + the new carry-out paragraph); §10 Dragon section (the
  sustained-breath paragraph).
- `docs/technical_design.md` — new §42.
- `docs/manual_test_matrix.md` — rows 171–172.

## Final status

`implemented, awaiting manual approval`
