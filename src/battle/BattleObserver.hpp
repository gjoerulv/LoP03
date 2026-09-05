#pragma once

#include <string>

#include "content/Enums.hpp"  // M109: StatusApplied carries the status type

// M60 (CrystalForge sim lab): a RECORD-ONLY telemetry hook for battle
// resolution. The battle model emits an event at each chokepoint where the
// fact is known (an action starts, effective damage/healing lands, a unit
// falls or rises); an observer may tally them, never steer them. The contract
// that keeps determinism untouched:
//
//   - `Battle::observer` is a non-owning pointer, default null. The game and
//     the Simulator never set it; only telemetry consumers (the editor's sim
//     lab, tests) do.
//   - Every emit site is `if (observer) observer->onEvent(...)` — no other
//     branching, no rolls, no state reads that could differ by observer.
//     `rollCursor` and every outcome are byte-identical with and without a
//     recorder attached (pinned by tests/test_battle_observer.cpp), so there
//     is NO kBattleRulesVersion bump.
//   - Amounts are EFFECTIVE (HP actually lost/gained after clamps), so a
//     recorder's totals reconcile exactly against the units' HP deltas.

namespace cd::battle {

struct BattleEvent {
    enum class Type {
        Action,  // a unit acts: id = skill id, "" = basic attack; item = true for items
        Damage,  // `target` lost `amount` HP (poison = a status tick, otherwise a hit)
        Heal,    // `target` regained `amount` HP
        KO,      // `target` just fell
        Revive,  // `target` just rose with `amount` HP
        // M109 (additive, the same if-observer discipline): a unit guards, or a
        // status actually LANDS on a unit (an immune or no-op application never
        // emits). Damage/KO events now also carry the ATTACKER in `actor`
        // where one exists (a deliberate hit, the thorns bearer, the
        // counter-attacker); poison ticks stay -1.
        Guard,          // actor = target = the guarding unit
        StatusApplied,  // actor = the applier (-1 if none), target = the bearer
    };

    Type type = Type::Action;
    int actor = -1;   // acting/causing unit index; -1 = not attributable here
    int target = -1;  // affected unit index
    int amount = 0;   // effective HP delta (Damage/Heal/Revive)
    bool poison = false;
    bool item = false;
    std::string id;  // Action: skill/item id
    // M109 (StatusApplied only). Trailing with defaults, so every positional
    // emit written before them keeps compiling unchanged.
    content::StatusType status = content::StatusType::None;
    int duration = 0;  // authored turns (before the M35 duration scaling)
};

struct BattleObserver {
    virtual ~BattleObserver() = default;
    virtual void onEvent(const BattleEvent& event) = 0;
};

}  // namespace cd::battle
