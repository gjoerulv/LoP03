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
//
// M90: the same state also serves the pause menus' "Equip Party" (owner item
// 10) via `partyMode` — it opens straight in the equip flow (EquipChar), the
// shop's top menu and Buy phases are unreachable, Cancel from EquipChar
// leaves, and the header reads "Equip Party" with no shop dressing. One phase
// machine, zero duplication.
class EquipShopState : public GameState {
public:
    EquipShopState(StateStack& stack, AppContext& context, bool partyMode = false);

    void handleInput(const Input& input) override;
    void render() override;

#ifdef CRYSTAL_CAPTURE
    // Capture-only (M31): open straight into the buy-category menu so the new
    // category-selection UI is covered by the overflow check. Not shipped.
    void captureEnterBuyCategory();
    // Capture-only (M37): open straight into a filtered buy list, to overflow-check
    // the scrolling list at max stock (town 7).
    void captureEnterBuyList(content::EquipSlot slot);
    // Capture-only (M81): park the buy cursor on a specific item id, so the
    // scene referees a chosen row's detail line (the ward-charm resist text).
    void captureCursorToItem(const std::string& itemId);
    // Capture-only (M52): open the equip-item list for a member's slot, so the
    // current + diff detail panel is overflow-checked.
    void captureEnterEquipItem(int charIndex, content::EquipSlot slot);
#endif

private:
    enum class Phase { Menu, BuyCategory, Buy, EquipChar, EquipSlot, EquipItem };

    void rebuild();
    void confirm();
    void openItemDetails();  // M22: item stats + per-member equip deltas

    AppContext& context_;
    bool partyMode_ = false;  // M90: pause-menu equip flow (no shop phases)
    Phase phase_ = Phase::Menu;
    ui::Menu menu_;
    ui::ScrollWindow scroll_;
    std::vector<std::string> rowIds_;  // item ids parallel to menu rows (Buy / EquipItem)
    content::EquipSlot buyCategory_ = content::EquipSlot::Weapon;  // filter for Phase::Buy (M31)
    int selectedChar_ = 0;
    int selectedSlot_ = 0;  // 0 weapon, 1 armor, 2 accessory
    std::string message_;
    bool messageIsError_ = false;  // picks the banner treatment (presentation only)
};

}  // namespace cd
