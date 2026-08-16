#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <filesystem>
#include <string>
#include <vector>

#include "battle/Battle.hpp"
#include "content/ContentDatabase.hpp"
#include "content/ContentLoader.hpp"
#include "content/LoadReport.hpp"
#include "dungeon/DungeonModel.hpp"
#include "game/Party.hpp"
#include "game/Scrolls.hpp"
#include "save/SaveSystem.hpp"

// M64 — scroll learning: `grantsSkill` finally does something. The learn
// rules, the class-learnset union, the buildBattle wiring, and the save
// round-trip with defensive drops.

using namespace cd;

namespace {
content::ContentDatabase loadContent() {
    content::ContentDatabase db;
    content::LoadReport rep;
    content::loadAll(std::filesystem::path(CRYSTAL_TEST_DATA_DIR), db, rep);
    return db;
}
}  // namespace

TEST_CASE("scrolls: every shipped scroll teaches a real skill", "[scroll][content]") {
    const content::ContentDatabase db = loadContent();
    int scrolls = 0;
    for (const auto& [id, item] : db.items()) {
        if (item.type != content::ItemType::Scroll) {
            continue;
        }
        ++scrolls;
        INFO(id);
        REQUIRE_FALSE(item.grantsSkill.empty());
        CHECK(db.hasSkill(item.grantsSkill));
    }
    CHECK(scrolls == 19);  // 3 base + 6 M65 Lost Scrolls + 7 M92 trove + 3 M95 summon scrolls
}

TEST_CASE("scrolls: refusal rules and the learn", "[scroll]") {
    const content::ContentDatabase db = loadContent();
    const content::ItemDef* scroll = db.findItem("scroll_fireball");
    REQUIRE(scroll != nullptr);
    Character knight = createCharacter(*db.findClass("knight"), "Rolan", 5);

    // A knight does not know Fireball: the scroll teaches it, once.
    CHECK(scrollRefusal(knight, *scroll, db).empty());
    learnScroll(knight, *scroll);
    CHECK(std::find(knight.extraSkills.begin(), knight.extraSkills.end(), "fireball") !=
          knight.extraSkills.end());
    CHECK_FALSE(scrollRefusal(knight, *scroll, db).empty());  // already knows it now

    // A mage learns Fireball from its class: the scroll refuses outright.
    const content::ClassDef* mageCls = db.findClass("mage");
    REQUIRE(mageCls != nullptr);
    Character mage = createCharacter(*mageCls, "Wiz", 50);
    const std::vector<std::string> mageKnown = allKnownSkills(mage, db);
    if (std::find(mageKnown.begin(), mageKnown.end(), "fireball") != mageKnown.end()) {
        CHECK_FALSE(scrollRefusal(mage, *scroll, db).empty());
    }

    // A non-scroll item is never a teacher.
    const content::ItemDef* potion = db.findItem("potion");
    REQUIRE(potion != nullptr);
    CHECK_FALSE(scrollRefusal(knight, *potion, db).empty());
}

TEST_CASE("scrolls: a learned skill reaches the battle", "[scroll][battle]") {
    const content::ContentDatabase db = loadContent();
    Party p;
    Character c = createCharacter(*db.findClass("knight"), "Rolan", 5);
    c.extraSkills.push_back("fireball");
    p.members.push_back(c);
    dungeon::EnemyTeam team;
    const battle::Battle b = battle::buildBattle(p, team, db);
    REQUIRE(b.units.size() == 1);
    CHECK(std::find(b.units[0].skillIds.begin(), b.units[0].skillIds.end(), "fireball") !=
          b.units[0].skillIds.end());
    // The union de-duplicates: an extra the class already grants appears once.
    const std::vector<std::string> known = allKnownSkills(p.members[0], db);
    for (const std::string& id : known) {
        CHECK(std::count(known.begin(), known.end(), id) == 1);
    }
}

TEST_CASE("scrolls: learned skills survive the save; unknown ids drop", "[scroll][save]") {
    const content::ContentDatabase db = loadContent();
    const std::filesystem::path dir =
        std::filesystem::temp_directory_path() / "crystal_scroll_save_test";
    std::filesystem::remove_all(dir);
    save::SaveSystem saves(db, dir);

    Party p;
    Character c = createCharacter(*db.findClass("knight"), "Rolan", 5);
    c.extraSkills = {"fireball", "no_such_skill", "fireball"};
    p.members.push_back(c);

    content::LoadReport rep;
    REQUIRE(saves.save(save::SaveSlot::Manual1, p, rep));
    Party loaded;
    REQUIRE(saves.load(save::SaveSlot::Manual1, loaded, rep));
    REQUIRE(loaded.members.size() == 1);
    CHECK(loaded.members[0].extraSkills == std::vector<std::string>{"fireball"});
    std::filesystem::remove_all(dir);
}
