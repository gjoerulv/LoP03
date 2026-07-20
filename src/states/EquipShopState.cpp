#include "states/EquipShopState.hpp"

#include <algorithm>
#include <utility>

#include <memory>

#include "audio/AudioManager.hpp"
#include "content/ContentDatabase.hpp"
#include "states/DetailsOverlayState.hpp"
#include "states/EquipShopFilter.hpp"
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
const char* const kSlotNames[3] = {"Weapon", "Armor", "Accessory"};
// Buy-list categories (M31), parallel to slotEnum(0..2): Weapon/Armor/Accessory.
const char* const kCategoryNames[3] = {"Weapons", "Armor", "Accessories"};

constexpr int kListX = 40;
constexpr int kListY = 64;
constexpr int kListItemH = 14;
constexpr int kVisibleRows = 9;

std::string statBonusSummary(const content::StatBlock& b) {
    std::string out;
    auto add = [&out](const char* tag, int v) {
        if (v != 0) {
            out += (out.empty() ? "" : " ") + std::string(tag) + (v > 0 ? "+" : "") +
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

// One-line summary of a piece of gear: slot, stat bonus, description.
std::string equipDetail(const content::ItemDef& it) {
    std::string out;
    switch (it.slot) {
        case content::EquipSlot::Weapon: out = "Weapon"; break;
        case content::EquipSlot::Armor: out = "Armor"; break;
        case content::EquipSlot::Accessory: out = "Accessory"; break;
        case content::EquipSlot::None: break;
    }
    const std::string stats = statBonusSummary(it.statBonus);
    if (!stats.empty()) {
        out += (out.empty() ? "" : "  ") + stats;
    }
    if (!it.description.empty()) {
        out += (out.empty() ? "" : "  -  ") + it.description;
    }
    return out;
}

content::EquipSlot slotEnum(int slot) {
    switch (slot) {
        case 0: return content::EquipSlot::Weapon;
        case 1: return content::EquipSlot::Armor;
        default: return content::EquipSlot::Accessory;
    }
}

std::string& slotRef(Character& c, int slot) {
    switch (slot) {
        case 0: return c.weapon;
        case 1: return c.armor;
        default: return c.accessory;
    }
}

// Player-facing name of the current buy category (M31), for hints/footers.
const char* categoryLabel(content::EquipSlot s) {
    switch (s) {
        case content::EquipSlot::Weapon: return "Weapons";
        case content::EquipSlot::Armor: return "Armor";
        case content::EquipSlot::Accessory: return "Accessories";
        case content::EquipSlot::None: break;
    }
    return "Gear";
}
}  // namespace

EquipShopState::EquipShopState(StateStack& stack, AppContext& context)
    : GameState(stack), context_(context) {
    // Populate the top menu at construction (phase_ defaults to Menu). Doing it
    // here rather than in onEnter lets the capture hook force a phase that
    // onEnter would otherwise reset, matching BattleState's capture pattern.
    rebuild();
}

#ifdef CRYSTAL_CAPTURE
void EquipShopState::captureEnterBuyCategory() {
    phase_ = Phase::BuyCategory;
    rebuild();
}
#endif

void EquipShopState::rebuild() {
    rowIds_.clear();
    std::vector<ui::MenuItem> items;

    switch (phase_) {
        case Phase::Menu:
            items = {{"Buy Gear", true}, {"Equip Party", !context_.party.empty()}, {"Back", true}};
            break;
        case Phase::BuyCategory:
            for (const char* name : kCategoryNames) {
                items.push_back({name, true});
            }
            break;
        case Phase::Buy: {
            const std::vector<std::string> ids =
                equipShopBuyIds(context_.content, buyCategory_);
            for (const std::string& id : ids) {
                const content::ItemDef* it = context_.content.findItem(id);
                rowIds_.push_back(id);
                items.push_back({it->name + "  -  " + std::to_string(it->value) + "g", true});
            }
            break;
        }
        case Phase::EquipChar:
            for (const Character& c : context_.party.members) {
                items.push_back({c.name, true});
            }
            break;
        case Phase::EquipSlot: {
            const Character& c = context_.party.members[static_cast<std::size_t>(selectedChar_)];
            const std::string eq[3] = {c.weapon, c.armor, c.accessory};
            for (int i = 0; i < 3; ++i) {
                std::string label = std::string(kSlotNames[i]) + ": ";
                if (eq[i].empty()) {
                    label += "(none)";
                } else if (const content::ItemDef* it = context_.content.findItem(eq[i])) {
                    label += it->name;
                } else {
                    label += eq[i];
                }
                items.push_back({label, true});
            }
            break;
        }
        case Phase::EquipItem: {
            items.push_back({"(Unequip)", true});
            std::vector<std::string> ids;
            for (const ItemStack& s : context_.party.inventory.stacks) {
                const content::ItemDef* it = context_.content.findItem(s.itemId);
                if (it != nullptr && isEquippableItem(*it) && it->slot == slotEnum(selectedSlot_)) {
                    ids.push_back(s.itemId);
                }
            }
            std::sort(ids.begin(), ids.end());
            for (const std::string& id : ids) {
                const content::ItemDef* it = context_.content.findItem(id);
                rowIds_.push_back(id);
                items.push_back({it->name + "  x" + std::to_string(context_.party.inventory.count(id)),
                                 true});
            }
            break;
        }
    }
    menu_.setItems(std::move(items));
    scroll_.reset();
    scroll_.follow(static_cast<int>(menu_.size()), kVisibleRows, menu_.cursor());
}

void EquipShopState::confirm() {
    if (!menu_.currentEnabled()) {
        return;
    }
    const int cursor = menu_.cursor();
    switch (phase_) {
        case Phase::Menu:
            if (cursor == 0) {
                phase_ = Phase::BuyCategory;
            } else if (cursor == 1) {
                phase_ = Phase::EquipChar;
            } else {
                stack().popState();
                return;
            }
            rebuild();
            break;
        case Phase::BuyCategory:
            buyCategory_ = slotEnum(cursor);  // 0 weapon, 1 armor, 2 accessory
            phase_ = Phase::Buy;
            rebuild();
            break;
        case Phase::Buy: {
            const content::ItemDef* it = context_.content.findItem(rowIds_[static_cast<std::size_t>(cursor)]);
            if (it == nullptr) {
                return;
            }
            if (context_.party.gold >= it->value) {
                context_.party.gold -= it->value;
                context_.party.inventory.add(it->id, 1);
                context_.audio.play(Sfx::Confirm);
                message_ = "Bought " + it->name;
            } else {
                context_.audio.play(Sfx::Error);
                message_ = "Not enough gold for " + it->name;
            }
            break;
        }
        case Phase::EquipChar:
            selectedChar_ = cursor;
            phase_ = Phase::EquipSlot;
            rebuild();
            break;
        case Phase::EquipSlot:
            selectedSlot_ = cursor;
            phase_ = Phase::EquipItem;
            rebuild();
            break;
        case Phase::EquipItem: {
            Character& c = context_.party.members[static_cast<std::size_t>(selectedChar_)];
            std::string& ref = slotRef(c, selectedSlot_);
            if (cursor == 0) {  // Unequip
                if (!ref.empty()) {
                    context_.party.inventory.add(ref, 1);
                    ref.clear();
                }
            } else {
                const std::string newId = rowIds_[static_cast<std::size_t>(cursor - 1)];
                if (!ref.empty()) {
                    context_.party.inventory.add(ref, 1);
                }
                context_.party.inventory.remove(newId, 1);
                ref = newId;
            }
            refreshCharacter(c, context_.content);
            phase_ = Phase::EquipSlot;
            rebuild();
            break;
        }
    }
}

namespace {

// "ATK +2  DEF -1" style delta between two equip bonuses; empty = no change.
std::string bonusDelta(const content::StatBlock& next, const content::StatBlock& cur) {
    std::string out;
    const auto add = [&out](const char* tag, int d) {
        if (d == 0) {
            return;
        }
        if (!out.empty()) {
            out += "  ";
        }
        out += std::string(tag) + (d > 0 ? " +" : " ") + std::to_string(d);
    };
    add("HP", next.maxHp - cur.maxHp);
    add("ATK", next.attack - cur.attack);
    add("MAG", next.magic - cur.magic);
    add("DEF", next.defense - cur.defense);
    add("SPD", next.speed - cur.speed);
    return out;
}

const char* slotLabel(content::EquipSlot s) {
    switch (s) {
        case content::EquipSlot::Weapon: return "Weapon";
        case content::EquipSlot::Armor: return "Armor";
        case content::EquipSlot::Accessory: return "Accessory";
        case content::EquipSlot::None: break;
    }
    return "Item";
}

}  // namespace

void EquipShopState::openItemDetails() {
    if (rowIds_.empty() || menu_.cursor() >= static_cast<int>(rowIds_.size())) {
        return;
    }
    const content::ItemDef* it =
        context_.content.findItem(rowIds_[static_cast<std::size_t>(menu_.cursor())]);
    if (it == nullptr) {
        return;
    }
    std::string body = it->name + " - " + slotLabel(it->slot) + ", " +
                       std::to_string(it->value) + "g.";
    const std::string own = bonusDelta(it->statBonus, content::StatBlock{});
    if (!own.empty()) {
        body += "\nBonus: " + own;
    }
    if (!it->description.empty()) {
        body += "\n" + it->description;
    }
    body += "\n\nIf equipped (vs current gear):";
    for (Character& m : context_.party.members) {
        const std::string& curId = it->slot == content::EquipSlot::Weapon  ? m.weapon
                                   : it->slot == content::EquipSlot::Armor ? m.armor
                                                                           : m.accessory;
        content::StatBlock cur{};
        if (const content::ItemDef* curItem = context_.content.findItem(curId)) {
            cur = curItem->statBonus;
        }
        const std::string delta = bonusDelta(it->statBonus, cur);
        body += "\n" + m.name + ": " + (delta.empty() ? "no change" : delta);
    }
    stack().pushState(
        std::make_unique<DetailsOverlayState>(stack(), context_, "Gear Details", body));
}

void EquipShopState::handleInput(const Input& input) {
    if (input.navPressed(InputAction::MoveUp)) {
        menu_.moveUp();
    }
    if (input.navPressed(InputAction::MoveDown)) {
        menu_.moveDown();
    }
    scroll_.follow(static_cast<int>(menu_.size()), kVisibleRows, menu_.cursor());
    if (phase_ == Phase::Buy && input.pressed(InputAction::Details)) {
        openItemDetails();
        return;
    }
    if (input.pressed(InputAction::Confirm)) {
        confirm();
    }
    if (input.pressed(InputAction::Cancel)) {
        switch (phase_) {
            case Phase::Menu: stack().popState(); break;
            case Phase::BuyCategory:
            case Phase::EquipChar: phase_ = Phase::Menu; rebuild(); break;
            case Phase::Buy: phase_ = Phase::BuyCategory; rebuild(); break;
            case Phase::EquipSlot: phase_ = Phase::EquipChar; rebuild(); break;
            case Phase::EquipItem: phase_ = Phase::EquipSlot; rebuild(); break;
        }
    }
}

void EquipShopState::render() {
    const int w = context_.virtualWidth;
    const int h = context_.virtualHeight;
    ui::drawSceneBackground(context_.resources, "bg.equip_shop", Color{16, 16, 24, 255},
                            context_.virtualWidth, context_.virtualHeight);
    ui::drawTextCentered("Equipment Shop", w / 2, 14, style::kFontScreenTitle, style::palette().text);
    ui::drawTextRight(TextFormat("Gold: %d", context_.party.gold), w - 14, 14, style::kFontMenu,
                      style::palette().gold);

    std::string buyHint;
    const char* hint = "Confirm: Select   Cancel: Back";
    switch (phase_) {
        case Phase::Menu: hint = "Buy gear or equip your party."; break;
        case Phase::BuyCategory: hint = "Choose a category to browse."; break;
        case Phase::Buy:
            buyHint = std::string("Buy ") + categoryLabel(buyCategory_) +
                      "   Confirm: Buy   Cancel: Back";
            hint = buyHint.c_str();
            break;
        case Phase::EquipChar: hint = "Choose a party member."; break;
        case Phase::EquipSlot: hint = "Choose a slot."; break;
        case Phase::EquipItem: hint = "Choose an item to equip."; break;
    }
    ui::drawText(hint, 20, 36, style::kFontBody, style::palette().textDim);
    if (!message_.empty()) {
        ui::drawTextFitted(message_, 20, 50, w - 40, style::kFontBody, style::palette().success,
                           "equipshop.message");
    }

    ui::drawMenuScrolled(menu_, scroll_, kVisibleRows, kListX, kListY, kListItemH,
                         style::kFontMenu, 300, style::palette().text, style::palette().disabled, style::palette().cursor,
                         "equipshop.list");

    // Detail line for the selected piece of gear (Buy and EquipItem phases).
    const content::ItemDef* detail = nullptr;
    if (phase_ == Phase::Buy && !rowIds_.empty()) {
        detail = context_.content.findItem(rowIds_[static_cast<std::size_t>(menu_.cursor())]);
    } else if (phase_ == Phase::EquipItem) {
        if (menu_.cursor() == 0) {
            ui::drawTextWrapped("Remove the current equipment.", 20, h - 52, w - 40,
                                style::kFontBody, style::palette().textDim, "equipshop.detail", 2);
        } else if (!rowIds_.empty()) {
            detail = context_.content.findItem(
                rowIds_[static_cast<std::size_t>(menu_.cursor() - 1)]);
        }
    }
    if (detail != nullptr) {
        ui::drawTextWrapped(equipDetail(*detail), 20, h - 52, w - 40, style::kFontBody,
                            style::palette().success, "equipshop.detail", 2);
    }

    std::string footer = input::prompt(context_.input.map(), InputAction::Cancel,
                                       context_.input.activeDevice(), "Back");
    if (phase_ == Phase::Buy) {
        footer = input::prompt(context_.input.map(), InputAction::Details,
                               context_.input.activeDevice(), "Compare") +
                 "    " + footer;
    }
    ui::drawTextCentered(footer.c_str(), w / 2, h - style::kFooterHeight + 2, 9,
                         style::palette().textHint);
}

}  // namespace cd
