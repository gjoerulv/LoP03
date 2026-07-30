#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <atomic>
#include <filesystem>
#include <set>
#include <string>

#include "content/LoadReport.hpp"
#include "game/Achievements.hpp"
#include "game/Party.hpp"
#include "game/RunStats.hpp"

// M42 achievements + run stats: the roster is well-formed, predicates read the
// right state, the global store round-trips and reports newly-unlocked ids, and
// RunStats picks an MVP. Presentation/persistence only.

using namespace cd;

namespace {
std::filesystem::path tempDir() {
    static std::atomic<int> counter{0};
    return std::filesystem::temp_directory_path() /
           ("cd_achv_" + std::to_string(counter.fetch_add(1)));
}
}  // namespace

TEST_CASE("achievements: the roster is well-formed", "[achievement]") {
    CHECK(kAchievementCount >= 15);
    CHECK(kAchievementCount <= 20);
    std::set<std::string> ids;
    for (const AchievementDef& a : kAchievements) {
        INFO(a.id);
        CHECK(ids.insert(a.id).second);  // unique
        CHECK(std::string(a.name).size() > 0);
        CHECK(std::string(a.description).size() > 0);
        CHECK(findAchievement(a.id) == &a);
    }
    CHECK(findAchievement("no_such") == nullptr);
}

TEST_CASE("achievements: predicates read party + run state", "[achievement]") {
    Party p;
    AchvContext none;
    // Nothing done -> nothing met.
    CHECK_FALSE(achievementMet("first_clear", p, none));
    CHECK_FALSE(achievementMet("ladders_end", p, none));
    CHECK_FALSE(achievementMet("kingslayer", p, none));

    // Run-quality achievements need the run event.
    AchvContext run;
    run.clearedDungeon = true;
    run.runNoDeath = true;
    run.runTurns = 15;
    run.runDepth = 12;
    CHECK(achievementMet("first_clear", p, run));
    CHECK(achievementMet("untouchable", p, run));
    CHECK(achievementMet("decisive", p, run));   // 15 <= 20
    CHECK(achievementMet("deep_diver", p, run));  // depth 12 >= 10
    run.runTurns = 21;
    CHECK_FALSE(achievementMet("decisive", p, run));

    // State-based achievements.
    p.highestUnlockedTown = 7;
    CHECK(achievementMet("trailblazer", p, none));
    CHECK(achievementMet("ladders_end", p, none));
    p.castleUnlocked = true;
    CHECK(achievementMet("the_climb", p, none));
    p.castleRecords.kingDefeated = true;
    p.castleRecords.kingTitle = "Champion Title";
    p.castleRecords.kingBestTurns = 12;  // M53: Champion reads this, not the title
    p.castleRecords.bossRushBestTurns = 40;
    p.castleRecords.endlessBestWave = 12;
    CHECK(achievementMet("kingslayer", p, none));
    CHECK(achievementMet("champion", p, none));  // 12 <= kChampionKingTurns
    CHECK(achievementMet("gauntlet", p, none));
    CHECK(achievementMet("the_long_night", p, none));
    p.storyMet = 0x7F;
    CHECK(achievementMet("loremaster", p, none));
    for (int i = 0; i < 30; ++i) {
        p.encountered.push_back("foe_" + std::to_string(i));
    }
    CHECK(achievementMet("naturalist", p, none));
}

TEST_CASE("achievements: Champion is an efficiency bar, not a King kill", "[achievement]") {
    // M53: Champion = "beat the King in kChampionKingTurns turns or fewer",
    // read from castleRecords.kingBestTurns; the earned title no longer gates it.
    Party p;
    AchvContext none;
    // Never fought the King (best 0) -> not met, even with a title present (an
    // old save could carry a title; the predicate ignores it now).
    p.castleRecords.kingTitle = "Some Old Title";
    CHECK_FALSE(achievementMet("champion", p, none));
    // Exactly at the bar -> met (retro-unlock for a save already this efficient).
    p.castleRecords.kingBestTurns = kChampionKingTurns;
    CHECK(achievementMet("champion", p, none));
    // Comfortably under the bar -> met.
    p.castleRecords.kingBestTurns = 1;
    CHECK(achievementMet("champion", p, none));
    // One turn over the bar -> not met (a slow King kill is Kingslayer, not
    // Champion).
    p.castleRecords.kingBestTurns = kChampionKingTurns + 1;
    CHECK_FALSE(achievementMet("champion", p, none));
}

TEST_CASE("achievements: the global store round-trips and reports new unlocks",
          "[achievement]") {
    const std::string text = serializeAchievements({"kingslayer", "loremaster"});
    std::set<std::string> parsed;
    content::LoadReport rep;
    REQUIRE(parseAchievementsText(text, parsed, rep));
    CHECK(parsed.count("kingslayer") == 1);
    CHECK(parsed.count("loremaster") == 1);
    // Malformed / foreign version -> fresh (empty) + reported.
    std::set<std::string> bad;
    content::LoadReport rep2;
    CHECK_FALSE(parseAchievementsText("{not json", bad, rep2));
    CHECK(bad.empty());

    const std::filesystem::path dir = tempDir();
    std::filesystem::create_directories(dir);
    {
        AchievementStore store(dir / "achievements.json");
        content::LoadReport r;
        REQUIRE(store.load(r));  // missing file -> fresh
        Party p;
        p.highestUnlockedTown = 7;  // trailblazer + ladders_end now met
        const std::vector<std::string> newly = store.check(p, AchvContext{});
        CHECK(std::find(newly.begin(), newly.end(), "ladders_end") != newly.end());
        CHECK(store.isUnlocked("ladders_end"));
        // A second check with the same state unlocks nothing new (no re-toast).
        CHECK(store.check(p, AchvContext{}).empty());
    }
    {
        // A fresh store over the same file loads what was saved.
        AchievementStore reloaded(dir / "achievements.json");
        content::LoadReport r;
        REQUIRE(reloaded.load(r));
        CHECK(reloaded.isUnlocked("ladders_end"));
        CHECK_FALSE(reloaded.isUnlocked("kingslayer"));
    }
    std::filesystem::remove_all(dir);
}

TEST_CASE("run stats: MVP is the member who dealt the most damage", "[runstats]") {
    RunStats s;
    CHECK(s.mvpMember() == -1);  // no damage yet
    s.damageByMember[0] = 100;
    s.damageByMember[2] = 250;
    s.damageByMember[1] = 80;
    CHECK(s.mvpMember() == 2);
    s.totalDamage = 430;
    s.biggestHit = 180;
    CHECK(s.totalDamage == 430);
    CHECK(s.biggestHit == 180);
}
