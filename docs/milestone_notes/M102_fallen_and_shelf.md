# M102 — Rules: the fallen & the shelf (KO XP + the scroll ban)

**Status:** implemented, awaiting manual approval
**Program:** M98–M108 "Are P Geese" (owner-authorized 2026-08-16).
**Generation 17 → 18** (the scroll-free item pools change what seeds' chest
and merchant rolls produce; the scoreboard tags comparability as always).
NO battle-rules bump: the KO rule is post-battle spoils bookkeeping — the
deterministic fight, the Simulator, and every seeded outcome are untouched
(reasoning recorded here rather than in Battle.hpp's history, which tracks
sim rules only).

## Scope (owner items)

"KOd characters should not receive exp at the end of a fight. And should
not be revived automatically." · "Chests, events or merchants in dungeons
should not be able to drop/reward/sell skill scrolls of any kind" (the
M104 reels prize is the owner-sanctioned exception).

## What was built

- **The fallen earn nothing** ([Spoils.hpp](../../src/game/Spoils.hpp)
  `applySpoils`): only members standing at battle end receive the XP —
  covering every spoils-paying fight (normal, patrol, treasure guard). The
  victory panel stays honest for free: its LevelUpDiffs derive from the
  same before/after, so a fallen member never shows a diff. This also
  closed the "auto-revive" the owner reported: its mechanism was the
  level-up heal ([Party.cpp:206](../../src/game/Party.cpp)) standing a
  KO'd member back up when award-time XP leveled them; no XP, no level, no
  heal. Verified sanctioned revives remain: Phoenix Tear (usable from the
  field menu — `itemUseRefusal`'s "nobody fallen" path — and in battle),
  Renew (battle), the Inn (town). Castle fights route through the same
  spoils layer; Training Hall (`grantXp`, one member, paid) and the Elder
  Root (`grantPartyXp`, paid) are deliberately unfiltered — tuition is not
  a fight, and Root-leveling a fallen friend back to their feet is a
  priced, deliberate path (noted for the owner's judgment).
- **The shelf lost its scrolls**
  ([DungeonGenerator.cpp](../../src/dungeon/DungeonGenerator.cpp)
  `buildPools`): `ItemType::Scroll` is excluded from the dungeon item
  pool, which feeds BOTH chest rewards and the Duck Peddler's offer — the
  only two in-dungeon scroll sources found (verified: the armory ghost
  trades equipment/relics only, the reliquary drops relics, the surveyor
  sells maps, the trove is at the Guild, the digs are in town).
  Generation bumped 17 → 18 with the history comment.

## Verification

- New tests: the KO spoils case (no XP, no level, hp stays 0, panel shows
  only the living, gold unaffected) and a 60-seed sweep across all themes
  / towns / depths proving no chest or peddler item is ever a scroll
  (>50 rewards inspected). The v17 pin in test_danger moved to v18.
- Full suite + capture: green (recorded in the completion report).

## Deviations from the plan

None. The plan's open question (castle XP path) resolved: castle fights
pay through the same applySpoils layer, so the rule covers them with no
extra code.

## Manual owner checklist

Matrix rows **195–196**: KO a member, win, confirm no XP/level/revive for
them and an honest victory panel; sweep some chests/peddlers at depth and
meet no scrolls; confirm the trove and town digs still pay them.

## Documentation updated

game_design §8 (the fallen earn nothing) and §10 (chest rewards + the
three scroll sources), ledger row, matrix rows 195–196, this note.
