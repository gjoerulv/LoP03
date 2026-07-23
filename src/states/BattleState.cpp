#include "states/BattleState.hpp"

#include <algorithm>
#include <memory>
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
#include "resource/ResourceManager.hpp"
#include "settings/Settings.hpp"
#include "states/DetailsOverlayState.hpp"
#include "states/StateStack.hpp"
#include "states/TutorialPromptState.hpp"
#include "tutorial/Tutorial.hpp"
#include "ui/UiDraw.hpp"
#include "ui/UiStyle.hpp"

namespace cd {

namespace style = ui::style;

namespace {

// Bottom-panel layout (M12): everything must fit inside the 60px panel.
constexpr int kPanelH = 60;
constexpr int kListX = 20;       // command/skill/item lists (selection slab at x-12)
constexpr int kListItemH = 12;
constexpr int kListRows = 4;     // visible rows in skill/item lists (scrolled)
// Right column: actor line + descriptions. The split is set by the widest skill
// row (longest name + its right-aligned MP column) so neither is ever clipped;
// the description column keeps enough room for two wrapped lines.
constexpr int kInfoX = 186;

bool skillNeedsTarget(content::SkillTarget t) {
    return t == content::SkillTarget::SingleEnemy || t == content::SkillTarget::SingleAlly;
}

// M45: the Jester's mid-battle quips — twelve original dry one-liners in the
// voice of the castle Jester (M41). Presentation only: which one appears (and
// whether one appears at all) comes from a pure hash that never touches the
// battle's roll stream, so a quip can never change an outcome.
constexpr const char* kJestLines[] = {
    "I had a plan. It left.",
    "Do stop me if you've seen this one.",
    "Gerald would have LOVED that.",
    "That was intentional. Mostly.",
    "I'm told there's a strategy. Sounds exhausting.",
    "Aim is a construct. So is the floor.",
    "Somewhere, a goose is judging me.",
    "The spoon is still down the well, you know.",
    "I meant to do the other thing.",
    "Bold of you to expect a pattern.",
    "This is fine. This is the bit that's fine.",
    "Applause optional. Encouraged, but optional.",
};
constexpr int kJestLineCount = static_cast<int>(sizeof(kJestLines) / sizeof(kJestLines[0]));

const char* statusShort(content::StatusType t) {
    switch (t) {
        case content::StatusType::Poison: return "PSN";
        case content::StatusType::AttackUp: return "ATK+";
        case content::StatusType::AttackDown: return "ATK-";
        case content::StatusType::DefenseUp: return "DEF+";
        case content::StatusType::DefenseDown: return "DEF-";
        case content::StatusType::Confusion: return "CNF";
        case content::StatusType::Silence: return "SIL";
        case content::StatusType::Blind: return "BLD";
        case content::StatusType::Terrified: return "TRF";  // M44: forced to Guard
        case content::StatusType::Stunned: return "STN";    // M44: skips its turn
        case content::StatusType::None: return "";
    }
    return "";
}

std::string joinTags(const std::vector<std::string>& tags) {
    std::string out;
    for (const std::string& t : tags) {
        if (!out.empty()) {
            out += " ";
        }
        out += t;
    }
    return out;
}

// Packs a unit's *visible* status tags into at most maxLines lines that each fit
// maxWidth. Whatever still does not fit is summarised as a trailing "+N" rather
// than clipped, so a tag is never silently lost (Details lists them in full).
std::vector<std::string> statusLines(const battle::Combatant& c, int maxWidth, int fontSize,
                                     int maxLines) {
    std::vector<std::string> tags;
    for (const battle::StatusInstance& s : c.statuses) {
        if (battle::isImmuneTo(c, s.type)) {
            continue;  // M40: an immune unit never reads as afflicted
        }
        const std::string tag = statusShort(s.type);
        if (!tag.empty()) {
            tags.push_back(tag);
        }
    }
    std::vector<std::vector<std::string>> packed;
    for (const std::string& tag : tags) {
        if (!packed.empty() &&
            ui::measureText(joinTags(packed.back()) + " " + tag, fontSize) <= maxWidth) {
            packed.back().push_back(tag);
            continue;
        }
        packed.push_back({tag});
    }

    int hidden = 0;
    if (static_cast<int>(packed.size()) > maxLines) {
        int shown = 0;
        packed.resize(static_cast<std::size_t>(maxLines));
        for (const std::vector<std::string>& line : packed) {
            shown += static_cast<int>(line.size());
        }
        hidden = static_cast<int>(tags.size()) - shown;
        // Give the marker room by dropping tags off the last line as needed.
        std::vector<std::string>& last = packed.back();
        while (last.size() > 1 &&
               ui::measureText(joinTags(last) + " +" + std::to_string(hidden), fontSize) >
                   maxWidth) {
            last.pop_back();
            ++hidden;
        }
    }

    std::vector<std::string> lines;
    for (std::size_t i = 0; i < packed.size(); ++i) {
        std::string line = joinTags(packed[i]);
        if (hidden > 0 && i + 1 == packed.size()) {
            line += " +" + std::to_string(hidden);
        }
        lines.push_back(std::move(line));
    }
    return lines;
}
}  // namespace

BattleState::BattleState(StateStack& stack, AppContext& context, battle::Battle battle,
                         battle::BattleResult* resultSlot, MusicTrack musicOverride,
                         RunStats* statsSlot, bool castleChallenge)
    : GameState(stack),
      context_(context),
      battle_(std::move(battle)),
      resultSlot_(resultSlot),
      musicOverride_(musicOverride),
      stats_(statsSlot),
      castleChallenge_(castleChallenge) {
    for (const battle::Combatant& c : battle_.units) {
        if (c.side == battle::Side::Enemy && c.isBoss) {
            bossBattle_ = true;
            if (!c.telegraph.empty()) {
                bossTelegraph_ = c.telegraph;
            }
        }
        // M42 bestiary: record every foe faced (dedup, insertion order kept).
        if (c.side == battle::Side::Enemy && !c.sourceId.empty()) {
            std::vector<std::string>& seen = context_.party.encountered;
            if (std::find(seen.begin(), seen.end(), c.sourceId) == seen.end()) {
                seen.push_back(c.sourceId);
            }
        }
    }
    message_ = bossBattle_ ? "A boss appears!" : "A battle begins!";
    displayHp_.reserve(battle_.units.size());
    for (const battle::Combatant& c : battle_.units) {
        displayHp_.push_back(c.hp);
    }
    hitFlags_.assign(battle_.units.size(), 0);
    koFade_.assign(battle_.units.size(), 1.0f);
    context_.fade.start();
    context_.audio.setMusic(musicOverride_ != MusicTrack::None
                                ? musicOverride_
                                : (bossBattle_ ? MusicTrack::Boss : MusicTrack::Battle));
}

void BattleState::onEnter() { maybeTutorialPrompt(stack(), context_, tutorial::kBattleFirst); }

#ifdef CRYSTAL_CAPTURE
void BattleState::captureEnterTargeting() {
    order_ = battle::turnOrder(battle_);
    orderPos_ = 0;
    for (std::size_t i = 0; i < order_.size(); ++i) {
        const int u = order_[static_cast<std::size_t>(i)];
        if (battle_.units[static_cast<std::size_t>(u)].side == battle::Side::Party &&
            battle_.units[static_cast<std::size_t>(u)].alive()) {
            orderPos_ = static_cast<int>(i);
            break;
        }
    }
    pendingKind_ = PendingKind::Attack;
    targetCandidates_ = battle_.aliveIndices(battle::Side::Enemy);
    targetCursor_ = 0;
    phase_ = Phase::ChooseTarget;
}

void BattleState::captureEnterSkillMenu(std::vector<std::string> skills) {
    captureEnterTargeting();  // reuse: puts a living party member on turn
    if (!skills.empty()) {
        battle_.units[static_cast<std::size_t>(currentActor())].skillIds = std::move(skills);
    }
    buildSkillMenu();
    phase_ = Phase::ChooseSkill;
}

void BattleState::captureEnterItemMenu() {
    captureEnterTargeting();  // reuse: puts a living party member on turn
    buildItemMenu();
    phase_ = Phase::ChooseItem;
}

void BattleState::captureShowJest() {
    captureEnterTargeting();
    const char* longest = kJestLines[0];
    for (const char* line : kJestLines) {
        if (std::string(line).size() > std::string(longest).size()) {
            longest = line;
        }
    }
    jestLine_ = longest;
    jestTimer_ = 999.0f;  // captures render one frame; never expire
}
#endif

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

void BattleState::stageNumbers(const std::vector<int>& hpBefore, int damageSfx,
                               bool statusAction) {
    hitFlags_.assign(battle_.units.size(), 0);
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
        pendingFloats_.push_back(std::move(f));
        if (delta > 0) {
            anyHeal = true;
        } else {
            anyDamage = true;
            hitFlags_[i] = 1;  // brightened during the impact beat
        }
        if (hpBefore[i] > 0 && battle_.units[i].hp <= 0) {
            anyKo = true;
        }
    }
    // "Miss!" floaters for Blind misses (M35): the missed units took no HP delta,
    // so they are surfaced here from the action's recorded miss list.
    for (int mi : battle_.lastMissed) {
        if (mi < 0 || mi >= static_cast<int>(battle_.units.size())) {
            continue;
        }
        int mx = 0;
        int my = 0;
        unitScreenPos(mi, mx, my);
        FloatNumber f;
        f.x = static_cast<float>(mx) + 14.0f;
        f.y = static_cast<float>(my);
        f.timer = 0.9f;
        f.heal = false;
        f.text = "Miss!";
        pendingFloats_.push_back(std::move(f));
    }
    pendingSfx_ = anyKo ? 3 : (anyDamage ? damageSfx : (anyHeal ? 1 : (statusAction ? 5 : 0)));
}

void BattleState::commitPresentation() {
    for (FloatNumber& f : pendingFloats_) {
        floats_.push_back(std::move(f));
    }
    pendingFloats_.clear();
    for (std::size_t i = 0; i < battle_.units.size(); ++i) {
        if (battle_.units[i].hp > 0 && displayHp_[i] <= 0) {
            koFade_[i] = 1.0f;  // revived (Commander rally, revive items): rise again
        }
        displayHp_[i] = battle_.units[i].hp;
    }
    switch (pendingSfx_) {
        case 5: context_.audio.play(Sfx::Status); break;
        case 4: context_.audio.play(Sfx::HitMagic); break;
        case 3: context_.audio.play(Sfx::Ko); break;
        case 2: context_.audio.play(Sfx::Hit); break;
        case 1: context_.audio.play(Sfx::Heal); break;
        default: break;
    }
    pendingSfx_ = 0;
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
    battle_.beginRound();    // enmity decay at round start (M28); mirrors Simulator
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
    stageNumbers(hpBefore);
    commitPresentation();  // status ticks are not action-staged; show at once
    if (!battle_.units[static_cast<std::size_t>(actor)].alive()) {
        message_ = tick;
        afterAction();  // poison felled the unit; skip its action
        return;
    }
    battle_.clearGuard(actor);
    if (battle_.units[static_cast<std::size_t>(actor)].side == battle::Side::Party) {
        // Forced actions (M35 confusion, M44 Terrified/Stunned): a character whose
        // turn was taken away gets no player input — it resolves automatically,
        // just like an enemy's turn, through the one shared rule.
        if (battle::forcedActionFor(battle_.units[static_cast<std::size_t>(actor)]) !=
            battle::ForcedAction::None) {
            executeConfused(actor);
        } else if (battle_.units[static_cast<std::size_t>(actor)].uncontrolled) {
            executeUncontrolled(actor);  // M45: the Jester decides for itself
        } else {
            phase_ = Phase::Command;
            buildCommandMenu();
        }
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
            battle_.beginRound();  // enmity decay at round start (M28)
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
        // Silence (M35): MP-cost skills are blocked and labelled so the player
        // sees *why* they are greyed (non-color-alone, M22).
        const bool silencedBlocked = !battle::canCast(a, *s);
        const bool enabled = a.mp >= s->mpCost && !silencedBlocked;
        // The cost lives in its own right-aligned column so a long skill name can
        // never push it out of view; a blocked skill shows the reason instead.
        std::string suffix;
        if (silencedBlocked) {
            suffix = "SIL";
        } else if (s->mpCost > 0) {
            suffix = "MP " + std::to_string(s->mpCost);
        }
        items.push_back({s->name, enabled, std::move(suffix)});
    }
    skillMenu_.setItems(std::move(items));
    skillScroll_.reset();
    skillScroll_.follow(static_cast<int>(skillMenu_.size()), kListRows, skillMenu_.cursor());
}

std::string BattleState::itemBlockReason(const content::ItemDef& item) const {
    // M43: why an item is greyed out, spelled out rather than left to the player
    // to decode (M22, non-color-alone). Empty means the item is usable.
    if (!battle::itemTargets(battle_, battle::Side::Party, item).empty()) {
        return "";
    }
    if (item.battleTarget == content::BattleTarget::Enemy) {
        return "No foe left to use this on.";  // M44 relics
    }
    return item.effect == content::ConsumableEffect::Revive
               ? "No fallen ally to revive."
               : "No able ally to use this on.";
}

void BattleState::buildItemMenu() {
    itemIds_ = consumableIds();
    std::vector<ui::MenuItem> items;
    for (const std::string& id : itemIds_) {
        const content::ItemDef* it = context_.content.findItem(id);
        const std::string name = it != nullptr ? it->name : id;
        // An item with no legal target (M43) is offered greyed, never spent.
        const bool enabled = it != nullptr && itemBlockReason(*it).empty();
        items.push_back(
            {name, enabled, "x" + std::to_string(context_.party.inventory.count(id))});
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
        // M43: a revive-capable ally skill (Renew) may be aimed at the fallen.
        targetCandidates_ = ally ? battle::skillAllyTargets(battle_, battle::Side::Party, *s)
                                 : battle_.aliveIndices(battle::Side::Enemy);
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
    if (!itemMenu_.currentEnabled()) {
        context_.audio.play(Sfx::Error);
        return;  // no legal target; the reason is shown beside the list
    }
    pendingItemId_ = itemIds_[static_cast<std::size_t>(itemMenu_.cursor())];
    const content::ItemDef* it = context_.content.findItem(pendingItemId_);
    if (it == nullptr) {
        return;
    }
    // M43: candidates follow the item's effect - a potion cannot be poured into a
    // fallen ally, and a Phoenix Tear reaches only the fallen, so an item can no
    // longer be spent on a "No effect" no-op. M44: a relic aims at the enemies.
    targetCandidates_ = battle::itemTargets(battle_, battle::Side::Party, *it);
    if (targetCandidates_.empty()) {
        return;
    }
    pendingKind_ = PendingKind::Item;
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
    int damageSfx = 2;
    bool statusAction = false;
    bool offensiveStatus = false;  // M42: a status-carrying skill aimed at enemies
    switch (pendingKind_) {
        case PendingKind::Attack:
            message_ = battle_.attack(actor, targetUnit);
            break;
        case PendingKind::Skill:
            if (const content::SkillDef* s = context_.content.findSkill(pendingSkillId_)) {
                message_ = battle_.useSkill(actor, targetUnit, *s);
                damageSfx = s->category == content::SkillCategory::Magic ? 4 : 2;
                statusAction = s->statusEffect != content::StatusType::None;
                offensiveStatus = statusAction &&
                                  (s->target == content::SkillTarget::SingleEnemy ||
                                   s->target == content::SkillTarget::AllEnemies);
            }
            break;
        case PendingKind::Item:
            if (const content::ItemDef* it = context_.content.findItem(pendingItemId_)) {
                // M44: an item that does nothing here (the Dragon Crown outside the
                // King's fight) is NOT spent - ask before the battle mutates.
                const bool spends = battle::itemAffects(battle_, targetUnit, *it);
                message_ = battle_.useItem(actor, targetUnit, *it);
                statusAction = !it->statuses.empty();
                offensiveStatus =
                    statusAction && it->battleTarget == content::BattleTarget::Enemy;
                if (spends) {
                    context_.party.inventory.remove(pendingItemId_, 1);
                } else {
                    message_ += " (kept)";
                }
            }
            break;
    }
    accumulateStats(hpBefore, actor, offensiveStatus);
    stageNumbers(hpBefore, damageSfx, statusAction);
    afterAction();
}

void BattleState::accumulateStats(const std::vector<int>& hpBefore, int actor,
                                  bool offensiveStatus) {
    if (stats_ == nullptr) {
        return;  // castle challenges pass no slot; their own records cover them
    }
    int actionDmg = 0;
    int biggest = 0;
    for (std::size_t i = 0; i < battle_.units.size(); ++i) {
        if (battle_.units[i].side != battle::Side::Enemy) {
            continue;
        }
        const int d = hpBefore[i] - battle_.units[i].hp;
        if (d > 0) {
            actionDmg += d;
            if (d > biggest) {
                biggest = d;
            }
        }
    }
    const int pm = battle_.units[static_cast<std::size_t>(actor)].partyIndex;
    if (pm >= 0 && pm < kRunStatsMembers) {
        stats_->damageByMember[pm] += actionDmg;
    }
    stats_->totalDamage += actionDmg;
    if (biggest > stats_->biggestHit) {
        stats_->biggestHit = biggest;
    }
    if (offensiveStatus) {
        stats_->statusesInflicted++;
    }
}

void BattleState::executeEnemy(int actor) {
    std::vector<int> hpBefore;
    hpBefore.reserve(battle_.units.size());
    for (const battle::Combatant& u : battle_.units) {
        hpBefore.push_back(u.hp);
    }
    const battle::EnemyChoice choice = battle::chooseEnemyAction(battle_, actor, context_.content);
    int damageSfx = 2;
    bool statusAction = false;
    const battle::Combatant& self = battle_.units[static_cast<std::size_t>(actor)];
    if (choice.forced == battle::ForcedAction::Guard) {
        // M44 (Evil Goose): the foe is too frightened to do anything but guard.
        message_ = self.name + " is terrified and can only cower behind its guard!";
        battle_.guard(actor);
    } else if (choice.forced == battle::ForcedAction::Skip) {
        // M44 (Tax Sheets): the foe spends its turn on the paperwork.
        message_ = self.name + " is buried in paperwork and loses its turn!";
    } else if (choice.useSkill) {
        if (const content::SkillDef* s = context_.content.findSkill(choice.skillId)) {
            message_ = battle_.useSkill(actor, choice.target, *s);
            damageSfx = s->category == content::SkillCategory::Magic ? 4 : 2;
            statusAction = s->statusEffect != content::StatusType::None;
        }
    } else if (choice.target >= 0) {
        message_ = battle_.attack(actor, choice.target);
    } else {
        message_ = battle_.units[static_cast<std::size_t>(actor)].name + " hesitates.";
    }
    stageNumbers(hpBefore, damageSfx, statusAction);
    afterAction();
}

void BattleState::executeConfused(int actor) {
    std::vector<int> hpBefore;
    hpBefore.reserve(battle_.units.size());
    for (const battle::Combatant& u : battle_.units) {
        hpBefore.push_back(u.hp);
    }
    // M43/M44: the forced action comes from the shared rule the Simulator and the
    // enemy AI also use, so an imposed turn resolves identically everywhere.
    const battle::Combatant& a = battle_.units[static_cast<std::size_t>(actor)];
    const battle::ForcedAction forced = battle::forcedActionFor(a);
    const battle::EnemyChoice choice = battle::forcedChoice(battle_, actor, forced);
    switch (forced) {
        case battle::ForcedAction::Guard:
            message_ = a.name + " is terrified and can only cower behind its guard!";
            battle_.guard(actor);
            break;
        case battle::ForcedAction::Skip:
            message_ = a.name + " is buried in paperwork and loses its turn!";
            break;
        case battle::ForcedAction::BasicAttack:
        case battle::ForcedAction::None:
            message_ = battle_.attack(actor, choice.target);
            break;
    }
    stageNumbers(hpBefore, 2, false);
    afterAction();
}

void BattleState::executeUncontrolled(int actor) {
    std::vector<int> hpBefore;
    hpBefore.reserve(battle_.units.size());
    for (const battle::Combatant& u : battle_.units) {
        hpBefore.push_back(u.hp);
    }
    // M45: the same shared, pure rule the Simulator uses — the player gets no say,
    // and neither driver can drift from the other.
    const battle::EnemyChoice choice =
        battle::uncontrolledChoice(battle_, actor, context_.content);
    int damageSfx = 2;
    bool statusAction = false;
    if (choice.target < 0) {
        message_ = battle_.units[static_cast<std::size_t>(actor)].name + " capers pointlessly.";
    } else if (choice.useSkill) {
        if (const content::SkillDef* s = context_.content.findSkill(choice.skillId)) {
            message_ = battle_.useSkill(actor, choice.target, *s);
            damageSfx = s->category == content::SkillCategory::Magic ? 4 : 2;
            statusAction = s->statusEffect != content::StatusType::None;
        }
    } else {
        message_ = battle_.attack(actor, choice.target);
    }
    // The quip rides on top of the resolved action and never changes it: a pure
    // hash under its own salt, so hiding or showing it cannot move the battle.
    int line = 0;
    if (battle::jestThisTurn(battle_, actor, kJestLineCount, line)) {
        jestLine_ = kJestLines[static_cast<std::size_t>(line)];
        jestTimer_ =
            2.0f * settings::messageDurationScale(context_.settings.values.messageSpeed);
    }
    accumulateStats(hpBefore, actor, false);
    stageNumbers(hpBefore, damageSfx, statusAction);
    afterAction();
}

void BattleState::afterAction() {
    for (const battle::Combatant& c : battle_.units) {
        if (c.side == battle::Side::Party && c.hp <= 0) {
            koOccurred_ = true;
        }
    }
    phase_ = Phase::Resolve;
    // Stage the presentation of the already-final result: windup and impact
    // only when something was actually hit/healed, then the speed-scaled
    // message pause. Confirm always skips the whole sequence.
    lungeUnit_ = currentActor();
    render::BattleStageParams params;
    params.speed = context_.settings.values.battleSpeed;
    params.flash = context_.settings.values.effectFlash;
    params.shake = context_.settings.values.effectShake;
    seq_.start(!pendingFloats_.empty(), settings::resolveSeconds(params.speed), params);
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
            // Defeat consequences are stated, not silent (UI-INFO-014) — and
            // stated TRUTHFULLY (M43): a castle challenge costs no gold and has
            // no run to forfeit, so it must not borrow the dungeon's penalty.
            return castleChallenge_
                       ? "The party has fallen... The castle guard carries you back "
                         "to the gates. The challenge ends; nothing is lost."
                       : "The party has fallen... You are carried back to town; "
                         "half your gold is lost and the run is forfeit.";
        case battle::Outcome::Escaped:
            return "Escaped!";
        case battle::Outcome::Ongoing:
            return "";
    }
    return "";
}

void BattleState::openDetails() {
    // The focused unit: the highlighted target while aiming, else the actor.
    int unit = currentActor();
    if (phase_ == Phase::ChooseTarget && !targetCandidates_.empty()) {
        unit = targetCandidates_[static_cast<std::size_t>(targetCursor_)];
    }
    const battle::Combatant& c = battle_.units[static_cast<std::size_t>(unit)];
    std::string body = c.name + " - HP " + std::to_string(c.hp) + "/" +
                       std::to_string(c.maxHp);
    if (c.side == battle::Side::Party) {
        body += "  MP " + std::to_string(c.mp) + "/" + std::to_string(c.maxMp);
    }
    body += "\nATK " + std::to_string(c.stats.attack) + "  MAG " +
            std::to_string(c.stats.magic) + "  DEF " + std::to_string(c.stats.defense) +
            "  SPD " + std::to_string(c.stats.speed);
    if (c.guarding) {
        body += "\nGuarding: incoming damage is halved this round.";
    }
    std::string statusList;
    for (const battle::StatusInstance& s : c.statuses) {
        if (battle::isImmuneTo(c, s.type)) {
            continue;  // M40: immune statuses are not shown
        }
        statusList += " " + std::string(statusShort(s.type)) + "(" + std::to_string(s.turns) +
                      " turns)";
    }
    if (statusList.empty()) {
        body += "\nNo active statuses.";
    } else {
        body += "\nStatuses:" + statusList;
    }
    // Passive skills (M36): name and, for a single passive, its description.
    for (const std::string& pid : c.passiveIds) {
        const content::PassiveDef* p = context_.content.findPassive(pid);
        if (p != nullptr) {
            body += "\nPassive: " + p->name + " - " + p->description;
        }
    }
    body +=
        "\n\nPSN: poison, damage at the start of each turn. ATK+/ATK-: attack "
        "raised/lowered. DEF+/DEF-: defense raised/lowered. CNF: confused, "
        "attacks its own side. SIL: silenced, cannot use MP skills. BLD: blinded, "
        "physical attacks usually miss.\nTurn order follows Speed. Guard halves "
        "damage. Escape forfeits the guarded reward - and every battle turn "
        "counts against your score.";
    stack().pushState(
        std::make_unique<DetailsOverlayState>(stack(), context_, "Battle Details", body));
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

    // Contextual Details (M22): explain the focused unit and the status
    // shorthand whenever the player is choosing, never mid-resolve.
    if (phase_ != Phase::Resolve && phase_ != Phase::Done &&
        input.pressed(InputAction::Details)) {
        openDetails();
        return;
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
                seq_.skip();  // skip windup/impact/pause; the result still shows
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
    if (jestTimer_ > 0.0f) {  // M45: the Jester's quip fades on its own
        jestTimer_ -= dt;
        if (jestTimer_ <= 0.0f) {
            jestLine_.clear();
        }
    }
    // Floating numbers hold still during the impact beat (a brief hit-stop)
    // and rise otherwise.
    if (seq_.stage() != render::BattleStage::Impact) {
        for (FloatNumber& f : floats_) {
            f.y -= 22.0f * dt;
            f.timer -= dt;
        }
        floats_.erase(std::remove_if(floats_.begin(), floats_.end(),
                                     [](const FloatNumber& f) { return f.timer <= 0.0f; }),
                      floats_.end());
    }
    // Fallen enemies sink away once their shown HP reaches zero; party
    // members stay visible (they can be revived).
    for (std::size_t i = 0; i < battle_.units.size(); ++i) {
        if (battle_.units[i].side == battle::Side::Enemy && displayHp_[i] <= 0 &&
            koFade_[i] > 0.0f) {
            koFade_[i] = std::max(0.0f, koFade_[i] - dt / 0.4f);
        }
    }

    if (phase_ != Phase::Resolve) {
        return;
    }
    seq_.update(dt);
    if (seq_.takeCommit()) {
        commitPresentation();
    }
    if (!seq_.finished()) {
        return;
    }
    lungeUnit_ = -1;
    const battle::Outcome o = battle_.outcome();
    if (o != battle::Outcome::Ongoing) {
        result_ = o;
        phase_ = Phase::Done;
        message_ = outcomeMessage();
        // One-shot jingle on the music channel (M21); if its file is missing
        // the AudioManager falls back to the matching stinger SFX.
        context_.audio.setMusic(o == battle::Outcome::Victory ? MusicTrack::Victory
                                                             : MusicTrack::Defeat);
    } else {
        advanceTurn();
    }
}

void BattleState::drawUnit(const battle::Combatant& c, int index, int x, int y, bool current,
                           bool targeted) const {
    // Everything drawn here shows the *displayed* HP state, which commits at
    // the sequencer's impact beat — the sim result itself is already final.
    const int shownHp = displayHp_[static_cast<std::size_t>(index)];
    const bool shownAlive = shownHp > 0;
    const float fade = koFade_[static_cast<std::size_t>(index)];
    if (c.side == battle::Side::Enemy && fade <= 0.0f) {
        return;  // fully sunk after its KO was shown
    }
    if (index == lungeUnit_) {
        // Acting unit leans toward the foe during windup/impact.
        x += static_cast<int>(seq_.lunge() * 4.0f) * (c.side == battle::Side::Party ? -1 : 1);
    }
    const float flash =
        hitFlags_[static_cast<std::size_t>(index)] != 0 ? seq_.flashStrength() : 0.0f;

    // Sprite lookup: a specific id first (per-enemy art is a manifest
    // drop-in), then the tier-generic sprite, then the pre-asset rectangle.
    std::string spriteId;
    if (c.side == battle::Side::Party) {
        spriteId = "actor." + c.sourceId + ".battle";
    } else if (c.isBoss) {
        spriteId = "boss." + c.sourceId + ".battle";
        if (!context_.resources.hasTexture(spriteId)) {
            spriteId = "boss.generic.battle";
        }
    } else {
        spriteId = "enemy." + c.sourceId + ".battle";
        if (!context_.resources.hasTexture(spriteId)) {
            const content::EnemyDef* def = context_.content.findEnemy(c.sourceId);
            spriteId = (def != nullptr && def->tier == content::EnemyTier::Elite)
                           ? "enemy.elite.battle"
                           : "enemy.normal.battle";
        }
    }

    const style::Palette& p = style::palette();
    if (context_.resources.hasTexture(spriteId)) {
        const Texture2D& tex = context_.resources.texture(spriteId);
        const int sx = x + 20 - tex.width / 2;   // bottom-center on the old
        const int sy = y + 16 - tex.height;      // 40x16 footprint
        Color tint = shownAlive ? WHITE : Color{110, 110, 125, 255};
        tint.a = static_cast<unsigned char>(255.0f * fade);
        DrawTexture(tex, sx, sy, tint);
        if (flash > 0.0f) {
            DrawRectangle(sx, sy, tex.width, tex.height, Fade(WHITE, 0.55f * flash));
        }
        if (targeted) {
            // Corner brackets (M46): shape-first focus that survives grayscale,
            // hit flashes, and any sprite behind it.
            ui::drawFocusBrackets(sx - 1, sy - 1, tex.width + 2, tex.height + 2, p.cursor);
        }
    } else {
        const Color body = c.side == battle::Side::Enemy ? Color{170, 80, 90, 255}
                                                         : Color{90, 130, 190, 255};
        DrawRectangle(x, y, 40, 16, Fade(shownAlive ? body : Color{60, 60, 70, 255}, fade));
        if (flash > 0.0f) {
            DrawRectangle(x, y, 40, 16, Fade(WHITE, 0.55f * flash));
        }
        if (targeted) {
            ui::drawFocusBrackets(x - 1, y - 1, 42, 18, p.cursor);
        }
    }
    if (current) {
        // Acting unit: gold chevron with the sanctioned one-pixel nudge.
        ui::drawChevron(x - 10, y + 4, p.cursor, ui::motionPhase());
    }

    // Names are no longer painted over every sprite (M25 slice 2); a unit's
    // name and judgment stats appear on the target-info panel while it is being
    // targeted, and via Details.
    // Framed HP meter (displayed value); hidden once a sinking KO begins.
    if (c.side == battle::Side::Party || shownAlive) {
        ui::drawMeter(x, y + 17, 40, 5, shownHp < 0 ? 0 : shownHp, c.maxHp, p.hpFill);
    }
    if (c.side == battle::Side::Party) {
        ui::drawMeter(x, y + 23, 40, 4, c.mp < 0 ? 0 : c.mp, c.maxMp, p.mpFill);
        // HP and MP as numerals (M25 slice 3): MP is the resource that gates
        // skills, so its exact value gets the same clarity as HP.
        ui::drawText(TextFormat("HP %d/%d", shownHp < 0 ? 0 : shownHp, c.maxHp), x + 44, y + 4, 8,
                     p.text);
        ui::drawText(TextFormat("MP %d/%d", c.mp < 0 ? 0 : c.mp, c.maxMp), x + 44, y + 13, 8,
                     p.mpFill);
    }
    if (!shownAlive) {
        ui::drawText("KO", x + 13, y + 4, 8, Fade(p.dangerText, fade));
    }

    if (!c.statuses.empty() && shownAlive) {
        // Both sides read their status tags in the open field to the right of the
        // sprite: party rows below their HP/MP numerals, enemy rows level with the
        // sprite. Nothing is painted over a neighbouring row's graphics.
        const bool party = c.side == battle::Side::Party;
        const int sx = x + 44;
        const int sy = y + (party ? 22 : 4);
        const int maxWidth = party ? context_.virtualWidth - sx - 2 : 120;
        const std::vector<std::string> lines = statusLines(c, maxWidth, 8, 2);
        for (std::size_t i = 0; i < lines.size(); ++i) {
            ui::drawTextFitted(lines[i], sx, sy + static_cast<int>(i) * 8, maxWidth, 8,
                               ui::lighten(p.magic, 32),
                               party ? "battle.status.party" : "battle.status.enemy");
        }
    }
}

void BattleState::render() {
    const int w = context_.virtualWidth;
    const int h = context_.virtualHeight;
    const style::Palette& pal = style::palette();
    ClearBackground(pal.canvas);

    // Stage dressing (M46): one broad value band behind the combatants with
    // ink keylines, side grounding brackets per side, and a violet accent
    // pair when a boss is on the field. No texture, no noise behind units.
    {
        const int bandY = 24;
        const int bandH = h - kPanelH - 34 - bandY;
        DrawRectangle(0, bandY, w, bandH, pal.panel);
        DrawRectangle(0, bandY, w, 1, pal.ink);
        DrawRectangle(0, bandY + bandH - 1, w, 1, pal.ink);
        DrawRectangle(0, bandY + 1, w, 1, pal.borderDark);
        const int gy = bandY + bandH - 8;
        DrawRectangle(6, gy, 10, 2, pal.borderDark);       // enemy-side bracket
        DrawRectangle(6, gy - 6, 2, 8, pal.borderDark);
        DrawRectangle(w - 16, gy, 10, 2, pal.borderDark);  // party-side bracket
        DrawRectangle(w - 8, gy - 6, 2, 8, pal.borderDark);
        bool bossOnField = false;
        for (const battle::Combatant& c : battle_.units) {
            bossOnField = bossOnField || c.isBoss;
        }
        if (bossOnField) {
            const auto pip = [&pal](int x, int y) {
                DrawRectangle(x, y + 1, 3, 1, pal.magic);
                DrawRectangle(x + 1, y, 1, 3, pal.magic);
            };
            pip(6, bandY + 4);
            pip(12, bandY + 4);
        }
    }

    const int actor = currentActor();
    const bool partyTurn = phase_ == Phase::Command || phase_ == Phase::ChooseTarget ||
                           phase_ == Phase::ChooseSkill || phase_ == Phase::ChooseItem;

    int enemyRow = 0;
    int partyRow = 0;
    int targetUnit = -1;
    if (phase_ == Phase::ChooseTarget && !targetCandidates_.empty()) {
        targetUnit = targetCandidates_[static_cast<std::size_t>(targetCursor_)];
    }
    // Screen shake (impact beat only; honors the shake setting) offsets the
    // battlefield, never the UI panel.
    const int shakeX = seq_.shakeOffset();
    const int enemyY0 = enemyBaseY();
    for (std::size_t i = 0; i < battle_.units.size(); ++i) {
        const battle::Combatant& c = battle_.units[i];
        const bool isCurrent = partyTurn && static_cast<int>(i) == actor;
        const bool isTarget = static_cast<int>(i) == targetUnit;
        if (c.side == battle::Side::Enemy) {
            drawUnit(c, static_cast<int>(i), 36 + shakeX, enemyY0 + enemyRow * 34, isCurrent,
                     isTarget);
            ++enemyRow;
        } else {
            // The party column leaves room to its right for the HP/MP numerals
            // *and* the status tags underneath them.
            drawUnit(c, static_cast<int>(i), w - 124 + shakeX, 36 + partyRow * 34, isCurrent,
                     isTarget);
            ++partyRow;
        }
    }

    // Turn counter as a compact badge top-right: the top-left is needed by
    // tall enemy columns; the badge stays quieter than the acting unit.
    ui::drawChipRight(TextFormat("Turns %d", battle_.turnsTaken), w - 4, 4, pal.gold);

    // M45: the Jester's quip, mid-screen above the panel — decorative, dismissed
    // by its own timer, and fitted so a long line can never spill.
    if (!jestLine_.empty()) {
        ui::drawTextFitted(jestLine_, 20, h - kPanelH - 22, w - 40, style::kFontBody,
                           pal.gold, "battle.jest");
    }

    // Floating damage / heal numbers.
    for (const FloatNumber& f : floats_) {
        ui::drawText(f.text.c_str(), static_cast<int>(f.x) + shakeX, static_cast<int>(f.y), 10,
                 f.heal ? pal.success : pal.dangerText);
    }

    // Bottom panel: lists live in the left column (scrolled to kListRows),
    // the actor line and selected-entry description in the right column. A
    // boss introduction gets the Danger frame with its warning tab.
    const int panelY = h - kPanelH - 4;
    const bool bossIntro = phase_ == Phase::Intro && !bossTelegraph_.empty();
    ui::drawFrame(4, panelY, w - 8, kPanelH,
                  bossIntro ? ui::FrameStyle::Danger : ui::FrameStyle::Standard);
    const bool listPhase = phase_ == Phase::Command || phase_ == Phase::ChooseSkill ||
                           phase_ == Phase::ChooseItem;
    if (listPhase) {
        // Column divider separates the list from the info column.
        DrawRectangle(kInfoX - 10, panelY + 6, 1, kPanelH - 12, pal.borderDark);
    }
    const int infoW = w - kInfoX - 12;
    const int listLabelW = kInfoX - kListX - 18;
    const InputMap& map = context_.input.map();
    const ActiveDevice device = context_.input.activeDevice();
    const std::string backHint = input::prompt(map, InputAction::Cancel, device, "Back") +
                                 "  " +
                                 input::prompt(map, InputAction::Details, device, "Details");

    switch (phase_) {
        case Phase::Intro:
            // Headline, telegraph, and confirm prompt as three separated
            // layers inside the (boss: Danger) frame.
            ui::drawTextCentered(message_.c_str(), w / 2, panelY + 6, 12, pal.text);
            if (!bossTelegraph_.empty()) {
                ui::drawDivider(20, panelY + 21, w - 40);
                ui::drawTextWrapped(bossTelegraph_, 16, panelY + 25, w - 32, style::kFontBody,
                                    pal.dangerText, "battle.telegraph", 2);
            }
            ui::drawTextCentered(
                input::prompt(map, InputAction::Confirm, device, "Begin").c_str(), w / 2,
                panelY + 48, style::kFontBody, pal.gold);
            break;
        case Phase::Command:
            ui::drawMenu(commandMenu_, kListX, panelY + 5, 11, style::kFontBody, pal.text,
                         pal.disabled, pal.cursor);
            ui::drawTextFitted(
                TextFormat("%s's turn",
                           battle_.units[static_cast<std::size_t>(actor)].name.c_str()),
                kInfoX, panelY + 6, infoW, style::kFontBody, style::palette().text, "battle.actor");
            // Say *why* a grayed command is unavailable.
            if (!commandMenu_.currentEnabled()) {
                const char* why = commandMenu_.cursor() == 1 ? "No skills learned."
                                  : commandMenu_.cursor() == 2 ? "No usable items."
                                                               : "";
                ui::drawTextFitted(why, kInfoX, panelY + 20, infoW, style::kFontBody,
                                   style::palette().textDim, "battle.why");
            }
            break;
        case Phase::ChooseSkill: {
            ui::drawMenuScrolled(skillMenu_, skillScroll_, kListRows, kListX, panelY + 6,
                                 kListItemH, style::kFontBody, listLabelW, pal.text,
                                 pal.disabled, pal.cursor, "battle.skills",
                                 style::kFontSmall, pal.mpFill);
            ui::drawTextFitted("Skill  " + backHint, kInfoX, panelY + 6, infoW, style::kFontBody,
                               style::palette().textDim, "battle.skillhint");
            if (!skillIds_.empty()) {
                const std::string& sid =
                    skillIds_[static_cast<std::size_t>(skillMenu_.cursor())];
                if (const content::SkillDef* s = context_.content.findSkill(sid)) {
                    // The row's "SIL" tag is terse by necessity; spell the block
                    // out here rather than leaving the player to decode it.
                    const battle::Combatant& a =
                        battle_.units[static_cast<std::size_t>(actor)];
                    if (!battle::canCast(a, *s)) {
                        ui::drawTextWrapped("SIL: silenced - MP skills are blocked.", kInfoX,
                                            panelY + 20, infoW, style::kFontBody,
                                            style::palette().textDim, "battle.skillblocked", 2);
                    } else if (!s->description.empty()) {
                        ui::drawTextWrapped(s->description, kInfoX, panelY + 20, infoW,
                                            style::kFontBody, style::palette().success,
                                            "battle.skilldesc", 2);
                    }
                }
            }
            break;
        }
        case Phase::ChooseItem: {
            ui::drawMenuScrolled(itemMenu_, itemScroll_, kListRows, kListX, panelY + 6,
                                 kListItemH, style::kFontBody, listLabelW, pal.text,
                                 pal.disabled, pal.cursor, "battle.items",
                                 style::kFontSmall, pal.gold);
            ui::drawTextFitted("Item  " + backHint, kInfoX, panelY + 6, infoW, style::kFontBody,
                               style::palette().textDim, "battle.itemhint");
            if (!itemIds_.empty()) {
                const std::string& iid = itemIds_[static_cast<std::size_t>(itemMenu_.cursor())];
                if (const content::ItemDef* it = context_.content.findItem(iid)) {
                    // M43: a greyed item says why before it says what it does.
                    const std::string blocked = itemBlockReason(*it);
                    if (!blocked.empty()) {
                        ui::drawTextWrapped(blocked, kInfoX, panelY + 20, infoW, style::kFontBody,
                                            style::palette().textDim, "battle.itemblocked", 2);
                    } else if (!it->description.empty()) {
                        ui::drawTextWrapped(it->description, kInfoX, panelY + 20, infoW,
                                            style::kFontBody, style::palette().success,
                                            "battle.itemdesc", 2);
                    }
                }
            }
            break;
        }
        case Phase::ChooseTarget: {
            // Target-info panel (M25 slice 2): name, vitals, and the stats a
            // player needs to judge a target — shown only for the currently
            // targeted unit, in the dedicated bottom panel so it never collides
            // with the side-specific status lines. M28 depends on this panel.
            if (targetUnit >= 0) {
                const battle::Combatant& t = battle_.units[static_cast<std::size_t>(targetUnit)];
                ui::drawTextFitted("Target: " + t.name, kListX, panelY + 5, w - kListX - 96,
                                   style::kFontHeading, style::palette().cursor,
                                   "battle.target.name");
                ui::drawTextRight(backHint, w - 10, panelY + 7, style::kFontBody,
                                  style::palette().textDim);
                std::string vitals = "HP " + std::to_string(t.hp < 0 ? 0 : t.hp) + "/" +
                                     std::to_string(t.maxHp);
                if (t.side == battle::Side::Party) {
                    vitals += "     MP " + std::to_string(t.mp < 0 ? 0 : t.mp) + "/" +
                              std::to_string(t.maxMp);
                }
                ui::drawTextFitted(vitals, kListX, panelY + 22, w - 2 * kListX, style::kFontBody,
                                   style::palette().text, "battle.target.vitals");
                const std::string stats =
                    "ATK " + std::to_string(t.stats.attack) + "    MAG " +
                    std::to_string(t.stats.magic) + "    DEF " + std::to_string(t.stats.defense) +
                    "    SPD " + std::to_string(t.stats.speed);
                ui::drawTextFitted(stats, kListX, panelY + 34, w - 2 * kListX, style::kFontBody,
                                   style::palette().textDim, "battle.target.stats");
                std::string line;
                for (const battle::StatusInstance& s : t.statuses) {
                    if (battle::isImmuneTo(t, s.type)) {
                        continue;  // M40: immune statuses are not shown
                    }
                    if (line.empty()) {
                        line = "Status:";
                    }
                    line += " " + std::string(statusShort(s.type)) + "(" +
                            std::to_string(s.turns) + ")";
                }
                // A revealed passive (M36): show the foe/ally's passive by name.
                if (!t.passiveIds.empty()) {
                    if (!line.empty()) {
                        line += "   ";
                    }
                    line += "Passive:";
                    for (const std::string& pid : t.passiveIds) {
                        const content::PassiveDef* p = context_.content.findPassive(pid);
                        line += " " + std::string(p != nullptr ? p->name : pid);
                    }
                }
                if (!line.empty()) {
                    ui::drawTextFitted(line, kListX, panelY + 47, w - 2 * kListX, style::kFontSmall,
                                       style::palette().textDim, "battle.target.status");
                }
            } else {
                ui::drawTextCentered(("Choose a target   " + backHint).c_str(), w / 2,
                                     panelY + 24, style::kFontBody, style::palette().text);
            }
            break;
        }
        case Phase::Resolve:
            ui::drawTextWrapped(message_, 16, panelY + 14, w - 32, style::kFontBody,
                                style::palette().text, "battle.message", 3);
            break;
        case Phase::Done:
            ui::drawTextWrapped(message_, 16, panelY + 10, w - 32, 12,
                                pal.gold, "battle.outcome", 2);
            ui::drawTextCentered(
                input::prompt(map, InputAction::Confirm, device, "Continue").c_str(), w / 2,
                panelY + 44, style::kFontBody, pal.gold);
            break;
    }
}

}  // namespace cd
