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

**Class learnsets (M29).** Each class begins with a small starting skill set and
**learns further skills at set levels** along a curve, so a character visibly
grows and its battle menu widens with progression. Known skills are *derived*
from the class and the character's level (not stored), so leveling mid-run
immediately unlocks the new options and no save is ever invalidated. By the
level cap each class commands roughly six to seven skills.

**Passive skills (M36).** Beyond skills, each character can carry a **passive** —
an always-on battle trait bought at the Training Hall for gold. The economy is
**own many, equip one**: purchased passives stay owned, and the single equipped
passive swaps freely, so a character is tuned per run without losing what it
bought. The ten passives are **Counter Attack** (retaliate after surviving a
physical hit), **Evasion** (25 % of physical attacks miss; a Blind attacker always
misses you), **Spell Ward** (25 % of hostile magic fizzles), **Thorns** (reflect
20 % of physical damage), **Lifedrink** (heal 15 % of physical damage dealt),
**Clarity** (+2 MP a round, immune to Silence; M43), **Iron Will** (survive a lethal
blow at 1 HP once a battle), **First Strike** (act first in round 1, +50 % on your
first hit), **Bodyguard** (soak 25 % of a hit on your weakest ally), and **Keen
Senses** (immune to Blind, +10 % vs debuffed foes). Enemies and bosses carry
passives too — the target-info panel reveals a foe's passive so you can play
around it. All effects are fully deterministic (chance-based ones seeded off the
battle, so play and the simulator agree exactly).

## 5. Town hub (functional, not huge)

Inn/rest · Item shop · Equipment shop · Guild/dungeon selection · Training
hall/class info · Score board · Save/load point · minimal NPC dialogue.

**The inn is a paid rest (M30).** A full HP/MP restore costs gold that scales
with the highest party level (so it stays a real decision as income grows), or
is free by spending a **rest token** earned from a dungeon event. The inn shows
the cost and tokens held; declining is **never a soft-lock** — HP/MP persist,
battles still pay gold, and the token is a relief valve, so a wounded, broke
party can always earn its way back to health.

**The town ladder (M32; travel reworked M50).** Town is not a single place but a
chain of **seven** towns, each harder and higher-scoring. The town is a compact,
centred layout whose **edges are walk-through roads** (M50): walk into the **east**
road for the next town, the **west** road for the previous one — no button press,
and arriving drops you at the matching edge so travel reads as one continuous
walk. Travel between reached towns is free and instant (fade + a darker, more
sinister music track per town). A
higher town multiplies every enemy team's stats — **+0/25/50/75/100/150/200 %**
across towns 1–7 (composed on top of depth scaling; danger labels rise
honestly) — and grants a **visible score bonus** on the whole run subtotal —
**+0/10/20/30/40/50/100 %**. XP and gold rewards do **not** scale with town, so
the top town is never the best farm; the climb pays in score. **Per-town gear
(M37):** each town from 2 to 7 unlocks its own equipment at the Equip Shop
(stronger weapons/armor/accessories, priced to gate — sim-validated affordable
against depth-driven income, and kept below the legendary tier) and adds pieces
to the dungeon chest pools, so climbing the ladder opens a real gear progression
to match the rising difficulty. **Per-town foes (M38):** each town from 2 to 7
also introduces new standard enemies (≥ 2 each) and a new boss, each with its own
sprite, behaviour, and status/passive kit, gated so they appear only once their
town is reached — so the roster deepens as you climb, not just its numbers.
Access is earned:
completing at least one dungeon in a town unlocks the road to the next, and the
furthest reached town persists in the save. Old saves start in town 1. Each town
has its own exterior and service-interior art and its own theme track; the three
dungeon themes are unchanged (towns scale difficulty, they do not retheme
dungeons).

## 6. Dungeons

Deterministically generated from a **seed**, using a **room graph/grid** (not
cave noise). Each dungeon has: start room, boss room, main path, side rooms,
chests, **visible** enemy teams, **≥3 mandatory gate battles before the boss**,
optional guarded chests, and an exit/retreat option. **No random encounters.**

Visible enemy team shows: team name, danger level, enemy count, optional tags
(Fast, Magic, Armored, Poison, …).

**Encounters are composed, not rolled blind (M20).** Every enemy has a
tactical **role** — bruiser, sniper, healer, buffer, protector, attrition,
disruptor — and authored composition rules (`data/composition.json`) that
generation cannot violate: team size and elite share grow with depth, at
most one support enemy per normal team, at least one damage role, bounded
boss minions. Past depth 5 enemies also grow stronger (+6% stats per depth,
capped +90%) so deep dungeons keep escalating; danger labels are computed
from the scaled stats, so they never lie.

**Room events (M20, +M30).** Dead-end event rooms offer real decisions whose
full trade-off is shown **before** confirming: a shrine (gold for healing), a
one-use healing spring, a wandering merchant (one item at a **bargain — 75 % of
value** (M37), a reward for exploring, not a tax), an elite challenge (double
danger score, no treasure), an omen's score
wager (+150 if you finish with no deaths, −100 if anyone falls — shown in
the score breakdown), and a **rest camp (M30)** that grants a free-rest token
redeemable at the inn. Some unguarded chests are visibly **trapped**: extra
gold, but claiming wounds the whole party.

**Bosses are mechanically distinct (M20).** Each archetype has one
deterministic mechanic, stated in its telegraph: the Brute's damage swells
below half HP (announced when it triggers), the Sorcerer's magic grows as
its minions fall, the Commander rallies its fallen minions once below half
HP, and the Rush tyrant's opening blow lands with doubled force.

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

**Status effects (M35, extends M7).** Beyond poison and the attack/defense
buffs/debuffs, three afflictions deepen the tactics. **Blind** makes a unit's
physical attacks usually miss (the game's first to-hit roll — magic and items are
unaffected). **Silence** blocks MP-cost skills (basic attacks and items still
work). **Confusion** takes over a unit's turn entirely: it acts automatically —
**no player input for a confused character** — lashing out with a basic attack at
a seeded random member of its own side. Since **M43** this binds **both sides
equally**: a confused enemy can no longer heal, buff, or curse its way through the
affliction, because the forced basic attack is enforced in the shared rules every
turn-decider uses. A confused unit is **snapped out of it the
moment it takes damage**, so hitting a confused ally (or letting a foe strike your
confused member) is a real counter. All three are fully deterministic (seeded off the
battle, so live play and the simulator agree exactly), applied by class skills (a
Rogue and Ranger blind, a Mage silence and confusion) and enemy kits (disruptors,
sorcerers, and several bosses now weave them in where the role fits), and all are
lifted by a **Remedy** item or the Cleric's **Purify** (which since M43 is a pure
cure — it lifts everything and heals nothing). A miss reads as "Miss!"
and every affliction shows a labelled icon, so nothing depends on colour alone.
Afflictions and buffs both **linger** (durations run long), and **poison bites
hard** (it deals heavy damage each turn), so status play — inflicting, curing,
and outlasting — is a meaningful axis of a fight.

**Enmity & enemy targeting (M28).** Enemies no longer always pile onto the
lowest-HP party member (which perversely made an efficiently-played mage the
tank). Each party member accrues **threat** from the damage and healing they do;
threat decays each round. Every enemy has a **targeting profile derived from its
role** — *aggressive* chases threat, *opportunist* goes for the kill (low HP),
*tactician* hunts the backline caster, *protector* peels for whoever most
threatens its allies, *spread* prefers the healthiest for damage-over-time —
plus a small **seeded tie-break** so targeting is readable but not perfectly
predictable (fully deterministic and reproducible per encounter; the simulator
and live play agree exactly). Players influence aggro with three **control
skills**: **Taunt** (Guardian) forces foes onto the caster, **Fade** (Mage)
sheds the caster's threat, and **Redirect** (Knight) makes the caster take
single hits aimed at allies until its next turn. Because this changes how every
battle resolves, scoreboard entries carry a **battle-rules version** and pre-M28
runs are flagged as played under older rules (never silently ranked as equal).

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

**Score comparability (M19, owner-approved):** runs are never silently
normalized. Every new score entry is tagged with its run conditions — depth,
theme, seed, generation version, and **party level** (highest member at
completion) — and the scoreboard shows Depth and Lv per row with a plain
legend: *compare runs at the same Depth and Lv*. Older entries without the
tag show "-". Farming power before a run is legal; it is simply visible.
The score formula itself ignores party level.

Ranking priority: (1) dungeon completed? → (2) fewest battle turns → (3) most
optional danger defeated → (4) most treasure → (5) fastest real time (final
tie-break only).

Score components: base completion, **battle-turn penalty**, escape penalty where
relevant, chest bonus, boss-defeat bonus, danger bonus, no-death bonus, and — as
percentages of the whole subtotal — the **town-ladder bonus** (M32, +10 % per
town above town 1, up to +100 %) and the **stakes penalty** (M33, subtracted).
The town a run was cleared in is recorded as an optional `townIndex` tag and
shown on the scoreboard as "T#" (a comparability tag like depth and level, never
used for ranking); *compare runs at the same Town, Depth and Lv*. Design guard:
**never** reward farming/stalling or actions that undercut the fewest-turns
premise.

**Stakes escalation (M33).** A run's *stakes* is its `(town, depth)`, compared
town-first against your **previous completed run**. If a completed run does **not**
raise the stakes above the last one, it loses score — **−30 % per repeat,
stacking to a −99 % floor** (M35 re-tune; was −15 %/−90 %) — and each further
non-raising run deepens it.
Clearing a run that **does** raise the stakes (a higher town, or a deeper dungeon
at the same town) **resets the penalty to zero**. Retreats and wipes (score-0
runs) don't count either way. The Guild **forewarns** the penalty the configured
run will incur, updating live as you change Depth ("Stakes penalty: −30 % — raise
town or depth to clear"), the result screen itemizes it, and the penalty state is
saved and travels with the entry autosave, so reloading can't shed it. This
pushes the player to keep climbing rather than farm one comfortable stake.

## 10. Chests & rewards

Useful chests; most valuable are **guarded** (show enemy danger, chest rarity,
"fight to claim"); some minor chests unguarded. Rewards: consumables, equipment,
gold, relics/accessories, rare class skill scrolls.

**Black market & legendary gear (M34).** Optional **elite challenges** in
dungeons (a room event) pay, on victory, **one legendary token** on top of their
double danger score. After a **stakes-raising** completed run in **town 2 or
higher**, there is a **20 % chance** (seeded from the run, so a reload can't
reroll it) that a hooded **black-market dealer** appears in that town, offering a
single **legendary** piece — gear a clear step above Epic, sold **nowhere else**
— for **5000+ gold or 3 legendary tokens**. Declining costs nothing and the
offer waits until bought; a later stakes-raising hit replaces it. Legendaries are
aspirational endgame gear (sim-validated not to trivialize the top town), the
prize for climbing and taking optional risks.

**High-stakes market path (M52).** On top of that 20 % rule, **any** beaten
dungeon boss at **town 7, depth 20 or deeper** rolls a **second, independent
34 % chance** of the same dealer appearing — regardless of score, stakes, or a
penalty that floors the run's score to 0 (retreats and defeats never count). It
rides its own fresh seeded stream (a run can win one roll and lose the other),
so it is reload-proof the same way, and it simply gives the deepest top-end runs
a reliably better shot at a legendary. The 20 % rule is unchanged.

**Boss legendary & token drops (M39).** Beating a dungeon boss in **town 3 or
higher** and at **depth 4 or deeper** rolls two independent rewards, seeded from
the run (so a reload can't reroll them): a chance of **legendary tokens** and a
separate chance of a **legendary equipment** piece (from the same pool the black
market sells). Both chances **ramp with town and depth** — from a low floor at
town 3 / depth 4 up to the caps at **town 7 / depth 20: 75 % for tokens, 30 % for
a legendary**; a token drop in **town 7 pays 2 tokens**. Drops are shown on the
result screen. The rates are tuned (sim-tabulated) to reward deep, high-town
clears without replacing the black market: one top clear's expected token drip is
worth less than a market purchase, and most top clears still drop no legendary, so
scarcity holds. Boss drops never affect the score (they are a post-battle reward).

**The castle & the King (M40).** Clearing any **town-7 dungeon** opens a road that
climbs from town 7 up to the **castle** — a distinct place above the seven-town
ladder (not a town: no shops, dungeons, stakes, or score bonus). The castle hosts
an inn, a save point, the party's **castle records**, and the **King's three
challenges**, each a step above normal play and each paying a **one-time first-clear
reward**:

- **Boss Rush** — the full 12-boss roster, back to back, **each with its own
  minions** (M49), with **no free healing between fights** (items and skills
  still work). Record: fewest total turns.
- **Endless Rush** — deterministic escalating waves; survive as long as you can, no
  free healing. Record: best wave reached.
- **The Hollow King** — the hardest fight in the game. A bespoke boss above every
  town-7 foe, immune to Blind, Silence, **and** Confusion, striking your afflicted
  party harder as the fight wears on, with a kit that inflicts every status —
  and, since M49, **two Royal Guards he keeps calling back**. Beating
  him the first time grants a **unique legendary** (the Sovereign's Regalia, won
  nowhere else), a **visible title**, gold, and tokens.

Castle records live **entirely apart from the dungeon scoreboard** (score
comparability is preserved); challenges never touch your dungeon score, stakes, or
the scoreboard. Travel back to town is free.

**Story & lore (M41).** A light-hearted running serial threads the climb: a
**wandering storyteller** stands in every town and, town by town, spins the
increasingly absurd "Ballad of the Hollow King" — one verse per town, growing
taller with each telling. Hear all seven verses and the **Jester** at the castle
delivers the punchline: the dread King is, in truth, a very tired, very bored,
goose-fearing man who mostly wants a nap and a worthy challenger. The serial is
optional flavor (never gates progress or combat); it simply rewards curiosity and
gives the King a wink. Reading is remembered across saves; the Jester's finale
unlocks once all seven verses are heard. All writing is original.

**Enrichment: bestiary, victory stats, achievements (M42).** Three optional
presentation features round out the endgame. A **Bestiary** (from the town pause
menu) is a codex of the whole roster: a foe you have fought shows its sprite,
stats, behaviour profile (role/tier or boss archetype), tags, passives (one per
line), and — for bosses — its flavor text; a foe you have not met yet reads as an
unknown, so the roster's size and your progress through it are always visible.
Each known foe also shows a `max` line beneath its base stats (M52): its stats
**at their strongest real fight context**, so a codex entry conveys not just what
a foe is worth on paper but how dangerous it becomes at the top of the game. The
context is the hardest place the foe is actually fought — regular enemies (and
elites) at the town-7 dungeon ceiling (×5.70), the Royal Guards and the King at
his throne-room scale (×5.00), and every other boss at the Boss Rush (×5.80). The
**Endless Rush is deliberately excluded**: its scale climbs without bound, so no
single "strongest" number would be honest.
**Victory stats** appear on the clear screen's Run-stats view: this run's
total damage, biggest single hit, statuses inflicted, and the party MVP, plus your
personal records (biggest hit ever, most damage in a run) — display-only, never
ranked. **Achievements** (also from the pause menu) are ~16 original cross-game
goals — clearing dungeons, climbing the ladder, beating the King's challenges,
hearing the whole story, and more — persisted globally, each with a single toast
when it unlocks. None of the three touch battle, generation, or scoring.

**Balance pass (M43).** The endgame arc opens by making consumables a real budget
decision and healing a real choice, at one battle-rules bump (v4).

- **The repriced shelf:** Remedy **100 g** (was 15), Ether **150 g** (was 60),
  Hi-Ether **500 g** (was 150), Phoenix Tear **300 g** (was 150) — and a Phoenix
  Tear now revives at **25 %** HP, not 50 %. Potions are untouched, so HP stays
  cheap while MP, cures, and resurrection cost what they are worth. One clear's
  gold buys a handful of Remedies or a single Tear, so the shopping list is a
  decision rather than a formality.
- **Purify** lifts the party's afflictions and **heals nothing** (M47 narrowed
  its scope further — see below); **Renew** becomes the emergency button — a weak
  heal that can also **raise a fallen ally at 20 % HP**, the first skill in the
  game that can.
- **Battle items now respect the state of their target:** potions, ethers, and
  cures reach only living allies, and a revive reaches only the fallen. An item
  with nobody to use it on is greyed out with the reason, so nothing is ever spent
  on "No effect".
- **Royal Snacks (250 g)** — "Bring Snacks!" — are sold **only in the first
  town**. Ordinarily they are a joke: 10 HP and a shrug that lifts ATK-/DEF-. In
  **the King's fight** they are 100 HP and 10 MP. The King's counterplay starts
  where the game did.
- **Clarity** gives **+2 MP** a round (was +3).
- **Losing a castle challenge takes no gold:** the message used to claim the
  dungeon's gold penalty, and no longer does. (M43 also fully healed the party at
  the gates; **M47 removed that free heal** — see the castle-stakes section
  below.)

**Royal Relics & the doubled King (M44).** The King stops being a wall of stats
you out-level and becomes a fight you answer with absurd objects.

- **The reliquary** is a rare room event that *replaces* an ordinary rolled event
  from **town 2, depth 2** onward: **3 %** in town 2, **5 %** in towns 3–6, **7 %**
  in town 7, **+5 %pts from depth 20 up**, and never more than one per dungeon.
  It costs nothing and asks nothing — it simply hands over one relic.
- **Which relic** is a seeded 40 / 40 / 15 / 5 draw over Evil Goose, Tax Sheets,
  Dragon Crown, Deadly Spoon, with anything you already hold excluded and the rest
  renormalized; hold all four and the plain draw returns, giving a duplicate. The
  draw is fixed by the run's seed and the room, so reloading cannot fish for the
  Spoon.
- **The four relics** are single-use, aimed at an **enemy**, and sold nowhere:
  - **Evil Goose** — "A terrifying goose." The target can only **Guard** next turn.
  - **Tax Sheets** — "Busy your enemies with taxes." The target **loses** its next turn.
  - **Dragon Crown** — "The real Dragon Crown." Saps the **Hollow King's** attack
    and defense. Anyone else shrugs — and you keep the crown. **Hidden effect
    (M52):** used on the King, it *also* permanently ends his revive clock — his
    fallen Royal Guards never return for the rest of the fight. This is a
    **deliberately hidden** interaction: the game shows no text for it (he simply
    stops calling them back), so it stays discoverable-but-unexplained. Recorded
    here because the design docs are the truth; the game keeps the secret. It is
    schema-driven (an optional `disablesMinionRevive` item flag, no item id is
    special-cased) and lives in shared battle code, so simulation and live play
    resolve it identically — the one battle-rules bump this milestone makes
    (rules 9 → 10).
  - **Deadly Spoon** — "Most deadly thing known to man." **Halves** the target's
    ATK/MAG/DEF/SPD for the rest of the battle. The rarest, and it shows.
  The King is **not** immune to any of them: that is the whole point.
- **The King doubles.** 750 HP (was 560) and doubled ATK/MAG/DEF/SPD
  (36 / 44 / 36 / 26). A party carrying no relics and no snacks **loses**, and
  relics buy survivors — the fight is a puzzle, not a farming toll. (The
  challenge multiplier and the counterplay bar have been retuned twice since
  this milestone; the current fight — his Royal Guards, the revive clock, the
  ×5.00 castle-floor scale, and the level-99 three-relic bar — is described
  under **The King's Court** and **The castle floor** below, which supersede
  the numbers this paragraph originally carried.)

**The King's classes (M45).** Beating the Hollow King unlocks three more classes
**for the player, not for that save** — they are offered on every future New
Game, and a save that already beat him unlocks them retroactively. Until then
they are listed at character creation as **Locked**, so the reward is visible
long before it is earned. All three are jokes that are also real classes:

- **Dragon** — enormous stats, **no skills**, **no armor**. Its basic attack hits
  **every** living foe and leaves poison and blindness on each one it connects
  with. Its score modifier is **−20 % per Dragon**: overwhelming force is not
  efficiency, and the scoreboard says so.
- **Jester** — **uncontrolled**. You do not choose its turns: each round it picks
  one of its own skills (any it can afford and cast) or a swing, at a foe of its
  choosing. It carries no weapon, quips one of twelve dry lines about 15 % of the
  time, and pays **+5 % score per Jester** for the indignity.
- **Goose** — dreadful stats, **equips nothing at all**. Its heals and cures work
  — and cheerfully buff **every enemy** at the same time. At level 30 it learns
  one ultimate that lays every debuff on every foe for 30 MP. **+5 % per Goose.**

The class modifiers are **additive across the party** (three Dragons and a Goose
= −55 %), shown as their own line on the result screen and tagged on the
scoreboard. Per M19 they are visible and tagged, never normalized away, and they
never change how runs are ranked.

**Rules & flow pass (M47).** Four small changes that sharpen the endgame and the
edges of the interface, at one battle-rules bump (v7).

- **Purify cleanses afflictions, not weakness.** A `cleanse` skill now lifts
  **Poison, Blind, Silence and Confusion** only; **ATK- and DEF- survive it**, as
  do the Royal Relics' Terrified/Stunned. Cure **items** (a Remedy) are unchanged
  and still lift everything, and Royal Snacks still lift the stat debuffs alone —
  so each answer to a bad status now has a distinct shape, and a Cleric can no
  longer erase a boss's whole debuff kit for 10 MP. (The same narrowing applies to
  the Goose's Generous Mending, the only other skill that cleanses.) Purify still
  heals nothing.
- **Losing at the castle costs something.** A failed — or fled — challenge no
  longer heals the party at the gates. Everyone still standing is carried out at
  **1 HP**, the **fallen stay fallen**, and **MP is untouched**; a total wipe
  leaves **exactly one member** on their feet so an Inn is always reachable. Still
  **no gold penalty** and still no run to forfeit: the King takes your strength,
  not your purse. Fleeing keeps whatever HP/MP the fight ended with, minus the
  heal that used to follow. Dungeon defeat is unchanged (half your gold, full
  heal).
- **Pause closes with the same key it opens with.** Tab / Start now closes the
  town and dungeon pause menus, alongside Cancel.
- **You can quit the game from inside the game.** Both pause menus offer **Quit**
  → **Quit to Title / Quit Game / Keep Playing**, with the cursor on Keep Playing
  and the warning stating what that screen actually loses (in a dungeon: the run
  goes, the entry autosave stays).

**Elements (M48).** A thin, sparse affinity layer over the existing damage
formulas, at one battle-rules bump (v8). Some foes have an **elemental weakness**
(that element deals **×150 %**) or an **immunity** (that element deals **nothing
at all** — no damage and no status rider; an immune blow lands and accomplishes
nothing, which the log and the float both say outright, and which never reads as
a miss). Nothing is ever both.

Elements reach a foe two ways: a **skill's own element** (the shipped fire, ice,
lightning, earth and holy spells were always authored with one — until now it
did nothing), and a **weapon's element**, which every basic attack of its wielder
carries. Five weapons are elemental, spanning the ladder from the town-1 **Holy
Mace** to the legendary **Dawnforged Blade**, so the system is met early and
still matters late. Enemies carry no weapons, so their own basic attacks stay
unelemented, and the party has no affinities — the layer only ever describes
foes.

**Affinities are shown, never guessed at.** A foe the party has fought lists
`Weak: Fire` / `Immune: Ice` in the **bestiary**; the **battle target panel**
shows the same as chips while you aim at it; and a resolved hit floats **"Weak!"**
(gold) or **"Immune"** (coral) beside the number. Every one is shape *and* text,
never color alone.

The curation is deliberately small — 8 enemies, 3 bosses, 5 weapons — and follows
one hard rule: **no weapon element is ever an immunity anywhere in the game.** A
skill can be swapped for another, but a basic attack cannot, and one class (the
Dragon) has nothing else — so a wielded element that could be nullified would be
a trap rather than a decision. Immunities use ice, earth and lightning; weapons
use fire and holy. Dark stays reserved for later content.

**The King's Court (M49).** The Hollow King stops fighting alone. Two **Royal
Guards** — the **Throne Blade** and the **Throne Stave** — stand with him:
armoured, slow, magic-heavy, and built to last rather than to burst. They carry
**iron will**, **evasion** and a **spell ward** between them, and their support
skills brace the *whole court*, so killing the King through a wall of buffs is
the real problem. Each has one elemental weakness (lightning and earth) and no
immunity, so the M48 tools answer them.

And they do not stay down. On the **fifth of the King's own turns with both
guards fallen**, he calls them back at full health, unmarked — **every time the
condition recurs**, for as long as the fight lasts. The clock resets the moment
one of them is standing. Cutting the escort down is therefore a tempo decision,
not a checklist: you either kill the King inside the window you bought, or you
pay for the court twice.

**The castle floor (owner decision, 2026-07-23).** The castle is the place
*above* the ladder, and its numbers now say so: **every challenge starts above
the strongest multiplier a dungeon can produce** (town 7 at the depth cap,
×5.70). Previously the Boss Rush (×2.60) and even the King (×3.10) sat *below* a
deep town-7 dungeon boss, which made the throne room a smaller number than the
place you climbed out of. The new floor is a derived invariant, not a literal:
it is computed from the same town-ladder and depth-curve rules the generator
uses, so it moves if they do.

- **Boss Rush ×5.80**, twelve bosses with their minions and no free healing —
  above the ladder ceiling on the multiplier alone.
- **Endless Rush from ×5.00**, climbing ×0.10 per wave, passing the ladder
  ceiling within a handful of waves and never stopping.
- **The Hollow King ×5.00** — a lower percentage than the rush, and still by far
  the largest single fight in the game: his base stats are so far above any
  dungeon boss that ×5.00 makes him a **3750 HP** opponent where the deepest
  dungeon boss the ladder can produce is 2280. Against a **level-99** party (the
  cap, raised from 50 so the endgame has an answer) the simulation beats him with
  three Tax Sheets, three Evil Geese and a bag of Royal Snacks — three survivors
  at a third health — and comfortably with the Dragon Crown and Deadly Spoon on
  top. He rewards a full relic haul without demanding the absolute maximum, and a
  party carrying nothing still loses.

Because a multiplier alone is misleading once base stats differ this much, the
"castle outranks the ladder" rule is enforced in **effective stats**, not in
percent.

This deliberately supersedes the earlier approach of tuning each challenge down
to whatever the headless simulator could beat. The castle is meant to be very
hard; the simulator's scripted party is a floor on player skill, not a ceiling.

**The Boss Rush grows a court too (M49).** Every boss in the gauntlet now brings
the **same minions it brings in a dungeon**, so the rush finally tests the fights
the game actually taught — and, under the castle floor above, each of them is
scaled beyond the deepest dungeon boss as well.

## 11. Bosses

One boss per dungeon. Each: multiple actions, telegraph-style status text, ≥1
unique mechanic, escapable (but escaping fails the score). Archetypes:

1. **Brute** — high HP/attack.
2. **Sorcerer** — magic/status.
3. **Commander** — summons/minions/buffs.
4. **Enemy Rush** — continuous waves culminating in a boss (with optional
   minions).

## 12. Progression & failure

XP/levels (cap **99**, raised from 50 in 2026-07-23 so a fully-levelled party can
answer the raised castle challenges), gold, shops/upgrades in town.
Abandoned/failed dungeon → 0 dungeon
score. Successful escape → keep basic XP/gold but no dungeon score. Death →
return to town, 0 dungeon score, partial gold loss. **Recovery is a paid loop
(M30):** healing costs gold at the inn (or a free-rest token), so gold now has a
recovery sink and attrition matters — but battles pay gold even without resting,
so a broke party can always earn its way back and is never soft-locked.
Save/load required (JSON, versioned). Any dungeon suspend-save (if added)
continues the same run and must not enable save-scumming.

## 12b. Onboarding & accessibility (M22)

**Onboarding teaches through play, never through a forced sequence.** Nine
one-time contextual prompts (`src/tutorial/`) fire on first encounter —
town, guild, dungeon, battle, guarded chest, event, first victory, first
result, first return to town — each a small dismissible panel; once seen,
never again (progress persists in `tutorial.json`; disable-all and reset
live in Settings). The **Details** action (remappable; C / gamepad Y)
opens read-only contextual help wherever the footer offers it: unit stats
and status shorthand in battle, danger-tier derivation in the dungeon,
score components on the result and scoreboard, and per-member gear
comparison in the equip shop. Nothing behind Details is required to play.

**Accessibility commitments:** keyboard-only and gamepad-only complete;
no color-only or sound-only information anywhere; a High Contrast palette
toggle; battle flash/shake reducible to off; message pacing and battle
speed configurable; Settings reachable before starting a game; destructive
actions need explicit confirmation — a save overwrite takes a second Confirm on
the same slot, and quitting asks its question outright (since M47 with three
answers: to the title, to the desktop, or keep playing), with the cursor starting
on the safe answer and Cancel resolving to it.
These are engineering bars, not formal WCAG claims.

**Presentation & options (M51).** Five closing polish features, none touching
gameplay:
- The **title screen** greets you with one of a dozen original dry-comedy
  one-liners (chosen per visit, pulsing gently) — never a description of the
  genre. "Geese and Dragons; Spoons and Snacks!" is in the pool.
- **Settings are organized into submenus** — Audio / Display / Gameplay /
  Controls, plus Reset — so the option list is no longer one long scroll. Cancel
  steps back one level, then saves and closes.
- A subtle **CRT effect** (Display → CRT Effect, **Off by default**) adds faint
  scanlines and a light mask with **no curvature**, so the pixels stay crisp.
- **Losing window focus now mutes the audio by default**; a new **Background
  Audio** toggle (Audio submenu) keeps it playing. (A deliberate change — the
  game used to always play.)
- An **all-target action** (a mass spell, the Goose's ultimate, the Dragon's
  sweep) briefly **tints the whole screen** during its impact — heal green,
  damage coral, debuff violet — a single faint pulse, gated by the Battle Flash
  setting, never a strobe.

**Comforts & secrets (M52).** Six small quality-of-life additions and one
secret, none touching the core loop:
- **Ambience has its own volume slider** (Audio submenu), no longer chained to
  the SFX slider as it was since M27. It **defaults to 5/10** — quieter by
  design — and applies live and at startup. Old settings files (with no ambience
  field) load at 0.5.
- An **in-battle battle log**: the **Menu/Pause** action opens a scrollable
  overlay of the **last 30 action results** (the exact lines the battle showed);
  the same action, or Cancel, closes it. It is presentation-only — a mid-fight
  memory aid that never affects how the battle resolves and is gone when the
  battle ends.
- The **Equip Shop** now shows the **owned count** beside each buy price, and
  the equip flow shows the **currently equipped item** in the chosen slot plus
  the **stat difference** of the highlighted candidate (green for a net gain,
  coral for a loss), so gear decisions read at a glance.
- Bestiary **max stats** (see §10), the **Dragon Crown's hidden effect** against
  the King's revive clock (see §10, Royal Relics — a deliberate secret), and the
  **high-stakes black-market path** (see §10, Black market) round out the
  milestone.

## 13. First-complete-version content target

6 classes (each with a level-based learnset) · 26 normal enemy types · 17 elites ·
12 bosses across 4 archetypes · 50+ items/equipment · 48 skills/spells across the
broad categories · **10 passive skills** · 3 themes (**Ruined Keep, Crystal Mine,
Hollow Forest**) · infinite seeded dungeons with depth and town scaling. (Counts
as of M38, which added 12 per-town enemies and 6 per-town bosses; the roster and
lists grow with content milestones.)

## 14. Open design questions (decide with human when reached)

- Exact danger weights and tier thresholds (M6).
- Exact score coefficients and anti-farming caps (M6).
- Equipment/relic stat ranges and economy tuning (M7/M9).
- Suspend-save anti-scum mechanism, if implemented (M3+).
