#pragma once

#include <string>
#include <vector>

#include "save/SaveSystem.hpp"
#include "states/GameState.hpp"
#include "ui/Menu.hpp"

namespace cd {

struct AppContext;

enum class SlotMenuMode { Save, Load };

// Lists save slots with summaries. In Save mode (from the town Save Point) writes
// the active party to a manual slot. In Load mode (from the main menu) loads a
// slot and drops the player into town.
class SlotMenuState : public GameState {
public:
    SlotMenuState(StateStack& stack, AppContext& context, SlotMenuMode mode);

    void onEnter() override;
    void handleInput(const Input& input) override;
    void render() override;

private:
    void rebuild();
    void confirmSelection();
    void disarmOverwrite();

    AppContext& context_;
    SlotMenuMode mode_;
    ui::Menu menu_;
    std::vector<save::SaveSlot> slots_;
    // A party's King title (M40) is long enough to run off the row, so it is kept
    // apart from the slot label and drawn on its own line under it.
    std::vector<std::string> titles_;
    std::string message_;
    // M22: overwriting an existing save needs a second Confirm on the same
    // slot; moving the cursor or Cancel disarms it.
    int pendingOverwrite_ = -1;
};

}  // namespace cd
