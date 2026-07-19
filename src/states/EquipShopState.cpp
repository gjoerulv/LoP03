#include "states/EquipShopState.hpp"

#include <algorithm>
#include <utility>

#include "content/ContentDatabase.hpp"
#include "content/Definitions.hpp"
#include "core/AppContext.hpp"
#include "game/Party.hpp"
#include "input/Input.hpp"
#include "raylib.h"
#include "states/StateStack.hpp"
#include "ui/UiDraw.hpp"
#include "ui/UiStyle.hpp"

namespace cd {

namespace style = ui::style;

namespace {
const char* const kSlotNames[3] = {"Weapon", "Armor", "Accessory"};

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

bool isEquippable(const content::ItemDef& item) {
    return item.type == content::ItemType::Equipment || item.type == content::ItemType::Relic;
}
}  // namespace

EquipShopState::EquipShopState(StateStack& stack, AppContext& context)
    : GameState(stack), context_(context) {}

void EquipShopState::onEnter() {
    phase_ = Phase::Menu;
    rebuild();
}

void EquipShopState::rebuild() {
    rowIds_.clear();
    std::vector<ui::MenuItem> items;

    switch (phase_) {
        case Phase::Menu:
            items = {{"Buy Gear", true}, {"Equip Party", !context_.party.empty()}, {"Back", true}};
            break;
        case Phase::Buy: {
            std::vector<std::string> ids;
            for (const auto& [id, def] : context_.content.items()) {
                if (isEquippable(def)) {
                    ids.push_back(id);
                }
            }
            std::sort(ids.begin(), ids.end());
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
                if (it != nullptr && isEquippable(*it) && it->slot == slotEnum(selectedSlot_)) {
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
                phase_ = Phase::Buy;
            } else if (cursor == 1) {
                phase_ = Phase::EquipChar;
            } else {
                stack().popState();
                return;
            }
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
                message_ = "Bought " + it->name;
            } else {
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

void EquipShopState::handleInput(const Input& input) {
    if (input.pressed(InputAction::MoveUp)) {
        menu_.moveUp();
    }
    if (input.pressed(InputAction::MoveDown)) {
        menu_.moveDown();
    }
    scroll_.follow(static_cast<int>(menu_.size()), kVisibleRows, menu_.cursor());
    if (input.pressed(InputAction::Confirm)) {
        confirm();
    }
    if (input.pressed(InputAction::Cancel)) {
        switch (phase_) {
            case Phase::Menu: stack().popState(); break;
            case Phase::Buy:
            case Phase::EquipChar: phase_ = Phase::Menu; rebuild(); break;
            case Phase::EquipSlot: phase_ = Phase::EquipChar; rebuild(); break;
            case Phase::EquipItem: phase_ = Phase::EquipSlot; rebuild(); break;
        }
    }
}

void EquipShopState::render() {
    const int w = context_.virtualWidth;
    const int h = context_.virtualHeight;
    ClearBackground(Color{16, 16, 24, 255});
    ui::drawTextCentered("Equipment Shop", w / 2, 14, style::kFontScreenTitle, style::kText);
    ui::drawTextRight(TextFormat("Gold: %d", context_.party.gold), w - 14, 14, style::kFontMenu,
                      style::kGold);

    const char* hint = "Confirm: Select   Cancel: Back";
    switch (phase_) {
        case Phase::Menu: hint = "Buy gear or equip your party."; break;
        case Phase::Buy: hint = "Confirm: Buy   Cancel: Back"; break;
        case Phase::EquipChar: hint = "Choose a party member."; break;
        case Phase::EquipSlot: hint = "Choose a slot."; break;
        case Phase::EquipItem: hint = "Choose an item to equip."; break;
    }
    DrawText(hint, 20, 36, style::kFontBody, style::kTextDim);
    if (!message_.empty()) {
        ui::drawTextFitted(message_, 20, 50, w - 40, style::kFontBody, style::kSuccess,
                           "equipshop.message");
    }

    ui::drawMenuScrolled(menu_, scroll_, kVisibleRows, kListX, kListY, kListItemH,
                         style::kFontMenu, 300, style::kText, style::kDisabled, style::kCursor,
                         "equipshop.list");

    // Detail line for the selected piece of gear (Buy and EquipItem phases).
    const content::ItemDef* detail = nullptr;
    if (phase_ == Phase::Buy && !rowIds_.empty()) {
        detail = context_.content.findItem(rowIds_[static_cast<std::size_t>(menu_.cursor())]);
    } else if (phase_ == Phase::EquipItem) {
        if (menu_.cursor() == 0) {
            ui::drawTextWrapped("Remove the current equipment.", 20, h - 52, w - 40,
                                style::kFontBody, style::kTextDim, "equipshop.detail", 2);
        } else if (!rowIds_.empty()) {
            detail = context_.content.findItem(
                rowIds_[static_cast<std::size_t>(menu_.cursor() - 1)]);
        }
    }
    if (detail != nullptr) {
        ui::drawTextWrapped(equipDetail(*detail), 20, h - 52, w - 40, style::kFontBody,
                            style::kSuccess, "equipshop.detail", 2);
    }

    ui::drawTextCentered("Cancel: Back", w / 2, h - style::kFooterHeight + 2, 9,
                         style::kTextHint);
}

}  // namespace cd
