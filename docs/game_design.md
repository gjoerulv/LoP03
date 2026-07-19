# Crystal Dungeons — Game Design

> Living document. Update whenever player-facing design changes. Authoritative
> contract is `CLAUDE.md`; this explains *what the game is and why*.

## 1. Pitch

A 16-bit-inspired, turn-based **JRPG roguelite** about clearing seeded dungeons
**efficiently**. You take a party of 4 from a town hub into a procedurally
generated dungeon full of **visible** enemy teams and guarded chests, fight at
least 3 mandatory battles, beat a boss, and earn a score driven mainly by **how
few battle turns you spent**. Return to town, upgrade, go again — forever, via
seeds and scaling depth.

It is original. It evokes the clarity, side-view battles, readable windows, and
class identity of classic 16-bit JRPGs without copying any of them.

## 2. Audience & design north star

**Primary audience:** fans of 16-bit JRPGs and roguelites who want **real
tactical depth with a readable UI** — medium difficulty.

Implications used to settle ambiguous calls:

- Systems should be **legible**: the player can see danger, turn count, and
  trade-offs before committing.
- Difficulty is **fair**, not punishing-opaque and not hand-holding.
- Score-chasing (fewest turns) is the **rewarding core**, but a player who just
  wants to clear dungeons should still have fun.
- Session length per dungeon run: roughly **20–40 minutes**.

## 3. Core loop

Town hub → prepare party (heal, shop, equip, pick dungeon) → enter seeded dungeon
→ fight visible enemy teams guarding gates/chests → defeat boss → score on
battle-turn efficiency → return to town → upgrade/unlock → repeat.

## 4. Party & classes

Party of 4, chosen and renamable at new game from 6 classes:

1. **Knight** — durable physical attacker.
2. **Ranger** — fast, precision attacks.
3. **Mage** — elemental damage.
4. **Cleric** — healing / support.
5. **Rogue** — speed, item efficiency, crits.
6. **Guardian** — tank / protect / defensive buffs.

Characters gain XP/levels; player gains gold/items.

## 5. Town hub (functional, not huge)

Inn/heal · Item shop · Equipment shop · Guild/dungeon selection · Training
hall/class info · Score board · Save/load point · minimal NPC dialogue.

## 6. Dungeons

Deterministically generated from a **seed**, using a **room graph/grid** (not
cave noise). Each dungeon has: start room, boss room, main path, side rooms,
chests, **visible** enemy teams, **≥3 mandatory gate battles before the boss**,
optional guarded chests, and an exit/retreat option. **No random encounters.**

Visible enemy team shows: team name, danger level, enemy count, optional tags
(Fast, Magic, Armored, Poison, …).

**Rooms are compact and purposeful (M16),** drawn centered on the screen
rather than filling it. Each room is realized as one of eight archetypes
derived from its role in the graph — Entry, Corridor, Crossroads, Gate
Chamber, Treasure Alcove, Treasure Vault, Boss Antechamber, Boss Arena — with
bounded dimensions (common chambers ≈9–15 × 7–11 tiles, corridors narrow
along their travel axis, the boss arena largest), centered door gaps, sparse
landmark pillars that never block a path, and purposeful anchor placement
(chest opposite the entrance with its guard beside it; the boss at the arena
center). The same seed always reproduces the same rooms; room realization is
versioned (score entries record the generation version) so published seeds
stay comparable.

## 7. Danger rating (derived, never hand-authored)

Displayed danger is computed **deterministically from enemy stats and abilities**
(HP, Attack, Magic, Defense, Speed, skill threat, team synergy), may be compared
to a dungeon-depth baseline, and maps to tiers: **Trivial, Easy, Fair,
Dangerous, Deadly, Boss**. Formula is explicit and unit-tested (M6).

## 8. Combat

Classic **deterministic turn-based** (not ATB). Speed sets turn order. Party ≤4,
enemy team 1–5. Commands: **Attack, Skill/Magic, Item, Guard, Escape**. KO and
revive exist; game over if all party KO. **Every** encounter (incl. bosses) is
escapable. Escaping a normal battle forfeits that guarded chest/reward; escaping
the boss or leaving the dungeon gives **0 dungeon score**.

**Presentation (M18)** never changes results — it stages how the computed
outcome is shown: a brief lunge, an impact beat (hit flash, small shake,
damage numbers, HP bars updating), then the message. Because score play means
many repeated battles, pacing is a design rule: **Battle Speed** Normal is
brisk (~0.3s of staging per action), Fast halves it, Instant skips staging
outright, and Confirm always skips ahead. **Battle Flash** and **Battle
Shake** settings (full/reduced/off) gate the impact effects; flash is a
gentle brighten, never a strobe. Fallen enemies sink from the field; fallen
allies stay visible for revives; defeat states its consequences before
returning to town. Combat stays fully readable with audio muted.

## 9. Scoring

Ranking priority: (1) dungeon completed? → (2) fewest battle turns → (3) most
optional danger defeated → (4) most treasure → (5) fastest real time (final
tie-break only).

Score components: base completion, **battle-turn penalty**, escape penalty where
relevant, chest bonus, boss-defeat bonus, danger bonus, no-death bonus. Design
guard: **never** reward farming/stalling or actions that undercut the
fewest-turns premise.

## 10. Chests & rewards

Useful chests; most valuable are **guarded** (show enemy danger, chest rarity,
"fight to claim"); some minor chests unguarded. Rewards: consumables, equipment,
gold, relics/accessories, rare class skill scrolls.

## 11. Bosses

One boss per dungeon. Each: multiple actions, telegraph-style status text, ≥1
unique mechanic, escapable (but escaping fails the score). Archetypes:

1. **Brute** — high HP/attack.
2. **Sorcerer** — magic/status.
3. **Commander** — summons/minions/buffs.
4. **Enemy Rush** — continuous waves culminating in a boss (with optional
   minions).

## 12. Progression & failure

XP/levels, gold, shops/upgrades in town. Abandoned/failed dungeon → 0 dungeon
score. Successful escape → keep basic XP/gold but no dungeon score. Death →
return to town, 0 dungeon score, partial gold loss. Save/load required (JSON,
versioned). Any dungeon suspend-save (if added) continues the same run and must
not enable save-scumming.

## 13. First-complete-version content target

6 classes · 12–18 enemy types · 6 elites · 3 boss archetypes · 30+ items/equipment
· 8–12 skills/spells per broad category · 3 themes (**Ruined Keep, Crystal Mine,
Hollow Forest**) · infinite seeded dungeons with depth scaling.

## 14. Open design questions (decide with human when reached)

- Exact danger weights and tier thresholds (M6).
- Exact score coefficients and anti-farming caps (M6).
- Equipment/relic stat ranges and economy tuning (M7/M9).
- Suspend-save anti-scum mechanism, if implemented (M3+).
