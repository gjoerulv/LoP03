#include "states/HelpState.hpp"

#include "audio/AudioManager.hpp"
#include "core/AppContext.hpp"
#include "input/Input.hpp"
#include "raylib.h"
#include "states/StateStack.hpp"
#include "ui/UiDraw.hpp"

namespace cd {

namespace {
struct Row {
    const char* action;
    const char* keyboard;
    const char* gamepad;
};
constexpr Row kRows[] = {
    {"Move / Navigate", "Arrows or WASD", "D-Pad / Left Stick"},
    {"Confirm", "Enter or Space", "A"},
    {"Cancel / Back", "Esc or Backspace", "B"},
    {"Menu / Pause", "Tab", "Start"},
    {"Adjust (Guild)", "Left / Right", "D-Pad L/R"},
    {"Toggle debug overlay", "F1", "-"},
};
}  // namespace

HelpState::HelpState(StateStack& stack, AppContext& context)
    : GameState(stack), context_(context) {}

void HelpState::handleInput(const Input& input) {
    if (input.pressed(InputAction::Cancel) || input.pressed(InputAction::Confirm)) {
        context_.audio.play(Sfx::Cancel);
        stack().popState();
    }
}

void HelpState::render() {
    const int w = context_.virtualWidth;
    const int h = context_.virtualHeight;
    ClearBackground(Color{16, 16, 26, 255});
    ui::drawTextCentered("Controls", w / 2, 18, 18, RAYWHITE);

    const int x = 40;
    int y = 56;
    DrawText("Action", x, y, 10, Color{170, 170, 200, 255});
    DrawText("Keyboard", x + 130, y, 10, Color{170, 170, 200, 255});
    DrawText("Gamepad", x + 270, y, 10, Color{170, 170, 200, 255});
    y += 18;
    for (const Row& r : kRows) {
        DrawText(r.action, x, y, 10, RAYWHITE);
        DrawText(r.keyboard, x + 130, y, 10, Color{200, 210, 230, 255});
        DrawText(r.gamepad, x + 270, y, 10, Color{200, 210, 230, 255});
        y += 16;
    }

    ui::drawTextCentered("The window is resizable; the image always scales to fit.", w / 2, h - 30,
                         9, Color{160, 160, 180, 255});
    ui::drawTextCentered("Cancel: Back", w / 2, h - 14, 10, Color{150, 150, 170, 255});
}

}  // namespace cd
