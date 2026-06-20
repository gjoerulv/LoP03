#include "states/ScoreboardState.hpp"

#include "core/AppContext.hpp"
#include "input/Input.hpp"
#include "raylib.h"
#include "score/Scoreboard.hpp"
#include "states/StateStack.hpp"
#include "ui/UiDraw.hpp"

namespace cd {

ScoreboardState::ScoreboardState(StateStack& stack, AppContext& context)
    : GameState(stack), context_(context) {}

void ScoreboardState::handleInput(const Input& input) {
    if (input.pressed(InputAction::Cancel) || input.pressed(InputAction::Confirm)) {
        stack().popState();
    }
}

void ScoreboardState::render() {
    const int w = context_.virtualWidth;
    const int h = context_.virtualHeight;
    ClearBackground(Color{16, 16, 24, 255});
    ui::drawTextCentered("Scoreboard", w / 2, 16, 18, RAYWHITE);

    const auto& entries = context_.scoreboard.entries();
    if (entries.empty()) {
        ui::drawTextCentered("No runs recorded yet. Clear a dungeon to set a score!", w / 2, h / 2,
                             10, Color{180, 180, 200, 255});
    } else {
        DrawText("#   Score   Turns   Danger   Theme", 40, 44, 10, Color{170, 170, 190, 255});
        int y = 60;
        const int shown = entries.size() < 14 ? static_cast<int>(entries.size()) : 14;
        for (int i = 0; i < shown; ++i) {
            const score::ScoreEntry& e = entries[static_cast<std::size_t>(i)];
            const Color rowColor = e.noDeath ? Color{210, 230, 200, 255} : RAYWHITE;
            DrawText(TextFormat("%-2d  %6d  %5d   %5d    %s%s", i + 1, e.score, e.battleTurns,
                                e.dangerDefeated, e.theme.c_str(), e.noDeath ? "  *" : ""),
                     40, y, 10, rowColor);
            y += 13;
        }
        DrawText("* = no-death run", 40, y + 4, 8, Color{150, 170, 150, 255});
    }

    ui::drawTextCentered("Cancel: Back", w / 2, h - 16, 10, Color{150, 150, 170, 255});
}

}  // namespace cd
