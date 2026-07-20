#include "states/ScoreboardState.hpp"

#include <memory>

#include "battle/Battle.hpp"
#include "core/AppContext.hpp"
#include "input/Input.hpp"
#include "input/PromptLabels.hpp"
#include "raylib.h"
#include "score/Scoreboard.hpp"
#include "states/DetailsOverlayState.hpp"
#include "states/ScoreDetailsText.hpp"
#include "states/StateStack.hpp"
#include "ui/UiDraw.hpp"
#include "ui/UiStyle.hpp"

namespace cd {

namespace style = ui::style;

namespace {
// Column layout: numeric columns are right-aligned at fixed edges (space
// padding cannot align a variable-width font); the theme fills the rest.
constexpr int kRankX = 40;       // left edge, "#"
constexpr int kScoreR = 126;     // right edge of Score
constexpr int kTurnsR = 178;     // right edge of Turns
constexpr int kDangerR = 230;    // right edge of Danger
constexpr int kDepthR = 270;     // right edge of Depth
constexpr int kLvR = 304;        // right edge of Lv (M19 comparability tag)
constexpr int kThemeX = 318;     // left edge of Theme
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
    if (input.pressed(InputAction::Details)) {
        stack().pushState(std::make_unique<DetailsOverlayState>(
            stack(), context_, "How Scoring Works", scoreDetailsText()));
        return;
    }
    if (input.pressed(InputAction::Cancel) || input.pressed(InputAction::Confirm)) {
        stack().popState();
    }
}

void ScoreboardState::render() {
    const int w = context_.virtualWidth;
    const int h = context_.virtualHeight;
    ui::drawSceneBackground(context_.resources, "bg.scoreboard", Color{16, 16, 24, 255},
                            context_.virtualWidth, context_.virtualHeight);
    ui::drawTextCentered("Scoreboard", w / 2, 16, style::kFontScreenTitle, style::palette().text);

    const auto& entries = context_.scoreboard.entries();
    if (entries.empty()) {
        ui::drawTextWrapped("No runs recorded yet. Clear a dungeon to set a score!", 60, h / 2,
                            w - 120, style::kFontBody, style::palette().textDim, "scoreboard.empty", 2);
        ui::drawTextCentered(input::prompt(context_.input.map(), InputAction::Cancel,
                                           context_.input.activeDevice(), "Back")
                                 .c_str(),
                             w / 2, h - style::kFooterHeight + 2, style::kFontBody,
                             style::palette().textHint);
        return;
    }

    // Header.
    ui::drawText("#", kRankX, kHeaderY, style::kFontBody, style::palette().textDim);
    ui::drawTextRight("Score", kScoreR, kHeaderY, style::kFontBody, style::palette().textDim);
    ui::drawTextRight("Turns", kTurnsR, kHeaderY, style::kFontBody, style::palette().textDim);
    ui::drawTextRight("Danger", kDangerR, kHeaderY, style::kFontBody, style::palette().textDim);
    ui::drawTextRight("Depth", kDepthR, kHeaderY, style::kFontBody, style::palette().textDim);
    ui::drawTextRight("Lv", kLvR, kHeaderY, style::kFontBody, style::palette().textDim);
    ui::drawText("Theme", kThemeX, kHeaderY, style::kFontBody, style::palette().textDim);

    const int total = static_cast<int>(entries.size());
    const int first = scroll_.top();
    const int count = scroll_.visibleCount(total, kVisibleRows);
    for (int row = 0; row < count; ++row) {
        const int i = first + row;
        const score::ScoreEntry& e = entries[static_cast<std::size_t>(i)];
        const int y = kRowsY + row * kRowH;
        const Color rowColor = e.noDeath ? Color{210, 230, 200, 255} : style::palette().text;
        ui::drawText(TextFormat("%d", i + 1), kRankX, y, style::kFontBody, rowColor);
        ui::drawTextRight(TextFormat("%d", e.score), kScoreR, y, style::kFontBody, rowColor);
        ui::drawTextRight(TextFormat("%d", e.battleTurns), kTurnsR, y, style::kFontBody,
                          rowColor);
        ui::drawTextRight(TextFormat("%d", e.dangerDefeated), kDangerR, y, style::kFontBody,
                          rowColor);
        ui::drawTextRight(TextFormat("%d", e.depth), kDepthR, y, style::kFontBody, rowColor);
        // Party level at completion; legacy entries (pre-M19) show "-".
        if (e.partyLevel > 0) {
            ui::drawTextRight(TextFormat("%d", e.partyLevel), kLvR, y, style::kFontBody,
                              rowColor);
        } else {
            ui::drawTextRight("-", kLvR, y, style::kFontBody, style::palette().textDim);
        }
        std::string theme = e.theme;
        if (e.noDeath) {
            theme += "  *";
        }
        // Runs played under older battle rules (pre-M28 enmity/AI) are flagged
        // so they are visibly distinguished, never silently ranked as equal.
        if (e.battleRulesVersion < battle::kBattleRulesVersion) {
            theme += " ~";
        }
        ui::drawTextFitted(theme, kThemeX, y, w - kThemeX - 16, style::kFontBody, rowColor,
                           "scoreboard.theme");
    }

    // The honest-comparison conditions (M19 policy): no hidden normalization;
    // players compare runs at matching conditions instead.
    const int legendY = kRowsY + count * kRowH + 4;
    ui::drawText("* = no-death   Lv = party level ('-' = older run)", kRankX, legendY,
             style::kFontSmall, Color{150, 170, 150, 255});
    ui::drawText("Compare at same Depth/Lv.  ~ = older battle rules (pre-M28).", kRankX,
                 legendY + 10, style::kFontSmall, style::palette().textDim);
    if (scroll_.moreAbove() || scroll_.moreBelow(total, kVisibleRows)) {
        ui::drawTextRight(TextFormat("%d-%d of %d   Up/Down: Scroll", first + 1, first + count,
                                     total),
                          w - 16, legendY, style::kFontSmall, style::palette().textDim);
    }

    ui::drawTextCentered((input::prompt(context_.input.map(), InputAction::Details,
                                        context_.input.activeDevice(), "How scoring works") +
                          "    " +
                          input::prompt(context_.input.map(), InputAction::Cancel,
                                        context_.input.activeDevice(), "Back"))
                             .c_str(),
                         w / 2, h - style::kFooterHeight + 2, style::kFontBody,
                         style::palette().textHint);
}

}  // namespace cd
