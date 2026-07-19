#pragma once

#include <string>
#include <vector>

#include "states/GameState.hpp"
#include "ui/Menu.hpp"
#include "ui/ScrollWindow.hpp"

namespace cd {

struct AppContext;

// The Equipment Shop: buy weapons/armor/accessories/relics with gold, and equip
// them to party members (their stat bonuses fold into derived stats). A small
// phase machine: top menu -> buy list, or equip (character -> slot -> item).
class EquipShopState : public GameState {
public:
    EquipShopState(StateStack& stack, AppContext& context);

    void onEnter() override;
    void handleInput(const Input& input) override;
    void render() override;

private:
    enum class Phase { Menu, Buy, EquipChar, EquipSlot, EquipItem };

    void rebuild();
    void confirm();

    AppContext& context_;
    Phase phase_ = Phase::Menu;
    ui::Menu menu_;
    ui::ScrollWindow scroll_;
    std::vector<std::string> rowIds_;  // item ids parallel to menu rows (Buy / EquipItem)
    int selectedChar_ = 0;
    int selectedSlot_ = 0;  // 0 weapon, 1 armor, 2 accessory
    std::string message_;
};

}  // namespace cd
