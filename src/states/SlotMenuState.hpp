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

    AppContext& context_;
    SlotMenuMode mode_;
    ui::Menu menu_;
    std::vector<save::SaveSlot> slots_;
    std::string message_;
};

}  // namespace cd
