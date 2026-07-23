#include "states/TownMenuState.hpp"

#include <memory>

#include "core/AppContext.hpp"
#include "input/Input.hpp"
#include "raylib.h"
#include "states/AchievementsState.hpp"
#include "states/BestiaryState.hpp"
#include "states/QuitFlow.hpp"
#include "states/QuitPrompt.hpp"
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
constexpr int kQuit = 4;
}  // namespace

TownMenuState::TownMenuState(StateStack& stack, AppContext& context)
    : GameState(stack), context_(context) {
    menu_.setItems({{"Resume", true},
                    {"Bestiary", true},
                    {"Achievements", true},
                    {"Settings", true},
                    {"Quit", true}});
}

void TownMenuState::handleInput(const Input& input) {
    if (input.navPressed(InputAction::MoveUp)) {
        menu_.moveUp();
    }
    if (input.navPressed(InputAction::MoveDown)) {
        menu_.moveDown();
    }
    // M47: the key that opened the pause menu closes it again (Tab / Start),
    // alongside Cancel. One action, both devices, no new hint text.
    if (input.pressed(InputAction::Cancel) || input.pressed(InputAction::Menu)) {
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
            case kQuit:
                // Destructive-action confirmation (M22): progress since the last
                // save is lost, so quitting asks the question outright rather than
                // arming a second press on the same entry. M47 turned the single
                // answer into three (title / desktop / stay).
                pushQuitPrompt(stack(), context_, quit::kTownBody);
                break;
            default:
                break;
        }
    }
}

void TownMenuState::render() {
    const int w = context_.virtualWidth;
    const int h = context_.virtualHeight;

    const ui::style::Palette& p = ui::style::palette();
    ui::drawModalDim(w, h);

    const int boxW = 220;
    const int boxH = 132;  // fits the 5 pause entries (M42)
    const int boxX = w / 2 - boxW / 2;
    const int boxY = h / 2 - boxH / 2;
    ui::drawFrame(boxX, boxY, boxW, boxH, ui::FrameStyle::Raised);
    ui::drawTitlePlaque("Paused", w / 2, boxY - 10, 12);
    ui::drawMenu(menu_, boxX + 44, boxY + 26, 20, 12, p.text, p.disabled, p.cursor);
}

}  // namespace cd
