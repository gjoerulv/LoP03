#include "states/EncounterPreviewState.hpp"

#include <string>
#include <utility>

#include "content/ContentDatabase.hpp"
#include "core/AppContext.hpp"
#include "input/Input.hpp"
#include "raylib.h"
#include "states/StateStack.hpp"
#include "ui/UiDraw.hpp"

namespace cd {

EncounterPreviewState::EncounterPreviewState(StateStack& stack, AppContext& context,
                                             dungeon::EnemyTeam team)
    : GameState(stack), context_(context), team_(std::move(team)) {}

void EncounterPreviewState::handleInput(const Input& input) {
    if (input.pressed(InputAction::Cancel) || input.pressed(InputAction::Confirm)) {
        stack().popState();
    }
}

void EncounterPreviewState::render() {
    const int w = context_.virtualWidth;
    const int h = context_.virtualHeight;
    DrawRectangle(0, 0, w, h, Color{0, 0, 0, 170});

    const int boxW = 320;
    const int boxH = 180;
    const int boxX = w / 2 - boxW / 2;
    const int boxY = h / 2 - boxH / 2;
    const Color accent = team_.isBoss ? Color{220, 110, 110, 255} : Color{200, 180, 120, 255};
    ui::drawPanel(boxX, boxY, boxW, boxH, Color{26, 22, 34, 245}, accent);

    ui::drawTextCentered(team_.isBoss ? "Boss Encounter" : "Enemy Team", w / 2, boxY + 12, 16,
                         RAYWHITE);
    ui::drawTextCentered(team_.name.c_str(), w / 2, boxY + 34, 12, accent);

    DrawText(TextFormat("Enemies: %d", team_.count()), boxX + 20, boxY + 58, 10, RAYWHITE);

    int y = boxY + 76;
    for (const std::string& id : team_.enemyIds) {
        const char* display = id.c_str();
        if (const content::EnemyDef* def = context_.content.findEnemy(id)) {
            display = def->name.c_str();
        }
        DrawText(TextFormat("- %s", display), boxX + 28, y, 10, Color{210, 210, 225, 255});
        y += 14;
    }

    std::string tagLine = "Tags:";
    if (team_.tags.empty()) {
        tagLine += " none";
    } else {
        for (const std::string& t : team_.tags) {
            tagLine += " " + t;
        }
    }
    DrawText(tagLine.c_str(), boxX + 20, boxY + boxH - 50, 10, Color{170, 200, 170, 255});
    DrawText("Danger: rated in Milestone 6", boxX + 20, boxY + boxH - 36, 10,
             Color{150, 150, 170, 255});

    ui::drawTextCentered("Battle arrives in Milestone 5   -   Cancel: Back", w / 2,
                         boxY + boxH - 18, 10, Color{200, 180, 120, 255});
}

}  // namespace cd
