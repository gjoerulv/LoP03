#include "states/DungeonMenuState.hpp"

#include <memory>

#include "core/AppContext.hpp"
#include "input/Input.hpp"
#include "raylib.h"
#include "states/SettingsState.hpp"
#include "states/StateStack.hpp"
#include "ui/UiDraw.hpp"

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
    DrawRectangle(0, 0, w, h, Color{0, 0, 0, 150});

    const int boxW = 190;
    const int boxH = 124;
    const int boxX = w / 2 - boxW / 2;
    const int boxY = h / 2 - boxH / 2;
    ui::drawPanel(boxX, boxY, boxW, boxH, Color{24, 22, 40, 240}, Color{150, 130, 180, 255});
    ui::drawTextCentered("Dungeon Paused", w / 2, boxY + 12, 14, RAYWHITE);
    ui::drawMenu(menu_, boxX + 30, boxY + 40, 18, 12, RAYWHITE, Color{90, 90, 110, 255},
                 Color{240, 220, 120, 255});
    ui::drawTextCentered("Retreat forfeits dungeon score", w / 2, boxY + boxH - 14, 8,
                         Color{160, 160, 180, 255});
}

}  // namespace cd
