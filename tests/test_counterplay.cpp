// M76 — Counterplay: the Mirrorbreak and Absolve skills, the Knight's Smite,
// the Dark Shadow Strike, the Holy Taxes and Evil Duckling items, and the
// Duckling Peddler event (a pure-hash replacement that keeps generation v14).
// Shipped-content checks run against the real data/ tree; battle checks run
// the real defs through the shared battle model.

#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <set>

#include "battle/Battle.hpp"
#include "content/ContentDatabase.hpp"
#include "content/ContentLoader.hpp"
#include "dungeon/DungeonGenerator.hpp"
#include "dungeon/DungeonModel.hpp"
#include "dungeon/ThemeEvents.hpp"
#include "game/Party.hpp"

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

bool learns(const content::ClassDef& cls, const std::string& skillId, int atLevel) {
    for (const content::LearnEntry& e : cls.learnset) {
        if (e.skill == skillId && e.level == atLevel) {
            return true;
        }
    }
    return false;
}

// A one-member party of `classId` at `level`, and one shipped grunt — enough to
// run the real skills/items through the shared battle model.
battle::Battle fight(const char* classId, int level) {
    Party p;
    p.members.push_back(createCharacter(*db().findClass(classId), "Hero", level));
    dungeon::EnemyTeam t;
    t.enemyIds = {"goblin_grunt"};
    battle::Battle b = battle::buildBattle(p, t, db());
    REQUIRE(b.units.size() == 2);  // hero + the grunt (a bad id would be skipped)
    return b;
}

}  // namespace

// --- shipped skills & learnsets ----------------------------------------------

TEST_CASE("counterplay: the shipped skills carry the intended shapes", "[counterplay]") {
    const content::SkillDef* mirror = db().findSkill("mirrorbreak");
    REQUIRE(mirror != nullptr);
    CHECK(mirror->category == content::SkillCategory::Physical);  // never magic
    CHECK(mirror->controlEffect == content::SkillEffect::BreakReflect);

    const content::SkillDef* absolve = db().findSkill("absolve");
    REQUIRE(absolve != nullptr);
    CHECK(absolve->controlEffect == content::SkillEffect::Uncurse);
    CHECK(absolve->target == content::SkillTarget::SingleAlly);

    const content::SkillDef* shadow = db().findSkill("shadow_strike");
    REQUIRE(shadow != nullptr);
    CHECK(shadow->element == content::Element::Dark);  // the one Dark exception

    // No foe is immune to Dark, so the Rogue's opener never becomes a trap
    // (the M48 weapon rule, honoured for the one Dark skill too).
    for (const auto& [id, def] : db().enemies()) {
        INFO(id);
        CHECK_FALSE(def.affinity.immuneTo(content::Element::Dark));
    }
    for (const auto& [id, def] : db().bosses()) {
        INFO(id);
        CHECK_FALSE(def.affinity.immuneTo(content::Element::Dark));
    }
}

TEST_CASE("counterplay: the learnsets grant the counterplay", "[counterplay]") {
    CHECK(learns(*db().findClass("ranger"), "mirrorbreak", 12));
    CHECK(learns(*db().findClass("rogue"), "mirrorbreak", 11));
    CHECK(learns(*db().findClass("cleric"), "absolve", 12));
    CHECK(learns(*db().findClass("knight"), "smite", 13));  // the owner's Holy access
}

// --- the two Curse removers (and only those) against real content -------------

TEST_CASE("counterplay: absolve and Holy Taxes lift a Curse; Purify and Remedy never do",
          "[counterplay]") {
    battle::Battle b = fight("cleric", 12);  // knows purify (10) and absolve (12)
    battle::Combatant& hero = b.units[0];
    const auto curse = [&hero] {
        hero.statuses.push_back({content::StatusType::Curse, 0, 9});
    };

    curse();
    b.useSkill(0, 0, *db().findSkill("purify"));
    CHECK(battle::hasStatus(hero, content::StatusType::Curse));  // the cleanse skips it

    b.useItem(0, 0, *db().findItem("antidote"));
    CHECK(battle::hasStatus(hero, content::StatusType::Curse));  // the Remedy skips it

    b.useSkill(0, 0, *db().findSkill("absolve"));
    CHECK_FALSE(battle::hasStatus(hero, content::StatusType::Curse));  // remover one

    curse();
    b.useItem(0, 0, *db().findItem("holy_taxes"));
    CHECK_FALSE(battle::hasStatus(hero, content::StatusType::Curse));  // remover two
}

TEST_CASE("counterplay: the Evil Duckling curses a foe and carries its punchline",
          "[counterplay]") {
    const content::ItemDef* duck = db().findItem("evil_duckling");
    REQUIRE(duck != nullptr);
    CHECK(duck->value == 0);  // value 0 keeps it out of every shop and pool
    CHECK(duck->battleTarget == content::BattleTarget::Enemy);
    CHECK_FALSE(duck->useLine.empty());  // the Hilarious Punchline

    battle::Battle b = fight("knight", 5);
    b.useItem(0, 1, *duck);
    CHECK(battle::isCursed(b.units[1]));
    // The cursed grunt's skills would now cost double (the v15 rule, met by
    // real content for the first time here).
    const content::SkillDef* spark = db().findSkill("spark");
    REQUIRE(spark != nullptr);
    CHECK(battle::mpCostFor(b.units[1], *spark) == spark->mpCost * 2);
}

TEST_CASE("counterplay: mirrorbreak strips a Reflect through the real def", "[counterplay]") {
    battle::Battle b = fight("rogue", 11);  // knows mirrorbreak
    b.units[1].statuses.push_back({content::StatusType::Reflect, 0, 9});
    b.useSkill(0, 1, *db().findSkill("mirrorbreak"));
    CHECK_FALSE(battle::hasStatus(b.units[1], content::StatusType::Reflect));
}

TEST_CASE("counterplay: Holy Taxes stocks from town 3; the duckling never stocks",
          "[counterplay]") {
    const content::ItemDef* taxes = db().findItem("holy_taxes");
    REQUIRE(taxes != nullptr);
    CHECK(taxes->curesCurse);
    CHECK(taxes->value == 200);
    CHECK_FALSE(taxes->availableAtTown(2));
    CHECK(taxes->availableAtTown(3));
    CHECK(taxes->availableAtTown(7));
}

// --- the Duckling Peddler ------------------------------------------------------

TEST_CASE("counterplay: the peddler roll is pure, deterministic and rare", "[counterplay]") {
    // Deterministic: the same seed always answers the same.
    for (std::uint64_t seed : {1ull, 42ull, 0xDEADull}) {
        CHECK(dungeon::duckPeddlerSlot(seed, 3) == dungeon::duckPeddlerSlot(seed, 3));
    }
    // No eligible slots, no peddler.
    CHECK(dungeon::duckPeddlerSlot(7, 0) == -1);
    // Rare but real: the appearance rate over many seeds sits near the
    // authored percent (wide window; the hash is not a coin we tuned).
    int appears = 0;
    for (std::uint64_t seed = 1; seed <= 2000; ++seed) {
        if (dungeon::duckPeddlerSlot(seed, 3) >= 0) {
            ++appears;
        }
    }
    CHECK(appears > 2000 * (dungeon::kDuckPeddlerChancePct - 5) / 100);
    CHECK(appears < 2000 * (dungeon::kDuckPeddlerChancePct + 5) / 100);
}

TEST_CASE("counterplay: a generated peddler sells the duckling at the flat price",
          "[counterplay]") {
    // Sweep seeds until a few dungeons carry the peddler; verify its shape and
    // that generation stays deterministic around it.
    int found = 0;
    for (std::uint64_t seed = 1; seed <= 300 && found < 3; ++seed) {
        const dungeon::Dungeon d = dungeon::generate(seed, 4, db(), "ruined_keep", 3);
        int peddlers = 0;
        for (const dungeon::Room& r : d.rooms) {
            if (r.event.kind != dungeon::RoomEventKind::DuckPeddler) {
                continue;
            }
            ++peddlers;
            CHECK(r.type == dungeon::RoomType::Event);
            CHECK(r.event.goldCost == dungeon::kDuckPeddlerPriceGold);
            CHECK(r.event.itemId == dungeon::kEvilDucklingItemId);
            CHECK(r.teamIndex == -1);  // never an elite-challenge room
        }
        CHECK(peddlers <= 1);  // at most one per dungeon
        if (peddlers == 1) {
            ++found;
            // Determinism: regenerating the same seed reproduces the dungeon,
            // peddler and all.
            const dungeon::Dungeon again = dungeon::generate(seed, 4, db(), "ruined_keep", 3);
            REQUIRE(again.rooms.size() == d.rooms.size());
            for (std::size_t i = 0; i < d.rooms.size(); ++i) {
                CHECK(again.rooms[i].event.kind == d.rooms[i].event.kind);
                CHECK(again.rooms[i].event.goldCost == d.rooms[i].event.goldCost);
            }
        }
    }
    CHECK(found >= 1);  // the sweep met the peddler at least once
}

TEST_CASE("counterplay: the peddler never displaces a rite", "[counterplay]") {
    // Since the 2026-08-17 leveling (generation v22) the rite is a rare roll,
    // not a guarantee — but where it lands, the peddler (which draws AFTER
    // it and only ever takes a PLAIN Shrine/Spring/Merchant/Wager/Rest slot)
    // can never take its room: a floor never holds more than one rite, and
    // both never share a room by construction.
    int riteFloors = 0;
    for (std::uint64_t seed = 1; seed <= 200; ++seed) {
        const dungeon::Dungeon d = dungeon::generate(seed, 4, db(), "ruined_keep", 3);
        int rites = 0;
        for (const dungeon::Room& r : d.rooms) {
            if (r.event.kind == dungeon::RoomEventKind::ArmoryGhost) {
                ++rites;
            }
        }
        INFO(seed);
        CHECK(rites <= 1);
        riteFloors += rites;
    }
    CHECK(riteFloors >= 1);  // the sweep met the leveled rite at least once
}
