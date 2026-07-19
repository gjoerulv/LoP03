#include "states/HelpState.hpp"

#include <string>

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
// Column layout (x positions) inside the 426px virtual screen.
constexpr int kCol1X = 28;
constexpr int kCol2X = 168;
constexpr int kCol3X = 312;

// The actions shown on the controls page, generated from the LIVE bindings
// (M13): remapping is always reflected here.
constexpr InputAction kShownActions[] = {
    InputAction::MoveUp,    InputAction::MoveDown, InputAction::MoveLeft,
    InputAction::MoveRight, InputAction::Confirm,  InputAction::Cancel,
    InputAction::Menu,      InputAction::TextBackspace, InputAction::ToggleDebug,
};
}  // namespace

HelpState::HelpState(StateStack& stack, AppContext& context)
    : GameState(stack), context_(context) {}

void HelpState::handleInput(const Input& input) {
    if (input.pressed(InputAction::Cancel) || input.pressed(InputAction::Confirm)) {
        context_.audio.play(Sfx::Cancel);
        stack().popState();
    }
}

void HelpState::render() {
    const int w = context_.virtualWidth;
    const int h = context_.virtualHeight;
    ClearBackground(Color{16, 16, 26, 255});
    ui::drawTextCentered("Controls", w / 2, 14, style::kFontScreenTitle, style::kText);

    const int col1W = kCol2X - kCol1X - 8;
    const int col2W = kCol3X - kCol2X - 8;
    const int col3W = w - kCol3X - style::kSafeMargin - 4;
    const InputMap& map = context_.input.map();

    int y = 42;
    ui::drawTextFitted("Action", kCol1X, y, col1W, style::kFontBody, style::kTextDim,
                       "help.header");
    ui::drawTextFitted("Keyboard", kCol2X, y, col2W, style::kFontBody, style::kTextDim,
                       "help.header");
    ui::drawTextFitted("Gamepad", kCol3X, y, col3W, style::kFontBody, style::kTextDim,
                       "help.header");
    y += 17;
    for (InputAction a : kShownActions) {
        ui::drawTextFitted(std::string(actionDisplayName(a)), kCol1X, y, col1W,
                           style::kFontBody, style::kText, "help.action");
        ui::drawTextFitted(input::allLabels(map, a, ActiveDevice::Keyboard), kCol2X, y, col2W,
                           style::kFontBody, Color{200, 210, 230, 255}, "help.keyboard");
        ui::drawTextFitted(input::allLabels(map, a, ActiveDevice::Gamepad), kCol3X, y, col3W,
                           style::kFontBody, Color{200, 210, 230, 255}, "help.gamepad");
        y += 15;
    }

    ui::drawTextWrapped(
        "Bindings can be changed under Settings. The window is resizable; the image always "
        "scales to fit.",
        kCol1X, h - 36, w - 2 * kCol1X, 9, Color{160, 160, 180, 255}, "help.note", 2);
    const std::string footer =
        input::prompt(map, InputAction::Cancel, context_.input.activeDevice(), "Back");
    ui::drawTextCentered(footer.c_str(), w / 2, h - style::kFooterHeight + 2, style::kFontBody,
                         style::kTextHint);
}

}  // namespace cd
