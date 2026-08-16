#include <catch2/catch_test_macros.hpp>

#include "content/ContentDatabase.hpp"
#include "content/Definitions.hpp"
#include "danger/DangerRating.hpp"
#include "dungeon/DungeonModel.hpp"
#include "dungeon/RoomLayout.hpp"
#include "game/Character.hpp"

#ifdef CRYSTAL_TEST_DATA_DIR
#include <algorithm>
#include <filesystem>
#include <iostream>
#include <vector>

#include "battle/Battle.hpp"
#include "battle/Simulator.hpp"
#include "content/ContentLoader.hpp"
#include "content/LoadReport.hpp"
#include "dungeon/DungeonGenerator.hpp"
#include "game/Party.hpp"
#include "states/EquipShopFilter.hpp"  // pure: the shop shelf per town
#endif

using namespace cd;
using namespace cd::danger;

namespace {

content::ContentDatabase makeDb() {
    content::ContentDatabase db;
    content::SkillDef strike;
    strike.id = "strike";
    strike.name = "Strike";
    strike.category = content::SkillCategory::Physical;
    strike.power = 10;
    db.addSkill(strike);

    content::EnemyDef rat;
    rat.id = "rat";
    rat.name = "Rat";
    rat.stats = {10, 4, 0, 1, 5};
    rat.tier = content::EnemyTier::Normal;
    db.addEnemy(rat);

    content::EnemyDef ogre;
    ogre.id = "ogre";
    ogre.name = "Ogre";
    ogre.stats = {110, 22, 0, 14, 7};
    ogre.tier = content::EnemyTier::Elite;
    ogre.skills = {"strike"};
    db.addEnemy(ogre);
    return db;
}

dungeon::EnemyTeam team(std::vector<std::string> ids, bool boss = false) {
    dungeon::EnemyTeam t;
    t.enemyIds = std::move(ids);
    t.isBoss = boss;
    return t;
}

Character member(int maxHp, int atk, int mag, int def, int spd) {
    Character c;
    c.maxHp = maxHp;
    c.stats.attack = atk;
    c.stats.magic = mag;
    c.stats.defense = def;
    c.stats.speed = spd;
    return c;
}

}  // namespace

// The exact generation pin rides the newest bump — v17 (M93) tags the two
// new pure-hash event replacements (Surveyor, Dragonform) alongside v16's
// 20-floor shape (the pin moves with each owner-approved bump).
TEST_CASE("danger: generation version rides the newest bump", "[danger]") {
    CHECK(dungeon::kGenerationVersion == 17);
}

TEST_CASE("danger: threat grows with stronger and more numerous enemies", "[danger]") {
    const content::ContentDatabase db = makeDb();
    const int oneRat = teamThreat(team({"rat"}), db);
    const int twoRats = teamThreat(team({"rat", "rat"}), db);
    const int oneOgre = teamThreat(team({"ogre"}), db);

    REQUIRE(oneRat > 0);
    REQUIRE(twoRats > oneRat);   // more enemies (and synergy) => more threat
    REQUIRE(oneOgre > twoRats);  // a much stronger enemy
}

TEST_CASE("danger: a stronger team rates a higher tier for the same party", "[danger]") {
    const content::ContentDatabase db = makeDb();
    const int pt = partyThreat({member(60, 12, 6, 8, 7)});
    const Tier weak = assess(team({"rat"}), db, pt);
    const Tier strong = assess(team({"ogre", "ogre"}), db, pt);
    REQUIRE(static_cast<int>(strong) > static_cast<int>(weak));
}

TEST_CASE("danger: a stronger party lowers the tier of the same team", "[danger]") {
    const content::ContentDatabase db = makeDb();
    const dungeon::EnemyTeam foes = team({"ogre", "ogre"});
    const Tier vsWeak = assess(foes, db, partyThreat({member(40, 8, 4, 5, 6)}));
    const Tier vsStrong = assess(
        foes, db,
        partyThreat({member(300, 60, 40, 45, 30), member(280, 55, 65, 40, 35),
                     member(320, 70, 20, 55, 25), member(260, 45, 70, 35, 40)}));
    REQUIRE(static_cast<int>(vsStrong) < static_cast<int>(vsWeak));
    // And a monotonic sweep: growing the party never RAISES a tier.
    int previous = static_cast<int>(Tier::Boss);
    for (int scale = 1; scale <= 12; ++scale) {
        const int pt = partyThreat({member(40 * scale, 8 * scale, 4 * scale, 5 * scale, 6 * scale)});
        const int tier = static_cast<int>(assess(foes, db, pt));
        REQUIRE(tier <= previous);
        previous = tier;
    }
}

TEST_CASE("danger: a boss team is always the Boss tier", "[danger]") {
    const content::ContentDatabase db = makeDb();
    REQUIRE(assess(team({"rat"}, /*boss=*/true), db, 1000) == Tier::Boss);
}

TEST_CASE("danger: tierFor crosses the party-relative bands", "[danger]") {
    // Bands are percent of the party's own threat: <20 / <40 / <70 / <110.
    REQUIRE(tierFor(0, 1000, false) == Tier::Trivial);
    REQUIRE(tierFor(199, 1000, false) == Tier::Trivial);
    REQUIRE(tierFor(200, 1000, false) == Tier::Easy);
    REQUIRE(tierFor(399, 1000, false) == Tier::Easy);
    REQUIRE(tierFor(400, 1000, false) == Tier::Fair);
    REQUIRE(tierFor(699, 1000, false) == Tier::Fair);
    REQUIRE(tierFor(700, 1000, false) == Tier::Dangerous);
    REQUIRE(tierFor(1099, 1000, false) == Tier::Dangerous);
    REQUIRE(tierFor(1100, 1000, false) == Tier::Deadly);
    REQUIRE(tierFor(10, 1000, true) == Tier::Boss);
}

TEST_CASE("danger: tier weights are ordered", "[danger]") {
    REQUIRE(tierWeight(Tier::Trivial) < tierWeight(Tier::Fair));
    REQUIRE(tierWeight(Tier::Fair) < tierWeight(Tier::Deadly));
    REQUIRE(tierWeight(Tier::Deadly) < tierWeight(Tier::Boss));
}

TEST_CASE("danger: rating is deterministic", "[danger]") {
    const content::ContentDatabase db = makeDb();
    REQUIRE(teamThreat(team({"ogre", "rat"}), db) == teamThreat(team({"ogre", "rat"}), db));
    REQUIRE(memberThreat(member(120, 25, 5, 20, 10)) == memberThreat(member(120, 25, 5, 20, 10)));
}

#ifdef CRYSTAL_TEST_DATA_DIR

// --- Data-driven calibration ----------------------------------------------

namespace {

content::ContentDatabase loadContent() {
    content::ContentDatabase db;
    content::LoadReport rep;
    content::loadAll(std::filesystem::path(CRYSTAL_TEST_DATA_DIR), db, rep);
    return db;
}

Party makeParty(const content::ContentDatabase& db, int level) {
    Party p;
    for (const char* id : {"knight", "ranger", "mage", "cleric"}) {
        if (const content::ClassDef* cls = db.findClass(id)) {
            p.members.push_back(createCharacter(*cls, id, level));
        }
    }
    return p;
}

bool partyClears(const content::ContentDatabase& db, const dungeon::Dungeon& d, int level) {
    for (const dungeon::EnemyTeam& t : d.teams) {
        battle::Battle b = battle::buildBattle(makeParty(db, level), t, db);
        if (battle::simulate(b, db).outcome != battle::Outcome::Victory) {
            return false;
        }
    }
    return true;
}

int clearingLevel(const content::ContentDatabase& db, const dungeon::Dungeon& d) {
    for (int level = 1; level <= kMaxLevel; ++level) {
        if (partyClears(db, d, level)) {
            return level;
        }
    }
    return kMaxLevel + 1;
}

// The realistic calibration party: each member wears the best (priciest)
// class-legal piece the town's shop stocks in every slot — what an actual
// party at that town looks like, unlike the bare simulator baseline.
Party gearedParty(const content::ContentDatabase& db, int level, int town) {
    Party p = makeParty(db, level);
    for (Character& c : p.members) {
        for (content::EquipSlot slot :
             {content::EquipSlot::Weapon, content::EquipSlot::Armor,
              content::EquipSlot::Accessory}) {
            if (!canEquipSlot(c, slot, db)) {
                continue;
            }
            const content::ItemDef* best = nullptr;
            for (const std::string& id : equipShopBuyIds(db, slot, town)) {
                const content::ItemDef* it = db.findItem(id);
                if (it != nullptr && (best == nullptr || it->value > best->value)) {
                    best = it;
                }
            }
            if (best != nullptr) {
                (slot == content::EquipSlot::Weapon
                     ? c.weapon
                     : slot == content::EquipSlot::Armor ? c.armor : c.accessory) = best->id;
            }
        }
        refreshCharacter(c, db);
    }
    return p;
}

}  // namespace

// The calibration promise, kept loose enough to survive balance motion: a
// party with real headroom reads its dungeon down the bands, a hopeless one
// reads it Deadly, and growth is monotonic.
TEST_CASE("danger: party-relative calibration anchors", "[danger]") {
    const content::ContentDatabase db = loadContent();

    // A maxed party reads an entry dungeon as beneath it.
    {
        const dungeon::Dungeon d = dungeon::generate(11, 3, db, "ruined_keep", 1);
        const int pt = partyThreat(makeParty(db, kMaxLevel).members);
        for (const dungeon::EnemyTeam& t : d.teams) {
            if (!t.isBoss) {
                REQUIRE(static_cast<int>(assess(t, db, pt)) <=
                        static_cast<int>(Tier::Easy));
            }
        }
    }
    // A fresh party at the top of the world reads everything Deadly.
    {
        const dungeon::Dungeon d = dungeon::generate(11, 20, db, "ruined_keep", 7);
        const int pt = partyThreat(makeParty(db, 1).members);
        for (const dungeon::EnemyTeam& t : d.teams) {
            if (!t.isBoss) {
                REQUIRE(assess(t, db, pt) == Tier::Deadly);
            }
        }
    }
    // Growth is monotonic against a real mid-world dungeon.
    {
        const dungeon::Dungeon d = dungeon::generate(222, 8, db, "crystal_mine", 4);
        int previousSum = 1 << 20;
        for (int level : {5, 15, 25, 35, 50}) {
            const int pt = partyThreat(makeParty(db, level).members);
            int sum = 0;
            for (const dungeon::EnemyTeam& t : d.teams) {
                sum += static_cast<int>(assess(t, db, pt));
            }
            REQUIRE(sum <= previousSum);
            previousSum = sum;
        }
    }
}

// On-demand calibration table for the milestone note and owner review:
//   crystal_tests.exe "[danger-report]" -s
// For each (town, depth): the simulator's clearing level, then how a party at
// that level (and +8 / +16 headroom) reads the dungeon's non-boss teams —
// min/median/max threat ratio (percent of party threat) and the tier counts
// as Trivial/Easy/Fair/Dangerous/Deadly.
TEST_CASE("danger report: party-relative tier calibration", "[.][danger-report]") {
    const content::ContentDatabase db = loadContent();
    std::cout << "GEARED party (town-shelf best in every slot)\n";
    std::cout << "town depth | clearLv | party(+0/+8/+16): ratio min/med/max -> T/E/F/D/DL\n";
    for (int town : {1, 3, 5, 7}) {
        for (int depth : {3, 8, 15, 20}) {
            const dungeon::Dungeon d = dungeon::generate(11, depth, db, "ruined_keep", town);
            const int clearLv = clearingLevel(db, d);
            std::cout << town << " " << depth << " | " << clearLv << " |";
            for (int bonus : {0, 8, 16}) {
                const int level = std::min(kMaxLevel, clearLv + bonus);
                const int pt = partyThreat(gearedParty(db, level, town).members);
                std::vector<int> ratios;
                int counts[5] = {0, 0, 0, 0, 0};
                for (const dungeon::EnemyTeam& t : d.teams) {
                    if (t.isBoss) {
                        continue;
                    }
                    ratios.push_back(teamThreat(t, db) * 100 / std::max(1, pt));
                    const int tier = static_cast<int>(assess(t, db, pt));
                    if (tier >= 0 && tier < 5) {
                        ++counts[tier];
                    }
                }
                std::sort(ratios.begin(), ratios.end());
                const int n = static_cast<int>(ratios.size());
                std::cout << "  Lv" << level << ": " << (n > 0 ? ratios.front() : 0) << "/"
                          << (n > 0 ? ratios[static_cast<std::size_t>(n / 2)] : 0) << "/"
                          << (n > 0 ? ratios.back() : 0) << " -> " << counts[0] << "/"
                          << counts[1] << "/" << counts[2] << "/" << counts[3] << "/"
                          << counts[4] << " |";
            }
            std::cout << "\n";
        }
    }
    SUCCEED();
}

#endif  // CRYSTAL_TEST_DATA_DIR
