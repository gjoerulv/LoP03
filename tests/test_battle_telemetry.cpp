// M109 - the lifetime ledger's battle seam: the BattleTelemetry observer is
// record-only (the same seeded battle resolves byte-identically with and
// without it), attributes hits/KOs/heals/statuses/guards/summons through the
// model's own events, and recordBattleEnd applies the outcome and
// defeat-ledger rules (a boss once per encounter won, a raised clone never).

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <filesystem>
#include <string>
#include <vector>

#include "battle/Battle.hpp"
#include "battle/Simulator.hpp"
#include "content/ContentDatabase.hpp"
#include "content/ContentLoader.hpp"
#include "dungeon/DungeonModel.hpp"
#include "game/BattleTelemetry.hpp"
#include "game/Lifetime.hpp"
#include "game/Party.hpp"

namespace {

using namespace cd;

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

Party makeParty(int level) {
    Party party;
    for (const char* id : {"knight", "ranger", "mage", "cleric"}) {
        const content::ClassDef* cls = db().findClass(id);
        REQUIRE(cls != nullptr);
        party.members.push_back(createCharacter(*cls, id, level));
    }
    return party;
}

battle::Battle gauntlet(std::uint64_t seed) {
    dungeon::EnemyTeam team;
    team.name = "Telemetry gauntlet";
    team.enemyIds = {"mire_imp", "hex_wisp", "ogre_marauder", "grave_chanter"};
    team.statScalePct = 120;
    battle::Battle b = battle::buildBattle(makeParty(8), team, db());
    b.rngSeed = seed;
    return b;
}

// A high-level party against one goblin: every deliberate action is legible.
battle::Battle duel() {
    dungeon::EnemyTeam team;
    team.name = "Duel";
    team.enemyIds = {"goblin_grunt"};
    battle::Battle b = battle::buildBattle(makeParty(20), team, db());
    b.rngSeed = 7;
    return b;
}

int enemyIndex(const battle::Battle& b) {
    for (std::size_t i = 0; i < b.units.size(); ++i) {
        if (b.units[i].side == battle::Side::Enemy) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

int partyIndexOf(const battle::Battle& b, int slot) {
    for (std::size_t i = 0; i < b.units.size(); ++i) {
        if (b.units[i].side == battle::Side::Party && b.units[i].partyIndex == slot) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

const content::SkillDef* firstSkill(bool (*pred)(const content::SkillDef&)) {
    std::vector<std::string> ids;
    for (const auto& [id, def] : db().skills()) {
        (void)def;
        ids.push_back(id);
    }
    std::sort(ids.begin(), ids.end());  // deterministic pick
    for (const std::string& id : ids) {
        const content::SkillDef* s = db().findSkill(id);
        if (s != nullptr && pred(*s)) {
            return s;
        }
    }
    return nullptr;
}

}  // namespace

TEST_CASE("telemetry: attaching the ledger observer changes nothing at all",
          "[lifetime][telemetry][battle]") {
    for (std::uint64_t seed : {11ull, 22ull, 33ull}) {
        battle::Battle bare = gauntlet(seed);
        battle::Battle watched = gauntlet(seed);
        LifetimeStats stats;
        BattleTelemetry telemetry(stats, watched, db());
        watched.observer = &telemetry;
        const battle::SimResult a = battle::simulateInPlace(bare, db());
        const battle::SimResult b = battle::simulateInPlace(watched, db());
        REQUIRE(a.outcome == b.outcome);
        REQUIRE(a.rounds == b.rounds);
        REQUIRE(bare.rollCursor == watched.rollCursor);
        REQUIRE(bare.units.size() == watched.units.size());
        for (std::size_t i = 0; i < bare.units.size(); ++i) {
            REQUIRE(bare.units[i].hp == watched.units[i].hp);
            REQUIRE(bare.units[i].mp == watched.units[i].mp);
            REQUIRE(bare.units[i].statuses.size() == watched.units[i].statuses.size());
        }
        // ...and the ledger saw the fight: something was dealt and taken.
        REQUIRE(stats.combat.damageDealt > 0);
        REQUIRE(stats.combat.damageTaken > 0);
    }
}

TEST_CASE("telemetry: a deliberate hit is dealt by its author and taken by its target",
          "[lifetime][telemetry]") {
    battle::Battle b = duel();
    LifetimeStats stats;
    BattleTelemetry telemetry(stats, b, db());
    b.observer = &telemetry;
    const int foe = enemyIndex(b);
    const int knight = partyIndexOf(b, 0);
    REQUIRE(foe >= 0);
    REQUIRE(knight >= 0);
    b.units[static_cast<std::size_t>(foe)].hp = 100000;  // survives the swing
    b.units[static_cast<std::size_t>(foe)].maxHp = 100000;
    const int foeBefore = b.units[static_cast<std::size_t>(foe)].hp;
    b.attack(knight, foe);
    const int dealt = foeBefore - b.units[static_cast<std::size_t>(foe)].hp;
    REQUIRE(dealt > 0);
    REQUIRE(stats.members[0].damageDealt == dealt);
    REQUIRE(stats.members[0].biggestHit == dealt);
    REQUIRE(stats.combat.damageDealt == dealt);
    REQUIRE(stats.combat.highestHit == dealt);
    REQUIRE(stats.combat.highestHitMember == 0);
    REQUIRE(stats.combat.damageTaken == 0);
    REQUIRE(stats.members[1].damageDealt == 0);

    // The foe swings back: taken by the target, dealt by nobody on our side.
    const int knightBefore = b.units[static_cast<std::size_t>(knight)].hp;
    b.units[static_cast<std::size_t>(foe)].stats.attack = 400;  // a hit that lands hard
    b.attack(foe, knight);
    const int taken = knightBefore - b.units[static_cast<std::size_t>(knight)].hp;
    REQUIRE(taken > 0);
    REQUIRE(stats.members[0].damageTaken == taken);
    REQUIRE(stats.combat.damageTaken == taken);
    REQUIRE(stats.combat.damageDealt == dealt);  // unchanged
}

TEST_CASE("telemetry: KOs attribute finishing blows and count the fallen",
          "[lifetime][telemetry]") {
    battle::Battle b = duel();
    LifetimeStats stats;
    BattleTelemetry telemetry(stats, b, db());
    b.observer = &telemetry;
    const int foe = enemyIndex(b);
    const int ranger = partyIndexOf(b, 1);
    b.units[static_cast<std::size_t>(foe)].hp = 1;
    b.attack(ranger, foe);
    REQUIRE_FALSE(b.units[static_cast<std::size_t>(foe)].alive());
    REQUIRE(stats.combat.enemiesKo == 1);
    REQUIRE(stats.members[1].finishingBlows == 1);
    REQUIRE(stats.members[0].finishingBlows == 0);

    // A member falls to the foe: counted against the member, never as a foe KO.
    battle::Battle c = duel();
    LifetimeStats stats2;
    BattleTelemetry telemetry2(stats2, c, db());
    c.observer = &telemetry2;
    const int foe2 = enemyIndex(c);
    const int mage = partyIndexOf(c, 2);
    c.units[static_cast<std::size_t>(mage)].hp = 1;
    c.units[static_cast<std::size_t>(foe2)].stats.attack = 400;
    c.attack(foe2, mage);
    REQUIRE_FALSE(c.units[static_cast<std::size_t>(mage)].alive());
    REQUIRE(stats2.members[2].timesKo == 1);
    REQUIRE(stats2.combat.enemiesKo == 0);
}

TEST_CASE("telemetry: guards, skills, statuses, heals and revives land at their seams",
          "[lifetime][telemetry]") {
    battle::Battle b = duel();
    LifetimeStats stats;
    BattleTelemetry telemetry(stats, b, db());
    b.observer = &telemetry;
    const int foe = enemyIndex(b);
    const int knight = partyIndexOf(b, 0);
    const int cleric = partyIndexOf(b, 3);
    b.units[static_cast<std::size_t>(foe)].hp = 100000;
    b.units[static_cast<std::size_t>(foe)].maxHp = 100000;

    b.guard(knight);
    REQUIRE(stats.combat.guardUses == 1);
    b.guard(foe);  // a foe's guard is not ours
    REQUIRE(stats.combat.guardUses == 1);

    // A single-foe status skill: cast + inflicted + applied.
    const content::SkillDef* hex = firstSkill([](const content::SkillDef& s) {
        return s.target == content::SkillTarget::SingleEnemy &&
               s.statusEffect != content::StatusType::None &&
               (s.category == content::SkillCategory::Physical ||
                s.category == content::SkillCategory::Magic);
    });
    REQUIRE(hex != nullptr);
    battle::Combatant& caster = b.units[static_cast<std::size_t>(knight)];
    caster.mp = 999;
    caster.maxMp = 999;
    caster.skillIds.push_back(hex->id);
    b.useSkill(knight, foe, *hex);
    REQUIRE(stats.members[0].skillsCast == 1);
    REQUIRE(stats.members[0].statusesInflicted == 1);
    REQUIRE(stats.combat.statusesApplied == 1);
    REQUIRE(hasStatus(b.units[static_cast<std::size_t>(foe)], hex->statusEffect));

    // A single-ally heal: done by the cleric, received by the knight.
    const content::SkillDef* mend = firstSkill([](const content::SkillDef& s) {
        return s.category == content::SkillCategory::Heal &&
               s.target == content::SkillTarget::SingleAlly && s.power > 0 &&
               s.reviveHpPct == 0;
    });
    REQUIRE(mend != nullptr);
    battle::Combatant& healer = b.units[static_cast<std::size_t>(cleric)];
    healer.mp = 999;
    healer.maxMp = 999;
    caster.hp = 1;
    b.useSkill(cleric, knight, *mend);
    const int healed = caster.hp - 1;
    REQUIRE(healed > 0);
    REQUIRE(stats.members[3].healingDone == healed);
    REQUIRE(stats.members[0].healingReceived == healed);
    REQUIRE(stats.combat.healing == healed);
    REQUIRE(stats.members[3].skillsCast == 1);

    // A revive-capable heal raises a fallen ally: a revive performed.
    const content::SkillDef* renew = firstSkill([](const content::SkillDef& s) {
        return s.category == content::SkillCategory::Heal && s.reviveHpPct > 0;
    });
    REQUIRE(renew != nullptr);
    caster.hp = 0;
    b.useSkill(cleric, knight, *renew);
    REQUIRE(caster.alive());
    REQUIRE(stats.members[3].revivesPerformed == 1);
    REQUIRE(stats.combat.revives == 1);
}

TEST_CASE("telemetry: a summon counts once when it is actually cast", "[lifetime][telemetry]") {
    const content::SkillDef* legend =
        firstSkill([](const content::SkillDef& s) { return s.oncePerRun; });
    REQUIRE(legend != nullptr);
    battle::Battle b = duel();
    LifetimeStats stats;
    BattleTelemetry telemetry(stats, b, db());
    b.observer = &telemetry;
    const int foe = enemyIndex(b);
    const int mage = partyIndexOf(b, 2);
    battle::Combatant& caster = b.units[static_cast<std::size_t>(mage)];
    caster.mp = 999;
    caster.maxMp = 999;
    caster.skillIds.push_back(legend->id);
    b.units[static_cast<std::size_t>(foe)].hp = 100000;
    b.units[static_cast<std::size_t>(foe)].maxHp = 100000;
    b.useSkill(mage, foe, *legend);
    REQUIRE(stats.combat.summonsUsed == 1);
    REQUIRE(stats.members[2].skillsCast == 1);
    b.useSkill(mage, foe, *legend);  // the M95 refusal: nothing is cast
    REQUIRE(stats.combat.summonsUsed == 1);
    REQUIRE(stats.members[2].skillsCast == 1);
}

TEST_CASE("telemetry: recordBattleEnd tallies outcomes, turns and the defeat ledger",
          "[lifetime][telemetry]") {
    // Two of the same minion: each felled unit counts its id once.
    dungeon::EnemyTeam team;
    team.enemyIds = {"goblin_grunt", "goblin_grunt", "wisp"};
    battle::Battle b = battle::buildBattle(makeParty(5), team, db());
    for (battle::Combatant& c : b.units) {
        if (c.side == battle::Side::Enemy && c.sourceId == "goblin_grunt") {
            c.hp = 0;
        }
    }
    LifetimeStats stats;
    recordBattleEnd(stats, b, battle::Outcome::Victory, 6, 2);
    REQUIRE(stats.combat.battlesWon == 1);
    REQUIRE(stats.combat.battleTurns == 6);
    REQUIRE(stats.combat.bossesDefeated == 0);
    REQUIRE(stats.defeats.at("goblin_grunt") == 2);
    REQUIRE(stats.defeats.count("wisp") == 0);  // it stood at the end
    REQUIRE(lifetimeTown(stats, 2).battlesWon == 1);
    REQUIRE(lifetimeTown(stats, 2).bossesDefeated == 0);

    recordBattleEnd(stats, b, battle::Outcome::Defeat, 3, 2);
    recordBattleEnd(stats, b, battle::Outcome::Escaped, 1, 2);
    recordBattleEnd(stats, b, battle::Outcome::Ongoing, 2, 2);
    REQUIRE(stats.combat.battlesLost == 1);
    REQUIRE(stats.combat.playerEscapes == 1);
    REQUIRE(stats.combat.battleTurns == 12);
    REQUIRE(stats.combat.battlesWon == 1);
}

TEST_CASE("telemetry: a boss encounter counts once and its raised clone never",
          "[lifetime][telemetry][dragon]") {
    dungeon::EnemyTeam team;
    team.isBoss = true;
    team.bossId = "the_dragon";
    team.name = "The Last Dragon";
    team.statScalePct = 100;
    battle::Battle b = battle::buildBattle(makeParty(30), team, db());
    int dragons = 0;
    for (battle::Combatant& c : b.units) {
        if (c.side == battle::Side::Enemy && c.sourceId == "the_dragon") {
            ++dragons;
            c.hp = 0;  // both the Dragon and its (raised, then felled) clone lie dead
        }
    }
    REQUIRE(dragons == 2);  // the M75 clone slot rides as a second unit
    LifetimeStats stats;
    recordBattleEnd(stats, b, battle::Outcome::Victory, 13, 0);
    REQUIRE(stats.defeats.at("the_dragon") == 1);
    REQUIRE(stats.combat.bossesDefeated == 1);
    REQUIRE(stats.combat.battlesWon == 1);
    // Town 0: no per-town attribution anywhere.
    for (const TownLifetime& t : stats.towns) {
        REQUIRE(t.battlesWon == 0);
        REQUIRE(t.bossesDefeated == 0);
    }
    // ...and a KO'd clone still credits the raw per-unit tallies.
    battle::Battle c = battle::buildBattle(makeParty(30), team, db());
    LifetimeStats raw;
    BattleTelemetry telemetry(raw, c, db());
    c.observer = &telemetry;
    int clone = -1;
    for (std::size_t i = 0; i < c.units.size(); ++i) {
        if (c.units[i].summonSlot) {
            clone = static_cast<int>(i);
        }
    }
    REQUIRE(clone >= 0);
    c.units[static_cast<std::size_t>(clone)].hp = 1;  // "raised", at the brink
    const int knight = partyIndexOf(c, 0);
    c.attack(knight, clone);
    REQUIRE_FALSE(c.units[static_cast<std::size_t>(clone)].alive());
    REQUIRE(raw.combat.enemiesKo == 1);
    REQUIRE(raw.members[0].finishingBlows == 1);
}
