#include "states/DungeonResultState.hpp"

#include <algorithm>
#include <memory>
#include <utility>

#include "audio/AudioManager.hpp"
#include "core/AppContext.hpp"
#include "core/FadeController.hpp"
#include "input/Input.hpp"
#include "input/PromptLabels.hpp"
#include "raylib.h"
#include "states/DetailsOverlayState.hpp"
#include "states/ScoreDetailsText.hpp"
#include "states/StateStack.hpp"
#include "states/TutorialPromptState.hpp"
#include "tutorial/Tutorial.hpp"
#include "ui/UiDraw.hpp"

namespace cd {

DungeonResultState::DungeonResultState(StateStack& stack, AppContext& context,
                                       score::RunSummary summary, int score)
    : GameState(stack), context_(context), summary_(std::move(summary)), score_(score) {
    context_.fade.start();
    context_.audio.setMusic(MusicTrack::Result);
    context_.audio.setAmbience(AmbienceTrack::None);
}

void DungeonResultState::onEnter() {
    maybeTutorialPrompt(stack(), context_, tutorial::kResultFirst);
}

void DungeonResultState::handleInput(const Input& input) {
    if (input.pressed(InputAction::Details)) {
        stack().pushState(std::make_unique<DetailsOverlayState>(
            stack(), context_, "How Scoring Works", scoreDetailsText()));
        return;
    }
    if (input.pressed(InputAction::Confirm) || input.pressed(InputAction::Cancel)) {
        stack().popState();  // close the result
        stack().popState();  // leave the dungeon -> back to town
    }
}

void DungeonResultState::render() {
    const int w = context_.virtualWidth;
    const int h = context_.virtualHeight;
    ClearBackground(Color{14, 18, 22, 255});

    const int boxW = 300;
    // The panel grows with its line count (escapes and the M20 wager are
    // conditional) so the breakdown never crowds the footer (UI-LAYOUT-018).
    const int lineCount =
        6 + (summary_.escapes > 0 ? 1 : 0) + (summary_.wagerAccepted ? 1 : 0);
    const int boxH = std::max(190, 62 + lineCount * 13 + 34);
    const int boxX = w / 2 - boxW / 2;
    const int boxY = h / 2 - boxH / 2;
    ui::drawFramedPanel(context_.resources, boxX, boxY, boxW, boxH, Color{22, 30, 36, 245}, Color{120, 200, 140, 255});

    ui::drawTextCentered("Dungeon Cleared!", w / 2, boxY + 12, 18, Color{150, 230, 160, 255});
    ui::drawTextCentered(TextFormat("Score: %d", score_), w / 2, boxY + 38, 16, RAYWHITE);

    const score::ScoreBreakdown b = score::scoreBreakdown(summary_);
    // Pitch 13 keeps the maximum 8-line breakdown (escapes shown) clear of
    // the footer inside the 190px panel (audit UI-LAYOUT-018).
    int y = boxY + 62;
    const Color label{200, 200, 215, 255};
    auto line = [&](const char* text, int value, Color color) {
        ui::drawText(text, boxX + 22, y, 10, label);
        ui::drawText(TextFormat("%+d", value), boxX + boxW - 70, y, 10, color);
        y += 13;
    };
    const Color plus{150, 220, 150, 255};
    const Color minus{225, 150, 150, 255};
    line("Base + boss", b.base + b.bossBonus, plus);
    line(TextFormat("Battle turns (%d)", summary_.battleTurns), -b.turnPenalty, minus);
    line(TextFormat("Chests (%d)", summary_.chestsOpened), b.chestBonus, plus);
    line(TextFormat("Danger defeated (%d)", summary_.dangerDefeated), b.dangerBonus, plus);
    line("Treasure", b.treasureBonus, plus);
    line(summary_.noDeath ? "No-death bonus" : "No-death bonus (lost)", b.noDeathBonus, plus);
    if (summary_.escapes > 0) {
        line(TextFormat("Escapes (%d)", summary_.escapes), -b.escapePenalty, minus);
    }
    if (summary_.wagerAccepted) {
        line(b.wager >= 0 ? "Omen wager won" : "Omen wager lost", b.wager,
             b.wager >= 0 ? plus : minus);
    }

    ui::drawTextCentered((input::prompt(context_.input.map(), InputAction::Confirm,
                                        context_.input.activeDevice(), "Return to Town") +
                          "   " +
                          input::prompt(context_.input.map(), InputAction::Details,
                                        context_.input.activeDevice(), "Details"))
                             .c_str(),
                         w / 2, boxY + boxH - 16, 10, Color{200, 200, 160, 255});
}

}  // namespace cd
