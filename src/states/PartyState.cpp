#include "states/PartyState.hpp"

#include <algorithm>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "audio/AudioManager.hpp"
#include "content/ContentDatabase.hpp"
#include "core/AppContext.hpp"
#include "game/Milestones.hpp"
#include "game/Party.hpp"
#include "game/Ledger.hpp"  // M109: the economy ledger seam
#include "game/Scrolls.hpp"
#include "input/Input.hpp"
#include "input/PromptLabels.hpp"
#include "raylib.h"
#include "resource/ResourceManager.hpp"
#include "states/DetailsOverlayState.hpp"
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

void PartyState::openMemberDetails() {
    if (context_.party.members.empty()) {
        return;
    }
    const content::ContentDatabase& db = context_.content;
    const Character& c = context_.party.members[static_cast<std::size_t>(cursor_)];
    const content::ClassDef* cls = db.findClass(c.classId);
    std::string body = "Lv." + std::to_string(c.level) + " " +
                       (cls != nullptr ? cls->name : c.classId);

    const content::PassiveDef* passive =
        c.equippedPassive.empty() ? nullptr : db.findPassive(c.equippedPassive);
    if (passive != nullptr) {
        body += "\n\nPassive: " + passive->name;
        if (!passive->description.empty()) {
            body += "\n" + passive->description;
        }
    }

    // The M96 keepsake, with its own words (owner fix 2026-08-17).
    const content::ItemDef* heirloom =
        c.equippedHeirloom.empty() ? nullptr : db.findItem(c.equippedHeirloom);
    if (heirloom != nullptr) {
        body += "\n\nHeirloom: " + heirloom->name;
        if (!heirloom->description.empty()) {
            body += "\n" + heirloom->description;
        }
    }

    bool anyMilestone = false;
    for (int tier : kMilestoneTiers) {
        const content::MilestoneDef* m = chosenMilestone(c, tier, db);
        if (m == nullptr) {
            continue;
        }
        body += anyMilestone ? "\n" : "\n\n";
        anyMilestone = true;
        body += "Lv." + std::to_string(tier) + "  " + m->name;
        if (!m->description.empty()) {
            body += "\n" + m->description;
        }
    }

    body += "\n\nSkills:";
    const std::vector<std::string> known = allKnownSkills(c, db);
    if (known.empty()) {
        body += " none";
    }
    bool anyScroll = false;
    for (const std::string& id : known) {
        const content::SkillDef* s = db.findSkill(id);
        std::string name = s != nullptr ? s->name : id;
        if (std::find(c.extraSkills.begin(), c.extraSkills.end(), id) != c.extraSkills.end()) {
            name += "*";
            anyScroll = true;
        }
        body += "\n" + name;
        if (s != nullptr && !s->description.empty()) {
            body += " - " + s->description;
        }
    }
    if (anyScroll) {
        body += "\n(* learned from a scroll)";
    }
    stack().pushState(
        std::make_unique<DetailsOverlayState>(stack(), context_, c.name, std::move(body)));
}

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
                    recordScrollLearned(context_.party, cursor_);  // M109
                    context_.party.inventory.remove(itemId, 1);
                    const content::SkillDef* skill = context_.content.findSkill(item->grantsSkill);
                    message_ = c.name + " learns " +
                               (skill != nullptr ? skill->name : item->grantsSkill) + "!";
                    messageIsError_ = false;
                    context_.audio.play(Sfx::Heal);
                    phase_ = Phase::Browse;
                } else {
                    message_ = refusal;
                    messageIsError_ = true;
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
    // M87: the full member sheet — the compact panel previews long texts;
    // Details reaches all of them in the scrollable reading overlay.
    if (input.pressed(InputAction::Details) && count > 0) {
        context_.audio.play(Sfx::Confirm);
        openMemberDetails();
        return;
    }
    if (input.pressed(InputAction::Confirm) && count > 0) {
        rebuildScrolls();
        if (scrollIds_.empty()) {
            message_ = "No teaching scrolls in the bag.";
            messageIsError_ = true;
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
        // M67: each row leads with the class battle sprite — the party menu's
        // graphical representation of the character.
        const std::string sprId = "actor." + c.classId + ".battle";
        if (context_.resources.hasTexture(sprId)) {
            DrawTexture(context_.resources.texture(sprId), listX + 2, y - 2, WHITE);
        }
        ui::drawTextFitted(c.name, listX + 30, y, 86, 11,
                           static_cast<int>(i) == cursor_ ? p.text : p.textDim, "party.name");
        const content::ClassDef* cls = db.findClass(c.classId);
        ui::drawTextFitted(TextFormat("Lv.%d %s", c.level,
                                      cls != nullptr ? cls->name.c_str() : c.classId.c_str()),
                           listX + 30, y + 12, 86, ui::style::kFontSmall, p.textHint, "party.class");
    }

    if (members.empty()) {
        return;
    }
    const Character& c = members[static_cast<std::size_t>(cursor_)];
    const content::StatBlock gear = gearBonus(c, db);

    // M72: the vitals moved into the previously dead space UNDER the roster —
    // HP/MP and the four derived stats (gear share included) — freeing the
    // right panel so a maxed member's three milestone descriptions and full
    // skill list all stay visible (the owner's Lv.99 screenshots clipped).
    {
        const int vh = 66;
        const int vy = listY + static_cast<int>(members.size()) * 26 + 8;
        ui::drawFrame(listX - 4, vy, 128, vh, ui::FrameStyle::Standard);
        int yy = vy + 5;
        ui::drawTextFitted(TextFormat("HP %d/%d", c.hp, c.maxHp), listX + 4, yy, 116, ui::style::kFontSmall, p.text,
                           "party.vitals");
        yy += 10;
        ui::drawTextFitted(TextFormat("MP %d/%d", c.mp, c.maxMp), listX + 4, yy, 116, ui::style::kFontSmall, p.text,
                           "party.vitals");
        yy += 10;
        ui::drawTextFitted(statLine("ATK", c.stats.attack, gear.attack), listX + 4, yy, 116, ui::style::kFontSmall,
                           p.text, "party.stat");
        yy += 10;
        ui::drawTextFitted(statLine("MAG", c.stats.magic, gear.magic), listX + 4, yy, 116, ui::style::kFontSmall,
                           p.text, "party.stat");
        yy += 10;
        ui::drawTextFitted(statLine("DEF", c.stats.defense, gear.defense), listX + 4, yy, 116, ui::style::kFontSmall,
                           p.text, "party.stat");
        yy += 10;
        ui::drawTextFitted(statLine("SPD", c.stats.speed, gear.speed), listX + 4, yy, 116, ui::style::kFontSmall,
                           p.text, "party.stat");
    }

    // Detail (right): everything else, with room to breathe.
    const int dx = 150;
    const int dw = w - dx - 10;
    ui::drawFrame(dx - 6, listY - 6, dw + 8, 184, ui::FrameStyle::Standard);
    const int bottom = listY - 6 + 184 - 6;  // inner floor of the detail frame
    int y = listY + 2;
    ui::drawText(TextFormat("XP %d  (next Lv: %d)", c.xp,
                            c.level >= kMaxLevel ? 0 : xpToNext(c.level) - c.xp),
                 dx, y, ui::style::kFontSmall, p.textDim);
    y += 11;
    // M81: each gear line leads with its category icon (the icon carries the
    // sword-vs-staff read the slot label cannot). Layout: [icon] label, with
    // the armor line fitting armor + accessory at measured positions.
    const auto gearIcon = [&](const std::string& itemId, int ix, int iy) {
        if (const content::ItemDef* it = db.findItem(itemId)) {
            ui::drawGearIcon(context_.resources, content::gearIconTextureId(*it), ix, iy);
        }
        return ix + ui::kGearIconSize + 3;
    };
    int gx = gearIcon(c.weapon, dx, y - 1);
    ui::drawTextFitted("Weapon: " + itemName(db, c.weapon), gx, y, dw - (gx - dx), ui::style::kFontSmall, p.textDim,
                       "party.gear");
    y += 10;
    gx = gearIcon(c.armor, dx, y - 1);
    const std::string armorTxt = "Armor: " + itemName(db, c.armor);
    ui::drawTextFitted(armorTxt, gx, y, dw - (gx - dx), ui::style::kFontSmall, p.textDim, "party.gear");
    int ax = gx + ui::measureText(armorTxt, ui::style::kFontSmall) + 10;
    if (ax < dx + dw - 40) {  // room for the accessory half; else it clips fitted
        ax = gearIcon(c.accessory, ax, y - 1);
        ui::drawTextFitted("Acc: " + itemName(db, c.accessory), ax, y, dx + dw - ax, ui::style::kFontSmall,
                           p.textDim, "party.gear");
    }
    y += 10;
    // The M96 keepsake slot, visible like every other piece worn (owner fix
    // 2026-08-17); its full effect text lives in Details.
    gx = gearIcon(c.equippedHeirloom, dx, y - 1);
    ui::drawTextFitted("Heirloom: " + itemName(db, c.equippedHeirloom), gx, y, dw - (gx - dx),
                       8, p.textDim, "party.gear");
    y += 10;
    const content::PassiveDef* passive =
        c.equippedPassive.empty() ? nullptr : db.findPassive(c.equippedPassive);
    ui::drawTextFitted("Passive: " + std::string(passive != nullptr ? passive->name : "-") +
                           TextFormat("  (owned %d)", static_cast<int>(c.ownedPassives.size())),
                       dx, y, dw, ui::style::kFontSmall, p.textDim, "party.passive");
    y += 9;
    if (passive != nullptr && !passive->description.empty()) {
        // M67: what the equipped passive does, in the hint colour. M72: long
        // descriptions wrap to a second line. M87: the two lines are a
        // policy-B preview (arrow marks more; Details has the full text).
        y = ui::drawTextPreview(passive->description, dx + 8, y, dw - 8, ui::style::kFontSmall, p.textHint, 2)
                .bottom;
    }

    // Milestone choices (M63) — M67: each chosen bonus shows its name and, in
    // the hint colour, what it does; unreached/unchosen tiers stay compact.
    // M72: descriptions wrap to two lines (the Lv.99 clip fix); y advances by
    // what was actually drawn.
    bool anyTier = false;
    std::string unchosen;
    for (int tier : kMilestoneTiers) {
        const content::MilestoneDef* m = chosenMilestone(c, tier, db);
        if (m != nullptr) {
            anyTier = true;
            ui::drawTextFitted(TextFormat("Lv.%d  %s", tier, m->name.c_str()), dx, y, dw, ui::style::kFontSmall,
                               p.text, "party.milestone");
            y += 9;
            y = ui::drawTextPreview(m->description, dx + 8, y, dw - 8, ui::style::kFontSmall, p.textHint, 2)
                    .bottom;
        } else if (c.level >= tier) {
            anyTier = true;
            unchosen += (unchosen.empty() ? "" : ", ") + std::string("Lv.") + std::to_string(tier);
        }
    }
    if (!unchosen.empty()) {
        ui::drawTextFitted("Milestone unchosen: " + unchosen, dx, y, dw, ui::style::kFontSmall, p.textHint,
                           "party.milestone");
        y += 9;
    }
    if (!anyTier) {
        ui::drawText("Milestones: none yet", dx, y, ui::style::kFontSmall, p.textDim);
        y += 9;
    }
    y += 2;

    // Every known skill; scroll-learned extras are marked. The line budget is
    // whatever the descriptions above left inside the frame.
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
    const int skillLines = std::max(2, (bottom - y) / ui::lineHeight(8));
    // M87: the frame's remaining room is a preview budget, not a hard budget —
    // an overlong (e.g. translated) list marks more and Details lists every
    // skill with its description.
    ui::drawTextPreview("Skills: " + (skills.empty() ? "none" : skills) +
                            (c.extraSkills.empty() ? "" : "   (* from a scroll)"),
                        dx, y, dw, ui::style::kFontSmall, p.text, skillLines);

    if (!message_.empty()) {
        // M67: overlay banner (the equip-shop toast idiom) — the old centered
        // line at the panel floor now collides with the taller detail text.
        ui::drawBanner(messageIsError_ ? ui::BannerKind::Danger : ui::BannerKind::Success,
                       message_, 60, 40, w - 120, "party.message");
    }

    if (phase_ == Phase::PickScroll) {
        const int boxW = 220;
        const int boxH = static_cast<int>(scrollIds_.size()) * 16 + 44;
        const int boxX = w / 2 - boxW / 2;
        const int boxY = h / 2 - boxH / 2;
        ui::drawModalDim(w, h);
        ui::drawFrame(boxX, boxY, boxW, boxH, ui::FrameStyle::Raised);
        ui::drawTextCentered(("Teach " + c.name + " from:").c_str(), w / 2, boxY + 8, 10, p.gold);
        ui::drawMenu(scrollMenu_, boxX + 24, boxY + 26, 16, ui::style::kFontSmall, p.text, p.disabled, p.cursor);
    }

    ui::drawFooterHints({{input::primaryLabel(context_.input.map(), InputAction::Confirm,
                                              context_.input.activeDevice()),
                          phase_ == Phase::PickScroll ? "Teach" : "Use Scroll"},
                         {input::primaryLabel(context_.input.map(), InputAction::Details,
                                              context_.input.activeDevice()),
                          "Details"},
                         {input::primaryLabel(context_.input.map(), InputAction::Cancel,
                                              context_.input.activeDevice()),
                          "Back"}},
                        w, h, "party.footer");
}

}  // namespace cd
