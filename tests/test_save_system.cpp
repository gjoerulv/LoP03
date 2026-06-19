#include <catch2/catch_test_macros.hpp>

#include <atomic>
#include <filesystem>
#include <fstream>
#include <random>
#include <string>

#include "content/ContentDatabase.hpp"
#include "content/Definitions.hpp"
#include "content/LoadReport.hpp"
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
    content::ClassDef mage;
    mage.id = "mage";
    mage.name = "Mage";
    mage.baseStats = {70, 6, 20, 6, 11};
    db.addClass(mage);
    return db;
}

fs::path makeTempDir() {
    static std::atomic<int> counter{0};
    std::random_device rd;
    fs::path dir = fs::temp_directory_path() /
                   ("cd_save_" + std::to_string(rd()) + "_" + std::to_string(counter++));
    fs::create_directories(dir);
    return dir;
}

void writeFile(const fs::path& p, const std::string& text) {
    std::ofstream out(p, std::ios::binary | std::ios::trunc);
    out << text;
}

}  // namespace

TEST_CASE("save: round-trips a party including damage and gold", "[save]") {
    const content::ContentDatabase db = makeDb();
    const fs::path dir = makeTempDir();
    const save::SaveSystem saves(db, dir);

    Party p;
    p.gold = 150;
    p.members.push_back(createCharacter(*db.findClass("knight"), "Rolan"));
    p.members.push_back(createCharacter(*db.findClass("mage"), "Mira", 3));
    p.members[0].hp = 50;  // damaged

    content::LoadReport rep;
    REQUIRE(saves.save(save::SaveSlot::Manual1, p, rep));
    REQUIRE(rep.ok());
    REQUIRE(saves.exists(save::SaveSlot::Manual1));

    Party loaded;
    content::LoadReport rep2;
    REQUIRE(saves.load(save::SaveSlot::Manual1, loaded, rep2));
    REQUIRE(rep2.ok());
    REQUIRE(loaded.gold == 150);
    REQUIRE(loaded.size() == 2);
    REQUIRE(loaded.members[0].name == "Rolan");
    REQUIRE(loaded.members[0].classId == "knight");
    REQUIRE(loaded.members[0].hp == 50);
    REQUIRE(loaded.members[1].level == 3);

    fs::remove_all(dir);
}

TEST_CASE("save: summary and autosave", "[save]") {
    const content::ContentDatabase db = makeDb();
    const fs::path dir = makeTempDir();
    const save::SaveSystem saves(db, dir);

    Party p;
    p.gold = 42;
    p.members.push_back(createCharacter(*db.findClass("mage"), "Mira", 5));

    content::LoadReport rep;
    REQUIRE(saves.autosave(p, rep));
    REQUIRE(saves.exists(save::SaveSlot::Auto));

    auto summary = saves.summary(save::SaveSlot::Auto);
    REQUIRE(summary.has_value());
    REQUIRE(summary->partySize == 1);
    REQUIRE(summary->highestLevel == 5);
    REQUIRE(summary->gold == 42);

    REQUIRE_FALSE(saves.summary(save::SaveSlot::Manual2).has_value());  // empty slot

    fs::remove_all(dir);
}

TEST_CASE("save: missing slot fails to load and leaves party untouched", "[save]") {
    const content::ContentDatabase db = makeDb();
    const fs::path dir = makeTempDir();
    const save::SaveSystem saves(db, dir);

    Party party;
    content::LoadReport rep;
    REQUIRE_FALSE(saves.load(save::SaveSlot::Manual1, party, rep));
    REQUIRE_FALSE(rep.ok());
    REQUIRE(party.empty());

    fs::remove_all(dir);
}

TEST_CASE("save: malformed, foreign-version, and unknown-class saves are rejected", "[save]") {
    const content::ContentDatabase db = makeDb();
    const fs::path dir = makeTempDir();
    const save::SaveSystem saves(db, dir);

    SECTION("malformed JSON") {
        writeFile(saves.slotPath(save::SaveSlot::Manual1), "{ this is not json ");
        Party party;
        content::LoadReport rep;
        REQUIRE_FALSE(saves.load(save::SaveSlot::Manual1, party, rep));
        REQUIRE(party.empty());
    }
    SECTION("unsupported version") {
        writeFile(saves.slotPath(save::SaveSlot::Manual2),
                  R"({"version":999,"gold":0,"party":[]})");
        Party party;
        content::LoadReport rep;
        REQUIRE_FALSE(saves.load(save::SaveSlot::Manual2, party, rep));
        REQUIRE(party.empty());
    }
    SECTION("unknown class reference") {
        writeFile(saves.slotPath(save::SaveSlot::Manual3),
                  R"({"version":1,"gold":0,"party":[
                     {"classId":"ghost","name":"X","level":1,"xp":0,"hp":1,"mp":0}]})");
        Party party;
        content::LoadReport rep;
        REQUIRE_FALSE(saves.load(save::SaveSlot::Manual3, party, rep));
        REQUIRE(party.empty());
    }

    fs::remove_all(dir);
}

TEST_CASE("save: slot paths are distinct files under the save dir", "[save]") {
    const content::ContentDatabase db = makeDb();
    const fs::path dir = makeTempDir();
    const save::SaveSystem saves(db, dir);

    REQUIRE(saves.slotPath(save::SaveSlot::Auto).parent_path() == dir);
    REQUIRE(saves.slotPath(save::SaveSlot::Auto) != saves.slotPath(save::SaveSlot::Manual1));
    REQUIRE(saves.slotPath(save::SaveSlot::Manual1).extension() == ".json");

    fs::remove_all(dir);
}
