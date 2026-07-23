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
// Column layout (x positions) inside the 426px virtual screen. Shifted left
// in M46 so the widest gamepad label clears the new inset frame's right edge.
constexpr int kCol1X = 20;
constexpr int kCol2X = 160;
constexpr int kCol3X = 300;

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
    const style::Palette& p = style::palette();
    ClearBackground(p.canvas);
    ui::drawHeaderBand("Controls", w, p.crystal);

    const int col1W = kCol2X - kCol1X - 8;
    const int col2W = kCol3X - kCol2X - 8;
    // Rightmost column: take the full remaining width to the safe margin so
    // the widest gamepad label ("D-Pad Right or Stick") fits the M25 font.
    const int col3W = w - kCol3X - 16;
    const InputMap& map = context_.input.map();

    ui::drawFrame(12, 30, w - 24, 17 + 9 * 15 + 14, ui::FrameStyle::Inset);
    int y = 38;
    ui::drawTextFitted("Action", kCol1X, y, col1W, style::kFontBody, p.textDim, "help.header");
    ui::drawTextFitted("Keyboard", kCol2X, y, col2W, style::kFontBody, p.textDim, "help.header");
    ui::drawTextFitted("Gamepad", kCol3X, y, col3W, style::kFontBody, p.textDim, "help.header");
    y += 12;
    ui::drawDivider(kCol1X - 4, y, w - 2 * (kCol1X - 4) - 16);
    y += 5;
    for (InputAction a : kShownActions) {
        ui::drawTextFitted(std::string(actionDisplayName(a)), kCol1X, y, col1W,
                           style::kFontBody, p.text, "help.action");
        ui::drawTextFitted(input::allLabels(map, a, ActiveDevice::Keyboard), kCol2X, y, col2W,
                           style::kFontBody, p.textDim, "help.keyboard");
        ui::drawTextFitted(input::allLabels(map, a, ActiveDevice::Gamepad), kCol3X, y, col3W,
                           style::kFontBody, p.textDim, "help.gamepad");
        y += 15;
    }

    ui::drawTextWrapped(
        "Bindings can be changed under Settings. The window is resizable; the image always "
        "scales to fit.",
        kCol1X, h - 34, w - 2 * kCol1X, 9, p.textHint, "help.note", 2);
    ui::drawFooterHints({{input::primaryLabel(map, InputAction::Cancel,
                                              context_.input.activeDevice()),
                          "Back"}},
                        w, h, "help.footer");
}

}  // namespace cd
