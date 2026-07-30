#pragma once

#include <string>
#include <vector>

#include "states/GameState.hpp"
#include "ui/Menu.hpp"
#include "ui/ScrollWindow.hpp"

// Development-only cheat console (M53). Reachable from both pause menus in debug
// builds via a "Debug" row; the pause-menu call sites and this file's whole .cpp
// body are guarded by `#ifdef CRYSTAL_DEBUG_OVERLAY`, so a Release binary never
// instantiates it and its methods are never defined there (the file is compiled
// into the target unconditionally — the BattleState-capture-block precedent).
//
// The header is unconditional so the pause menus include a stable type; nothing
// references the class when the macro is off, so its (undefined) methods and
// vtable are never ODR-used.

namespace cd {

class StateStack;
struct AppContext;
class Input;

class DebugMenuState : public GameState {
public:
    // `inDungeon` gates the two context-specific rows: "Instant dungeon clear"
    // (dungeon only) and "Spawn black market" (town only).
    DebugMenuState(StateStack& stack, AppContext& context, bool inDungeon);

    void handleInput(const Input& input) override;
    void render() override;

private:
    // What a highlighted row does. Steppers respond to MoveLeft/Right; actions
    // (and toggles) fire on Confirm.
    enum class Row {
        Lv,                // param = party member index; stepper 1..99
        Gold,              // stepper +/- 1000
        Tokens,            // stepper +/- 1
        Town,              // stepper 1..7 (highest unlocked)
        UnlockCastle,      // action
        GrantConsumables,  // action: 5x each consumable
        GrantLegendaries,  // action: 1x each legendary
        SpawnMarket,       // action (town only)
        GodMode,           // toggle
        InstantClear,      // action (dungeon only)
        UnlockClasses,     // action
        FillBestiary,      // action
    };
    struct RowDef {
        Row kind;
        int param = 0;  // member index for Lv rows
    };

    void rebuild();
    void adjust(const RowDef& row, int dir);   // MoveLeft/Right on steppers
    void activate(const RowDef& row);          // Confirm on actions/toggles
    void setMemberLevel(int index, int level);

    AppContext& context_;
    bool inDungeon_;
    ui::Menu menu_;
    ui::ScrollWindow scroll_;
    std::vector<RowDef> rows_;
    std::string message_;
};

}  // namespace cd
