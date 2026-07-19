#include "states/ScoreboardState.hpp"

#include "core/AppContext.hpp"
#include "input/Input.hpp"
#include "input/PromptLabels.hpp"
#include "raylib.h"
#include "score/Scoreboard.hpp"
#include "states/StateStack.hpp"
#include "ui/UiDraw.hpp"
#include "ui/UiStyle.hpp"

namespace cd {

namespace style = ui::style;

namespace {
// Column layout: numeric columns are right-aligned at fixed edges (space
// padding cannot align a variable-width font); the theme fills the rest.
constexpr int kRankX = 40;       // left edge, "#"
constexpr int kScoreR = 130;     // right edge of Score
constexpr int kTurnsR = 185;     // right edge of Turns
constexpr int kDangerR = 245;    // right edge of Danger
constexpr int kDepthR = 290;     // right edge of Depth
constexpr int kThemeX = 306;     // left edge of Theme
constexpr int kHeaderY = 44;
constexpr int kRowsY = 60;
constexpr int kRowH = 13;
constexpr int kVisibleRows = 11;
}  // namespace

ScoreboardState::ScoreboardState(StateStack& stack, AppContext& context)
    : GameState(stack), context_(context) {}

void ScoreboardState::handleInput(const Input& input) {
    const int total = static_cast<int>(context_.scoreboard.entries().size());
    if (input.navPressed(InputAction::MoveUp)) {
        scroll_.scrollBy(total, kVisibleRows, -1);
    }
    if (input.navPressed(InputAction::MoveDown)) {
        scroll_.scrollBy(total, kVisibleRows, 1);
    }
    if (input.pressed(InputAction::Cancel) || input.pressed(InputAction::Confirm)) {
        stack().popState();
    }
}

void ScoreboardState::render() {
    const int w = context_.virtualWidth;
    const int h = context_.virtualHeight;
    ClearBackground(Color{16, 16, 24, 255});
    ui::drawTextCentered("Scoreboard", w / 2, 16, style::kFontScreenTitle, style::kText);

    const auto& entries = context_.scoreboard.entries();
    if (entries.empty()) {
        ui::drawTextWrapped("No runs recorded yet. Clear a dungeon to set a score!", 60, h / 2,
                            w - 120, style::kFontBody, style::kTextDim, "scoreboard.empty", 2);
        ui::drawTextCentered(input::prompt(context_.input.map(), InputAction::Cancel,
                                           context_.input.activeDevice(), "Back")
                                 .c_str(),
                             w / 2, h - style::kFooterHeight + 2, style::kFontBody,
                             style::kTextHint);
        return;
    }

    // Header.
    DrawText("#", kRankX, kHeaderY, style::kFontBody, style::kTextDim);
    ui::drawTextRight("Score", kScoreR, kHeaderY, style::kFontBody, style::kTextDim);
    ui::drawTextRight("Turns", kTurnsR, kHeaderY, style::kFontBody, style::kTextDim);
    ui::drawTextRight("Danger", kDangerR, kHeaderY, style::kFontBody, style::kTextDim);
    ui::drawTextRight("Depth", kDepthR, kHeaderY, style::kFontBody, style::kTextDim);
    DrawText("Theme", kThemeX, kHeaderY, style::kFontBody, style::kTextDim);

    const int total = static_cast<int>(entries.size());
    const int first = scroll_.top();
    const int count = scroll_.visibleCount(total, kVisibleRows);
    for (int row = 0; row < count; ++row) {
        const int i = first + row;
        const score::ScoreEntry& e = entries[static_cast<std::size_t>(i)];
        const int y = kRowsY + row * kRowH;
        const Color rowColor = e.noDeath ? Color{210, 230, 200, 255} : style::kText;
        DrawText(TextFormat("%d", i + 1), kRankX, y, style::kFontBody, rowColor);
        ui::drawTextRight(TextFormat("%d", e.score), kScoreR, y, style::kFontBody, rowColor);
        ui::drawTextRight(TextFormat("%d", e.battleTurns), kTurnsR, y, style::kFontBody,
                          rowColor);
        ui::drawTextRight(TextFormat("%d", e.dangerDefeated), kDangerR, y, style::kFontBody,
                          rowColor);
        ui::drawTextRight(TextFormat("%d", e.depth), kDepthR, y, style::kFontBody, rowColor);
        std::string theme = e.theme + (e.noDeath ? "  *" : "");
        ui::drawTextFitted(theme, kThemeX, y, w - kThemeX - 16, style::kFontBody, rowColor,
                           "scoreboard.theme");
    }

    const int legendY = kRowsY + count * kRowH + 4;
    DrawText("* = no-death run", kRankX, legendY, style::kFontSmall,
             Color{150, 170, 150, 255});
    if (scroll_.moreAbove() || scroll_.moreBelow(total, kVisibleRows)) {
        ui::drawTextRight(TextFormat("%d-%d of %d   Up/Down: Scroll", first + 1, first + count,
                                     total),
                          w - 16, legendY, style::kFontSmall, style::kTextDim);
    }

    ui::drawTextCentered(input::prompt(context_.input.map(), InputAction::Cancel,
                                       context_.input.activeDevice(), "Back")
                             .c_str(),
                         w / 2, h - style::kFooterHeight + 2, style::kFontBody,
                         style::kTextHint);
}

}  // namespace cd
