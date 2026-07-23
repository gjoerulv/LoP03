#include "states/BattleLogState.hpp"

#include <utility>

#include "audio/AudioManager.hpp"
#include "core/AppContext.hpp"
#include "input/Input.hpp"
#include "input/PromptLabels.hpp"
#include "raylib.h"
#include "states/BattleLog.hpp"
#include "states/StateStack.hpp"
#include "ui/UiDraw.hpp"
#include "ui/UiStyle.hpp"

namespace cd {

namespace style = ui::style;

namespace {
constexpr int kPanelW = 340;
constexpr int kVisibleRows = 12;
constexpr int kTextW = kPanelW - 2 * style::kPad;
}  // namespace

BattleLogState::BattleLogState(StateStack& stack, AppContext& context, const BattleLog& log)
    : GameState(stack), context_(context) {
    // Snapshot: wrap every entry once at panel width (oldest -> newest). The
    // battle is frozen while this overlay is up, so one wrap is enough.
    for (const std::string& entry : log.entries()) {
        std::vector<std::string> wrapped =
            ui::wrapText(entry, kTextW, style::kFontBody, ui::raylibMeasure());
        for (std::string& line : wrapped) {
            lines_.push_back(std::move(line));
        }
    }
    // Open at the newest lines (bottom) — the just-happened action.
    scroll_.scrollBy(static_cast<int>(lines_.size()), kVisibleRows,
                     static_cast<int>(lines_.size()));
}

#ifdef CRYSTAL_CAPTURE
void BattleLogState::captureScrollUp(int rows) {
    scroll_.scrollBy(static_cast<int>(lines_.size()), kVisibleRows, -rows);
}
#endif

void BattleLogState::handleInput(const Input& input) {
    const int total = static_cast<int>(lines_.size());
    if (input.navPressed(InputAction::MoveUp)) {
        scroll_.scrollBy(total, kVisibleRows, -1);
    }
    if (input.navPressed(InputAction::MoveDown)) {
        scroll_.scrollBy(total, kVisibleRows, 1);
    }
    // Menu opens AND closes the log (owner decision); Cancel also closes.
    if (input.pressed(InputAction::Menu) || input.pressed(InputAction::Cancel)) {
        context_.audio.play(Sfx::Cancel);
        stack().popState();
    }
}

void BattleLogState::render() {
    const int w = context_.virtualWidth;
    const int h = context_.virtualHeight;
    const style::Palette& p = style::palette();
    ui::drawModalDim(w, h);

    const int listH = kVisibleRows * ui::lineHeight(style::kFontBody);
    const int panelH = style::kPad + style::kFontHeading + 6 + listH + 6 + style::kFontSmall +
                       style::kPad;
    const int x = (w - kPanelW) / 2;
    const int y = (h - panelH) / 2;
    ui::drawFrame(x, y, kPanelW, panelH, ui::FrameStyle::Raised);

    int ty = y + style::kPad;
    ui::drawTextCentered("Battle Log", x + kPanelW / 2, ty, style::kFontHeading, p.text);
    ty += style::kFontHeading + 6;

    const int listX = x + style::kPad;
    const int total = static_cast<int>(lines_.size());
    if (total == 0) {
        ui::drawTextCentered("No actions yet.", x + kPanelW / 2, ty + listH / 2 - 4,
                             style::kFontBody, p.textDim);
    } else {
        const int first = scroll_.top();
        const int count = scroll_.visibleCount(total, kVisibleRows);
        for (int row = 0; row < count; ++row) {
            const int ly = ty + row * ui::lineHeight(style::kFontBody);
            ui::drawTextFitted(lines_[static_cast<std::size_t>(first + row)], listX, ly, kTextW,
                               style::kFontBody, p.text, "battlelog.line");
        }
        // More-above / below markers (shape signal, not color): a small caret at
        // the panel's right edge whenever content is hidden that way.
        const int rightX = x + kPanelW - style::kPad - 6;
        if (scroll_.moreAbove()) {
            ui::drawTextCentered("^", rightX, ty - 1, style::kFontSmall, p.textHint);
        }
        if (scroll_.moreBelow(total, kVisibleRows)) {
            ui::drawTextCentered("v", rightX, ty + listH - style::kFontSmall - 1,
                                 style::kFontSmall, p.textHint);
        }
    }

    const InputMap& map = context_.input.map();
    const ActiveDevice device = context_.input.activeDevice();
    const std::string scrollHint = input::primaryLabel(map, InputAction::MoveUp, device) + "/" +
                                   input::primaryLabel(map, InputAction::MoveDown, device);
    const std::string closeHint = input::primaryLabel(map, InputAction::Menu, device);
    const std::string hint = scrollHint + " Scroll     " + closeHint + " Close";
    ui::drawTextCentered(hint.c_str(), x + kPanelW / 2, y + panelH - style::kPad - style::kFontSmall,
                         style::kFontSmall, p.textHint);
}

}  // namespace cd
