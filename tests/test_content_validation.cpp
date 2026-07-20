#include <catch2/catch_test_macros.hpp>

#include "content/ContentDatabase.hpp"
#include "content/ContentLoader.hpp"
#include "content/LoadReport.hpp"

using namespace cd::content;

namespace {
Json parse(const char* text) { return Json::parse(text, nullptr, false); }
}  // namespace

TEST_CASE("validation: a well-formed skill loads with correct fields", "[content]") {
    ContentDatabase db;
    LoadReport rep;
    Json root = parse(R"({"version":1,"skills":[
        {"id":"fireball","name":"Fireball","category":"magic","target":"single_enemy",
         "element":"fire","power":14,"mpCost":6}]})");
    parseSkills(root, "mem", db, rep);

    REQUIRE(rep.ok());
    REQUIRE(db.skillCount() == 1);
    const SkillDef* s = db.findSkill("fireball");
    REQUIRE(s != nullptr);
    REQUIRE(s->category == SkillCategory::Magic);
    REQUIRE(s->target == SkillTarget::SingleEnemy);
    REQUIRE(s->element == Element::Fire);
    REQUIRE(s->power == 14);
    REQUIRE(s->mpCost == 6);
}

TEST_CASE("validation: missing required field is reported and entry skipped", "[content]") {
    ContentDatabase db;
    LoadReport rep;
    Json root = parse(R"({"version":1,"skills":[
        {"name":"No Id","category":"physical","target":"single_enemy"}]})");
    parseSkills(root, "mem", db, rep);

    REQUIRE_FALSE(rep.ok());
    REQUIRE(db.skillCount() == 0);
}

TEST_CASE("validation: wrong field type is reported", "[content]") {
    ContentDatabase db;
    LoadReport rep;
    Json root = parse(R"({"version":1,"skills":[
        {"id":"x","name":"X","category":"physical","target":"single_enemy","power":"lots"}]})");
    parseSkills(root, "mem", db, rep);

    REQUIRE_FALSE(rep.ok());
    REQUIRE(db.skillCount() == 0);
}

TEST_CASE("validation: unknown enum value is reported", "[content]") {
    ContentDatabase db;
    LoadReport rep;
    Json root = parse(R"({"version":1,"skills":[
        {"id":"x","name":"X","category":"teleportation","target":"single_enemy"}]})");
    parseSkills(root, "mem", db, rep);

    REQUIRE_FALSE(rep.ok());
    REQUIRE(db.skillCount() == 0);
}

TEST_CASE("validation: out-of-range integer is reported", "[content]") {
    ContentDatabase db;
    LoadReport rep;
    Json root = parse(R"({"version":1,"skills":[
        {"id":"x","name":"X","category":"physical","target":"single_enemy","power":-5}]})");
    parseSkills(root, "mem", db, rep);

    REQUIRE_FALSE(rep.ok());
    REQUIRE(db.skillCount() == 0);
}

TEST_CASE("validation: duplicate id within a file is reported", "[content]") {
    ContentDatabase db;
    LoadReport rep;
    Json root = parse(R"({"version":1,"skills":[
        {"id":"dup","name":"A","category":"physical","target":"single_enemy"},
        {"id":"dup","name":"B","category":"physical","target":"single_enemy"}]})");
    parseSkills(root, "mem", db, rep);

    REQUIRE_FALSE(rep.ok());
    REQUIRE(db.skillCount() == 1);  // first one kept, duplicate rejected
}

TEST_CASE("validation: one bad entry does not discard the good ones", "[content]") {
    ContentDatabase db;
    LoadReport rep;
    Json root = parse(R"({"version":1,"skills":[
        {"id":"good","name":"Good","category":"physical","target":"single_enemy"},
        {"category":"physical","target":"single_enemy"},
        {"id":"good2","name":"Good2","category":"heal","target":"single_ally"}]})");
    parseSkills(root, "mem", db, rep);

    REQUIRE_FALSE(rep.ok());
    REQUIRE(db.skillCount() == 2);
    REQUIRE(db.hasSkill("good"));
    REQUIRE(db.hasSkill("good2"));
}

TEST_CASE("validation: malformed top-level wrappers are reported, never crash", "[content]") {
    SECTION("not an object") {
        ContentDatabase db;
        LoadReport rep;
        parseSkills(parse(R"([1,2,3])"), "mem", db, rep);
        REQUIRE_FALSE(rep.ok());
    }
    SECTION("missing version") {
        ContentDatabase db;
        LoadReport rep;
        parseSkills(parse(R"({"skills":[]})"), "mem", db, rep);
        REQUIRE_FALSE(rep.ok());
    }
    SECTION("unsupported version") {
        ContentDatabase db;
        LoadReport rep;
        parseSkills(parse(R"({"version":999,"skills":[]})"), "mem", db, rep);
        REQUIRE_FALSE(rep.ok());
    }
    SECTION("missing array") {
        ContentDatabase db;
        LoadReport rep;
        parseSkills(parse(R"({"version":1})"), "mem", db, rep);
        REQUIRE_FALSE(rep.ok());
    }
    SECTION("array element is not an object") {
        ContentDatabase db;
        LoadReport rep;
        parseSkills(parse(R"({"version":1,"skills":[42]})"), "mem", db, rep);
        REQUIRE_FALSE(rep.ok());
        REQUIRE(db.skillCount() == 0);
    }
    SECTION("empty array is valid") {
        ContentDatabase db;
        LoadReport rep;
        parseSkills(parse(R"({"version":1,"skills":[]})"), "mem", db, rep);
        REQUIRE(rep.ok());
        REQUIRE(db.skillCount() == 0);
    }
}

TEST_CASE("validation: enemy tags and tiers parse, bad tags reported", "[content]") {
    ContentDatabase db;
    LoadReport rep;
    Json good = parse(R"({"version":1,"enemies":[
        {"id":"bat","name":"Bat","stats":{"hp":18,"attack":8,"magic":0,"defense":2,"speed":15},
         "tier":"normal","role":"disruptor","tags":["fast","poison"]}]})");
    parseEnemies(good, "mem", db, rep);
    REQUIRE(rep.ok());
    const EnemyDef* bat = db.findEnemy("bat");
    REQUIRE(bat != nullptr);
    REQUIRE(bat->tags.size() == 2);

    LoadReport rep2;
    Json bad = parse(R"({"version":1,"enemies":[
        {"id":"x","name":"X","stats":{"hp":1,"attack":0,"magic":0,"defense":0,"speed":0},
         "tags":["flying"]}]})");
    ContentDatabase db2;
    parseEnemies(bad, "mem", db2, rep2);
    REQUIRE_FALSE(rep2.ok());
    REQUIRE(db2.enemyCount() == 0);
}

TEST_CASE("validation: item semantic rules are enforced", "[content]") {
    SECTION("scroll without grantsSkill is invalid") {
        ContentDatabase db;
        LoadReport rep;
        parseItems(parse(R"({"version":1,"items":[
            {"id":"s","name":"S","type":"scroll","value":10}]})"),
                   "mem", db, rep);
        REQUIRE_FALSE(rep.ok());
        REQUIRE(db.itemCount() == 0);
    }
    SECTION("equipment without a slot is invalid") {
        ContentDatabase db;
        LoadReport rep;
        parseItems(parse(R"({"version":1,"items":[
            {"id":"e","name":"E","type":"equipment","value":10}]})"),
                   "mem", db, rep);
        REQUIRE_FALSE(rep.ok());
        REQUIRE(db.itemCount() == 0);
    }
    SECTION("valid consumable with effect loads") {
        ContentDatabase db;
        LoadReport rep;
        parseItems(parse(R"({"version":1,"items":[
            {"id":"potion","name":"Potion","type":"consumable","value":25,
             "effect":"heal","effectAmount":30}]})"),
                   "mem", db, rep);
        REQUIRE(rep.ok());
        const ItemDef* potion = db.findItem("potion");
        REQUIRE(potion != nullptr);
        REQUIRE(potion->effect == ConsumableEffect::Heal);
        REQUIRE(potion->effectAmount == 30);
    }
}

TEST_CASE("validation: bosses parse and reject bad archetypes", "[content]") {
    SECTION("valid boss") {
        ContentDatabase db;
        LoadReport rep;
        parseBosses(parse(R"({"version":1,"bosses":[
            {"id":"b","name":"Boss","archetype":"brute",
             "stats":{"hp":300,"attack":20,"magic":0,"defense":10,"speed":8}}]})"),
                    "mem", db, rep);
        REQUIRE(rep.ok());
        REQUIRE(db.bossCount() == 1);
    }
    SECTION("unknown archetype") {
        ContentDatabase db;
        LoadReport rep;
        parseBosses(parse(R"({"version":1,"bosses":[
            {"id":"b","name":"Boss","archetype":"wizard",
             "stats":{"hp":300,"attack":20,"magic":0,"defense":10,"speed":8}}]})"),
                    "mem", db, rep);
        REQUIRE_FALSE(rep.ok());
        REQUIRE(db.bossCount() == 0);
    }
    SECTION("missing archetype") {
        ContentDatabase db;
        LoadReport rep;
        parseBosses(parse(R"({"version":1,"bosses":[
            {"id":"b","name":"Boss","stats":{"hp":1,"attack":0,"magic":0,"defense":0,"speed":0}}]})"),
                    "mem", db, rep);
        REQUIRE_FALSE(rep.ok());
        REQUIRE(db.bossCount() == 0);
    }
}

TEST_CASE("validation: themes require enemy and boss pools", "[content]") {
    SECTION("valid theme") {
        ContentDatabase db;
        LoadReport rep;
        parseThemes(parse(R"({"version":1,"themes":[
            {"id":"t","name":"Theme","normalEnemies":["goblin"],"bosses":["b"]}]})"),
                    "mem", db, rep);
        REQUIRE(rep.ok());
        REQUIRE(db.themeCount() == 1);
    }
    SECTION("missing normalEnemies") {
        ContentDatabase db;
        LoadReport rep;
        parseThemes(parse(R"({"version":1,"themes":[
            {"id":"t","name":"Theme","bosses":["b"]}]})"),
                    "mem", db, rep);
        REQUIRE_FALSE(rep.ok());
        REQUIRE(db.themeCount() == 0);
    }
    SECTION("missing bosses") {
        ContentDatabase db;
        LoadReport rep;
        parseThemes(parse(R"({"version":1,"themes":[
            {"id":"t","name":"Theme","normalEnemies":["goblin"]}]})"),
                    "mem", db, rep);
        REQUIRE_FALSE(rep.ok());
        REQUIRE(db.themeCount() == 0);
    }
}
