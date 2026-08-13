#include "states/StoryDialogState.hpp"

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
constexpr int kPanelW = 320;
constexpr int kTextW = kPanelW - 2 * style::kPad - ui::kScrollGutterW;
}  // namespace

StoryDialogState::StoryDialogState(StateStack& stack, AppContext& context, std::string speaker,
                                   std::string title, std::string body)
    : GameState(stack),
      context_(context),
      speaker_(std::move(speaker)),
      title_(std::move(title)),
      body_(std::move(body)) {}

void StoryDialogState::handleInput(const Input& input) {
    if (input.navPressed(InputAction::MoveUp) && bodyView_.scrollBy(-1)) {
        context_.audio.play(Sfx::Move);
    }
    if (input.navPressed(InputAction::MoveDown) && bodyView_.scrollBy(1)) {
        context_.audio.play(Sfx::Move);
    }
    if (input.pressed(InputAction::Confirm) || input.pressed(InputAction::Cancel)) {
        context_.audio.play(Sfx::Confirm);
        stack().popState();
    }
}

void StoryDialogState::render() {
    const int w = context_.virtualWidth;
    const int h = context_.virtualHeight;
    ui::drawModalDim(w, h);  // dim the frozen scene

    // M87: the panel no longer grows with its body — height caps inside the
    // safe area (fixed title/speaker, scrolling body, fixed Continue line),
    // so a translated beat of any length stays on screen and reachable.
    // Story JSON never carries layout newlines; '\n' is paragraph semantics.
    bodyView_.setContent(body_, kTextW, style::kFontBody, ui::raylibMeasure());
    const int chromeH = style::kPad + style::kFontHeading + 4 + style::kFontSmall + 6 + 4 +
                        style::kFontSmall + style::kPad;
    const int maxBodyH = h - 2 * style::kSafeMargin - chromeH;
    const int capLines = std::max(1, maxBodyH / ui::lineHeight(style::kFontBody));
    bodyView_.setVisibleLines(std::clamp(bodyView_.lineCount(), 1, capLines));
    const int bodyH = bodyView_.visibleLines() * ui::lineHeight(style::kFontBody);
    const int panelH = chromeH + bodyH;
    const int x = (w - kPanelW) / 2;
    const int y = (h - panelH) / 2;

    // Story beats arrive in the Reward frame: gold pips, storybook warmth.
    ui::drawFrame(x, y, kPanelW, panelH, ui::FrameStyle::Reward);
    int ty = y + style::kPad;
    ui::drawTextCentered(title_.c_str(), x + kPanelW / 2, ty, style::kFontHeading,
                         style::palette().gold);
    ty += style::kFontHeading + 4;
    ui::drawTextCentered(("- " + speaker_ + " -").c_str(), x + kPanelW / 2, ty, style::kFontSmall,
                         style::palette().textHint);
    ty += style::kFontSmall + 6;
    ty = ui::drawTextViewport(bodyView_, x + style::kPad, ty, style::palette().text);
    ty += 4;
    const InputMap& map = context_.input.map();
    const ActiveDevice device = context_.input.activeDevice();
    std::string hint = input::prompt(map, InputAction::Confirm, device, "Continue");
    if (bodyView_.scrollable()) {
        hint = input::primaryLabel(map, InputAction::MoveUp, device) + "/" +
               input::primaryLabel(map, InputAction::MoveDown, device) + " Scroll   " + hint;
    }
    ui::drawTextCentered(hint.c_str(), x + kPanelW / 2, ty, style::kFontSmall,
                         style::palette().textHint);
}

}  // namespace cd
