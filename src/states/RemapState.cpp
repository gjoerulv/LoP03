#include "states/RemapState.hpp"

#include <vector>

#include "audio/AudioManager.hpp"
#include "content/LoadReport.hpp"
#include "core/AppContext.hpp"
#include "core/Log.hpp"
#include "input/Input.hpp"
#include "input/PromptLabels.hpp"
#include "input/Remap.hpp"
#include "raylib.h"
#include "settings/Settings.hpp"
#include "states/StateStack.hpp"
#include "ui/UiDraw.hpp"
#include "ui/UiStyle.hpp"

namespace cd {

namespace style = ui::style;

namespace {
constexpr int kResetRow = static_cast<int>(kRemappableActionCount);      // after the actions
constexpr int kBackRow = static_cast<int>(kRemappableActionCount) + 1;
}  // namespace

RemapState::RemapState(StateStack& stack, AppContext& context, ActiveDevice device)
    : GameState(stack), context_(context), device_(device) {}

void RemapState::onEnter() { rebuild(); }

void RemapState::rebuild() {
    const InputMap& map = context_.input.map();
    std::vector<ui::MenuItem> items;
    for (InputAction a : kRemappableActions) {
        items.push_back({std::string(actionDisplayName(a)) + "   -   " +
                             input::allLabels(map, a, device_),
                         true});
    }
    items.push_back({device_ == ActiveDevice::Keyboard ? "Reset keyboard to defaults"
                                                       : "Reset gamepad to defaults",
                     true});
    items.push_back({"Back", true});
    const int previous = menu_.cursor();
    menu_.setItems(std::move(items));
    menu_.setCursor(previous);
}

void RemapState::applyRemap(int code) {
    const InputAction action = kRemappableActions[static_cast<std::size_t>(menu_.cursor())];
    InputMap& map = context_.input.map();
    const input::RemapResult result = device_ == ActiveDevice::Keyboard
                                          ? input::remapKey(map, action, code)
                                          : input::remapButton(map, action, code);
    const std::string codeLabel = device_ == ActiveDevice::Keyboard
                                      ? input::keyName(code)
                                      : input::buttonName(code);
    switch (result.outcome) {
        case input::RemapOutcome::Rebound:
            message_ = std::string(actionDisplayName(action)) + " is now " + codeLabel;
            context_.audio.play(Sfx::Confirm);
            break;
        case input::RemapOutcome::Swapped:
            message_ = std::string(actionDisplayName(action)) + " is now " + codeLabel +
                       " (swapped with " +
                       std::string(actionDisplayName(result.swappedWith)) + ")";
            context_.audio.play(Sfx::Confirm);
            break;
        case input::RemapOutcome::Blocked:
            message_ = codeLabel + " cannot be used here";
            context_.audio.play(Sfx::Cancel);
            break;
    }
    if (result.outcome != input::RemapOutcome::Blocked) {
        content::LoadReport report;
        if (!context_.settings.save(map, report)) {
            for (const auto& e : report.errors()) {
                log::warn(e.source + ": " + e.context + ": " + e.message);
            }
        }
    }
    // The key that was just bound is still physically held — without this it
    // would instantly fire as its new action (observed: binding J to Move Up
    // moved the cursor).
    context_.input.suppressUntilRelease();
    rebuild();
}

void RemapState::handleInput(const Input& input) {
    if (listening_) {
        // Esc always cancels listening (reserved; never bindable).
        for (int key = input.takeNextKey(); key != 0; key = input.takeNextKey()) {
            if (key == input::kReservedKeyEscape) {
                listening_ = false;
                message_ = "Cancelled";
                return;
            }
            if (device_ == ActiveDevice::Keyboard) {
                listening_ = false;
                applyRemap(key);
                return;
            }
        }
        if (device_ == ActiveDevice::Gamepad) {
            const int button = input.takePressedButton();
            if (button > 0) {
                listening_ = false;
                applyRemap(button);
                return;
            }
        }
        return;  // stay in listening mode; ignore normal navigation
    }

    if (input.navPressed(InputAction::MoveUp)) {
        menu_.moveUp();
        context_.audio.play(Sfx::Move);
    }
    if (input.navPressed(InputAction::MoveDown)) {
        menu_.moveDown();
        context_.audio.play(Sfx::Move);
    }
    if (input.pressed(InputAction::Confirm)) {
        const int row = menu_.cursor();
        if (row < static_cast<int>(kRemappableActionCount)) {
            listening_ = true;
            message_.clear();
        } else if (row == kResetRow) {
            // Reset both devices' bindings (the pure reset restores all
            // defaults; per-device partial reset is not worth the asymmetry).
            input::resetBindings(context_.input.map());
            content::LoadReport report;
            context_.settings.save(context_.input.map(), report);
            message_ = "Bindings reset to defaults";
            context_.audio.play(Sfx::Confirm);
            rebuild();
        } else if (row == kBackRow) {
            stack().popState();
        }
    }
    if (input.pressed(InputAction::Cancel)) {
        context_.audio.play(Sfx::Cancel);
        stack().popState();
    }
}

void RemapState::render() {
    const int w = context_.virtualWidth;
    const int h = context_.virtualHeight;
    ClearBackground(Color{16, 16, 26, 255});
    const char* title =
        device_ == ActiveDevice::Keyboard ? "Remap Keyboard" : "Remap Gamepad";
    ui::drawTextCentered(title, w / 2, 14, style::kFontScreenTitle, style::kText);

    ui::drawMenu(menu_, 60, 44, 17, style::kFontMenu, style::kText, style::kDisabled,
                 style::kCursor);

    if (!message_.empty()) {
        ui::drawTextFitted(message_, 40, h - 46, w - 80, style::kFontBody, style::kSuccess,
                           "remap.message");
    }

    if (listening_) {
        const int boxW = 300;
        const int boxH = 64;
        const int boxX = w / 2 - boxW / 2;
        const int boxY = h / 2 - boxH / 2;
        DrawRectangle(0, 0, w, h, Color{0, 0, 0, 140});
        ui::drawPanel(boxX, boxY, boxW, boxH, Color{28, 26, 48, 245},
                      Color{200, 200, 140, 255});
        const InputAction action =
            kRemappableActions[static_cast<std::size_t>(menu_.cursor())];
        const std::string line = std::string(device_ == ActiveDevice::Keyboard
                                                 ? "Press a key for "
                                                 : "Press a button for ") +
                                 std::string(actionDisplayName(action));
        ui::drawTextCentered(line.c_str(), w / 2, boxY + 14, style::kFontBody, style::kText);
        ui::drawTextCentered("[Esc] Cancel", w / 2, boxY + 38, style::kFontBody,
                             style::kTextHint);
        return;
    }

    const InputMap& map = context_.input.map();
    const ActiveDevice promptDevice = context_.input.activeDevice();
    const std::string footer =
        input::prompt(map, InputAction::Confirm, promptDevice, "Rebind") + "    " +
        input::prompt(map, InputAction::Cancel, promptDevice, "Back");
    ui::drawTextCentered(footer.c_str(), w / 2, h - style::kFooterHeight + 2, 9,
                         style::kTextHint);
}

}  // namespace cd
