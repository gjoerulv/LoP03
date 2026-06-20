#include "states/BattleState.hpp"

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

namespace cd {

namespace {
constexpr float kResolveTime = 0.9f;

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
    const std::string tick = battle_.tickStatuses(actor);
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
    afterAction();
}

void BattleState::executeEnemy(int actor) {
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
    afterAction();
}

void BattleState::afterAction() {
    for (const battle::Combatant& c : battle_.units) {
        if (c.side == battle::Side::Party && c.hp <= 0) {
            koOccurred_ = true;
        }
    }
    phase_ = Phase::Resolve;
    timer_ = kResolveTime;
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
    const bool up = input.pressed(InputAction::MoveUp) || input.pressed(InputAction::MoveLeft);
    const bool down = input.pressed(InputAction::MoveDown) || input.pressed(InputAction::MoveRight);

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
            if (input.pressed(InputAction::Confirm)) onSkillChosen();
            if (input.pressed(InputAction::Cancel)) {
                phase_ = Phase::Command;
            }
            break;
        case Phase::ChooseItem:
            if (up) itemMenu_.moveUp();
            if (down) itemMenu_.moveDown();
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
        DrawText(line.c_str(), x, y + 24, 8, Color{200, 160, 220, 255});
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
    for (std::size_t i = 0; i < battle_.units.size(); ++i) {
        const battle::Combatant& c = battle_.units[i];
        const bool isCurrent = partyTurn && static_cast<int>(i) == actor;
        const bool isTarget = static_cast<int>(i) == targetUnit;
        if (c.side == battle::Side::Enemy) {
            drawUnit(c, 36, 36 + enemyRow * 34, isCurrent, isTarget);
            ++enemyRow;
        } else {
            drawUnit(c, w - 110, 36 + partyRow * 34, isCurrent, isTarget);
            ++partyRow;
        }
    }

    DrawText(TextFormat("Turns: %d", battle_.turnsTaken), 6, 6, 8, Color{170, 170, 190, 255});

    // Bottom panel.
    const int panelY = h - 64;
    ui::drawPanel(4, panelY, w - 8, 60, Color{24, 22, 34, 240}, Color{120, 120, 160, 255});

    switch (phase_) {
        case Phase::Intro:
            ui::drawTextCentered(message_.c_str(), w / 2, panelY + 8, 12, RAYWHITE);
            if (!bossTelegraph_.empty()) {
                ui::drawTextCentered(bossTelegraph_.c_str(), w / 2, panelY + 28, 9,
                                     Color{225, 170, 170, 255});
            }
            ui::drawTextCentered("Confirm to begin", w / 2, panelY + 44, 10,
                                 Color{200, 200, 160, 255});
            break;
        case Phase::Command:
            DrawText(TextFormat("%s's turn", battle_.units[static_cast<std::size_t>(actor)].name.c_str()),
                     12, panelY + 6, 10, RAYWHITE);
            ui::drawMenu(commandMenu_, 24, panelY + 22, 12, 10, RAYWHITE, Color{90, 90, 110, 255},
                         Color{240, 220, 120, 255});
            break;
        case Phase::ChooseSkill:
            DrawText("Skill:", 12, panelY + 6, 10, RAYWHITE);
            ui::drawMenu(skillMenu_, 24, panelY + 20, 11, 10, RAYWHITE, Color{90, 90, 110, 255},
                         Color{240, 220, 120, 255});
            break;
        case Phase::ChooseItem:
            DrawText("Item:", 12, panelY + 6, 10, RAYWHITE);
            ui::drawMenu(itemMenu_, 24, panelY + 20, 11, 10, RAYWHITE, Color{90, 90, 110, 255},
                         Color{240, 220, 120, 255});
            break;
        case Phase::ChooseTarget:
            ui::drawTextCentered("Choose a target  (Cancel: back)", w / 2, panelY + 24, 10,
                                 RAYWHITE);
            break;
        case Phase::Resolve:
            ui::drawTextCentered(message_.c_str(), w / 2, panelY + 24, 10, RAYWHITE);
            break;
        case Phase::Done:
            ui::drawTextCentered(message_.c_str(), w / 2, panelY + 16, 12,
                                 Color{240, 230, 160, 255});
            ui::drawTextCentered("Confirm to continue", w / 2, panelY + 40, 10,
                                 Color{200, 200, 160, 255});
            break;
    }
}

}  // namespace cd
