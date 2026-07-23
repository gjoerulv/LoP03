#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <filesystem>
#include <string>
#include <vector>

#include "content/ContentDatabase.hpp"
#include "content/ContentLoader.hpp"
#include "content/LoadReport.hpp"

// M29 class learnsets: per-level skill grants derived (never stored), so they
// are save-safe and identical in the simulator and live play. These tests cover
// the pure derivation (knownSkillsFor), the loader/validator, and the shipped
// data's growth curve.

using namespace cd::content;
namespace fs = std::filesystem;

namespace {

Json parse(const char* text) { return Json::parse(text, nullptr, false); }

bool has(const std::vector<std::string>& v, const std::string& s) {
    return std::find(v.begin(), v.end(), s) != v.end();
}

// A class with startingSkills = [a] and a learnset granting b@5, c@10.
ClassDef makeClass() {
    ClassDef c;
    c.id = "test";
    c.name = "Test";
    c.startingSkills = {"a"};
    c.learnset = {{"b", 5}, {"c", 10}};
    return c;
}

}  // namespace

TEST_CASE("knownSkillsFor: level 1 exposes only the starting set", "[learnset]") {
    const ClassDef c = makeClass();
    const std::vector<std::string> s = knownSkillsFor(c, 1);
    REQUIRE(s.size() == 1);
    CHECK(s[0] == "a");
    CHECK_FALSE(has(s, "b"));
    CHECK_FALSE(has(s, "c"));
}

TEST_CASE("knownSkillsFor: a level gate grants its skill at that level", "[learnset]") {
    const ClassDef c = makeClass();
    CHECK_FALSE(has(knownSkillsFor(c, 4), "b"));  // one below the gate
    CHECK(has(knownSkillsFor(c, 5), "b"));        // exactly at the gate
    CHECK_FALSE(has(knownSkillsFor(c, 5), "c"));
    CHECK(has(knownSkillsFor(c, 10), "c"));       // second gate
}

TEST_CASE("knownSkillsFor: order is startingSkills first, then by ascending level", "[learnset]") {
    ClassDef c;
    c.startingSkills = {"a"};
    c.learnset = {{"late", 20}, {"early", 5}};  // authored out of order
    const std::vector<std::string> s = knownSkillsFor(c, 99);
    REQUIRE(s.size() == 3);
    CHECK(s[0] == "a");
    CHECK(s[1] == "early");
    CHECK(s[2] == "late");
}

TEST_CASE("knownSkillsFor: a skill already in startingSkills is not duplicated", "[learnset]") {
    ClassDef c;
    c.startingSkills = {"a", "b"};
    c.learnset = {{"b", 3}};  // b also learned later
    const std::vector<std::string> s = knownSkillsFor(c, 10);
    REQUIRE(s.size() == 2);
    CHECK(std::count(s.begin(), s.end(), std::string("b")) == 1);
}

TEST_CASE("knownSkillsFor: level below 1 clamps to 1", "[learnset]") {
    const ClassDef c = makeClass();
    const std::vector<std::string> s = knownSkillsFor(c, 0);
    REQUIRE(s.size() == 1);
    CHECK(s[0] == "a");
}

TEST_CASE("loader: a learnset parses and reference-validates", "[learnset][content]") {
    ContentDatabase db;
    LoadReport rep;
    parseSkills(parse(R"({"version":1,"skills":[
        {"id":"a","name":"A","category":"physical","target":"single_enemy"},
        {"id":"b","name":"B","category":"physical","target":"single_enemy"}]})"),
                "skills", db, rep);
    parseClasses(parse(R"({"version":1,"classes":[
        {"id":"knight","name":"Knight",
         "baseStats":{"hp":100,"attack":10,"magic":1,"defense":10,"speed":5},
         "startingSkills":["a"],
         "learnset":[{"skill":"b","level":6}]}]})"),
                 "classes", db, rep);
    REQUIRE(rep.ok());
    const ClassDef* c = db.findClass("knight");
    REQUIRE(c != nullptr);
    REQUIRE(c->learnset.size() == 1);
    CHECK(c->learnset[0].skill == "b");
    CHECK(c->learnset[0].level == 6);

    validateReferences(db, rep);
    REQUIRE(rep.ok());
}

TEST_CASE("loader: a learnset referencing an unknown skill is reported", "[learnset][content]") {
    ContentDatabase db;
    LoadReport rep;
    parseSkills(parse(R"({"version":1,"skills":[
        {"id":"a","name":"A","category":"physical","target":"single_enemy"}]})"),
                "skills", db, rep);
    parseClasses(parse(R"({"version":1,"classes":[
        {"id":"knight","name":"Knight",
         "baseStats":{"hp":100,"attack":10,"magic":1,"defense":10,"speed":5},
         "startingSkills":["a"],
         "learnset":[{"skill":"ghost","level":6}]}]})"),
                 "classes", db, rep);
    REQUIRE(rep.ok());  // parsing succeeds; references not yet checked
    validateReferences(db, rep);
    REQUIRE_FALSE(rep.ok());
}

TEST_CASE("loader: a learnset level below 1 is rejected", "[learnset][content]") {
    ContentDatabase db;
    LoadReport rep;
    parseSkills(parse(R"({"version":1,"skills":[
        {"id":"a","name":"A","category":"physical","target":"single_enemy"}]})"),
                "skills", db, rep);
    parseClasses(parse(R"({"version":1,"classes":[
        {"id":"knight","name":"Knight",
         "baseStats":{"hp":100,"attack":10,"magic":1,"defense":10,"speed":5},
         "startingSkills":["a"],
         "learnset":[{"skill":"a","level":0}]}]})"),
                 "classes", db, rep);
    REQUIRE_FALSE(rep.ok());  // level < 1 is a validation error
}

#ifdef CRYSTAL_TEST_DATA_DIR
TEST_CASE("learnset: shipped classes grow their skill set with level", "[learnset][data]") {
    ContentDatabase db;
    LoadReport rep;
    REQUIRE(loadAll(fs::path(CRYSTAL_TEST_DATA_DIR), db, rep));

    const ClassDef* knight = db.findClass("knight");
    REQUIRE(knight != nullptr);
    // whirlwind (formerly reachable only via scroll) is a level-10 knight grant.
    CHECK_FALSE(has(knownSkillsFor(*knight, 1), "whirlwind"));
    CHECK(has(knownSkillsFor(*knight, 10), "whirlwind"));

    // Every PLAYABLE-FROM-THE-START class exposes strictly more skills at the cap
    // than at level 1, and reaches a satisfying breadth (>= 6). The M45 reward
    // classes are deliberately exempt: their identity is what they LACK (the
    // Dragon has no skills at all, the Goose has three and a terrible idea), so
    // they are checked against their own intent below.
    for (const auto& [id, cls] : db.classes()) {
        if (cls.unlockedByKing) {
            continue;
        }
        const std::size_t atOne = knownSkillsFor(cls, 1).size();
        const std::size_t atCap = knownSkillsFor(cls, 50).size();
        INFO("class " << id << " lvl1=" << atOne << " cap=" << atCap);
        CHECK(atCap > atOne);
        CHECK(atCap >= 6);
    }

    // M45 reward classes, each odd on purpose.
    const ClassDef* dragon = db.findClass("dragon");
    REQUIRE(dragon != nullptr);
    CHECK(knownSkillsFor(*dragon, 50).empty());  // no skills, ever: it just bites

    const ClassDef* jester = db.findClass("jester");
    REQUIRE(jester != nullptr);
    CHECK(knownSkillsFor(*jester, 50).size() > knownSkillsFor(*jester, 1).size());

    const ClassDef* goose = db.findClass("goose");
    REQUIRE(goose != nullptr);
    CHECK(has(knownSkillsFor(*goose, 30), "everyone_is_welcome"));   // the level-30 ultimate
    CHECK_FALSE(has(knownSkillsFor(*goose, 29), "everyone_is_welcome"));
}
#endif
