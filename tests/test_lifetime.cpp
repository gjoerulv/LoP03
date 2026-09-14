// M109 - the persistent lifetime ledger: the pure model, the save
// round-trip through the field tables, old-save migration, the defensive
// reader, and the slot-keyed identity rule.

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <atomic>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <limits>
#include <random>
#include <set>
#include <string>

#include <nlohmann/json.hpp>

#include "content/ContentDatabase.hpp"
#include "content/Definitions.hpp"
#include "content/JsonValidation.hpp"
#include "content/LoadReport.hpp"
#include "game/Lifetime.hpp"
#include "game/Party.hpp"
#include "save/SaveSystem.hpp"

using namespace cd;
namespace fs = std::filesystem;

namespace {

content::ContentDatabase makeDb() {
    content::ContentDatabase db;
    content::ClassDef knight;
    knight.id = "knight";
    knight.name = "Knight";
    knight.baseStats = {120, 18, 4, 16, 8};
    db.addClass(knight);
    return db;
}

fs::path makeTempDir() {
    static std::atomic<int> counter{0};
    std::random_device rd;
    fs::path dir = fs::temp_directory_path() /
                   ("cd_lifetime_" + std::to_string(rd()) + "_" + std::to_string(counter++));
    fs::create_directories(dir);
    return dir;
}

void writeFile(const fs::path& p, const std::string& text) {
    std::ofstream out(p, std::ios::binary | std::ios::trunc);
    out << text;
}

std::string readFile(const fs::path& p) {
    std::ifstream in(p, std::ios::binary);
    return std::string((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
}

// Fills every table-driven counter with a distinct value (>= INT_MAX so the
// 64-bit path is genuinely exercised) and returns the ledger.
LifetimeStats filledLedger() {
    LifetimeStats s;
    std::int64_t v = static_cast<std::int64_t>(std::numeric_limits<int>::max()) + 1;
    for (MemberLifetime& m : s.members) {
        for (const auto& f : kMemberLifetimeFields) {
            m.*(f.member) = v++;
        }
    }
    for (const auto& f : kCombatLifetimeFields) {
        s.combat.*(f.member) = v++;
    }
    s.combat.highestHitMember = 2;
    for (const auto& f : kEconomyLifetimeFields) {
        s.economy.*(f.member) = v++;
    }
    for (TownLifetime& t : s.towns) {
        for (const auto& f : kTownLifetimeFields) {
            t.*(f.member) = v++;
        }
    }
    for (const auto& f : kPatrolLifetimeFields) {
        s.patrols.*(f.member) = v++;
    }
    for (const auto& f : kExploreLifetimeFields) {
        s.explore.*(f.member) = v++;
    }
    s.defeats["goblin_grunt"] = 41;
    s.defeats["the_dragon"] = 2;
    s.defeats["retired_foe_from_an_old_patch"] = 7;
    s.migrated = false;
    return s;
}

template <typename Group, std::size_t N>
void requireGroupEqual(const Group& a, const Group& b,
                       const std::array<LifetimeField<Group>, N>& fields) {
    for (const auto& f : fields) {
        INFO(f.key);
        REQUIRE(a.*(f.member) == b.*(f.member));
    }
}

void requireLedgerEqual(const LifetimeStats& a, const LifetimeStats& b) {
    for (std::size_t i = 0; i < kLifetimeMemberSlots; ++i) {
        requireGroupEqual(a.members[i], b.members[i], kMemberLifetimeFields);
    }
    requireGroupEqual(a.combat, b.combat, kCombatLifetimeFields);
    REQUIRE(a.combat.highestHitMember == b.combat.highestHitMember);
    requireGroupEqual(a.economy, b.economy, kEconomyLifetimeFields);
    for (std::size_t t = 0; t < static_cast<std::size_t>(kTownCount); ++t) {
        requireGroupEqual(a.towns[t], b.towns[t], kTownLifetimeFields);
    }
    requireGroupEqual(a.patrols, b.patrols, kPatrolLifetimeFields);
    requireGroupEqual(a.explore, b.explore, kExploreLifetimeFields);
    REQUIRE(a.defeats == b.defeats);
    REQUIRE(a.migrated == b.migrated);
}

Party partyWithLedger(const content::ContentDatabase& db, LifetimeStats ledger) {
    Party p;
    p.gold = 10;
    p.members.push_back(createCharacter(*db.findClass("knight"), "Rolan"));
    p.lifetime = std::move(ledger);
    return p;
}

}  // namespace

TEST_CASE("lifetime: a fresh ledger is all zeros and not migrated", "[lifetime]") {
    const LifetimeStats s;
    for (const MemberLifetime& m : s.members) {
        for (const auto& f : kMemberLifetimeFields) {
            REQUIRE(m.*(f.member) == 0);
        }
    }
    for (const auto& f : kCombatLifetimeFields) {
        REQUIRE(s.combat.*(f.member) == 0);
    }
    REQUIRE(s.combat.highestHitMember == -1);
    for (const auto& f : kEconomyLifetimeFields) {
        REQUIRE(s.economy.*(f.member) == 0);
    }
    for (const TownLifetime& t : s.towns) {
        for (const auto& f : kTownLifetimeFields) {
            REQUIRE(t.*(f.member) == 0);
        }
    }
    REQUIRE(s.defeats.empty());
    REQUIRE_FALSE(s.migrated);
    REQUIRE(kLifetimeMemberSlots == kMaxPartySize);
}

TEST_CASE("lifetime: the field tables name every counter exactly once", "[lifetime]") {
    // The tables are the serialization contract: a key may not repeat inside
    // a group, and the struct sizes below pin that every counter is listed
    // (adding a LifetimeCount without its table row fails here).
    const auto uniqueKeys = [](const auto& fields) {
        std::set<std::string> keys;
        for (const auto& f : fields) {
            keys.insert(f.key);
        }
        return keys.size() == fields.size();
    };
    REQUIRE(uniqueKeys(kMemberLifetimeFields));
    REQUIRE(uniqueKeys(kCombatLifetimeFields));
    REQUIRE(uniqueKeys(kEconomyLifetimeFields));
    REQUIRE(uniqueKeys(kTownLifetimeFields));
    REQUIRE(uniqueKeys(kPatrolLifetimeFields));
    REQUIRE(uniqueKeys(kExploreLifetimeFields));
    REQUIRE(sizeof(MemberLifetime) == kMemberLifetimeFields.size() * sizeof(LifetimeCount));
    REQUIRE(sizeof(EconomyLifetime) == kEconomyLifetimeFields.size() * sizeof(LifetimeCount));
    REQUIRE(sizeof(TownLifetime) == kTownLifetimeFields.size() * sizeof(LifetimeCount));
    REQUIRE(sizeof(PatrolLifetime) == kPatrolLifetimeFields.size() * sizeof(LifetimeCount));
    REQUIRE(sizeof(ExploreLifetime) == kExploreLifetimeFields.size() * sizeof(LifetimeCount));
    // Combat carries one extra int (the highest-hit member) beyond its table.
    REQUIRE(sizeof(CombatLifetime) >= kCombatLifetimeFields.size() * sizeof(LifetimeCount));
}

TEST_CASE("lifetime: recordLifetimeHit keeps member and global tallies in step", "[lifetime]") {
    LifetimeStats s;
    recordLifetimeHit(s, 1, 40);
    recordLifetimeHit(s, 3, 90);
    recordLifetimeHit(s, 1, 25);
    recordLifetimeHit(s, 1, 0);    // nothing dealt: nothing recorded
    recordLifetimeHit(s, -1, 500); // an unattributable hit still counts globally
    REQUIRE(s.combat.damageDealt == 655);
    REQUIRE(s.combat.highestHit == 500);
    REQUIRE(s.combat.highestHitMember == -1);
    REQUIRE(s.members[1].damageDealt == 65);
    REQUIRE(s.members[1].biggestHit == 40);
    REQUIRE(s.members[3].damageDealt == 90);
    REQUIRE(s.members[3].biggestHit == 90);
    // The author is recorded with the record it set.
    LifetimeStats t;
    recordLifetimeHit(t, 2, 12);
    REQUIRE(t.combat.highestHitMember == 2);
    recordLifetimeHit(t, 0, 11);
    REQUIRE(t.combat.highestHitMember == 2);
}

TEST_CASE("lifetime: the town accessor clamps like every town index", "[lifetime]") {
    LifetimeStats s;
    lifetimeTown(s, 3).clears = 5;
    REQUIRE(s.towns[2].clears == 5);
    lifetimeTown(s, 0).attempts = 1;   // clamps to town 1
    lifetimeTown(s, 99).attempts = 2;  // clamps to town 7
    REQUIRE(s.towns[0].attempts == 1);
    REQUIRE(s.towns[static_cast<std::size_t>(kTownCount) - 1].attempts == 2);
}

TEST_CASE("lifetime: the ledger round-trips through the save with 64-bit values", "[lifetime][save]") {
    const content::ContentDatabase db = makeDb();
    const fs::path dir = makeTempDir();
    const save::SaveSystem saves(db, dir);
    const LifetimeStats original = filledLedger();
    const Party p = partyWithLedger(db, original);

    content::LoadReport rep;
    REQUIRE(saves.save(save::SaveSlot::Manual1, p, rep));
    REQUIRE(rep.ok());

    Party loaded;
    content::LoadReport rep2;
    REQUIRE(saves.load(save::SaveSlot::Manual1, loaded, rep2));
    REQUIRE(rep2.ok());
    requireLedgerEqual(loaded.lifetime, original);
    REQUIRE_FALSE(loaded.lifetime.migrated);
    // The retired-foe id survived: the defeat ledger is never content-validated.
    REQUIRE(loaded.lifetime.defeats.at("retired_foe_from_an_old_patch") == 7);
    fs::remove_all(dir);
}

TEST_CASE("lifetime: an old save without the block migrates to zeros plus the one backfill",
          "[lifetime][save]") {
    const content::ContentDatabase db = makeDb();
    const fs::path dir = makeTempDir();
    const save::SaveSystem saves(db, dir);
    writeFile(dir / "save_slot1.json",
              R"({"version":1,"gold":5,"recordBiggestHit":321,"recordRunDamage":9999,
                  "party":[{"classId":"knight","name":"Rolan","level":2,"xp":0,"hp":50,"mp":4}]})");
    Party loaded;
    content::LoadReport rep;
    REQUIRE(saves.load(save::SaveSlot::Manual1, loaded, rep));
    REQUIRE(rep.ok());
    REQUIRE(loaded.lifetime.migrated);
    REQUIRE(loaded.lifetime.combat.highestHit == 321);
    REQUIRE(loaded.lifetime.combat.highestHitMember == -1);
    // Nothing else is fabricated: not from recordRunDamage, not from anything.
    REQUIRE(loaded.lifetime.combat.damageDealt == 0);
    REQUIRE(loaded.lifetime.combat.battlesWon == 0);
    REQUIRE(loaded.lifetime.explore.playSeconds == 0);
    REQUIRE(loaded.lifetime.defeats.empty());
    // Saving again persists the migrated flag so the summary can say so later.
    content::LoadReport rep2;
    REQUIRE(saves.save(save::SaveSlot::Manual2, loaded, rep2));
    Party again;
    content::LoadReport rep3;
    REQUIRE(saves.load(save::SaveSlot::Manual2, again, rep3));
    REQUIRE(again.lifetime.migrated);
    REQUIRE(again.lifetime.combat.highestHit == 321);
    fs::remove_all(dir);
}

TEST_CASE("lifetime: a below-zero counter degrades to zero without failing the load",
          "[lifetime][save]") {
    const content::ContentDatabase db = makeDb();
    const fs::path dir = makeTempDir();
    const save::SaveSystem saves(db, dir);
    writeFile(dir / "save_slot1.json",
              R"({"version":1,"gold":5,
                  "lifetime":{"combat":{"battlesWon":-3,"highestHit":12,"highestHitMember":9},
                              "defeats":{"goblin_grunt":-1,"wisp":"two","bat":4,"":3},
                              "towns":[{"clears":-8},{"clears":2}],
                              "members":[{},{"timesKo":1},{},{},{"timesKo":99}]},
                  "party":[{"classId":"knight","name":"Rolan","level":1,"xp":0,"hp":50,"mp":4}]})");
    Party loaded;
    content::LoadReport rep;
    REQUIRE(saves.load(save::SaveSlot::Manual1, loaded, rep));
    REQUIRE(rep.ok());
    REQUIRE_FALSE(loaded.lifetime.migrated);
    REQUIRE(loaded.lifetime.combat.battlesWon == 0);
    REQUIRE(loaded.lifetime.combat.highestHit == 12);
    REQUIRE(loaded.lifetime.combat.highestHitMember == -1);  // out-of-range slot -> unknown
    REQUIRE(loaded.lifetime.defeats.size() == 1);
    REQUIRE(loaded.lifetime.defeats.at("bat") == 4);
    REQUIRE(loaded.lifetime.towns[0].clears == 0);
    REQUIRE(loaded.lifetime.towns[1].clears == 2);
    REQUIRE(loaded.lifetime.members[1].timesKo == 1);
    // A fifth member entry is ignored, never fatal.
    fs::remove_all(dir);
}

TEST_CASE("lifetime: a wrong-typed ledger field still reports like every other field",
          "[lifetime][save]") {
    const content::ContentDatabase db = makeDb();
    const fs::path dir = makeTempDir();
    const save::SaveSystem saves(db, dir);
    writeFile(dir / "save_slot1.json",
              R"({"version":1,"gold":5,"lifetime":{"economy":{"goldEarned":"lots"}},
                  "party":[{"classId":"knight","name":"Rolan","level":1,"xp":0,"hp":50,"mp":4}]})");
    Party loaded;
    loaded.gold = 77;
    content::LoadReport rep;
    REQUIRE_FALSE(saves.load(save::SaveSlot::Manual1, loaded, rep));
    REQUIRE_FALSE(rep.ok());
    REQUIRE(loaded.gold == 77);  // the target party is untouched (the M3 rule)
    fs::remove_all(dir);
}

TEST_CASE("lifetime: the save carries the ledger as one nested object", "[lifetime][save]") {
    const content::ContentDatabase db = makeDb();
    const fs::path dir = makeTempDir();
    const save::SaveSystem saves(db, dir);
    LifetimeStats s;
    s.explore.tilesWalked = 12;
    const Party p = partyWithLedger(db, s);
    content::LoadReport rep;
    REQUIRE(saves.save(save::SaveSlot::Manual1, p, rep));
    const nlohmann::json root = nlohmann::json::parse(readFile(dir / "save_slot1.json"));
    REQUIRE(root.contains("lifetime"));
    REQUIRE(root["lifetime"].is_object());
    REQUIRE(root["lifetime"]["explore"]["tilesWalked"] == 12);
    REQUIRE(root["lifetime"]["members"].size() == kLifetimeMemberSlots);
    REQUIRE(root["lifetime"]["towns"].size() == static_cast<std::size_t>(kTownCount));
    REQUIRE(root["lifetime"]["migrated"] == false);
    REQUIRE(root["version"] == 1);  // the schema version is untouched
    fs::remove_all(dir);
}

TEST_CASE("lifetime: history is keyed by slot - a renamed member keeps it", "[lifetime]") {
    const content::ContentDatabase db = makeDb();
    Party p;
    p.members.push_back(createCharacter(*db.findClass("knight"), "Rolan"));
    p.members.push_back(createCharacter(*db.findClass("knight"), "Bram"));
    recordLifetimeHit(p.lifetime, 1, 77);
    p.lifetime.members[1].finishingBlows = 3;
    // No rename feature exists after creation; the rule is that the NAME is
    // display only. Renaming the second member changes nothing in the ledger.
    p.members[1].name = "Bramwell the Renamed";
    REQUIRE(p.lifetime.members[1].damageDealt == 77);
    REQUIRE(p.lifetime.members[1].finishingBlows == 3);
    REQUIRE(p.lifetime.members[0].damageDealt == 0);
    REQUIRE(p.lifetime.combat.highestHitMember == 1);
}

TEST_CASE("lifetime: a New Game starts a fresh ledger", "[lifetime]") {
    Party p;
    p.lifetime = filledLedger();
    resetForNewGame(p);
    REQUIRE(p.lifetime.combat.damageDealt == 0);
    REQUIRE(p.lifetime.defeats.empty());
    REQUIRE_FALSE(p.lifetime.migrated);
    REQUIRE(p.gold == kNewGameGold);
}

TEST_CASE("lifetime: optInt64 reads 64-bit values and reports only type errors", "[lifetime]") {
    const nlohmann::json obj = nlohmann::json::parse(
        R"({"big": 9007199254740993, "neg": -5, "str": "x"})");
    content::LoadReport rep;
    content::ObjectReader r(obj, "t", "t.json", rep);
    REQUIRE(r.optInt64("big", 0) == 9007199254740993LL);
    REQUIRE(r.optInt64("neg", 0) == -5);   // no minimum: the caller clamps
    REQUIRE(r.optInt64("absent", 4) == 4);
    REQUIRE(rep.ok());
    REQUIRE(r.optInt64("str", 1) == 1);
    REQUIRE_FALSE(rep.ok());
}
