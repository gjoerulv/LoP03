#include "states/ArmoryGhostState.hpp"

#include <memory>
#include <string>
#include <vector>

#include "audio/AudioManager.hpp"
#include "content/ContentDatabase.hpp"
#include "content/Definitions.hpp"
#include "core/AppContext.hpp"
#include "dungeon/DungeonModel.hpp"
#include "dungeon/ThemeEvents.hpp"
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
constexpr int kListX = 60;
constexpr int kListY = 54;
constexpr int kListItemH = 14;
constexpr int kVisibleRows = 8;
constexpr int kLabelW = 260;

const char* rarityName(content::Rarity r) {
    switch (r) {
        case content::Rarity::Common: return "Common";
        case content::Rarity::Uncommon: return "Uncommon";
        case content::Rarity::Rare: return "Rare";
        case content::Rarity::Epic: return "Epic";
        case content::Rarity::Legendary: return "Legendary";
    }
    return "";
}

const char* slotName(content::EquipSlot s) {
    switch (s) {
        case content::EquipSlot::Weapon: return "Weapon";
        case content::EquipSlot::Armor: return "Armor";
        case content::EquipSlot::Accessory: return "Accessory";
        case content::EquipSlot::None: break;
    }
    return "Gear";
}

bool tradeable(const content::ItemDef& it) {
    const bool equip = it.type == content::ItemType::Equipment ||
                       it.type == content::ItemType::Relic;
    return equip && it.slot != content::EquipSlot::None &&
           it.rarity != content::Rarity::Legendary;  // the ghost declines legendaries
}
}  // namespace

ArmoryGhostState::ArmoryGhostState(StateStack& stack, AppContext& context, std::uint64_t seed,
                                   int roomIndex, dungeon::RoomEvent* event)
    : GameState(stack), context_(context), seed_(seed), roomIndex_(roomIndex), event_(event) {
    rebuild();
}

void ArmoryGhostState::rebuild() {
    ids_.clear();
    std::vector<ui::MenuItem> items;
    for (const ItemStack& s : context_.party.inventory.stacks) {
        const content::ItemDef* it = context_.content.findItem(s.itemId);
        if (it == nullptr || !tradeable(*it)) {
            continue;
        }
        ids_.push_back(s.itemId);
        items.push_back({it->name, true, "x" + std::to_string(s.count)});
    }
    const int previous = menu_.cursor();
    menu_.setItems(std::move(items));
    menu_.setCursor(previous);
    scroll_.follow(static_cast<int>(menu_.size()), kVisibleRows, menu_.cursor());
}

void ArmoryGhostState::trade() {
    if (ids_.empty() || menu_.cursor() < 0 ||
        menu_.cursor() >= static_cast<int>(ids_.size())) {
        return;
    }
    const std::string tradedId = ids_[static_cast<std::size_t>(menu_.cursor())];
    // FNV-1a of the offered id -> salt, so the ghost's pick is deterministic per
    // (dungeon seed, room, offered piece) and a reload reproduces it.
    std::uint64_t salt = 1469598103934665603ull;
    for (char ch : tradedId) {
        salt ^= static_cast<unsigned char>(ch);
        salt *= 1099511628211ull;
    }
    const std::uint64_t h = dungeon::themeEventHash(seed_, roomIndex_, salt);
    const std::string upgrade = dungeon::armoryGhostUpgrade(context_.content, tradedId, h);
    if (upgrade.empty()) {
        // No finer piece exists in that slot (shouldn't happen for the shipped
        // roster, but the ghost never eats a piece for nothing).
        context_.audio.play(Sfx::Error);
        message_ = "The ghost finds nothing finer for that. Offer another.";
        return;
    }
    context_.party.inventory.remove(tradedId, 1);
    context_.party.inventory.add(upgrade, 1);
    if (event_ != nullptr) {
        event_->resolved = true;
    }
    const content::ItemDef* gave = context_.content.findItem(tradedId);
    const content::ItemDef* got = context_.content.findItem(upgrade);
    context_.audio.play(Sfx::Confirm);
    message_ = "The ghost takes your " + (gave != nullptr ? gave->name : tradedId) +
               " and leaves a " + (got != nullptr ? got->name : upgrade) + "!";
    done_ = true;
    rebuild();  // the offered piece is gone; refresh the list under the message
}

void ArmoryGhostState::handleInput(const Input& input) {
    if (done_) {
        if (input.pressed(InputAction::Confirm) || input.pressed(InputAction::Cancel)) {
            context_.audio.play(Sfx::Cancel);
            stack().popState();
        }
        return;
    }
    if (input.navPressed(InputAction::MoveUp)) {
        menu_.moveUp();
        context_.audio.play(Sfx::Move);
    }
    if (input.navPressed(InputAction::MoveDown)) {
        menu_.moveDown();
        context_.audio.play(Sfx::Move);
    }
    scroll_.follow(static_cast<int>(menu_.size()), kVisibleRows, menu_.cursor());
    if (input.pressed(InputAction::Confirm) && !ids_.empty() && menu_.currentEnabled()) {
        trade();
    }
    if (input.pressed(InputAction::Cancel)) {
        context_.audio.play(Sfx::Cancel);
        stack().popState();  // the ghost waits; the event stays unresolved
    }
}

void ArmoryGhostState::render() {
    const int w = context_.virtualWidth;
    const int h = context_.virtualHeight;
    const style::Palette& p = style::palette();
    ui::drawModalDim(w, h);
    ui::drawTitlePlaque("The Armory Ghost", w / 2, 16, 14);

    ui::drawFrame(kListX - 24, kListY - 8, kLabelW + 52, kVisibleRows * kListItemH + 14,
                  ui::FrameStyle::Inset);
    if (ids_.empty()) {
        ui::drawTextWrappedCentered("You carry no gear to offer. The ghost sighs and fades.",
                                    w / 2, kListY + 20, kLabelW, style::kFontBody, p.textDim,
                                    "ghost.empty", 2);
    } else {
        ui::drawMenuScrolled(menu_, scroll_, kVisibleRows, kListX, kListY, kListItemH,
                             style::kFontMenu, kLabelW, p.text, p.disabled, p.cursor, "ghost.list",
                             style::kFontSmall, p.textDim);
    }

    // Detail line for the highlighted piece: what slot/rarity it is, and that the
    // return is one tier finer, sight unseen.
    const int infoY = kListY + kVisibleRows * kListItemH + 12;
    if (!done_ && !ids_.empty() && menu_.cursor() >= 0 &&
        menu_.cursor() < static_cast<int>(ids_.size())) {
        if (const content::ItemDef* it =
                context_.content.findItem(ids_[static_cast<std::size_t>(menu_.cursor())])) {
            const std::string line = std::string("Offer this ") + rarityName(it->rarity) + " " +
                                     slotName(it->slot) + " for a random " +
                                     rarityName(dungeon::nextRarityUp(it->rarity)) + " " +
                                     slotName(it->slot) + " - sight unseen.";
            ui::drawTextWrapped(line, kListX - 14, infoY + 6, kLabelW + 28, style::kFontBody,
                                p.textDim, "ghost.detail", 2);
        }
    }

    if (!message_.empty()) {
        ui::drawBanner(ui::BannerKind::Reward, message_, 40, h - 46, w - 80, "ghost.message");
    }

    const InputMap& map = context_.input.map();
    const ActiveDevice device = context_.input.activeDevice();
    if (done_) {
        ui::drawFooterHints({{input::primaryLabel(map, InputAction::Confirm, device), "Leave"}}, w,
                            h, "ghost.footer");
    } else {
        ui::drawFooterHints({{input::primaryLabel(map, InputAction::Confirm, device), "Offer"},
                             {input::primaryLabel(map, InputAction::Cancel, device), "Leave"}},
                            w, h, "ghost.footer");
    }
}

}  // namespace cd
