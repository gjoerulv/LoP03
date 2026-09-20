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
// reason — nothing is ever spent on "No effect"). Equipment is an inspect-only
// row here: gear changes hands in Equip Party, and the row says so. Map pieces
// and curios never enter the bag — the footer points at the Maps screen.
// M122 (owner request): skill SCROLLS are taught from here - pick the scroll,
// then the member. While the member is picked the skill itself stays in view
// (kind icon, name, MP cost, kind line, the description as THAT member would
// cast it), and a member who already knows it is a greyed row that says so.
// The rules are game/Scrolls.hpp's, unchanged: class-agnostic, never wasted.
class InventoryState : public GameState {
public:
    InventoryState(StateStack& stack, AppContext& context);

    void handleInput(const Input& input) override;
    void render() override;

#ifdef CRYSTAL_CAPTURE
    // Capture-only (M90): park the cursor on a given item id so a chosen
    // detail line is overflow-checked.
    void captureCursorToItem(const std::string& itemId);
    // Capture-only (M122): Confirm on the highlighted row (a scroll opens its
    // pupil pick), then park the pick's cursor on `memberRow`.
    void captureConfirm(int memberRow);
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
