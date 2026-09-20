#include "states/CreditsState.hpp"

#include <algorithm>

#include "audio/AudioManager.hpp"
#include "core/AppContext.hpp"
#include "core/Licenses.hpp"
#include "game/Credits.hpp"
#include "input/Input.hpp"
#include "input/PromptLabels.hpp"
#include "raylib.h"
#include "states/StateStack.hpp"
#include "ui/UiDraw.hpp"
#include "ui/UiStyle.hpp"

namespace cd {

namespace style = ui::style;

namespace {
constexpr int kFrameX = 12;
constexpr int kCreditY = ui::kHeaderBandH + 6;
constexpr int kFrameY = kCreditY + style::kFontHeading + 8;
}  // namespace

CreditsState::CreditsState(StateStack& stack, AppContext& context)
    : GameState(stack), context_(context), body_(credits::bodyFromLicenses(licenses::kText)) {}

void CreditsState::handleInput(const Input& input) {
    if (input.navPressed(InputAction::MoveUp) && bodyView_.scrollBy(-1)) {
        context_.audio.play(Sfx::Move);
    }
    if (input.navPressed(InputAction::MoveDown) && bodyView_.scrollBy(1)) {
        context_.audio.play(Sfx::Move);
    }
    if (input.pressed(InputAction::Cancel) || input.pressed(InputAction::Confirm)) {
        context_.audio.play(Sfx::Cancel);
        stack().popState();
    }
}

void CreditsState::render() {
    const int w = context_.virtualWidth;
    const int h = context_.virtualHeight;
    const style::Palette& p = style::palette();
    ClearBackground(p.canvas);
    ui::drawHeaderBand("Credits & Licenses", w, p.gold);

    // The credit: its own line, flanked by the title screen's crystal pips.
    const int creditW = ui::measureText(credits::kCreditLine, style::kFontHeading);
    ui::drawTextCentered(credits::kCreditLine, w / 2, kCreditY, style::kFontHeading, p.gold);
    ui::drawCrystalPip(w / 2 - creditW / 2 - 12, kCreditY + 4);
    ui::drawCrystalPip(w / 2 + creditW / 2 + 9, kCreditY + 4);

    // The license prose: one inset well down to the footer strip, the text a
    // scrollable viewport inside it (M87 policy C) — wrapped once and cached.
    const int frameW = w - 2 * kFrameX;
    const int frameH = h - style::kFooterHeight - 6 - kFrameY;
    ui::drawFrame(kFrameX, kFrameY, frameW, frameH, ui::FrameStyle::Inset);
    const int textW = frameW - 2 * style::kPad - ui::kScrollGutterW;
    bodyView_.setContent(body_, textW, style::kFontBody, ui::raylibMeasure());
    const int capLines =
        std::max(1, (frameH - 2 * style::kPadSmall) / ui::lineHeight(style::kFontBody));
    bodyView_.setVisibleLines(capLines);
#ifdef CRYSTAL_CAPTURE
    if (pendingCaptureScroll_ != 0) {
        bodyView_.scrollBy(pendingCaptureScroll_);
        pendingCaptureScroll_ = 0;
    }
#endif
    ui::drawTextViewport(bodyView_, kFrameX + style::kPad, kFrameY + style::kPadSmall, p.text);

    const InputMap& map = context_.input.map();
    const ActiveDevice device = context_.input.activeDevice();
    ui::drawFooterHints({{input::primaryLabel(map, InputAction::MoveUp, device) + "/" +
                              input::primaryLabel(map, InputAction::MoveDown, device),
                          "Scroll"},
                         {input::primaryLabel(map, InputAction::Cancel, device), "Back"}},
                        w, h, "credits.footer");
}

}  // namespace cd
