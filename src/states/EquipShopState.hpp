#pragma once

#include <string>
#include <vector>

#include "content/Enums.hpp"
#include "states/GameState.hpp"
#include "ui/Menu.hpp"
#include "ui/ScrollWindow.hpp"

namespace cd {

struct AppContext;

// The Equipment Shop: buy weapons/armor/accessories/relics with gold, and equip
// them to party members (their stat bonuses fold into derived stats). A small
// phase machine: top menu -> pick a buy category -> filtered buy list, or equip
// (character -> slot -> item). "Buy Gear" splits into Weapons / Armor /
// Accessories (relics file under Accessories) so each list stays browsable (M31).
class EquipShopState : public GameState {
public:
    EquipShopState(StateStack& stack, AppContext& context);

    void handleInput(const Input& input) override;
    void render() override;

#ifdef CRYSTAL_CAPTURE
    // Capture-only (M31): open straight into the buy-category menu so the new
    // category-selection UI is covered by the overflow check. Not shipped.
    void captureEnterBuyCategory();
#endif

private:
    enum class Phase { Menu, BuyCategory, Buy, EquipChar, EquipSlot, EquipItem };

    void rebuild();
    void confirm();
    void openItemDetails();  // M22: item stats + per-member equip deltas

    AppContext& context_;
    Phase phase_ = Phase::Menu;
    ui::Menu menu_;
    ui::ScrollWindow scroll_;
    std::vector<std::string> rowIds_;  // item ids parallel to menu rows (Buy / EquipItem)
    content::EquipSlot buyCategory_ = content::EquipSlot::Weapon;  // filter for Phase::Buy (M31)
    int selectedChar_ = 0;
    int selectedSlot_ = 0;  // 0 weapon, 1 armor, 2 accessory
    std::string message_;
};

}  // namespace cd
