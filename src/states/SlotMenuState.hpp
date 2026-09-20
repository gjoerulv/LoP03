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
    void update(float dt) override;  // M127: the highlighted party's hop clock
    bool pausesPlayClock() const override { return true; }  // M109: a menu, not play
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
    // M123: each row's total play time in seconds (-1 = empty slot, no clock).
    std::vector<long long> playSeconds_;
    // M127: each row's party (class + fallen flag per member; empty = empty
    // slot) - drawn as sprites left of the clock, hopping while highlighted.
    std::vector<std::vector<save::SlotSummary::Member>> parties_;
    float time_ = 0.0f;
    // M123: a Save list opened by an Iron Man party - every row greyed, the
    // refusal on the banner. The one answer for all three Save entry points.
    bool ironManRefusal_ = false;
    std::string message_;
    // M22: overwriting an existing save needs a second Confirm on the same
    // slot; moving the cursor or Cancel disarms it.
    int pendingOverwrite_ = -1;
};

}  // namespace cd
