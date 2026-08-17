#include "states/TutorialPromptState.hpp"

#include <algorithm>
#include <memory>
#include <utility>

#include "audio/AudioManager.hpp"
#include "content/ContentDatabase.hpp"  // M99: forge-authored text overlay
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
constexpr int kTextW = kPanelW - 2 * style::kPad - ui::kScrollGutterW;
}  // namespace

TutorialPromptState::TutorialPromptState(StateStack& stack, AppContext& context,
                                         std::string title, std::string body)
    : GameState(stack), context_(context), title_(std::move(title)), body_(std::move(body)) {}

void TutorialPromptState::handleInput(const Input& input) {
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

void TutorialPromptState::render() {
    const int w = context_.virtualWidth;
    const int h = context_.virtualHeight;
    ui::drawModalDim(w, h);  // dim the frozen scene

    // M87: measured height caps inside the safe area; a body longer than the
    // cap scrolls (authored beats fit without scrolling — the container is
    // for translations, not an invitation to write longer beats).
    bodyView_.setContent(body_, kTextW, style::kFontBody, ui::raylibMeasure());
    const int chromeH = style::kPad + style::kFontHeading + 6 + 4 + style::kFontSmall +
                        style::kPad;
    const int maxBodyH = h - 2 * style::kSafeMargin - chromeH;
    const int capLines = std::max(1, maxBodyH / ui::lineHeight(style::kFontBody));
    bodyView_.setVisibleLines(std::clamp(bodyView_.lineCount(), 1, capLines));
    const int bodyH = bodyView_.visibleLines() * ui::lineHeight(style::kFontBody);
    const int panelH = chromeH + bodyH;
    const int x = (w - kPanelW) / 2;
    const int y = (h - panelH) / 2;

    // Teaching moment: the Crystal frame with its glinting corner pips.
    ui::drawFrame(x, y, kPanelW, panelH, ui::FrameStyle::Crystal);
    int ty = y + style::kPad;
    ui::drawTextCentered(title_.c_str(), x + kPanelW / 2, ty, style::kFontHeading,
                         style::palette().cursor);
    ty += style::kFontHeading + 6;
    ty = ui::drawTextViewport(bodyView_, x + style::kPad, ty, style::palette().text);
    ty += 4;
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
    // M99: forge-authored text (data/tutorials.json) wins; the constexpr beat
    // is the fallback, so a missing file or entry can never silence a prompt.
    std::string title = beat->title;
    std::string body = beat->body;
    if (const content::TutorialTextDef* t = context.content.findTutorialText(beatId)) {
        title = t->title;
        body = t->body;
    }
    stack.pushState(
        std::make_unique<TutorialPromptState>(stack, context, std::move(title), std::move(body)));
}

}  // namespace cd
