#include "states/PartyCreationState.hpp"

#include <algorithm>
#include <cmath>
#include <memory>
#include <string>

#include "content/ContentDatabase.hpp"
#include "content/Definitions.hpp"
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
constexpr int kBeginRow = 4;
constexpr int kRows = 5;
const char* const kDefaultNames[4] = {"Rolan", "Mira", "Sable", "Pell"};
}  // namespace

PartyCreationState::PartyCreationState(StateStack& stack, AppContext& context)
    : GameState(stack), context_(context) {}

void PartyCreationState::onEnter() {
    // Canonical class order first, then any extras the data may add.
    static const char* const order[] = {"knight", "ranger", "mage", "cleric", "rogue", "guardian"};
    classes_.clear();
    for (const char* id : order) {
        if (const content::ClassDef* c = context_.content.findClass(id)) {
            classes_.push_back(c);
        }
    }
    for (const auto& [id, def] : context_.content.classes()) {
        if (std::find(classes_.begin(), classes_.end(), &def) == classes_.end()) {
            classes_.push_back(&def);
        }
    }

    ready_ = !classes_.empty();
    for (int i = 0; i < 4; ++i) {
        slots_[static_cast<std::size_t>(i)].classIndex =
            ready_ ? (i % static_cast<int>(classes_.size())) : 0;
        slots_[static_cast<std::size_t>(i)].name = ui::TextInput(12, kDefaultNames[i]);
    }
    cursor_ = 0;
    editing_ = false;
}

void PartyCreationState::cycleClass(int slotIndex, int direction) {
    if (classes_.empty()) {
        return;
    }
    const int n = static_cast<int>(classes_.size());
    Slot& slot = slots_[static_cast<std::size_t>(slotIndex)];
    slot.classIndex = ((slot.classIndex + direction) % n + n) % n;
}

void PartyCreationState::begin() {
    context_.party.members.clear();
    context_.party.gold = 150;  // a little starting gold for the shops
    for (std::size_t i = 0; i < slots_.size(); ++i) {
        std::string name = slots_[i].name.trimmed();
        if (name.empty()) {
            name = kDefaultNames[i];
        }
        const content::ClassDef* cls = classes_[static_cast<std::size_t>(slots_[i].classIndex)];
        context_.party.members.push_back(createCharacter(*cls, name));
    }

    // Starter supplies so the Item command and revives are usable from the start.
    context_.party.inventory = Inventory{};
    context_.party.inventory.add("potion", 3);
    context_.party.inventory.add("antidote", 1);
    context_.party.inventory.add("ether", 1);
    context_.party.inventory.add("phoenix_tear", 1);

    stack().clearStates();
    stack().pushState(std::make_unique<TownState>(stack(), context_));
}

void PartyCreationState::handleInput(const Input& input) {
    if (!ready_) {
        if (input.pressed(InputAction::Cancel)) {
            stack().popState();
        }
        return;
    }

    if (editing_) {
        // Everything flows through the input layer (M13; fixes the
        // gamepad-only soft-lock UI-INPUT-001): typed characters via
        // nextChar, delete via TextBackspace (Backspace / gamepad X), and
        // Confirm or Cancel — from either device — finishes editing.
        // TextBackspace wins over Cancel when both fire from the same
        // Backspace press.
        for (int c = input.nextChar(); c > 0; c = input.nextChar()) {
            slots_[static_cast<std::size_t>(cursor_)].name.appendCodepoint(c);
        }
        if (input.navPressed(InputAction::TextBackspace)) {
            slots_[static_cast<std::size_t>(cursor_)].name.backspace();
        } else if (input.pressed(InputAction::Confirm) || input.pressed(InputAction::Cancel)) {
            editing_ = false;
        }
        return;
    }

    if (input.navPressed(InputAction::MoveUp)) {
        cursor_ = (cursor_ + kRows - 1) % kRows;
    }
    if (input.navPressed(InputAction::MoveDown)) {
        cursor_ = (cursor_ + 1) % kRows;
    }
    if (cursor_ < kBeginRow) {
        if (input.navPressed(InputAction::MoveLeft)) {
            cycleClass(cursor_, -1);
        }
        if (input.navPressed(InputAction::MoveRight)) {
            cycleClass(cursor_, +1);
        }
    }
    if (input.pressed(InputAction::Confirm)) {
        if (cursor_ < kBeginRow) {
            editing_ = true;
        } else {
            begin();
        }
    }
    if (input.pressed(InputAction::Cancel)) {
        stack().popState();
    }
}

void PartyCreationState::update(float dt) { caretTimer_ += dt; }

void PartyCreationState::render() {
    const int w = context_.virtualWidth;
    const int h = context_.virtualHeight;
    ClearBackground(Color{16, 16, 28, 255});
    ui::drawTextCentered("Create Your Party", w / 2, 14, 18, RAYWHITE);

    if (!ready_) {
        ui::drawTextCentered("No classes loaded. Press Cancel to go back.", w / 2, h / 2, 12,
                             Color{220, 150, 150, 255});
        return;
    }

    const int baseY = 58;
    const Color yellow{240, 220, 120, 255};
    for (int i = 0; i < 4; ++i) {
        const int y = baseY + i * 24;
        const bool selected = cursor_ == i;
        if (selected) {
            ui::drawText(">", 36, y, 12, yellow);
        }
        std::string nameText = slots_[static_cast<std::size_t>(i)].name.value();
        if (editing_ && selected) {
            nameText += (std::fmod(caretTimer_, 1.0f) < 0.5f) ? "_" : " ";
        }
        ui::drawText(nameText.c_str(), 54, y, 12, selected ? yellow : RAYWHITE);

        const content::ClassDef* cls =
            classes_[static_cast<std::size_t>(slots_[static_cast<std::size_t>(i)].classIndex)];
        ui::drawText(TextFormat("<  %s  >", cls->name.c_str()), 190, y, 12,
                 selected ? yellow : Color{170, 190, 220, 255});
    }

    const int beginY = baseY + 4 * 24 + 10;
    const bool beginSel = cursor_ == kBeginRow;
    if (beginSel) {
        ui::drawText(">", 36, beginY, 12, yellow);
    }
    ui::drawText("Begin Adventure", 54, beginY, 12, beginSel ? yellow : RAYWHITE);

    const InputMap& map = context_.input.map();
    const ActiveDevice device = context_.input.activeDevice();
    if (editing_) {
        const std::string editHint =
            device == ActiveDevice::Keyboard
                ? "Type a name   " +
                      input::prompt(map, InputAction::TextBackspace, device, "Delete") +
                      "   " + input::prompt(map, InputAction::Confirm, device, "Finish")
                : input::prompt(map, InputAction::TextBackspace, device, "Delete") + "   " +
                      input::prompt(map, InputAction::Confirm, device, "Finish") +
                      "   (typing needs a keyboard)";
        ui::drawTextCentered(editHint.c_str(), w / 2, h - 30, 10, Color{170, 220, 170, 255});
    }
    const std::string footer =
        "[" + input::primaryLabel(map, InputAction::MoveUp, device) + "/" +
        input::primaryLabel(map, InputAction::MoveDown, device) + "] Move   [" +
        input::primaryLabel(map, InputAction::MoveLeft, device) + "/" +
        input::primaryLabel(map, InputAction::MoveRight, device) + "] Class   " +
        input::prompt(map, InputAction::Confirm, device, "Edit/Begin") + "   " +
        input::prompt(map, InputAction::Cancel, device, "Back");
    ui::drawTextCentered(footer.c_str(), w / 2, h - 16, 9, Color{150, 150, 170, 255});
}

}  // namespace cd
