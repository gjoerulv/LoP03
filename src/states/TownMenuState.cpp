#include "states/TownMenuState.hpp"

#include <memory>

#include "core/AppContext.hpp"
#include "input/Input.hpp"
#include "raylib.h"
#include "states/MainMenuState.hpp"
#include "states/SettingsState.hpp"
#include "states/StateStack.hpp"
#include "ui/UiDraw.hpp"

namespace cd {

namespace {
constexpr int kResume = 0;
constexpr int kSettings = 1;
constexpr int kQuitToTitle = 2;
}  // namespace

TownMenuState::TownMenuState(StateStack& stack, AppContext& context)
    : GameState(stack), context_(context) {
    menu_.setItems({{"Resume", true}, {"Settings", true}, {"Quit to Title", true}});
}

void TownMenuState::handleInput(const Input& input) {
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
            case kQuitToTitle:
                stack().clearStates();
                stack().pushState(std::make_unique<MainMenuState>(stack(), context_));
                break;
            default:
                break;
        }
    }
}

void TownMenuState::render() {
    const int w = context_.virtualWidth;
    const int h = context_.virtualHeight;

    DrawRectangle(0, 0, w, h, Color{0, 0, 0, 150});

    const int boxW = 180;
    const int boxH = 106;
    const int boxX = w / 2 - boxW / 2;
    const int boxY = h / 2 - boxH / 2;
    ui::drawPanel(boxX, boxY, boxW, boxH, Color{28, 26, 48, 240}, Color{120, 120, 200, 255});
    ui::drawTextCentered("Paused", w / 2, boxY + 12, 14, RAYWHITE);
    ui::drawMenu(menu_, boxX + 40, boxY + 38, 18, 12, RAYWHITE, Color{90, 90, 110, 255},
                 Color{240, 220, 120, 255});
}

}  // namespace cd
