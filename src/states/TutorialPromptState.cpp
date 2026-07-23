#include "states/TutorialPromptState.hpp"

#include <memory>
#include <utility>
#include <vector>

#include "audio/AudioManager.hpp"
#include "core/AppContext.hpp"
#include "input/Input.hpp"
#include "raylib.h"
#include "states/StateStack.hpp"
#include "tutorial/Tutorial.hpp"
#include "ui/UiDraw.hpp"
#include "ui/UiStyle.hpp"

namespace cd {

namespace style = ui::style;

namespace {
constexpr int kPanelW = 320;
constexpr int kTextW = kPanelW - 2 * style::kPad;
}  // namespace

TutorialPromptState::TutorialPromptState(StateStack& stack, AppContext& context,
                                         std::string title, std::string body)
    : GameState(stack), context_(context), title_(std::move(title)), body_(std::move(body)) {}

void TutorialPromptState::handleInput(const Input& input) {
    if (input.pressed(InputAction::Confirm) || input.pressed(InputAction::Cancel)) {
        context_.audio.play(Sfx::Confirm);
        stack().popState();
    }
}

void TutorialPromptState::render() {
    const int w = context_.virtualWidth;
    const int h = context_.virtualHeight;
    ui::drawModalDim(w, h);  // dim the frozen scene

    // Measured height: title + wrapped body + footer hint, then center.
    const std::vector<std::string> lines =
        ui::wrapText(body_, kTextW, style::kFontBody, ui::raylibMeasure());
    const int bodyH = static_cast<int>(lines.size()) * ui::lineHeight(style::kFontBody);
    const int panelH = style::kPad + style::kFontHeading + 6 + bodyH + 4 +
                       style::kFontSmall + style::kPad;
    const int x = (w - kPanelW) / 2;
    const int y = (h - panelH) / 2;

    // Teaching moment: the Crystal frame with its glinting corner pips.
    ui::drawFrame(x, y, kPanelW, panelH, ui::FrameStyle::Crystal);
    int ty = y + style::kPad;
    ui::drawTextCentered(title_.c_str(), x + kPanelW / 2, ty, style::kFontHeading,
                         style::palette().cursor);
    ty += style::kFontHeading + 6;
    ui::drawTextWrapped(body_, x + style::kPad, ty, kTextW, style::kFontBody,
                        style::palette().text, "tutorial.body");
    ty += bodyH + 4;
    ui::drawTextCentered("Confirm - continue   (Tutorial prompts: Settings)",
                         x + kPanelW / 2, ty, style::kFontSmall, style::palette().textHint);
}

void maybeTutorialPrompt(StateStack& stack, AppContext& context, const char* beatId) {
    if (!context.tutorial.takeBeat(beatId)) {
        return;
    }
    const tutorial::Beat* beat = tutorial::findBeat(beatId);
    if (beat == nullptr) {
        return;
    }
    stack.pushState(
        std::make_unique<TutorialPromptState>(stack, context, beat->title, beat->body));
}

}  // namespace cd
