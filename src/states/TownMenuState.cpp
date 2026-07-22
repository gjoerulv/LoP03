#include "states/TownMenuState.hpp"

#include <memory>

#include "core/AppContext.hpp"
#include "input/Input.hpp"
#include "raylib.h"
#include "states/AchievementsState.hpp"
#include "states/BestiaryState.hpp"
#include "states/ConfirmPromptState.hpp"
#include "states/MainMenuState.hpp"
#include "states/SettingsState.hpp"
#include "states/StateStack.hpp"
#include "ui/UiDraw.hpp"
#include "ui/UiStyle.hpp"

namespace cd {

namespace {
constexpr int kResume = 0;
constexpr int kBestiary = 1;
constexpr int kAchievements = 2;
constexpr int kSettings = 3;
constexpr int kQuitToTitle = 4;
}  // namespace

TownMenuState::TownMenuState(StateStack& stack, AppContext& context)
    : GameState(stack), context_(context) {
    menu_.setItems({{"Resume", true},
                    {"Bestiary", true},
                    {"Achievements", true},
                    {"Settings", true},
                    {"Quit to Title", true}});
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
            case kBestiary:
                stack().pushState(std::make_unique<BestiaryState>(stack(), context_));
                break;
            case kAchievements:
                stack().pushState(std::make_unique<AchievementsState>(stack(), context_));
                break;
            case kSettings:
                stack().pushState(std::make_unique<SettingsState>(stack(), context_));
                break;
            case kQuitToTitle: {
                // Destructive-action confirmation (M22): progress since the last
                // save is lost, so quitting asks the question outright rather than
                // arming a second press on the same entry.
                StateStack* stackPtr = &stack();
                AppContext* contextPtr = &context_;
                stack().pushState(std::make_unique<ConfirmPromptState>(
                    stack(), context_, "Quit to Title",
                    "Progress since your last save will be lost.", "Quit to Title",
                    "Keep Playing", [stackPtr, contextPtr]() {
                        stackPtr->clearStates();
                        stackPtr->pushState(
                            std::make_unique<MainMenuState>(*stackPtr, *contextPtr));
                    }));
                break;
            }
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
    const int boxH = 150;  // fits the 5 pause entries (M42)
    const int boxX = w / 2 - boxW / 2;
    const int boxY = h / 2 - boxH / 2;
    ui::drawFramedPanel(context_.resources, boxX, boxY, boxW, boxH, Color{28, 26, 48, 240}, Color{120, 120, 200, 255});
    ui::drawTextCentered("Paused", w / 2, boxY + 12, 14, RAYWHITE);
    ui::drawMenu(menu_, boxX + 40, boxY + 38, 18, 12, RAYWHITE, Color{90, 90, 110, 255},
                 Color{240, 220, 120, 255});
}

}  // namespace cd
