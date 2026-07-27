// M61 — Goose Town & the Deadly Duck: the quack rule, the blanket affliction
// immunity, the boss-side AoE attack, the gauntlet teams and their
// owner-specified effective stats, the unlock/records save round-trip, the
// Quackbane achievement, and the sim proof that the gauntlet is winnable.

#include <catch2/catch_test_macros.hpp>

#include <filesystem>

#include "battle/Battle.hpp"
#include "battle/Simulator.hpp"
#include "content/ContentDatabase.hpp"
#include "content/ContentLoader.hpp"
#include "dungeon/DungeonModel.hpp"
#include "editor/EditorValidation.hpp"
#include "game/Achievements.hpp"
#include "game/Castle.hpp"
#include "game/Party.hpp"
#include "save/SaveSystem.hpp"
#include "states/BestiaryStats.hpp"

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

Party maxedParty() {
    Party party;
    for (const content::ClassDef* cls : editor::defaultSimClasses(db())) {
        Character c = createCharacter(*cls, cls->name, kMaxLevel);
        c.weapon = editor::pickSimGear(db(), *cls, content::EquipSlot::Weapon,
                                       editor::GearTier::Best);
        c.armor = editor::pickSimGear(db(), *cls, content::EquipSlot::Armor,
                                      editor::GearTier::Best);
        c.accessory = editor::pickSimGear(db(), *cls, content::EquipSlot::Accessory,
                                          editor::GearTier::Best);
        refreshCharacter(c, db());
        party.members.push_back(std::move(c));
    }
    healFull(party);
    return party;
}

}  // namespace

TEST_CASE("goose town: rules version reached 12", "[goose]") {
    CHECK(battle::kBattleRulesVersion >= 12);
}

TEST_CASE("goose town: the gauntlet teams land the owner-specified numbers", "[goose]") {
    const dungeon::EnemyTeam wave = gooseWaveTeam(db());
    REQUIRE(wave.enemyIds.size() == 5);
    REQUIRE(wave.statScalePct == kGooseTownScalePct);
    REQUIRE_FALSE(wave.isBoss);

    const dungeon::EnemyTeam duck = duckTeam(db());
    REQUIRE(duck.isBoss);
    REQUIRE(duck.bossId == kDuckBossId);
    REQUIRE(duck.enemyIds.empty());  // his court fell in fight 1

    // Effective HP: the Duck at 5000, each goose at ~500 (owner spec).
    Party party = maxedParty();
    battle::Battle duckFight = battle::buildBattle(party, duck, db());
    REQUIRE(duckFight.units.size() == party.members.size() + 1);
    const battle::Combatant& theDuck = duckFight.units.back();
    REQUIRE(theDuck.maxHp == 5000);
    REQUIRE(theDuck.attackHitsAll);
    REQUIRE_FALSE(theDuck.attackStatuses.empty());
    REQUIRE(theDuck.afflictionImmune);
    REQUIRE(theDuck.counterAttack);
    REQUIRE(theDuck.thornsPct > 0);
    REQUIRE(theDuck.spellWardPct > 0);

    battle::Battle wave1 = battle::buildBattle(party, wave, db());
    int geese = 0;
    for (const battle::Combatant& u : wave1.units) {
        if (u.side == battle::Side::Enemy) {
            ++geese;
            REQUIRE(u.maxHp >= 450);
            REQUIRE(u.maxHp <= 550);
            REQUIRE(u.doNothingPct == 10);
            REQUIRE(u.doNothingText == "Quack.");
        }
    }
    REQUIRE(geese == 5);
}

TEST_CASE("goose town: each Evil Goose has one weakness and two passives", "[goose]") {
    const content::BossDef* duck = db().findBoss(kDuckBossId);
    REQUIRE(duck != nullptr);
    for (const std::string& id : duck->minions) {
        const content::EnemyDef* goose = db().findEnemy(id);
        INFO(id);
        REQUIRE(goose != nullptr);
        REQUIRE(goose->bossOnly);  // never in dungeons or endless waves
        REQUIRE(goose->affinity.weaknesses.size() == 1);
        REQUIRE(goose->passives.size() == 2);
        REQUIRE(goose->doNothingPct == 10);
    }
}

TEST_CASE("goose town: the Duck's effective stats top every authored context", "[goose]") {
    const content::BossDef* duck = db().findBoss(kDuckBossId);
    REQUIRE(duck != nullptr);
    const content::StatBlock duckEff = content::scaledStats(duck->stats, kGooseTownScalePct);
    REQUIRE(duckEff.maxHp == 5000);
    const int floor = castleFloorScalePct(db());
    for (const auto& [id, def] : db().bosses()) {
        if (id == kDuckBossId) {
            continue;
        }
        const int pct = foeMaxScalePct(true, false, id == kKingBossId, floor);
        const content::StatBlock eff = content::scaledStats(def.stats, pct);
        INFO(id);
        CHECK(duckEff.maxHp > eff.maxHp);
        CHECK(duckEff.attack > eff.attack);
        CHECK(duckEff.magic > eff.magic);
        CHECK(duckEff.defense > eff.defense);
        CHECK(duckEff.speed > eff.speed);
    }
    for (const auto& [id, def] : db().enemies()) {
        const int pct = foeMaxScalePct(false, def.bossOnly, false, floor);
        const content::StatBlock eff = content::scaledStats(def.stats, pct);
        INFO(id);
        CHECK(duckEff.maxHp > eff.maxHp);
        CHECK(duckEff.attack > eff.attack);
        CHECK(duckEff.magic > eff.magic);
        CHECK(duckEff.defense > eff.defense);
        CHECK(duckEff.speed > eff.speed);
    }
}

TEST_CASE("goose town: the Duck stays out of the Boss Rush", "[goose]") {
    for (const std::string& id : bossRushOrder(db())) {
        CHECK(id != kDuckBossId);
        CHECK(id != kKingBossId);
    }
}

TEST_CASE("goose town: the quack roll is deterministic and only for authored foes", "[goose]") {
    Party party = maxedParty();
    battle::Battle wave = battle::buildBattle(party, gooseWaveTeam(db()), db());
    wave.rngSeed = 0x0060053ull;
    // Party units never quack; geese quack deterministically per (seed, turn).
    int quacks = 0;
    for (int turn = 1; turn <= 40; ++turn) {
        wave.turnsTaken = turn;
        for (int i = 0; i < static_cast<int>(wave.units.size()); ++i) {
            const bool q = battle::doesNothingThisTurn(wave, i);
            if (wave.units[static_cast<std::size_t>(i)].side == battle::Side::Party) {
                REQUIRE_FALSE(q);
            } else if (q) {
                ++quacks;
            }
            REQUIRE(battle::doesNothingThisTurn(wave, i) == q);  // pure: same answer
        }
    }
    // 5 geese x 40 turns at 10%: expect a handful, not zero and not half.
    CHECK(quacks > 0);
    CHECK(quacks < 60);
    REQUIRE(wave.rollCursor == 0);  // the roll never touches the seeded stream
}

TEST_CASE("goose town: afflictions bounce off the Duck, debuffs land", "[goose]") {
    Party party = maxedParty();
    battle::Battle b = battle::buildBattle(party, duckTeam(db()), db());
    const int duckIndex = static_cast<int>(b.units.size()) - 1;
    battle::Combatant& duck = b.units[static_cast<std::size_t>(duckIndex)];
    REQUIRE(duck.afflictionImmune);

    // Statuses applied through the shared chokepoint: afflictions are refused,
    // stat debuffs stick ("immune to every bad status but can be debuffed").
    const content::SkillDef* venom = db().findSkill("venom_mist");
    const content::SkillDef* weaken = db().findSkill("weaken");
    REQUIRE(venom != nullptr);
    REQUIRE(venom->statusEffect == content::StatusType::Poison);
    b.useSkill(0, duckIndex, *venom);
    CHECK_FALSE(battle::hasStatus(duck, content::StatusType::Poison));
    if (weaken != nullptr && weaken->statusEffect != content::StatusType::None) {
        b.useSkill(1, duckIndex, *weaken);
        CHECK(battle::hasStatus(duck, weaken->statusEffect));
    }
    // The immunity queries agree (display sites skip what cannot land).
    CHECK(battle::isImmuneTo(duck, content::StatusType::Stunned));
    CHECK(battle::isImmuneTo(duck, content::StatusType::Terrified));
    CHECK(battle::isImmuneTo(duck, content::StatusType::Confusion));
    CHECK_FALSE(battle::isImmuneTo(duck, content::StatusType::AttackDown));
}

TEST_CASE("goose town: unlock flag and duck record round-trip a save", "[goose][save]") {
    Party party = maxedParty();
    party.gooseTownUnlocked = true;
    party.castleRecords.duckBestTurns = 23;
    const std::filesystem::path dir =
        std::filesystem::temp_directory_path() / "cd_goose_save_test";
    std::filesystem::create_directories(dir);
    save::SaveSystem saves(db(), dir);
    content::LoadReport report;
    REQUIRE(saves.save(save::SaveSlot::Manual1, party, report));
    Party loaded;
    REQUIRE(saves.load(save::SaveSlot::Manual1, loaded, report));
    CHECK(loaded.gooseTownUnlocked);
    CHECK(loaded.castleRecords.duckBestTurns == 23);
    CHECK(loaded.castleRecords.duckDefeated());
    std::filesystem::remove_all(dir);
}

TEST_CASE("goose town: Quackbane unlocks on a felled Duck", "[goose]") {
    Party party;
    CHECK_FALSE(achievementMet("quackbane", party, AchvContext{}));
    party.castleRecords.duckBestTurns = 31;
    CHECK(achievementMet("quackbane", party, AchvContext{}));
    REQUIRE(findAchievement("quackbane") != nullptr);
}

namespace {

// The test_royal_relics scripted loop, in miniature: a round-loop over the
// REAL shared pieces that lets a party member spend a turn on an item. The
// Duck's acceptance bar mirrors the King's (M44/M54): he falls to a maxed
// party WITH the obtainable counterplay — here the Deadly Spoon (stat
// halving is not a status, so it lands through his affliction immunity;
// Tax Sheets and the Evil Goose bounce off him by design) plus honest
// healing items. A bare scripted party losing is the intended difficulty,
// exactly as it is for the King.
struct DuckKit {
    int spoons = 1;
    int elixirs = 6;
    int tears = 2;
};

battle::Outcome runDuckFight(battle::Battle& b, DuckKit kit) {
    const content::ItemDef* spoon = db().findItem("deadly_spoon");
    const content::ItemDef* elixir = db().findItem("elixir");
    const content::ItemDef* tear = db().findItem("phoenix_tear");
    REQUIRE(spoon != nullptr);
    REQUIRE(elixir != nullptr);
    REQUIRE(tear != nullptr);
    const int duckIndex = static_cast<int>(b.units.size()) - 1;
    int rounds = 0;
    while (b.outcome() == battle::Outcome::Ongoing && rounds < 300) {
        ++rounds;
        b.turnsTaken = rounds;
        b.beginRound();
        for (int actor : battle::turnOrder(b)) {
            battle::Combatant& self = b.units[static_cast<std::size_t>(actor)];
            if (!self.alive()) {
                continue;
            }
            b.tickStatuses(actor);
            if (!self.alive()) {
                continue;
            }
            b.clearGuard(actor);
            const bool party = self.side == battle::Side::Party;
            const bool forced =
                battle::forcedActionFor(self) != battle::ForcedAction::None;
            bool spent = false;
            if (party && !forced) {
                // Revive first, heal second, open with the Spoon.
                int fallen = -1;
                int weakest = -1;
                for (std::size_t i = 0; i < b.units.size(); ++i) {
                    const battle::Combatant& u = b.units[i];
                    if (u.side != battle::Side::Party) {
                        continue;
                    }
                    if (!u.alive() && fallen < 0) {
                        fallen = static_cast<int>(i);
                    }
                    if (u.alive() && (weakest < 0 ||
                                      u.hp * 100 / u.maxHp <
                                          b.units[static_cast<std::size_t>(weakest)].hp * 100 /
                                              b.units[static_cast<std::size_t>(weakest)].maxHp)) {
                        weakest = static_cast<int>(i);
                    }
                }
                if (kit.tears > 0 && fallen >= 0) {
                    --kit.tears;
                    b.useItem(actor, fallen, *tear);
                    spent = true;
                } else if (kit.elixirs > 0 && weakest >= 0 &&
                           b.units[static_cast<std::size_t>(weakest)].hp * 100 <
                               b.units[static_cast<std::size_t>(weakest)].maxHp * 55) {
                    --kit.elixirs;
                    b.useItem(actor, weakest, *elixir);
                    spent = true;
                } else if (kit.spoons > 0 &&
                           b.units[static_cast<std::size_t>(duckIndex)].alive() &&
                           !b.units[static_cast<std::size_t>(duckIndex)].statDiminished) {
                    --kit.spoons;
                    b.useItem(actor, duckIndex, *spoon);
                    spent = true;
                }
            }
            if (!spent) {
                if (party) {
                    battle::applyChoice(b, actor, battle::choosePartyAction(b, actor, db()),
                                        db());
                } else {
                    battle::applyChoice(b, actor, battle::chooseEnemyAction(b, actor, db()),
                                        db());
                }
            }
            if (b.outcome() != battle::Outcome::Ongoing) {
                break;
            }
        }
    }
    return b.outcome();
}

}  // namespace

TEST_CASE("goose town: the gauntlet falls to a maxed party with counterplay", "[goose]") {
    // The full no-heal gauntlet exactly as CastleChallengeState runs it: the
    // five geese (no items needed), condition carried into the Duck, who is
    // fought WITH the obtainable kit (one Deadly Spoon + elixirs + tears).
    // One winning seed proves beatability; a bare sim party losing is the
    // intended difficulty (recorded in the M61 note), the King's own bar.
    int wins = 0;
    int waveWins = 0;
    for (std::uint64_t seed : {0xD0C40001ull, 0xD0C40002ull, 0xD0C40003ull, 0xD0C40004ull,
                               0xD0C40005ull}) {
        Party party = maxedParty();
        battle::Battle wave = battle::buildBattle(party, gooseWaveTeam(db()), db());
        wave.rngSeed = seed;
        const battle::SimResult first = battle::simulateInPlace(wave, db());
        if (first.outcome != battle::Outcome::Victory) {
            continue;
        }
        ++waveWins;
        // Carry HP/MP into the Duck fight (the challenge state's no-heal rule).
        for (const battle::Combatant& u : wave.units) {
            if (u.side == battle::Side::Party && u.partyIndex >= 0 &&
                u.partyIndex < static_cast<int>(party.members.size())) {
                party.members[static_cast<std::size_t>(u.partyIndex)].hp = u.hp;
                party.members[static_cast<std::size_t>(u.partyIndex)].mp = u.mp;
            }
        }
        battle::Battle duck = battle::buildBattle(party, duckTeam(db()), db());
        duck.rngSeed = seed ^ 0xDEAD11FFull;
        if (runDuckFight(duck, DuckKit{}) == battle::Outcome::Victory) {
            ++wins;
        }
    }
    INFO("goose-wave wins: " << waveWins << "/5, full gauntlet with counterplay: " << wins
                             << "/5");
    CHECK(waveWins >= 1);
    CHECK(wins >= 1);
}
