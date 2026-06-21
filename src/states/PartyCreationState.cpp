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
        for (int c = GetCharPressed(); c > 0; c = GetCharPressed()) {
            slots_[static_cast<std::size_t>(cursor_)].name.appendCodepoint(c);
        }
        if (IsKeyPressed(KEY_BACKSPACE)) {
            slots_[static_cast<std::size_t>(cursor_)].name.backspace();
        }
        if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_ESCAPE)) {
            editing_ = false;
        }
        return;
    }

    if (input.pressed(InputAction::MoveUp)) {
        cursor_ = (cursor_ + kRows - 1) % kRows;
    }
    if (input.pressed(InputAction::MoveDown)) {
        cursor_ = (cursor_ + 1) % kRows;
    }
    if (cursor_ < kBeginRow) {
        if (input.pressed(InputAction::MoveLeft)) {
            cycleClass(cursor_, -1);
        }
        if (input.pressed(InputAction::MoveRight)) {
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
            DrawText(">", 36, y, 12, yellow);
        }
        std::string nameText = slots_[static_cast<std::size_t>(i)].name.value();
        if (editing_ && selected) {
            nameText += (std::fmod(caretTimer_, 1.0f) < 0.5f) ? "_" : " ";
        }
        DrawText(nameText.c_str(), 54, y, 12, selected ? yellow : RAYWHITE);

        const content::ClassDef* cls =
            classes_[static_cast<std::size_t>(slots_[static_cast<std::size_t>(i)].classIndex)];
        DrawText(TextFormat("<  %s  >", cls->name.c_str()), 190, y, 12,
                 selected ? yellow : Color{170, 190, 220, 255});
    }

    const int beginY = baseY + 4 * 24 + 10;
    const bool beginSel = cursor_ == kBeginRow;
    if (beginSel) {
        DrawText(">", 36, beginY, 12, yellow);
    }
    DrawText("Begin Adventure", 54, beginY, 12, beginSel ? yellow : RAYWHITE);

    if (editing_) {
        ui::drawTextCentered("Editing name - type, Backspace, Enter to finish", w / 2, h - 30, 10,
                             Color{170, 220, 170, 255});
    }
    ui::drawTextCentered("Up/Down: Move   Left/Right: Class   Confirm: Edit/Begin   Cancel: Back",
                         w / 2, h - 16, 10, Color{150, 150, 170, 255});
}

}  // namespace cd
