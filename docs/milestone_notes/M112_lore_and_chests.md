# M112 — The Jester's Lore trap & the Treasure chests (the Mimic, the mocking jingle)

**Status:** implemented, awaiting manual approval
**Program:** M109–M116 (owner-authorized 2026-09-02 via the approved plan).
**No version motion:** battle rules stay 19 (the three new `Battle` helpers
change no rule), generation stays 24, saves stay v1, content files stay v1
(one new optional file, one new boss, three new manifest ids at v2).

## Scope (plan section M112; owner sections 6, 7, 8, 12–17)

The dispatcher's 5 % Lore kind and 15 % Chests kind become real
encounters, hosted by the battle screen's new decision mode: the Jester's
question (two answers, the Jester itself, restricted commands, +500 gold
or a mock, a real KO for striking the Jester or sweeping the field, one
battle turn, no fake kills), and the three chests (reward / Mimic / empty
shuffled by the seed, the 50/50 gold-or-gear reward with the equipment
filter, the Mimic revealing itself into a boss fight in place with the
committed action as the opening blow, boss-equivalent XP + 500 gold). Plus
the mocking jingle (a new audio role), 28 original editable lore
questions with a Forge category, the Mimic's original art, the closed
chest prop, the one centralized AOE definition.

## What was built

- **Content:** `data/lore_questions.json` (28 questions, four per tier,
  one post-King; authored from shipped lore only — boss telegraphs, the
  Stranger's town scenes, the Guild Masters, the founding classes, the
  Goosy Gauntlet at tier 7; never a curio, never anything gated behind the
  King except the one flagged question), `data/bosses.json` `mimic`
  (brute, HP 300 / ATK 24 / MAG 8 / DEF 16 / SPD 10, power_smash + sunder
  + venom_fang, Counter Attack, `specialOnly`, xp 230, gold 0, "The lid
  was a lie."). Schema: `content::LoreQuestionDef`, `parseLoreQuestions`
  (the fourth optional file), the database accessors, Forge
  `Category::LoreQuestions` (count 15, inline canonical style, validation
  row), `styleForFile`.
- **Models** (`src/game/SpecialEncounter.hpp`, pure): the encounter kinds
  and results, `makeLoreEncounter` (eligibility = tier + King gate; the
  run-seeded Fisher-Yates walk by the per-kind ordinal; the answer side by
  hash), `makeChestEncounter` (roles by hash, the coin flip, the gear pool
  filter, the Mimic team at the shadow patrol's scale + `mimicSpoils`),
  `appendPlaceholders`, `resolveSpecial`, `carryPartyOver`,
  `orderAfterDecision`, `skillIsOffensive`, `specialPrompt`.
- **Engine helpers** (`Battle::hostileTargetCount`, `spendMp`, `koUnit`):
  the one AOE definition through the battle's own targeting; the
  decision's MP spend + Action event; the outright KO with its events.
- **Decision mode** (`BattleState`): the trailing `SpecialEncounter*`
  parameter, `inert_` bookkeeping, `noteRoster` (extracted from the ctor,
  skipping placeholders), `pruneOrder`, the command/skill menu
  restrictions with reasons, the prompt strip, the answer boxes and chest
  sprites in `drawUnit`, the target panel's choice line, the Details line,
  `resolveDecision` (rewards paid there; punishments; the settled-beat
  ending with Victory / EnemyFled / a real Defeat), `revealMimic` (the
  in-place morph; see technical_design §54), the Mock/Victory/Defeat
  jingle choice, `captureSpecialPick`.
- **Dungeon** (`DungeonState`): the Lore and Chests cases of
  `triggerPatrol`, `startSpecialBattle`, `special_`, the onResume tallies
  and corridor lines; fallbacks to an ordinary patrol when the content
  lacks a pool or the boss.
- **Audio:** `MusicTrack::Mock` / `music.mock` (`kMusicCount` 16, a
  jingle, `Sfx::Error` fallback), the original phrase in `music_data.ps1`,
  the manifest row; `generate_audio.ps1` re-run wrote only `mock.wav`.
- **Art:** `boss.mimic.battle` (36×36: a chest whose lid swung open into a
  maw — teeth on both rims, one red eye in the dark, a tongue of coins,
  two loose coins on the ground) and `prop.chest_battle` (24×24 closed
  chest), both hand-placed grids appended last (`Save-PropGrid` added);
  `generate_textures.ps1` re-run wrote only the two new PNGs; manifest and
  credits rows.
- **Captures:** `131_lore_question` (the longest question and answer),
  `132_lore_result_mock` (the wrong answer's settled beat, the longest
  mock line), `133_chests`, `134_mimic_revealed`.

## Deviations from the plan

- The decision's neutral ending (a wrong answer, an empty chest, a
  punishment that did not wipe the party) reuses `Outcome::EnemyFled` —
  the M111 "nothing gained, no escape counted" outcome — rather than a new
  Outcome value (no pure-model change, as the plan required); the screen's
  own message stands in for the flee line and the dungeon's corridor line
  comes from the encounter's result. A reward is `Victory`; a wipe is
  `Defeat`.
- The chest sprite id is `prop.chest_battle` (a prop, beside the M11 chest
  prop) rather than the plan's `ui.special.chest.battle`.
- The decision's reward is paid by the screen at the moment it is shown
  (the spoils precedent); the dungeon only tallies the patrol ledger.
- The command menu's greyed rows explain themselves in the info column
  (the existing "why" line) rather than in-row suffixes.
- An armed dragonform/flock is not spent on a decision patrol (it waits
  for the next real fight) — a routine choice, recorded here.

## Tests

`tests/test_lore_encounter.cpp` [lore]: the shipped pool (28, four a
tier, one post-King, length bounds), the loader's shape rules, the
eligibility gate at every tier × King state (sorted pools; 4 / 27 / 28),
the seeded walk (every eligible question before a repeat, the wrap, a
different seed walks differently), the answer sides (both seen; texts
match the question), the empty-pool nullopt, the field (Jester + two
answers, alive, inert only to the screen, a living side to the model), the
resolution rule (correct / wrong / Jester / sweep), the one AOE definition
over every shipped skill (all-enemies → 3, single → 1, ally-facing → 0)
and the sweep. `tests/test_chest_encounter.cpp` [chest]: the Mimic's kit
and `specialOnly` (no other boss carries it), the Mimic in no pool (rush
order, endless boss waves, themes, the generator's sweep over 40 seeds ×
7 towns), the roles (one of each, every slot seen, deterministic), the
coin flip and the gear filter, the pool per town (worn equipment only,
sold there), the resolution rule, the field and the Mimic team at scale,
the morph's pure halves (carry-over of HP/MP/statuses/guard/acted/own
turns/summons/debug/round, the Mimic untouched, the decider out of the
rest of round one, a fresh roll stream). The `[mimic-report]` battery
(`-s`, `[!benchmark]`) sims the Mimic against the theme bosses at the
same scale. Pins moved honestly: boss count 25 → 26, lore count 28, the
audio file count 48 → 49.

## Compatibility

- Rules 19, generation 24, saves v1 — untouched.
- Content: one optional file added; one boss added; `CrystalForge
  --canonicalize` normalized the hand-added entries.
- Manifest v2, three new ids (`music.mock`, `boss.mimic.battle`,
  `prop.chest_battle`); the capture set grows to 134.

## Automated validation

See the completion report below.

## Manual owner checklist

Matrix rows **219–222**: the Jester's question (commands, the right/wrong
picks, the real KO, one turn, no bestiary entry, the jingle), no spoilers
(tier gating and the King gate, reload honesty), the chests (the reward
coin flip and the gear filter, the empty mock, the Mimic's reveal with the
opening blow and boss music), the Mimic's weight and payout. **Judge the
Jester's dryness, the mocking jingle, whether the punishment reads as fair,
and whether the Mimic fights like a floor boss.**

## Documentation updated

`docs/milestones.md` (rows + sections), `docs/game_design.md` (§6 the Lore
and Chests passages), `docs/technical_design.md` (§54),
`docs/manual_test_matrix.md` (rows 219–222), `docs/asset_pipeline.md`
(the audio count, the `music.mock` role and fallback),
`docs/editor_guide.md` (the category count), `assets/credits.md`, this
note.

## Completion report

### 1. Implementation summary

- **Milestone:** M112 — The Jester's Lore trap & the Treasure chests.
- **Slices completed:** content + schema + Forge; the pure models; the
  engine helpers; decision mode; the Mimic morph; the dungeon seams; the
  jingle; the art; captures; tests; docs. All completed.
- **Player-facing changes:** ~5 % of patrols are the Jester's question and
  ~15 % the three chests; the Mimic is a new boss; the mocking jingle is a
  new sound.
- **Engineering changes:** `game/SpecialEncounter.hpp` (new),
  `content::LoreQuestionDef` + loader + database + Forge category,
  `Battle::hostileTargetCount/spendMp/koUnit`, BattleState decision mode
  + `revealMimic`, `DungeonState::startSpecialBattle` + `special_`,
  `MusicTrack::Mock`, `BossDef.specialOnly` in the boss pools.

### 2. Files changed

- **Source:** `src/game/SpecialEncounter.hpp` (new),
  `src/content/{Definitions.hpp,ContentDatabase.hpp,ContentDatabase.cpp,
  ContentLoader.hpp,ContentLoader.cpp}`, `src/editor/{FieldDescriptor.hpp,
  CategoryDescriptors.cpp,CanonicalJson.cpp,EditorValidation.cpp}`,
  `src/audio/{AudioRoles.hpp,AudioManager.cpp}`,
  `src/battle/{Battle.hpp,Battle.cpp}`, `src/dungeon/DungeonGenerator.cpp`,
  `src/game/Castle.cpp`, `src/states/{BattleState.hpp,BattleState.cpp,
  DungeonState.hpp,DungeonState.cpp}`, `src/capture/CaptureRunner.cpp`.
- **Tests:** `tests/test_lore_encounter.cpp`, `tests/test_chest_encounter.cpp`
  (new), `tests/CMakeLists.txt`, `tests/{test_content_loader,test_audio}.cpp`
  (pins).
- **Content/data:** `data/lore_questions.json` (new), `data/bosses.json`
  (+1), `assets/manifest.json` (+3), `assets/credits.md` (+3),
  `assets/textures/enemies/boss_mimic.png`,
  `assets/textures/props/chest_battle.png`, `assets/audio/music/mock.wav`
  (new), `tools/asset_gen/{generate_textures.ps1,music_data.ps1}`.
- **Documentation:** as listed above.
- **Build/release configuration:** none.

### 3. Plan deviations

See "Deviations from the plan" — all routine.

### 4. Compatibility

See "Compatibility".

### 5. Automated validation

- **Build:** `cmake --build --preset debug` — clean, zero project-code
  warnings (VS 2022 developer shell).
- **Targeted tests:** `crystal_tests.exe "[lore],[chest],[content],[editor],
  [audio],[lint],[glyphs],[castle],[goose],[offense],[data],[battle],
  [patrol],[lifetime]"` — **300 cases, 115 097 assertions green**. Pins
  moved honestly: boss count 25 → 26, lore questions 28, shipped WAVs
  48 → 49.
- **Balance battery** (`crystal_tests.exe "[mimic-report]" -s`, 7 towns ×
  3 depths × 6 roll streams, the standard sim party, the Mimic at the
  shadow patrol's scale beside the town-1..3 theme bosses at the same
  scale): the party beat the Mimic in every fight in **2–3 rounds** and the
  theme bosses in every fight in **2–4 rounds** — the Mimic sits on the
  theme-boss line, neither trivial nor a superboss; the owner's own fights
  judge its weight (matrix row 222).
- **Content:** `CrystalForge --canonicalize` — 1 file rewritten (the
  hand-added Mimic normalized), 0 content errors before and after; the
  new `lore_questions.json` passes the glyph lint.
- **Art/audio:** `generate_textures.ps1` re-run — `git status` shows only
  `boss_mimic.png` and `chest_battle.png` new; `generate_audio.ps1` re-run —
  only `mock.wav` new (every earlier WAV byte-identical).
- **Capture lint:** `ArePGeese.exe --capture <dir>` — exit 0, **134/134
  scenes clean** (`131_lore_question`, `132_lore_result_mock`,
  `133_chests`, `134_mimic_revealed` new; the first pass found the answer
  box 16 px short of the 26-glyph worst case and the prompt strip over the
  party's first row — the box widened to 190 px and the strip moved to the
  free top band beside the turn counter).
- **Full suite / Release:** ride the program's closing battery.

### 6. Manual owner validation

Matrix rows 219–222.

### 7. Known limitations

- The Mimic battery compares win rates and rounds against theme bosses
  with the standard sim party; the owner's own fights judge its weight.
- The lore pool is 28 entries: a long run at town 7 repeats after 27 (28
  post-King) questions — by design (the cycle is seeded and complete).

### 8. Documentation updated

As listed above.

### 9. Final status

`implemented, awaiting manual approval`
