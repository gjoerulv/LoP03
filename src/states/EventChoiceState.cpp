#include "states/EventChoiceState.hpp"

#include <algorithm>
#include <utility>

#include "audio/AudioManager.hpp"
#include "core/AppContext.hpp"
#include "input/Input.hpp"
#include "input/PromptLabels.hpp"
#include "states/StateStack.hpp"
#include "ui/UiDraw.hpp"
#include "ui/UiStyle.hpp"

namespace cd {

namespace style = ui::style;

namespace {
// Owner fix 2026-08-17: the box's row area is a fixed window — nine rows keep
// the tallest modal (56 + a second title line + 9x16 + margins) on the 240px
// screen; longer lists (the Sacrifice over a deep bag) scroll behind it.
constexpr int kMaxVisibleRows = 9;
}  // namespace

EventChoiceState::EventChoiceState(StateStack& stack, AppContext& context, std::string title,
                                   std::vector<std::string> rows,
                                   std::function<void(int)> onPick)
    : GameState(stack), context_(context), title_(std::move(title)), onPick_(std::move(onPick)) {
    std::vector<ui::MenuItem> items;
    for (std::string& r : rows) {
        items.push_back({std::move(r), true});
    }
    menu_.setItems(std::move(items));
}

void EventChoiceState::handleInput(const Input& input) {
    if (input.navPressed(InputAction::MoveUp)) {
        menu_.moveUp();
        context_.audio.play(Sfx::Move);
    }
    if (input.navPressed(InputAction::MoveDown)) {
        menu_.moveDown();
        context_.audio.play(Sfx::Move);
    }
    scroll_.follow(static_cast<int>(menu_.size()), kMaxVisibleRows, menu_.cursor());
    if (input.pressed(InputAction::Confirm)) {
        const int picked = menu_.cursor();
        // Pop FIRST (queued; applies between frames), then report — the
        // callback may push an outcome panel on the state below.
        stack().popState();
        if (onPick_) {
            onPick_(picked);
        }
        return;
    }
    if (input.pressed(InputAction::Cancel)) {
        context_.audio.play(Sfx::Cancel);
        stack().popState();  // nothing spent; the event stays unresolved
    }
}

void EventChoiceState::render() {
    const int w = context_.virtualWidth;
    const int h = context_.virtualHeight;
    const style::Palette& p = style::palette();
    ui::drawModalDim(w, h);

    // Owner fix 2026-08-17: the title wraps to two lines (long flavor lines
    // were squeezed into one) and the rows show through a fixed scrolling
    // window instead of growing the box past the screen.
    const int rows = static_cast<int>(menu_.size());
    const int visRows = std::min(rows, kMaxVisibleRows);
    const int titleH = 2 * ui::lineHeight(style::kFontBody);
    const int boxW = 300;
    const int boxH = 43 + titleH + visRows * 16;
    const int boxX = w / 2 - boxW / 2;
    const int boxY = h / 2 - boxH / 2;
    ui::drawFrame(boxX, boxY, boxW, boxH, ui::FrameStyle::Raised);
    ui::drawTextPreview(title_, boxX + 14, boxY + 10, boxW - 28, style::kFontBody, p.gold, 2);
    ui::drawMenuScrolled(menu_, scroll_, visRows, boxX + 28, boxY + 10 + titleH + 4, 16, 12,
                         boxW - 56, p.text, p.disabled, p.cursor, "eventchoice.list");

    const InputMap& map = context_.input.map();
    const ActiveDevice device = context_.input.activeDevice();
    const std::string hint = input::prompt(map, InputAction::Confirm, device, "Choose") + "   " +
                             input::prompt(map, InputAction::Cancel, device, "Step away");
    ui::drawTextCentered(hint.c_str(), w / 2, boxY + boxH - 16, style::kFontSmall, p.textHint);
}

}  // namespace cd
