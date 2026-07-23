#include "states/EquipShopState.hpp"

#include <algorithm>
#include <utility>

#include <memory>

#include "audio/AudioManager.hpp"
#include "content/ContentDatabase.hpp"
#include "states/DetailsOverlayState.hpp"
#include "states/EquipDiff.hpp"
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
constexpr int kListY = 60;
constexpr int kListItemH = 14;
constexpr int kVisibleRows = 8;

// One-line summary of a piece of gear: slot, stat bonus, description.
std::string equipDetail(const content::ItemDef& it) {
    std::string out;
    switch (it.slot) {
        case content::EquipSlot::Weapon: out = "Weapon"; break;
        case content::EquipSlot::Armor: out = "Armor"; break;
        case content::EquipSlot::Accessory: out = "Accessory"; break;
        case content::EquipSlot::None: break;
    }
    const std::string stats = equip::statBonusSummary(it.statBonus);
    if (!stats.empty()) {
        out += (out.empty() ? "" : "  ") + stats;
    }
    if (!it.description.empty()) {
        out += (out.empty() ? "" : "  -  ") + it.description;
    }
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
void EquipShopState::captureEnterBuyList(content::EquipSlot slot) {
    buyCategory_ = slot;
    phase_ = Phase::Buy;
    rebuild();
}
void EquipShopState::captureEnterEquipItem(int charIndex, content::EquipSlot slot) {
    selectedChar_ = charIndex;
    selectedSlot_ = slot == content::EquipSlot::Weapon ? 0
                    : (slot == content::EquipSlot::Armor ? 1 : 2);
    phase_ = Phase::EquipItem;
    rebuild();
    menu_.setCursor(1);  // highlight a real candidate (row 0 is Unequip)
    scroll_.follow(static_cast<int>(menu_.size()), kVisibleRows, menu_.cursor());
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
                equipShopBuyIds(context_.content, buyCategory_, context_.party.currentTown);
            for (const std::string& id : ids) {
                const content::ItemDef* it = context_.content.findItem(id);
                rowIds_.push_back(id);
                // M52: owned count + price, the item-shop column idiom (M46).
                items.push_back({it->name, true,
                                 TextFormat("x%-3d%5dg", context_.party.inventory.count(id),
                                            it->value)});
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
                items.push_back({it->name, true,
                                 "x" + std::to_string(context_.party.inventory.count(id))});
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
                messageIsError_ = false;
            } else {
                context_.audio.play(Sfx::Error);
                message_ = "Not enough gold for " + it->name;
                messageIsError_ = true;
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
            // M45: a class may be barred from a slot entirely. Buying is never
            // blocked — only equipping — and the refusal says why.
            if (cursor != 0 && !canEquipSlot(c, slotEnum(selectedSlot_), context_.content)) {
                context_.audio.play(Sfx::Error);
                const content::ClassDef* cls = context_.content.findClass(c.classId);
                message_ = std::string("A ") + (cls != nullptr ? cls->name : c.classId) +
                           " cannot equip " + slotLabel(slotEnum(selectedSlot_)) + ".";
                messageIsError_ = true;
                break;
            }
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
    const std::string own = equip::bonusDelta(it->statBonus, content::StatBlock{});
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
        const std::string delta = equip::bonusDelta(it->statBonus, cur);
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
    const style::Palette& p = style::palette();
    ui::drawSceneBackground(context_.resources, "bg.equip_shop", p.canvas,
                            context_.virtualWidth, context_.virtualHeight, context_.party.currentTown);
    ui::drawHeaderBand("Equipment Shop", w, p.gold, context_.party.gold);

    std::string buyHint;
    const char* hint = "Confirm: Select   Cancel: Back";
    switch (phase_) {
        case Phase::Menu: hint = "Buy gear or equip your party."; break;
        case Phase::BuyCategory: hint = "Choose a category to browse."; break;
        case Phase::Buy:
            buyHint = std::string("Buying ") + categoryLabel(buyCategory_);
            hint = buyHint.c_str();
            break;
        case Phase::EquipChar: hint = "Choose a party member."; break;
        case Phase::EquipSlot: hint = "Choose a slot."; break;
        case Phase::EquipItem: hint = "Choose an item to equip."; break;
    }
    ui::drawTextCentered(hint, w / 2, 30, style::kFontBody, p.textDim);

    ui::drawFrame(kListX - 24, kListY - 8, 352, kVisibleRows * kListItemH + 14,
                  ui::FrameStyle::Inset);
    ui::drawMenuScrolled(menu_, scroll_, kVisibleRows, kListX, kListY, kListItemH,
                         style::kFontMenu, 300, p.text, p.disabled, p.cursor,
                         "equipshop.list", style::kFontSmall, p.gold);

    // Detail panel for the selected piece of gear (Buy and EquipItem phases).
    const int infoY = kListY + kVisibleRows * kListItemH + 12;
    if (phase_ == Phase::Buy || phase_ == Phase::EquipItem) {
        ui::drawFrame(kListX - 24, infoY, 352, h - style::kFooterHeight - infoY - 4,
                      ui::FrameStyle::Standard);
    }
    if (phase_ == Phase::Buy && !rowIds_.empty()) {
        // Buy keeps the slot/bonus/description line — the shopping decision.
        if (const content::ItemDef* detail =
                context_.content.findItem(rowIds_[static_cast<std::size_t>(menu_.cursor())])) {
            ui::drawTextWrapped(equipDetail(*detail), kListX - 14, infoY + 6, 332,
                                style::kFontBody, p.textDim, "equipshop.detail", 2);
        }
    } else if (phase_ == Phase::EquipItem) {
        // M52: the slot's current item + the stat diff for the highlighted
        // candidate, so the equip decision is legible without opening Details.
        const Character& c = context_.party.members[static_cast<std::size_t>(selectedChar_)];
        const std::string& curId =
            selectedSlot_ == 0 ? c.weapon : (selectedSlot_ == 1 ? c.armor : c.accessory);
        content::StatBlock curBonus{};
        std::string curName = "(none)";
        if (const content::ItemDef* curItem = context_.content.findItem(curId)) {
            curBonus = curItem->statBonus;
            curName = curItem->name;
        }
        std::string curLine = "Current: " + curName;
        const std::string cs = equip::statBonusSummary(curBonus);
        if (!cs.empty()) {
            curLine += "  " + cs;
        }
        ui::drawTextFitted(curLine, kListX - 14, infoY + 6, 332, style::kFontBody, p.textDim,
                           "equipshop.current");

        // Unequip (cursor 0) diffs an empty bonus; a candidate diffs its own.
        content::StatBlock cand{};
        if (menu_.cursor() >= 1 && !rowIds_.empty()) {
            if (const content::ItemDef* candItem = context_.content.findItem(
                    rowIds_[static_cast<std::size_t>(menu_.cursor() - 1)])) {
                cand = candItem->statBonus;
            }
        }
        // M52: each stat coloured by its own change — a gain green, a loss coral,
        // no change in normal text — so a mixed swap (ATK up, SPD down) reads
        // truthfully instead of the whole line taking one colour.
        const int dy = infoY + 6 + ui::lineHeight(style::kFontBody);
        int dx = kListX - 14;
        ui::drawText("Diff:", dx, dy, style::kFontBody, p.textDim);
        dx += ui::measureText("Diff:", style::kFontBody) + 6;
        for (const equip::StatDelta& sd : equip::statDeltas(cand, curBonus)) {
            const Color segColor =
                sd.value > 0 ? p.success : (sd.value < 0 ? p.dangerText : p.text);
            const std::string seg =
                std::string(sd.tag) + " " + (sd.value > 0 ? "+" : "") + std::to_string(sd.value);
            ui::drawText(seg, dx, dy, style::kFontBody, segColor);
            dx += ui::measureText(seg, style::kFontBody) + 8;
        }
    }

    // Transient feedback rides an overlay banner (drawn last, like a toast).
    if (!message_.empty()) {
        ui::drawBanner(messageIsError_ ? ui::BannerKind::Danger : ui::BannerKind::Success,
                       message_, 60, 40, w - 120, "equipshop.message");
    }

    const InputMap& map = context_.input.map();
    const ActiveDevice device = context_.input.activeDevice();
    std::vector<ui::Hint> hints;
    hints.push_back({input::primaryLabel(map, InputAction::Confirm, device),
                     phase_ == Phase::Buy ? "Buy" : "Select"});
    if (phase_ == Phase::Buy) {
        hints.push_back({input::primaryLabel(map, InputAction::Details, device), "Compare"});
    }
    hints.push_back({input::primaryLabel(map, InputAction::Cancel, device), "Back"});
    ui::drawFooterHints(hints, w, h, "equipshop.footer");
}

}  // namespace cd
