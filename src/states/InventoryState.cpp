#include "states/InventoryState.hpp"

#include <algorithm>

#include "audio/AudioManager.hpp"
#include "content/ContentDatabase.hpp"
#include "core/AppContext.hpp"
#include "game/ItemUse.hpp"
#include "game/Ledger.hpp"     // M109: recordScrollLearned
#include "game/Party.hpp"
#include "game/Scrolls.hpp"    // M122: the scroll rules (refusal / learn)
#include "game/SkillInfo.hpp"  // M122: the taught skill's kind line + text
#include "input/Input.hpp"
#include "input/PromptLabels.hpp"
#include "raylib.h"
#include "states/ItemShopFilter.hpp"  // M88: the category ordering, reused
#include "states/StateStack.hpp"
#include "ui/UiDraw.hpp"
#include "ui/UiStyle.hpp"

namespace cd {

namespace {
constexpr int kListX = 74;
constexpr int kListY = 52;
constexpr int kListItemH = 14;
constexpr int kVisibleRows = 8;

// Bag ordering: usable consumables first (in the M88 shelf order), then
// equipment, then scrolls — three readable bands, no interleaving.
int bagBandFor(const content::ItemDef& def) {
    switch (def.type) {
        case content::ItemType::Consumable: return 0;
        case content::ItemType::Relic: return 1;  // the four Royal Relics (battle-use)
        case content::ItemType::Equipment: return 2;
        case content::ItemType::Scroll: return 3;
    }
    return 4;
}
}  // namespace

InventoryState::InventoryState(StateStack& stack, AppContext& context)
    : GameState(stack), context_(context) {
    rebuild();
}

#ifdef CRYSTAL_CAPTURE
void InventoryState::captureCursorToItem(const std::string& itemId) {
    for (std::size_t i = 0; i < rowIds_.size(); ++i) {
        if (rowIds_[i] == itemId) {
            menu_.setCursor(static_cast<int>(i));
            break;
        }
    }
    scroll_.follow(static_cast<int>(menu_.size()), kVisibleRows, menu_.cursor());
}

void InventoryState::captureConfirm(int memberRow) {
    confirm();
    if (phase_ == Phase::PickMember) {
        menu_.setCursor(memberRow);
    }
}
#endif

void InventoryState::rebuild() {
    rowIds_.clear();
    std::vector<ui::MenuItem> items;

    if (phase_ == Phase::List) {
        std::vector<std::string> ids;
        for (const ItemStack& s : context_.party.inventory.stacks) {
            if (s.count > 0 && context_.content.findItem(s.itemId) != nullptr) {
                ids.push_back(s.itemId);
            }
        }
        std::sort(ids.begin(), ids.end(), [this](const std::string& a, const std::string& b) {
            const content::ItemDef* da = context_.content.findItem(a);
            const content::ItemDef* db = context_.content.findItem(b);
            const int ba = bagBandFor(*da);
            const int bb = bagBandFor(*db);
            if (ba != bb) {
                return ba < bb;
            }
            const int ra = itemShopCategoryRank(*da);
            const int rb = itemShopCategoryRank(*db);
            if (ra != rb) {
                return ra < rb;
            }
            if (da->value != db->value) {
                return da->value < db->value;
            }
            return a < b;
        });
        for (const std::string& id : ids) {
            const content::ItemDef* it = context_.content.findItem(id);
            rowIds_.push_back(id);
            items.push_back({it->name, true,
                             "x" + std::to_string(context_.party.inventory.count(id)),
                             content::gearIconTextureId(*it)});
        }
        if (items.empty()) {
            items.push_back({"(The bag is empty)", false});
        }
    } else {  // PickMember
        const content::ItemDef* aimed = context_.content.findItem(selectedItem_);
        const bool teaching = aimed != nullptr && aimed->type == content::ItemType::Scroll;
        for (const Character& c : context_.party.members) {
            if (teaching) {
                // M122: who can still learn it. A member who knows the skill
                // is a greyed row the cursor can rest on (the reason shows on
                // Confirm); the suffix says it at a glance.
                const bool canLearn = scrollRefusal(c, *aimed, context_.content).empty();
                items.push_back({c.name, canLearn, canLearn ? "can learn" : "knows it"});
                continue;
            }
            const std::string vitals = std::to_string(c.hp) + "/" + std::to_string(c.maxHp) +
                                       " HP  " + std::to_string(c.mp) + "/" +
                                       std::to_string(c.maxMp) + " MP";
            items.push_back({c.name, true, vitals});
        }
        menu_.setFocusDisabled(teaching);
    }
    if (phase_ == Phase::List) {
        menu_.setFocusDisabled(false);
    }
    menu_.setItems(std::move(items));
    scroll_.reset();
    scroll_.follow(static_cast<int>(menu_.size()), kVisibleRows, menu_.cursor());
}

void InventoryState::confirm() {
    if (phase_ == Phase::List) {
        if (rowIds_.empty() || menu_.cursor() >= static_cast<int>(rowIds_.size())) {
            return;
        }
        const content::ItemDef* it =
            context_.content.findItem(rowIds_[static_cast<std::size_t>(menu_.cursor())]);
        if (it == nullptr) {
            return;
        }
        if (it->type == content::ItemType::Equipment) {
            message_ = "Gear changes hands in Equip Party.";
            messageIsError_ = false;
            context_.audio.play(Sfx::Cancel);
            return;
        }
        if (it->type == content::ItemType::Scroll) {
            // M122: scrolls teach from here. Never enter a pick that can only
            // refuse (the M43 rule): someone must still be able to learn it.
            bool anyLearner = false;
            for (const Character& c : context_.party.members) {
                if (scrollRefusal(c, *it, context_.content).empty()) {
                    anyLearner = true;
                    break;
                }
            }
            if (!anyLearner) {
                message_ = context_.party.members.empty()
                               ? std::string("No party.")
                               : (it->grantsSkill.empty() || !context_.content.hasSkill(it->grantsSkill)
                                      ? scrollRefusal(context_.party.members.front(), *it,
                                                      context_.content)
                                      : std::string("Everyone already knows that skill."));
                messageIsError_ = true;
                context_.audio.play(Sfx::Error);
                return;
            }
            selectedItem_ = it->id;
            phase_ = Phase::PickMember;
            message_.clear();
            context_.audio.play(Sfx::Confirm);
            menu_.setCursor(0);
            rebuild();
            return;
        }
        if (it->type == content::ItemType::Relic) {
            message_ = "A Royal Relic works in battle, on an enemy.";
            messageIsError_ = false;
            context_.audio.play(Sfx::Cancel);
            return;
        }
        // A consumable: is there ANY member it could reach? (M43: never enter a
        // pick that can only refuse.)
        bool anyTarget = false;
        for (const Character& c : context_.party.members) {
            if (itemUseRefusal(c, *it).empty()) {
                anyTarget = true;
                break;
            }
        }
        if (!anyTarget) {
            // Show the first member's reason — for the common cases (full HP,
            // nothing to cure, nobody fallen) it reads correctly for the party.
            message_ = context_.party.members.empty()
                           ? std::string("No party.")
                           : itemUseRefusal(context_.party.members.front(), *it);
            messageIsError_ = true;
            context_.audio.play(Sfx::Error);
            return;
        }
        selectedItem_ = it->id;
        phase_ = Phase::PickMember;
        message_.clear();
        menu_.setCursor(0);
        rebuild();
        return;
    }

    // PickMember: apply with the M43 gating, spend on success only.
    const content::ItemDef* it = context_.content.findItem(selectedItem_);
    const int cursor = menu_.cursor();
    if (it == nullptr || cursor < 0 || cursor >= static_cast<int>(context_.party.members.size())) {
        return;
    }
    Character& target = context_.party.members[static_cast<std::size_t>(cursor)];
    if (it->type == content::ItemType::Scroll) {
        // M122: teach under the M64 rules - refused with the reason, never
        // wasted; the scroll is spent only on success.
        const std::string why = scrollRefusal(target, *it, context_.content);
        if (!why.empty()) {
            message_ = why;
            messageIsError_ = true;
            context_.audio.play(Sfx::Error);
            return;
        }
        learnScroll(target, *it);
        recordScrollLearned(context_.party, cursor);  // M109
        context_.party.inventory.remove(it->id);
        const content::SkillDef* skill = context_.content.findSkill(it->grantsSkill);
        const std::string learned =
            target.name + " learns " + (skill != nullptr ? skill->name : it->grantsSkill) + "!";
        context_.audio.play(Sfx::Heal);
        // Taught: back to the bag (the usual case is one scroll, one pupil).
        phase_ = Phase::List;
        selectedItem_.clear();
        menu_.setCursor(0);
        rebuild();
        message_ = learned;
        messageIsError_ = false;
        return;
    }
    const std::string refusal = itemUseRefusal(target, *it);
    if (!refusal.empty()) {
        message_ = refusal;
        messageIsError_ = true;
        context_.audio.play(Sfx::Error);
        return;
    }
    const std::string line = applyItemUse(target, *it);
    context_.party.inventory.remove(it->id);
    context_.audio.play(Sfx::Heal);
    message_ = line;
    messageIsError_ = false;
    // Out of stock: fall back to the list (the row is gone); otherwise stay
    // for another sip. rebuild() refreshes the vitals column either way.
    if (context_.party.inventory.count(it->id) <= 0) {
        phase_ = Phase::List;
        selectedItem_.clear();
        menu_.setCursor(0);
    }
    const std::string keep = message_;  // rebuild clears nothing, but stay explicit
    rebuild();
    message_ = keep;
}

void InventoryState::handleInput(const Input& input) {
    if (input.navPressed(InputAction::MoveUp)) {
        menu_.moveUp();
    }
    if (input.navPressed(InputAction::MoveDown)) {
        menu_.moveDown();
    }
    scroll_.follow(static_cast<int>(menu_.size()), kVisibleRows, menu_.cursor());
    if (input.pressed(InputAction::Confirm)) {
        confirm();
    }
    if (input.pressed(InputAction::Cancel)) {
        if (phase_ == Phase::PickMember) {
            phase_ = Phase::List;
            selectedItem_.clear();
            message_.clear();
            rebuild();
        } else {
            stack().popState();
        }
    }
}

void InventoryState::render() {
    const int w = context_.virtualWidth;
    const int h = context_.virtualHeight;
    namespace style = ui::style;
    const style::Palette& p = style::palette();
    ClearBackground(p.canvas);
    ui::drawHeaderBand("Items", w, p.crystal);

    const content::ItemDef* aimed =
        phase_ == Phase::PickMember ? context_.content.findItem(selectedItem_) : nullptr;
    const bool teaching = aimed != nullptr && aimed->type == content::ItemType::Scroll;
    const char* hint = phase_ == Phase::List
                           ? "Use a consumable, teach a scroll, or inspect the bag."
                           : (teaching ? "Choose who learns it." : "Choose who takes it.");
    ui::drawTextCentered(hint, w / 2, 30, style::kFontBody, p.textDim);

    ui::drawFrame(kListX - 24, kListY - 8, 352, kVisibleRows * kListItemH + 14,
                  ui::FrameStyle::Inset);
    ui::drawMenuScrolled(menu_, scroll_, kVisibleRows, kListX, kListY, kListItemH,
                         style::kFontMenu, 300, p.text, p.disabled, p.cursor,
                         "inventory.list", style::kFontSmall, p.gold, &context_.resources);

    // Detail strip: the highlighted item's description (List) or the aimed
    // item's name (PickMember), then the transient message line.
    const int detailY = kListY - 8 + kVisibleRows * kListItemH + 14 + 4;
    if (phase_ == Phase::List && menu_.cursor() < static_cast<int>(rowIds_.size())) {
        if (const content::ItemDef* it =
                context_.content.findItem(rowIds_[static_cast<std::size_t>(menu_.cursor())])) {
            ui::drawTextPreview(it->description, kListX - 24, detailY, 352,
                                style::kFontBody, p.textDim, 2);
        }
    } else if (phase_ == Phase::PickMember) {
        if (const content::ItemDef* it = context_.content.findItem(selectedItem_)) {
            ui::drawTextFitted((teaching ? "Teaching: " : "Using: ") + it->name, kListX - 24,
                               detailY, 352, style::kFontBody, p.gold, "inventory.using");
        }
    }
    // M122: while a scroll's pupil is picked, the skill it teaches stays in
    // view in the list frame's free lower half - icon, name, cost, kind, and
    // the description as the HIGHLIGHTED member would cast it.
    if (teaching) {
        if (const content::SkillDef* s = context_.content.findSkill(aimed->grantsSkill)) {
            const int px = kListX - 16;
            const int pw = 336;
            int py = kListY + 4 * kListItemH + 4;
            ui::drawDivider(px, py, pw);
            py += 5;
            ui::drawGearIcon(context_.resources, content::skillKindTextureId(s->kind), px, py);
            const std::string cost = "MP " + std::to_string(s->mpCost);
            const int costW = ui::measureText(cost, style::kFontSmall);
            ui::drawTextFitted(s->name, px + ui::kGearIconSize + 4, py,
                               pw - ui::kGearIconSize - 4 - costW - 8, style::kFontBody, p.text,
                               "inventory.skill.name");
            ui::drawText(cost, px + pw - costW, py + 1, style::kFontSmall, p.mpFill);
            py += 12;
            ui::drawTextFitted(skillKindLine(*s), px, py, pw, style::kFontSmall, p.textHint,
                               "inventory.skill.kind");
            py += 11;
            const int cursor = menu_.cursor();
            const bool validMember =
                cursor >= 0 && cursor < static_cast<int>(context_.party.members.size());
            const std::string text =
                validMember ? skillTextFor(context_.party.members[static_cast<std::size_t>(cursor)],
                                           *s, context_.content)
                                  .description
                            : s->description;
            ui::drawTextPreview(text, px, py, pw, style::kFontBody, p.success, 2);
        }
    }
    if (!message_.empty()) {
        ui::drawBanner(messageIsError_ ? ui::BannerKind::Danger : ui::BannerKind::Success,
                       message_, 70, detailY + 24, w - 140, "inventory.message");
    }

    // The one thing the bag deliberately does not hold (owner item 10).
    ui::drawTextCentered("Map pieces and curios live on the Maps screen.", w / 2,
                         h - ui::style::kFooterHeight - 10, style::kFontBody, p.textHint);

    const InputMap& map = context_.input.map();
    const ActiveDevice device = context_.input.activeDevice();
    ui::drawFooterHints(
        {{input::primaryLabel(map, InputAction::Confirm, device),
          phase_ == Phase::List ? "Select" : (teaching ? "Teach" : "Use")},
         {input::primaryLabel(map, InputAction::Cancel, device), "Back"}},
        w, h, "inventory.footer");
}

}  // namespace cd
