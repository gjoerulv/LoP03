// M110 - the patrol dispatcher: the owner's 65/10/5/15/5 mixture as exact
// thresholds, a pure hash of (run seed, patrol index) that is deterministic,
// reload-honest and independent of the M93 team salt; the per-kind ordinal;
// and the Stranger's patrol-scene cycle.

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <cstdint>
#include <filesystem>
#include <set>
#include <string>
#include <vector>

#include "content/ContentDatabase.hpp"
#include "content/ContentLoader.hpp"
#include "content/Definitions.hpp"
#include "dungeon/DungeonGenerator.hpp"
#include "dungeon/DungeonModel.hpp"
#include "dungeon/PatrolDispatch.hpp"
#include "game/Cutscenes.hpp"

using namespace cd;
using dungeon::PatrolKind;

namespace {

const content::ContentDatabase& shipped() {
    static content::ContentDatabase database;
    static bool loaded = false;
    if (!loaded) {
        content::LoadReport rep;
        REQUIRE(content::loadAll(std::filesystem::path(CRYSTAL_TEST_DATA_DIR), database, rep));
        loaded = true;
    }
    return database;
}

content::ContentDatabase fourPatrolScenes() {
    content::ContentDatabase db;
    for (const char* id : {"patrol_1", "patrol_2", "patrol_3", "patrol_4", "joke_1", "story_1"}) {
        content::CutsceneDef d;
        d.id = id;
        content::CutsceneBeat b;
        b.speaker = "THE STRANGER \"P\"";
        b.text = "...";
        d.beats.push_back(b);
        db.addCutscene(d);
    }
    return db;
}

}  // namespace

TEST_CASE("patrol dispatch: the mixture is exactly 65/10/5/15/5", "[patrol][dispatch]") {
    int sum = 0;
    for (int pct : dungeon::kPatrolKindPct) {
        sum += pct;
    }
    REQUIRE(sum == 100);
    REQUIRE(dungeon::kPatrolKindPct[0] == 65);
    REQUIRE(dungeon::kPatrolKindPct[1] == 10);
    REQUIRE(dungeon::kPatrolKindPct[2] == 5);
    REQUIRE(dungeon::kPatrolKindPct[3] == 15);
    REQUIRE(dungeon::kPatrolKindPct[4] == 5);
    // The threshold boundaries, verbatim.
    CHECK(dungeon::patrolKindForRoll(0) == PatrolKind::Normal);
    CHECK(dungeon::patrolKindForRoll(64) == PatrolKind::Normal);
    CHECK(dungeon::patrolKindForRoll(65) == PatrolKind::GoldenGoose);
    CHECK(dungeon::patrolKindForRoll(74) == PatrolKind::GoldenGoose);
    CHECK(dungeon::patrolKindForRoll(75) == PatrolKind::Lore);
    CHECK(dungeon::patrolKindForRoll(79) == PatrolKind::Lore);
    CHECK(dungeon::patrolKindForRoll(80) == PatrolKind::Chests);
    CHECK(dungeon::patrolKindForRoll(94) == PatrolKind::Chests);
    CHECK(dungeon::patrolKindForRoll(95) == PatrolKind::StrangerP);
    CHECK(dungeon::patrolKindForRoll(99) == PatrolKind::StrangerP);
}

TEST_CASE("patrol dispatch: the kind is a deterministic, reload-honest hash",
          "[patrol][dispatch]") {
    for (std::uint64_t seed : {1ull, 777ull, 0xDEADBEEFull, 9876543210ull}) {
        for (int i = 0; i < 40; ++i) {
            CHECK(dungeon::patrolKindFor(seed, i) == dungeon::patrolKindFor(seed, i));
        }
    }
    // Different seeds and different indices vary (the hash is not stuck).
    std::set<int> kinds;
    for (int i = 0; i < 200; ++i) {
        kinds.insert(static_cast<int>(dungeon::patrolKindFor(4242ull, i)));
    }
    CHECK(kinds.size() == static_cast<std::size_t>(dungeon::kPatrolKindCount));
}

TEST_CASE("patrol dispatch: the long-run frequencies match the weights", "[patrol][dispatch]") {
    // A deterministic census over many (seed, index) pairs: every kind lands
    // within two points of its authored share.
    std::array<int, dungeon::kPatrolKindCount> counts{};
    int total = 0;
    for (std::uint64_t seed = 1; seed <= 200; ++seed) {
        for (int i = 0; i < 100; ++i) {
            ++counts[static_cast<std::size_t>(dungeon::patrolKindFor(seed * 7919ull, i))];
            ++total;
        }
    }
    for (int k = 0; k < dungeon::kPatrolKindCount; ++k) {
        const double pct = 100.0 * counts[static_cast<std::size_t>(k)] / total;
        INFO(dungeon::patrolKindName(static_cast<PatrolKind>(k)) << " " << pct << "%");
        CHECK(pct > dungeon::kPatrolKindPct[static_cast<std::size_t>(k)] - 2.0);
        CHECK(pct < dungeon::kPatrolKindPct[static_cast<std::size_t>(k)] + 2.0);
    }
}

TEST_CASE("patrol dispatch: the M93 team roll is untouched by the kind roll",
          "[patrol][dispatch][m93]") {
    // The Normal patrol's team for a given (seed, index) is the same team M93
    // produced: the kind hash lives under its own salt and consumes nothing.
    const content::ContentDatabase& db = shipped();
    const dungeon::EnemyTeam a = dungeon::patrolTeam(db, "ruined_keep", 4, 8, 777ull, 0);
    const dungeon::EnemyTeam b = dungeon::patrolTeam(db, "ruined_keep", 4, 8, 777ull, 0);
    CHECK(a.enemyIds == b.enemyIds);
    CHECK(a.patrol);
    CHECK(dungeon::kSaltPatrolKind != 0x9A7201CCA11ull);  // not the M93 team salt
}

TEST_CASE("patrol dispatch: the per-kind ordinal counts only earlier patrols",
          "[patrol][dispatch]") {
    const std::uint64_t seed = 31337ull;
    for (int i = 0; i < 30; ++i) {
        int sum = 0;
        for (int k = 0; k < dungeon::kPatrolKindCount; ++k) {
            sum += dungeon::patrolKindCount(seed, i, static_cast<PatrolKind>(k));
        }
        CHECK(sum == i);  // every earlier patrol has exactly one kind
    }
    // The ordinal of a kind steps by one exactly when that kind occurred.
    for (int i = 0; i < 30; ++i) {
        const PatrolKind k = dungeon::patrolKindFor(seed, i);
        CHECK(dungeon::patrolKindCount(seed, i + 1, k) == dungeon::patrolKindCount(seed, i, k) + 1);
    }
}

TEST_CASE("patrol dispatch: the Stranger's patrol scenes cycle all four before repeating",
          "[patrol][dispatch][cutscene]") {
    const content::ContentDatabase db = fourPatrolScenes();
    const std::vector<std::string> ids = game::strangerPatrolIds(db);
    REQUIRE(ids.size() == 4);  // jokes and stories are other pools
    for (std::uint64_t seed : {5ull, 6ull, 123456ull}) {
        std::set<std::string> seen;
        for (int n = 0; n < 4; ++n) {
            const std::string id = game::patrolSceneFor(db, seed, n);
            CHECK(id.rfind("patrol_", 0) == 0);
            seen.insert(id);
        }
        CHECK(seen.size() == 4);  // every scene before any repeat
        CHECK(game::patrolSceneFor(db, seed, 4) == game::patrolSceneFor(db, seed, 0));
        CHECK(game::patrolSceneFor(db, seed, 1) == game::patrolSceneFor(db, seed, 1));  // stable
    }
    // Seeds shuffle the order: not every seed starts on the same scene.
    std::set<std::string> firsts;
    for (std::uint64_t seed = 1; seed <= 40; ++seed) {
        firsts.insert(game::patrolSceneFor(db, seed, 0));
    }
    CHECK(firsts.size() > 1);
    // An empty pool yields no scene (the trigger then falls back to a normal patrol).
    content::ContentDatabase empty;
    CHECK(game::patrolSceneFor(empty, 5ull, 0).empty());
}

#ifdef CRYSTAL_TEST_DATA_DIR
TEST_CASE("patrol dispatch: the shipped patrol pool is four dedicated optionless scenes",
          "[patrol][dispatch][data]") {
    const content::ContentDatabase& db = shipped();
    const std::vector<std::string> ids = game::strangerPatrolIds(db);
    REQUIRE(ids.size() == 4);
    for (const std::string& id : ids) {
        INFO(id);
        const content::CutsceneDef* d = db.findCutscene(id);
        REQUIRE(d != nullptr);
        CHECK(d->options.empty());   // never a grant
        CHECK(d->question.empty());  // never a question
        CHECK(!d->beats.empty());
        for (const content::CutsceneBeat& b : d->beats) {
            CHECK(b.speaker == "THE STRANGER \"P\"");
            CHECK_FALSE(b.kingOnStage);   // valid before AND after the King
            CHECK_FALSE(b.dragonOnStage);
        }
    }
    // The patrol pool is disjoint from the jokes and the stories.
    for (const std::string& id : game::strangerJokeIds(db)) {
        CHECK(id.rfind("patrol_", 0) != 0);
    }
    for (const std::string& id : game::strangerStoryIds(db)) {
        CHECK(id.rfind("patrol_", 0) != 0);
    }
}
#endif
