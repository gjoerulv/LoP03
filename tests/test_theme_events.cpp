#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <filesystem>
#include <string>

#include "content/ContentDatabase.hpp"
#include "content/ContentLoader.hpp"
#include "content/Definitions.hpp"
#include "content/LoadReport.hpp"
#include "dungeon/DungeonGenerator.hpp"
#include "dungeon/DungeonModel.hpp"
#include "dungeon/RoomLayout.hpp"
#include "dungeon/ThemeEvents.hpp"
#include "game/Castle.hpp"     // M106: bossRushOrder — the rush inclusion pin
#include "game/Gooseform.hpp"  // M106: the flock transform
#include "game/Party.hpp"
#include "score/Scoring.hpp"   // M106: the +300 itemization

// M55 per-theme rites, LEVELED 2026-08-17 (v22) and folded into the equal
// encounter tier 2026-08-28 (v23): a themed floor rolls its rite at
// kEncounterChancePct like every other encounter, never a cross-theme one;
// the pure helpers (tier-up, cache, root) behave as designed and
// deterministically. Content-loading cases need the real data
// (CRYSTAL_TEST_DATA_DIR); the pure-math cases run anywhere.

using namespace cd;
using namespace cd::dungeon;

TEST_CASE("theme rites: generation version bumped to 11", "[theme-events]") {
    // The v11 rules are in force at or above their own version; the EXACT pin
    // lives with the newest generation milestone's tests (M65 = 12).
    CHECK(kGenerationVersion >= 11);
}

TEST_CASE("theme rites: one rite per theme; empty/unknown themes force nothing",
          "[theme-events]") {
    CHECK(themeEventKind("ruined_keep") == RoomEventKind::ArmoryGhost);
    CHECK(themeEventKind("crystal_mine") == RoomEventKind::MinersCache);
    CHECK(themeEventKind("hollow_forest") == RoomEventKind::ElderRoot);
    CHECK(themeEventKind("") == RoomEventKind::None);
    CHECK(themeEventKind("no_such_theme") == RoomEventKind::None);
}

TEST_CASE("theme rites: rarity steps up and legendary is the ceiling", "[theme-events]") {
    CHECK(nextRarityUp(content::Rarity::Common) == content::Rarity::Uncommon);
    CHECK(nextRarityUp(content::Rarity::Uncommon) == content::Rarity::Rare);
    CHECK(nextRarityUp(content::Rarity::Rare) == content::Rarity::Epic);
    CHECK(nextRarityUp(content::Rarity::Epic) == content::Rarity::Legendary);
    CHECK(nextRarityUp(content::Rarity::Legendary) == content::Rarity::Legendary);
}

TEST_CASE("theme rites: Miner's Cache wound is never fatal and pays above a trapped chest",
          "[theme-events]") {
    // A third of max HP; the caller clamps HP to >= 1, so the wound never kills.
    CHECK(minersCacheWound(300) == 100);
    CHECK(minersCacheWound(3) == 1);
    CHECK(minersCacheWound(2) == 0);
    // The reward exceeds the biggest possible trapped chest at every depth.
    for (int depth = 1; depth <= 20; ++depth) {
        const int maxTrapped = 55 * depth + 15;  // base rng(10,30)*d max + trapped add
        INFO("depth=" << depth);
        CHECK(minersCacheGold(depth) > maxTrapped);
    }
}

#ifdef CRYSTAL_TEST_DATA_DIR

namespace {
content::ContentDatabase loadContent() {
    content::ContentDatabase db;
    content::LoadReport rep;
    content::loadAll(std::filesystem::path(CRYSTAL_TEST_DATA_DIR), db, rep);
    return db;
}

int countRite(const Dungeon& d, RoomEventKind rite) {
    int n = 0;
    for (const Room& r : d.rooms) {
        if (r.type == RoomType::Event && r.event.kind == rite) {
            ++n;
        }
    }
    return n;
}

int countAnyRite(const Dungeon& d) {
    return countRite(d, RoomEventKind::ArmoryGhost) + countRite(d, RoomEventKind::MinersCache) +
           countRite(d, RoomEventKind::ElderRoot);
}
}  // namespace

TEST_CASE("theme rites: leveled to a rare roll, at most once, never cross-theme",
          "[theme-events]") {
    // Leveled at v22, equal-tier at v23: the rite is a kEncounterChancePct
    // pure-hash encounter like every other — the sweep pins the band, the
    // at-most-once rule, and that a theme only ever rolls ITS OWN rite.
    const content::ContentDatabase db = loadContent();
    struct ThemeCase {
        const char* id;
        RoomEventKind rite;
    };
    const ThemeCase cases[] = {
        {"ruined_keep", RoomEventKind::ArmoryGhost},
        {"crystal_mine", RoomEventKind::MinersCache},
        {"hollow_forest", RoomEventKind::ElderRoot},
    };
    for (const ThemeCase& tc : cases) {
        int withRite = 0;
        constexpr int kSeeds = 200;
        for (std::uint64_t seed = 1; seed <= kSeeds; ++seed) {
            const int depth = 1 + static_cast<int>(seed % 12);
            const int town = 1 + static_cast<int>(seed % 7);
            const Dungeon d = generate(seed, depth, db, tc.id, town);
            INFO("theme=" << tc.id << " seed=" << seed << " depth=" << depth << " town=" << town);
            const int own = countRite(d, tc.rite);
            CHECK(own <= 1);                    // never more than one per floor
            CHECK(countAnyRite(d) == own);      // and never another theme's rite
            withRite += own;
        }
        // ~8% of 200 floors = 16 expected; a generous band that still fails
        // hard on "always" (old behavior) or "never" (a broken roll).
        INFO("theme=" << tc.id << " withRite=" << withRite << "/" << kSeeds);
        CHECK(withRite >= 3);
        CHECK(withRite <= 45);
    }
}

TEST_CASE("theme rites: an empty-theme dungeon carries no rite", "[theme-events]") {
    const content::ContentDatabase db = loadContent();
    for (std::uint64_t seed = 1; seed <= 40; ++seed) {
        const Dungeon d = generate(seed, 5, db, "", 1);
        CHECK(countAnyRite(d) == 0);
    }
}

TEST_CASE("theme rites: the rite and the relic never share a room", "[theme-events]") {
    // The rite replaces only PLAIN rolled slots — never the relic (drawn from
    // the base stream first) and never an elite challenge (whose team would
    // be orphaned); the relic stays at most one per floor.
    const content::ContentDatabase db = loadContent();
    for (std::uint64_t seed = 1; seed <= 120; ++seed) {
        const Dungeon d = generate(seed, 20, db, "ruined_keep", 7);
        INFO("seed=" << seed);
        int relics = 0;
        for (const Room& r : d.rooms) {
            if (r.event.kind == RoomEventKind::RoyalRelic) {
                ++relics;
            }
            if (r.event.kind == RoomEventKind::ArmoryGhost) {
                CHECK(r.teamIndex < 0);  // never on an elite challenge's room
            }
        }
        CHECK(relics <= 1);
        CHECK(countRite(d, RoomEventKind::ArmoryGhost) <= 1);
    }
}

TEST_CASE("theme rites: same seed reproduces the same placement and payloads",
          "[theme-events]") {
    const content::ContentDatabase db = loadContent();
    const Dungeon a = generate(4242, 8, db, "crystal_mine", 4);
    const Dungeon b = generate(4242, 8, db, "crystal_mine", 4);
    REQUIRE(a.rooms.size() == b.rooms.size());
    for (std::size_t i = 0; i < a.rooms.size(); ++i) {
        CHECK(a.rooms[i].event.kind == b.rooms[i].event.kind);
        CHECK(a.rooms[i].event.itemId == b.rooms[i].event.itemId);
        CHECK(a.rooms[i].event.goldCost == b.rooms[i].event.goldCost);
    }
    // Where the leveled roll places a rite, its payload is baked exactly as
    // the old forced path baked it: the Miner's Cache carries an item, the
    // Elder Root a town-scaled price. Sweep until each has appeared once.
    bool sawCacheItem = false;
    for (std::uint64_t seed = 1; seed <= 400 && !sawCacheItem; ++seed) {
        const Dungeon d = generate(seed, 8, db, "crystal_mine", 4);
        for (const Room& r : d.rooms) {
            if (r.event.kind == RoomEventKind::MinersCache) {
                CHECK_FALSE(r.event.itemId.empty());
                sawCacheItem = true;
            }
        }
    }
    CHECK(sawCacheItem);
    bool sawRootPrice = false;
    for (std::uint64_t seed = 1; seed <= 400 && !sawRootPrice; ++seed) {
        const Dungeon d = generate(seed, 6, db, "hollow_forest", 3);
        for (const Room& r : d.rooms) {
            if (r.event.kind == RoomEventKind::ElderRoot) {
                CHECK(r.event.goldCost == elderRootPrice(3, 6));
                sawRootPrice = true;
            }
        }
    }
    CHECK(sawRootPrice);
}

TEST_CASE("theme rites: the Armory Ghost upgrades same-slot, next-rarity, refuses legendary",
          "[theme-events]") {
    const content::ContentDatabase db = loadContent();
    // A common weapon -> an uncommon weapon, whatever the hash.
    for (std::uint64_t h = 0; h < 8; ++h) {
        const std::string up = armoryGhostUpgrade(db, "iron_sword", h);  // common weapon
        REQUIRE_FALSE(up.empty());
        const content::ItemDef* it = db.findItem(up);
        REQUIRE(it != nullptr);
        CHECK(it->slot == content::EquipSlot::Weapon);
        CHECK(it->rarity == content::Rarity::Uncommon);
    }
    // An epic can yield a legendary (a designed third legendary source).
    {
        const std::string up = armoryGhostUpgrade(db, "sunforged_greatblade", 3);  // epic weapon
        REQUIRE_FALSE(up.empty());
        const content::ItemDef* it = db.findItem(up);
        REQUIRE(it != nullptr);
        CHECK(it->slot == content::EquipSlot::Weapon);
        CHECK(it->rarity == content::Rarity::Legendary);
    }
    // Same-slot guarantee for armor and accessory inputs too.
    {
        const std::string up = armoryGhostUpgrade(db, "leather_armor", 1);  // common armor
        REQUIRE_FALSE(up.empty());
        CHECK(db.findItem(up)->slot == content::EquipSlot::Armor);
    }
    // Legendary input: the ghost declines.
    CHECK(armoryGhostUpgrade(db, "worldbreaker_axe", 0).empty());  // legendary weapon
    // A consumable is not a valid trade-in.
    CHECK(armoryGhostUpgrade(db, "potion", 0).empty());
    // Unknown id.
    CHECK(armoryGhostUpgrade(db, "no_such_item", 0).empty());
    // Deterministic.
    CHECK(armoryGhostUpgrade(db, "iron_sword", 5) == armoryGhostUpgrade(db, "iron_sword", 5));
}

TEST_CASE("theme rites: Elder Root price is affordable and its XP is elite-battle sized",
          "[theme-events]") {
    const content::ContentDatabase db = loadContent();
    CHECK(elderRootPrice(1, 1) > 0);
    CHECK(elderRootPrice(1, 1) < elderRootPrice(7, 20));  // rises with town + depth
    // A reference "one elite battle": the party earns each enemy's flat xpReward,
    // so a 2-elite team using the strongest elite is a generous upper bound.
    int topEnemyXp = 0;
    for (const auto& [id, def] : db.enemies()) {
        (void)id;
        if (def.xpReward > topEnemyXp) {
            topEnemyXp = def.xpReward;
        }
    }
    REQUIRE(topEnemyXp > 0);
    for (int t = 1; t <= 7; ++t) {
        for (int d = 1; d <= 20; d += 4) {
            const int xp = elderRootXp(t, d);
            INFO("town=" << t << " depth=" << d << " xp=" << xp << " topEnemyXp=" << topEnemyXp);
            CHECK(xp >= topEnemyXp / 2);   // at least half a single elite
            CHECK(xp <= topEnemyXp * 4);   // never a farm (a couple elite battles at most)
        }
    }
}

#endif  // CRYSTAL_TEST_DATA_DIR

// ============================ M106: the Goosy Gauntlet ========================

TEST_CASE("goosy: the theme ships town-gated with its own roster", "[goosy][m106]") {
    const content::ContentDatabase db = loadContent();
    const content::DungeonThemeDef* goosy = db.findTheme("goosy_gauntlet");
    REQUIRE(goosy != nullptr);
    CHECK(goosy->minTown == 7);
    // Every classic theme stays everywhere (the loader default).
    CHECK(db.findTheme("ruined_keep")->minTown == 1);
    CHECK(db.findTheme("crystal_mine")->minTown == 1);
    CHECK(db.findTheme("hollow_forest")->minTown == 1);
    // The three new bosses are town-7-gated, never Guild Masters, and join
    // the rush roster (the owner's ask).
    const std::vector<std::string> rush = bossRushOrder(db);
    for (const char* id : {"the_gray_gander", "mother_of_ponds", "the_pondlord"}) {
        INFO(id);
        const content::BossDef* b = db.findBoss(id);
        REQUIRE(b != nullptr);
        CHECK(b->minTown == 7);
        CHECK(b->guildTown == 0);
        CHECK(std::find(rush.begin(), rush.end(), std::string(id)) != rush.end());
    }
}

TEST_CASE("goosy: goosy bosses lead, and the Flock rolls at the leveled rate",
          "[goosy][m106]") {
    const content::ContentDatabase db = loadContent();
    const content::DungeonThemeDef* goosy = db.findTheme("goosy_gauntlet");
    REQUIRE(goosy != nullptr);
    int rites = 0;
    constexpr int kSeeds = 200;
    for (std::uint64_t seed = 1; seed <= kSeeds; ++seed) {
        const dungeon::Dungeon d = dungeon::generate(seed, 8, db, "goosy_gauntlet", 7);
        // The boss comes from the goosy list.
        bool bossListed = false;
        for (const dungeon::EnemyTeam& t : d.teams) {
            if (t.isBoss) {
                bossListed = std::find(goosy->bosses.begin(), goosy->bosses.end(), t.bossId) !=
                             goosy->bosses.end();
            }
        }
        CHECK(bossListed);
        int perFloor = 0;
        for (const dungeon::Room& r : d.rooms) {
            if (r.event.kind == dungeon::RoomEventKind::GoosyFlock) {
                ++perFloor;
            }
        }
        CHECK(perFloor <= 1);  // leveled: a rare roll, never stacked
        rites += perFloor;
    }
    // Leveled at v22, equal-tier at v23: the Flock rolls at
    // kEncounterChancePct like every other encounter — the +300 is a find,
    // not a per-floor stipend.
    INFO("rites=" << rites << "/" << kSeeds);
    CHECK(rites >= 3);
    CHECK(rites <= 45);
}

TEST_CASE("goosy: the flock turns everyone and turns everyone back", "[goosy][m106]") {
    const content::ContentDatabase db = loadContent();
    Party p;
    p.members.push_back(createCharacter(*db.findClass("knight"), "Rolan", 20));
    p.members.push_back(createCharacter(*db.findClass("mage"), "Mira", 20));
    p.members[0].equippedHeirloom = "heirloom_hearthstone";
    refreshCharacter(p.members[0], db);
    const int mageMaxBefore = p.members[1].maxHp;

    const DragonformStash stash = enterFlockGooseform(p, db);
    REQUIRE(stash.original.size() == 2);
    for (const Character& m : p.members) {
        CHECK(m.classId == "goose");
    }
    CHECK(p.members[0].equippedHeirloom == "heirloom_hearthstone");  // memories stay
    p.members[1].hp = 0;  // the mage-goose falls
    leaveDragonform(p, stash);
    CHECK(p.members[0].classId == "knight");
    CHECK(p.members[1].classId == "mage");
    CHECK(p.members[1].hp == 0);              // KO maps to KO
    CHECK(p.members[1].maxHp == mageMaxBefore);
}

TEST_CASE("goosy: flock battles pay +300 each, itemized", "[goosy][m106]") {
    score::RunSummary run;
    run.completed = true;
    run.battleTurns = 20;
    const int base = score::computeScore(run);
    run.gooseFlockFights = 1;
    CHECK(score::computeScore(run) == base + 300);
    CHECK(score::scoreBreakdown(run).gooseFlock == 300);
}
