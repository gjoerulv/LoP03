#pragma once

// Development-only cheat state (M53). Held as an unconditional value member on
// AppContext so its layout is the same in every build — but EVERY reader and
// writer is guarded by `#ifdef CRYSTAL_DEBUG_OVERLAY`, so nothing in a Release
// binary can observe or change it, and it never influences a shipped build.
//
// The struct itself is macro-independent on purpose: making the AppContext
// member conditional would change the context's layout between configurations,
// which is exactly the kind of ABI fragility the project avoids.

namespace cd {

struct DebugCheats {
    // Party god mode: the BattleState ctor copies this into the battle's
    // debugPartyUnkillable flag (also debug-only). Off by default.
    bool godMode = false;

    // One-shot request set by the debug menu's "Instant dungeon clear" row and
    // consumed by DungeonState on its next update, which routes it through the
    // real completeDungeon() path so scoring/unlocks/market rolls stay honest.
    bool requestDungeonClear = false;
};

}  // namespace cd
