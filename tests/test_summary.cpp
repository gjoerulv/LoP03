// M116 - the End-game Summary: the unlock and once-only flag, the pure page
// rows, the formatters and the jingle-then-loop music chain.

#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <set>
#include <string>
#include <vector>

#include "audio/AudioManager.hpp"
#include "content/ContentDatabase.hpp"
#include "content/ContentLoader.hpp"
#include "game/Cutscenes.hpp"
#include "game/Party.hpp"
#include "game/Summary.hpp"
#include "save/SaveSystem.hpp"

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

Party makeParty() {
    Party party;
    for (const char* id : {"knight", "ranger", "mage", "cleric"}) {
        const content::ClassDef* cls = db().findClass(id);
        REQUIRE(cls != nullptr);
        party.members.push_back(createCharacter(*cls, id, 20));
    }
    return party;
}

}  // namespace

TEST_CASE("summary: unlocks only once the finale's choice is recorded", "[summary]") {
    Party party = makeParty();
    CHECK_FALSE(summaryUnlocked(party));
    CHECK_FALSE(shouldAutoShowSummary(party));
    // The King's fall alone is not the story's end.
    party.castleUnlocked = true;
    game::markCutsceneSeen(party, "finale");
    CHECK_FALSE(summaryUnlocked(party));
    game::recordCutsceneChoice(party, "finale", "heirloom_keepsakeknife");
    CHECK(summaryUnlocked(party));
    CHECK(shouldAutoShowSummary(party));
    party.summaryShown = true;
    CHECK(summaryUnlocked(party));
    CHECK_FALSE(shouldAutoShowSummary(party));  // once, then only through P
}

TEST_CASE("summary: the once-only flag survives a save and load", "[summary][save]") {
    const std::filesystem::path dir =
        std::filesystem::temp_directory_path() / "cd_summary_test";
    std::filesystem::create_directories(dir);
    Party party = makeParty();
    game::recordCutsceneChoice(party, "finale", "heirloom_hearthstone");
    party.summaryShown = true;
    const save::SaveSystem saves(db(), dir);
    content::LoadReport rep;
    REQUIRE(saves.save(save::SaveSlot::Manual1, party, rep));
    Party loaded;
    content::LoadReport rep2;
    REQUIRE(saves.load(save::SaveSlot::Manual1, loaded, rep2));
    CHECK(loaded.summaryShown);
    CHECK_FALSE(shouldAutoShowSummary(loaded));  // a reload never re-shows
    // A fresh party (and an old save without the field) starts unshown.
    CHECK_FALSE(Party{}.summaryShown);
}

TEST_CASE("summary: the formatters", "[summary]") {
    CHECK(formatCount(0) == "0");
    CHECK(formatCount(999) == "999");
    CHECK(formatCount(1000) == "1,000");
    CHECK(formatCount(1234567) == "1,234,567");
    CHECK(formatCount(9999999999LL) == "9,999,999,999");
    CHECK(formatPlayTime(0) == "0h 00m");
    CHECK(formatPlayTime(59) == "0h 00m");
    CHECK(formatPlayTime(3725) == "1h 02m");
    CHECK(formatPlayTime(100 * 3600 + 59 * 60) == "100h 59m");
}

TEST_CASE("summary: the page cycle wraps both ways", "[summary]") {
    CHECK(summaryPageAt(0) == SummaryPage::Overview);
    CHECK(summaryPageAt(5) == SummaryPage::Patrols);
    CHECK(summaryPageAt(6) == SummaryPage::Overview);
    CHECK(summaryPageAt(-1) == SummaryPage::Patrols);
    std::set<std::string> names;
    for (int i = 0; i < kSummaryPageCount; ++i) {
        names.insert(summaryPageName(summaryPageAt(i)));
    }
    CHECK(names.size() == 6);
}

TEST_CASE("summary: every page builds pure rows that fit the list", "[summary]") {
    Party party = makeParty();
    party.members[0].name = "Wolfgangheim";  // the longest allowed name
    LifetimeStats& L = party.lifetime;
    L.combat.battlesWon = 1234567;
    L.combat.highestHit = 9999999;
    L.combat.highestHitMember = 0;
    L.explore.playSeconds = 999LL * 3600 + 59 * 60;
    L.explore.runsAttempted = 7654321;
    L.explore.runsCompleted = 1234567;
    L.economy.goldEarned = 99999999;
    for (auto& t : L.towns) {
        t.bestScore = 9999999;
    }
    for (auto& m : L.members) {
        m.damageDealt = 9999999;
    }
    L.patrols.total = 9999999;
    for (int i = 0; i < kSummaryPageCount; ++i) {
        const std::vector<SummaryRow> rows = summaryRows(summaryPageAt(i), party, db());
        INFO(summaryPageName(summaryPageAt(i)));
        CHECK_FALSE(rows.empty());
        for (const SummaryRow& r : rows) {
            INFO(r.label << " = " << r.value);
            CHECK_FALSE(r.label.empty());
            if (r.header) {
                CHECK(r.value.empty());
            }
            // The list's label column and value column budgets (chars at the
            // body size; the capture lint is the pixel referee).
            CHECK(r.label.size() <= 40);
            CHECK(r.value.size() <= 26);
        }
    }
    // The Overview names the biggest hitter.
    const std::vector<SummaryRow> overview = summaryRows(SummaryPage::Overview, party, db());
    bool named = false;
    for (const SummaryRow& r : overview) {
        if (r.label == "Biggest hit") {
            named = r.value == "9,999,999 (Wolfgangheim)";
        }
    }
    CHECK(named);
    // The migration note leads the Overview only for a migrated ledger.
    CHECK(overview.front().label != "Lifetime tracking began with this version");
    L.migrated = true;
    const std::vector<SummaryRow> migrated = summaryRows(SummaryPage::Overview, party, db());
    CHECK(migrated.front().header);
    CHECK(migrated.front().label == "Lifetime tracking began with this version");
    // Heroes: one header per member, and exactly the eleven ledger rows plus
    // the live "skills known" row under each.
    const std::vector<SummaryRow> heroes = summaryRows(SummaryPage::Heroes, party, db());
    int headers = 0;
    for (const SummaryRow& r : heroes) {
        headers += r.header ? 1 : 0;
    }
    CHECK(headers == 4);
    CHECK(heroes.size() == 4 * 13);
}

TEST_CASE("summary: the bestiary page never marks an unmet foe", "[summary]") {
    Party party = makeParty();
    party.encountered = {"goblin_grunt", "keep_warden"};
    party.lifetime.defeats["goblin_grunt"] = 42;
    const std::vector<SummaryRow> rows = summaryRows(SummaryPage::Bestiary, party, db());
    CHECK(rows.size() == db().enemyCount() + db().bossCount());
    int known = 0;
    int unknown = 0;
    bool grunt = false;
    bool warden = false;
    for (const SummaryRow& r : rows) {
        if (r.label == "? ? ?") {
            ++unknown;
            CHECK(r.value.empty());
        } else {
            ++known;
            CHECK(r.value.rfind("Defeated: ", 0) == 0);
            grunt = grunt || (r.label == db().findEnemy("goblin_grunt")->name && r.value == "Defeated: 42");
            warden = warden || (r.label == db().findBoss("keep_warden")->name && r.value == "Defeated: 0");
        }
    }
    CHECK(known == 2);
    CHECK(unknown == static_cast<int>(rows.size()) - 2);
    CHECK(grunt);
    CHECK(warden);
    // Building the rows marks nothing.
    CHECK(party.encountered.size() == 2);
}

TEST_CASE("summary: the jingle-then-loop chain, headless", "[summary][audio]") {
    // No audio device in the test host: the manager tracks the intent. A
    // chain lands on its loop; a plain setMusic clears a stale chain.
    AudioManager audio;
    audio.setMusicThen(MusicTrack::Victory, MusicTrack::Result);
    CHECK(audio.currentMusic() == MusicTrack::Result);
    audio.setMusic(MusicTrack::Town);
    CHECK(audio.currentMusic() == MusicTrack::Town);
    audio.setMusicThen(MusicTrack::Mock, MusicTrack::Result);
    CHECK(audio.currentMusic() == MusicTrack::Result);
    // A loop as the "jingle" is refused: the next track plays outright.
    audio.setMusicThen(MusicTrack::Battle, MusicTrack::Castle);
    CHECK(audio.currentMusic() == MusicTrack::Castle);
}
