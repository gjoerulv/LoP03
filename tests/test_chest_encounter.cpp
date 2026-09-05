// M112 - the Treasure chests and the Mimic: the seeded roles and reward, the
// equipment filter, the resolution rule, the Mimic's team and the pure parts
// of the morph (party carry-over, the rest of round one), the Mimic's
// exclusion from every boss pool, and the [mimic-report] battery.

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <set>
#include <string>
#include <vector>

#include "battle/Battle.hpp"
#include "battle/Simulator.hpp"
#include "content/ContentDatabase.hpp"
#include "content/ContentLoader.hpp"
#include "dungeon/DungeonGenerator.hpp"
#include "game/Castle.hpp"
#include "game/Party.hpp"
#include "game/SpecialEncounter.hpp"
#include "game/TreasureMap.hpp"

using namespace cd;

namespace {

const content::ContentDatabase& db() {
    static content::ContentDatabase database;
    static bool loaded = false;
    if (!loaded) {
        content::LoadReport rep;
        REQUIRE(content::loadAll(std::filesystem::path(CRYSTAL_TEST_DATA_DIR), database, rep));
        loaded = true;
    }
    return database;
}

Party makeParty(int level = 12) {
    Party party;
    for (const char* id : {"knight", "ranger", "mage", "cleric"}) {
        const content::ClassDef* cls = db().findClass(id);
        REQUIRE(cls != nullptr);
        party.members.push_back(createCharacter(*cls, id, level));
    }
    return party;
}

}  // namespace

TEST_CASE("chest: the Mimic is a boss on the median line, special-only", "[chest][content][data]") {
    const content::BossDef* m = db().findBoss(kMimicBossId);
    REQUIRE(m != nullptr);
    CHECK(m->specialOnly);
    CHECK(m->guildTown == 0);
    CHECK(m->minions.empty());
    CHECK(m->goldReward == 0);  // the bounty is the encounter's, not the boss's
    CHECK(m->xpReward == 230);
    CHECK(m->stats.maxHp == 300);
    CHECK_FALSE(m->telegraph.empty());
    // No other boss is special-only (M111 read the flag ahead of its use).
    for (const auto& [id, def] : db().bosses()) {
        if (id != std::string(kMimicBossId)) {
            CHECK_FALSE(def.specialOnly);
        }
    }
}

TEST_CASE("chest: the Mimic is in no boss pool", "[chest]") {
    for (const std::string& id : bossRushOrder(db())) {
        CHECK(id != std::string(kMimicBossId));
    }
    for (int wave = 9; wave < 400; wave += 10) {
        const dungeon::EnemyTeam t = endlessWaveTeam(db(), wave);
        CHECK(t.bossId != std::string(kMimicBossId));
    }
    for (const auto& [id, theme] : db().themes()) {
        for (const std::string& b : theme.bosses) {
            CHECK(b != std::string(kMimicBossId));
        }
    }
    // The generator's fallback sweep skips it too (a theme with no bosses).
    for (std::uint64_t seed = 1; seed <= 40; ++seed) {
        for (int town = 1; town <= 7; ++town) {
            const dungeon::Dungeon d = dungeon::generate(seed, 3, db(), "ruined_keep", town);
            for (const dungeon::EnemyTeam& t : d.teams) {
                CHECK(t.bossId != std::string(kMimicBossId));
            }
        }
    }
}

TEST_CASE("chest: three roles, one of each, seeded by the patrol", "[chest]") {
    int mimicAt[3] = {0, 0, 0};
    for (int patrol = 0; patrol < 60; ++patrol) {
        const auto e = makeChestEncounter(db(), 3, 31ull, patrol, 150);
        REQUIRE(e.has_value());
        const std::set<ChestRole> roles(e->roles.begin(), e->roles.end());
        CHECK(roles.size() == 3);
        for (int i = 0; i < 3; ++i) {
            if (e->roles[static_cast<std::size_t>(i)] == ChestRole::Mimic) {
                ++mimicAt[i];
            }
        }
        // Deterministic: the same (seed, patrol) rebuilds the same chests.
        const auto again = makeChestEncounter(db(), 3, 31ull, patrol, 150);
        REQUIRE(again.has_value());
        CHECK(again->roles == e->roles);
        CHECK(again->rewardIsGold == e->rewardIsGold);
        CHECK(again->gearId == e->gearId);
    }
    CHECK(mimicAt[0] > 8);
    CHECK(mimicAt[1] > 8);
    CHECK(mimicAt[2] > 8);
}

TEST_CASE("chest: the reward is a coin flip between 500 gold and town gear", "[chest]") {
    int gold = 0;
    int gear = 0;
    for (int patrol = 0; patrol < 80; ++patrol) {
        const auto e = makeChestEncounter(db(), 4, 8ull, patrol, 150);
        REQUIRE(e.has_value());
        if (e->rewardIsGold) {
            ++gold;
            CHECK(e->gearId.empty());
        } else {
            ++gear;
            const content::ItemDef* it = db().findItem(e->gearId);
            REQUIRE(it != nullptr);
            CHECK(it->type == content::ItemType::Equipment);
            CHECK(it->rarity != content::Rarity::Legendary);
            CHECK(it->availableAtTown(4));
            CHECK((it->slot == content::EquipSlot::Weapon || it->slot == content::EquipSlot::Armor ||
                   it->slot == content::EquipSlot::Accessory));
        }
    }
    CHECK(gold > 20);
    CHECK(gear > 20);
}

TEST_CASE("chest: the gear pool is worn equipment only, sold at the town", "[chest]") {
    for (int town = 1; town <= 7; ++town) {
        const std::vector<std::string> pool = chestGearPool(db(), town);
        CHECK_FALSE(pool.empty());
        CHECK(std::is_sorted(pool.begin(), pool.end()));
        for (const std::string& id : pool) {
            const content::ItemDef* it = db().findItem(id);
            REQUIRE(it != nullptr);
            INFO(id);
            CHECK(it->type != content::ItemType::Consumable);
            CHECK(it->type != content::ItemType::Scroll);
            CHECK(it->type != content::ItemType::Relic);
            CHECK(it->type != content::ItemType::Heirloom);
            CHECK(it->rarity != content::Rarity::Legendary);
            CHECK(it->availableAtTown(town));
            CHECK(it->slot != content::EquipSlot::Heirloom);
            CHECK(it->slot != content::EquipSlot::None);
        }
        // A town-7 piece never shows up in town 1.
        if (town == 1) {
            for (const std::string& id : pool) {
                CHECK(db().findItem(id)->minTown <= 1);
            }
        }
    }
    // An empty pool falls back to gold (a database with no items).
    content::ContentDatabase noItems;
    CHECK(chestGearPool(noItems, 3).empty());
}

TEST_CASE("chest: the resolution rule", "[chest]") {
    SpecialEncounter e;
    e.kind = SpecialKind::Chests;
    e.chests = *makeChestEncounter(db(), 2, 5ull, 0, 120);
    int rewardAt = -1;
    int mimicAt = -1;
    int emptyAt = -1;
    for (int i = 0; i < 3; ++i) {
        switch (e.chests.roles[static_cast<std::size_t>(i)]) {
            case ChestRole::Reward: rewardAt = i; break;
            case ChestRole::Mimic: mimicAt = i; break;
            case ChestRole::Empty: emptyAt = i; break;
        }
    }
    REQUIRE(rewardAt >= 0);
    REQUIRE(mimicAt >= 0);
    REQUIRE(emptyAt >= 0);

    SpecialEncounter r = e;
    resolveSpecial(r, rewardAt, false);
    if (e.chests.rewardIsGold) {
        CHECK(r.result == SpecialResult::ChestGold);
        CHECK(r.rewardGold == kChestGoldReward);
    } else {
        CHECK(r.result == SpecialResult::ChestGear);
        CHECK(r.rewardGearId == e.chests.gearId);
    }
    CHECK(specialRewards(r.result));

    SpecialEncounter m = e;
    CHECK(resolveSpecial(m, mimicAt, false) == SpecialResult::MimicRevealed);
    CHECK_FALSE(specialMocks(m.result));
    CHECK_FALSE(specialRewards(m.result));

    SpecialEncounter n = e;
    CHECK(resolveSpecial(n, emptyAt, false) == SpecialResult::ChestEmpty);
    CHECK(specialMocks(n.result));
    CHECK_FALSE(specialPunishes(n.result));

    SpecialEncounter s = e;
    CHECK(resolveSpecial(s, rewardAt, true) == SpecialResult::MimicRevealed);  // a sweep wakes the Mimic
    CHECK(specialPrompt(e) == "Three chests. One of them is lying.");
}

TEST_CASE("chest: the field is three chests; the Mimic team is a lone boss at the patrol's scale",
          "[chest]") {
    SpecialEncounter e;
    e.kind = SpecialKind::Chests;
    e.chests = *makeChestEncounter(db(), 5, 11ull, 3, 260);
    battle::Battle b = battle::buildBattle(makeParty(), dungeon::EnemyTeam{}, db());
    const int first = appendPlaceholders(b, e);
    REQUIRE(first == 4);
    for (int i = first; i < first + kSpecialPlaceholders; ++i) {
        CHECK(b.units[static_cast<std::size_t>(i)].name == "Chest");
        CHECK(b.units[static_cast<std::size_t>(i)].sourceId.empty());
        CHECK(b.units[static_cast<std::size_t>(i)].hp == 1);
    }
    CHECK(e.chests.mimicTeam.isBoss);
    CHECK(e.chests.mimicTeam.bossId == std::string(kMimicBossId));
    CHECK(e.chests.mimicTeam.enemyIds.empty());
    CHECK(e.chests.mimicTeam.statScalePct == 260);
    CHECK(e.chests.mimicSpoils.xp == 230);
    CHECK(e.chests.mimicSpoils.gold == kMimicBounty);
    // The Mimic battle is a real boss battle at that scale.
    battle::Battle mimic = battle::buildBattle(makeParty(), e.chests.mimicTeam, db());
    REQUIRE(mimic.units.size() == 5);
    CHECK(mimic.units[4].isBoss);
    CHECK(mimic.units[4].sourceId == std::string(kMimicBossId));
    CHECK(mimic.units[4].maxHp == 300 * 260 / 100);
    // A database without the boss gives no chests (the caller falls back).
    content::ContentDatabase none;
    CHECK_FALSE(makeChestEncounter(none, 5, 11ull, 3, 260).has_value());
}

TEST_CASE("chest: the morph carries the party over and hands the rest of round one on",
          "[chest]") {
    SpecialEncounter e;
    e.kind = SpecialKind::Chests;
    e.chests = *makeChestEncounter(db(), 2, 3ull, 0, 130);
    battle::Battle decision = battle::buildBattle(makeParty(), dungeon::EnemyTeam{}, db());
    appendPlaceholders(decision, e);
    decision.turnsTaken = 1;
    // Wear from before the encounter: hurt, spent, poisoned, one guarding,
    // the decider having acted, a summon spent this run.
    decision.units[0].hp = 40;
    decision.units[1].mp = 3;
    decision.units[2].statuses.push_back({content::StatusType::Poison, 4, 2});
    decision.units[3].guarding = true;
    decision.units[0].actedOnce = true;
    decision.units[0].ownTurnsTaken = 1;
    decision.usedSummons = {"summon_goose"};
#ifndef CRYSTAL_SHIPPING_BUILD
    decision.debugPartyUnkillable = true;
#endif

    battle::Battle fresh = battle::buildBattle(makeParty(), e.chests.mimicTeam, db());
    carryPartyOver(decision, fresh);
    CHECK(fresh.units[0].hp == 40);
    CHECK(fresh.units[1].mp == 3);
    REQUIRE(fresh.units[2].statuses.size() == 1);
    CHECK(fresh.units[2].statuses[0].type == content::StatusType::Poison);
    CHECK(fresh.units[3].guarding);
    CHECK(fresh.units[0].actedOnce);
    CHECK(fresh.units[0].ownTurnsTaken == 1);
    CHECK(fresh.usedSummons == std::vector<std::string>{"summon_goose"});
#ifndef CRYSTAL_SHIPPING_BUILD
    CHECK(fresh.debugPartyUnkillable);
#endif
    CHECK(fresh.turnsTaken == 1);
    // The Mimic itself is untouched by the carry-over.
    CHECK(fresh.units[4].hp == fresh.units[4].maxHp);
    CHECK(fresh.units[4].statuses.empty());
    // The decider (party index 0) is out of the rest of round one; everyone
    // else keeps their place in the fresh battle's own order.
    const std::vector<int> rest = orderAfterDecision(fresh, 0);
    CHECK(rest.size() == 4);
    for (int i : rest) {
        CHECK(i != 0);
    }
    const std::vector<int> full = battle::turnOrder(fresh);
    std::vector<int> expected;
    for (int i : full) {
        if (i != 0) {
            expected.push_back(i);
        }
    }
    CHECK(rest == expected);
    // The roll stream starts fresh: nothing rolled during the decision.
    CHECK(fresh.rollCursor == 0);
}

TEST_CASE("mimic-report: the Mimic against the theme bosses at the same scale (-s)",
          "[mimic-report][chest][!benchmark]") {
    // The Mimic must fight like a dungeon boss of its floor: comparable win
    // rates and rounds to the theme bosses at the same scale — not trivial,
    // not a superboss. Read with -s.
    constexpr int kSeeds = 6;
    const std::vector<std::string> peers = {"keep_warden", "crystal_sorcerer", "hollow_commander",
                                            "sand_warlord", "frost_monarch"};
    for (int town = 1; town <= 7; ++town) {
        for (int depth : {1, 5, 10}) {
            const int level = std::min(99, 4 + depth * 2 + (town - 1) * 6);
            const dungeon::EnemyTeam shadow =
                dungeon::patrolTeam(db(), "ruined_keep", town, depth, 99ull, 1);
            const int scale = shadow.statScalePct;
            auto sim = [&](const dungeon::EnemyTeam& team, int& wins, int& rounds) {
                for (int seed = 0; seed < kSeeds; ++seed) {
                    battle::Battle b = battle::buildBattle(makeParty(level), team, db());
                    b.rngSeed ^= 0xA11CE5ull + static_cast<std::uint64_t>(seed) * 104729ull;
                    const battle::SimResult r = battle::simulateInPlace(b, db(), 60);
                    if (r.outcome == battle::Outcome::Victory) {
                        ++wins;
                    }
                    rounds += r.rounds;
                }
            };
            const auto chests = makeChestEncounter(db(), town, 99ull, 1, scale);
            REQUIRE(chests.has_value());
            int mimicWins = 0;
            int mimicRounds = 0;
            sim(chests->mimicTeam, mimicWins, mimicRounds);
            int peerWins = 0;
            int peerRounds = 0;
            int peerFights = 0;
            for (const std::string& id : peers) {
                const content::BossDef* boss = db().findBoss(id);
                if (boss == nullptr || boss->minTown > town) {
                    continue;
                }
                dungeon::EnemyTeam t;
                t.isBoss = true;
                t.bossId = id;
                t.name = boss->name;
                t.enemyIds = boss->minions;
                t.statScalePct = scale;
                sim(t, peerWins, peerRounds);
                peerFights += kSeeds;
            }
            INFO("town " << town << " depth " << depth << " level " << level << " scale " << scale
                         << "%: party beat the Mimic " << mimicWins << "/" << kSeeds << " in "
                         << mimicRounds / kSeeds << " rounds; beat the theme bosses " << peerWins
                         << "/" << peerFights << " in "
                         << (peerFights > 0 ? peerRounds / peerFights : 0) << " rounds");
            CHECK(peerFights > 0);
        }
    }
}
