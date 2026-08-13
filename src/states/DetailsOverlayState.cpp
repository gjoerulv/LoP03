#include "states/DetailsOverlayState.hpp"

#include <algorithm>
#include <utility>

#include "audio/AudioManager.hpp"
#include "core/AppContext.hpp"
#include "input/Input.hpp"
#include "input/PromptLabels.hpp"
#include "raylib.h"
#include "states/StateStack.hpp"
#include "ui/UiDraw.hpp"
#include "ui/UiStyle.hpp"

namespace cd {

namespace style = ui::style;

namespace {
constexpr int kPanelW = 360;
// Prose wraps to the panel's inner width minus the indicator gutter.
constexpr int kTextW = kPanelW - 2 * style::kPad - ui::kScrollGutterW;
}  // namespace

DetailsOverlayState::DetailsOverlayState(StateStack& stack, AppContext& context,
                                         std::string title, std::string body)
    : GameState(stack), context_(context), title_(std::move(title)), body_(std::move(body)) {}

void DetailsOverlayState::handleInput(const Input& input) {
    if (input.navPressed(InputAction::MoveUp) && bodyView_.scrollBy(-1)) {
        context_.audio.play(Sfx::Move);
    }
    if (input.navPressed(InputAction::MoveDown) && bodyView_.scrollBy(1)) {
        context_.audio.play(Sfx::Move);
    }
    if (input.pressed(InputAction::Confirm) || input.pressed(InputAction::Cancel) ||
        input.pressed(InputAction::Details)) {
        context_.audio.play(Sfx::Cancel);
        stack().popState();
    }
}

void DetailsOverlayState::render() {
    const int w = context_.virtualWidth;
    const int h = context_.virtualHeight;
    ui::drawModalDim(w, h);

    // The body viewport: wrapped once (cached), capped by the safe area —
    // the panel owns its height; the text scrolls (M87 policy C).
    bodyView_.setContent(body_, kTextW, style::kFontBody, ui::raylibMeasure());
    const int maxBodyH = h - 2 * style::kSafeMargin - style::kPad * 2 -
                         style::kFontHeading - 6 - style::kFontSmall - 4;
    const int capLines = std::max(1, maxBodyH / ui::lineHeight(style::kFontBody));
    bodyView_.setVisibleLines(std::clamp(bodyView_.lineCount(), 1, capLines));
#ifdef CRYSTAL_CAPTURE
    if (pendingCaptureScroll_ != 0) {
        bodyView_.scrollBy(pendingCaptureScroll_);
        pendingCaptureScroll_ = 0;
    }
#endif
    const int bodyH = bodyView_.visibleLines() * ui::lineHeight(style::kFontBody);
    const int panelH = style::kPad + style::kFontHeading + 6 + bodyH + 4 +
                       style::kFontSmall + style::kPad;
    const int x = (w - kPanelW) / 2;
    const int y = (h - panelH) / 2;

    ui::drawFrame(x, y, kPanelW, panelH, ui::FrameStyle::Raised);
    int ty = y + style::kPad;
    ui::drawTextCentered(title_.c_str(), x + kPanelW / 2, ty, style::kFontHeading,
                         style::palette().text);
    ty += style::kFontHeading + 6;
    ty = ui::drawTextViewport(bodyView_, x + style::kPad, ty, style::palette().text);
    ty += 4;
    const InputMap& map = context_.input.map();
    const ActiveDevice device = context_.input.activeDevice();
    std::string hint = input::prompt(map, InputAction::Confirm, device, "Close");
    if (bodyView_.scrollable()) {
        hint = input::primaryLabel(map, InputAction::MoveUp, device) + "/" +
               input::primaryLabel(map, InputAction::MoveDown, device) + " Scroll   " + hint;
    }
    ui::drawTextCentered(hint.c_str(), x + kPanelW / 2, ty, style::kFontSmall,
                         style::palette().textHint);
}

}  // namespace cd
