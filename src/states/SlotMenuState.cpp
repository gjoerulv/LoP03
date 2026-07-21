#include "states/SlotMenuState.hpp"

#include <memory>

#include "content/LoadReport.hpp"
#include "core/AppContext.hpp"
#include "game/Party.hpp"
#include "input/Input.hpp"
#include "input/PromptLabels.hpp"
#include "raylib.h"
#include "states/StateStack.hpp"
#include "states/TownState.hpp"
#include "ui/UiDraw.hpp"

namespace cd {

namespace {
std::string slotLabel(save::SaveSystem& saves, save::SaveSlot slot) {
    std::string label = save::slotDisplayName(slot);
    if (auto s = saves.summary(slot)) {
        label += TextFormat("  -  Lv.%d  party %d  %dg", s->highestLevel, s->partySize, s->gold);
        // M40: a party that has beaten the King carries a visible title here.
        if (!s->kingTitle.empty()) {
            label += "  -  " + s->kingTitle;
        }
    } else {
        label += "  -  (empty)";
    }
    return label;
}
}  // namespace

SlotMenuState::SlotMenuState(StateStack& stack, AppContext& context, SlotMenuMode mode)
    : GameState(stack), context_(context), mode_(mode) {}

void SlotMenuState::onEnter() { rebuild(); }

void SlotMenuState::rebuild() {
    slots_.clear();
    std::vector<ui::MenuItem> items;

    if (mode_ == SlotMenuMode::Load) {
        slots_ = {save::SaveSlot::Auto, save::SaveSlot::Manual1, save::SaveSlot::Manual2,
                  save::SaveSlot::Manual3};
    } else {
        // The autosave slot is not manually writable.
        slots_ = {save::SaveSlot::Manual1, save::SaveSlot::Manual2, save::SaveSlot::Manual3};
    }

    for (save::SaveSlot slot : slots_) {
        const bool exists = context_.saves.exists(slot);
        const bool enabled = (mode_ == SlotMenuMode::Save) || exists;  // can't load empty
        items.push_back({slotLabel(context_.saves, slot), enabled});
    }
    const int previous = menu_.cursor();
    menu_.setItems(std::move(items));
    menu_.setCursor(previous);
}

void SlotMenuState::confirmSelection() {
    if (!menu_.currentEnabled() || slots_.empty()) {
        return;
    }
    const save::SaveSlot slot = slots_[static_cast<std::size_t>(menu_.cursor())];
    content::LoadReport report;

    if (mode_ == SlotMenuMode::Save) {
        // Destructive-action confirmation (M22): overwriting an existing
        // save requires a second Confirm on the same slot.
        if (context_.saves.exists(slot) && pendingOverwrite_ != menu_.cursor()) {
            pendingOverwrite_ = menu_.cursor();
            message_ = std::string("Overwrite ") + std::string(save::slotDisplayName(slot)) +
                       "? Confirm again to overwrite.";
            return;
        }
        pendingOverwrite_ = -1;
        if (context_.saves.save(slot, context_.party, report)) {
            message_ = std::string("Saved to ") + save::slotDisplayName(slot);
        } else {
            message_ = "Save failed (see log)";
        }
        rebuild();
    } else {
        if (context_.saves.load(slot, context_.party, report)) {
            stack().clearStates();
            stack().pushState(std::make_unique<TownState>(stack(), context_));
        } else {
            message_ = "Load failed: save is missing or invalid";
        }
    }
}

void SlotMenuState::handleInput(const Input& input) {
    if (input.navPressed(InputAction::MoveUp)) {
        menu_.moveUp();
        disarmOverwrite();
    }
    if (input.navPressed(InputAction::MoveDown)) {
        menu_.moveDown();
        disarmOverwrite();
    }
    if (input.pressed(InputAction::Confirm)) {
        confirmSelection();
    }
    if (input.pressed(InputAction::Cancel)) {
        if (pendingOverwrite_ >= 0) {
            disarmOverwrite();  // keep the save; stay on the screen
            return;
        }
        stack().popState();
    }
}

void SlotMenuState::disarmOverwrite() {
    if (pendingOverwrite_ >= 0) {
        pendingOverwrite_ = -1;
        message_.clear();
    }
}

void SlotMenuState::render() {
    const int w = context_.virtualWidth;
    ClearBackground(Color{14, 14, 22, 255});

    const char* title = mode_ == SlotMenuMode::Save ? "Save Game" : "Load Game";
    ui::drawTextCentered(title, w / 2, 28, 18, RAYWHITE);

    ui::drawMenu(menu_, 70, 80, 24, 12, RAYWHITE, Color{90, 90, 110, 255},
                 Color{240, 220, 120, 255});

    if (!message_.empty()) {
        ui::drawTextCentered(message_.c_str(), w / 2, 180, 10, Color{170, 220, 170, 255});
    }
    const InputMap& map = context_.input.map();
    const ActiveDevice device = context_.input.activeDevice();
    const std::string footer =
        input::prompt(map, InputAction::Confirm, device,
                      mode_ == SlotMenuMode::Save ? "Save" : "Load") +
        "    " + input::prompt(map, InputAction::Cancel, device, "Back");
    ui::drawTextCentered(footer.c_str(), w / 2, 218, 10, Color{150, 150, 170, 255});
}

}  // namespace cd
