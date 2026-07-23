#include "states/ConfirmPromptState.hpp"

#include <utility>
#include <vector>

#include "audio/AudioManager.hpp"
#include "core/AppContext.hpp"
#include "input/Input.hpp"
#include "raylib.h"
#include "states/StateStack.hpp"
#include "ui/TextLayout.hpp"
#include "ui/UiDraw.hpp"
#include "ui/UiStyle.hpp"

namespace cd {

namespace style = ui::style;

namespace {
constexpr int kPanelW = 260;
constexpr int kTextW = kPanelW - 2 * style::kPad;
constexpr int kRowH = 18;
constexpr int kConfirm = 0;
constexpr int kCancel = 1;
}  // namespace

ConfirmPromptState::ConfirmPromptState(StateStack& stack, AppContext& context, std::string title,
                                       std::string body, std::string confirmLabel,
                                       std::string cancelLabel, std::function<void()> onConfirm)
    : GameState(stack),
      context_(context),
      title_(std::move(title)),
      body_(std::move(body)),
      onConfirm_(std::move(onConfirm)) {
    menu_.setItems({{std::move(confirmLabel), true}, {std::move(cancelLabel), true}});
    menu_.setCursor(kCancel);  // never start on the destructive answer
}

void ConfirmPromptState::answerNo() {
    context_.audio.play(Sfx::Cancel);
    stack().popState();
}

void ConfirmPromptState::handleInput(const Input& input) {
    if (input.navPressed(InputAction::MoveUp)) {
        menu_.moveUp();
        context_.audio.play(Sfx::Move);
    }
    if (input.navPressed(InputAction::MoveDown)) {
        menu_.moveDown();
        context_.audio.play(Sfx::Move);
    }
    if (input.pressed(InputAction::Cancel)) {
        answerNo();
        return;
    }
    if (input.pressed(InputAction::Confirm)) {
        if (menu_.cursor() != kConfirm) {
            answerNo();
            return;
        }
        context_.audio.play(Sfx::Confirm);
        stack().popState();  // queued, so the action below decides what follows
        if (onConfirm_) {
            onConfirm_();
        }
    }
}

void ConfirmPromptState::render() {
    const int w = context_.virtualWidth;
    const int h = context_.virtualHeight;
    const style::Palette& p = style::palette();
    ui::drawModalDim(w, h);  // dim the frozen scene

    // Measured height: title + wrapped body + divider + the two answers.
    const std::vector<std::string> lines =
        ui::wrapText(body_, kTextW, style::kFontBody, ui::raylibMeasure());
    const int bodyH = static_cast<int>(lines.size()) * ui::lineHeight(style::kFontBody);
    const int panelH =
        style::kPad + style::kFontHeading + 8 + bodyH + 14 + 2 * kRowH + style::kPad;
    const int x = (w - kPanelW) / 2;
    const int y = (h - panelH) / 2;

    // Destructive decision: the Danger frame with its warning tab.
    ui::drawFrame(x, y, kPanelW, panelH, ui::FrameStyle::Danger);
    int ty = y + style::kPad;
    ui::drawTextCentered(title_.c_str(), x + kPanelW / 2, ty, style::kFontHeading, p.text);
    ty += style::kFontHeading + 8;
    ui::drawTextWrapped(body_, x + style::kPad, ty, kTextW, style::kFontBody, p.dangerText,
                        "confirm.body");
    ty += bodyH + 6;
    ui::drawDivider(x + 10, ty, kPanelW - 20);
    ty += 8;
    ui::drawMenu(menu_, x + style::kPad + 20, ty, kRowH, style::kFontBody, p.text, p.disabled,
                 p.cursor);
}

}  // namespace cd
