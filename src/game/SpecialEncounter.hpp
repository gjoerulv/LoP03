#pragma once

// M112: the two decision encounters the patrol dispatcher can answer with —
// the Jester's LORE question (two answers and the Jester itself on the field;
// the right answer pays, the wrong one mocks, striking the Jester or sweeping
// the field costs the striker) and the TREASURE CHESTS (three chests, one
// paying, one lying, one empty; the lying one is the Mimic, a boss). Both
// are pure models: seeded by the run seed and the patrol index (SKILL gotcha
// 10 — hashes, never rng-stream draws), hosted by BattleState's decision
// mode, and resolved by one pure function the tests drive directly.
//
// The battle model never learns of any of this: the placeholders are real
// enemy-side Combatants at 1 HP (Battle::outcome() would otherwise declare
// victory over an empty side), inert only in the screen's bookkeeping; the
// one shared AOE definition is Battle::hostileTargetCount.

#include <algorithm>
#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "battle/Battle.hpp"
#include "content/ContentDatabase.hpp"
#include "content/Definitions.hpp"
#include "dungeon/DungeonModel.hpp"
#include "game/BlackMarket.hpp"
#include "game/Spoils.hpp"

namespace cd {

enum class SpecialKind { None, Lore, Chests };
enum class ChestRole { Reward, Mimic, Empty };
enum class SpecialResult {
    Unresolved,
    LoreCorrect,     // +kLoreReward gold, the victory fanfare
    LoreWrong,       // the mock line and the mocking jingle
    JesterPunished,  // the striker is knocked out — for real
    AoePunished,     // a sweep of the field: the same, for real
    ChestGold,       // +kChestGoldReward gold
    ChestGear,       // one town-appropriate piece of equipment
    ChestEmpty,      // the mock line and the mocking jingle
    MimicRevealed,   // the lying chest is a boss; the fight is on
};

inline constexpr int kSpecialPlaceholders = 3;
inline constexpr int kLoreReward = 500;
inline constexpr int kChestGoldReward = 500;
inline constexpr int kMimicBounty = 500;
inline constexpr const char* kMimicBossId = "mimic";
inline constexpr const char* kJesterSourceId = "jester";  // the reward class's own sprite

// Fresh salts per use (never the M93 team salt, never the dispatcher's).
inline constexpr std::uint64_t kSaltLoreShuffle = 0x10AE5C0FFEE00001ull;
inline constexpr std::uint64_t kSaltLoreAnswer = 0x10AE0A0B0C0D0E0Full;
inline constexpr std::uint64_t kSaltChestRoles = 0xC4E5700000000A11ull;
inline constexpr std::uint64_t kSaltChestReward = 0xC4E57000000060DDull;
inline constexpr std::uint64_t kSaltChestGear = 0xC4E5700000006EA2ull;

struct LoreEncounter {
    std::string questionId;
    std::string question;
    std::array<std::string, 2> answers;  // field ordinals 1 and 2 (the Jester is 0)
    int correctAnswer = 0;               // which of `answers` is right
    std::string mockLine;
};

struct ChestEncounter {
    std::array<ChestRole, 3> roles{ChestRole::Reward, ChestRole::Mimic, ChestRole::Empty};
    bool rewardIsGold = true;
    std::string gearId;             // the reward when !rewardIsGold
    dungeon::EnemyTeam mimicTeam;   // the boss the lying chest becomes
    BattleSpoils mimicSpoils;       // its authored boss XP + the bounty
};

struct SpecialEncounter {
    SpecialKind kind = SpecialKind::None;
    LoreEncounter lore;
    ChestEncounter chests;
    SpecialResult result = SpecialResult::Unresolved;
    int rewardGold = 0;          // what the resolution pays (the screen pays it)
    std::string rewardGearId;    // or hands over
    int deciderPartyIndex = -1;  // who chose (the Mimic morph needs it)
    bool resolved() const { return result != SpecialResult::Unresolved; }
};

// ---------------------------------------------------------------- lore ----

inline bool loreEligible(const content::LoreQuestionDef& q, int highestTown, bool kingDefeated) {
    return q.minTown <= highestTown && (!q.postKing || kingDefeated);
}

// The eligible pool, sorted by id (a stable base for the seeded walk).
inline std::vector<std::string> eligibleLoreIds(const content::ContentDatabase& db,
                                                int highestTown, bool kingDefeated) {
    std::vector<std::string> ids;
    for (const auto& [id, q] : db.loreQuestions()) {
        if (loreEligible(q, highestTown, kingDefeated)) {
            ids.push_back(id);
        }
    }
    std::sort(ids.begin(), ids.end());
    return ids;
}

// The `loreCount`-th lore patrol of this run (0-based) asks the
// `loreCount % n`-th question of a run-seeded Fisher-Yates order over the
// eligible pool — every question once before any repeat, reload-honest.
// The answer's field position is its own hash so the right answer is not
// always the top one. Nullopt when nothing is eligible (the caller falls
// back to an ordinary patrol).
inline std::optional<LoreEncounter> makeLoreEncounter(const content::ContentDatabase& db,
                                                      int highestTown, bool kingDefeated,
                                                      std::uint64_t runSeed, int patrolIndex,
                                                      int loreCount) {
    std::vector<std::string> ids = eligibleLoreIds(db, highestTown, kingDefeated);
    if (ids.empty()) {
        return std::nullopt;
    }
    for (std::size_t i = ids.size() - 1; i > 0; --i) {
        const std::uint64_t h = blackMarketHash(runSeed, kSaltLoreShuffle + i);
        std::swap(ids[i], ids[static_cast<std::size_t>(h % (i + 1))]);
    }
    const std::string& pick =
        ids[static_cast<std::size_t>(std::max(0, loreCount)) % ids.size()];
    const content::LoreQuestionDef* q = db.findLoreQuestion(pick);
    if (q == nullptr) {
        return std::nullopt;
    }
    LoreEncounter e;
    e.questionId = q->id;
    e.question = q->question;
    e.mockLine = q->mockLine;
    const bool rightFirst =
        (blackMarketHash(runSeed, kSaltLoreAnswer + static_cast<std::uint64_t>(patrolIndex)) &
         1ull) == 0ull;
    e.answers = rightFirst ? std::array<std::string, 2>{q->answer, q->wrongAnswer}
                           : std::array<std::string, 2>{q->wrongAnswer, q->answer};
    e.correctAnswer = rightFirst ? 0 : 1;
    return e;
}

// -------------------------------------------------------------- chests ----

// What a reward chest may hold: worn equipment sold at this town — weapon,
// armor or accessory, never legendary, never a relic, heirloom, consumable
// or scroll. Sorted ids (a stable base for the seeded pick).
inline std::vector<std::string> chestGearPool(const content::ContentDatabase& db, int town) {
    std::vector<std::string> ids;
    for (const auto& [id, it] : db.items()) {
        if (it.type != content::ItemType::Equipment || it.rarity == content::Rarity::Legendary) {
            continue;
        }
        if (it.slot != content::EquipSlot::Weapon && it.slot != content::EquipSlot::Armor &&
            it.slot != content::EquipSlot::Accessory) {
            continue;
        }
        if (!it.availableAtTown(town)) {
            continue;
        }
        ids.push_back(id);
    }
    std::sort(ids.begin(), ids.end());
    return ids;
}

// The three chests: roles shuffled by hash, the reward a coin flip between
// gold and gear (gold when the gear pool is empty), the Mimic team at the
// scale the caller gives (the patrol's own scale for this town and depth).
// Nullopt when the content lacks the Mimic boss.
inline std::optional<ChestEncounter> makeChestEncounter(const content::ContentDatabase& db,
                                                        int town, std::uint64_t runSeed,
                                                        int patrolIndex, int statScalePct) {
    const content::BossDef* mimic = db.findBoss(kMimicBossId);
    if (mimic == nullptr) {
        return std::nullopt;
    }
    ChestEncounter e;
    const std::uint64_t pi = static_cast<std::uint64_t>(patrolIndex);
    e.roles = {ChestRole::Reward, ChestRole::Mimic, ChestRole::Empty};
    for (std::size_t i = e.roles.size() - 1; i > 0; --i) {
        const std::uint64_t h = blackMarketHash(runSeed, kSaltChestRoles + pi * 8 + i);
        std::swap(e.roles[i], e.roles[static_cast<std::size_t>(h % (i + 1))]);
    }
    const std::vector<std::string> pool = chestGearPool(db, town);
    e.rewardIsGold =
        pool.empty() || (blackMarketHash(runSeed, kSaltChestReward + pi) & 1ull) == 0ull;
    if (!e.rewardIsGold) {
        e.gearId = pool[static_cast<std::size_t>(blackMarketHash(runSeed, kSaltChestGear + pi) %
                                                 pool.size())];
    }
    e.mimicTeam.isBoss = true;
    e.mimicTeam.bossId = kMimicBossId;
    e.mimicTeam.name = mimic->name;
    e.mimicTeam.enemyIds = mimic->minions;
    e.mimicTeam.statScalePct = statScalePct;
    e.mimicSpoils.xp = mimic->xpReward;
    e.mimicSpoils.gold = kMimicBounty;
    return e;
}

// ------------------------------------------------------- the field --------

// Appends the three placeholders to a party-only battle and returns the index
// of the first. Real enemy-side units at 1 HP, speed 0 — the model counts
// them as a living side; the screen keeps them off the turn order and out of
// the bestiary. Lore: the Jester (ordinal 0, the centre slot) and the two
// answers; Chests: three chests.
inline int appendPlaceholders(battle::Battle& b, const SpecialEncounter& e) {
    const int first = static_cast<int>(b.units.size());
    for (int i = 0; i < kSpecialPlaceholders; ++i) {
        battle::Combatant u;
        u.side = battle::Side::Enemy;
        u.hp = 1;
        u.maxHp = 1;
        u.stats.maxHp = 1;
        u.stats.speed = 0;
        if (e.kind == SpecialKind::Lore) {
            if (i == 0) {
                u.name = "The Jester";
                u.sourceId = kJesterSourceId;
            } else {
                u.name = e.lore.answers[static_cast<std::size_t>(i - 1)];
            }
        } else {
            u.name = "Chest";
        }
        b.units.push_back(std::move(u));
    }
    return first;
}

inline bool skillIsOffensive(const content::SkillDef& s) {
    return s.target == content::SkillTarget::SingleEnemy ||
           s.target == content::SkillTarget::AllEnemies;
}

// The one resolution rule. `ordinal` is the struck placeholder (0..2);
// `aoe` says the action would have hit more than one of them.
inline SpecialResult resolveSpecial(SpecialEncounter& e, int ordinal, bool aoe) {
    e.rewardGold = 0;
    e.rewardGearId.clear();
    if (e.kind == SpecialKind::Lore) {
        if (aoe) {
            e.result = SpecialResult::AoePunished;
        } else if (ordinal == 0) {
            e.result = SpecialResult::JesterPunished;
        } else if (ordinal - 1 == e.lore.correctAnswer) {
            e.result = SpecialResult::LoreCorrect;
            e.rewardGold = kLoreReward;
        } else {
            e.result = SpecialResult::LoreWrong;
        }
        return e.result;
    }
    if (e.kind == SpecialKind::Chests) {
        const ChestRole role =
            aoe ? ChestRole::Mimic
                : e.chests.roles[static_cast<std::size_t>(std::clamp(ordinal, 0, 2))];
        switch (role) {
            case ChestRole::Reward:
                if (e.chests.rewardIsGold) {
                    e.result = SpecialResult::ChestGold;
                    e.rewardGold = kChestGoldReward;
                } else {
                    e.result = SpecialResult::ChestGear;
                    e.rewardGearId = e.chests.gearId;
                }
                break;
            case ChestRole::Mimic:
                e.result = SpecialResult::MimicRevealed;
                break;
            case ChestRole::Empty:
                e.result = SpecialResult::ChestEmpty;
                break;
        }
        return e.result;
    }
    return e.result;
}

inline bool specialPunishes(SpecialResult r) {
    return r == SpecialResult::JesterPunished || r == SpecialResult::AoePunished;
}
inline bool specialMocks(SpecialResult r) {
    return r == SpecialResult::LoreWrong || r == SpecialResult::ChestEmpty || specialPunishes(r);
}
inline bool specialRewards(SpecialResult r) {
    return r == SpecialResult::LoreCorrect || r == SpecialResult::ChestGold ||
           r == SpecialResult::ChestGear;
}

// ------------------------------------------------- the Mimic morph --------

// Carries the party's fight state from the decision battle into the fresh
// Mimic battle (never a writeBackParty round-trip: its once-only latch would
// block the real end-of-fight write-back). HP/MP/statuses/guard/turn counts
// by partyIndex, the run's summon ledger, the debug flag, the round.
inline void carryPartyOver(const battle::Battle& from, battle::Battle& to) {
    for (battle::Combatant& t : to.units) {
        if (t.side != battle::Side::Party) {
            continue;
        }
        for (const battle::Combatant& f : from.units) {
            if (f.side == battle::Side::Party && f.partyIndex == t.partyIndex) {
                t.hp = f.hp;
                t.mp = f.mp;
                t.statuses = f.statuses;
                t.guarding = f.guarding;
                t.actedOnce = f.actedOnce;
                t.ownTurnsTaken = f.ownTurnsTaken;
                break;
            }
        }
    }
    to.usedSummons = from.usedSummons;
#ifndef CRYSTAL_SHIPPING_BUILD
    to.debugPartyUnkillable = from.debugPartyUnkillable;  // the M53 god-mode flag (dev builds)
#endif
    to.turnsTaken = from.turnsTaken;
}

// The rest of round one after the decider's opening action: the fresh
// battle's turn order without the decider (it has acted).
inline std::vector<int> orderAfterDecision(const battle::Battle& b, int deciderPartyIndex) {
    std::vector<int> order = battle::turnOrder(b);
    order.erase(std::remove_if(order.begin(), order.end(),
                               [&](int i) {
                                   const battle::Combatant& u =
                                       b.units[static_cast<std::size_t>(i)];
                                   return u.side == battle::Side::Party &&
                                          u.partyIndex == deciderPartyIndex;
                               }),
                order.end());
    return order;
}

// The prompt above the field while the encounter stands unresolved.
inline std::string specialPrompt(const SpecialEncounter& e) {
    if (e.kind == SpecialKind::Lore) {
        return e.lore.question;
    }
    if (e.kind == SpecialKind::Chests) {
        return "Three chests. One of them is lying.";
    }
    return {};
}

}  // namespace cd
