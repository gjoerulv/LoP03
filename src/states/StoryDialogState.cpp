#include "states/StoryDialogState.hpp"

#include <utility>
#include <vector>

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
constexpr int kTextW = kPanelW - 2 * style::kPad;
}  // namespace

StoryDialogState::StoryDialogState(StateStack& stack, AppContext& context, std::string speaker,
                                   std::string title, std::string body)
    : GameState(stack),
      context_(context),
      speaker_(std::move(speaker)),
      title_(std::move(title)),
      body_(std::move(body)) {}

void StoryDialogState::handleInput(const Input& input) {
    if (input.pressed(InputAction::Confirm) || input.pressed(InputAction::Cancel)) {
        context_.audio.play(Sfx::Confirm);
        stack().popState();
    }
}

void StoryDialogState::render() {
    const int w = context_.virtualWidth;
    const int h = context_.virtualHeight;
    ui::drawModalDim(w, h);  // dim the frozen scene

    const std::vector<std::string> lines =
        ui::wrapText(body_, kTextW, style::kFontBody, ui::raylibMeasure());
    const int bodyH = static_cast<int>(lines.size()) * ui::lineHeight(style::kFontBody);
    const int panelH = style::kPad + style::kFontHeading + 4 + style::kFontSmall + 6 + bodyH + 4 +
                       style::kFontSmall + style::kPad;
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
    ui::drawTextWrapped(body_, x + style::kPad, ty, kTextW, style::kFontBody,
                        style::palette().text, "story.body");
    ty += bodyH + 4;
    ui::drawTextCentered(
        input::prompt(context_.input.map(), InputAction::Confirm, context_.input.activeDevice(),
                      "Continue")
            .c_str(),
        x + kPanelW / 2, ty, style::kFontSmall, style::palette().textHint);
}

}  // namespace cd
