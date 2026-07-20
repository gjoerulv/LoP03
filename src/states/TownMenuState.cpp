#include "states/TownMenuState.hpp"

#include <memory>

#include "core/AppContext.hpp"
#include "input/Input.hpp"
#include "raylib.h"
#include "states/MainMenuState.hpp"
#include "states/SettingsState.hpp"
#include "states/StateStack.hpp"
#include "ui/UiDraw.hpp"
#include "ui/UiStyle.hpp"

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
        quitArmed_ = false;
    }
    if (input.navPressed(InputAction::MoveDown)) {
        menu_.moveDown();
        quitArmed_ = false;
    }
    if (input.pressed(InputAction::Cancel)) {
        if (quitArmed_) {
            quitArmed_ = false;  // keep playing
            return;
        }
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
                // Destructive-action confirmation (M22): progress since the
                // last save is lost, so quitting needs a second Confirm.
                if (!quitArmed_) {
                    quitArmed_ = true;
                    return;
                }
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

    const int boxW = 220;
    const int boxH = quitArmed_ ? 128 : 106;
    const int boxX = w / 2 - boxW / 2;
    const int boxY = h / 2 - boxH / 2;
    ui::drawFramedPanel(context_.resources, boxX, boxY, boxW, boxH, Color{28, 26, 48, 240}, Color{120, 120, 200, 255});
    ui::drawTextCentered("Paused", w / 2, boxY + 12, 14, RAYWHITE);
    ui::drawMenu(menu_, boxX + 60, boxY + 38, 18, 12, RAYWHITE, Color{90, 90, 110, 255},
                 Color{240, 220, 120, 255});
    if (quitArmed_) {
        ui::drawTextWrapped("Progress since your last save is lost. Confirm again to quit.",
                            boxX + 10, boxY + 96, boxW - 20, 8,
                            ui::style::palette().dangerText, "townmenu.quitwarn", 2);
    }
}

}  // namespace cd
