#include "states/BlackMarketState.hpp"

#include <utility>
#include <vector>

#include "audio/AudioManager.hpp"
#include "content/ContentDatabase.hpp"
#include "content/Definitions.hpp"
#include "core/AppContext.hpp"
#include "game/BlackMarket.hpp"
#include "game/Party.hpp"
#include "input/Input.hpp"
#include "input/PromptLabels.hpp"
#include "raylib.h"
#include "states/StateStack.hpp"
#include "states/TutorialPromptState.hpp"
#include "tutorial/Tutorial.hpp"
#include "ui/UiDraw.hpp"
#include "ui/UiStyle.hpp"

namespace cd {

namespace {

// "ATK +6  DEF +6  HP +40" style summary of a piece of gear's stat bonus.
std::string statBonusSummary(const content::StatBlock& b) {
    std::string out;
    const auto add = [&out](const char* tag, int v) {
        if (v != 0) {
            out += (out.empty() ? "" : "  ") + std::string(tag) + (v > 0 ? " +" : " ") +
                   std::to_string(v);
        }
    };
    add("HP", b.maxHp);
    add("ATK", b.attack);
    add("MAG", b.magic);
    add("DEF", b.defense);
    add("SPD", b.speed);
    return out;
}

}  // namespace

BlackMarketState::BlackMarketState(StateStack& stack, AppContext& context)
    : GameState(stack), context_(context) {}

void BlackMarketState::onEnter() {
    itemId_ = context_.party.blackMarket.itemId;
    priceGold_ = context_.party.blackMarket.priceGold;
    purchased_ = false;
    rebuild();
    maybeTutorialPrompt(stack(), context_, tutorial::kFirstMarket);
}

void BlackMarketState::rebuild() {
    // Fixed rows: buy-with-gold (0), buy-with-tokens (1), leave (2). The buy rows
    // disable when unaffordable or already sold; Leave is always enabled.
    const int tokens = context_.party.legendaryTokens;
    const bool canGold = !purchased_ && context_.party.gold >= priceGold_;
    const bool canTokens = !purchased_ && tokens >= kBlackMarketTokenPrice;

    std::string goldLabel;
    std::string tokenLabel;
    if (purchased_) {
        goldLabel = "Buy with gold  -  sold";
        tokenLabel = "Buy with tokens  -  sold";
    } else {
        goldLabel = TextFormat("Buy for %dg%s", priceGold_,
                               canGold ? "" : "  (not enough gold)");
        tokenLabel = TextFormat("Buy for %d legendary tokens  (%d held)%s",
                                kBlackMarketTokenPrice, tokens,
                                canTokens ? "" : "  (not enough)");
    }

    std::vector<ui::MenuItem> items;
    items.push_back({goldLabel, canGold});
    items.push_back({tokenLabel, canTokens});
    items.push_back({"Leave", true});
    const int previous = menu_.cursor();
    menu_.setItems(std::move(items));
    menu_.setCursor(previous);
}

void BlackMarketState::grantOffered() {
    context_.party.inventory.add(itemId_, 1);
    context_.party.blackMarket = BlackMarketOffer{};  // consumed
    purchased_ = true;
    context_.audio.play(Sfx::Confirm);
    const content::ItemDef* it = context_.content.findItem(itemId_);
    message_ = "The " + (it != nullptr ? it->name : itemId_) + " is yours. The dealer vanishes.";
    rebuild();
}

void BlackMarketState::handleInput(const Input& input) {
    if (input.navPressed(InputAction::MoveUp)) {
        menu_.moveUp();
        context_.audio.play(Sfx::Move);
    }
    if (input.navPressed(InputAction::MoveDown)) {
        menu_.moveDown();
        context_.audio.play(Sfx::Move);
    }
    if (input.pressed(InputAction::Confirm)) {
        const int cursor = menu_.cursor();
        if (cursor == 2) {  // Leave
            context_.audio.play(Sfx::Cancel);
            stack().popState();
            return;
        }
        if (!menu_.currentEnabled()) {
            context_.audio.play(Sfx::Error);
            return;
        }
        if (cursor == 0) {  // gold
            context_.party.gold -= priceGold_;
            grantOffered();
        } else if (cursor == 1) {  // tokens
            context_.party.legendaryTokens -= kBlackMarketTokenPrice;
            grantOffered();
        }
    }
    if (input.pressed(InputAction::Cancel)) {
        context_.audio.play(Sfx::Cancel);
        stack().popState();
    }
}

void BlackMarketState::render() {
    const int w = context_.virtualWidth;
    const int h = context_.virtualHeight;
    namespace style = ui::style;
    const style::Palette& p = style::palette();
    ClearBackground(p.canvas);

    ui::drawHeaderBand("Black Market", w, p.magic, context_.party.gold);
    ui::drawChip(TextFormat("Tokens %d", context_.party.legendaryTokens), 4, 27, p.magic);
    ui::drawTextCentered("A hooded dealer offers a single legendary.", w / 2, 30,
                         style::kFontBody, p.textDim);

    // The single legendary offer sits in a Reward frame.
    const content::ItemDef* it = context_.content.findItem(itemId_);
    if (it != nullptr) {
        const char* slot = it->slot == content::EquipSlot::Weapon      ? "Weapon"
                           : it->slot == content::EquipSlot::Armor     ? "Armor"
                           : it->slot == content::EquipSlot::Accessory ? "Accessory"
                                                                       : "Relic";
        ui::drawFrame(32, 46, w - 64, 70, ui::FrameStyle::Reward);
        ui::drawTextFitted(TextFormat("%s  (Legendary %s)", it->name.c_str(), slot), 46, 53,
                           w - 92, 12, p.gold, "market.name");
        ui::drawTextFitted(statBonusSummary(it->statBonus), 46, 70, w - 92, style::kFontBody,
                           p.success, "market.stats");
        ui::drawTextWrapped(it->description, 46, 85, w - 92, style::kFontBody, p.textDim,
                            "market.desc", 2);
    }

    const int rows = static_cast<int>(menu_.size());
    ui::drawFrame(44, 122, 300, rows * 18 + 14, ui::FrameStyle::Inset);
    ui::drawMenu(menu_, 66, 131, 18, style::kFontMenu, p.text, p.disabled, p.cursor);

    if (!message_.empty()) {
        ui::drawBanner(ui::BannerKind::Reward, message_, 60, h - 46, w - 120, "market.message");
    }
    const InputMap& map = context_.input.map();
    const ActiveDevice device = context_.input.activeDevice();
    ui::drawFooterHints({{input::primaryLabel(map, InputAction::Confirm, device), "Choose"},
                         {input::primaryLabel(map, InputAction::Cancel, device), "Leave"}},
                        w, h, "market.footer");
}

}  // namespace cd
