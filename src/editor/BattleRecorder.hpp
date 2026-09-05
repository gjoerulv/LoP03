#pragma once

#include <map>
#include <string>
#include <vector>

#include "battle/Battle.hpp"
#include "battle/BattleObserver.hpp"

// M60 — the sim lab's telemetry consumer: tallies BattleEvents into
// per-action and per-combatant totals. Attribution rules: damage recorded
// while an action is in flight belongs to that action's actor UNLESS it lands
// on the actor itself (thorns/counter retaliation) — that stays unattributed
// (`retaliation`), as do poison ticks. Amounts are effective (post-clamp), so
// every unit's HP delta reconciles exactly:
//   finalHp - initialHp == healedReceived + reviveHp - damageTaken
// (pinned by tests/test_battle_observer.cpp). Header-only, pure.

namespace cd::editor {

struct ActionTally {
    int uses = 0;
    long damage = 0;   // dealt by this action's user while it resolved
    long healing = 0;  // healing attributed the same way
};

struct UnitTally {
    std::string name;
    bool party = false;
    long dealt = 0;           // damage attributed to this unit's actions
    long taken = 0;           // all damage received (incl. poison/retaliation)
    long healingGiven = 0;    // heals this unit caused (incl. lifedrink on self)
    long healingReceived = 0; // heals + revive HP received
    int kos = 0;              // times this unit fell
    int revives = 0;          // times this unit rose
};

class BattleRecorder : public battle::BattleObserver {
public:
    // Captures names/sides; call once on the battle the recorder will watch
    // (before simulating).
    void bind(const battle::Battle& battle) {
        units_.clear();
        for (const battle::Combatant& u : battle.units) {
            UnitTally tally;
            tally.name = u.name;
            tally.party = u.side == battle::Side::Party;
            units_.push_back(std::move(tally));
        }
    }

    void onEvent(const battle::BattleEvent& e) override {
        switch (e.type) {
            case battle::BattleEvent::Type::Action: {
                currentActor_ = e.actor;
                currentAction_ = e.item ? "item:" + e.id : (e.id.empty() ? "(attack)" : e.id);
                ++actions_[currentAction_].uses;
                break;
            }
            case battle::BattleEvent::Type::Damage: {
                if (valid(e.target)) {
                    units_[static_cast<std::size_t>(e.target)].taken += e.amount;
                }
                // Poison and self-directed damage (thorns/counter retaliation
                // against the current actor) stay unattributed.
                if (!e.poison && currentActor_ >= 0 && e.target != currentActor_ &&
                    valid(currentActor_)) {
                    units_[static_cast<std::size_t>(currentActor_)].dealt += e.amount;
                    actions_[currentAction_].damage += e.amount;
                }
                break;
            }
            case battle::BattleEvent::Type::Heal: {
                if (valid(e.target)) {
                    units_[static_cast<std::size_t>(e.target)].healingReceived += e.amount;
                }
                if (valid(e.actor)) {
                    units_[static_cast<std::size_t>(e.actor)].healingGiven += e.amount;
                }
                if (e.actor == currentActor_ && currentActor_ >= 0) {
                    actions_[currentAction_].healing += e.amount;
                }
                break;
            }
            case battle::BattleEvent::Type::KO: {
                if (valid(e.target)) {
                    ++units_[static_cast<std::size_t>(e.target)].kos;
                }
                break;
            }
            case battle::BattleEvent::Type::Revive: {
                if (valid(e.target)) {
                    UnitTally& tally = units_[static_cast<std::size_t>(e.target)];
                    ++tally.revives;
                    tally.healingReceived += e.amount;
                }
                break;
            }
            case battle::BattleEvent::Type::Guard:
            case battle::BattleEvent::Type::StatusApplied:
                break;  // M109 telemetry events; the sim lab tallies neither
        }
        ++eventCount_;
    }

    const std::vector<UnitTally>& units() const { return units_; }
    const std::map<std::string, ActionTally>& actions() const { return actions_; }
    long eventCount() const { return eventCount_; }

    // Merges another recorder's totals (seed sweeps aggregate one per run).
    void mergeFrom(const BattleRecorder& other) {
        if (units_.size() < other.units_.size()) {
            units_.resize(other.units_.size());
        }
        for (std::size_t i = 0; i < other.units_.size(); ++i) {
            UnitTally& mine = units_[i];
            const UnitTally& theirs = other.units_[i];
            if (mine.name.empty()) {
                mine.name = theirs.name;
                mine.party = theirs.party;
            }
            mine.dealt += theirs.dealt;
            mine.taken += theirs.taken;
            mine.healingGiven += theirs.healingGiven;
            mine.healingReceived += theirs.healingReceived;
            mine.kos += theirs.kos;
            mine.revives += theirs.revives;
        }
        for (const auto& [id, tally] : other.actions_) {
            ActionTally& mine = actions_[id];
            mine.uses += tally.uses;
            mine.damage += tally.damage;
            mine.healing += tally.healing;
        }
        eventCount_ += other.eventCount_;
    }

private:
    bool valid(int index) const {
        return index >= 0 && index < static_cast<int>(units_.size());
    }

    std::vector<UnitTally> units_;
    std::map<std::string, ActionTally> actions_;
    int currentActor_ = -1;
    std::string currentAction_;
    long eventCount_ = 0;
};

}  // namespace cd::editor
