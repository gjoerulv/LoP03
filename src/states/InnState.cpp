#include "states/InnState.hpp"

#include "audio/AudioManager.hpp"
#include "core/AppContext.hpp"
#include "game/Party.hpp"
#include "input/Input.hpp"
#include "input/PromptLabels.hpp"
#include "raylib.h"
#include "states/StateStack.hpp"
#include "ui/UiDraw.hpp"

namespace cd {

InnState::InnState(StateStack& stack, AppContext& context)
    : GameState(stack), context_(context) {}

void InnState::onEnter() {
    healFull(context_.party);
    context_.audio.play(Sfx::Heal);
}

void InnState::handleInput(const Input& input) {
    if (input.pressed(InputAction::Cancel) || input.pressed(InputAction::Confirm)) {
        stack().popState();
    }
}

void InnState::render() {
    const int w = context_.virtualWidth;
    const int h = context_.virtualHeight;
    ClearBackground(Color{20, 16, 24, 255});

    const int boxW = 320;
    const int boxH = 170;
    const int boxX = w / 2 - boxW / 2;
    const int boxY = h / 2 - boxH / 2;
    ui::drawPanel(boxX, boxY, boxW, boxH, Color{30, 24, 40, 255}, Color{200, 170, 120, 255});

    ui::drawTextCentered("Inn", w / 2, boxY + 12, 18, RAYWHITE);
    ui::drawTextCentered("The party rests. HP and MP fully restored.", w / 2, boxY + 38, 10,
                         Color{200, 200, 170, 255});

    // Two real columns; space-padding cannot align a variable-width font.
    int y = boxY + 62;
    for (const Character& c : context_.party.members) {
        ui::drawTextFitted(TextFormat("%s  Lv.%d", c.name.c_str(), c.level), boxX + 20, y, 124,
                           10, RAYWHITE, "inn.name");
        DrawText(TextFormat("HP %d/%d   MP %d/%d", c.hp, c.maxHp, c.mp, c.maxMp), boxX + 150, y,
                 10, Color{170, 220, 170, 255});
        y += 18;
    }

    ui::drawTextCentered(input::prompt(context_.input.map(), InputAction::Cancel,
                                       context_.input.activeDevice(), "Back")
                             .c_str(),
                         w / 2, boxY + boxH - 20, 10, Color{160, 150, 140, 255});
}

}  // namespace cd
