// M131 - owner batch 4: THE STRANGER "P" waits on THIS save's King kill, the
// Iron Man Dragon bar (party creation and the three Iron accomplishments),
// and the Alarm - a field-only consumable that rings the dungeon's next
// patrol at once.

#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <filesystem>
#include <initializer_list>
#include <string>
#include <vector>

#include "content/ContentDatabase.hpp"
#include "content/ContentLoader.hpp"
#include "content/Definitions.hpp"
#include "content/Enums.hpp"
#include "content/LoadReport.hpp"
#include "dungeon/DungeonGenerator.hpp"
#include "dungeon/DungeonModel.hpp"
#include "game/Achievements.hpp"
#include "game/Character.hpp"
#include "game/Cutscenes.hpp"
#include "game/IronMan.hpp"
#include "game/ItemCaps.hpp"
#include "game/ItemUse.hpp"
#include "game/Party.hpp"
#include "states/ItemShopFilter.hpp"

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

Party makeParty(std::initializer_list<const char*> classIds) {
    Party p;
    resetForNewGame(p);
    for (const char* id : classIds) {
        const content::ClassDef* cls = db().findClass(id);
        REQUIRE(cls != nullptr);
        p.members.push_back(createCharacter(*cls, id, 5));
    }
    return p;
}

bool contains(const std::vector<std::string>& v, const char* id) {
    for (const std::string& s : v) {
        if (s == id) {
            return true;
        }
    }
    return false;
}

content::Json parse(const char* text) { return content::Json::parse(text, nullptr, false); }

}  // namespace

// ---------------------------------------------------------------- P ----

TEST_CASE("m131: P stands at the roadside only after THIS save's King kill",
          "[m131][cutscene]") {
    // The owner's report: a brand-new party reached town 7 and P was already
    // there - the gate read the cross-save profile flag. Now it reads the
    // party's own castle record, so a fresh save meets the King first.
    Party p = makeParty({"knight", "ranger", "mage", "cleric"});
    p.currentTown = 7;
    p.highestUnlockedTown = 7;
    CHECK_FALSE(game::strangerAtRoadside(p));
    p.castleRecords.kingDefeated = true;
    CHECK(game::strangerAtRoadside(p));
    p.currentTown = 6;  // only town 7's eastern road
    CHECK_FALSE(game::strangerAtRoadside(p));
    p.currentTown = 99;  // a stored index is clamped onto the ladder
    CHECK(game::strangerAtRoadside(p));
    // A save that beat the King before this build already carries the record
    // (the castle writes it on the kill), so its P is unchanged.
    Party veteran = makeParty({"knight", "ranger", "mage", "cleric"});
    veteran.currentTown = 7;
    veteran.castleRecords.kingDefeated = true;
    veteran.castleRecords.kingBestTurns = 20;
    CHECK(game::strangerAtRoadside(veteran));
}

// -------------------------------------------------------- the Dragon bar ----

TEST_CASE("m131: the Dragon class is barred from an Iron Man party", "[m131][ironman]") {
    CHECK(ironman::classBarred("dragon"));
    for (const char* id :
         {"knight", "ranger", "mage", "cleric", "rogue", "guardian", "jester", "goose", ""}) {
        INFO(id);
        CHECK_FALSE(ironman::classBarred(id));
    }
    REQUIRE(db().findClass(ironman::kBarredClassId) != nullptr);  // the shipped id
    CHECK_FALSE(ironman::hasBarredMember(makeParty({"knight", "ranger", "mage", "cleric"})));
    CHECK(ironman::hasBarredMember(makeParty({"knight", "dragon", "mage", "cleric"})));
    CHECK_FALSE(ironman::hasBarredMember(Party{}));

    // The rules page spells the bar out before the mode can be chosen.
    CHECK(ironman::kRules.size() == 5);
    std::string all;
    for (const char* rule : ironman::kRules) {
        all += rule;
        all += ' ';
    }
    CHECK(all.find("Dragon class") != std::string::npos);
    CHECK(std::string(ironman::kBarredClassNote).find("Dragon") != std::string::npos);
    CHECK(std::string(ironman::kBarredClassSuffix).find("Not allowed") != std::string::npos);
}

TEST_CASE("m131: a Dragon in the party forfeits the Iron accomplishments",
          "[m131][ironman][achievement]") {
    Party p = makeParty({"dragon", "knight", "mage", "cleric"});
    p.ironMan = true;
    p.castleRecords.kingDefeated = true;
    p.castleRecords.dragonBestTurns = 30;
    p.castleRecords.duckBestTurns = 12;
    const AchvContext ctx{};
    CHECK_FALSE(achievementMet("iron_crown", p, ctx));
    CHECK_FALSE(achievementMet("iron_scales", p, ctx));
    CHECK_FALSE(achievementMet("iron_bill", p, ctx));
    // The ordinary trophies are untouched by the bar.
    CHECK(achievementMet("kingslayer", p, ctx));
    CHECK(achievementMet("wyrmbane", p, ctx));
    CHECK(achievementMet("quackbane", p, ctx));

    // Without the Dragon the same run earns all three - the bar is the only
    // difference.
    p.members.erase(p.members.begin());
    CHECK(achievementMet("iron_crown", p, ctx));
    CHECK(achievementMet("iron_scales", p, ctx));
    CHECK(achievementMet("iron_bill", p, ctx));

    // A Normal party with a Dragon never had them anyway.
    Party normal = makeParty({"dragon", "knight", "mage", "cleric"});
    normal.castleRecords.kingDefeated = true;
    CHECK_FALSE(achievementMet("iron_crown", normal, ctx));
    CHECK(achievementMet("kingslayer", normal, ctx));
}

// -------------------------------------------------------------- the Alarm ----

TEST_CASE("m131: 'alarm' is a consumable-only effect that targets nothing", "[m131][content]") {
    using content::ConsumableEffect;
    CHECK(content::parseConsumableEffect("alarm") == ConsumableEffect::Alarm);
    CHECK(std::string(content::toString(ConsumableEffect::Alarm)) == "alarm");
    CHECK(content::consumableEffectIds().size() == 6);

    SECTION("a plain alarm consumable loads") {
        content::ContentDatabase d;
        content::LoadReport rep;
        content::parseItems(parse(R"({"version":1,"items":[
            {"id":"bell","name":"Bell","type":"consumable","value":200,"minTown":4,
             "effect":"alarm"}]})"),
                            "mem", d, rep);
        REQUIRE(rep.ok());
        const content::ItemDef* bell = d.findItem("bell");
        REQUIRE(bell != nullptr);
        CHECK(bell->effect == ConsumableEffect::Alarm);
        CHECK(itemIsAlarm(*bell));
    }
    SECTION("on equipment it is refused") {
        content::ContentDatabase d;
        content::LoadReport rep;
        content::parseItems(parse(R"({"version":1,"items":[
            {"id":"ring","name":"Ring","type":"equipment","slot":"accessory","value":10,
             "effect":"alarm"}]})"),
                            "mem", d, rep);
        REQUIRE_FALSE(rep.ok());
        CHECK(d.itemCount() == 0);
    }
    SECTION("an enemy target on it is refused") {
        content::ContentDatabase d;
        content::LoadReport rep;
        content::parseItems(parse(R"({"version":1,"items":[
            {"id":"bell","name":"Bell","type":"consumable","value":200,
             "effect":"alarm","battleTarget":"enemy"}]})"),
                            "mem", d, rep);
        REQUIRE_FALSE(rep.ok());
    }
    SECTION("a status rider on it is refused") {
        content::ContentDatabase d;
        content::LoadReport rep;
        content::parseItems(parse(R"({"version":1,"items":[
            {"id":"bell","name":"Bell","type":"consumable","value":200,
             "effect":"alarm","statuses":[{"type":"stunned","duration":1}]}]})"),
                            "mem", d, rep);
        REQUIRE_FALSE(rep.ok());
    }
}

TEST_CASE("m131: the shipped Alarm - 200 gold, town 4 and up, the default cap, an oddity",
          "[m131][itemshop][caps]") {
    const content::ItemDef* alarm = db().findItem("alarm");
    REQUIRE(alarm != nullptr);
    CHECK(alarm->type == content::ItemType::Consumable);
    CHECK(alarm->effect == content::ConsumableEffect::Alarm);
    CHECK(alarm->value == 200);
    CHECK(alarm->minTown == 4);
    CHECK(alarm->maxTown == 0);
    CHECK_FALSE(alarm->notSoldInTown);
    CHECK(alarm->maxHeld == 0);      // the normal quantity rule ...
    CHECK(capFor(*alarm) == 2);      // ... two held, at the till
    CHECK(capFor(*alarm, 1) == 3);   // raised by the pockets perks like any other
    CHECK(itemIsAlarm(*alarm));
    CHECK_FALSE(itemIsAlarm(*db().findItem("potion")));
    CHECK_FALSE(itemIsAlarm(*db().findItem("holy_taxes")));

    // Sold in town 4 and after, nowhere before - the ordinary town window.
    for (int town = 1; town <= 3; ++town) {
        INFO("town " << town);
        CHECK_FALSE(contains(itemShopBuyIds(db(), town), "alarm"));
    }
    for (int town = 4; town <= 7; ++town) {
        INFO("town " << town);
        CHECK(contains(itemShopBuyIds(db(), town), "alarm"));
    }
    CHECK(itemShopCategoryRank(*alarm) == 4);  // with the oddities, after the revives
}

TEST_CASE("m131: the Alarm is rung, never taken", "[m131][inventory]") {
    const content::ItemDef* alarm = db().findItem("alarm");
    REQUIRE(alarm != nullptr);
    const content::ClassDef* knight = db().findClass("knight");
    REQUIRE(knight != nullptr);
    Character wounded = createCharacter(*knight, "Rolan", 5);
    wounded.hp = 1;  // a potion would be accepted here
    CHECK(itemUseRefusal(wounded, *db().findItem("potion")).empty());
    CHECK(itemUseRefusal(wounded, *alarm) == std::string(kAlarmMemberRefusal));
    Character fallen = createCharacter(*knight, "Mira", 5);
    fallen.hp = 0;
    CHECK(itemUseRefusal(fallen, *alarm) == std::string(kAlarmMemberRefusal));
    CHECK(std::string(kAlarmTownRefusal).find("dungeon") != std::string::npos);
    // applyItemUse never has a line for it either.
    CHECK(applyItemUse(wounded, *alarm).empty());
    CHECK(wounded.hp == 1);
}

TEST_CASE("m131: the Alarm is never dungeon loot - no chest, no peddler, any town",
          "[m131][dungeon]") {
    // Town shelves only (owner decision): it enters neither pool, so every
    // seed's chests and merchant offers are byte-identical to before it
    // existed - no generation bump.
    int offers = 0;
    int chests = 0;
    for (int town = 4; town <= 7; ++town) {
        for (const char* theme :
             {"ruined_keep", "crystal_mine", "hollow_forest", "goosy_gauntlet", ""}) {
            for (std::uint64_t seed = 1; seed <= 10; ++seed) {
                const dungeon::Dungeon d =
                    dungeon::generate(seed * 7919 + static_cast<std::uint64_t>(town), 3, db(),
                                      theme, town);
                for (const dungeon::Room& r : d.rooms) {
                    INFO("town " << town << " theme '" << theme << "' seed " << seed);
                    CHECK(r.chest.itemId != "alarm");
                    CHECK(r.event.itemId != "alarm");
                    if (!r.event.itemId.empty()) {
                        ++offers;
                    }
                    if (!r.chest.itemId.empty()) {
                        ++chests;
                    }
                }
            }
        }
    }
    CHECK(offers > 0);  // the pools were exercised
    CHECK(chests > 0);
}
