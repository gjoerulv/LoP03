#include "states/ItemShopState.hpp"

#include <algorithm>
#include <utility>

#include "audio/AudioManager.hpp"
#include "content/ContentDatabase.hpp"
#include "content/Definitions.hpp"
#include "core/AppContext.hpp"
#include "game/Party.hpp"
#include "input/Input.hpp"
#include "input/PromptLabels.hpp"
#include "raylib.h"
#include "states/StateStack.hpp"
#include "ui/UiDraw.hpp"
#include "ui/UiStyle.hpp"

namespace cd {

namespace style = ui::style;

namespace {
constexpr int kListX = 50;
constexpr int kListY = 48;
constexpr int kListItemH = 14;
constexpr int kVisibleRows = 10;
}  // namespace

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
    scroll_.follow(static_cast<int>(menu_.size()), kVisibleRows, menu_.cursor());
}

void ItemShopState::handleInput(const Input& input) {
    if (input.navPressed(InputAction::MoveUp)) {
        menu_.moveUp();
        context_.audio.play(Sfx::Move);
    }
    if (input.navPressed(InputAction::MoveDown)) {
        menu_.moveDown();
        context_.audio.play(Sfx::Move);
    }
    scroll_.follow(static_cast<int>(menu_.size()), kVisibleRows, menu_.cursor());
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
    ui::drawTextCentered("Item Shop", w / 2, 14, style::kFontScreenTitle, style::kText);
    ui::drawTextRight(TextFormat("Gold: %d", context_.party.gold), w - 14, 14, style::kFontMenu,
                      style::kGold);
    if (!message_.empty()) {
        ui::drawTextFitted(message_, 20, 32, w - 40, style::kFontBody, style::kSuccess,
                           "itemshop.message");
    }

    ui::drawMenuScrolled(menu_, scroll_, kVisibleRows, kListX, kListY, kListItemH,
                         style::kFontMenu, 300, style::kText, style::kDisabled, style::kCursor,
                         "itemshop.list");

    // Description of the selected consumable.
    if (!ids_.empty()) {
        const content::ItemDef* it =
            context_.content.findItem(ids_[static_cast<std::size_t>(menu_.cursor())]);
        if (it != nullptr && !it->description.empty()) {
            ui::drawTextWrapped(it->description, 20, h - 52, w - 40, style::kFontBody,
                                style::kSuccess, "itemshop.detail", 2);
        }
    }

    const InputMap& map = context_.input.map();
    const ActiveDevice device = context_.input.activeDevice();
    const std::string footer = input::prompt(map, InputAction::Confirm, device, "Buy") +
                               "    " + input::prompt(map, InputAction::Cancel, device, "Back");
    ui::drawTextCentered(footer.c_str(), w / 2, h - style::kFooterHeight + 2, style::kFontBody,
                         style::kTextHint);
}

}  // namespace cd
