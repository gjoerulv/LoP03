#pragma once

#include <algorithm>

#include "game/Castle.hpp"  // kKingScalePct, kBossRushScalePct

// M52 — the bestiary's "at their strongest" scale rule, as a pure function.
//
// Headless-testable so the four-context mapping is pinned by a test rather than
// buried in the render. Raylib-free.

namespace cd {

// The strongest real fight context a foe appears in, as a stat-scale percent
// (owner rule, 2026-07-23):
//   - an own-arena boss (the King; M61 adds the Deadly Duck): its bespoke
//     arena scale (kKingScalePct == kGooseTownScalePct == 500%) — neither is
//     ever in the Boss Rush;
//   - any other boss: the greater of the dungeon ceiling and the Boss Rush
//     (max(castleFloorPct, kBossRushScalePct) = 580% at the shipped values);
//   - a bossOnly foe (a Royal Guard, an Evil Goose): the arena scale (500%),
//     since they are only ever fought in those bespoke fights;
//   - a regular enemy (incl. elites): the dungeon ceiling castleFloorPct (570%).
// The endless rush is deliberately excluded — its scale is unbounded by design.
// `castleFloorPct` is castleFloorScalePct(content), passed in so this stays a
// pure mapping independent of the content database.
inline int foeMaxScalePct(bool boss, bool bossOnly, bool ownArena, int castleFloorPct) {
    static_assert(kKingScalePct == kGooseTownScalePct,
                  "own-arena bosses share one bespoke scale; split this mapping if they diverge");
    if (ownArena) {
        return kKingScalePct;
    }
    if (boss) {
        return std::max(castleFloorPct, kBossRushScalePct);
    }
    if (bossOnly) {
        return kKingScalePct;
    }
    return castleFloorPct;
}

}  // namespace cd
