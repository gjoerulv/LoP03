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
    DrawRectangle(0, 0, w, h, Color{0, 0, 0, 150});  // dim the frozen scene

    // Measured height: title + wrapped body + the two answers.
    const std::vector<std::string> lines =
        ui::wrapText(body_, kTextW, style::kFontBody, ui::raylibMeasure());
    const int bodyH = static_cast<int>(lines.size()) * ui::lineHeight(style::kFontBody);
    const int panelH = style::kPad + style::kFontHeading + 8 + bodyH + 10 + 2 * kRowH + style::kPad;
    const int x = (w - kPanelW) / 2;
    const int y = (h - panelH) / 2;

    ui::drawFramedPanel(context_.resources, x, y, kPanelW, panelH, Color{28, 26, 48, 245},
                        Color{120, 120, 200, 255});
    int ty = y + style::kPad;
    ui::drawTextCentered(title_.c_str(), x + kPanelW / 2, ty, style::kFontHeading,
                         style::palette().cursor);
    ty += style::kFontHeading + 8;
    ui::drawTextWrapped(body_, x + style::kPad, ty, kTextW, style::kFontBody,
                        style::palette().dangerText, "confirm.body");
    ty += bodyH + 10;
    ui::drawMenu(menu_, x + style::kPad + 20, ty, kRowH, style::kFontBody, RAYWHITE,
                 style::palette().disabled, style::palette().cursor);
}

}  // namespace cd
