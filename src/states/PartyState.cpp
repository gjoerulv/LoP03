#include "states/PartyState.hpp"

#include <string>
#include <vector>

#include "audio/AudioManager.hpp"
#include "content/ContentDatabase.hpp"
#include "core/AppContext.hpp"
#include "game/Milestones.hpp"
#include "game/Party.hpp"
#include "game/Scrolls.hpp"
#include "input/Input.hpp"
#include "input/PromptLabels.hpp"
#include "raylib.h"
#include "states/StateStack.hpp"
#include "ui/UiDraw.hpp"
#include "ui/UiStyle.hpp"

namespace cd {

namespace {

// The gear share of one derived stat: the sum of the three equipped items'
// bonuses — shown beside the total so a number explains itself.
content::StatBlock gearBonus(const Character& c, const content::ContentDatabase& db) {
    content::StatBlock bonus;
    for (const std::string& id : {c.weapon, c.armor, c.accessory}) {
        if (id.empty()) {
            continue;
        }
        if (const content::ItemDef* item = db.findItem(id)) {
            bonus.maxHp += item->statBonus.maxHp;
            bonus.attack += item->statBonus.attack;
            bonus.magic += item->statBonus.magic;
            bonus.defense += item->statBonus.defense;
            bonus.speed += item->statBonus.speed;
        }
    }
    return bonus;
}

std::string statLine(const char* label, int total, int fromGear) {
    std::string s = std::string(label) + " " + std::to_string(total);
    if (fromGear != 0) {
        s += " (" + std::string(fromGear > 0 ? "+" : "") + std::to_string(fromGear) + " gear)";
    }
    return s;
}

std::string itemName(const content::ContentDatabase& db, const std::string& id) {
    if (id.empty()) {
        return "-";
    }
    const content::ItemDef* item = db.findItem(id);
    return item != nullptr ? item->name : id;
}

}  // namespace

PartyState::PartyState(StateStack& stack, AppContext& context)
    : GameState(stack), context_(context) {}

void PartyState::rebuildScrolls() {
    std::vector<ui::MenuItem> rows;
    scrollIds_.clear();
    for (const ItemStack& stack : context_.party.inventory.stacks) {
        const content::ItemDef* item = context_.content.findItem(stack.itemId);
        if (item == nullptr || item->type != content::ItemType::Scroll ||
            item->grantsSkill.empty() || stack.count <= 0) {
            continue;
        }
        rows.push_back({TextFormat("%s  x%d", item->name.c_str(), stack.count), true});
        scrollIds_.push_back(stack.itemId);
    }
    scrollMenu_.setItems(std::move(rows));
}

void PartyState::handleInput(const Input& input) {
    const int count = static_cast<int>(context_.party.members.size());
    if (phase_ == Phase::PickScroll) {
        if (input.navPressed(InputAction::MoveUp)) {
            scrollMenu_.moveUp();
        }
        if (input.navPressed(InputAction::MoveDown)) {
            scrollMenu_.moveDown();
        }
        if (input.pressed(InputAction::Cancel)) {
            context_.audio.play(Sfx::Cancel);
            phase_ = Phase::Browse;
            return;
        }
        if (input.pressed(InputAction::Confirm) && !scrollIds_.empty() && count > 0) {
            const std::string& itemId = scrollIds_[static_cast<std::size_t>(scrollMenu_.cursor())];
            const content::ItemDef* item = context_.content.findItem(itemId);
            Character& c = context_.party.members[static_cast<std::size_t>(cursor_)];
            if (item != nullptr) {
                const std::string refusal = scrollRefusal(c, *item, context_.content);
                if (refusal.empty()) {
                    learnScroll(c, *item);
                    context_.party.inventory.remove(itemId, 1);
                    const content::SkillDef* skill = context_.content.findSkill(item->grantsSkill);
                    message_ = c.name + " learns " +
                               (skill != nullptr ? skill->name : item->grantsSkill) + "!";
                    context_.audio.play(Sfx::Heal);
                    phase_ = Phase::Browse;
                } else {
                    message_ = refusal;
                    context_.audio.play(Sfx::Error);
                }
            }
        }
        return;
    }
    if (input.navPressed(InputAction::MoveUp) && count > 0) {
        cursor_ = (cursor_ + count - 1) % count;
        message_.clear();
    }
    if (input.navPressed(InputAction::MoveDown) && count > 0) {
        cursor_ = (cursor_ + 1) % count;
        message_.clear();
    }
    if (input.pressed(InputAction::Cancel)) {
        context_.audio.play(Sfx::Cancel);
        stack().popState();
        return;
    }
    if (input.pressed(InputAction::Confirm) && count > 0) {
        rebuildScrolls();
        if (scrollIds_.empty()) {
            message_ = "No teaching scrolls in the bag.";
            context_.audio.play(Sfx::Error);
        } else {
            context_.audio.play(Sfx::Confirm);
            phase_ = Phase::PickScroll;
        }
    }
}

void PartyState::render() {
    const int w = context_.virtualWidth;
    const int h = context_.virtualHeight;
    const ui::style::Palette& p = ui::style::palette();
    ClearBackground(p.canvas);
    ui::drawHeaderBand("Party", w, p.gold);

    const content::ContentDatabase& db = context_.content;
    const auto& members = context_.party.members;

    // Member list (left).
    const int listX = 12;
    const int listY = 34;
    ui::drawFrame(listX - 4, listY - 6, 128, static_cast<int>(members.size()) * 26 + 12,
                  ui::FrameStyle::Standard);
    for (std::size_t i = 0; i < members.size(); ++i) {
        const Character& c = members[i];
        const int y = listY + static_cast<int>(i) * 26;
        if (static_cast<int>(i) == cursor_) {
            ui::drawSelectionSlab(listX - 2, y - 2, 124, 24);
        }
        ui::drawTextFitted(c.name, listX + 4, y, 112, 11,
                           static_cast<int>(i) == cursor_ ? p.text : p.textDim, "party.name");
        const content::ClassDef* cls = db.findClass(c.classId);
        ui::drawTextFitted(TextFormat("Lv.%d %s", c.level,
                                      cls != nullptr ? cls->name.c_str() : c.classId.c_str()),
                           listX + 4, y + 12, 112, 8, p.textHint, "party.class");
    }

    if (members.empty()) {
        return;
    }
    const Character& c = members[static_cast<std::size_t>(cursor_)];

    // Detail (right).
    const int dx = 150;
    const int dw = w - dx - 10;
    ui::drawFrame(dx - 6, listY - 6, dw + 8, 182, ui::FrameStyle::Standard);
    int y = listY + 2;
    const content::StatBlock gear = gearBonus(c, db);
    ui::drawText(TextFormat("HP %d/%d   MP %d/%d", c.hp, c.maxHp, c.mp, c.maxMp), dx, y, 10,
                 p.text);
    y += 13;
    ui::drawText(TextFormat("XP %d  (next Lv: %d)", c.xp,
                            c.level >= kMaxLevel ? 0 : xpToNext(c.level) - c.xp),
                 dx, y, 8, p.textDim);
    y += 13;
    ui::drawTextFitted(statLine("ATK", c.stats.attack, gear.attack), dx, y, dw / 2 - 4, 9, p.text,
                       "party.stat");
    ui::drawTextFitted(statLine("MAG", c.stats.magic, gear.magic), dx + dw / 2, y, dw / 2 - 4, 9,
                       p.text, "party.stat");
    y += 12;
    ui::drawTextFitted(statLine("DEF", c.stats.defense, gear.defense), dx, y, dw / 2 - 4, 9,
                       p.text, "party.stat");
    ui::drawTextFitted(statLine("SPD", c.stats.speed, gear.speed), dx + dw / 2, y, dw / 2 - 4, 9,
                       p.text, "party.stat");
    y += 14;
    ui::drawTextFitted("Weapon: " + itemName(db, c.weapon), dx, y, dw, 8, p.textDim,
                       "party.gear");
    y += 11;
    ui::drawTextFitted("Armor: " + itemName(db, c.armor) + "   Acc: " + itemName(db, c.accessory),
                       dx, y, dw, 8, p.textDim, "party.gear");
    y += 11;
    const content::PassiveDef* passive =
        c.equippedPassive.empty() ? nullptr : db.findPassive(c.equippedPassive);
    ui::drawTextFitted("Passive: " + std::string(passive != nullptr ? passive->name : "-") +
                           TextFormat("  (owned %d)", static_cast<int>(c.ownedPassives.size())),
                       dx, y, dw, 8, p.textDim, "party.passive");
    y += 13;

    // Milestone choices (M63).
    std::string milestones;
    for (int tier : kMilestoneTiers) {
        const content::MilestoneDef* m = chosenMilestone(c, tier, db);
        if (m != nullptr) {
            milestones += (milestones.empty() ? "" : ", ") + m->name;
        } else if (c.level >= tier) {
            milestones += (milestones.empty() ? "" : ", ") + std::string("Lv.") +
                          std::to_string(tier) + " unchosen";
        }
    }
    ui::drawTextWrapped("Milestones: " + (milestones.empty() ? "none yet" : milestones), dx, y,
                        dw, 8, p.textDim, "party.milestones", 2);
    y += 22;

    // Every known skill; scroll-learned extras are marked.
    std::string skills;
    const std::vector<std::string> known = allKnownSkills(c, db);
    for (const std::string& id : known) {
        const content::SkillDef* s = db.findSkill(id);
        std::string name = s != nullptr ? s->name : id;
        if (std::find(c.extraSkills.begin(), c.extraSkills.end(), id) != c.extraSkills.end()) {
            name += "*";
        }
        skills += (skills.empty() ? "" : ", ") + name;
    }
    ui::drawTextWrapped("Skills: " + (skills.empty() ? "none" : skills) +
                            (c.extraSkills.empty() ? "" : "   (* from a scroll)"),
                        dx, y, dw, 8, p.text, "party.skills", 4);

    if (!message_.empty()) {
        ui::drawTextCentered(message_.c_str(), w / 2, h - 40, 9, p.gold);
    }

    if (phase_ == Phase::PickScroll) {
        const int boxW = 220;
        const int boxH = static_cast<int>(scrollIds_.size()) * 16 + 44;
        const int boxX = w / 2 - boxW / 2;
        const int boxY = h / 2 - boxH / 2;
        ui::drawModalDim(w, h);
        ui::drawFrame(boxX, boxY, boxW, boxH, ui::FrameStyle::Raised);
        ui::drawTextCentered(("Teach " + c.name + " from:").c_str(), w / 2, boxY + 8, 10, p.gold);
        ui::drawMenu(scrollMenu_, boxX + 24, boxY + 26, 16, 9, p.text, p.disabled, p.cursor);
    }

    ui::drawFooterHints({{input::primaryLabel(context_.input.map(), InputAction::Confirm,
                                              context_.input.activeDevice()),
                          phase_ == Phase::PickScroll ? "Teach" : "Use Scroll"},
                         {input::primaryLabel(context_.input.map(), InputAction::Cancel,
                                              context_.input.activeDevice()),
                          "Back"}},
                        w, h, "party.footer");
}

}  // namespace cd
