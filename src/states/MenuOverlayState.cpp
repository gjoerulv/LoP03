#include "states/MenuOverlayState.hpp"

#include "core/AppContext.hpp"
#include "core/Log.hpp"
#include "input/Input.hpp"
#include "raylib.h"
#include "states/StateStack.hpp"

namespace cd {

MenuOverlayState::MenuOverlayState(StateStack& stack, AppContext& context)
    : GameState(stack), context_(context) {}

void MenuOverlayState::onEnter() { log::info("Opened MenuOverlayState"); }

void MenuOverlayState::handleInput(const Input& input) {
    if (input.pressed(InputAction::Cancel) || input.pressed(InputAction::Menu)) {
        stack().popState();  // resume the title
    }
}

void MenuOverlayState::render() {
    const int w = context_.virtualWidth;
    const int h = context_.virtualHeight;

    // Dim the screen so the title shows through, proving transparent rendering.
    DrawRectangle(0, 0, w, h, Color{0, 0, 0, 150});

    // A simple framed window.
    const int boxW = 220;
    const int boxH = 90;
    const int boxX = w / 2 - boxW / 2;
    const int boxY = h / 2 - boxH / 2;
    DrawRectangle(boxX, boxY, boxW, boxH, Color{28, 26, 48, 240});
    DrawRectangleLines(boxX, boxY, boxW, boxH, Color{120, 120, 200, 255});

    const char* heading = "PAUSE / MENU";
    DrawText(heading, w / 2 - MeasureText(heading, 12) / 2, boxY + 14, 12, RAYWHITE);

    const char* line1 = "(placeholder menu)";
    DrawText(line1, w / 2 - MeasureText(line1, 10) / 2, boxY + 40, 10,
             Color{170, 170, 200, 255});

    const char* line2 = "Esc / Tab:  Back";
    DrawText(line2, w / 2 - MeasureText(line2, 10) / 2, boxY + 62, 10, RAYWHITE);
}

}  // namespace cd
