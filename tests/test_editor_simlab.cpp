// M60 — the sim lab's pure model: opponent building, sweep determinism and
// aggregation math, report rendering, and the test-runner filter builder.

#include <catch2/catch_test_macros.hpp>

#include <filesystem>

#include "content/ContentDatabase.hpp"
#include "content/ContentLoader.hpp"
#include "dungeon/DungeonModel.hpp"
#include "editor/SimLab.hpp"
#include "editor/TestRunner.hpp"
#include "game/Castle.hpp"

namespace {

using namespace cd;

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

editor::SimLabConfig baseConfig() {
    editor::SimLabConfig config;
    config.level = 12;
    for (const char* id : {"knight", "ranger", "mage", "cleric"}) {
        config.members.push_back(editor::presetMember(db(), id, editor::GearTier::Median));
    }
    config.mode = editor::OpponentMode::Manual;
    config.enemyIds = {"goblin_grunt", "skeleton_archer", "mire_imp"};
    config.statScalePct = 100;
    config.seeds = 10;
    return config;
}

}  // namespace

TEST_CASE("simlab: opponent building per mode", "[editor]") {
    std::string error;
    editor::SimLabConfig config = baseConfig();
    const dungeon::EnemyTeam manual = editor::buildOpponent(config, db(), error);
    REQUIRE(error.empty());
    REQUIRE(manual.enemyIds.size() == 3);
    REQUIRE(manual.statScalePct == 100);

    config.mode = editor::OpponentMode::King;
    const dungeon::EnemyTeam king = editor::buildOpponent(config, db(), error);
    REQUIRE(error.empty());
    REQUIRE(king.isBoss);
    REQUIRE(king.bossId == kKingBossId);

    config.mode = editor::OpponentMode::Manual;
    config.enemyIds = {"no_such_enemy"};
    editor::buildOpponent(config, db(), error);
    REQUIRE_FALSE(error.empty());
}

TEST_CASE("simlab: a sweep is deterministic and its aggregates are sound", "[editor]") {
    const editor::SimLabConfig config = baseConfig();
    const editor::SimLabResult a = editor::runSweep(config, db());
    const editor::SimLabResult b = editor::runSweep(config, db());
    REQUIRE(a.ok);
    REQUIRE(b.ok);
    REQUIRE(a.runs == 10);
    REQUIRE(a.wins == b.wins);
    REQUIRE(a.avgRounds == b.avgRounds);
    REQUIRE(a.medianRounds == b.medianRounds);
    REQUIRE(a.partyKos == b.partyKos);
    REQUIRE(a.telemetry.eventCount() == b.telemetry.eventCount());

    // Aggregate sanity.
    REQUIRE(a.wins + a.defeats + a.stalls == a.runs);
    REQUIRE(a.minRounds <= a.medianRounds);
    REQUIRE(a.medianRounds <= a.maxRounds);
    REQUIRE(a.avgRounds >= a.minRounds);
    REQUIRE(a.avgRounds <= a.maxRounds);
    REQUIRE(a.avgHpFraction >= 0.0);
    REQUIRE(a.avgHpFraction <= 1.0);
    REQUIRE_FALSE(a.dangerTier.empty());
    // A mid-level median-geared party crushes the town-1 openers.
    REQUIRE(a.wins == a.runs);
    // Telemetry accumulated across runs (at least one action per run).
    REQUIRE(a.telemetry.eventCount() > a.runs);
}

TEST_CASE("simlab: reports render and carry deltas", "[editor]") {
    editor::SimLabConfig config = baseConfig();
    const editor::SimLabResult first = editor::runSweep(config, db());
    config.statScalePct = 200;
    const editor::SimLabResult second = editor::runSweep(config, db());
    REQUIRE(first.ok);
    REQUIRE(second.ok);

    const std::string md = editor::reportMarkdown(config, second, &first);
    REQUIRE(md.find("win rate %") != std::string::npos);
    REQUIRE(md.find("delta") != std::string::npos);
    REQUIRE(md.find("## Actions") != std::string::npos);
    REQUIRE(md.find("## Combatants") != std::string::npos);

    const std::string csv = editor::reportCsv(config, second);
    REQUIRE(csv.find("win_rate_pct,") != std::string::npos);
    REQUIRE(csv.find("action,uses,damage,healing") != std::string::npos);
}

TEST_CASE("simlab: an empty party or team fails cleanly", "[editor]") {
    editor::SimLabConfig config;
    config.seeds = 10;
    const editor::SimLabResult result = editor::runSweep(config, db());
    REQUIRE_FALSE(result.ok);
    REQUIRE_FALSE(result.error.empty());
}

TEST_CASE("testrunner: category specs build correct Catch2 filters", "[editor]") {
    const std::vector<editor::TestCategory>& cats = editor::testCategories();
    REQUIRE(cats.size() >= 6);
    REQUIRE(cats.back().testFiles.empty());  // "Everything"

    const std::string spec = editor::catch2Spec(cats.front());
    REQUIRE(spec.find("[#test_balance]") != std::string::npos);
    REQUIRE(spec.find(",") != std::string::npos);

    const std::vector<std::string> args = editor::testInvocation(cats.front());
    REQUIRE(args.size() == 2);
    REQUIRE(args.back() == "--filenames-as-tags");

    const std::vector<std::string> all = editor::testInvocation(cats.back());
    REQUIRE(all.size() == 1);  // no spec: run everything
    REQUIRE(all.front() == "--filenames-as-tags");
}
