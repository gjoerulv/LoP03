# M88 — Town, Guild & dungeon UX fixes

**Status:** implemented, awaiting manual approval
**Program:** M88–M97 (owner-authorized 2026-08-14; one plan, one
authorization; plan file with the exploration record and owner Q&A decisions
lives outside the repo in the owner's Claude plans directory).

## Scope (owner items 1–4 + review addition 5)

1. **Guild seed & stakes warning.** The stakes-penalty banner clipped behind
   the footer at its longest text (owner screenshot, −99%); the seed was a
   read-only chip below the panel. Wanted: seed next to where new seeds are
   selected, manually adjustable; warning fully visible.
2. **Scoreboard & Save Point from all sides**, not one designated tile.
3. **Storyteller ↔ black market spacing** — the dealer could spawn 2 tiles
   from the bard and their name labels mingled; move the Storyteller 2 tiles
   right.
4. **Item shop order by category** (HP heal, MP heal, …), Potion on top.
5. **Enemy-team inspection** in the dungeon (plan-review addition): judge a
   team's members before engaging, to gear for it.

## What was built

- **Seed row (interpretation note):** "move seed next to where we can select
  new seed" was implemented by MERGING the two — one Seed stepper row (the
  M25/M46 idiom): the value in a wide capsule on the row, Left/Right rolls a
  fresh seed (the retired "New Seed" row's job), Confirm opens a modal digit
  editor. `ui::TextFilter::Digits` (new) + `core/SeedParse.hpp`
  (`parseSeedDigits`, pure): empty or zero keeps the old seed; a value past
  the 20-digit uint64 ceiling clamps to the maximum. The editor consumes
  TextBackspace before Cancel (PartyCreation precedent) and therefore
  advertises only OK/Erase in the footer; its own hint line says "Empty
  keeps the old seed."
- **Banner reposition:** with the chip gone the M33 banner moved from
  panel+24 to panel+8 — a two-line wrap now ends ≈10 px above the footer.
  Capture `26_guild_penalty` is pinned at the −99% cap (longest text);
  new capture `106_guild_seed_edit` pins the 20-digit editor.
- **Monument adjacency:** `town::monumentInteractsFromAllSides()` +
  `town::tileAdjacentToBuilding()` (pure, in `town/TownData.hpp`);
  `TownState::buildingAtPlayerTile()` tries exact doors first, then monument
  rims. Only Scoreboard and SavePoint opt in. Tests prove the rims are
  walkable, reachable, exactly the orthogonal surround, and swallow no other
  interactable.
- **Storyteller move:** bard tile → `town/TownData.hpp` (`kBardTileX/Y`,
  now (5,5) — the header's "single layout authority" claim is finally true;
  it lived in TownState.cpp). The black-market tile list retired {5,5}
  (the bard's new spot) for {17,10}; a spacing rule is now a test: no market
  tile within Chebyshev 2 of the bard, none within 6 tiles horizontally on
  the bard's row (labels are ~60–72 px wide at tileTop−9). Pre-M88 saves
  holding an offer on the retired tile are snapped by the save loader's
  EXISTING tile validation — no new migration code.
- **Shop order:** `itemShopCategoryRank()` — heal → restore_mp → cure →
  revive → oddities; value ascending then id within a group. Items whose
  real job is a special rider (`curesDebuffs`, `curesCurse`,
  `kingEffectAmount` — Royal Snacks, Holy Taxes) rank as oddities despite
  their token 10-HP `effect`. Town-1 shelf: Potion, Hi-Potion, Mega Potion,
  Ether, Remedy, Phoenix Tear, Royal Snacks.
- **Team inspection:** `dungeon/TeamInspect.hpp` (`describeTeam`, pure) —
  the dungeon Details overlay now leads with the faced team's roster: per
  member the SCALED stats (`content::scaledStats` × `team.statScalePct`,
  the buildBattle multiply — honesty by construction), Weak/Immune lines,
  passives; duplicates collapse to xN; the boss block first; the M22 tier
  reference prose follows. Disclosure deliberately equals the in-battle
  target panel (never bestiary-gated) — the panel reveals nothing the first
  aim would not. New capture `107_team_inspect` (generated boss court).

## Plan deviations

- The plan sketched the Seed row ABOVE a kept "New Seed" row; implementation
  merged them (above). Fewer rows, no panel growth, and the value sits
  exactly "next to where we select new seed". Routine UI decision within
  scope; flagged here for the owner's manual pass.
- The plan's replacement market tile example was {7,9}; {17,10} was chosen
  after checking walkability, monument rims, and the label-spacing rule.

## Compatibility

- **Saves:** no schema change (v1). Retired market tile snaps via the
  existing loader validation.
- **Settings/content schemas:** unchanged.
- **Deterministic seeds:** unchanged — no generation change, no version
  bump. The market tile INDEX derivation is untouched (presentation-only
  list edit); dungeon content never depended on the town tile.
- **Scores:** unaffected.
- **Assets/manifest:** unchanged (no new assets).

## Automated validation

- Build: `cmake --build --preset debug` (VS2022 dev shell) — clean, no
  project warnings.
- Tests: `ctest --preset debug` — **749/749 pass.** Two pre-existing
  itemshop tests asserted the OLD alphabetical order and were updated to the
  new contract (the ordering contract itself is tested in
  `test_item_caps.cpp`).
- Capture: `CrystalDungeons.exe --capture` — **107/107 scenes clean**
  (105 + the two new M88 scenes), including the −99% banner worst case.
- Release: `cmake --build --preset release` — clean.

## Manual owner checklist

See `docs/manual_test_matrix.md` rows **167–170**:
167 guild seed row + −99% banner · 168 monument all-side interaction ·
169 shop shelf order · 170 team inspection. Feel judgments the automation
cannot make: does the merged Seed row read naturally; is the digit editor
comfortable; do the monument rims feel right with a gamepad; is the
inspection body's information level right (it mirrors the battle target
panel by design).

## Known limitations

- The seed editor is keyboard-only for digit entry (gamepad users can still
  reroll with Left/Right; typing needs the keyboard — the PartyCreation
  naming flow has the same shape).
- The inspection body shows base-scaled stats, not live boss-mechanic
  context (enrage, triggers) — those remain telegraph/bestiary territory.

## Documentation updated

- `docs/milestones.md` — M88 row + program section.
- `docs/game_design.md` — §5 monuments/shop order, §6 inspection + seed row.
- `docs/technical_design.md` — new §41 (M88), storyteller tile authority,
  capture count 105→107.
- `docs/ui_style_guide.md` — stale "Guild seed chip" example replaced.
- `docs/manual_test_matrix.md` — rows 167–170.

## Final status

`implemented, awaiting manual approval`
