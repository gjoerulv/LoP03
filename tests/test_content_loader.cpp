#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <fstream>
#include <string>

#include "content/ContentDatabase.hpp"
#include "content/ContentLoader.hpp"
#include "content/LoadReport.hpp"

using namespace cd::content;
namespace fs = std::filesystem;

namespace {

Json parse(const char* text) { return Json::parse(text, nullptr, false); }

const char* kSkills =
    R"({"version":1,"skills":[
        {"id":"strike","name":"Strike","category":"physical","target":"single_enemy"}]})";

// Writes `text` to a unique temp file and returns its path.
fs::path writeTemp(const std::string& name, const std::string& text) {
    const fs::path p = fs::temp_directory_path() / name;
    std::ofstream out(p, std::ios::binary | std::ios::trunc);
    out << text;
    return p;
}

}  // namespace

TEST_CASE("loader: a fully valid set passes reference validation", "[content]") {
    ContentDatabase db;
    LoadReport rep;
    parseSkills(parse(kSkills), "skills", db, rep);
    parseClasses(parse(R"({"version":1,"classes":[
        {"id":"knight","name":"Knight",
         "baseStats":{"hp":100,"attack":10,"magic":1,"defense":10,"speed":5},
         "startingSkills":["strike"]}]})"),
                 "classes", db, rep);
    REQUIRE(rep.ok());

    validateReferences(db, rep);
    REQUIRE(rep.ok());
}

TEST_CASE("loader: a class referencing an unknown skill is reported", "[content]") {
    ContentDatabase db;
    LoadReport rep;
    parseSkills(parse(kSkills), "skills", db, rep);
    parseClasses(parse(R"({"version":1,"classes":[
        {"id":"knight","name":"Knight",
         "baseStats":{"hp":100,"attack":10,"magic":1,"defense":10,"speed":5},
         "startingSkills":["strike","nonexistent"]}]})"),
                 "classes", db, rep);
    REQUIRE(rep.ok());  // parsing succeeds; references not yet checked

    validateReferences(db, rep);
    REQUIRE_FALSE(rep.ok());
}

TEST_CASE("loader: an enemy referencing an unknown skill is reported", "[content]") {
    ContentDatabase db;
    LoadReport rep;
    parseSkills(parse(kSkills), "skills", db, rep);
    parseEnemies(parse(R"({"version":1,"enemies":[
        {"id":"goblin","name":"Goblin",
         "stats":{"hp":20,"attack":8,"magic":0,"defense":3,"speed":9},
         "skills":["ghost_skill"]}]})"),
                 "enemies", db, rep);
    REQUIRE(rep.ok());

    validateReferences(db, rep);
    REQUIRE_FALSE(rep.ok());
}

TEST_CASE("loader: a scroll referencing an unknown skill is reported", "[content]") {
    ContentDatabase db;
    LoadReport rep;
    parseSkills(parse(kSkills), "skills", db, rep);
    parseItems(parse(R"({"version":1,"items":[
        {"id":"scroll_x","name":"Scroll","type":"scroll","value":10,"grantsSkill":"missing"}]})"),
               "items", db, rep);
    REQUIRE(rep.ok());

    validateReferences(db, rep);
    REQUIRE_FALSE(rep.ok());
}

TEST_CASE("loader: readJsonFile reports missing and malformed files", "[content]") {
    SECTION("missing file") {
        LoadReport rep;
        Json out;
        const bool ok = readJsonFile(fs::temp_directory_path() / "definitely_not_here.json", out, rep);
        REQUIRE_FALSE(ok);
        REQUIRE_FALSE(rep.ok());
    }
    SECTION("malformed JSON") {
        const fs::path p = writeTemp("cd_bad.json", R"({ "version": 1, "skills": [ )");
        LoadReport rep;
        Json out;
        const bool ok = readJsonFile(p, out, rep);
        REQUIRE_FALSE(ok);
        REQUIRE_FALSE(rep.ok());
        std::error_code ec;
        fs::remove(p, ec);
    }
    SECTION("valid JSON") {
        const fs::path p = writeTemp("cd_good.json", R"({"version":1,"skills":[]})");
        LoadReport rep;
        Json out;
        const bool ok = readJsonFile(p, out, rep);
        REQUIRE(ok);
        REQUIRE(rep.ok());
        REQUIRE(out.is_object());
        std::error_code ec;
        fs::remove(p, ec);
    }
}

TEST_CASE("loader: loadAll on a missing directory reports, never crashes", "[content]") {
    ContentDatabase db;
    LoadReport rep;
    const bool ok = loadAll(fs::temp_directory_path() / "cd_no_such_dir", db, rep);
    REQUIRE_FALSE(ok);
    REQUIRE_FALSE(rep.ok());
    REQUIRE(db.empty());
}

#ifdef CRYSTAL_TEST_DATA_DIR
TEST_CASE("loader: shipped data loads with zero errors", "[content][data]") {
    ContentDatabase db;
    LoadReport rep;
    const bool ok = loadAll(fs::path(CRYSTAL_TEST_DATA_DIR), db, rep);
    INFO(rep.summary());
    REQUIRE(ok);
    REQUIRE(db.classCount() == 6);
    REQUIRE(db.skillCount() == 28);
    REQUIRE(db.enemyCount() == 20);
    REQUIRE(db.itemCount() == 36);
    REQUIRE(db.bossCount() == 4);
    REQUIRE(db.themeCount() == 3);
    REQUIRE(db.findClass("knight") != nullptr);
    REQUIRE(db.hasSkill("fireball"));
    REQUIRE(db.findItem("scroll_fireball") != nullptr);
    REQUIRE(db.findBoss("keep_warden") != nullptr);
    REQUIRE(db.findBoss("keep_warden")->xpReward > 0);
    REQUIRE(db.findTheme("ruined_keep") != nullptr);
}
#endif
