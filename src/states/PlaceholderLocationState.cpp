#include "states/PlaceholderLocationState.hpp"

#include <utility>

#include "core/AppContext.hpp"
#include "input/Input.hpp"
#include "raylib.h"
#include "states/StateStack.hpp"
#include "ui/UiDraw.hpp"

namespace cd {

PlaceholderLocationState::PlaceholderLocationState(StateStack& stack, AppContext& context,
                                                   std::string title,
                                                   std::vector<std::string> lines)
    : GameState(stack), context_(context), title_(std::move(title)), lines_(std::move(lines)) {}

void PlaceholderLocationState::handleInput(const Input& input) {
    if (input.pressed(InputAction::Cancel) || input.pressed(InputAction::Confirm)) {
        stack().popState();
    }
}

void PlaceholderLocationState::render() {
    const int w = context_.virtualWidth;
    const int h = context_.virtualHeight;
    ClearBackground(Color{16, 14, 26, 255});

    const int boxW = 300;
    const int boxH = 150;
    const int boxX = w / 2 - boxW / 2;
    const int boxY = h / 2 - boxH / 2;
    ui::drawPanel(boxX, boxY, boxW, boxH, Color{28, 26, 48, 255}, Color{120, 120, 200, 255});

    ui::drawTextCentered(title_.c_str(), w / 2, boxY + 14, 18, RAYWHITE);

    int y = boxY + 48;
    for (const std::string& line : lines_) {
        ui::drawTextCentered(line.c_str(), w / 2, y, 10, Color{190, 190, 210, 255});
        y += 16;
    }

    ui::drawTextCentered("Cancel: Back", w / 2, boxY + boxH - 20, 10, Color{150, 150, 170, 255});
}

}  // namespace cd
