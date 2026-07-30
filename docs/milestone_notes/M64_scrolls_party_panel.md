# M64 — Scroll learning + the Party panel

Third milestone of the M62–M66 program (plan approved 2026-07-27).
Implemented 2026-07-27.

## A. Status

**◑ implemented, awaiting manual approval** — set 2026-07-27. Evidence in §E.

## B. Goal (owner brief + audit finding)

A detailed party information panel reachable from the pause menu in both
town and dungeons — and, feeding the M65 treasure system, a real
scroll-learning mechanism. The M63 exploration discovered the three shipped
scrolls (`scroll_fireball` / `scroll_greater_heal` / `scroll_whirlwind`)
were **inert**: `grantsSkill` was loaded and validated since M2 but no code
ever consumed it. The owner chose to fix them with the same mechanism the
treasure scrolls will use.

## C. As implemented

- **`Character.extraSkills`** (optional save field; unknown ids dropped,
  de-duplicated on load). `game/Scrolls.hpp` is the pure rule set:
  `allKnownSkills` (class learnset ∪ extras, deduped — the ONE rule
  `buildBattle` and the panel both read), `scrollRefusal` (not a scroll /
  illegible / already known — a scroll is never wasted), `learnScroll`.
  Class-agnostic (owner decision). No version bumps: `buildBattle`'s skill
  list only changes for a character who actually learned a scroll —
  something no save could contain before M64.
- **`PartyState`** (both pause menus gained a "Party" row; boxes grew one
  row): member list + detail — HP/MP, XP-to-next, the four stats each with
  its **gear share** ("(+N gear)"), equipment names, equipped passive +
  owned count, M63 milestone choices (with "Lv.N unchosen" hints), and
  every known skill with scroll-learned entries marked `*`.
- **Use Scroll** lives on the panel: Confirm opens a picker over the bag's
  teaching scrolls; teaching consumes the scroll, refuses with the stated
  reason, and works in town and mid-dungeon alike.

## D. Files changed

`game/Character.hpp`, `game/Scrolls.hpp` (new), `battle/Battle.cpp`
(skill-list union), `save/SaveSystem.cpp`, `states/PartyState.{hpp,cpp}`
(new), `states/TownMenuState.cpp`, `states/DungeonMenuState.cpp`,
`capture/CaptureRunner.cpp` (+`79_party_panel`), `CMakeLists.txt`,
`tests/CMakeLists.txt`, `tests/test_scrolls.cpp` (new), docs.

## E. Automated validation (2026-07-27)

- `[scroll]` battery: **4 cases / 27 assertions green** — the shipped
  scrolls all teach real skills; refusal rules (repeat-learn, class-known,
  non-scroll); the learned skill reaches `buildBattle`'s skill list deduped;
  save round-trip drops unknown ids and duplicates.
- Closing verification: **583/583 Debug and 579/579 Release tests green**
  (the 4-case gap is the debug-only god-mode battery); `--capture` **79/79**
  scenes clean (+`79_party_panel`); zero project-code warnings.

## F. Manual owner checklist

1. Pause in town AND in a dungeon: both menus list **Party**; the panel
   shows each member's stats (gear share reads truthfully against the Equip
   Shop), gear, passive, milestones, and skills.
2. Buy/find a scroll, use it from the panel: the named member learns the
   skill permanently (check the battle Skill menu, marked `*` on the
   panel); the scroll is consumed; teaching it to someone who knows the
   skill refuses and keeps the scroll.
3. Save and reload — the learned skill persists.
4. Panel legibility at 426×240 with 12-char names and a full bag (capture
   `79_party_panel` guards overflow; feel is yours).

## G. Known limitations

- The panel is read-only apart from Use Scroll — equipping/passives still
  happen in their own shops (deliberate: one action per place).
- Learned skills cannot be forgotten; a forget/respec service would be a
  new owner decision.

## H. Final status

`implemented, awaiting manual approval`
