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
#include "game/FieldSkills.hpp"  // M122: dungeon-only heal casting
#include "game/Scrolls.hpp"
#include "game/SkillInfo.hpp"  // M121: milestone-aware skill text
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

namespace {
constexpr int kSkillRows = 7;     // visible rows of the member's skill list
constexpr int kSkillRowH = 12;
}  // namespace

PartyState::PartyState(StateStack& stack, AppContext& context, bool inDungeon)
    : GameState(stack), context_(context), inDungeon_(inDungeon) {
    // M121/M122: a greyed skill is still a row to read - why it cannot be cast
    // here, and its full sheet through Details.
    skillMenu_.setFocusDisabled(true);
}

#ifdef CRYSTAL_CAPTURE
void PartyState::captureOpenSkills(int skillRow, bool pickTarget) {
    rebuildSkills();
    phase_ = Phase::Skills;
    skillMenu_.setCursor(skillRow);
    skillScroll_.follow(static_cast<int>(skillMenu_.size()), kSkillRows, skillMenu_.cursor());
    if (pickTarget) {
        confirmSkill();
    }
}
#endif

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
    bool anyMilestoneSkill = false;
    for (const std::string& id : known) {
        const content::SkillDef* s = db.findSkill(id);
        std::string name = s != nullptr ? s->name : id;
        if (std::find(c.extraSkills.begin(), c.extraSkills.end(), id) != c.extraSkills.end()) {
            name += "*";
            anyScroll = true;
        }
        // M121: the description as THIS member casts it (a held milestone's
        // adjusted text included); "^" marks a milestone-touched skill.
        const SkillTextFor text = s != nullptr ? skillTextFor(c, *s, db) : SkillTextFor{};
        if (text.marked()) {
            name += "^";
            anyMilestoneSkill = true;
        }
        body += "\n" + name;
        if (!text.description.empty()) {
            body += " - " + text.description;
        }
    }
    if (anyScroll) {
        body += "\n(* learned from a scroll)";
    }
    if (anyMilestoneSkill) {
        body += "\n(^ changed by a milestone)";
    }
    stack().pushState(
        std::make_unique<DetailsOverlayState>(stack(), context_, c.name, std::move(body)));
}

void PartyState::rebuildSkills() {
    std::vector<ui::MenuItem> rows;
    skillIds_.clear();
    if (context_.party.members.empty()) {
        skillMenu_.setItems({});
        return;
    }
    const content::ContentDatabase& db = context_.content;
    const Character& c = context_.party.members[static_cast<std::size_t>(cursor_)];
    for (const std::string& id : allKnownSkills(c, db)) {
        const content::SkillDef* s = db.findSkill(id);
        if (s == nullptr) {
            continue;
        }
        // A row is live only when the cast would work right now; every other
        // skill stays listed, greyed, with its reason beside the list.
        const bool castable = fieldSkillRefusal(context_.party, cursor_, *s, inDungeon_).empty();
        ui::MenuItem row{content::isSummonSkill(*s) ? s->summonName : s->name, castable,
                         s->mpCost > 0 ? "MP " + std::to_string(s->mpCost) : std::string()};
        row.icon = content::skillKindTextureId(s->kind);
        if (skillTextFor(c, *s, db).marked()) {
            row.icon2 = content::kMilestoneIconId;
        }
        rows.push_back(std::move(row));
        skillIds_.push_back(id);
    }
    const int previous = skillMenu_.cursor();
    skillMenu_.setItems(std::move(rows));
    skillMenu_.setCursor(previous);
    skillScroll_.follow(static_cast<int>(skillMenu_.size()), kSkillRows, skillMenu_.cursor());
}

const content::SkillDef* PartyState::currentSkill() const {
    const int row = skillMenu_.cursor();
    if (row < 0 || row >= static_cast<int>(skillIds_.size())) {
        return nullptr;
    }
    return context_.content.findSkill(skillIds_[static_cast<std::size_t>(row)]);
}

void PartyState::openSkillDetails() {
    const content::SkillDef* s = currentSkill();
    if (s == nullptr || context_.party.members.empty()) {
        return;
    }
    const Character& c = context_.party.members[static_cast<std::size_t>(cursor_)];
    // Only a healing skill has a reason worth a line here; "its moment is in
    // battle" on every attack would be noise on the sheet.
    const std::string why = isFieldSkill(*s) || content::isSummonSkill(*s)
                                ? fieldSkillRefusal(context_.party, cursor_, *s, inDungeon_)
                                : std::string();
    const bool marked = skillTextFor(c, *s, context_.content).marked();
    stack().pushState(std::make_unique<DetailsOverlayState>(
        stack(), context_, s->name,
        skillDetailsBody(*s, &c, context_.content, s->mpCost, why),
        content::skillKindTextureId(s->kind), marked ? content::kMilestoneIconId : ""));
}

void PartyState::afterCast(std::string line) {
    message_ = std::move(line);
    messageIsError_ = false;
    context_.audio.play(Sfx::Heal);
    rebuildSkills();  // MP fell, HP rose: every row's verdict may have changed
}

void PartyState::confirmSkill() {
    const content::SkillDef* s = currentSkill();
    if (s == nullptr) {
        return;
    }
    const std::string refusal = fieldSkillRefusal(context_.party, cursor_, *s, inDungeon_);
    if (!refusal.empty()) {
        message_ = refusal;
        messageIsError_ = true;
        context_.audio.play(Sfx::Error);
        return;
    }
    if (fieldSkillNeedsTarget(*s)) {
        // Open the pick on the first member the cast would actually help.
        targetCursor_ = 0;
        for (int i = 0; i < static_cast<int>(context_.party.members.size()); ++i) {
            if (fieldSkillHelps(*s, context_.party.members[static_cast<std::size_t>(i)])) {
                targetCursor_ = i;
                break;
            }
        }
        message_.clear();
        context_.audio.play(Sfx::Confirm);
        phase_ = Phase::PickTarget;
        return;
    }
    afterCast(applyFieldSkill(context_.party, cursor_, -1, *s, context_.content));
}

void PartyState::confirmTarget() {
    const content::SkillDef* s = currentSkill();
    const int count = static_cast<int>(context_.party.members.size());
    if (s == nullptr || targetCursor_ < 0 || targetCursor_ >= count) {
        return;
    }
    const std::string refusal =
        fieldTargetRefusal(*s, context_.party.members[static_cast<std::size_t>(targetCursor_)]);
    if (!refusal.empty()) {
        message_ = refusal;
        messageIsError_ = true;
        context_.audio.play(Sfx::Error);
        return;
    }
    afterCast(applyFieldSkill(context_.party, cursor_, targetCursor_, *s, context_.content));
    // Stay aimed for another cast while one is still possible; otherwise the
    // pick has nothing left to offer.
    if (!fieldSkillRefusal(context_.party, cursor_, *s, inDungeon_).empty()) {
        phase_ = Phase::Skills;
    }
}

void PartyState::handleInput(const Input& input) {
    const int count = static_cast<int>(context_.party.members.size());
    if (phase_ == Phase::PickTarget) {
        if (input.navPressed(InputAction::MoveUp) && count > 0) {
            targetCursor_ = (targetCursor_ + count - 1) % count;
            message_.clear();
            context_.audio.play(Sfx::Move);
        }
        if (input.navPressed(InputAction::MoveDown) && count > 0) {
            targetCursor_ = (targetCursor_ + 1) % count;
            message_.clear();
            context_.audio.play(Sfx::Move);
        }
        if (input.pressed(InputAction::Cancel)) {
            context_.audio.play(Sfx::Cancel);
            phase_ = Phase::Skills;
            return;
        }
        if (input.pressed(InputAction::Confirm)) {
            confirmTarget();
        }
        return;
    }
    if (phase_ == Phase::Skills) {
        if (input.navPressed(InputAction::MoveUp)) {
            skillMenu_.moveUp();
            message_.clear();
            context_.audio.play(Sfx::Move);
        }
        if (input.navPressed(InputAction::MoveDown)) {
            skillMenu_.moveDown();
            message_.clear();
            context_.audio.play(Sfx::Move);
        }
        skillScroll_.follow(static_cast<int>(skillMenu_.size()), kSkillRows, skillMenu_.cursor());
        if (input.pressed(InputAction::Cancel)) {
            context_.audio.play(Sfx::Cancel);
            message_.clear();
            phase_ = Phase::Browse;
            return;
        }
        if (input.pressed(InputAction::Details)) {
            context_.audio.play(Sfx::Confirm);
            openSkillDetails();
            return;
        }
        if (input.pressed(InputAction::Confirm)) {
            confirmSkill();
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
    // M122: Confirm opens the shown member's skill list (scrolls are taught
    // from the Items screen now).
    if (input.pressed(InputAction::Confirm) && count > 0) {
        skillMenu_.setCursor(0);
        rebuildSkills();
        if (skillIds_.empty()) {
            message_ = "No skills learned yet.";
            messageIsError_ = true;
            context_.audio.play(Sfx::Error);
        } else {
            message_.clear();
            context_.audio.play(Sfx::Confirm);
            phase_ = Phase::Skills;
        }
    }
}

// M122: the right-hand panel while a member's skills are open - the list on
// top, what the highlighted skill does (or why it cannot be cast here) below.
void PartyState::renderSkillPanel(int dx, int dy, int dw, int dh) const {
    const ui::style::Palette& p = ui::style::palette();
    const content::ContentDatabase& db = context_.content;
    const Character& c = context_.party.members[static_cast<std::size_t>(cursor_)];

    int y = dy;
    ui::drawTextFitted(c.name + "'s skills", dx, y, dw - 84, ui::style::kFontBody, p.gold,
                       "party.skills.title");
    const std::string mp = TextFormat("MP %d/%d", c.mp, c.maxMp);
    ui::drawText(mp, dx + dw - ui::measureText(mp, ui::style::kFontSmall), y + 1,
                 ui::style::kFontSmall, p.mpFill);
    y += 14;
    ui::drawMenuScrolled(skillMenu_, skillScroll_, kSkillRows, dx + 12, y, kSkillRowH,
                         ui::style::kFontBody, dw - 26, p.text, p.disabled, p.cursor,
                         "party.skills", ui::style::kFontSmall, p.mpFill, &context_.resources);
    y += kSkillRows * kSkillRowH + 4;
    ui::drawDivider(dx, y, dw);
    y += 5;

    const content::SkillDef* s = currentSkill();
    if (s == nullptr) {
        return;
    }
    const int bottom = dy + dh;
    if (phase_ == Phase::PickTarget) {
        const int count = static_cast<int>(context_.party.members.size());
        if (targetCursor_ >= 0 && targetCursor_ < count) {
            const Character& t = context_.party.members[static_cast<std::size_t>(targetCursor_)];
            const std::string refusal = fieldTargetRefusal(*s, t);
            ui::drawTextFitted("Cast " + s->name + " on " + t.name + "?", dx, y, dw,
                               ui::style::kFontBody, p.text, "party.skills.aim");
            y += 12;
            std::string effect = refusal;
            if (effect.empty()) {
                effect = t.hp > 0 ? "Restores up to " +
                                        std::to_string(fieldHealAmount(c, *s, db)) + " HP."
                                  : "Raises the fallen.";
            }
            ui::drawTextPreview(effect, dx, y, dw, ui::style::kFontBody,
                                refusal.empty() ? p.success : p.textDim,
                                std::max(1, (bottom - y) / ui::lineHeight(ui::style::kFontBody)),
                                /*markMore=*/false);
        }
        return;
    }
    // The kind in words, then why it is greyed (healing skills only - "its
    // moment is in battle" on every attack would be noise), then the text as
    // THIS member casts it.
    ui::drawTextFitted(skillKindLine(*s), dx, y, dw, ui::style::kFontSmall, p.textHint,
                       "party.skills.kind");
    y += 11;
    const bool healing = isFieldSkill(*s) || content::isSummonSkill(*s) ||
                         s->category == content::SkillCategory::Heal;
    const std::string why =
        healing ? fieldSkillRefusal(context_.party, cursor_, *s, inDungeon_) : std::string();
    if (!why.empty()) {
        y = ui::drawTextPreview(why, dx, y, dw, ui::style::kFontBody, p.textDim, 2,
                                /*markMore=*/false)
                .bottom;
    }
    const int lines = (bottom - y) / ui::lineHeight(ui::style::kFontBody);
    if (lines > 0) {
        ui::drawTextPreview(skillTextFor(c, *s, db).description, dx, y, dw, ui::style::kFontBody,
                            p.success, lines);
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
        // M122: while aiming a heal the slab follows the TARGET cursor and the
        // row shows HP instead of the class line; the caster stays gold.
        const bool aiming = phase_ == Phase::PickTarget;
        const int focus = aiming ? targetCursor_ : cursor_;
        if (static_cast<int>(i) == focus) {
            ui::drawSelectionSlab(listX - 2, y - 2, 124, 24);
        }
        // M67: each row leads with the class battle sprite — the party menu's
        // graphical representation of the character.
        const std::string sprId = "actor." + c.classId + ".battle";
        if (context_.resources.hasTexture(sprId)) {
            DrawTexture(context_.resources.texture(sprId), listX + 2, y - 2, WHITE);
        }
        const Color nameColor = aiming && static_cast<int>(i) == cursor_
                                    ? p.gold
                                    : (static_cast<int>(i) == focus ? p.text : p.textDim);
        ui::drawTextFitted(c.name, listX + 30, y, 86, 11, nameColor, "party.name");
        const content::ClassDef* cls = db.findClass(c.classId);
        if (aiming) {
            ui::drawTextFitted(c.hp > 0 ? TextFormat("HP %d/%d", c.hp, c.maxHp) : "Fallen",
                               listX + 30, y + 12, 86, ui::style::kFontSmall,
                               c.hp > 0 ? p.textHint : p.dangerText, "party.class");
        } else {
            ui::drawTextFitted(TextFormat("Lv.%d %s", c.level,
                                          cls != nullptr ? cls->name.c_str()
                                                         : c.classId.c_str()),
                               listX + 30, y + 12, 86, ui::style::kFontSmall, p.textHint,
                               "party.class");
        }
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
    const auto drawChrome = [&]() {
        if (!message_.empty()) {
            // M67: overlay banner (the equip-shop toast idiom).
            ui::drawBanner(messageIsError_ ? ui::BannerKind::Danger : ui::BannerKind::Success,
                           message_, 60, 40, w - 120, "party.message");
        }
        const InputMap& map = context_.input.map();
        const ActiveDevice device = context_.input.activeDevice();
        const char* confirmLabel = phase_ == Phase::Browse
                                       ? "Skills"
                                       : (phase_ == Phase::PickTarget ? "Cast" : "Cast");
        std::vector<ui::Hint> hints = {
            {input::primaryLabel(map, InputAction::Confirm, device), confirmLabel}};
        if (phase_ != Phase::PickTarget) {
            hints.push_back({input::primaryLabel(map, InputAction::Details, device), "Details"});
        }
        hints.push_back({input::primaryLabel(map, InputAction::Cancel, device), "Back"});
        ui::drawFooterHints(hints, w, h, "party.footer");
    };
    if (phase_ != Phase::Browse) {
        renderSkillPanel(dx, y, dw - 4, bottom - y);
        drawChrome();
        return;
    }
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

    drawChrome();
}

}  // namespace cd
