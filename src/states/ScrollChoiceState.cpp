#include "states/ScrollChoiceState.hpp"

#include "audio/AudioManager.hpp"
#include "content/ContentDatabase.hpp"
#include "core/AppContext.hpp"
#include "game/Party.hpp"
#include "input/Input.hpp"
#include "raylib.h"
#include "states/StateStack.hpp"
#include "ui/UiDraw.hpp"
#include "ui/UiStyle.hpp"

namespace cd {

ScrollChoiceState::ScrollChoiceState(StateStack& stack, AppContext& context,
                                     std::vector<std::string> offers)
    : GameState(stack), context_(context), offers_(std::move(offers)) {
    std::vector<ui::MenuItem> items;
    for (const std::string& id : offers_) {
        const content::ItemDef* it = context_.content.findItem(id);
        items.push_back({it != nullptr ? it->name : id, true});
    }
    items.push_back({"(Leave it)", true});
    menu_.setItems(std::move(items));
}

void ScrollChoiceState::handleInput(const Input& input) {
    if (input.navPressed(InputAction::MoveUp)) {
        menu_.moveUp();
    }
    if (input.navPressed(InputAction::MoveDown)) {
        menu_.moveDown();
    }
    if (!input.pressed(InputAction::Confirm)) {
        return;  // Cancel deliberately does nothing: answer the trove
    }
    const int cursor = menu_.cursor();
    if (cursor >= 0 && cursor < static_cast<int>(offers_.size())) {
        context_.party.inventory.add(offers_[static_cast<std::size_t>(cursor)], 1);
        context_.audio.play(Sfx::Chest);
    } else {
        context_.audio.play(Sfx::Cancel);  // left for the next descent
    }
    stack().popState();
}

void ScrollChoiceState::render() {
    const int w = context_.virtualWidth;
    const int h = context_.virtualHeight;
    namespace style = ui::style;
    const style::Palette& p = style::palette();
    ui::drawModalDim(w, h);

    const int boxW = 260;
    const int boxH = 80 + static_cast<int>(menu_.size()) * 16;
    const int boxX = w / 2 - boxW / 2;
    const int boxY = h / 2 - boxH / 2;
    ui::drawFrame(boxX, boxY, boxW, boxH, ui::FrameStyle::Reward);
    ui::drawTitlePlaque("The Guild's Trove", w / 2, boxY - 10, 12);
    ui::drawTextCentered("A stakes-raising descent earns a scroll.", w / 2, boxY + 14,
                         style::kFontBody, p.textDim);
    ui::drawMenu(menu_, boxX + 28, boxY + 30, 16, 12, p.text, p.disabled, p.cursor);

    // The highlighted scroll's own description under the list.
    const int cursor = menu_.cursor();
    const int detailY = boxY + 34 + static_cast<int>(menu_.size()) * 16;
    if (cursor >= 0 && cursor < static_cast<int>(offers_.size())) {
        if (const content::ItemDef* it =
                context_.content.findItem(offers_[static_cast<std::size_t>(cursor)])) {
            ui::drawTextPreview(it->description + " Give it from the Party panel.", boxX + 14,
                                detailY, boxW - 28, style::kFontBody, p.textDim, 2);
        }
    } else {
        ui::drawTextPreview("Decline - the trove keeps nothing for later.", boxX + 14, detailY,
                            boxW - 28, style::kFontBody, p.textDim, 2);
    }
}

}  // namespace cd
