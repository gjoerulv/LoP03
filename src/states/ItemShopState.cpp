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
#include "states/ItemShopFilter.hpp"
#include "states/StateStack.hpp"
#include "ui/UiDraw.hpp"
#include "ui/UiStyle.hpp"

namespace cd {

namespace style = ui::style;

namespace {
constexpr int kListX = 50;
constexpr int kListY = 54;
constexpr int kListItemH = 14;
constexpr int kVisibleRows = 9;
constexpr int kLabelW = 300;
}  // namespace

ItemShopState::ItemShopState(StateStack& stack, AppContext& context)
    : GameState(stack), context_(context) {}

void ItemShopState::onEnter() { rebuild(); }

void ItemShopState::rebuild() {
    // M43: consumables are stocked by the same town window gear uses, so a
    // town-1-only item (Royal Snacks) is genuinely town-1-only.
    ids_ = itemShopBuyIds(context_.content, context_.party.currentTown);

    std::vector<ui::MenuItem> items;
    for (const std::string& id : ids_) {
        const content::ItemDef* it = context_.content.findItem(id);
        const int owned = context_.party.inventory.count(id);
        // Name column left, owned-count + price columns right (M46): the
        // suffix is right-aligned by drawMenuScrolled and padded to a fixed
        // width (monospace glyphs), so both value columns line up per row.
        items.push_back({it->name, true, TextFormat("x%-3d%5dg", owned, it->value)});
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
                messageIsError_ = false;
                rebuild();
            } else {
                context_.audio.play(Sfx::Error);
                message_ = "Not enough gold for " + it->name;
                messageIsError_ = true;
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
    const style::Palette& p = style::palette();
    ui::drawSceneBackground(context_.resources, "bg.item_shop", p.canvas,
                            context_.virtualWidth, context_.virtualHeight, context_.party.currentTown);

    // Shop identity header with the gold badge (M46).
    ui::drawHeaderBand("Item Shop", w, p.success, context_.party.gold);

    // Transient feedback rides a compact banner between header and wares.
    if (!message_.empty()) {
        ui::drawBanner(messageIsError_ ? ui::BannerKind::Danger : ui::BannerKind::Success,
                       message_, 60, 27, w - 120, "itemshop.message");
    }

    // Inset merchandise panel; name column left, count/price columns right.
    ui::drawFrame(kListX - 24, kListY - 8, kLabelW + 52, kVisibleRows * kListItemH + 14,
                  ui::FrameStyle::Inset);
    ui::drawMenuScrolled(menu_, scroll_, kVisibleRows, kListX, kListY, kListItemH,
                         style::kFontMenu, kLabelW, p.text, p.disabled, p.cursor,
                         "itemshop.list", style::kFontSmall, p.gold);

    // Description panel for the selected consumable (existing data only).
    const int infoY = kListY + kVisibleRows * kListItemH + 12;
    ui::drawFrame(kListX - 24, infoY, kLabelW + 52, h - style::kFooterHeight - infoY - 4,
                  ui::FrameStyle::Standard);
    if (!ids_.empty()) {
        const content::ItemDef* it =
            context_.content.findItem(ids_[static_cast<std::size_t>(menu_.cursor())]);
        if (it != nullptr && !it->description.empty()) {
            ui::drawTextWrapped(it->description, kListX - 14, infoY + 6, kLabelW + 32,
                                style::kFontBody, p.textDim, "itemshop.detail", 2);
        }
    }

    const InputMap& map = context_.input.map();
    const ActiveDevice device = context_.input.activeDevice();
    ui::drawFooterHints({{input::primaryLabel(map, InputAction::Confirm, device), "Buy"},
                         {input::primaryLabel(map, InputAction::Cancel, device), "Back"}},
                        w, h, "itemshop.footer");
}

}  // namespace cd
