#include "states/ScoreboardState.hpp"

#include <memory>

#include "battle/Battle.hpp"
#include "core/AppContext.hpp"
#include "game/Party.hpp"
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
// padding cannot align a variable-width font); the theme fills the rest. The
// block was shifted ~16px left in M32 to widen the theme column so the town tag
// (T#) fits alongside the longest theme + no-death marker.
constexpr int kRankX = 24;       // left edge, "#"
constexpr int kScoreR = 110;     // right edge of Score
constexpr int kTurnsR = 162;     // right edge of Turns
constexpr int kDangerR = 214;    // right edge of Danger
constexpr int kDepthR = 254;     // right edge of Depth
constexpr int kLvR = 288;        // right edge of Lv (M19 comparability tag)
constexpr int kThemeX = 296;     // left edge of Theme (M35: -6px to fit the T# + theme + * + ~ case)
constexpr int kHeaderY = 44;
constexpr int kRowsY = 60;
constexpr int kRowH = 13;
constexpr int kVisibleRows = 10;
}  // namespace

ScoreboardState::ScoreboardState(StateStack& stack, AppContext& context)
    : GameState(stack), context_(context) {
    rebuildBoard();
}

void ScoreboardState::rebuildBoard() {
    // M82: the board shows only its shape's runs; ranking within a board is
    // the scoreboard's own order (entries() is already ranked).
    visible_.clear();
    const auto& entries = context_.scoreboard.entries();
    for (std::size_t i = 0; i < entries.size(); ++i) {
        if (score::onFloorsBoard(entries[i], boardFloors_)) {
            visible_.push_back(static_cast<int>(i));
        }
    }
    scroll_.reset();
}

#ifdef CRYSTAL_CAPTURE
void ScoreboardState::captureShowFourFloorBoard() {
    boardFloors_ = 4;
    rebuildBoard();
}
#endif

void ScoreboardState::handleInput(const Input& input) {
    const int total = static_cast<int>(visible_.size());
    if (input.navPressed(InputAction::MoveUp)) {
        scroll_.scrollBy(total, kVisibleRows, -1);
    }
    if (input.navPressed(InputAction::MoveDown)) {
        scroll_.scrollBy(total, kVisibleRows, 1);
    }
    // M82/M92: the M79 cycle pair walks the three boards (1 -> 4 -> 20).
    if (input.pressed(InputAction::CycleNext)) {
        boardFloors_ = boardFloors_ == 1 ? 4 : (boardFloors_ == 4 ? 20 : 1);
        rebuildBoard();
    } else if (input.pressed(InputAction::CyclePrev)) {
        boardFloors_ = boardFloors_ == 1 ? 20 : (boardFloors_ == 20 ? 4 : 1);
        rebuildBoard();
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
    const style::Palette& pal = style::palette();
    ui::drawSceneBackground(context_.resources, "bg.scoreboard", pal.canvas,
                            context_.virtualWidth, context_.virtualHeight, context_.party.currentTown);
    ui::drawHeaderBand("Scoreboard", w, pal.gold);
    // M82: which board is showing, as a centered chip under the band. Both
    // boards always exist; the cycle hint lives in the footer.
    {
        const char* label = boardFloors_ == 1
                                ? "1-Floor Runs"
                                : (boardFloors_ == 4 ? "4-Floor Runs" : "20-Floor Runs");
        const int chipW = ui::measureText(label, style::kFontSmall) + 12;
        ui::drawChip(label, w / 2 - chipW / 2, 26,
                     boardFloors_ == 1 ? pal.gold
                                       : (boardFloors_ == 4 ? pal.crystal : pal.danger));
    }

    const auto& entries = context_.scoreboard.entries();
    if (visible_.empty()) {
        const char* empty =
            boardFloors_ == 1
                ? "No runs recorded yet. Clear a dungeon to set a score!"
                : (boardFloors_ == 4
                       ? "No 4-floor runs yet. Choose Floors: 4 at the Guild and descend!"
                       : "No 20-floor runs yet. Choose Floors: 20 at the Guild and "
                         "brave the long descent!");
        ui::drawTextWrapped(empty, 60, h / 2, w - 120, style::kFontBody, pal.textDim,
                            "scoreboard.empty", 2);
        const InputMap& emap = context_.input.map();
        const ActiveDevice edev = context_.input.activeDevice();
        ui::drawFooterHints({{input::primaryLabel(emap, InputAction::CyclePrev, edev) + "/" +
                                  input::primaryLabel(emap, InputAction::CycleNext, edev),
                              "Board"},
                             {input::primaryLabel(emap, InputAction::Cancel, edev), "Back"}},
                            w, h, "scoreboard.footer");
        return;
    }

    // Ranked table in one inset well; header row, then the entries.
    ui::drawFrame(12, 36, w - 24, 16 + kVisibleRows * kRowH + 10, ui::FrameStyle::Inset);
    ui::drawText("#", kRankX, kHeaderY, style::kFontBody, style::palette().textDim);
    ui::drawTextRight("Score", kScoreR, kHeaderY, style::kFontBody, style::palette().textDim);
    ui::drawTextRight("Turns", kTurnsR, kHeaderY, style::kFontBody, style::palette().textDim);
    ui::drawTextRight("Danger", kDangerR, kHeaderY, style::kFontBody, style::palette().textDim);
    ui::drawTextRight("Depth", kDepthR, kHeaderY, style::kFontBody, style::palette().textDim);
    ui::drawTextRight("Lv", kLvR, kHeaderY, style::kFontBody, style::palette().textDim);
    ui::drawText("Theme", kThemeX, kHeaderY, style::kFontBody, style::palette().textDim);

    const int total = static_cast<int>(visible_.size());
    const int first = scroll_.top();
    const int count = scroll_.visibleCount(total, kVisibleRows);
    for (int row = 0; row < count; ++row) {
        const int i = first + row;
        const score::ScoreEntry& e =
            entries[static_cast<std::size_t>(visible_[static_cast<std::size_t>(i)])];
        const int y = kRowsY + row * kRowH;
        const Color rowColor = e.noDeath ? pal.success : pal.text;
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
        // Town-ladder tag (M32): shown only for town >= 2, so town-1 and legacy
        // (townIndex 0) rows read exactly as before. Fits the flexible column.
        if (e.townIndex >= 2) {
            theme = "T" + std::to_string(e.townIndex) + " " + theme;
        }
        if (e.noDeath) {
            theme += "  *";
        }
        // Runs played under older battle rules (pre-M28 enmity/AI) are flagged
        // so they are visibly distinguished, never silently ranked as equal.
        if (e.battleRulesVersion < battle::kBattleRulesVersion) {
            theme += " ~";
        }
        ui::drawTextFitted(theme, kThemeX, y, w - kThemeX - 14, style::kFontBody, rowColor,
                           "scoreboard.theme");
    }

    // The honest-comparison conditions (M19 policy): no hidden normalization;
    // players compare runs at matching conditions instead.
    const int legendY = kRowsY + count * kRowH + 8;
    ui::drawText("* = no-death   Lv = party level   T# = town", kRankX, legendY,
             style::kFontSmall, pal.success);
    ui::drawText("Compare at same Depth/Lv.  ~ = older battle rules.", kRankX,
                 legendY + 10, style::kFontSmall, pal.textDim);
    if (scroll_.moreAbove() || scroll_.moreBelow(total, kVisibleRows)) {
        ui::drawTextRight(TextFormat("%d-%d of %d   Up/Down: Scroll", first + 1, first + count,
                                     total),
                          w - 16, legendY, style::kFontSmall, style::palette().textDim);
    }

    const InputMap& map = context_.input.map();
    const ActiveDevice device = context_.input.activeDevice();
    ui::drawFooterHints(
        {{input::primaryLabel(map, InputAction::CyclePrev, device) + "/" +
              input::primaryLabel(map, InputAction::CycleNext, device),
          "Board"},  // M82
         {input::primaryLabel(map, InputAction::Details, device), "How scoring works"},
         {input::primaryLabel(map, InputAction::Cancel, device), "Back"}},
        w, h, "scoreboard.footer");
}

}  // namespace cd
