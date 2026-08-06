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
// M79: ten actions plus Reset/Back — 14px rows keep the list inside the frame
// at the 426x240 virtual resolution.
constexpr int kRowH = 14;
constexpr int kListY = 42;
constexpr int kFrameX = 30;
constexpr int kLabelX = 42;
constexpr int kLabelW = 118;  // fits "Next Party Member" at the menu font
// The binding columns start right of the labels and share the frame's
// remaining width (computed in render from the virtual width).
constexpr int kColsX = kLabelX + kLabelW + 6;

const char* slotName(int slot) {
    switch (slot) {
        case 0: return "Primary";
        case 1: return "Alt 1";
        default: return "Alt 2";
    }
}
}  // namespace

RemapState::RemapState(StateStack& stack, AppContext& context, ActiveDevice device)
    : GameState(stack), context_(context), device_(device) {}

void RemapState::onEnter() { rebuild(); }

void RemapState::rebuild() {
    // The menu is cursor/row bookkeeping only — rows render manually so the
    // binding columns align (M79 owner feedback: no more " - " run-ons).
    std::vector<ui::MenuItem> items;
    for (InputAction a : kRemappableActions) {
        items.push_back({std::string(actionDisplayName(a)), true});
    }
    items.push_back({device_ == ActiveDevice::Keyboard ? "Reset keyboard to defaults"
                                                       : "Reset gamepad to defaults",
                     true});
    items.push_back({"Back", true});
    const int previous = menu_.cursor();
    menu_.setItems(std::move(items));
    menu_.setCursor(previous);
}

void RemapState::raiseMessage(std::string text, bool isError) {
    message_ = std::move(text);
    messageIsError_ = isError;
    messageTimer_ =
        2.5f * settings::messageDurationScale(context_.settings.values.messageSpeed);
}

void RemapState::update(float dt) {
    if (messageTimer_ > 0.0f) {
        messageTimer_ -= dt;
        if (messageTimer_ <= 0.0f) {
            message_.clear();
        }
    }
}

// The M13 gamepad flow: replace the binding, swapping with a conflicting owner.
void RemapState::applyRemap(int code) {
    const InputAction action = kRemappableActions[static_cast<std::size_t>(menu_.cursor())];
    InputMap& map = context_.input.map();
    const input::RemapResult result = input::remapButton(map, action, code);
    const std::string codeLabel = input::buttonName(code);
    switch (result.outcome) {
        case input::RemapOutcome::Rebound:
            // The row updates in place; no banner needed (owner feedback).
            context_.audio.play(Sfx::Confirm);
            break;
        case input::RemapOutcome::Swapped:
            raiseMessage(std::string(actionDisplayName(action)) + " is now " + codeLabel +
                             " (swapped with " +
                             std::string(actionDisplayName(result.swappedWith)) + ")",
                         false);
            context_.audio.play(Sfx::Confirm);
            break;
        case input::RemapOutcome::Blocked:
            raiseMessage(codeLabel + " cannot be used here", true);
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
    // The button that was just bound is still physically held — without this it
    // would instantly fire as its new action.
    context_.input.suppressUntilRelease();
    rebuild();
}

// M79 keyboard flow: one slot at a time, with an explicit steal confirmation.
void RemapState::applySlotAssign(int key, bool confirmSteal) {
    const InputAction action = kRemappableActions[static_cast<std::size_t>(menu_.cursor())];
    InputMap& map = context_.input.map();
    const input::SlotResult result =
        input::assignKeySlot(map, action, slotSel_, key, confirmSteal);
    const std::string keyLabel = input::keyName(key);
    bool save = false;
    switch (result.outcome) {
        case input::SlotOutcome::Rebound:
            // The slot cell updates in place; no banner (owner feedback).
            context_.audio.play(Sfx::Confirm);
            save = true;
            break;
        case input::SlotOutcome::Stolen:
            raiseMessage(keyLabel + " moved here from " +
                             std::string(actionDisplayName(result.owner)),
                         false);
            context_.audio.play(Sfx::Confirm);
            save = true;
            break;
        case input::SlotOutcome::NeedsConfirm:
            // Nothing changed yet: raise the warning and wait for the player.
            confirmPending_ = true;
            pendingKey_ = key;
            pendingOwnerLabel_ = std::string(actionDisplayName(result.owner)) + " (" +
                                 slotName(result.ownerSlot) + ")";
            return;
        case input::SlotOutcome::Blocked:
            raiseMessage(confirmSteal
                             ? "Blocked: that would leave " +
                                   std::string(actionDisplayName(result.owner)) +
                                   " with no keys"
                             : keyLabel + " cannot be used here",
                         true);
            context_.audio.play(Sfx::Cancel);
            break;
    }
    if (save) {
        content::LoadReport report;
        if (!context_.settings.save(map, report)) {
            for (const auto& e : report.errors()) {
                log::warn(e.source + ": " + e.context + ": " + e.message);
            }
        }
    }
    context_.input.suppressUntilRelease();
    rebuild();
}

void RemapState::handleInput(const Input& input) {
    if (confirmPending_) {
        // The steal warning modal: Confirm takes the key, Cancel keeps things.
        if (input.pressed(InputAction::Confirm)) {
            confirmPending_ = false;
            applySlotAssign(pendingKey_, true);
            return;
        }
        if (input.pressed(InputAction::Cancel)) {
            confirmPending_ = false;
            raiseMessage("Kept as it was", false);
            context_.audio.play(Sfx::Cancel);
        }
        return;
    }

    if (listening_) {
        // Esc always cancels listening (reserved; never bindable).
        for (int key = input.takeNextKey(); key != 0; key = input.takeNextKey()) {
            if (key == input::kReservedKeyEscape) {
                listening_ = false;
                raiseMessage("Cancelled", false);
                return;
            }
            if (device_ == ActiveDevice::Keyboard) {
                listening_ = false;
                applySlotAssign(key, false);
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
    // M79: Left/Right pick the keyboard slot column on an action row.
    if (device_ == ActiveDevice::Keyboard &&
        menu_.cursor() < static_cast<int>(kRemappableActionCount)) {
        if (input.navPressed(InputAction::MoveLeft) && slotSel_ > 0) {
            --slotSel_;
            context_.audio.play(Sfx::Move);
        }
        if (input.navPressed(InputAction::MoveRight) &&
            slotSel_ < input::kKeySlotCount - 1) {
            ++slotSel_;
            context_.audio.play(Sfx::Move);
        }
    }
    if (input.pressed(InputAction::Confirm)) {
        const int row = menu_.cursor();
        if (row < static_cast<int>(kRemappableActionCount)) {
            listening_ = true;
            message_.clear();
            messageTimer_ = 0.0f;
        } else if (row == kResetRow) {
            // Reset both devices' bindings (the pure reset restores all
            // defaults; per-device partial reset is not worth the asymmetry).
            input::resetBindings(context_.input.map());
            content::LoadReport report;
            context_.settings.save(context_.input.map(), report);
            raiseMessage("Bindings reset to defaults", false);
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
    const style::Palette& p = style::palette();
    ClearBackground(p.canvas);
    const char* title =
        device_ == ActiveDevice::Keyboard ? "Remap Keyboard" : "Remap Gamepad";
    ui::drawHeaderBand(title, w, p.crystal);

    const int rows = static_cast<int>(menu_.size());
    const int frameW = w - kFrameX * 2;
    const int colsRight = kFrameX + frameW - 12;  // inner right edge for cells
    ui::drawFrame(kFrameX, kListY - 8, frameW, rows * kRowH + 14, ui::FrameStyle::Inset);

    // Binding columns: three slot cells (keyboard) or one wide cell (gamepad),
    // computed from the frame so nothing can spill past its border.
    const int colCount = device_ == ActiveDevice::Keyboard ? input::kKeySlotCount : 1;
    const int colW = (colsRight - kColsX) / colCount;
    const InputMap& map = context_.input.map();

    for (int row = 0; row < rows; ++row) {
        const int rowY = kListY + row * kRowH;
        const bool isCursor = row == menu_.cursor();
        const bool actionRow = row < static_cast<int>(kRemappableActionCount);
        // The cursor slab spans the label column on action rows and the whole
        // width on Reset/Back — never a second box around a cell (M79 owner
        // feedback: the old rects overlapped and overran the frame).
        if (isCursor) {
            const int slabW = actionRow ? kLabelW + 16 : frameW - 24;
            ui::drawSelectionSlab(kLabelX - 12, rowY - 2,
                                  slabW, std::min(kRowH + 1, style::kFontMenu + 6));
            ui::drawChevron(kLabelX - 9, rowY + (style::kFontMenu - 8) / 2, p.cursor,
                            ui::motionPhase());
        }
        const Color labelColor = isCursor ? p.cursor : p.text;
        ui::drawTextFitted(menu_.items()[static_cast<std::size_t>(row)].label, kLabelX,
                           rowY, actionRow ? kLabelW : frameW - 36, style::kFontMenu,
                           labelColor, "remap.row");
        if (!actionRow) {
            continue;
        }
        const InputAction action = kRemappableActions[static_cast<std::size_t>(row)];
        if (device_ == ActiveDevice::Keyboard) {
            const auto& keys = map.keys(action);
            for (int s = 0; s < input::kKeySlotCount; ++s) {
                const int cellX = kColsX + s * colW;
                const bool addressed = isCursor && s == slotSel_;
                const std::string label =
                    s < static_cast<int>(keys.size())
                        ? input::keyName(keys[static_cast<std::size_t>(s)])
                        : "-";
                // The addressed slot is highlighted TEXT with an underline —
                // no rectangle to collide with anything.
                ui::drawTextFitted(label, cellX, rowY, colW - 8, style::kFontMenu,
                                   addressed ? p.cursor : p.textDim, "remap.slot");
                if (addressed) {
                    DrawRectangle(cellX, rowY + style::kFontMenu + 1, colW - 12, 1,
                                  p.cursor);
                }
            }
        } else {
            ui::drawTextFitted(input::allLabels(map, action, ActiveDevice::Gamepad),
                               kColsX, rowY, colsRight - kColsX, style::kFontMenu,
                               p.textDim, "remap.pad");
        }
    }

    // Column headers ride above the frame (keyboard only).
    if (device_ == ActiveDevice::Keyboard) {
        for (int s = 0; s < input::kKeySlotCount; ++s) {
            ui::drawText(slotName(s), kColsX + s * colW, kListY - 8 - 11,
                         style::kFontSmall, p.textHint);
        }
    }

    // The transient banner rides the TOP of the screen (over the headers, never
    // the Back row) and times out — see update() (M79 owner feedback).
    if (!message_.empty() && messageTimer_ > 0.0f) {
        ui::drawBanner(messageIsError_ ? ui::BannerKind::Danger : ui::BannerKind::Success,
                       message_, 60, 21, w - 120, "remap.message");
    }

    if (listening_ || confirmPending_) {
        const int boxW = 340;
        const int boxH = 64;
        const int boxX = w / 2 - boxW / 2;
        const int boxY = h / 2 - boxH / 2;
        ui::drawModalDim(w, h);
        ui::drawFrame(boxX, boxY, boxW, boxH, ui::FrameStyle::Crystal);
        const InputAction action =
            kRemappableActions[static_cast<std::size_t>(menu_.cursor())];
        if (confirmPending_) {
            // M79: the in-use warning — explicit consent before a steal.
            const std::string line = input::keyName(pendingKey_) + " already does " +
                                     pendingOwnerLabel_ + ".";
            ui::drawTextCentered(line.c_str(), w / 2, boxY + 12, style::kFontBody, p.text);
            const ActiveDevice promptDevice = context_.input.activeDevice();
            const std::string hint =
                input::prompt(map, InputAction::Confirm, promptDevice, "Steal it") + "   " +
                input::prompt(map, InputAction::Cancel, promptDevice, "Keep");
            ui::drawTextCentered(hint.c_str(), w / 2, boxY + 38, style::kFontBody,
                                 p.textHint);
        } else {
            const std::string line =
                std::string(device_ == ActiveDevice::Keyboard ? "Press a key for "
                                                              : "Press a button for ") +
                std::string(actionDisplayName(action)) +
                (device_ == ActiveDevice::Keyboard
                     ? std::string(" (") + slotName(slotSel_) + ")"
                     : std::string());
            ui::drawTextCentered(line.c_str(), w / 2, boxY + 14, style::kFontBody, p.text);
            ui::drawTextCentered("[Esc] Cancel", w / 2, boxY + 38, style::kFontBody,
                                 p.textHint);
        }
        return;
    }

    const ActiveDevice promptDevice = context_.input.activeDevice();
    std::vector<ui::Hint> hints;
    hints.push_back({input::primaryLabel(map, InputAction::Confirm, promptDevice), "Rebind"});
    if (device_ == ActiveDevice::Keyboard) {
        hints.push_back({input::primaryLabel(map, InputAction::MoveLeft, promptDevice) + "/" +
                             input::primaryLabel(map, InputAction::MoveRight, promptDevice),
                         "Slot"});
    }
    hints.push_back({input::primaryLabel(map, InputAction::Cancel, promptDevice), "Back"});
    ui::drawFooterHints(hints, w, h, "remap.footer");
}

}  // namespace cd
