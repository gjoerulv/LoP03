// M61 — Goose Town & the Deadly Duck: the quack rule, the blanket affliction
// immunity, the boss-side AoE attack, the gauntlet teams and their
// owner-specified effective stats, the unlock/records save round-trip, the
// Quackbane achievement, and the sim proof that the gauntlet is winnable.

#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <iostream>

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
    // M77 (owner decision 2026-08-06): the blanket flag became a bespoke list
    // with exactly one chink — a Curse lands, everything else still bounces.
    REQUIRE_FALSE(theDuck.afflictionImmune);
    REQUIRE(theDuck.statusImmunities.size() == 7);
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

TEST_CASE("goose town: afflictions bounce off the Duck, a Curse does not", "[goose]") {
    Party party = maxedParty();
    battle::Battle b = battle::buildBattle(party, duckTeam(db()), db());
    const int duckIndex = static_cast<int>(b.units.size()) - 1;
    battle::Combatant& duck = b.units[static_cast<std::size_t>(duckIndex)];

    // Statuses applied through the shared chokepoint: afflictions are refused,
    // stat debuffs stick ("immune to every bad status but can be debuffed") —
    // and since M77 (owner decision 2026-08-06) exactly ONE affliction slips
    // through the feathers: a Curse. The Evil Duckling finally has a worthy
    // target.
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
    const content::ItemDef* duckling = db().findItem("evil_duckling");
    REQUIRE(duckling != nullptr);
    b.useItem(0, duckIndex, *duckling);  // an item never bounces off his mirror
    CHECK(battle::isCursed(duck));
    // The immunity queries agree (display sites skip what cannot land).
    CHECK(battle::isImmuneTo(duck, content::StatusType::Stunned));
    CHECK(battle::isImmuneTo(duck, content::StatusType::Terrified));
    CHECK(battle::isImmuneTo(duck, content::StatusType::Confusion));
    CHECK(battle::isImmuneTo(duck, content::StatusType::Sleep));
    CHECK_FALSE(battle::isImmuneTo(duck, content::StatusType::Curse));
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
// REAL shared pieces that lets a party member spend a turn on an item or a
// counterplay skill. M77 CLOSED the Deadly Spoon (the Duck shrugs every
// battle-long stat scale), so the obtainable counterplay is the M76 kit:
// Mirrorbreak against the mirrors (the Duck opens behind one now), Absolve
// and Holy Taxes against the Hexwing's party-wide curse — plus honest
// healing items. The acceptance bar keeps its M61/M44 shape: the gauntlet
// must fall to a maxed party WITH that obtainable counterplay; a bare
// scripted party losing is the intended difficulty.
struct GooseKit {
    int elixirs = 6;
    int tears = 2;
    int taxes = 2;      // Holy Taxes (M76): the item-side curse remover
    int ducklings = 1;  // the Evil Duckling (M76): one per customer, duck rules
};

bool knowsSkill(const battle::Combatant& c, const char* id) {
    for (const std::string& s : c.skillIds) {
        if (s == id) {
            return true;
        }
    }
    return false;
}

// One kit serves the whole gauntlet (it is the party's bag, not per-fight).
battle::Outcome runGooseFight(battle::Battle& b, GooseKit& kit, int* roundsOut = nullptr) {
    const content::ItemDef* elixir = db().findItem("elixir");
    const content::ItemDef* tear = db().findItem("phoenix_tear");
    const content::ItemDef* holyTaxes = db().findItem("holy_taxes");
    const content::ItemDef* duckling = db().findItem("evil_duckling");
    const content::SkillDef* mirrorbreak = db().findSkill("mirrorbreak");
    const content::SkillDef* absolve = db().findSkill("absolve");
    REQUIRE(elixir != nullptr);
    REQUIRE(tear != nullptr);
    REQUIRE(holyTaxes != nullptr);
    REQUIRE(mirrorbreak != nullptr);
    REQUIRE(absolve != nullptr);
    const auto hpPct = [&b](int i) {
        const battle::Combatant& u = b.units[static_cast<std::size_t>(i)];
        return u.hp * 100 / u.maxHp;
    };
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
                // Revive first, heal second, then the counterplay: shatter a
                // mirror, lift a curse — and only then fight on.
                int fallen = -1;
                int weakest = -1;
                int cursed = -1;
                int mirrored = -1;
                int boss = -1;
                for (std::size_t i = 0; i < b.units.size(); ++i) {
                    const battle::Combatant& u = b.units[i];
                    if (u.side == battle::Side::Party) {
                        if (!u.alive() && fallen < 0) {
                            fallen = static_cast<int>(i);
                        }
                        if (u.alive() &&
                            (weakest < 0 || hpPct(static_cast<int>(i)) < hpPct(weakest))) {
                            weakest = static_cast<int>(i);
                        }
                        if (u.alive() && battle::isCursed(u) && cursed < 0) {
                            cursed = static_cast<int>(i);
                        }
                    } else if (u.alive()) {
                        if (battle::hasReflect(u) && mirrored < 0) {
                            mirrored = static_cast<int>(i);
                        }
                        if (u.isBoss) {
                            boss = static_cast<int>(i);
                        }
                    }
                }
                // Ordinary healing is the cleric's job (choosePartyAction
                // already group-mends the hurt); the bag answers emergencies
                // only, so the other three keep swinging.
                if (kit.tears > 0 && fallen >= 0) {
                    --kit.tears;
                    b.useItem(actor, fallen, *tear);
                    spent = true;
                } else if (mirrored >= 0 && knowsSkill(self, "mirrorbreak") &&
                           battle::mpCostFor(self, *mirrorbreak) <= self.mp &&
                           battle::canCast(self, *mirrorbreak)) {
                    b.useSkill(actor, mirrored, *mirrorbreak);
                    spent = true;
                } else if (boss >= 0 && kit.ducklings > 0 && duckling != nullptr &&
                           !battle::isCursed(b.units[static_cast<std::size_t>(boss)])) {
                    // The owner's 2026-08-06 chink: the Duck is cursable, so
                    // the one held duckling goes straight at him.
                    --kit.ducklings;
                    b.useItem(actor, boss, *duckling);
                    spent = true;
                } else if (kit.elixirs > 0 && weakest >= 0 && hpPct(weakest) < 35) {
                    --kit.elixirs;
                    b.useItem(actor, weakest, *elixir);
                    spent = true;
                } else if (cursed >= 0 && knowsSkill(self, "absolve") &&
                           battle::mpCostFor(self, *absolve) <= self.mp &&
                           battle::canCast(self, *absolve)) {
                    b.useSkill(actor, cursed, *absolve);
                    spent = true;
                } else if (cursed >= 0 && kit.taxes > 0) {
                    --kit.taxes;
                    b.useItem(actor, cursed, *holyTaxes);
                    spent = true;
                } else if (boss >= 0) {
                    // The M75 rebalance made ATK-/DEF- real levers, and the
                    // Duck's affliction immunity deliberately leaves debuffs
                    // open ("can be debuffed by design"). Keeping them up is
                    // obtainable counterplay the fixed sim AI never uses.
                    const battle::Combatant& bossU = b.units[static_cast<std::size_t>(boss)];
                    const content::SkillDef* bash = db().findSkill("shield_bash");
                    const content::SkillDef* ham = db().findSkill("hamstring");
                    if (bash != nullptr && knowsSkill(self, "shield_bash") &&
                        !battle::hasStatus(bossU, content::StatusType::DefenseDown) &&
                        battle::mpCostFor(self, *bash) <= self.mp &&
                        battle::canCast(self, *bash)) {
                        b.useSkill(actor, boss, *bash);
                        spent = true;
                    } else if (ham != nullptr && knowsSkill(self, "hamstring") &&
                               !battle::hasStatus(bossU, content::StatusType::AttackDown) &&
                               battle::mpCostFor(self, *ham) <= self.mp &&
                               battle::canCast(self, *ham)) {
                        b.useSkill(actor, boss, *ham);
                        spent = true;
                    }
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
    if (roundsOut != nullptr) {
        *roundsOut = rounds;
    }
    return b.outcome();
}

}  // namespace

TEST_CASE("goose town: the gauntlet falls to a maxed party with counterplay", "[goose]") {
    // The full no-heal gauntlet exactly as CastleChallengeState runs it: the
    // five geese, condition carried into the Duck. M77 closed the Spoon, so
    // both fights are scripted with the M76 counterplay and ONE shared item
    // kit. One winning seed proves beatability; a bare sim party losing is
    // the intended difficulty (recorded in the M61/M77 notes), the King's bar.
    int wins = 0;
    int waveWins = 0;
    for (std::uint64_t seed : {0xD0C40001ull, 0xD0C40002ull, 0xD0C40003ull, 0xD0C40004ull,
                               0xD0C40005ull}) {
        Party party = maxedParty();
        GooseKit kit;
        battle::Battle wave = battle::buildBattle(party, gooseWaveTeam(db()), db());
        wave.rngSeed = seed;
        if (runGooseFight(wave, kit) != battle::Outcome::Victory) {
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
        int rounds = 0;
        const battle::Outcome o = runGooseFight(duck, kit, &rounds);
        const battle::Combatant& d = duck.units.back();
        std::cout << "duck battery seed " << std::hex << seed << std::dec << ": "
                  << (o == battle::Outcome::Victory ? "WON" : "lost") << " in " << rounds
                  << " rounds, duck hp " << d.hp * 100 / d.maxHp << "%, kit left e"
                  << kit.elixirs << "/t" << kit.tears << "\n";
        if (o == battle::Outcome::Victory) {
            ++wins;
        }
    }
    INFO("goose-wave wins: " << waveWins << "/5, full gauntlet with counterplay: " << wins
                             << "/5");
    CHECK(waveWins >= 1);
    CHECK(wins >= 1);
}
