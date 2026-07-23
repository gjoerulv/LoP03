#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <string>

#include "battle/Battle.hpp"
#include "battle/Simulator.hpp"
#include "content/ContentDatabase.hpp"
#include "content/ContentLoader.hpp"
#include "content/Definitions.hpp"
#include "content/LoadReport.hpp"
#include "content/Stats.hpp"
#include "game/BlackMarket.hpp"
#include "game/Castle.hpp"
#include "input/InputMap.hpp"
#include "settings/Settings.hpp"
#include "states/BattleLog.hpp"
#include "states/BestiaryStats.hpp"
#include "states/EquipDiff.hpp"

// M52 — Comforts & secrets: the ambience slider, the battle log, the equip-shop
// diff helpers, the bestiary max-stats math, the Dragon Crown's hidden effect,
// and the high-stakes black-market roll.

using namespace cd;
using namespace cd::battle;

namespace {

content::ContentDatabase loadContent() {
    content::ContentDatabase db;
    content::LoadReport rep;
    content::loadAll(std::filesystem::path(CRYSTAL_TEST_DATA_DIR), db, rep);
    return db;
}

// --- A hand-built King fight, mirroring test_kings_court -----------------------
Combatant king(int reviveTurns) {
    Combatant c;
    c.side = Side::Enemy;
    c.name = "The Hollow King";
    c.sourceId = "the_hollow_king";
    c.isBoss = true;
    c.stats = {900, 30, 30, 20, 20};
    c.maxHp = c.hp = 900;
    c.mp = c.maxMp = 200;
    c.reviveMinionTurns = reviveTurns;
    return c;
}

Combatant guard(const std::string& name) {
    Combatant c;
    c.side = Side::Enemy;
    c.name = name;
    c.sourceId = name;
    c.stats = {100, 10, 10, 10, 5};
    c.maxHp = c.hp = 100;
    c.mp = c.maxMp = 30;
    return c;
}

Combatant hero() {
    Combatant c;
    c.side = Side::Party;
    c.name = "Aldo";
    c.sourceId = "knight";
    c.stats = {200, 20, 10, 10, 10};
    c.maxHp = c.hp = 200;
    c.mp = c.maxMp = 20;
    return c;
}

// Unit 0 hero, unit 1 King, units 2-3 his court.
Battle court(int reviveTurns = 5) {
    Battle b;
    b.units.push_back(hero());
    b.units.push_back(king(reviveTurns));
    b.units.push_back(guard("Sword"));
    b.units.push_back(guard("Staff"));
    b.threat.assign(b.units.size(), 0);
    b.rngSeed = 0xC0117A11ull;
    return b;
}

void fell(Battle& b, std::size_t unit) { b.units[unit].hp = 0; }

std::string takeKingTurns(Battle& b, int n) {
    std::string spoke;
    for (int i = 0; i < n; ++i) {
        const std::string line = b.beginUnitTurn(1);
        if (!line.empty()) {
            spoke = line;
        }
    }
    return spoke;
}

// A minimal Dragon Crown: enemy-targeted, King-only, revive-disabling.
content::ItemDef makeCrown() {
    content::ItemDef d;
    d.id = "dragon_crown";
    d.name = "Dragon Crown";
    d.battleTarget = content::BattleTarget::Enemy;
    d.requiresBossId = "the_hollow_king";
    d.disablesMinionRevive = true;
    return d;
}

}  // namespace

// === E1: ambience volume slider ==============================================

TEST_CASE("comforts: ambienceVolume round-trips, defaults to 0.5 absent, clamps",
          "[comforts][settings]") {
    using namespace cd::settings;

    Settings values;
    values.ambienceVolume = 0.3f;
    InputMap map;
    const std::string text = serializeSettings(values, map);

    Settings loaded;
    InputMap lm;
    content::LoadReport rep;
    REQUIRE(parseSettingsText(text, loaded, lm, rep));
    CHECK(rep.errorCount() == 0);
    CHECK(loaded.ambienceVolume == 0.3f);

    // A pre-M52 audio block (no ambience key) keeps the 0.5 default.
    Settings old;
    content::LoadReport rep2;
    REQUIRE(parseSettingsText(R"({"version":1,"audio":{"master":1.0,"music":1.0,"sfx":1.0}})", old,
                              lm, rep2));
    CHECK(old.ambienceVolume == 0.5f);

    // Out-of-range values clamp to 0..1.
    Settings hi;
    content::LoadReport rep3;
    REQUIRE(parseSettingsText(R"({"version":1,"audio":{"ambience":5.0}})", hi, lm, rep3));
    CHECK(hi.ambienceVolume == 1.0f);
    Settings lo;
    content::LoadReport rep4;
    REQUIRE(parseSettingsText(R"({"version":1,"audio":{"ambience":-2.0}})", lo, lm, rep4));
    CHECK(lo.ambienceVolume == 0.0f);
}

// === E2: the battle log ring buffer ==========================================

TEST_CASE("comforts: the battle log keeps the last 30 lines in order", "[comforts][battlelog]") {
    BattleLog log;
    CHECK(log.empty());

    for (int i = 0; i < 35; ++i) {
        log.push("line " + std::to_string(i));
    }
    CHECK(log.size() == BattleLog::kMaxEntries);
    CHECK(log.size() == 30);
    // The oldest five (0..4) were evicted; order is preserved.
    CHECK(log.entries().front() == "line 5");
    CHECK(log.entries().back() == "line 34");

    // Empty lines (nothing was shown) are ignored.
    const std::size_t before = log.size();
    log.push("");
    CHECK(log.size() == before);
}

// === E3: the equip-shop diff helpers =========================================

TEST_CASE("comforts: equip diff formats summaries, deltas, and signs",
          "[comforts][equip]") {
    using content::StatBlock;
    const StatBlock sword{0, 10, 0, 0, 2};   // ATK+10 SPD+2
    const StatBlock dagger{0, 7, 0, 0, 4};   // ATK+7  SPD+4

    CHECK(equip::statBonusSummary(sword) == "ATK+10 SPD+2");
    CHECK(equip::statBonusSummary(StatBlock{}).empty());

    // Unequip: the diff is the whole bonus, negated.
    CHECK(equip::bonusDelta(StatBlock{}, sword) == "ATK -10  SPD -2");
    CHECK(equip::deltaSign(StatBlock{}, sword) < 0);

    // dagger vs sword: ATK -3, SPD +2 -> nets -1.
    CHECK(equip::bonusDelta(dagger, sword) == "ATK -3  SPD +2");
    CHECK(equip::deltaSign(dagger, sword) < 0);

    // No change: empty diff, zero sign.
    CHECK(equip::bonusDelta(sword, sword).empty());
    CHECK(equip::deltaSign(sword, sword) == 0);

    // A pure upgrade reads positive.
    CHECK(equip::deltaSign(sword, StatBlock{}) > 0);

    // Per-stat deltas (for the coloured panel): all five, in display order.
    const auto deltas = equip::statDeltas(dagger, sword);
    REQUIRE(deltas.size() == 5);
    CHECK(std::string(deltas[0].tag) == "HP");
    CHECK(deltas[0].value == 0);   // unchanged -> normal colour
    CHECK(std::string(deltas[1].tag) == "ATK");
    CHECK(deltas[1].value == -3);  // loss -> coral
    CHECK(deltas[4].value == 2);   // SPD gain -> green
}

// === E4: bestiary max-stats math =============================================

TEST_CASE("comforts: scaledStats multiplies each field by pct/100 (integer floor)",
          "[comforts][bestiary]") {
    using content::StatBlock;
    const StatBlock base{100, 20, 30, 10, 8};
    const StatBlock s = content::scaledStats(base, 570);
    CHECK(s.maxHp == 570);
    CHECK(s.attack == 114);
    CHECK(s.magic == 171);
    CHECK(s.defense == 57);
    CHECK(s.speed == 45);  // 8 * 570 / 100 = 45.6 -> 45

    const StatBlock identity = content::scaledStats(base, 100);
    CHECK(identity.maxHp == base.maxHp);
    CHECK(identity.speed == base.speed);
}

TEST_CASE("comforts: foeMaxScalePct maps the four real fight contexts",
          "[comforts][bestiary]") {
    const int floorPct = 570;  // the shipped dungeon ceiling
    // A regular enemy (incl. elite): the dungeon ceiling.
    CHECK(foeMaxScalePct(false, false, false, floorPct) == floorPct);
    // A bossOnly foe (a Royal Guard): the King's scale.
    CHECK(foeMaxScalePct(false, true, false, floorPct) == kKingScalePct);
    // Any other boss: the greater of the ceiling and the Boss Rush.
    CHECK(foeMaxScalePct(true, false, false, floorPct) == std::max(floorPct, kBossRushScalePct));
    CHECK(foeMaxScalePct(true, false, false, floorPct) == kBossRushScalePct);  // 580 > 570
    // The King: his own scale, even though he is a boss.
    CHECK(foeMaxScalePct(true, false, true, floorPct) == kKingScalePct);
}

TEST_CASE("comforts: the shipped ceiling is 570 so the readout scales agree",
          "[comforts][bestiary]") {
    content::ContentDatabase db = loadContent();
    // Pins the composition + ladder the bestiary and buildBattle both scale from.
    CHECK(castleFloorScalePct(db) == 570);
}

// === E5: the Dragon Crown's hidden effect ====================================

TEST_CASE("comforts: the Crown ends the King's revive clock when used on him",
          "[comforts][crown][battle]") {
    Battle b = court();
    const content::ItemDef crown = makeCrown();

    REQUIRE(itemAffects(b, 1, crown));  // it does affect the King
    b.useItem(0, 1, crown);
    CHECK(b.units[1].reviveMinionTurns == 0);
    CHECK(b.units[1].reviveMinionCounter == 0);

    // The court falls; no number of King turns brings them back.
    fell(b, 2);
    fell(b, 3);
    CHECK(takeKingTurns(b, 12).empty());
    CHECK_FALSE(b.units[2].alive());
    CHECK_FALSE(b.units[3].alive());
}

TEST_CASE("comforts: the Crown does nothing off the King and is kept",
          "[comforts][crown][battle]") {
    Battle b = court();
    const content::ItemDef crown = makeCrown();

    CHECK_FALSE(itemAffects(b, 2, crown));  // a guard: keep it, do not spend
    const std::string log = b.useItem(0, 2, crown);
    CHECK(log.find("Nothing happens") != std::string::npos);
    CHECK(b.units[1].reviveMinionTurns == 5);  // the King's clock is untouched

    // And that clock still fires normally, proving the Crown is what stops it.
    fell(b, 2);
    fell(b, 3);
    CHECK_FALSE(takeKingTurns(b, 5).empty());
    CHECK(b.units[2].alive());
}

TEST_CASE("comforts: the effect is hidden and drove battle rules to 10",
          "[comforts][crown]") {
    CHECK(kBattleRulesVersion == 10);

    content::ContentDatabase db = loadContent();
    const content::ItemDef* crown = db.findItem("dragon_crown");
    REQUIRE(crown != nullptr);
    CHECK(crown->disablesMinionRevive);
    CHECK(crown->battleTarget == content::BattleTarget::Enemy);
    CHECK(crown->requiresBossId == "the_hollow_king");
    // The description gives nothing away about the court.
    CHECK(crown->description.find("court") == std::string::npos);
    CHECK(crown->description.find("guard") == std::string::npos);
    CHECK(crown->description.find("revive") == std::string::npos);
}

TEST_CASE("comforts: a full sim keeps the crowned King's court down (sim == live)",
          "[comforts][crown][battle]") {
    content::ContentDatabase db = loadContent();

    Battle b = court();
    b.useItem(0, 1, makeCrown());  // the hero crowns the King first
    fell(b, 2);
    fell(b, 3);
    // simulateInPlace drives the same shared beginUnitTurn the battle screen
    // does, so with the clock zeroed the court can never return in either.
    simulateInPlace(b, db, 80);
    CHECK_FALSE(b.units[2].alive());
    CHECK_FALSE(b.units[3].alive());
}

// === E6: the high-stakes black-market roll ===================================

TEST_CASE("comforts: the 34% market spawns on a t7 d20 boss kill regardless of score/stakes",
          "[comforts][blackmarket]") {
    // A seed that wins the 34% roll at (town 7, depth 20), and one that loses it.
    std::uint64_t hit = 0;
    std::uint64_t miss = 0;
    for (std::uint64_t s = 1; s < 5000 && (hit == 0 || miss == 0); ++s) {
        const std::uint64_t seed = s * 2654435761u;
        if (blackMarketHighStakesRolls(seed, 7, 20)) {
            if (hit == 0) hit = seed;
        } else if (miss == 0) {
            miss = seed;
        }
    }
    REQUIRE(hit != 0);
    REQUIRE(miss != 0);

    // The 34% path needs only a completed boss kill at t7 d20 — a score floored to
    // 0 (completedWithScore=false) and no stakes raise (raisedStakes=false) both
    // still spawn it.
    CHECK(blackMarketShouldSpawn(false, true, false, 7, hit, 20));
    CHECK_FALSE(blackMarketShouldSpawn(false, true, false, 7, miss, 20));
    // A retreat/defeat (not completed) never spawns, even on a winning seed.
    CHECK_FALSE(blackMarketShouldSpawn(false, false, false, 7, hit, 20));
    // Below town 7 or depth 20 the high-stakes path never fires.
    CHECK_FALSE(blackMarketShouldSpawn(false, true, false, 6, hit, 20));
    CHECK_FALSE(blackMarketShouldSpawn(false, true, false, 7, hit, 19));
}

TEST_CASE("comforts: the 20% and 34% market rolls are independent streams",
          "[comforts][blackmarket]") {
    // Some seed makes the two rolls disagree at the same (town, depth).
    bool disagree = false;
    for (std::uint64_t s = 1; s < 20000 && !disagree; ++s) {
        const std::uint64_t seed = s * 2654435761u;
        disagree = blackMarketRolls(seed, 7, 20) != blackMarketHighStakesRolls(seed, 7, 20);
    }
    CHECK(disagree);

    // The high-stakes roll is deterministic (reload-proof).
    CHECK(blackMarketHighStakesRolls(12345u, 7, 20) == blackMarketHighStakesRolls(12345u, 7, 20));

    // And it lands near its 34% rate.
    int hits = 0;
    for (std::uint64_t s = 1; s <= 5000; ++s) {
        if (blackMarketHighStakesRolls(s * 2654435761u, 7, 20)) {
            ++hits;
        }
    }
    CHECK(hits > 1500);  // ~34% of 5000 = 1700
    CHECK(hits < 1900);
}
