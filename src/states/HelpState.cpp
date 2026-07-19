#include "states/HelpState.hpp"

#include "audio/AudioManager.hpp"
#include "core/AppContext.hpp"
#include "input/Input.hpp"
#include "raylib.h"
#include "states/StateStack.hpp"
#include "ui/UiDraw.hpp"
#include "ui/UiStyle.hpp"

namespace cd {

namespace {
struct Row {
    const char* action;
    const char* keyboard;
    const char* gamepad;
};
// NOTE: gamepad navigation is D-pad only until analog-stick support lands in
// M13 (control_standard.md); this table must state what actually works.
constexpr Row kRows[] = {
    {"Move / Navigate", "Arrows or WASD", "D-Pad"},
    {"Confirm", "Enter or Space", "A"},
    {"Cancel / Back", "Esc or Backspace", "B"},
    {"Menu / Pause", "Tab", "Start"},
    {"Adjust (Guild)", "Left / Right", "D-Pad L/R"},
    {"Toggle debug overlay", "F1", "-"},
};

// Column layout (x, max width) inside the 426px virtual screen.
constexpr int kCol1X = 32;
constexpr int kCol2X = 172;
constexpr int kCol3X = 316;
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
    namespace style = ui::style;
    const int w = context_.virtualWidth;
    const int h = context_.virtualHeight;
    ClearBackground(Color{16, 16, 26, 255});
    ui::drawTextCentered("Controls", w / 2, 16, style::kFontScreenTitle, style::kText);

    const int col1W = kCol2X - kCol1X - 8;
    const int col2W = kCol3X - kCol2X - 8;
    const int col3W = w - kCol3X - style::kSafeMargin - 4;

    int y = 52;
    ui::drawTextFitted("Action", kCol1X, y, col1W, style::kFontBody, style::kTextDim,
                       "help.header");
    ui::drawTextFitted("Keyboard", kCol2X, y, col2W, style::kFontBody, style::kTextDim,
                       "help.header");
    ui::drawTextFitted("Gamepad", kCol3X, y, col3W, style::kFontBody, style::kTextDim,
                       "help.header");
    y += 18;
    for (const Row& r : kRows) {
        ui::drawTextFitted(r.action, kCol1X, y, col1W, style::kFontBody, style::kText,
                           "help.action");
        ui::drawTextFitted(r.keyboard, kCol2X, y, col2W, style::kFontBody,
                           Color{200, 210, 230, 255}, "help.keyboard");
        ui::drawTextFitted(r.gamepad, kCol3X, y, col3W, style::kFontBody,
                           Color{200, 210, 230, 255}, "help.gamepad");
        y += 16;
    }

    ui::drawTextWrapped("The window is resizable; the image always scales to fit.", kCol1X,
                        h - 34, w - 2 * kCol1X, 9, Color{160, 160, 180, 255}, "help.note", 1);
    ui::drawTextCentered("Cancel: Back", w / 2, h - style::kFooterHeight + 2, style::kFontBody,
                         style::kTextHint);
}

}  // namespace cd
