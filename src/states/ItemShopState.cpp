#include "states/ItemShopState.hpp"

#include <algorithm>
#include <utility>

#include "audio/AudioManager.hpp"
#include "content/ContentDatabase.hpp"
#include "content/Definitions.hpp"
#include "core/AppContext.hpp"
#include "game/Party.hpp"
#include "input/Input.hpp"
#include "raylib.h"
#include "states/StateStack.hpp"
#include "ui/UiDraw.hpp"

namespace cd {

ItemShopState::ItemShopState(StateStack& stack, AppContext& context)
    : GameState(stack), context_(context) {}

void ItemShopState::onEnter() { rebuild(); }

void ItemShopState::rebuild() {
    ids_.clear();
    for (const auto& [id, def] : context_.content.items()) {
        if (def.type == content::ItemType::Consumable) {
            ids_.push_back(id);
        }
    }
    std::sort(ids_.begin(), ids_.end());

    std::vector<ui::MenuItem> items;
    for (const std::string& id : ids_) {
        const content::ItemDef* it = context_.content.findItem(id);
        const int owned = context_.party.inventory.count(id);
        items.push_back({it->name + "  -  " + std::to_string(it->value) + "g  (x" +
                             std::to_string(owned) + ")",
                         true});
    }
    const int previous = menu_.cursor();
    menu_.setItems(std::move(items));
    menu_.setCursor(previous);
}

void ItemShopState::handleInput(const Input& input) {
    if (input.pressed(InputAction::MoveUp)) {
        menu_.moveUp();
        context_.audio.play(Sfx::Move);
    }
    if (input.pressed(InputAction::MoveDown)) {
        menu_.moveDown();
        context_.audio.play(Sfx::Move);
    }
    if (input.pressed(InputAction::Confirm) && !ids_.empty()) {
        const content::ItemDef* it =
            context_.content.findItem(ids_[static_cast<std::size_t>(menu_.cursor())]);
        if (it != nullptr) {
            if (context_.party.gold >= it->value) {
                context_.party.gold -= it->value;
                context_.party.inventory.add(it->id, 1);
                context_.audio.play(Sfx::Confirm);
                message_ = "Bought " + it->name;
                rebuild();
            } else {
                context_.audio.play(Sfx::Cancel);
                message_ = "Not enough gold for " + it->name;
            }
        }
    }
    if (input.pressed(InputAction::Cancel)) {
        context_.audio.play(Sfx::Cancel);
        stack().popState();
    }
}

void ItemShopState::render() {
    const int w = context_.virtualWidth;
    const int h = context_.virtualHeight;
    ClearBackground(Color{16, 16, 24, 255});
    ui::drawTextCentered("Item Shop", w / 2, 14, 16, RAYWHITE);
    DrawText(TextFormat("Gold: %d", context_.party.gold), w - 110, 14, 11,
             Color{230, 220, 150, 255});

    ui::drawMenu(menu_, 50, 44, 14, 11, RAYWHITE, Color{90, 90, 110, 255},
                 Color{240, 220, 120, 255});

    if (!message_.empty()) {
        ui::drawTextCentered(message_.c_str(), w / 2, h - 30, 10, Color{170, 220, 170, 255});
    }
    ui::drawTextCentered("Confirm: Buy    Cancel: Back", w / 2, h - 14, 10,
                         Color{150, 150, 170, 255});
}

}  // namespace cd
