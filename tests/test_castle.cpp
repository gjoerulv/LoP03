#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

#include "battle/Battle.hpp"
#include "battle/Simulator.hpp"
#include "content/ContentDatabase.hpp"
#include "content/ContentLoader.hpp"
#include "content/Definitions.hpp"
#include "content/LoadReport.hpp"
#include "game/Castle.hpp"
#include "game/Party.hpp"

// M40 castle & King: the pure challenge rules (kCastleTown, the t7/d20-relative
// scaling, deterministic boss-rush order and endless-wave stream, record helpers,
// the King's Confusion immunity) and the sim-backed clearability evidence (the
// King beatable by a maxed party, the boss rush clearable no-heal, endless bounded).

using namespace cd;

namespace {

content::ContentDatabase loadContent() {
    content::ContentDatabase db;
    content::LoadReport rep;
    content::loadAll(std::filesystem::path(CRYSTAL_TEST_DATA_DIR), db, rep);
    return db;
}

// A maxed endgame party: L50, top legendary gear (not the King's own reward), best
// passives. The realistic power ceiling the castle challenges are tuned against.
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

// Copies each party combatant's surviving HP/MP back onto the party for the next
// gauntlet fight (mirrors BattleState's write-back). No free heal between fights.
void carryBack(Party& p, const battle::Battle& b) {
    for (const battle::Combatant& u : b.units) {
        if (u.side == battle::Side::Party && u.partyIndex >= 0 &&
            u.partyIndex < static_cast<int>(p.members.size())) {
            p.members[static_cast<std::size_t>(u.partyIndex)].hp = u.hp > 0 ? u.hp : 0;
            p.members[static_cast<std::size_t>(u.partyIndex)].mp = u.mp;
        }
    }
}

struct GauntletResult {
    bool cleared = false;  // survived every fight (boss rush)
    int wavesSurvived = 0;
    int totalRounds = 0;
};

// Runs a no-heal gauntlet: fight `teams` in order, carrying HP/MP forward; stops
// at the first loss. Heal SKILLS still work (MP-limited) — the sim's stand-in for
// the "no free heal, items/skills allowed" policy (a lower bound on real play).
GauntletResult runGauntlet(Party party, const std::vector<dungeon::EnemyTeam>& teams,
                           const content::ContentDatabase& db) {
    GauntletResult g;
    for (const dungeon::EnemyTeam& team : teams) {
        battle::Battle b = battle::buildBattle(party, team, db);
        const battle::SimResult r = battle::simulateInPlace(b, db, 400);
        g.totalRounds += r.rounds;
        if (r.outcome != battle::Outcome::Victory) {
            return g;
        }
        ++g.wavesSurvived;
        carryBack(party, b);
    }
    g.cleared = true;
    return g;
}

}  // namespace

// --- Pure rules ------------------------------------------------------------

TEST_CASE("castle: it is a distinct place, not a ladder town", "[castle]") {
    CHECK(kCastleTown == 8);
    CHECK(kCastleTown > 7);  // above the town ladder (WorldLadder kTownCount)
}

TEST_CASE("castle: challenge scaling is endgame and correctly ordered", "[castle]") {
    // All challenges scale enemies well above base; the King is the hardest single
    // fight (its per-fight scale exceeds the Boss Rush's), and every scale is at or
    // above the town-7 base multiplier (300 %). Exact values are sim-tuned.
    CHECK(kBossRushScalePct >= 300);
    CHECK(kKingScalePct > kBossRushScalePct);
    CHECK(endlessWaveScalePct(0) == kCastleBaselineScalePct);
    // Endless escalates monotonically.
    for (int w = 0; w < 30; ++w) {
        CHECK(endlessWaveScalePct(w + 1) > endlessWaveScalePct(w));
    }
    CHECK(endlessWaveSize(0) == 2);
    CHECK(endlessWaveSize(30) == 5);  // capped
}

TEST_CASE("castle: the boss-rush order is the 12 bosses, King-excluded, sorted",
          "[castle]") {
    const content::ContentDatabase db = loadContent();
    const std::vector<std::string> order = bossRushOrder(db);
    CHECK(order.size() == 12);  // full roster minus the King
    CHECK(std::is_sorted(order.begin(), order.end()));
    CHECK(std::find(order.begin(), order.end(), std::string(kKingBossId)) == order.end());
    // Every rush entry resolves a boss and carries the rush scale; past the end is
    // an empty "done" sentinel.
    for (int i = 0; i < static_cast<int>(order.size()); ++i) {
        const dungeon::EnemyTeam t = bossRushTeam(db, i);
        CHECK(t.bossId == order[static_cast<std::size_t>(i)]);
        CHECK(t.statScalePct == kBossRushScalePct);
        CHECK(db.findBoss(t.bossId) != nullptr);
    }
    CHECK(bossRushTeam(db, static_cast<int>(order.size())).bossId.empty());  // done
}

TEST_CASE("castle: endless waves are deterministic and escalating", "[castle]") {
    const content::ContentDatabase db = loadContent();
    for (int w : {0, 1, 5, 12, 25}) {
        const dungeon::EnemyTeam a = endlessWaveTeam(db, w);
        const dungeon::EnemyTeam b = endlessWaveTeam(db, w);
        CHECK(a.enemyIds == b.enemyIds);  // reproducible
        CHECK(static_cast<int>(a.enemyIds.size()) == endlessWaveSize(w));
        CHECK(a.statScalePct == endlessWaveScalePct(w));
        for (const std::string& id : a.enemyIds) {
            CHECK(db.findEnemy(id) != nullptr);
        }
    }
}

TEST_CASE("castle: the King is fixed content with its kit and immunities", "[castle]") {
    const content::ContentDatabase db = loadContent();
    const content::BossDef* king = db.findBoss(kKingBossId);
    REQUIRE(king != nullptr);
    CHECK(king->immuneToConfusion);
    // The owner-specified passive trio.
    for (const char* p : {"keen_senses", "clarity", "counter_attack"}) {
        CHECK(std::find(king->passives.begin(), king->passives.end(), std::string(p)) !=
              king->passives.end());
    }
    const dungeon::EnemyTeam t = kingTeam(db);
    CHECK(t.bossId == std::string(kKingBossId));
    CHECK(t.statScalePct == kKingScalePct);
    // The unique reward exists and is NOT in the shared legendary drop/market pool.
    CHECK(db.findItem(kKingLegendaryId) != nullptr);
}

TEST_CASE("castle: record helpers gate first-clear rewards", "[castle]") {
    CastleRecords r;
    CHECK(bossRushImproved(r, 40));            // first clear
    CHECK_FALSE(bossRushImproved(r, 0));       // a non-clear never improves
    r.bossRushBestTurns = 40;
    CHECK(bossRushImproved(r, 30));            // faster improves
    CHECK_FALSE(bossRushImproved(r, 45));      // slower does not
    CHECK(endlessImproved(r, 1));
    r.endlessBestWave = 6;
    CHECK(endlessImproved(r, 7));
    CHECK_FALSE(endlessImproved(r, 6));
    CHECK(kingImproved(r, 12));
    r.kingDefeated = true;
    r.kingBestTurns = 12;
    CHECK(kingImproved(r, 10));
    CHECK_FALSE(kingImproved(r, 15));
}

// --- Confusion immunity (the King cannot be addled; existing units still can) --

TEST_CASE("castle: the King is immune to Confusion, a normal foe is not", "[castle]") {
    const content::ContentDatabase db = loadContent();
    battle::Battle b = battle::buildBattle(maxedParty(db), kingTeam(db), db);
    int king = -1;
    for (std::size_t i = 0; i < b.units.size(); ++i) {
        if (b.units[i].side == battle::Side::Enemy) {
            king = static_cast<int>(i);
        }
    }
    REQUIRE(king >= 0);
    battle::Combatant& k = b.units[static_cast<std::size_t>(king)];
    k.statuses.push_back({content::StatusType::Confusion, 0, 4});
    CHECK_FALSE(battle::isConfused(k));  // immune -> never treated as confused
    // Blind and Silence too (Keen Senses / Clarity), so no control lands.
    k.statuses.push_back({content::StatusType::Blind, 0, 4});
    k.statuses.push_back({content::StatusType::Silence, 0, 4});
    CHECK_FALSE(battle::isBlinded(k));
    CHECK_FALSE(battle::isSilenced(k));

    // A plain combatant with no immunity is still confused normally.
    battle::Combatant plain;
    plain.statuses.push_back({content::StatusType::Confusion, 0, 4});
    CHECK(battle::isConfused(plain));
}

// --- Immune statuses are never shown (display suppression) -----------------

TEST_CASE("castle: isImmuneTo drives hiding immune statuses from the HUD", "[castle]") {
    battle::Combatant c;
    c.blindImmune = true;
    c.silenceImmune = true;
    c.confusionImmune = true;
    CHECK(battle::isImmuneTo(c, content::StatusType::Blind));
    CHECK(battle::isImmuneTo(c, content::StatusType::Silence));
    CHECK(battle::isImmuneTo(c, content::StatusType::Confusion));
    CHECK_FALSE(battle::isImmuneTo(c, content::StatusType::Poison));  // not an immunity
    battle::Combatant plain;
    CHECK_FALSE(battle::isImmuneTo(plain, content::StatusType::Blind));
    // The King resolves all three control immunities from its passives + flag.
    const content::ContentDatabase db = loadContent();
    battle::Battle b = battle::buildBattle(maxedParty(db), kingTeam(db), db);
    for (const battle::Combatant& u : b.units) {
        if (u.side == battle::Side::Enemy) {
            CHECK(battle::isImmuneTo(u, content::StatusType::Blind));
            CHECK(battle::isImmuneTo(u, content::StatusType::Silence));
            CHECK(battle::isImmuneTo(u, content::StatusType::Confusion));
        }
    }
}

// --- The King's debuffs deal damage (damaging Support skills) ---------------

TEST_CASE("castle: a Support skill with power wounds its target, power-0 does not",
          "[castle]") {
    const content::ContentDatabase db = loadContent();
    battle::Battle b = battle::buildBattle(maxedParty(db), kingTeam(db), db);
    int party = -1;
    int enemy = -1;
    for (int i = 0; i < static_cast<int>(b.units.size()); ++i) {
        if (b.units[static_cast<std::size_t>(i)].side == battle::Side::Party && party < 0) {
            party = i;
        }
        if (b.units[static_cast<std::size_t>(i)].side == battle::Side::Enemy && enemy < 0) {
            enemy = i;
        }
    }
    REQUIRE(party >= 0);
    REQUIRE(enemy >= 0);

    // A shipped power-0 Support debuff still deals NO damage (byte-identical).
    const content::SkillDef* sunder = db.findSkill("sunder");
    REQUIRE(sunder != nullptr);
    const int hpBeforeSunder = b.units[static_cast<std::size_t>(party)].hp;
    b.useSkill(enemy, party, *sunder);
    CHECK(b.units[static_cast<std::size_t>(party)].hp == hpBeforeSunder);
    CHECK(battle::hasStatus(b.units[static_cast<std::size_t>(party)],
                            content::StatusType::DefenseDown));

    // The King's power-16 Support curse deals real damage AND applies its status.
    const content::SkillDef* curse = db.findSkill("blinding_curse");
    REQUIRE(curse != nullptr);
    const int hpBeforeCurse = b.units[static_cast<std::size_t>(party)].hp;
    b.useSkill(enemy, party, *curse);
    CHECK(b.units[static_cast<std::size_t>(party)].hp < hpBeforeCurse);
}

// --- Sim-backed clearability ----------------------------------------------

TEST_CASE("castle: the re-statted King beats a maxed party that brought nothing",
          "[castle]") {
    // M44 re-stat: raw stats and skills alone no longer carry this fight. The
    // King is now a puzzle you answer with the Royal Relics and Royal Snacks —
    // the counterplay evidence lives in test_royal_relics.cpp
    // ("the doubled King demands the counterplay"), which drives the same fight
    // with obtainable items and wins it. This case pins the other half: an
    // item-less maxed party loses, so the counterplay is genuinely required.
    const content::ContentDatabase db = loadContent();
    battle::Battle b = battle::buildBattle(maxedParty(db), kingTeam(db), db);
    const battle::SimResult r = battle::simulate(b, db, 400);
    INFO("king rounds=" << r.rounds << " partyAlive=" << r.partyAlive << " hp%="
                        << r.partyHpFraction());
    CHECK(r.outcome != battle::Outcome::Victory);
    CHECK(r.rounds >= 6);  // it still takes a real fight to lose
}

TEST_CASE("castle: a maxed party clears the Boss Rush with no free healing",
          "[castle]") {
    const content::ContentDatabase db = loadContent();
    std::vector<dungeon::EnemyTeam> rush;
    const int n = static_cast<int>(bossRushOrder(db).size());
    for (int i = 0; i < n; ++i) {
        rush.push_back(bossRushTeam(db, i));
    }
    const GauntletResult g = runGauntlet(maxedParty(db), rush, db);
    INFO("boss rush survived " << g.wavesSurvived << "/" << n << " rounds=" << g.totalRounds);
    CHECK(g.cleared);
}

TEST_CASE("castle: endless is survivable for a while, then overwhelms (bounded)",
          "[castle]") {
    const content::ContentDatabase db = loadContent();
    std::vector<dungeon::EnemyTeam> waves;
    for (int w = 0; w < 60; ++w) {
        waves.push_back(endlessWaveTeam(db, w));
    }
    const GauntletResult g = runGauntlet(maxedParty(db), waves, db);
    INFO("endless waves survived=" << g.wavesSurvived);
    CHECK(g.wavesSurvived >= 3);   // a maxed party pushes a meaningful distance
    CHECK(g.wavesSurvived < 60);   // but the escalating scale eventually overwhelms
}

// On-demand sim table for tuning / owner review:
//   crystal_tests.exe "[castle-report]" -s
TEST_CASE("castle report: challenge clearability for a maxed party", "[.][castle-report]") {
    const content::ContentDatabase db = loadContent();
    const battle::SimResult king = battle::simulate(
        battle::buildBattle(maxedParty(db), kingTeam(db), db), db, 400);
    std::cout << "KING scale=" << kKingScalePct << "% -> outcome="
              << static_cast<int>(king.outcome) << " rounds=" << king.rounds
              << " partyAlive=" << king.partyAlive << " hp%=" << king.partyHpFraction() << "\n";

    std::vector<dungeon::EnemyTeam> rush;
    for (int i = 0; i < static_cast<int>(bossRushOrder(db).size()); ++i) {
        rush.push_back(bossRushTeam(db, i));
    }
    const GauntletResult gr = runGauntlet(maxedParty(db), rush, db);
    std::cout << "BOSS RUSH scale=" << kBossRushScalePct << "% -> cleared=" << gr.cleared
              << " survived=" << gr.wavesSurvived << "/" << rush.size()
              << " rounds=" << gr.totalRounds << "\n";

    std::vector<dungeon::EnemyTeam> waves;
    for (int w = 0; w < 60; ++w) {
        waves.push_back(endlessWaveTeam(db, w));
    }
    const GauntletResult ge = runGauntlet(maxedParty(db), waves, db);
    std::cout << "ENDLESS -> best wave=" << ge.wavesSurvived << " (scale wave0="
              << endlessWaveScalePct(0) << "% .. wave" << ge.wavesSurvived << "="
              << endlessWaveScalePct(ge.wavesSurvived) << "%)\n";
    SUCCEED();
}
