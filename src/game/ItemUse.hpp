#pragma once

#include <string>

#include "content/Definitions.hpp"
#include "content/Enums.hpp"
#include "game/Character.hpp"

// M90: out-of-battle consumable use (the pause menus' Items screen), pure and
// raylib-free. The M43 battle gating carries over: an item with nobody to use
// it on is refused WITH THE REASON, so nothing is ever spent on "No effect".
// Statuses are battle-scoped (a Character carries none), so cures and other
// status-rider items have no out-of-battle target and are refused outright —
// their moment is in combat.

namespace cd {

// M131: the Alarm (`ConsumableEffect::Alarm`) targets no member - it acts on
// the dungeon around the party. The Items screen rings it itself when the
// party stands in a dungeon (the run's next patrol answers at once, by the
// seeded rules) and refuses it in town with kAlarmTownRefusal; the bag in
// battle never lists it. `itemUseRefusal` therefore never accepts it for a
// member, whatever the member's state.
inline bool itemIsAlarm(const content::ItemDef& item) {
    return item.type == content::ItemType::Consumable &&
           item.effect == content::ConsumableEffect::Alarm;
}
inline constexpr const char* kAlarmTownRefusal =
    "The Alarm is for a dungeon - nothing here answers.";
inline constexpr const char* kAlarmMemberRefusal = "The Alarm is rung, not taken.";

// Empty = usable on this member; otherwise the reason to grey the row.
inline std::string itemUseRefusal(const Character& target, const content::ItemDef& item) {
    if (item.type != content::ItemType::Consumable) {
        return "Not usable from the bag.";
    }
    switch (item.effect) {
        case content::ConsumableEffect::Heal:
            if (target.hp <= 0) {
                return "The fallen need a revive, not a potion.";
            }
            // M120: a heal with an MP rider (the Elixir) is still worth using
            // on a member whose HP is full but whose MP is not.
            if (target.hp >= target.maxHp) {
                if (item.mpAmount <= 0) {
                    return "Already at full HP.";
                }
                if (target.mp >= target.maxMp) {
                    return "Already at full HP and MP.";
                }
            }
            return "";
        case content::ConsumableEffect::RestoreMp:
            if (target.hp <= 0) {
                return "The fallen need a revive first.";
            }
            if (target.mp >= target.maxMp) {
                return "Already at full MP.";
            }
            return "";
        case content::ConsumableEffect::Revive:
            if (target.hp > 0) {
                return "Still standing - a revive reaches only the fallen.";
            }
            return "";
        case content::ConsumableEffect::Cure:
            return "Nothing to cure outside battle.";
        case content::ConsumableEffect::Alarm:
            return kAlarmMemberRefusal;  // M131: rung by the Items screen, never taken
        case content::ConsumableEffect::None:
            break;
    }
    return "Its moment is in battle.";
}

// Applies an accepted item (call only when itemUseRefusal returned empty) and
// returns the log line. Caps mirror battle behavior; a revive raises at the
// item's authored percentage of max HP (the M43 Phoenix Tear rule).
inline std::string applyItemUse(Character& target, const content::ItemDef& item) {
    switch (item.effect) {
        case content::ConsumableEffect::Heal: {
            const int before = target.hp;
            target.hp = target.hp + item.effectAmount > target.maxHp ? target.maxHp
                                                                     : target.hp + item.effectAmount;
            std::string line =
                target.name + " recovers " + std::to_string(target.hp - before) + " HP";
            if (item.mpAmount > 0) {  // M120: the Elixir's MP rider
                const int mpBefore = target.mp;
                target.mp = target.mp + item.mpAmount > target.maxMp ? target.maxMp
                                                                     : target.mp + item.mpAmount;
                line += " and " + std::to_string(target.mp - mpBefore) + " MP";
            }
            return line + ".";
        }
        case content::ConsumableEffect::RestoreMp: {
            const int before = target.mp;
            target.mp = target.mp + item.effectAmount > target.maxMp ? target.maxMp
                                                                     : target.mp + item.effectAmount;
            return target.name + " recovers " + std::to_string(target.mp - before) + " MP.";
        }
        case content::ConsumableEffect::Revive: {
            const int hp = target.maxHp * item.effectAmount / 100;
            target.hp = hp < 1 ? 1 : hp;
            return target.name + " rises again!";
        }
        case content::ConsumableEffect::Cure:
        case content::ConsumableEffect::Alarm:
        case content::ConsumableEffect::None:
            break;
    }
    return "";
}

}  // namespace cd
