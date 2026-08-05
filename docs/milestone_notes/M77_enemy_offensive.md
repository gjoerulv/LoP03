# M77 — Enemy & boss offensive pass: the new statuses in enemy hands

Authorized 2026-08-05 as part of the M75–M86 expansion program (see the
program section in `docs/milestones.md`). Re-audit this note against the
then-current checkout before implementation begins.

## A. Status

☐ planned

## B. Goal (owner brief)

Content authoring on the v15 engine: enemies and bosses actually use
Reflect, Sleep, Curse, MP damage and the trigger system; the King and the
Deadly Duck get their reworks; lackluster boss/elite AI gets triggers; boss
introduction text stops coaching the player.

## C. Scope (planned)

### Roster-wide (`data/enemies.json`, `data/bosses.json`)

- A few enemy attacks also damage MP (`mpDamagePct: 25` — ¼ of dealt HP
  damage, the owner's ratio).
- A few enemies/bosses **start battle with Reflect** (`initialStatuses`).
- A few bosses **curse the party**; a few enemies/bosses can put the
  **whole party to sleep**.
- **Dark weaknesses** on a few enemies and exactly **one boss** (owner
  decision 2026-08-05, pairing with the Dark `shadow_strike`).
- Trigger authoring across the lackluster bosses, elites and boss minions —
  the owner's three examples (every 4th hit received → stun; first time at
  half HP → +25% ATK; first time at half HP → gain Reflect) plus a few
  creative deterministic ones, varied so bosses stay distinguishable.

### The King (castle)

- New abilities: **party-wide sleep**; **damage+stun** single target;
  **sleep+damage** single target.
- Targeting: the King does **not** attack sleeping party members unless a
  sleeper is the only target or the attack is multi-hit
  (`avoidSleepingTargets`).
- One minion **casts Reflect on the King** when that minion is at low HP
  (the `allyHpBelowPct`-style trigger); one minion **curses+damages** a
  single target.

### The Deadly Duck (Goose Town)

- Gains **Reflect**, a party-wide **stun+damage** (the Tax-Sheet effect
  turned against the party), and party-wide **sleep**.
- **Immune to the Deadly Spoon** (the M75 flag — his one former counterplay
  closes; the M76 kit replaces it, and the balance battery must prove the
  gauntlet still falls with the new counterplay).
- Will **not** use the stun while the entire party is sleeping.
- One goose can **curse the whole party**; one goose **starts with
  Reflect**.

### Presentation

- Telegraph/intro texts in `data/bosses.json` rewritten **flavor-only** —
  no tactical guidance ("silence it first", "guard the first strike" etc.
  all replaced with atmosphere in the same voice).

## D. Schema, save & version implications

- None — pure content on the v15 schema (fields already validated in M75).
- Balance is re-proven: castle/king/goose batteries updated for the new
  kits; the counterplay-assisted bar (the M44/M61 precedent) is the
  standard, with bare/kit win rates recorded.

## E. Out of scope

Guild Masters (M84) and the Dragon (M85) — they author against the same
vocabulary later. Player-side content (done in M76).

## F. Dependencies

M75 (engine), M76 (the party's counterplay must exist before the threats
ship — the "counterplay ships with the threat" bar).

## G. Acceptance criteria

- Every §C behavior observable in the sim and on screen, deterministic.
- King/Duck gauntlets remain beatable with obtainable counterplay (battery
  evidence with seeds recorded); no telegraph contains tactical advice.
- Pre-M77 dungeon fights against untouched enemies resolve identically.

## H. Automated validation

Battery updates ([castle-report], [king-report], [goose]); per-trigger
content tests (the King minion's reflect fires at the authored threshold;
the Duck refuses the Spoon; no stun while all sleep); telegraph lint for
banned coaching phrases if cheap. Full suite green.

## I. Owner manual validation

Fight the King and the Duck with the new kits: judge difficulty, fairness,
readability of sleep/curse/reflect in the flow, and that the de-hinted
intros still land. Spot-check two reworked dungeon bosses/elites.
