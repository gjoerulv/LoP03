#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <set>
#include <sstream>
#include <string>
#include <vector>

#include "battle/Battle.hpp"
#include "battle/Simulator.hpp"
#include "content/ContentDatabase.hpp"
#include "content/ContentLoader.hpp"
#include "content/Definitions.hpp"
#include "content/LoadReport.hpp"
#include "dungeon/DungeonGenerator.hpp"
#include "game/Achievements.hpp"
#include "game/Castle.hpp"
#include "game/Guild.hpp"
#include "game/ItemCaps.hpp"
#include "game/Party.hpp"
#include "game/Spoils.hpp"
#include "save/SaveSystem.hpp"

// M84 — Guild Masters & town milestones: the perk table against the owner's
// 2026-08-05 decision, the per-town records and their save round-trip, the
// deterministic gauntlet teams at the derived depth-20 threat, the Masters'
// exclusion from every roster they must not join, the Endless Rush's
// every-10th-wave boss draw, the Mind-the-Spoon omen post-pass, and the
// sim-backed clearability evidence at the castle bar.

using namespace cd;

namespace {

content::ContentDatabase loadContent() {
    content::ContentDatabase db;
    content::LoadReport rep;
    content::loadAll(std::filesystem::path(CRYSTAL_TEST_DATA_DIR), db, rep);
    return db;
}

// The castle battery's maxed endgame party (L99, top gear, best passives).
Party maxedParty(const content::ContentDatabase& db) {
    Party p;
    struct Loadout {
        const char* cls;
        const char* passive;
    };
    const Loadout loadouts[] = {
        {"knight", "counter_attack"},
        {"ranger", "keen_senses"},
        {"mage", "spell_ward"},
        {"cleric", "clarity"},
    };
    for (const Loadout& l : loadouts) {
        if (const content::ClassDef* c = db.findClass(l.cls)) {
            Character ch = createCharacter(*c, l.cls, kMaxLevel);
            ch.weapon = std::string(l.cls) == "mage" ? "voidpiercer_rod" : "worldbreaker_axe";
            ch.armor = "aegis_eternal";
            ch.accessory = "titanforged_heart";
            ch.equippedPassive = l.passive;
            refreshCharacter(ch, db);
            p.members.push_back(ch);
        }
    }
    return p;
}

void carryBack(Party& p, const battle::Battle& b) {
    for (const battle::Combatant& u : b.units) {
        if (u.side == battle::Side::Party && u.partyIndex >= 0 &&
            u.partyIndex < static_cast<int>(p.members.size())) {
            p.members[static_cast<std::size_t>(u.partyIndex)].hp = u.hp > 0 ? u.hp : 0;
            p.members[static_cast<std::size_t>(u.partyIndex)].mp = u.mp;
        }
    }
}

// Chooses a town's perk on a fresh record set (simulating a first victory).
GuildRecords withPerk(const char* perkId) {
    GuildRecords g;
    const GuildPerkDef* p = findGuildPerk(perkId);
    REQUIRE(p != nullptr);
    GuildTownRecord& r = guildRecord(g, p->town);
    r.unlocked = true;
    r.bestTurns = 10;
    r.perkId = perkId;
    return g;
}

}  // namespace

// --- The perk table ---------------------------------------------------------

TEST_CASE("guild: the perk table is the owner's, two options per town", "[guild]") {
    CHECK(kGuildPerkCount == 14);
    std::set<std::string> ids;
    for (const GuildPerkDef& p : kGuildPerks) {
        CHECK(p.town >= 1);
        CHECK(p.town <= kTownCount);
        CHECK(ids.insert(p.id).second);  // unique, stable ids
        CHECK(p.magnitude > 0);
    }
    for (int town = 1; town <= kTownCount; ++town) {
        const auto pair = guildPerkPair(town);
        REQUIRE(pair.first != nullptr);
        REQUIRE(pair.second != nullptr);
        CHECK(pair.first != pair.second);
    }
    // The exact owner table, option A / option B per town.
    const auto effectOf = [](const char* id) { return findGuildPerk(id)->effect; };
    CHECK(effectOf("t1_pockets") == GuildPerkEffect::MaxItems);
    CHECK(effectOf("t1_lessons") == GuildPerkEffect::ExpBonus);
    CHECK(findGuildPerk("t1_lessons")->magnitude == 10);
    CHECK(effectOf("t2_cartographers") == GuildPerkEffect::MapChance);
    CHECK(findGuildPerk("t2_cartographers")->magnitude == 5);
    CHECK(effectOf("t2_pockets") == GuildPerkEffect::MaxItems);
    CHECK(effectOf("t3_bounty") == GuildPerkEffect::EnemyGold);
    CHECK(findGuildPerk("t3_bounty")->magnitude == 10);
    CHECK(effectOf("t3_whispers") == GuildPerkEffect::BlackMarket);
    CHECK(effectOf("t4_trapsense") == GuildPerkEffect::TrapGuard);
    CHECK(findGuildPerk("t4_trapsense")->magnitude == 5);
    CHECK(effectOf("t4_appraisal") == GuildPerkEffect::ChestGold);
    CHECK(findGuildPerk("t4_appraisal")->magnitude == 15);
    CHECK(effectOf("t5_spoon") == GuildPerkEffect::SpoonOmen);
    CHECK(effectOf("t5_cartographers") == GuildPerkEffect::MapChance);
    CHECK(effectOf("t6_pockets") == GuildPerkEffect::MaxItems);
    CHECK(effectOf("t6_lessons") == GuildPerkEffect::ExpBonus);
    CHECK(effectOf("t7_patronage") == GuildPerkEffect::TokenPrice);
    CHECK(findGuildPerk("t7_patronage")->magnitude == 1);
    CHECK(effectOf("t7_mastery") == GuildPerkEffect::ExpBonus);
    CHECK(findGuildPerk("t7_mastery")->magnitude == 15);
    // The town-5 option A is worded cryptically: the description never names
    // the relic event it feeds (owner: "worded cryptically").
    const std::string cryptic = findGuildPerk("t5_spoon")->description;
    CHECK(cryptic.find("relic") == std::string::npos);
    CHECK(cryptic.find("Relic") == std::string::npos);
}

// --- Records & effect queries -----------------------------------------------

TEST_CASE("guild: records improve like the castle's, and pending perks queue", "[guild]") {
    GuildTownRecord r;
    CHECK_FALSE(r.defeated());
    CHECK(guildImproved(r, 20));       // first victory
    CHECK_FALSE(guildImproved(r, 0));  // a non-victory never improves
    r.bestTurns = 20;
    CHECK(r.defeated());
    CHECK(guildImproved(r, 15));
    CHECK_FALSE(guildImproved(r, 25));

    GuildRecords g;
    CHECK(guildPendingPerkTown(g) == 0);
    guildRecord(g, 3).bestTurns = 12;  // defeated, no choice stored
    guildRecord(g, 5).bestTurns = 9;
    CHECK(guildPendingPerkTown(g) == 3);  // lowest pending town first
    guildRecord(g, 3).perkId = "t3_bounty";
    CHECK(guildPendingPerkTown(g) == 5);
    guildRecord(g, 5).perkId = "t5_spoon";
    CHECK(guildPendingPerkTown(g) == 0);
}

TEST_CASE("guild: effect queries sum chosen perks and shrug off bad ids", "[guild]") {
    GuildRecords g;
    CHECK(guildCapBonus(g) == 0);
    CHECK(guildExpBonusPct(g) == 0);
    CHECK(guildTokenPrice(g) == kBlackMarketTokenPrice);

    // Three Deep Pockets ranks stack; EXP stacks across towns 1/6/7.
    guildRecord(g, 1).bestTurns = 10;
    guildRecord(g, 1).perkId = "t1_pockets";
    guildRecord(g, 2).bestTurns = 10;
    guildRecord(g, 2).perkId = "t2_pockets";
    guildRecord(g, 6).bestTurns = 10;
    guildRecord(g, 6).perkId = "t6_lessons";
    guildRecord(g, 7).bestTurns = 10;
    guildRecord(g, 7).perkId = "t7_mastery";
    CHECK(guildCapBonus(g) == 2);
    CHECK(guildExpBonusPct(g) == 25);  // 10 + 15
    // A tampered/unknown id counts as nothing rather than crashing.
    guildRecord(g, 4).bestTurns = 10;
    guildRecord(g, 4).perkId = "not_a_perk";
    CHECK(guildCapBonus(g) == 2);

    CHECK(guildTokenPrice(withPerk("t7_patronage")) == 1);
    CHECK(guildMapBonusPct(withPerk("t2_cartographers")) == 5);
    CHECK(guildEnemyGoldPct(withPerk("t3_bounty")) == 10);
    CHECK(guildBlackMarketPct(withPerk("t3_whispers")) == 10);
    CHECK(guildTrapGuardPct(withPerk("t4_trapsense")) == 5);
    CHECK(guildChestGoldPct(withPerk("t4_appraisal")) == 15);
    CHECK(guildSpoonOmenPct(withPerk("t5_spoon")) == 5);
}

TEST_CASE("guild: perks feed the M78 caps and the battle spoils", "[guild]") {
    const content::ContentDatabase db = loadContent();
    // Deep Pockets widens every consumable cap by its rank, ceiling 9.
    const content::ItemDef* potion = db.findItem("potion");
    const content::ItemDef* remedy = db.findItem("antidote");  // display name "Remedy"
    REQUIRE(potion != nullptr);
    REQUIRE(remedy != nullptr);
    CHECK(capFor(*potion, 1) == 9);  // authored 9 stays 9 (hard ceiling)
    CHECK(capFor(*remedy, 0) == 2);
    CHECK(capFor(*remedy, 2) == 4);

    // Battle EXP and gold pick the perks up inside applySpoils, so the panel
    // shows exactly what was granted.
    Party p;
    p.members.push_back(createCharacter(*db.findClass("knight"), "Rolan", 5));
    p.guild = withPerk("t1_lessons");  // +10% EXP
    guildRecord(p.guild, 3).bestTurns = 10;
    guildRecord(p.guild, 3).perkId = "t3_bounty";  // +10% gold
    BattleSpoils spoils;
    spoils.xp = 100;
    spoils.gold = 100;
    const int goldBefore = p.gold;
    const SpoilsResult out = applySpoils(p, spoils, db);
    CHECK(out.xp == 110);
    CHECK(out.gold == 110);
    CHECK(p.gold == goldBefore + 110);
}

// --- The gauntlet teams -----------------------------------------------------

TEST_CASE("guild: exactly one Master per town, court authored and boss-only", "[guild]") {
    const content::ContentDatabase db = loadContent();
    for (int town = 1; town <= kTownCount; ++town) {
        const content::BossDef* master = findGuildMaster(db, town);
        REQUIRE(master != nullptr);
        CHECK(master->guildTown == town);
        CHECK(master->minions.size() >= 1);
        CHECK(master->minions.size() <= 3);
        for (const std::string& id : master->minions) {
            const content::EnemyDef* minion = db.findEnemy(id);
            REQUIRE(minion != nullptr);
            CHECK(minion->bossOnly);  // the court appears nowhere else
        }
        const dungeon::EnemyTeam team = guildMasterTeam(db, town);
        CHECK(team.isBoss);
        CHECK(team.bossId == master->id);
        CHECK(team.enemyIds == master->minions);
        CHECK(team.statScalePct == guildScalePct(db, town));
    }
}

TEST_CASE("guild: the threat bar is depth-20 at the town's own scaling", "[guild]") {
    const content::ContentDatabase db = loadContent();
    const int depthPct = 100 + db.composition().statScalePct(kGuildThreatDepth);
    for (int town = 1; town <= kTownCount; ++town) {
        CHECK(guildScalePct(db, town) == combineTownScale(depthPct, town));
    }
    // Documents today's endpoints; the check above derives them.
    CHECK(guildScalePct(db, 1) == 190);
    CHECK(guildScalePct(db, 7) == 570);
}

TEST_CASE("guild: the trial wave is five, seeded, and town-gated", "[guild]") {
    const content::ContentDatabase db = loadContent();
    for (int town : {1, 4, 7}) {
        const dungeon::EnemyTeam a = guildWaveTeam(db, town);
        const dungeon::EnemyTeam b = guildWaveTeam(db, town);
        CHECK(a.enemyIds == b.enemyIds);  // the same trial every attempt
        CHECK(static_cast<int>(a.enemyIds.size()) == kGuildWaveSize);
        CHECK(a.statScalePct == guildScalePct(db, town));
        CHECK(a.bossId.empty());
        for (const std::string& id : a.enemyIds) {
            const content::EnemyDef* e = db.findEnemy(id);
            REQUIRE(e != nullptr);
            CHECK_FALSE(e->bossOnly);
            CHECK(e->minTown <= town);  // the town's UNLOCKED pool (M38 gate)
        }
    }
    // The pool genuinely widens with the town gate.
    CHECK(guildTownPool(db, 1).size() < guildTownPool(db, 7).size());
}

TEST_CASE("guild: Masters stay out of every roster that is not theirs", "[guild]") {
    const content::ContentDatabase db = loadContent();
    std::set<std::string> masters;
    for (const auto& [id, def] : db.bosses()) {
        if (def.guildTown != 0) {
            masters.insert(id);
        }
    }
    REQUIRE(masters.size() == 7);
    // Not in the Boss Rush (the kKingBossId rule, as data).
    for (const std::string& id : bossRushOrder(db)) {
        CHECK(masters.count(id) == 0);
    }
    // Not authored into any dungeon theme's boss list.
    for (const auto& [themeId, theme] : db.themes()) {
        (void)themeId;
        for (const std::string& id : theme.bosses) {
            CHECK(masters.count(id) == 0);
        }
    }
    // Not generated into a dungeon — including the theme-less fallback sweep.
    for (const std::uint64_t seed : {1ull, 77ull, 424242ull}) {
        for (int town : {1, 4, 7}) {
            for (const std::string& themeId : {std::string(""), std::string("ruined_keep")}) {
                const dungeon::Dungeon d =
                    dungeon::generate(seed, 20, db, themeId, town);
                for (const dungeon::EnemyTeam& t : d.teams) {
                    if (t.isBoss) {
                        CHECK(masters.count(t.bossId) == 0);
                    }
                }
            }
        }
    }
}

// --- Endless Rush: every 10th wave ------------------------------------------

TEST_CASE("guild: every 10th endless wave fields a boss with its court", "[guild]") {
    const content::ContentDatabase db = loadContent();
    for (int w : {9, 19, 29, 99}) {  // 0-based: waves 10, 20, 30, 100
        const dungeon::EnemyTeam a = endlessWaveTeam(db, w);
        const dungeon::EnemyTeam b = endlessWaveTeam(db, w);
        CHECK(a.bossId == b.bossId);  // reproducible
        CHECK(a.isBoss);
        const content::BossDef* boss = db.findBoss(a.bossId);
        REQUIRE(boss != nullptr);
        CHECK(a.enemyIds == boss->minions);  // its usual court
        CHECK(a.statScalePct == endlessWaveScalePct(w));
        // Never the King or the Duck — they keep their own arenas.
        CHECK(a.bossId != std::string(kKingBossId));
        CHECK(a.bossId != std::string(kDuckBossId));
    }
    // Ordinary waves are untouched by the rule.
    for (int w : {0, 8, 10, 18, 20}) {
        const dungeon::EnemyTeam t = endlessWaveTeam(db, w);
        CHECK(t.bossId.empty());
        CHECK(static_cast<int>(t.enemyIds.size()) == endlessWaveSize(w));
    }
    // The draw spans both rosters over many boss waves: some Guild Master
    // eventually takes a 10th wave (deterministic, so this is a fixed fact of
    // the shipped content, not a flaky roll).
    bool sawMaster = false;
    for (int i = 0; i < 40 && !sawMaster; ++i) {
        const dungeon::EnemyTeam t = endlessWaveTeam(db, i * 10 + 9);
        if (const content::BossDef* boss = db.findBoss(t.bossId)) {
            sawMaster = boss->guildTown != 0;
        }
    }
    CHECK(sawMaster);
}

// --- Mind the Spoon (the omen post-pass) ------------------------------------

TEST_CASE("guild: the omen upgrades one plain event per floor, deterministically",
          "[guild]") {
    const content::ContentDatabase db = loadContent();
    const std::uint64_t runSeed = 20260806ull;
    const auto makeFloors = [&]() {
        return dungeon::generateFloors(runSeed, 8, db, "ruined_keep", 5, 4);
    };
    const auto relicCount = [](const dungeon::Dungeon& d) {
        int n = 0;
        for (const dungeon::Room& room : d.rooms) {
            if (room.event.kind == dungeon::RoomEventKind::RoyalRelic) {
                ++n;
            }
        }
        return n;
    };

    // pct 0 is a byte-level no-op.
    std::vector<dungeon::Dungeon> untouched = makeFloors();
    std::vector<dungeon::Dungeon> zero = makeFloors();
    applyGuildRelicOmen(zero, runSeed, 0);
    for (std::size_t f = 0; f < untouched.size(); ++f) {
        for (std::size_t r = 0; r < untouched[f].rooms.size(); ++r) {
            CHECK(zero[f].rooms[r].event.kind == untouched[f].rooms[r].event.kind);
        }
    }

    // pct 100 upgrades exactly one plain event on every floor that lacked a
    // relic, keeps the M44 at-most-one rule, and is deterministic + idempotent.
    std::vector<dungeon::Dungeon> omen = makeFloors();
    applyGuildRelicOmen(omen, runSeed, 100);
    std::vector<dungeon::Dungeon> again = makeFloors();
    applyGuildRelicOmen(again, runSeed, 100);
    for (std::size_t f = 0; f < omen.size(); ++f) {
        CHECK(relicCount(omen[f]) <= 1);
        CHECK(relicCount(omen[f]) >= relicCount(untouched[f]));
        for (std::size_t r = 0; r < omen[f].rooms.size(); ++r) {
            CHECK(omen[f].rooms[r].event.kind == again[f].rooms[r].event.kind);
            // Nothing but a plain rolled kind may have changed.
            if (omen[f].rooms[r].event.kind != untouched[f].rooms[r].event.kind) {
                CHECK(omen[f].rooms[r].event.kind == dungeon::RoomEventKind::RoyalRelic);
                switch (untouched[f].rooms[r].event.kind) {
                    case dungeon::RoomEventKind::Shrine:
                    case dungeon::RoomEventKind::HealingSpring:
                    case dungeon::RoomEventKind::Merchant:
                    case dungeon::RoomEventKind::ScoreWager:
                    case dungeon::RoomEventKind::RestToken:
                        break;
                    default:
                        FAIL("omen upgraded a protected event slot");
                }
            }
        }
    }
    // Applying the omen twice adds nothing (the at-most-one guard).
    std::vector<dungeon::Dungeon> twice = omen;
    applyGuildRelicOmen(twice, runSeed, 100);
    for (std::size_t f = 0; f < omen.size(); ++f) {
        CHECK(relicCount(twice[f]) == relicCount(omen[f]));
    }
}

// --- Save round-trip & the achievement --------------------------------------

TEST_CASE("guild: records round-trip the save; tampering degrades", "[guild][save]") {
    const content::ContentDatabase db = loadContent();
    const std::filesystem::path dir =
        std::filesystem::temp_directory_path() / "crystal_guild_save_test";
    std::filesystem::remove_all(dir);
    save::SaveSystem saves(db, dir);

    Party p;
    p.members.push_back(createCharacter(*db.findClass("knight"), "Rolan", 5));
    guildRecord(p.guild, 2).unlocked = true;
    guildRecord(p.guild, 3).unlocked = true;
    guildRecord(p.guild, 3).bestTurns = 14;
    guildRecord(p.guild, 3).perkId = "t3_bounty";
    guildRecord(p.guild, 7).bestTurns = 22;  // defeated but unlock bit unset

    content::LoadReport rep;
    REQUIRE(saves.save(save::SaveSlot::Manual1, p, rep));
    Party loaded;
    REQUIRE(saves.load(save::SaveSlot::Manual1, loaded, rep));
    CHECK(guildRecord(loaded.guild, 1).unlocked == false);
    CHECK(guildRecord(loaded.guild, 2).unlocked == true);
    CHECK(guildRecord(loaded.guild, 3).bestTurns == 14);
    CHECK(guildRecord(loaded.guild, 3).perkId == "t3_bounty");
    CHECK(guildRecord(loaded.guild, 7).unlocked == true);  // defeat proves the audience

    // Tamper: a perk id stored on the wrong town is dropped (the choice simply
    // re-offers), never trusted.
    const std::filesystem::path file = saves.slotPath(save::SaveSlot::Manual1);
    std::ifstream in(file, std::ios::binary);
    std::stringstream buf;
    buf << in.rdbuf();
    in.close();
    std::string text = buf.str();
    const std::string from = "\"guildPerk3\": \"t3_bounty\"";
    const std::string to = "\"guildPerk3\": \"t5_spoon\"";
    const std::size_t at = text.find(from);
    REQUIRE(at != std::string::npos);
    text.replace(at, from.size(), to);
    std::ofstream out(file, std::ios::binary | std::ios::trunc);
    out << text;
    out.close();
    Party tampered;
    REQUIRE(saves.load(save::SaveSlot::Manual1, tampered, rep));
    CHECK(guildRecord(tampered.guild, 3).perkId.empty());
    CHECK(guildPendingPerkTown(tampered.guild) == 3);  // it re-asks
    std::filesystem::remove_all(dir);
}

TEST_CASE("guild: Guildbane fires on any Master's first fall", "[guild]") {
    Party p;
    CHECK_FALSE(achievementMet("guildbane", p, AchvContext{}));
    guildRecord(p.guild, 4).bestTurns = 18;
    CHECK(achievementMet("guildbane", p, AchvContext{}));
    CHECK(findAchievement("guildbane") != nullptr);
}

// --- Clearability evidence (the castle-battery bar) -------------------------

TEST_CASE("guild: every gauntlet is clearable by the maxed party", "[guild]") {
    const content::ContentDatabase db = loadContent();
    for (int town = 1; town <= kTownCount; ++town) {
        Party party = maxedParty(db);
        int totalRounds = 0;
        bool cleared = true;
        for (const dungeon::EnemyTeam& team :
             {guildWaveTeam(db, town), guildMasterTeam(db, town)}) {
            battle::Battle b = battle::buildBattle(party, team, db);
            const battle::SimResult r = battle::simulateInPlace(b, db, 400);
            totalRounds += r.rounds;
            if (r.outcome != battle::Outcome::Victory) {
                cleared = false;
                break;
            }
            carryBack(party, b);
        }
        std::cout << "[guild] town " << town << " gauntlet: "
                  << (cleared ? "CLEARED" : "LOST") << " in " << totalRounds
                  << " rounds at " << guildScalePct(db, town) << "%\n";
        CHECK(cleared);  // the maxed party is the bar every Master must clear
    }
}
