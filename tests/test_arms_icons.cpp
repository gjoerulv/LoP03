// M81 — Arms, elements & icons: the ward-charm set, the two new legendaries,
// the elemental weapons filling the t2-t5 gaps, the enemy-side elemental
// coverage those resists exist for, and the gear-icon schema. Shipped-content
// pins prove the catalog; in-memory JSON proves the loader rules; buildBattle
// proves worn resistance lands on the model (the resist MATH is proven in
// test_rules_v15 — here we prove the shipped charms reach it).

#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <set>
#include <string>
#include <vector>

#include "battle/Battle.hpp"
#include "content/ContentDatabase.hpp"
#include "content/ContentLoader.hpp"
#include "content/Definitions.hpp"
#include "content/LoadReport.hpp"
#include "dungeon/DungeonModel.hpp"
#include "game/BossDrops.hpp"
#include "game/Party.hpp"

using namespace cd;

namespace {

content::Json parse(const char* text) { return content::Json::parse(text, nullptr, false); }

const content::ContentDatabase& db() {
    static content::ContentDatabase database = [] {
        content::ContentDatabase d;
        content::LoadReport report;
        REQUIRE(content::loadAll(std::filesystem::path(CRYSTAL_TEST_DATA_DIR), d, report));
        return d;
    }();
    return database;
}

int resistAt(const battle::Combatant& u, content::Element e) {
    return u.elementResist[static_cast<std::size_t>(e)];
}

// A one-hero party wearing the given accessory, fighting one goblin.
battle::Battle wearAndFight(const std::string& accessoryId) {
    Party party;
    party.members.push_back(createCharacter(*db().findClass("knight"), "Hero", 5));
    party.members[0].accessory = accessoryId;
    dungeon::EnemyTeam team;
    team.enemyIds = {"goblin_grunt"};
    return battle::buildBattle(party, team, db());
}

}  // namespace

// --- the ward set + legendaries (shipped content) ----------------------------

TEST_CASE("arms: the six ward charms cover every element across the ladder", "[arms]") {
    struct Ward {
        const char* id;
        content::Element element;
        int minTown;
    };
    const Ward wards[] = {
        {"flameward_charm", content::Element::Fire, 2},
        {"stormward_charm", content::Element::Lightning, 2},
        {"frostward_charm", content::Element::Ice, 3},
        {"nightward_charm", content::Element::Dark, 3},
        {"stoneward_charm", content::Element::Earth, 4},
        {"lightward_charm", content::Element::Holy, 4},
    };
    std::set<content::Element> covered;
    for (const Ward& w : wards) {
        const content::ItemDef* it = db().findItem(w.id);
        INFO(w.id);
        REQUIRE(it != nullptr);
        CHECK(it->type == content::ItemType::Equipment);
        CHECK(it->slot == content::EquipSlot::Accessory);
        CHECK(it->resistPct == 50);
        REQUIRE(it->resistElements.size() == 1);
        CHECK(it->resistElements[0] == w.element);
        CHECK(it->minTown == w.minTown);
        CHECK(it->rarity != content::Rarity::Legendary);  // buyable + chest pool
        covered.insert(w.element);
    }
    CHECK(static_cast<int>(covered.size()) == content::kElementCount);
}

TEST_CASE("arms: the two new legendaries sit in the shared legendary pool", "[arms]") {
    const content::ItemDef* watch = db().findItem("overwound_pocketwatch");
    REQUIRE(watch != nullptr);
    CHECK(watch->rarity == content::Rarity::Legendary);
    CHECK(watch->statBonus.speed == 200);  // the owner's number, verbatim

    const content::ItemDef* aegis = db().findItem("motley_aegis");
    REQUIRE(aegis != nullptr);
    CHECK(aegis->rarity == content::Rarity::Legendary);
    CHECK(aegis->resistPct == 50);
    CHECK(static_cast<int>(aegis->resistElements.size()) == content::kElementCount);

    // Legendary rarity is the pool membership rule (boss drops and the black
    // market draw from the identical sorted set), so both join automatically.
    const std::vector<std::string> pool = legendaryDropPool(db());
    const auto has = [&pool](const char* id) {
        return std::find(pool.begin(), pool.end(), std::string(id)) != pool.end();
    };
    CHECK(has("overwound_pocketwatch"));
    CHECK(has("motley_aegis"));
}

TEST_CASE("arms: worn ward charms land on the battle model at buildBattle", "[arms]") {
    battle::Battle plain = wearAndFight("");
    CHECK(resistAt(plain.units[0], content::Element::Fire) == 0);

    battle::Battle warded = wearAndFight("flameward_charm");
    CHECK(resistAt(warded.units[0], content::Element::Fire) == 50);
    CHECK(resistAt(warded.units[0], content::Element::Ice) == 0);  // one element only

    battle::Battle motley = wearAndFight("motley_aegis");
    for (content::Element e :
         {content::Element::Fire, content::Element::Ice, content::Element::Lightning,
          content::Element::Earth, content::Element::Holy, content::Element::Dark}) {
        INFO(content::elementDisplayName(e));
        CHECK(resistAt(motley.units[0], e) == 50);
    }
}

TEST_CASE("arms: stacked resist takes the best piece, never the sum", "[arms]") {
    content::ContentDatabase mem;
    content::LoadReport rep;
    content::parseClasses(parse(R"({"version":1,"classes":[
        {"id":"hero","name":"Hero","baseStats":{"hp":100,"attack":20,"magic":10,
         "defense":8,"speed":10}}]})"),
                          "mem", mem, rep);
    content::parseEnemies(parse(R"({"version":1,"enemies":[
        {"id":"e","name":"E","role":"bruiser",
         "stats":{"hp":50,"attack":10,"magic":0,"defense":5,"speed":5}}]})"),
                          "mem", mem, rep);
    content::parseItems(parse(R"({"version":1,"items":[
        {"id":"vest","name":"Vest","type":"equipment","slot":"armor",
         "resistPct":30,"resistElements":["fire"]},
        {"id":"band","name":"Band","type":"equipment","slot":"accessory",
         "resistPct":50,"resistElements":["fire"]}]})"),
                        "mem", mem, rep);
    REQUIRE(rep.ok());

    Party party;
    party.members.push_back(createCharacter(*mem.findClass("hero"), "Hero", 1));
    party.members[0].armor = "vest";
    party.members[0].accessory = "band";
    dungeon::EnemyTeam team;
    team.enemyIds = {"e"};
    battle::Battle b = battle::buildBattle(party, team, mem);
    CHECK(resistAt(b.units[0], content::Element::Fire) == 50);  // max(30, 50), not 80
}

// --- the elemental weapons ---------------------------------------------------

TEST_CASE("arms: the new weapons fill the tier gaps and dark stays enemy-only", "[arms]") {
    struct Arm {
        const char* id;
        content::Element element;
        int minTown;
        const char* icon;
    };
    const Arm arms[] = {
        {"winterbrand", content::Element::Ice, 2, "sword"},
        {"stormstring", content::Element::Lightning, 3, "bow"},
        {"quakemaul", content::Element::Earth, 4, "mace"},
        {"emberfangs", content::Element::Fire, 5, "dagger"},
        {"vigil_lance", content::Element::Holy, 5, "spear"},
    };
    for (const Arm& a : arms) {
        const content::ItemDef* it = db().findItem(a.id);
        INFO(a.id);
        REQUIRE(it != nullptr);
        CHECK(it->slot == content::EquipSlot::Weapon);
        CHECK(it->element == a.element);
        CHECK(it->minTown == a.minTown);
        CHECK(content::iconCategoryFor(*it) == a.icon);
    }
    // The owner's ruling (2026-08-05): no Dark weapon, ever. shadow_strike is
    // the lone Dark exception and it is a skill, not a weapon.
    for (const auto& [id, def] : db().items()) {
        INFO("item " << id);
        CHECK(def.element != content::Element::Dark);
    }
}

// --- enemy-side coverage (what the wards and weapons exist FOR) --------------

TEST_CASE("arms: every real element has an enemy-side damage dealer", "[arms]") {
    std::set<content::Element> dealt;
    const auto scan = [&dealt](const std::vector<std::string>& kit) {
        for (const std::string& sid : kit) {
            const content::SkillDef* s = db().findSkill(sid);
            if (s == nullptr || s->element == content::Element::None || s->power <= 0) {
                continue;
            }
            if (s->category == content::SkillCategory::Physical ||
                s->category == content::SkillCategory::Magic) {
                dealt.insert(s->element);
            }
        }
    };
    for (const auto& [id, def] : db().enemies()) {
        scan(def.skills);
    }
    for (const auto& [id, def] : db().bosses()) {
        scan(def.skills);
    }
    for (content::Element e :
         {content::Element::Fire, content::Element::Ice, content::Element::Lightning,
          content::Element::Earth, content::Element::Holy, content::Element::Dark}) {
        INFO(content::elementDisplayName(e));
        CHECK(dealt.count(e) == 1);
    }
}

TEST_CASE("arms: the coverage recipients can afford their new casts", "[arms]") {
    // Enemy MP derives from MAG (deriveMaxMp), so a kit addition without the
    // stat is a skill that never fires — the silent failure this pins against.
    struct Cast {
        const char* enemyId;
        const char* skillId;
    };
    for (const Cast& c : {Cast{"stone_golem", "stone_edge"}, Cast{"titan_guard", "stone_edge"},
                          Cast{"soul_render", "inferno"}, Cast{"standard_bearer", "smite"}}) {
        const content::EnemyDef* e = db().findEnemy(c.enemyId);
        const content::SkillDef* s = db().findSkill(c.skillId);
        INFO(c.enemyId << " casts " << c.skillId);
        REQUIRE(e != nullptr);
        REQUIRE(s != nullptr);
        const auto& kit = e->skills;
        CHECK(std::find(kit.begin(), kit.end(), std::string(c.skillId)) != kit.end());
        CHECK(deriveMaxMp(e->stats.magic) >= s->mpCost);
    }
}

// --- the icon schema ---------------------------------------------------------

TEST_CASE("arms: icon categories resolve for every piece of shipped gear", "[arms]") {
    for (const auto& [id, def] : db().items()) {
        // M96: heirlooms are worn gear too — they draw the relic keepsake
        // glyph (the dedicated icon is deferred; the milestone note records it).
        if (def.type != content::ItemType::Equipment && def.type != content::ItemType::Relic &&
            def.type != content::ItemType::Heirloom) {
            CHECK(content::iconCategoryFor(def).empty());  // non-gear never has one
            continue;
        }
        INFO("gear " << id);
        const std::string cat = content::iconCategoryFor(def);
        CHECK_FALSE(cat.empty());
        CHECK(content::isIconCategory(cat));
        CHECK(content::gearIconTextureId(def) == "ui.icon." + cat);
    }
    // The two shields carry the authored override the armor default would hide.
    CHECK(content::iconCategoryFor(*db().findItem("oak_shield")) == "shield");
    CHECK(content::iconCategoryFor(*db().findItem("tower_shield")) == "shield");
    // Slot-derived defaults carry the rest.
    CHECK(content::iconCategoryFor(*db().findItem("chain_mail")) == "armor");
    CHECK(content::iconCategoryFor(*db().findItem("swift_boots")) == "accessory");
    CHECK(content::iconCategoryFor(*db().findItem("ember_charm")) == "relic");
    // M96: the heirloom slot rides the relic glyph.
    CHECK(content::iconCategoryFor(*db().findItem("heirloom_emberwake")) == "relic");
}

TEST_CASE("arms: the loader guards the icon field", "[arms]") {
    const auto itemErrors = [](const char* json) {
        content::ContentDatabase mem;
        content::LoadReport rep;
        content::parseItems(parse(json), "mem", mem, rep);
        return !rep.ok();
    };
    // A weapon has no slot default, so it must author a category.
    CHECK(itemErrors(R"({"version":1,"items":[
        {"id":"w","name":"W","type":"equipment","slot":"weapon"}]})"));
    // The category must be one the shipped icon set draws.
    CHECK(itemErrors(R"({"version":1,"items":[
        {"id":"w","name":"W","type":"equipment","slot":"weapon",
         "iconCategory":"halberd"}]})"));
    // Only gear renders icons.
    CHECK(itemErrors(R"({"version":1,"items":[
        {"id":"p","name":"P","type":"consumable","iconCategory":"sword"}]})"));
    // The valid shapes pass: an authored weapon, and armor riding its default.
    content::ContentDatabase mem;
    content::LoadReport rep;
    content::parseItems(parse(R"({"version":1,"items":[
        {"id":"w","name":"W","type":"equipment","slot":"weapon","iconCategory":"axe"},
        {"id":"a","name":"A","type":"equipment","slot":"armor"}]})"),
                        "mem", mem, rep);
    REQUIRE(rep.ok());
    CHECK(content::iconCategoryFor(*mem.findItem("w")) == "axe");
    CHECK(content::iconCategoryFor(*mem.findItem("a")) == "armor");
}
