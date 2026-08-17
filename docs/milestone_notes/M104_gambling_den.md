# M104 — The gambling den: reels & blackjack

**Status:** implemented, awaiting manual approval
**Program:** M98–M108 "Are P Geese" (owner-authorized 2026-08-16).
**Generation 19 → 20** (two more pure-hash event replacements). No
battle-rules or save change; all randomness is hashed from (dungeon seed,
room, draw index), so reloads replay the same reels and the same shoe —
save-scumming buys nothing (the entry-only autosave already made mid-run
scumming impossible; this makes even that pointless).

## Scope (owner events 2 and 3 + interview rulings)

The one-shot reels (1 spin 10 g / 3 spins 70 g — prices verbatim, the
rip-off bundle IS the joke; event disappears after playing) with the
owner's seven symbols and prize table (crown = the Dragon Crown), and
blackjack (bet gold, win pays the bet back doubled).

## What was built

- **Pure rules** ([game/Gamble.hpp](../../src/game/Gamble.hpp)): weighted
  reel symbols (weights 22/18/14/14/14/10/8, sum-pinned) with a 35 %
  copy-reel bias lifting three-of-a-kind to roughly one spin in five
  (pure independence would land near 2 % — a one-shot event needs real
  odds); a seeded rank-only card stream (the bitmap font has no suit
  glyphs, and suits carry no blackjack value) and soft-ace hand values.
  All owner-tunable constants; every outcome headless-tested.
- **The Reels** (`RoomEventKind::Reels`, 7 %/dungeon): buy-in via the
  M103 choice modal, spins resolved on the outcome panel — each spin's
  three symbols named, matches paying per the owner's table:
  tax = pay min(100, gold) · goose head = one dry goose joke + equipment
  **new at this town** (minTown == town, never legendary; town-1
  fallback: anything stocked) · P-Spoon = Deadly Spoon (capFor honored;
  at cap "the machine keeps it") · crown = **Dragon Crown** (same cap
  rule) · red X = a map piece through the shared M83 grant · bald head =
  one scroll from the trove's normal pool (**the one sanctioned
  in-dungeon scroll source**, per the interview) · 7 = 1000 g. Gamble
  gold is plain gold, never score treasure. One play, win or lose.
- **Blackjack** (`RoomEventKind::Blackjack`, 7 %/dungeon): bet from the
  affordable steps (10/25/50/100), then an interactive hand in
  [BlackjackEventState](../../src/states/BlackjackEventState.hpp) —
  Confirm hits, Cancel stands, dealer draws to 17, win pays the bet back
  doubled, push returns it. One round per den. Minimal fair rules
  (documented assumption: no splits/doubles/insurance).

## Verification

- New tests: reel determinism + weight sum + match-rate band (8–35 % over
  700 seeds) + full symbol coverage; card-stream range/replay + soft-ace
  values; den placement (400 seeds, ≤1 each, both appear). Pins moved
  deliberately: flavor vocabulary 19 → 21, generation 19 → 20, and the
  M20-era "9 kinds" sample pin recounted (15 land in its fixed sample;
  full coverage rides the 400-seed sweeps).
- Build clean; canonicalize 0 rewrites; full suite + capture recorded in
  the completion report.

## Deviations from the plan

- The reels resolve TEXTUALLY on the outcome panel (each spin's symbols
  named) — no bespoke icon sprites or spin animation. The plan's icon
  ambition is deferred to the owner's judgment: if wanted, it belongs
  with M106's art wave. Flagged for the manual pass.
- The plan reserved "reels is one-shot" pricing questions — the owner's
  interview answer (verbatim prices, one play) is implemented exactly.

## Manual owner checklist

Matrix row **200**: meet both dens; spin once and thrice; hit a match
(debug gold + seed sweeps help); verify every prize row incl. the at-cap
spoon/crown refusals and the tax clamp; play hands to a win, a loss, a
push, a dealt 21; confirm both dens vanish after one play/round; judge
whether text-only reels need M106 icon art.

## Documentation updated

game_design §6 (the dens paragraph), ledger row, matrix row 200, this
note.
