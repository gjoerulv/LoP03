// M123 - the save slots' play-time clock and the Iron Man rules: the
// "HHH:MM:SS" formatter and its greyed hour digits, SaveSystem refusing an
// Iron Man party (manual and autosave), the escape price (gold and bag gone,
// worn gear and heirlooms kept, standing members at 1 HP / 0 MP, the fallen
// untouched), the three Iron accomplishments, and the fall notice's text.

#include <catch2/catch_test_macros.hpp>

#include <atomic>
#include <filesystem>
#include <string>

#include "content/ContentDatabase.hpp"
#include "content/ContentLoader.hpp"
#include "content/LoadReport.hpp"
#include "dungeon/DungeonModel.hpp"
#include "game/Achievements.hpp"
#include "game/Character.hpp"
#include "game/IronMan.hpp"
#include "game/Party.hpp"
#include "game/PlayTime.hpp"
#include "save/SaveSystem.hpp"

using namespace cd;

namespace {

const content::ContentDatabase& db() {
    static const content::ContentDatabase loaded = [] {
        content::ContentDatabase d;
        content::LoadReport rep;
        content::loadAll(std::filesystem::path(CRYSTAL_TEST_DATA_DIR), d, rep);
        REQUIRE(rep.ok());
        return d;
    }();
    return loaded;
}

std::filesystem::path tempDir() {
    static std::atomic<int> counter{0};
    return std::filesystem::temp_directory_path() /
           ("cd_ironman_" + std::to_string(counter.fetch_add(1)));
}

Party makeParty() {
    Party p;
    resetForNewGame(p);
    for (const char* id : {"knight", "ranger", "mage", "cleric"}) {
        const content::ClassDef* cls = db().findClass(id);
        REQUIRE(cls != nullptr);
        p.members.push_back(createCharacter(*cls, id, 5));
    }
    return p;
}

}  // namespace

TEST_CASE("m123: the slot clock is always HHH:MM:SS", "[playtime][m123]") {
    CHECK(formatSlotPlayTime(0).text == "000:00:00");
    CHECK(formatSlotPlayTime(59).text == "000:00:59");
    CHECK(formatSlotPlayTime(60).text == "000:01:00");
    CHECK(formatSlotPlayTime(3600 + 3 * 60 + 56).text == "001:03:56");  // the owner's example
    CHECK(formatSlotPlayTime(12 * 3600 + 34 * 60 + 5).text == "012:34:05");
    CHECK(formatSlotPlayTime(123 * 3600 + 45 * 60 + 6).text == "123:45:06");
    for (long long s : {0LL, 1LL, 3599LL, 3600LL, 36000LL, 360000LL, kSlotPlayTimeCap}) {
        CHECK(formatSlotPlayTime(s).text.size() == 9);
    }
}

TEST_CASE("m123: hour digits not reached yet are greyed", "[playtime][m123]") {
    CHECK(formatSlotPlayTime(0).greyChars == 3);             // 000
    CHECK(formatSlotPlayTime(3599).greyChars == 3);          // still no hour
    CHECK(formatSlotPlayTime(3600).greyChars == 2);          // 001
    CHECK(formatSlotPlayTime(9 * 3600 + 59).greyChars == 2); // 009
    CHECK(formatSlotPlayTime(10 * 3600).greyChars == 1);     // 010
    CHECK(formatSlotPlayTime(99 * 3600).greyChars == 1);     // 099
    CHECK(formatSlotPlayTime(100 * 3600).greyChars == 0);    // 100
}

TEST_CASE("m123: the slot clock clamps instead of wrapping", "[playtime][m123]") {
    CHECK(formatSlotPlayTime(-5).text == "000:00:00");
    CHECK(formatSlotPlayTime(kSlotPlayTimeCap).text == "999:59:59");
    CHECK(formatSlotPlayTime(kSlotPlayTimeCap + 1).text == "999:59:59");
    CHECK(formatSlotPlayTime(5000LL * 3600).text == "999:59:59");
    CHECK(formatSlotPlayTime(5000LL * 3600).greyChars == 0);
}

TEST_CASE("m123: a slot summary carries the party's play time", "[save][m123]") {
    const std::filesystem::path dir = tempDir();
    save::SaveSystem saves(db(), dir);
    Party p = makeParty();
    p.lifetime.explore.playSeconds = 3600 + 3 * 60 + 56;
    content::LoadReport rep;
    REQUIRE(saves.save(save::SaveSlot::Manual1, p, rep));
    const auto summary = saves.summary(save::SaveSlot::Manual1);
    REQUIRE(summary.has_value());
    CHECK(summary->playSeconds == 3600 + 3 * 60 + 56);
    CHECK(formatSlotPlayTime(summary->playSeconds).text == "001:03:56");
    std::error_code ec;
    std::filesystem::remove_all(dir, ec);
}

TEST_CASE("m123: an Iron Man party is never written to a slot", "[save][ironman][m123]") {
    const std::filesystem::path dir = tempDir();
    save::SaveSystem saves(db(), dir);
    Party p = makeParty();
    p.ironMan = true;

    content::LoadReport manual;
    CHECK_FALSE(saves.save(save::SaveSlot::Manual1, p, manual));
    CHECK_FALSE(manual.ok());
    CHECK_FALSE(saves.exists(save::SaveSlot::Manual1));

    content::LoadReport autosave;
    CHECK_FALSE(saves.autosave(p, autosave));
    CHECK_FALSE(saves.exists(save::SaveSlot::Auto));

    // The same party saves fine once it is not an Iron Man run - the flag is
    // the only thing in the way - and the flag never comes back from a load.
    p.ironMan = false;
    content::LoadReport ok;
    REQUIRE(saves.save(save::SaveSlot::Manual1, p, ok));
    Party loaded;
    loaded.ironMan = true;  // a load replaces the WHOLE object
    content::LoadReport loadRep;
    REQUIRE(saves.load(save::SaveSlot::Manual1, loaded, loadRep));
    CHECK_FALSE(loaded.ironMan);
    std::error_code ec;
    std::filesystem::remove_all(dir, ec);
}

TEST_CASE("m123: a New Game clears the Iron Man flag with everything else", "[ironman][m123]") {
    Party p = makeParty();
    p.ironMan = true;
    resetForNewGame(p);
    CHECK_FALSE(p.ironMan);
}

TEST_CASE("m123: the escape price takes the gold and the bag", "[ironman][m123]") {
    Party p = makeParty();
    p.gold = 777;
    p.inventory.add("potion", 3);
    p.inventory.add("elixir", 1);
    p.inventory.add("heirloom_emberwake", 1);  // an heirloom carried, not worn
    p.inventory.add("no_such_item", 2);        // unknown ids are forfeit too
    p.members[0].weapon = "worldbreaker_axe";  // worn gear is not in the bag
    p.members[1].equippedHeirloom = "heirloom_lastlight";
    p.restTokens = 2;
    p.legendaryTokens = 3;
    p.mapPieces = 1;
    p.ownedCurios = {"curio_a"};
    const auto lostBefore = p.lifetime.economy.goldLost;

    const ironman::EscapeForfeit lost = ironman::applyEscape(p, db());

    CHECK(lost.gold == 777);
    CHECK(lost.items == 6);
    CHECK(p.gold == 0);
    CHECK(p.lifetime.economy.goldLost == lostBefore + 777);  // a LOSS in the ledger
    CHECK(p.inventory.count("potion") == 0);
    CHECK(p.inventory.count("elixir") == 0);
    CHECK(p.inventory.count("no_such_item") == 0);
    CHECK(p.inventory.count("heirloom_emberwake") == 1);
    REQUIRE(p.inventory.stacks.size() == 1);
    CHECK(p.members[0].weapon == "worldbreaker_axe");
    CHECK(p.members[1].equippedHeirloom == "heirloom_lastlight");
    // Counters that are not bag items ride through (the plan's accepted reading).
    CHECK(p.restTokens == 2);
    CHECK(p.legendaryTokens == 3);
    CHECK(p.mapPieces == 1);
    CHECK(p.ownedCurios.size() == 1);
}

TEST_CASE("m123: the escape leaves the standing at 1 HP and 0 MP", "[ironman][m123]") {
    Party p = makeParty();
    p.members[0].hp = p.members[0].maxHp;
    p.members[0].mp = p.members[0].maxMp;
    p.members[1].hp = 1;
    p.members[1].mp = 7;
    p.members[2].hp = 0;  // fallen: stays fallen, and keeps what it had
    p.members[2].mp = 11;
    p.members[3].hp = 42;
    p.members[3].mp = 0;

    ironman::applyEscape(p, db());

    CHECK(p.members[0].hp == 1);
    CHECK(p.members[0].mp == 0);
    CHECK(p.members[1].hp == 1);
    CHECK(p.members[1].mp == 0);
    CHECK(p.members[2].hp == 0);
    CHECK(p.members[2].mp == 11);
    CHECK(p.members[3].hp == 1);
    CHECK(p.members[3].mp == 0);

    // Idempotent: the dungeon pins the vitals again after a form restore.
    ironman::clampEscapeVitals(p);
    CHECK(p.members[0].hp == 1);
    CHECK(p.members[2].hp == 0);

    // A penniless, empty-handed party pays nothing and breaks nothing.
    Party poor = makeParty();
    poor.gold = 0;
    poor.inventory.stacks.clear();
    const ironman::EscapeForfeit none = ironman::applyEscape(poor, db());
    CHECK(none.gold == 0);
    CHECK(none.items == 0);
    CHECK(poor.gold == 0);
}

TEST_CASE("m123: only heirlooms survive in the bag", "[ironman][m123]") {
    CHECK(ironman::keptOnEscape("heirloom_emberwake", db()));
    CHECK(ironman::keptOnEscape("heirloom_vowknot", db()));
    CHECK_FALSE(ironman::keptOnEscape("potion", db()));
    CHECK_FALSE(ironman::keptOnEscape("worldbreaker_axe", db()));   // unequipped gear
    CHECK_FALSE(ironman::keptOnEscape("titanforged_heart", db()));  // a relic is not an heirloom
    CHECK_FALSE(ironman::keptOnEscape("", db()));
}

TEST_CASE("m123: the Iron accomplishments need an Iron Man party", "[achievement][ironman][m123]") {
    CHECK(kAchievementCount == 23);
    REQUIRE(findAchievement("iron_crown") != nullptr);
    REQUIRE(findAchievement("iron_scales") != nullptr);
    REQUIRE(findAchievement("iron_bill") != nullptr);

    Party p = makeParty();
    p.castleRecords.kingDefeated = true;
    p.castleRecords.dragonBestTurns = 30;
    p.castleRecords.duckBestTurns = 12;
    const AchvContext ctx{};
    // The same kills in a normal game earn the ordinary trophies only.
    CHECK(achievementMet("kingslayer", p, ctx));
    CHECK(achievementMet("wyrmbane", p, ctx));
    CHECK(achievementMet("quackbane", p, ctx));
    CHECK_FALSE(achievementMet("iron_crown", p, ctx));
    CHECK_FALSE(achievementMet("iron_scales", p, ctx));
    CHECK_FALSE(achievementMet("iron_bill", p, ctx));

    p.ironMan = true;
    CHECK(achievementMet("iron_crown", p, ctx));
    CHECK(achievementMet("iron_scales", p, ctx));
    CHECK(achievementMet("iron_bill", p, ctx));
    CHECK(achievementMet("kingslayer", p, ctx));  // and the ordinary ones still count

    // Each is its own kill.
    Party fresh = makeParty();
    fresh.ironMan = true;
    CHECK_FALSE(achievementMet("iron_crown", fresh, ctx));
    fresh.castleRecords.duckBestTurns = 9;
    CHECK(achievementMet("iron_bill", fresh, ctx));
    CHECK_FALSE(achievementMet("iron_scales", fresh, ctx));
}

TEST_CASE("m123: the fall notice says where and by whom", "[ironman][m123]") {
    CHECK(ironman::fallenPlaceDungeon(3, "Ember Mine", 2, 4, false) ==
          "Town 3 - Ember Mine, floor 2 of 4");
    CHECK(ironman::fallenPlaceDungeon(1, "Old Keep", 1, 1, false) == "Town 1 - Old Keep");
    CHECK(ironman::fallenPlaceDungeon(7, "Old Keep", 9, 1, true) ==
          "Town 7 - Old Keep, Eternal floor 9");
    CHECK(ironman::fallenPlaceDungeon(2, "", 1, 1, false) == "Town 2");

    dungeon::EnemyTeam named;
    named.name = "The Gate Wardens";
    CHECK(ironman::fallenFoes(named, db()) == "The Gate Wardens");

    // An unnamed boss team reads as its boss; an unnamed pack as its leader.
    REQUIRE_FALSE(db().bosses().empty());
    const auto& [bossId, boss] = *db().bosses().begin();
    dungeon::EnemyTeam bossTeam;
    bossTeam.bossId = bossId;
    CHECK(ironman::fallenFoes(bossTeam, db()) == boss.name);

    REQUIRE_FALSE(db().enemies().empty());
    const auto& [enemyId, enemy] = *db().enemies().begin();
    dungeon::EnemyTeam solo;
    solo.enemyIds = {enemyId};
    CHECK(ironman::fallenFoes(solo, db()) == enemy.name);
    dungeon::EnemyTeam pack;
    pack.enemyIds = {enemyId, enemyId};
    CHECK(ironman::fallenFoes(pack, db()) == enemy.name + " and company");

    CHECK(ironman::fallenFoes(dungeon::EnemyTeam{}, db()) == "Something unseen");
}

TEST_CASE("m123: every Iron Man rule is spelled out before the mode is chosen",
          "[ironman][m123]") {
    std::string all;
    for (const char* rule : ironman::kRules) {
        REQUIRE(rule != nullptr);
        CHECK(std::string(rule).size() > 0);
        all += rule;
        all += ' ';
    }
    // The owner's ruling: no saves, permadeath, and the whole escape price.
    CHECK(all.find("No saves") != std::string::npos);
    CHECK(all.find("autosaves") != std::string::npos);
    CHECK(all.find("wipe ends the run") != std::string::npos);
    CHECK(all.find("ALL gold") != std::string::npos);
    CHECK(all.find("whole bag") != std::string::npos);
    CHECK(all.find("heirlooms stay") != std::string::npos);
    CHECK(all.find("1 HP and 0 MP") != std::string::npos);
    CHECK(all.find("fallen stay fallen") != std::string::npos);
    CHECK(std::string(ironman::kEscapeBody).find("ALL gold") != std::string::npos);
}
