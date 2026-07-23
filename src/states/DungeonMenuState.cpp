#include "states/DungeonMenuState.hpp"

#include <memory>

#include "core/AppContext.hpp"
#include "input/Input.hpp"
#include "raylib.h"
#include "states/SettingsState.hpp"
#include "states/StateStack.hpp"
#include "ui/UiDraw.hpp"
#include "ui/UiStyle.hpp"

namespace cd {

namespace {
constexpr int kResume = 0;
constexpr int kSettings = 1;
constexpr int kRetreat = 2;
}  // namespace

DungeonMenuState::DungeonMenuState(StateStack& stack, AppContext& context)
    : GameState(stack), context_(context) {
    menu_.setItems({{"Resume", true}, {"Settings", true}, {"Retreat to Town", true}});
}

void DungeonMenuState::handleInput(const Input& input) {
    if (input.navPressed(InputAction::MoveUp)) {
        menu_.moveUp();
    }
    if (input.navPressed(InputAction::MoveDown)) {
        menu_.moveDown();
    }
    if (input.pressed(InputAction::Cancel)) {
        stack().popState();
        return;
    }
    if (input.pressed(InputAction::Confirm)) {
        switch (menu_.cursor()) {
            case kResume:
                stack().popState();
                break;
            case kSettings:
                stack().pushState(std::make_unique<SettingsState>(stack(), context_));
                break;
            case kRetreat:
                stack().popState();  // close this menu
                stack().popState();  // leave the dungeon, back to town
                break;
            default:
                break;
        }
    }
}

void DungeonMenuState::render() {
    const int w = context_.virtualWidth;
    const int h = context_.virtualHeight;
    namespace style = ui::style;
    const style::Palette& p = style::palette();
    ui::drawModalDim(w, h);

    const int boxW = 190;
    const int boxH = 112;
    const int boxX = w / 2 - boxW / 2;
    const int boxY = h / 2 - boxH / 2;
    ui::drawFrame(boxX, boxY, boxW, boxH, ui::FrameStyle::Raised);
    // Integrated title plaque riding the top edge.
    ui::drawTitlePlaque("Dungeon Paused", w / 2, boxY - 10, 12);
    ui::drawMenu(menu_, boxX + 34, boxY + 26, 18, 12, p.text, p.disabled, p.cursor);
    ui::drawDivider(boxX + 12, boxY + boxH - 24, boxW - 24);
    // Retreat consequence as a danger note: warning shape + coral text.
    const char* note = "Retreat forfeits dungeon score";
    const int noteW = ui::measureText(note, style::kFontSmall);
    const int noteX = w / 2 - noteW / 2 + 5;
    DrawRectangle(noteX - 11, boxY + boxH - 16, 2, 2, p.danger);
    DrawRectangle(noteX - 13, boxY + boxH - 14, 6, 2, p.danger);
    DrawRectangle(noteX - 15, boxY + boxH - 12, 10, 2, p.danger);
    ui::drawText(note, noteX, boxY + boxH - 17, style::kFontSmall, p.dangerText);
}

}  // namespace cd
