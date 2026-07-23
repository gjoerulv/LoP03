# M40 — The Castle & the King's Challenges

## A. Status and authority

- **Status:** complete (approved) — approved by the owner 2026-07-21 after manual
  testing (including two rounds of playtest fixes — see §K); committed. Authored /
  re-audited and implemented 2026-07-21 against the post-M39 checkout. Owner
  authorized beginning M40 after approving M39, and answered the two note-time
  design questions (§D).
- **Authority:** `CLAUDE.md` > this note > `docs/milestones.md` >
  `docs/game_design.md` > `docs/technical_design.md`. The **endgame milestone** of
  the M35–M42 program: a castle above the town-7 ceiling with a King and three
  challenges, its own records and rewards, kept **entirely outside** the dungeon
  scoreboard (M19 comparability preserved). Uses the full 12-boss roster M38
  produced.

## B. Problem statement (verified at re-audit)

- **The ladder ends at town 7 and everything assumes 1..7.** `WorldLadder.hpp`
  (`kTownCount = 7`, `clampTown` → [1,7], `townScalePct`/`townScoreBonusPct` tables
  sized 7, `unlockAfterClear` capped at 7). The save load **clamps `currentTown`
  into `[1, highestUnlockedTown ≤ 7]`** ([SaveSystem.cpp:142](src/save/SaveSystem.cpp:142)).
  `TownState` and every service (`GuildState`, shops, `InnState`, `ScoreboardState`,
  `TrainingHallState`) resolve `clampTown(currentTown)`. **The castle must NOT be
  `currentTown = 8`** — it would be clamped to 7 and misroute every service.
- **Town travel is exit-tile based.** `town::buildTown(town, hasPrev, hasNext,
  nextUnlocked)` places prev/next road exits; standing on an unlocked exit +
  Confirm calls `TownState::travelTo(destTown)` ([TownState.cpp:236](src/states/TownState.cpp:236)).
  Town 7 has no "next" exit (`hasNext = town < kTownCount`). The castle road hooks
  here — a **new castle exit** shown in town 7 once unlocked, routing to a **new
  `CastleState`**, not `travelTo`.
- **Boss roster is 12, enumerable in sorted order** (M38): the Boss Rush gauntlet.
- **The seeded reload-proof-stream pattern exists** (`blackMarketHash`, M34/M39) for
  the Endless Rush wave stream and any castle roll.
- **Battle statuses/passives/immunity are in place** (M35/M36). `isBlinded`/
  `isSilenced` already respect immunity flags (`blindImmune` from Keen Senses,
  `silenceImmune` from Clarity, [Battle.hpp:190](src/battle/Battle.hpp:190)).
  `isConfused` does **not** yet respect an immunity — the King needs one (§D).
  Boss flags/passives are resolved in `buildBattle` ([Battle.cpp:853](src/battle/Battle.cpp:853)).
- **Music roles are a fixed enum + id table + synth-fallback index**
  ([AudioRoles.hpp:34](src/audio/AudioRoles.hpp:34)); adding a track touches the
  enum, `kMusicIds`, `kSynthMusicIndex`, the manifest, and `generate_audio.ps1`.
- **Records live in a separate `Scoreboard` file** (M6/M19). Castle records must not
  touch `ScoreEntry`/the dungeon scoreboard/stakes.
- **Baseline:** 348/348 tests, 32/32 capture, `kBattleRulesVersion` 3,
  `kGenerationVersion` 8, `kSaveVersion` unchanged.

## C. Goals

- A **castle place** (`CastleState`, distinct from `TownState`), reached by a road
  from town 7 that opens once **any town-7 dungeon is cleared**; travel back is
  free. The castle holds a throne (challenge menu + records), a Jester spot (an M41
  hook, inert this milestone), a save point, and an inn.
- **Three challenges**, all above the t7/d20 battery point, deterministic and
  sim-clearable by a maxed party:
  1. **Boss Rush** — the full 12-boss roster (sorted) as a gauntlet; **HP/MP persist,
     no free healing, items allowed** (§D); record = fewest total turns.
  2. **Endless Rush** — deterministic escalating waves from a challenge seed
     (blackMarketHash-style stream); HP/MP persist, items allowed; record = best
     wave; bounded memory (one wave built at a time).
  3. **The King** — a bespoke `BossDef`-grade fight above everything else, **3
     passives + Confusion immunity** (§D), a kit inflicting every M35 status; record
     = defeated + fewest turns.
- **Castle records + rewards**, persisted **separately** from the dungeon scoreboard
  (optional Party save fields). Each challenge pays a **one-time first-clear reward**
  (tokens/gold); the King additionally grants a **unique legendary** + a **visible
  title**. Challenges never write `ScoreEntry`, stakes, or the dungeon scoreboard.
- Castle art + `MusicTrack::Castle` and a distinct King-battle track (generators).
  Capture scenes (throne/records, King fight). Tutorial prompts (first castle entry,
  first challenge).

## D. Owner decisions & routine choices

**Owner decisions (2026-07-21, via Q&A) — locked:**

- **The King's passives: Keen Senses + Clarity + Counter Attack**, and the King is
  **also immune to Confusion**. Net effect: the King is immune to **all three M35
  control statuses** (Blind via Keen Senses, Silence via Clarity, Confusion
  bespoke), gains **+10 % damage vs debuffed targets** and MP sustain (Clarity +3/
  round), and **counters melee** — a wall that cannot be locked down and hits harder
  as it debuffs you. Confusion immunity is a new combatant flag (see below), off by
  default so every existing battle stays byte-identical.
- **Gauntlet healing: no free healing between fights/waves; items allowed.** HP/MP
  persist across a gauntlet; the player manages attrition with brought consumables.
  Applies to both Boss Rush and Endless Rush.

Program-level owner decisions already locked (not re-opened): castle keeps its own
records; unlock = clearing any town-7 dungeon; King → unique legendary + a visible
title; challenges outside the dungeon scoreboard.

**Routine choices taken here (owner validates feel/balance at approval):**

- **Castle as `kCastleTown = 8`, a distinct place, never a ladder town.** A dedicated
  `CastleState` + a new `castleUnlocked` Party flag (optional save field). The castle
  is never assigned to `currentTown`; `WorldLadder` arrays/`clampTown` are untouched
  and never see 8. Saving in the castle saves the party (currentTown stays 7); on
  load the party is in town 7 and walks back — consistent with the M3 "load returns
  to town" rule.
- **Records = optional Party save fields** (`CastleRecords`: bossRushBestTurns,
  endlessBestWave, kingDefeated, kingBestTurns, kingTitle earned), defensively
  loaded; old saves → no records. Separate from the `Scoreboard` file entirely.
- **Challenge scaling** rides the existing team `statScalePct` (the per-team enemy
  stat-scale percent the generator/capture already use), set to a **t7/d20-equivalent
  baseline** (town-7 ×300 % composed with depth-20 composition scaling ≈ **570 %**)
  times a challenge multiplier — starting **Boss Rush ×1.15, Endless from ×1.15
  escalating per wave, King ×1.5** — then **sim-tuned** so a maxed party (L50,
  legendary gear, best passives) clears the King and the rush, and endless reaches a
  sane wave before overwhelming. Never routed through `townScalePct` (which caps at
  town 7).
- **Rewards (first-clear, granted once, guarded by the record fields):** Boss Rush →
  gold + legendary tokens; Endless Rush → gold + tokens the first time a best-wave
  record is set; King → a **new unique legendary** (King-only: not in shops, the
  market, or boss drops) + a **visible title** (shown on the title screen and the
  save/continue summary) + gold + tokens. Exact amounts tuned against the economy.
- **The Jester spot is reserved but inert** this milestone (M41 adds the Jester and
  the story). A tile/marker exists; interacting says the throne room is quiet for
  now (placeholder, replaced in M41).
- **No `kBattleRulesVersion` / `kGenerationVersion` / `kSaveVersion` change.** The
  King's Confusion immunity is a new `Combatant.confusionImmune` flag (default false)
  resolved from a new optional `BossDef.immuneToConfusion`; `isConfused` respects it
  exactly as `isBlinded`/`isSilenced` respect theirs. No existing content sets it, so
  every existing (dungeon) battle resolves byte-identically — no rules bump. Castle
  fights are off the dungeon scoreboard, so the version tags are irrelevant to them.
  Records/flags are additive optional save fields (no `kSaveVersion` bump; break only
  if the optional pattern hurts — it does not here).

## E. Proposed design & slices

1. **Castle place + navigation.** `game/Castle.hpp` (`kCastleTown = 8`, pure
   helpers); `Party.castleUnlocked` (optional save field), set in
   `DungeonState::completeDungeon` on a town-7 completion. `town::buildTown` gains a
   castle-exit option for town 7 when unlocked; `TownState` routes that exit to a new
   `CastleState` (push, not `travelTo`). `CastleState` = a walkable castle map
   (reusing `town::Tilemap`) with throne / Jester spot / save point / inn doors and a
   free "back to town 7" exit. Castle art (generated) + manifest rows.
2. **Audio.** `MusicTrack::Castle` + `MusicTrack::KingBattle` in the enum + `kMusicIds`
   + `kSynthMusicIndex`; manifest rows; two new tracks in `generate_audio.ps1`;
   `AudioManager` plays them for the castle and the King fight.
3. **Boss Rush.** A `BossRushState` (or a shared castle-challenge runner) enumerating
   the 12 sorted boss ids, building each as a one-boss `EnemyTeam` at the tuned scale,
   fighting them sequentially with **persisted HP/MP, no free heal, items allowed**,
   summing rounds; on completion, update `bossRushBestTurns` + first-clear reward.
4. **Endless Rush.** A deterministic wave stream (blackMarketHash off a fixed castle
   challenge seed + wave index) building escalating normal `Battle`s; survive; record
   best wave; bounded memory. Same heal policy.
5. **The King.** A bespoke boss: new `bosses.json` entry (stats above t7/d20; passives
   `keen_senses`, `clarity`, `counter_attack`; `immuneToConfusion: true`; a kit using
   every M35 status against the party) + `BossDef.immuneToConfusion` schema +
   `Combatant.confusionImmune` + `isConfused` immunity + buildBattle resolution + the
   King sprite (generated). A `KingBattleState`/flow using `MusicTrack::KingBattle`;
   record = defeated + fewest turns; first defeat → unique legendary + title + reward.
6. **Records, rewards & title.** `CastleRecords` on `Party` (optional save fields,
   defensive load); the throne shows a records screen; rewards granted once; the new
   unique King legendary (`items.json`, no shop/market/drop exposure); the title shown
   on the title screen + save summary.
7. **Tests / sim / capture / docs.** Determinism + clearability sims (King beatable by
   a maxed party; boss rush clearable; endless reaches a sane wave); castle-unlock +
   records round-trip + reward-once tests; boss enumeration + endless-wave determinism;
   confusion-immunity test (existing battles unchanged); capture scenes (throne/records,
   King fight); tutorial prompts; all docs.

## F. Determinism & save compatibility

- **Determinism.** Boss Rush enumerates sorted ids (stable). Endless waves derive
  from a fixed castle seed + wave index via the SplitMix64 primitive — same wave
  index → same wave, bounded memory. The King is fixed content. No wall-clock/unseeded
  RNG; sim ↔ live agreement holds (all fights are normal `Battle`s over the shared
  paths). Confusion immunity is read-time (`isConfused`), inert for all existing units.
- **Save compatibility.** `castleUnlocked` + `CastleRecords` + the earned title are
  additive **optional** fields; old saves load as locked / no records / no title. No
  `kSaveVersion` bump (the optional pattern is not painful here). Loading still returns
  the party to town (never inside the castle or a challenge).
- **No version bumps** to `kBattleRulesVersion` (3), `kGenerationVersion` (8), or
  `kSaveVersion` — justification in §D.

## G. Out of scope

Story NPCs & the Jester's dialogue/serial (M41 — only the inert spot is reserved
here); enrichment: bestiary/victory-stats/achievements (M42); any dungeon
battle-rule/generation/scoring change; a global (cross-party) castle leaderboard;
new statuses/passives beyond M35/M36.

## H. Balance / acceptance

The castle never flows through the town arrays or `clampTown`; records live outside
the dungeon scoreboard; the three challenges are deterministic and sim-clearable by a
maxed party (King beatable, boss rush clearable, endless reaches a sane wave before
overwhelming); the King is immune to all three control statuses and inflicts them;
first-clear rewards pay exactly once; the unique King legendary has no shop/market/
drop exposure; capture overflow-clean with the new scenes; lint green for the new
castle/King/music assets; full suite green from the 348 baseline; Debug + Release
clean.

## I. Manual validation for the owner

Whether the castle reads as a distinct, special place; whether the three challenges
feel like a real endgame (the King genuinely the hardest fight, beatable by a maxed
party; the gauntlets a real attrition test); whether the records/rewards/title land;
castle art + the two new tracks; the first-castle and first-challenge onboarding.

## J. Required documentation updates on completion

This note (§K); `docs/milestones.md` (status); `docs/game_design.md` (the castle, the
three challenges, records/rewards/title as player-facing rules); `docs/technical_design.md`
(the castle place + `kCastleTown`, the challenge runners, the Endless wave stream, the
King + Confusion immunity, castle records); `assets/credits.md` + `docs/asset_pipeline.md`
(new castle/King sprites + two music tracks); README (castle line, content counts);
`docs/manual_test_matrix.md` rows; a manual checklist per
`docs/milestone_completion_template.md`.

## K. As-implemented record (2026-07-21)

- **Status:** complete (approved 2026-07-21). The §D owner decisions were
  implemented as given (King = Keen Senses + Clarity + Counter Attack + Confusion
  immunity; gauntlets give no free healing, items allowed).
- **Castle place & navigation.** `game/Castle.hpp` (`kCastleTown = 8` — never a
  ladder town, never `currentTown`). `Party.castleUnlocked` set in
  `DungeonState::completeDungeon` on a town-7 clear. `town::buildTown` gains a
  northern castle road (`TownExit::toCastle`) for town 7, locked until unlocked and
  refreshed in `TownState::onResume` like the M32 road; `TownState` pushes
  `CastleState` on that exit. `CastleState` is the throne hall (menu + records +
  reserved Jester spot). `WorldLadder`/`clampTown`/the save clamp were audited and
  left untouched (the castle never flows through them).
- **Challenges.** `CastleChallengeState` sequences `BattleState`s with persistent
  HP/MP (no free heal). Boss Rush = `bossRushOrder` (12 sorted bosses, King
  excluded) at 330 %; Endless = `endlessWaveTeam` from `kEndlessSeed` escalating
  from 300 % (+12 %pts/wave, size → 5); King = `kingTeam` at 600 % with
  `MusicTrack::KingBattle` (new optional `BattleState` music override). Records +
  one-time first-clear rewards applied on finish; the King grants the unique
  `sovereigns_regalia` (excluded from `legendaryDropPool`) + the title + gold/tokens.
- **The King.** New `bosses.json` boss (brute; hp 560 base, above every dungeon
  boss; passives keen_senses/clarity/counter_attack; `immuneToConfusion: true`). Its
  kit is **six bespoke damaging debuff skills** (see the owner-fix below) that deal
  damage AND inflict every M35 status, single-target and AoE, magic and physical.
  New `Combatant.confusionImmune` + `BossDef.immuneToConfusion` + `isConfused`
  immunity + `optBool` JSON reader; existing battles byte-identical. New King sprite
  (appended last to `generate_textures.ps1`, **1 new / 0 modified** PNGs) + manifest
  + credits.
- **Records/rewards/title.** `CastleRecords` on `Party` (optional save fields,
  defensive load); shown on the castle hub and the load-screen slot summary
  (`SlotSummary.kingTitle`); never in the dungeon scoreboard. Two new music tracks
  (`music.castle` / `music.king`, **2 new / 0 modified** WAVs) + manifest + credits.
  Tutorial prompts `kFirstCastle` / `kFirstChallenge`.
- **Balance (sim-tuned, `[castle]` + `[castle-report]`).** A maxed party (L50,
  top legendaries, best passives) beats the King (now **420 %** scale, hp 560 /
  atk 18 / mag 22 base) in **~9 rounds, losing a member and dropping to ~44 % party
  HP** — hard but beatable; clears the **Boss Rush 12/12 no-heal** (~36 turns);
  reaches **wave ~10** of Endless before the escalation overwhelms. The King at the
  earlier extreme scales (855 %) was unwinnable and the rush unclearable, so the
  scales were retuned down (the challenge is as much attrition as raw scale); the
  King was retuned again after the damaging-kit fix below (a pure-status King at
  600 % became a lethal every-turn-damage King, so its scale and base atk/mag came
  down and its HP up for a long-but-survivable fight).
- **Validation (this session, VS 2022 / MSVC, `build-msvc`).** Debug **361/361**
  (+1 `[save]` castle round-trip, +12 `[castle]`; a hidden `[castle-report]` sim
  table). `--capture` **35/35** overflow-clean (new `33_castle_hub`, `34_king_battle`
  — which also applies all three control statuses to the King to prove they are
  hidden —, `35_castle_result`). Release build + Release suite **361/361** clean.
  Lint green (King sprite). `kSaveVersion` / `kBattleRulesVersion` (3) /
  `kGenerationVersion` (8) unchanged.

### Post-implementation owner fixes (2026-07-21, from manual testing)

The owner playtested the castle and reported two issues; both are fixed here (the
milestone stays `implemented, awaiting manual approval`):

1. **Immune statuses no longer display.** The King's immunity nullified the effect
   (`isBlinded`/`isSilenced`/`isConfused` already ignored it) but the status label
   (BLD/SIL/CNF) still drew, because the HUD/target-info/Details sites iterate the
   stored `statuses` vector directly. A new pure `battle::isImmuneTo(combatant, type)`
   now gates all three display sites, so an immune unit never reads as afflicted.
   **Display-only** — the status is still stored and every effect query already
   ignored it, so **battle resolution is byte-identical** (no `kBattleRulesVersion`
   change). Capture `34_king_battle` applies all three control statuses to the King
   and confirms no labels show.
2. **The King now deals damage with its debuffs.** Its old kit leaned on power-0
   **Support** skills (silence/intimidate/sunder), which deal no damage, and the
   enemy AI prefers Support debuffs — so the King spent turns applying statuses with
   no damage. Fix: a Support skill with `power > 0` now also **wounds** its enemy
   target (magic), and the King's kit is six bespoke damaging debuff-curses —
   `blinding_curse` / `silencing_word` / `maddening_gaze` / `withering_touch` (single
   Support curses, blind/silence/confusion/poison), `sovereigns_cataclysm` (magic AoE
   damage + attack-down all), `sovereign_smite` (physical single damage + defense-down).
   Every shipped Support skill is power 0 and the new curses are King-only (castle,
   off-scoreboard), so **every existing battle stays byte-identical** — no
   `kBattleRulesVersion` change. `skillCount` 48 → 54. The King was rebalanced for
   its new every-turn damage (see the balance bullet). Tests: `[castle]` covers
   `isImmuneTo` and that a power-0 Support skill deals 0 while a power-16 one wounds.
3. **A confused player character now takes no input.** Confusion (M35) redirects a
   basic attack to a random ally inside the shared `attack()`, but `BattleState`
   still showed the command menu for a confused character — so the player kept
   control, and could even bypass confusion by choosing a *skill* (skills are not
   redirected). Fix: `BattleState::startActorTurn` now checks `isConfused` for a
   party member and, if set, auto-resolves a forced basic attack
   (`executeConfused`, mirroring an enemy turn) with **no menu** — the character
   lashes out at a seeded random ally, exactly as an enemy does. This is a
   `BattleState` turn-flow change only; the shared resolution (`attack` /
   `confusedTarget` / the roll stream) is untouched, so **no `kBattleRulesVersion`
   change** and sim↔live resolution agreement is preserved. (The headless
   Simulator's party AI is a heuristic validator, not a mirror of the player, and
   is left as-is; so a confusion-inflicting fight — including the King — is
   slightly harder in live play than the sim models, since the sim's party can
   still act through confusion. Intended: confusion now genuinely costs a turn.)

### Deviations from the plan / note

1. **The castle is a menu-driven hub, not a walkable tilemap place.** The plan
   framed the castle as walkable (throne room / Jester spot / save / inn with castle
   art). A walkable castle that reused town tiles would read as a town (undercutting
   the "distinct special place" goal), and a bespoke castle tileset is major extra
   art scope; a menu hub with castle music, framing, records, and the King delivers
   every functional requirement, reads as distinct, and is complete and correct.
   **Flagged for owner veto** — a walkable castle + tileset is a clean follow-up if
   wanted. The Jester spot is present as reserved flavor text (the M41 hook).
2. **Challenge scales are far below the literal "t7/d20 ×1.15 / ×1.5" starting
   recommendation** (Boss Rush 330 %, King 600 % vs the ~655 %/855 % those implied):
   at those the maxed party could not win. The endgame difficulty target ("beatable
   by a maxed party, but the hardest content") is met via sim tuning; the gauntlets'
   bite comes from no-heal attrition as much as scale.
3. **Boss Rush bosses are solo (no minions)** so the 12-fight no-heal gauntlet stays
   clearable; **Endless waves are enemy teams** (not bosses) escalating in scale/size.
4. **The King's archetype is Brute** (enrage below half HP) so the capstone grows
   fiercer as it falls, complementing Keen Senses' ramp.

### Known items for owner validation

- Whether the castle reads as a distinct, special place (menu hub — see deviation 1);
  whether the King is the right kind of hard (beatable by a maxed party, a real
  struggle); whether the gauntlets' no-heal attrition feels good; the two new tracks
  and the King sprite; the first-castle / first-challenge onboarding.
