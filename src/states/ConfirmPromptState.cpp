#include "states/ConfirmPromptState.hpp"

#include <cstddef>
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
}  // namespace

ConfirmPromptState::ConfirmPromptState(StateStack& stack, AppContext& context, std::string title,
                                       std::string body, std::vector<Answer> answers,
                                       int safeIndex)
    : GameState(stack),
      context_(context),
      title_(std::move(title)),
      body_(std::move(body)),
      answers_(std::move(answers)),
      safeIndex_(safeIndex) {
    std::vector<ui::MenuItem> items;
    items.reserve(answers_.size());
    for (const Answer& a : answers_) {
        items.push_back({a.label, true});
    }
    menu_.setItems(std::move(items));
    const int count = static_cast<int>(answers_.size());
    if (safeIndex_ < 0 || safeIndex_ >= count) {
        safeIndex_ = count > 0 ? count - 1 : 0;  // defensive: the last row is the way out
    }
    menu_.setCursor(safeIndex_);  // never start on a destructive answer
}

ConfirmPromptState::ConfirmPromptState(StateStack& stack, AppContext& context, std::string title,
                                       std::string body, std::string confirmLabel,
                                       std::string cancelLabel, std::function<void()> onConfirm)
    : ConfirmPromptState(stack, context, std::move(title), std::move(body),
                         std::vector<Answer>{{std::move(confirmLabel), std::move(onConfirm)},
                                             {std::move(cancelLabel), nullptr}},
                         1) {}

void ConfirmPromptState::answerSafe() {
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
        answerSafe();
        return;
    }
    if (input.pressed(InputAction::Confirm)) {
        const int cursor = menu_.cursor();
        if (cursor < 0 || cursor >= static_cast<int>(answers_.size()) ||
            !answers_[static_cast<std::size_t>(cursor)].onChoose) {
            answerSafe();  // the safe row (or any row with nothing to do) dismisses
            return;
        }
        context_.audio.play(Sfx::Confirm);
        stack().popState();  // queued, so the action below decides what follows
        answers_[static_cast<std::size_t>(cursor)].onChoose();
    }
}

void ConfirmPromptState::render() {
    const int w = context_.virtualWidth;
    const int h = context_.virtualHeight;
    const style::Palette& p = style::palette();
    ui::drawModalDim(w, h);  // dim the frozen scene

    // Measured height: title + wrapped body + divider + every answer row.
    const std::vector<std::string> lines =
        ui::wrapText(body_, kTextW, style::kFontBody, ui::raylibMeasure());
    const int bodyH = static_cast<int>(lines.size()) * ui::lineHeight(style::kFontBody);
    const int rows = static_cast<int>(answers_.size());
    const int panelH =
        style::kPad + style::kFontHeading + 8 + bodyH + 14 + rows * kRowH + style::kPad;
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
