#pragma once

#include <cstddef>
#include <string>

#include "battle/Battle.hpp"
#include "battle/BattleObserver.hpp"
#include "content/ContentDatabase.hpp"
#include "content/Definitions.hpp"
#include "game/Lifetime.hpp"

// M109: the lifetime ledger's battle seam. The pure battle model already emits
// record-only telemetry events (the M60 BattleObserver hook); this consumer
// turns them into the save's cumulative statistics. It is attached ONLY by the
// battle screen, and only for a real fight - the sparring mirror, the
// Simulator, the editor's sim lab, capture scenes and tests never attach it,
// so nothing zero-stakes can ever leak into a player's numbers. Pure and
// raylib-free so the seam is unit-tested against a real Battle.

namespace cd {

// Where a fight's facts go. `stats` null = record nothing (a zero-stakes
// battle); `town` 0 = no per-town attribution (castle / Goose Town fights
// count in the combat totals only). Passed by value through the battle
// launch payloads (BossIntroState -> BattleState).
struct LifetimeHook {
    LifetimeStats* stats = nullptr;
    int town = 0;
};

class BattleTelemetry final : public battle::BattleObserver {
public:
    BattleTelemetry(LifetimeStats& stats, const battle::Battle& battle,
                    const content::ContentDatabase& db)
        : stats_(stats), battle_(battle), db_(db) {}

    void onEvent(const battle::BattleEvent& e) override {
        using Type = battle::BattleEvent::Type;
        switch (e.type) {
            case Type::Action: {
                const int slot = slotOf(e.actor);
                if (slot < 0 || e.item || e.id.empty()) {
                    break;  // basic attacks and items are not "skills cast"
                }
                ++stats_.members[static_cast<std::size_t>(slot)].skillsCast;
                if (const content::SkillDef* skill = db_.findSkill(e.id);
                    skill != nullptr && skill->oncePerRun) {
                    ++stats_.combat.summonsUsed;  // M95: the legends answer once a run
                }
                break;
            }
            case Type::Damage: {
                // Dealt: a deliberate (non-poison) hit by a party member ON A
                // FOE - striking a confused ally is nobody's feat.
                if (!e.poison && isEnemy(e.target)) {
                    const int slot = slotOf(e.actor);
                    if (slot >= 0) {
                        recordLifetimeHit(stats_, slot, e.amount);
                    }
                }
                // Taken: every HP a party member loses, poison ticks included.
                if (const int slot = slotOf(e.target); slot >= 0) {
                    stats_.members[static_cast<std::size_t>(slot)].damageTaken += e.amount;
                    stats_.combat.damageTaken += e.amount;
                }
                break;
            }
            case Type::Heal: {
                if (const int slot = slotOf(e.target); slot >= 0) {
                    stats_.members[static_cast<std::size_t>(slot)].healingReceived += e.amount;
                }
                if (const int slot = slotOf(e.actor); slot >= 0) {
                    stats_.members[static_cast<std::size_t>(slot)].healingDone += e.amount;
                    stats_.combat.healing += e.amount;
                }
                break;
            }
            case Type::KO: {
                if (isEnemy(e.target)) {
                    ++stats_.combat.enemiesKo;
                    if (const int slot = slotOf(e.actor); slot >= 0) {
                        ++stats_.members[static_cast<std::size_t>(slot)].finishingBlows;
                    }
                } else if (const int slot = slotOf(e.target); slot >= 0) {
                    ++stats_.members[static_cast<std::size_t>(slot)].timesKo;
                }
                break;
            }
            case Type::Revive: {
                // A party member's revive (item or skill). A boss raising its
                // own court, or its clone, is the foe's business.
                if (const int slot = slotOf(e.actor); slot >= 0) {
                    ++stats_.members[static_cast<std::size_t>(slot)].revivesPerformed;
                    ++stats_.combat.revives;
                }
                break;
            }
            case Type::Guard: {
                if (slotOf(e.actor) >= 0) {
                    ++stats_.combat.guardUses;
                }
                break;
            }
            case Type::StatusApplied: {
                const int slot = slotOf(e.actor);
                if (slot < 0) {
                    break;
                }
                ++stats_.combat.statusesApplied;  // anything a member landed, buffs included
                if (isEnemy(e.target)) {
                    ++stats_.members[static_cast<std::size_t>(slot)].statusesInflicted;
                }
                break;
            }
        }
    }

private:
    // The party slot (0..3) behind a party-side unit index, or -1.
    int slotOf(int unit) const {
        if (unit < 0 || unit >= static_cast<int>(battle_.units.size())) {
            return -1;
        }
        const battle::Combatant& c = battle_.units[static_cast<std::size_t>(unit)];
        if (c.side != battle::Side::Party || !validLifetimeSlot(c.partyIndex)) {
            return -1;
        }
        return c.partyIndex;
    }
    bool isEnemy(int unit) const {
        return unit >= 0 && unit < static_cast<int>(battle_.units.size()) &&
               battle_.units[static_cast<std::size_t>(unit)].side == battle::Side::Enemy;
    }

    LifetimeStats& stats_;
    const battle::Battle& battle_;
    const content::ContentDatabase& db_;
};

// The battle's end, recorded ONCE by the battle screen (the single place every
// real fight ends). Outcome tallies, the turn total, the boss-encounter count
// (a boss unit on the field of a won battle), and the defeat ledger: every
// dead enemy unit that is not a prebuilt summon slot counts its content id
// once - a boss once per encounter won, a minion once even if a revive clock
// made it fall twice, a raised clone never (the Dragon's clone shares its
// id; the encounter is the Dragon's one defeat).
inline void recordBattleEnd(LifetimeStats& stats, const battle::Battle& battle,
                            battle::Outcome outcome, int rounds, int town) {
    stats.combat.battleTurns += rounds > 0 ? rounds : 0;
    switch (outcome) {
        case battle::Outcome::Victory: {
            ++stats.combat.battlesWon;
            if (town > 0) {
                ++lifetimeTown(stats, town).battlesWon;
            }
            bool boss = false;
            for (const battle::Combatant& c : battle.units) {
                if (c.side != battle::Side::Enemy) {
                    continue;
                }
                if (c.isBoss) {
                    boss = true;
                }
                if (!c.alive() && !c.summonSlot && !c.sourceId.empty()) {
                    ++stats.defeats[c.sourceId];
                }
            }
            if (boss) {
                ++stats.combat.bossesDefeated;
                if (town > 0) {
                    ++lifetimeTown(stats, town).bossesDefeated;
                }
            }
            break;
        }
        case battle::Outcome::Defeat:
            ++stats.combat.battlesLost;
            break;
        case battle::Outcome::Escaped:
            ++stats.combat.playerEscapes;
            break;
        case battle::Outcome::EnemyFled:  // M111: the foe's getaway — turns only
        case battle::Outcome::Ongoing:
            break;
    }
}

}  // namespace cd
