#include "states/DetailsOverlayState.hpp"

#include <utility>
#include <vector>

#include "audio/AudioManager.hpp"
#include "core/AppContext.hpp"
#include "input/Input.hpp"
#include "raylib.h"
#include "states/StateStack.hpp"
#include "ui/UiDraw.hpp"
#include "ui/UiStyle.hpp"

namespace cd {

namespace style = ui::style;

namespace {
constexpr int kPanelW = 360;
constexpr int kTextW = kPanelW - 2 * style::kPad;
}  // namespace

DetailsOverlayState::DetailsOverlayState(StateStack& stack, AppContext& context,
                                         std::string title, std::string body)
    : GameState(stack), context_(context), title_(std::move(title)), body_(std::move(body)) {}

void DetailsOverlayState::handleInput(const Input& input) {
    if (input.pressed(InputAction::Confirm) || input.pressed(InputAction::Cancel) ||
        input.pressed(InputAction::Details)) {
        context_.audio.play(Sfx::Cancel);
        stack().popState();
    }
}

void DetailsOverlayState::render() {
    const int w = context_.virtualWidth;
    const int h = context_.virtualHeight;
    DrawRectangle(0, 0, w, h, Color{0, 0, 0, 150});

    const std::vector<std::string> lines =
        ui::wrapText(body_, kTextW, style::kFontBody, ui::raylibMeasure());
    // The panel is height-capped to the screen; drawTextWrapped's maxLines
    // reports truncation to the log if a body ever outgrows it.
    const int maxBodyH = h - 2 * style::kSafeMargin - style::kPad * 2 -
                         style::kFontHeading - 6 - style::kFontSmall - 4;
    const int maxLines = maxBodyH / ui::lineHeight(style::kFontBody);
    const int shown = static_cast<int>(lines.size()) < maxLines
                          ? static_cast<int>(lines.size())
                          : maxLines;
    const int bodyH = shown * ui::lineHeight(style::kFontBody);
    const int panelH = style::kPad + style::kFontHeading + 6 + bodyH + 4 +
                       style::kFontSmall + style::kPad;
    const int x = (w - kPanelW) / 2;
    const int y = (h - panelH) / 2;

    ui::drawFramedPanel(context_.resources, x, y, kPanelW, panelH,
                        Color{18, 22, 32, 250}, style::palette().textDim);
    int ty = y + style::kPad;
    ui::drawTextCentered(title_.c_str(), x + kPanelW / 2, ty, style::kFontHeading,
                         style::palette().text);
    ty += style::kFontHeading + 6;
    ui::drawTextWrapped(body_, x + style::kPad, ty, kTextW, style::kFontBody,
                        style::palette().text, "details.body", maxLines);
    ty += bodyH + 4;
    ui::drawTextCentered("Confirm - close", x + kPanelW / 2, ty, style::kFontSmall,
                         style::palette().textHint);
}

}  // namespace cd
