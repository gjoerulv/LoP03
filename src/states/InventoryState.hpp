#pragma once

#include <string>
#include <vector>

#include "states/GameState.hpp"
#include "ui/Menu.hpp"
#include "ui/ScrollWindow.hpp"

namespace cd {

struct AppContext;

// M90 (owner item 10): the pause menus' Items screen — inspect the whole bag
// and USE healing consumables outside battle under the M43 gating rules
// (heal/cure only the living, revive only the fallen, capped, refused with a
// reason — nothing is ever spent on "No effect"). Equipment and skill scrolls
// are inspect-only rows here: gear changes hands in Equip Party, scrolls teach
// from the Party panel, and each row's detail line says so. Map pieces and
// curios never enter the bag — the footer points at the Maps screen.
class InventoryState : public GameState {
public:
    InventoryState(StateStack& stack, AppContext& context);

    void handleInput(const Input& input) override;
    void render() override;

#ifdef CRYSTAL_CAPTURE
    // Capture-only (M90): park the cursor on a given item id so a chosen
    // detail line is overflow-checked.
    void captureCursorToItem(const std::string& itemId);
#endif

private:
    enum class Phase { List, PickMember };

    void rebuild();
    void confirm();

    AppContext& context_;
    Phase phase_ = Phase::List;
    ui::Menu menu_;
    ui::ScrollWindow scroll_;
    std::vector<std::string> rowIds_;  // item ids parallel to List rows
    std::string selectedItem_;         // the consumable being aimed (PickMember)
    std::string message_;
    bool messageIsError_ = false;
};

}  // namespace cd
