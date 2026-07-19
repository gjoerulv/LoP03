#include "states/BattleState.hpp"

#include <algorithm>
#include <utility>

#include "audio/AudioManager.hpp"
#include "content/ContentDatabase.hpp"
#include "content/Definitions.hpp"
#include "core/AppContext.hpp"
#include "core/FadeController.hpp"
#include "game/Party.hpp"
#include "input/Input.hpp"
#include "raylib.h"
#include "input/PromptLabels.hpp"
#include "settings/Settings.hpp"
#include "states/StateStack.hpp"
#include "ui/UiDraw.hpp"
#include "ui/UiStyle.hpp"

namespace cd {

namespace style = ui::style;

namespace {

// Bottom-panel layout (M12): everything must fit inside the 60px panel.
constexpr int kPanelH = 60;
constexpr int kListX = 24;       // command/skill/item lists (cursor at x-10)
constexpr int kListItemH = 12;
constexpr int kListRows = 4;     // visible rows in skill/item lists (scrolled)
constexpr int kInfoX = 150;      // right column: actor line + descriptions

bool skillNeedsTarget(content::SkillTarget t) {
    return t == content::SkillTarget::SingleEnemy || t == content::SkillTarget::SingleAlly;
}

const char* statusShort(content::StatusType t) {
    switch (t) {
        case content::StatusType::Poison: return "PSN";
        case content::StatusType::AttackUp: return "ATK+";
        case content::StatusType::AttackDown: return "ATK-";
        case content::StatusType::DefenseUp: return "DEF+";
        case content::StatusType::DefenseDown: return "DEF-";
        case content::StatusType::None: return "";
    }
    return "";
}
}  // namespace

BattleState::BattleState(StateStack& stack, AppContext& context, battle::Battle battle,
                         battle::BattleResult* resultSlot)
    : GameState(stack), context_(context), battle_(std::move(battle)), resultSlot_(resultSlot) {
    for (const battle::Combatant& c : battle_.units) {
        if (c.side == battle::Side::Enemy && c.isBoss) {
            bossBattle_ = true;
            if (!c.telegraph.empty()) {
                bossTelegraph_ = c.telegraph;
            }
        }
    }
    message_ = bossBattle_ ? "A boss appears!" : "A battle begins!";
    context_.fade.start();
    context_.audio.setMusic(MusicTrack::Battle);
}

int BattleState::enemyBaseY() const {
    int enemies = 0;
    for (const battle::Combatant& c : battle_.units) {
        if (c.side == battle::Side::Enemy) {
            ++enemies;
        }
    }
    // Five enemy rows only fit above the bottom panel when the column starts
    // higher (the turn counter lives top-right, so this space is free).
    return enemies >= 5 ? 20 : 36;
}

void BattleState::unitScreenPos(int index, int& outX, int& outY) const {
    int enemyRow = 0;
    int partyRow = 0;
    for (int i = 0; i < index; ++i) {
        if (battle_.units[static_cast<std::size_t>(i)].side == battle::Side::Enemy) {
            ++enemyRow;
        } else {
            ++partyRow;
        }
    }
    if (battle_.units[static_cast<std::size_t>(index)].side == battle::Side::Enemy) {
        outX = 36;
        outY = enemyBaseY() + enemyRow * 34;
    } else {
        outX = context_.virtualWidth - 110;
        outY = 36 + partyRow * 34;
    }
}

void BattleState::spawnNumbers(const std::vector<int>& hpBefore) {
    bool anyDamage = false;
    bool anyHeal = false;
    bool anyKo = false;
    for (std::size_t i = 0; i < battle_.units.size() && i < hpBefore.size(); ++i) {
        const int delta = battle_.units[i].hp - hpBefore[i];
        if (delta == 0) {
            continue;
        }
        int x = 0;
        int y = 0;
        unitScreenPos(static_cast<int>(i), x, y);
        FloatNumber f;
        f.x = static_cast<float>(x) + 14.0f;
        f.y = static_cast<float>(y);
        f.timer = 0.9f;
        f.heal = delta > 0;
        f.text = delta > 0 ? "+" + std::to_string(delta) : std::to_string(-delta);
        floats_.push_back(std::move(f));
        if (delta > 0) {
            anyHeal = true;
        } else {
            anyDamage = true;
        }
        if (hpBefore[i] > 0 && battle_.units[i].hp <= 0) {
            anyKo = true;
        }
    }
    if (anyKo) {
        context_.audio.play(Sfx::Ko);
    } else if (anyDamage) {
        context_.audio.play(Sfx::Hit);
    } else if (anyHeal) {
        context_.audio.play(Sfx::Heal);
    }
}

int BattleState::currentActor() const {
    if (order_.empty()) {
        return 0;
    }
    return order_[static_cast<std::size_t>(orderPos_)];
}

void BattleState::beginTurns() {
    order_ = battle::turnOrder(battle_);
    orderPos_ = 0;
    battle_.turnsTaken = 1;  // round 1
    startActorTurn();
}

void BattleState::startActorTurn() {
    const int actor = currentActor();
    if (!battle_.units[static_cast<std::size_t>(actor)].alive()) {
        advanceTurn();
        return;
    }
    // Status effects tick at the start of the unit's turn (poison, durations).
    std::vector<int> hpBefore;
    hpBefore.reserve(battle_.units.size());
    for (const battle::Combatant& u : battle_.units) {
        hpBefore.push_back(u.hp);
    }
    const std::string tick = battle_.tickStatuses(actor);
    spawnNumbers(hpBefore);
    if (!battle_.units[static_cast<std::size_t>(actor)].alive()) {
        message_ = tick;
        afterAction();  // poison felled the unit; skip its action
        return;
    }
    battle_.clearGuard(actor);
    if (battle_.units[static_cast<std::size_t>(actor)].side == battle::Side::Party) {
        phase_ = Phase::Command;
        buildCommandMenu();
    } else {
        executeEnemy(actor);
    }
}

void BattleState::advanceTurn() {
    const int guard = 2 * static_cast<int>(battle_.units.size()) + 4;
    for (int steps = 0; steps < guard; ++steps) {
        ++orderPos_;
        if (orderPos_ >= static_cast<int>(order_.size())) {
            order_ = battle::turnOrder(battle_);
            orderPos_ = 0;
            ++battle_.turnsTaken;  // a new round begins
        }
        if (!order_.empty() && battle_.units[static_cast<std::size_t>(order_[static_cast<std::size_t>(orderPos_)])].alive()) {
            startActorTurn();
            return;
        }
    }
    // Safety: no actor found (should not happen while Ongoing).
    result_ = battle_.outcome();
    phase_ = Phase::Done;
    message_ = outcomeMessage();
}

std::vector<std::string> BattleState::consumableIds() const {
    std::vector<std::string> ids;
    for (const ItemStack& s : context_.party.inventory.stacks) {
        const content::ItemDef* it = context_.content.findItem(s.itemId);
        if (it != nullptr && it->type == content::ItemType::Consumable && s.count > 0) {
            ids.push_back(s.itemId);
        }
    }
    return ids;
}

void BattleState::buildCommandMenu() {
    const battle::Combatant& a = battle_.units[static_cast<std::size_t>(currentActor())];
    const bool hasSkills = !a.skillIds.empty();
    const bool hasItems = !consumableIds().empty();
    commandMenu_.setItems({{"Attack", true},
                           {"Skill", hasSkills},
                           {"Item", hasItems},
                           {"Guard", true},
                           {"Escape", true}});
}

void BattleState::buildSkillMenu() {
    const battle::Combatant& a = battle_.units[static_cast<std::size_t>(currentActor())];
    skillIds_ = a.skillIds;
    std::vector<ui::MenuItem> items;
    for (const std::string& sid : skillIds_) {
        const content::SkillDef* s = context_.content.findSkill(sid);
        if (s == nullptr) {
            items.push_back({sid, false});
            continue;
        }
        const bool affordable = a.mp >= s->mpCost;
        items.push_back({s->name + "  (MP " + std::to_string(s->mpCost) + ")", affordable});
    }
    skillMenu_.setItems(std::move(items));
    skillScroll_.reset();
    skillScroll_.follow(static_cast<int>(skillMenu_.size()), kListRows, skillMenu_.cursor());
}

void BattleState::buildItemMenu() {
    itemIds_ = consumableIds();
    std::vector<ui::MenuItem> items;
    for (const std::string& id : itemIds_) {
        const content::ItemDef* it = context_.content.findItem(id);
        const std::string name = it != nullptr ? it->name : id;
        items.push_back({name + "  x" + std::to_string(context_.party.inventory.count(id)), true});
    }
    itemMenu_.setItems(std::move(items));
    itemScroll_.reset();
    itemScroll_.follow(static_cast<int>(itemMenu_.size()), kListRows, itemMenu_.cursor());
}

void BattleState::onCommand() {
    if (!commandMenu_.currentEnabled()) {
        return;
    }
    switch (commandMenu_.cursor()) {
        case 0:  // Attack
            pendingKind_ = PendingKind::Attack;
            targetCandidates_ = battle_.aliveIndices(battle::Side::Enemy);
            targetCursor_ = 0;
            phase_ = Phase::ChooseTarget;
            break;
        case 1:  // Skill
            buildSkillMenu();
            phase_ = Phase::ChooseSkill;
            break;
        case 2:  // Item
            buildItemMenu();
            phase_ = Phase::ChooseItem;
            break;
        case 3:  // Guard
            message_ = battle_.guard(currentActor());
            afterAction();
            break;
        case 4:  // Escape
            result_ = battle::Outcome::Escaped;
            message_ = "The party flees the battle!";
            phase_ = Phase::Done;
            break;
        default:
            break;
    }
}

void BattleState::onSkillChosen() {
    if (!skillMenu_.currentEnabled() || skillIds_.empty()) {
        return;
    }
    pendingSkillId_ = skillIds_[static_cast<std::size_t>(skillMenu_.cursor())];
    pendingKind_ = PendingKind::Skill;
    const content::SkillDef* s = context_.content.findSkill(pendingSkillId_);
    if (s == nullptr) {
        return;
    }
    if (skillNeedsTarget(s->target)) {
        const bool ally = s->target == content::SkillTarget::SingleAlly;
        targetCandidates_ = battle_.aliveIndices(ally ? battle::Side::Party : battle::Side::Enemy);
        targetCursor_ = 0;
        phase_ = Phase::ChooseTarget;
    } else {
        executePending(-1);
    }
}

void BattleState::onItemChosen() {
    if (itemIds_.empty()) {
        return;
    }
    pendingItemId_ = itemIds_[static_cast<std::size_t>(itemMenu_.cursor())];
    pendingKind_ = PendingKind::Item;
    // Items target any party member (alive or KO'd, for revives).
    targetCandidates_.clear();
    for (std::size_t i = 0; i < battle_.units.size(); ++i) {
        if (battle_.units[i].side == battle::Side::Party) {
            targetCandidates_.push_back(static_cast<int>(i));
        }
    }
    targetCursor_ = 0;
    phase_ = Phase::ChooseTarget;
}

void BattleState::executePending(int targetUnit) {
    const int actor = currentActor();
    std::vector<int> hpBefore;
    hpBefore.reserve(battle_.units.size());
    for (const battle::Combatant& u : battle_.units) {
        hpBefore.push_back(u.hp);
    }
    switch (pendingKind_) {
        case PendingKind::Attack:
            message_ = battle_.attack(actor, targetUnit);
            break;
        case PendingKind::Skill:
            if (const content::SkillDef* s = context_.content.findSkill(pendingSkillId_)) {
                message_ = battle_.useSkill(actor, targetUnit, *s);
            }
            break;
        case PendingKind::Item:
            if (const content::ItemDef* it = context_.content.findItem(pendingItemId_)) {
                message_ = battle_.useItem(actor, targetUnit, *it);
                context_.party.inventory.remove(pendingItemId_, 1);
            }
            break;
    }
    spawnNumbers(hpBefore);
    afterAction();
}

void BattleState::executeEnemy(int actor) {
    std::vector<int> hpBefore;
    hpBefore.reserve(battle_.units.size());
    for (const battle::Combatant& u : battle_.units) {
        hpBefore.push_back(u.hp);
    }
    const battle::EnemyChoice choice = battle::chooseEnemyAction(battle_, actor, context_.content);
    if (choice.useSkill) {
        if (const content::SkillDef* s = context_.content.findSkill(choice.skillId)) {
            message_ = battle_.useSkill(actor, choice.target, *s);
        }
    } else if (choice.target >= 0) {
        message_ = battle_.attack(actor, choice.target);
    } else {
        message_ = battle_.units[static_cast<std::size_t>(actor)].name + " hesitates.";
    }
    spawnNumbers(hpBefore);
    afterAction();
}

void BattleState::afterAction() {
    for (const battle::Combatant& c : battle_.units) {
        if (c.side == battle::Side::Party && c.hp <= 0) {
            koOccurred_ = true;
        }
    }
    phase_ = Phase::Resolve;
    // Battle speed setting scales the between-action pause; Confirm always
    // skips it regardless.
    timer_ = settings::resolveSeconds(context_.settings.values.battleSpeed);
}

void BattleState::finish() {
    for (const battle::Combatant& c : battle_.units) {
        if (c.partyIndex >= 0 && c.partyIndex < static_cast<int>(context_.party.members.size())) {
            Character& m = context_.party.members[static_cast<std::size_t>(c.partyIndex)];
            m.hp = c.hp;
            m.mp = c.mp;
        }
    }
    if (resultSlot_ != nullptr) {
        resultSlot_->outcome = result_;
        resultSlot_->rounds = battle_.turnsTaken;
        resultSlot_->partyKoOccurred = koOccurred_;
    }
    stack().popState();
}

std::string BattleState::outcomeMessage() const {
    switch (result_) {
        case battle::Outcome::Victory:
            return bossBattle_ ? "Victory! The dungeon boss is defeated!" : "Victory!";
        case battle::Outcome::Defeat:
            return "The party has fallen...";
        case battle::Outcome::Escaped:
            return "Escaped!";
        case battle::Outcome::Ongoing:
            return "";
    }
    return "";
}

void BattleState::handleInput(const Input& input) {
    // Up/Down only: Left/Right are reserved for future columns/adjust
    // (control standard; M13 dropped the old Left/Right aliases).
    const bool up = input.navPressed(InputAction::MoveUp);
    const bool down = input.navPressed(InputAction::MoveDown);

    if (phase_ != Phase::Resolve) {
        if (up || down) {
            context_.audio.play(Sfx::Move);
        }
        if (input.pressed(InputAction::Confirm)) {
            context_.audio.play(Sfx::Confirm);
        }
    }
    if (input.pressed(InputAction::Cancel)) {
        context_.audio.play(Sfx::Cancel);
    }

    switch (phase_) {
        case Phase::Intro:
            if (input.pressed(InputAction::Confirm)) {
                beginTurns();
            }
            break;
        case Phase::Command:
            if (up) commandMenu_.moveUp();
            if (down) commandMenu_.moveDown();
            if (input.pressed(InputAction::Confirm)) onCommand();
            break;
        case Phase::ChooseSkill:
            if (up) skillMenu_.moveUp();
            if (down) skillMenu_.moveDown();
            if (up || down) {
                skillScroll_.follow(static_cast<int>(skillMenu_.size()), kListRows,
                                    skillMenu_.cursor());
            }
            if (input.pressed(InputAction::Confirm)) onSkillChosen();
            if (input.pressed(InputAction::Cancel)) {
                phase_ = Phase::Command;
            }
            break;
        case Phase::ChooseItem:
            if (up) itemMenu_.moveUp();
            if (down) itemMenu_.moveDown();
            if (up || down) {
                itemScroll_.follow(static_cast<int>(itemMenu_.size()), kListRows,
                                   itemMenu_.cursor());
            }
            if (input.pressed(InputAction::Confirm)) onItemChosen();
            if (input.pressed(InputAction::Cancel)) {
                phase_ = Phase::Command;
            }
            break;
        case Phase::ChooseTarget:
            if (!targetCandidates_.empty()) {
                if (up) {
                    targetCursor_ = (targetCursor_ + static_cast<int>(targetCandidates_.size()) - 1) %
                                    static_cast<int>(targetCandidates_.size());
                }
                if (down) {
                    targetCursor_ = (targetCursor_ + 1) % static_cast<int>(targetCandidates_.size());
                }
                if (input.pressed(InputAction::Confirm)) {
                    executePending(targetCandidates_[static_cast<std::size_t>(targetCursor_)]);
                }
            }
            if (input.pressed(InputAction::Cancel)) {
                phase_ = Phase::Command;
            }
            break;
        case Phase::Resolve:
            if (input.pressed(InputAction::Confirm)) {
                timer_ = 0.0f;  // skip the pause
            }
            break;
        case Phase::Done:
            if (input.pressed(InputAction::Confirm)) {
                finish();
            }
            break;
    }
}

void BattleState::update(float dt) {
    for (FloatNumber& f : floats_) {
        f.y -= 22.0f * dt;
        f.timer -= dt;
    }
    floats_.erase(std::remove_if(floats_.begin(), floats_.end(),
                                 [](const FloatNumber& f) { return f.timer <= 0.0f; }),
                  floats_.end());

    if (phase_ != Phase::Resolve) {
        return;
    }
    timer_ -= dt;
    if (timer_ > 0.0f) {
        return;
    }
    const battle::Outcome o = battle_.outcome();
    if (o != battle::Outcome::Ongoing) {
        result_ = o;
        phase_ = Phase::Done;
        message_ = outcomeMessage();
        context_.audio.play(o == battle::Outcome::Victory ? Sfx::Victory : Sfx::Defeat);
    } else {
        advanceTurn();
    }
}

void BattleState::drawUnit(const battle::Combatant& c, int x, int y, bool current,
                           bool targeted) const {
    const Color body = c.side == battle::Side::Enemy ? Color{170, 80, 90, 255}
                                                     : Color{90, 130, 190, 255};
    DrawRectangle(x, y, 40, 16, c.alive() ? body : Color{60, 60, 70, 255});
    if (targeted) {
        DrawRectangleLines(x - 2, y - 2, 44, 20, Color{240, 220, 120, 255});
    }
    if (current) {
        DrawText(">", x - 9, y + 3, 10, Color{240, 220, 120, 255});
    }

    DrawText(c.name.c_str(), x, y - 9, 8, RAYWHITE);
    // HP bar.
    const float hpFrac = c.maxHp > 0 ? static_cast<float>(c.hp) / static_cast<float>(c.maxHp) : 0.0f;
    DrawRectangle(x, y + 17, 40, 3, Color{40, 40, 40, 255});
    DrawRectangle(x, y + 17, static_cast<int>(40 * hpFrac), 3, Color{90, 200, 110, 255});
    if (c.side == battle::Side::Party) {
        const float mpFrac =
            c.maxMp > 0 ? static_cast<float>(c.mp) / static_cast<float>(c.maxMp) : 0.0f;
        DrawRectangle(x, y + 21, 40, 2, Color{40, 40, 40, 255});
        DrawRectangle(x, y + 21, static_cast<int>(40 * mpFrac), 2, Color{90, 120, 210, 255});
        DrawText(TextFormat("%d/%d", c.hp, c.maxHp), x + 44, y + 4, 8, RAYWHITE);
    }
    if (!c.alive()) {
        DrawText("KO", x + 13, y + 4, 8, Color{220, 120, 120, 255});
    }

    if (!c.statuses.empty() && c.alive()) {
        std::string line;
        for (const battle::StatusInstance& s : c.statuses) {
            if (!line.empty()) {
                line += " ";
            }
            line += statusShort(s.type);
        }
        // Party rows have HP text to the right, so their statuses sit below;
        // enemy rows use the open field to the right, keeping tall enemy
        // columns clear of the bottom panel.
        if (c.side == battle::Side::Party) {
            ui::drawTextFitted(line, x, y + 24, 100, 8, Color{200, 160, 220, 255},
                               "battle.status.party");
        } else {
            ui::drawTextFitted(line, x + 44, y + 4, 120, 8, Color{200, 160, 220, 255},
                               "battle.status.enemy");
        }
    }
}

void BattleState::render() {
    const int w = context_.virtualWidth;
    const int h = context_.virtualHeight;
    ClearBackground(Color{18, 16, 24, 255});

    const int actor = currentActor();
    const bool partyTurn = phase_ == Phase::Command || phase_ == Phase::ChooseTarget ||
                           phase_ == Phase::ChooseSkill || phase_ == Phase::ChooseItem;

    int enemyRow = 0;
    int partyRow = 0;
    int targetUnit = -1;
    if (phase_ == Phase::ChooseTarget && !targetCandidates_.empty()) {
        targetUnit = targetCandidates_[static_cast<std::size_t>(targetCursor_)];
    }
    const int enemyY0 = enemyBaseY();
    for (std::size_t i = 0; i < battle_.units.size(); ++i) {
        const battle::Combatant& c = battle_.units[i];
        const bool isCurrent = partyTurn && static_cast<int>(i) == actor;
        const bool isTarget = static_cast<int>(i) == targetUnit;
        if (c.side == battle::Side::Enemy) {
            drawUnit(c, 36, enemyY0 + enemyRow * 34, isCurrent, isTarget);
            ++enemyRow;
        } else {
            drawUnit(c, w - 110, 36 + partyRow * 34, isCurrent, isTarget);
            ++partyRow;
        }
    }

    // Turn counter top-right: the top-left is needed by tall enemy columns.
    ui::drawTextRight(TextFormat("Turns: %d", battle_.turnsTaken), w - 6, 6, 8,
                      style::kTextDim);

    // Floating damage / heal numbers.
    for (const FloatNumber& f : floats_) {
        DrawText(f.text.c_str(), static_cast<int>(f.x), static_cast<int>(f.y), 10,
                 f.heal ? Color{120, 220, 120, 255} : Color{245, 120, 120, 255});
    }

    // Bottom panel: lists live in the left column (scrolled to kListRows),
    // the actor line and selected-entry description in the right column.
    const int panelY = h - kPanelH - 4;
    ui::drawPanel(4, panelY, w - 8, kPanelH, Color{24, 22, 34, 240}, Color{120, 120, 160, 255});
    const int infoW = w - kInfoX - 12;
    const int listLabelW = kInfoX - kListX - 18;
    const InputMap& map = context_.input.map();
    const ActiveDevice device = context_.input.activeDevice();
    const std::string backHint = input::prompt(map, InputAction::Cancel, device, "Back");

    switch (phase_) {
        case Phase::Intro:
            ui::drawTextCentered(message_.c_str(), w / 2, panelY + 6, 12, style::kText);
            if (!bossTelegraph_.empty()) {
                ui::drawTextWrapped(bossTelegraph_, 16, panelY + 22, w - 32, style::kFontBody,
                                    Color{225, 170, 170, 255}, "battle.telegraph", 2);
            }
            ui::drawTextCentered(
                input::prompt(map, InputAction::Confirm, device, "Begin").c_str(), w / 2,
                panelY + 48, style::kFontBody, Color{200, 200, 160, 255});
            break;
        case Phase::Command:
            ui::drawMenu(commandMenu_, kListX, panelY + 5, 11, style::kFontBody, style::kText,
                         style::kDisabled, style::kCursor);
            ui::drawTextFitted(
                TextFormat("%s's turn",
                           battle_.units[static_cast<std::size_t>(actor)].name.c_str()),
                kInfoX, panelY + 6, infoW, style::kFontBody, style::kText, "battle.actor");
            break;
        case Phase::ChooseSkill: {
            ui::drawMenuScrolled(skillMenu_, skillScroll_, kListRows, kListX, panelY + 6,
                                 kListItemH, style::kFontBody, listLabelW, style::kText,
                                 style::kDisabled, style::kCursor, "battle.skills");
            ui::drawTextFitted("Choose a skill   " + backHint, kInfoX, panelY + 6, infoW,
                               style::kFontBody, style::kTextDim, "battle.skillhint");
            if (!skillIds_.empty()) {
                const std::string& sid =
                    skillIds_[static_cast<std::size_t>(skillMenu_.cursor())];
                if (const content::SkillDef* s = context_.content.findSkill(sid)) {
                    if (!s->description.empty()) {
                        ui::drawTextWrapped(s->description, kInfoX, panelY + 20, infoW,
                                            style::kFontBody, style::kSuccess,
                                            "battle.skilldesc", 2);
                    }
                }
            }
            break;
        }
        case Phase::ChooseItem: {
            ui::drawMenuScrolled(itemMenu_, itemScroll_, kListRows, kListX, panelY + 6,
                                 kListItemH, style::kFontBody, listLabelW, style::kText,
                                 style::kDisabled, style::kCursor, "battle.items");
            ui::drawTextFitted("Choose an item   " + backHint, kInfoX, panelY + 6, infoW,
                               style::kFontBody, style::kTextDim, "battle.itemhint");
            if (!itemIds_.empty()) {
                const std::string& iid = itemIds_[static_cast<std::size_t>(itemMenu_.cursor())];
                if (const content::ItemDef* it = context_.content.findItem(iid)) {
                    if (!it->description.empty()) {
                        ui::drawTextWrapped(it->description, kInfoX, panelY + 20, infoW,
                                            style::kFontBody, style::kSuccess,
                                            "battle.itemdesc", 2);
                    }
                }
            }
            break;
        }
        case Phase::ChooseTarget:
            ui::drawTextCentered(("Choose a target   " + backHint).c_str(), w / 2, panelY + 24,
                                 style::kFontBody, style::kText);
            break;
        case Phase::Resolve:
            ui::drawTextWrapped(message_, 16, panelY + 14, w - 32, style::kFontBody,
                                style::kText, "battle.message", 3);
            break;
        case Phase::Done:
            ui::drawTextWrapped(message_, 16, panelY + 10, w - 32, 12,
                                Color{240, 230, 160, 255}, "battle.outcome", 2);
            ui::drawTextCentered(
                input::prompt(map, InputAction::Confirm, device, "Continue").c_str(), w / 2,
                panelY + 44, style::kFontBody, Color{200, 200, 160, 255});
            break;
    }
}

}  // namespace cd
