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

    content::ItemDef potion;
    potion.id = "potion";
    potion.name = "Potion";
    potion.type = content::ItemType::Consumable;
    db.addItem(potion);

    content::ItemDef sword;
    sword.id = "iron_sword";
    sword.name = "Iron Sword";
    sword.type = content::ItemType::Equipment;
    sword.slot = content::EquipSlot::Weapon;
    sword.statBonus = {0, 6, 0, 0, 0};
    db.addItem(sword);

    content::PassiveDef evasion;  // M36
    evasion.id = "evasion";
    evasion.name = "Evasion";
    evasion.hook = content::PassiveHook::Evasion;
    evasion.magnitude = 25;
    evasion.price = 1000;
    db.addPassive(evasion);
    content::PassiveDef thorns;
    thorns.id = "thorns";
    thorns.name = "Thorns";
    thorns.hook = content::PassiveHook::Thorns;
    thorns.magnitude = 20;
    db.addPassive(thorns);
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

TEST_CASE("save: inventory round-trips; missing inventory loads empty", "[save]") {
    const content::ContentDatabase db = makeDb();
    const fs::path dir = makeTempDir();
    const save::SaveSystem saves(db, dir);

    Party p;
    p.members.push_back(createCharacter(*db.findClass("knight"), "Rolan"));
    p.inventory.add("potion", 4);
    p.restTokens = 3;  // M30

    content::LoadReport rep;
    REQUIRE(saves.save(save::SaveSlot::Manual1, p, rep));

    Party loaded;
    content::LoadReport rep2;
    REQUIRE(saves.load(save::SaveSlot::Manual1, loaded, rep2));
    REQUIRE(loaded.inventory.count("potion") == 4);
    REQUIRE(loaded.restTokens == 3);  // M30 round-trips

    // Backward compatibility: a save with no inventory/restTokens field still
    // loads (empty inventory, zero tokens).
    writeFile(saves.slotPath(save::SaveSlot::Manual2),
              R"({"version":1,"gold":0,"party":[
                 {"classId":"knight","name":"X","level":1,"xp":0,"hp":120,"mp":4}]})");
    Party old;
    content::LoadReport rep3;
    REQUIRE(saves.load(save::SaveSlot::Manual2, old, rep3));
    REQUIRE(old.inventory.empty());
    REQUIRE(old.restTokens == 0);  // M30 absent -> 0

    fs::remove_all(dir);
}

TEST_CASE("save: passives round-trip and drop unknown/unowned ids (M36)", "[save]") {
    const content::ContentDatabase db = makeDb();
    const fs::path dir = makeTempDir();
    const save::SaveSystem saves(db, dir);

    Party p;
    Character c = createCharacter(*db.findClass("knight"), "Rolan");
    c.ownedPassives = {"evasion", "thorns", "ghost_passive"};  // last is unknown -> dropped
    c.equippedPassive = "thorns";
    p.members.push_back(c);

    content::LoadReport rep;
    REQUIRE(saves.save(save::SaveSlot::Manual1, p, rep));
    Party loaded;
    content::LoadReport rep2;
    REQUIRE(saves.load(save::SaveSlot::Manual1, loaded, rep2));
    REQUIRE(loaded.members.size() == 1);
    const Character& lc = loaded.members[0];
    REQUIRE(lc.ownedPassives.size() == 2);  // the unknown id was dropped
    CHECK(std::find(lc.ownedPassives.begin(), lc.ownedPassives.end(), "evasion") !=
          lc.ownedPassives.end());
    CHECK(std::find(lc.ownedPassives.begin(), lc.ownedPassives.end(), "thorns") !=
          lc.ownedPassives.end());
    CHECK(lc.equippedPassive == "thorns");

    // An equipped id that is not owned is dropped on load.
    Party p2;
    Character c2 = createCharacter(*db.findClass("knight"), "Mara");
    c2.ownedPassives = {"evasion"};
    c2.equippedPassive = "thorns";  // not owned
    p2.members.push_back(c2);
    content::LoadReport repB;
    REQUIRE(saves.save(save::SaveSlot::Manual2, p2, repB));
    Party loaded2;
    content::LoadReport repB2;
    REQUIRE(saves.load(save::SaveSlot::Manual2, loaded2, repB2));
    CHECK(loaded2.members[0].equippedPassive.empty());  // unowned equip dropped

    // Backward compatibility: a pre-M36 save with no passive fields loads empty.
    writeFile(saves.slotPath(save::SaveSlot::Manual3),
              R"({"version":1,"gold":0,"party":[
                 {"classId":"knight","name":"X","level":1,"xp":0,"hp":120,"mp":4}]})");
    Party legacy;
    content::LoadReport rep3;
    REQUIRE(saves.load(save::SaveSlot::Manual3, legacy, rep3));
    REQUIRE(legacy.members.size() == 1);
    CHECK(legacy.members[0].ownedPassives.empty());
    CHECK(legacy.members[0].equippedPassive.empty());

    fs::remove_all(dir);
}

TEST_CASE("save: town-ladder fields round-trip and default/clamp safely (M32)", "[save]") {
    const content::ContentDatabase db = makeDb();
    const fs::path dir = makeTempDir();
    const save::SaveSystem saves(db, dir);

    Party p;
    p.members.push_back(createCharacter(*db.findClass("knight"), "Rolan"));
    p.currentTown = 3;
    p.highestUnlockedTown = 5;
    p.stakes = {4, 7, 3};  // M33: prev stakes (town 4, depth 7), 3 penalty steps
    p.legendaryTokens = 4;  // M34
    p.blackMarket = {true, 3, "iron_sword", 5750, 18, 5};  // M34 offer on a current plaza tile

    content::LoadReport rep;
    REQUIRE(saves.save(save::SaveSlot::Manual1, p, rep));
    Party loaded;
    content::LoadReport rep2;
    REQUIRE(saves.load(save::SaveSlot::Manual1, loaded, rep2));
    REQUIRE(loaded.currentTown == 3);
    REQUIRE(loaded.highestUnlockedTown == 5);
    REQUIRE(loaded.stakes.prevTown == 4);       // M33 round-trips
    REQUIRE(loaded.stakes.prevDepth == 7);
    REQUIRE(loaded.stakes.penaltySteps == 3);
    REQUIRE(loaded.legendaryTokens == 4);       // M34 round-trips
    REQUIRE(loaded.blackMarket.present);
    REQUIRE(loaded.blackMarket.town == 3);
    REQUIRE(loaded.blackMarket.itemId == "iron_sword");
    REQUIRE(loaded.blackMarket.priceGold == 5750);
    REQUIRE(loaded.blackMarket.tileX == 18);  // a current plaza tile round-trips exactly
    REQUIRE(loaded.blackMarket.tileY == 5);

    // Defensive: an offer whose item the content no longer knows is dropped.
    Party stale;
    stale.members.push_back(createCharacter(*db.findClass("knight"), "Rolan"));
    stale.blackMarket = {true, 2, "does_not_exist", 5000, 5, 7};
    content::LoadReport repS;
    REQUIRE(saves.save(save::SaveSlot::Manual3, stale, repS));
    Party staleLoaded;
    content::LoadReport repS2;
    REQUIRE(saves.load(save::SaveSlot::Manual3, staleLoaded, repS2));
    REQUIRE_FALSE(staleLoaded.blackMarket.present);  // dropped, not dangling

    // M50 defensive: a present offer saved on a tile the compact town no longer
    // has (e.g. an old 26x15 plaza cell) snaps onto a current plaza tile on load,
    // so the NPC stays reachable rather than stranded on a wall.
    Party oldTile;
    oldTile.members.push_back(createCharacter(*db.findClass("knight"), "Mira"));
    oldTile.blackMarket = {true, 3, "iron_sword", 5750, 20, 7};  // a pre-M50 tile
    content::LoadReport repO;
    REQUIRE(saves.save(save::SaveSlot::Manual3, oldTile, repO));
    Party oldLoaded;
    content::LoadReport repO2;
    REQUIRE(saves.load(save::SaveSlot::Manual3, oldLoaded, repO2));
    REQUIRE(oldLoaded.blackMarket.present);
    REQUIRE(oldLoaded.blackMarket.tileX == kBlackMarketTiles[0].x);
    REQUIRE(oldLoaded.blackMarket.tileY == kBlackMarketTiles[0].y);

    // Backward compatibility: a pre-M32/M33 save with no town/stakes fields loads
    // as 1/1 and a fresh (zero) stakes state.
    writeFile(saves.slotPath(save::SaveSlot::Manual2),
              R"({"version":1,"gold":0,"party":[
                 {"classId":"knight","name":"X","level":1,"xp":0,"hp":120,"mp":4}]})");
    Party legacy;
    content::LoadReport rep3;
    REQUIRE(saves.load(save::SaveSlot::Manual2, legacy, rep3));
    REQUIRE(legacy.currentTown == 1);
    REQUIRE(legacy.highestUnlockedTown == 1);
    REQUIRE(legacy.stakes.prevTown == 0);
    REQUIRE(legacy.stakes.penaltySteps == 0);
    REQUIRE(legacy.legendaryTokens == 0);       // M34 absent -> 0
    REQUIRE_FALSE(legacy.blackMarket.present);  // M34 absent -> no offer

    // Defensive: an out-of-range / inconsistent file clamps to [1,7] and keeps
    // currentTown <= highestUnlockedTown (a tampered file can't strand travel).
    writeFile(saves.slotPath(save::SaveSlot::Manual3),
              R"({"version":1,"gold":0,"currentTown":9,"highestUnlockedTown":2,"party":[
                 {"classId":"knight","name":"X","level":1,"xp":0,"hp":120,"mp":4}]})");
    Party clamped;
    content::LoadReport rep4;
    REQUIRE(saves.load(save::SaveSlot::Manual3, clamped, rep4));
    REQUIRE(clamped.highestUnlockedTown == 2);
    REQUIRE(clamped.currentTown == 2);  // 9 -> 7 -> clamped down to highestUnlocked

    fs::remove_all(dir);
}

TEST_CASE("save: castle unlock + records + King title round-trip (M40)", "[save]") {
    const content::ContentDatabase db = makeDb();
    const fs::path dir = makeTempDir();
    const save::SaveSystem saves(db, dir);

    Party p;
    p.members.push_back(createCharacter(*db.findClass("knight"), "Rolan"));
    p.castleUnlocked = true;
    p.castleRecords.bossRushBestTurns = 44;
    p.castleRecords.endlessBestWave = 12;
    p.castleRecords.kingDefeated = true;
    p.castleRecords.kingBestTurns = 18;
    p.castleRecords.kingTitle = kKingTitle;
    p.storyMet = 0b1010101;  // M41: some story installments heard
    p.encountered = {"goblin", "boss_deep_king"};  // M42 bestiary set
    p.recordBiggestHit = 777;                       // M42 personal records
    p.recordRunDamage = 5555;

    content::LoadReport rep;
    REQUIRE(saves.save(save::SaveSlot::Manual1, p, rep));
    Party loaded;
    content::LoadReport rep2;
    REQUIRE(saves.load(save::SaveSlot::Manual1, loaded, rep2));
    REQUIRE(loaded.castleUnlocked);
    REQUIRE(loaded.castleRecords.bossRushBestTurns == 44);
    REQUIRE(loaded.castleRecords.endlessBestWave == 12);
    REQUIRE(loaded.castleRecords.kingDefeated);
    REQUIRE(loaded.castleRecords.kingBestTurns == 18);
    REQUIRE(loaded.castleRecords.kingTitle == std::string(kKingTitle));
    REQUIRE(loaded.storyMet == 0b1010101);  // M41 round-trips
    REQUIRE(loaded.encountered == std::vector<std::string>{"goblin", "boss_deep_king"});  // M42
    REQUIRE(loaded.recordBiggestHit == 777);
    REQUIRE(loaded.recordRunDamage == 5555);

    // Backward compatibility: a save with no castle fields loads locked / no records.
    writeFile(saves.slotPath(save::SaveSlot::Manual2),
              R"({"version":1,"gold":0,"party":[
                 {"classId":"knight","name":"X","level":1,"xp":0,"hp":120,"mp":4}]})");
    Party legacy;
    content::LoadReport rep3;
    REQUIRE(saves.load(save::SaveSlot::Manual2, legacy, rep3));
    REQUIRE_FALSE(legacy.castleUnlocked);
    REQUIRE_FALSE(legacy.castleRecords.kingDefeated);
    REQUIRE(legacy.castleRecords.bossRushBestTurns == 0);
    REQUIRE(legacy.castleRecords.kingTitle.empty());
    REQUIRE(legacy.storyMet == 0);  // M41 absent -> nothing heard
    REQUIRE(legacy.encountered.empty());     // M42 absent -> empty bestiary
    REQUIRE(legacy.recordBiggestHit == 0);
    REQUIRE(legacy.recordRunDamage == 0);

    fs::remove_all(dir);
}

TEST_CASE("save: a minimal save with no equipment still loads", "[save]") {
    const content::ContentDatabase db = makeDb();
    const fs::path dir = makeTempDir();
    const save::SaveSystem saves(db, dir);

    writeFile(saves.slotPath(save::SaveSlot::Manual1),
              R"({"version":1,"gold":10,"party":[
                 {"classId":"knight","name":"Rolan","level":1,"xp":0,"hp":120,"mp":4}]})");
    Party p;
    content::LoadReport rep;
    REQUIRE(saves.load(save::SaveSlot::Manual1, p, rep));
    REQUIRE(p.members.size() == 1);
    REQUIRE(p.members[0].weapon.empty());
    REQUIRE(p.inventory.empty());

    fs::remove_all(dir);
}

TEST_CASE("save: equipment round-trips; unknown gear is dropped", "[save]") {
    const content::ContentDatabase db = makeDb();
    const fs::path dir = makeTempDir();
    const save::SaveSystem saves(db, dir);

    Party p;
    Character c = createCharacter(*db.findClass("knight"), "Rolan");
    c.weapon = "iron_sword";
    refreshCharacter(c, db);
    const int armedAttack = c.stats.attack;
    p.members.push_back(c);

    content::LoadReport rep;
    REQUIRE(saves.save(save::SaveSlot::Manual1, p, rep));

    Party loaded;
    content::LoadReport rep2;
    REQUIRE(saves.load(save::SaveSlot::Manual1, loaded, rep2));
    REQUIRE(loaded.members[0].weapon == "iron_sword");
    REQUIRE(loaded.members[0].stats.attack == armedAttack);  // bonus reapplied

    // A save referencing gear the content no longer knows is dropped (not fatal).
    writeFile(saves.slotPath(save::SaveSlot::Manual2),
              R"({"version":1,"gold":0,"party":[
                 {"classId":"knight","name":"X","level":1,"xp":0,"hp":120,"mp":4,
                  "weapon":"ghost_blade"}]})");
    Party dropped;
    content::LoadReport rep3;
    REQUIRE(saves.load(save::SaveSlot::Manual2, dropped, rep3));
    REQUIRE(dropped.members[0].weapon.empty());

    fs::remove_all(dir);
}
