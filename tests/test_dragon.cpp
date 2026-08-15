#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <filesystem>
#include <iostream>
#include <set>
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
#include "game/Curios.hpp"
#include "game/Party.hpp"
#include "game/Story.hpp"
#include "save/SaveSystem.hpp"

// M85 — the Last Dragon & curio lore: the authored immunity matrix against
// the owner brief, the six all-party breaths, the three deterministic
// triggers, the gauntlet's seeded vigil waves and records, the exclusions
// that keep the Dragon out of every other roster, the curio-lore coverage
// the loader deliberately leaves to this battery, the Pale Jester's beat,
// Wyrmbane, and sim-backed clearability at the castle counterplay bar.

using namespace cd;
using content::Element;
using content::StatusType;

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

}  // namespace

// --- The Dragon's authored kit ----------------------------------------------

TEST_CASE("dragon: the immunity matrix is the owner's, exactly", "[dragon]") {
    const content::ContentDatabase db = loadContent();
    const content::BossDef* d = db.findBoss(kDragonBossId);
    REQUIRE(d != nullptr);
    // Immune to every status EXCEPT ATK-, DEF-, Curse and Poison: the listed
    // six are the afflictions the party can actually throw at a foe.
    const std::set<StatusType> immune(d->statusImmunities.begin(), d->statusImmunities.end());
    CHECK(immune == std::set<StatusType>{StatusType::Confusion, StatusType::Silence,
                                         StatusType::Blind, StatusType::Terrified,
                                         StatusType::Stunned, StatusType::Sleep});
    CHECK(immune.count(StatusType::Poison) == 0);       // the door stays open
    CHECK(immune.count(StatusType::AttackDown) == 0);
    CHECK(immune.count(StatusType::DefenseDown) == 0);
    CHECK(immune.count(StatusType::Curse) == 0);
    // Fire immunity (the one authored in the game — the elements lint pins
    // that exclusivity; the intrinsic-bite engine rule has its own test).
    CHECK(d->affinity.immuneTo(Element::Fire));
    CHECK(d->affinity.weaknesses.empty());  // no cheap door on a superboss
    // The Spoon question (§G): REFUSES it, the Duck precedent for superbosses.
    CHECK(d->immuneToStatScale);
    // Passives: the owner trio.
    for (const char* p : {"first_strike", "lifedrink", "spell_ward"}) {
        CHECK(std::find(d->passives.begin(), d->passives.end(), std::string(p)) !=
              d->passives.end());
    }
    // Never uses Reflect: nothing in the kit or the start-of-battle statuses.
    CHECK(d->initialStatuses.empty());
    for (const std::string& sid : d->skills) {
        const content::SkillDef* s = db.findSkill(sid);
        REQUIRE(s != nullptr);
        CHECK(s->statusEffect != StatusType::Reflect);
    }
    CHECK(d->minions.empty());  // he fights alone; the vigil is the court
}

TEST_CASE("dragon: six breaths, one per element, each sweeping the party", "[dragon]") {
    const content::ContentDatabase db = loadContent();
    const content::BossDef* d = db.findBoss(kDragonBossId);
    REQUIRE(d != nullptr);
    REQUIRE(d->skills.size() == 6);
    std::set<Element> covered;
    for (const std::string& sid : d->skills) {
        const content::SkillDef* s = db.findSkill(sid);
        REQUIRE(s != nullptr);
        CHECK(s->category == content::SkillCategory::Magic);
        CHECK(s->target == content::SkillTarget::AllEnemies);
        CHECK(s->power > 0);
        CHECK(s->element != Element::None);
        covered.insert(s->element);
    }
    CHECK(covered.size() == 6);  // fire/ice/lightning/earth/holy/dark, once each
}

TEST_CASE("dragon: the three triggers are authored as briefed", "[dragon]") {
    const content::ContentDatabase db = loadContent();
    const content::BossDef* d = db.findBoss(kDragonBossId);
    REQUIRE(d != nullptr);
    REQUIRE(d->triggers.size() == 3);
    // One-time full MP deplete once below half.
    CHECK(d->triggers[0].when == content::TriggerWhen::FirstTimeHpBelowPct);
    CHECK(d->triggers[0].threshold == 50);
    CHECK(d->triggers[0].action == content::TriggerDo::DrainFoeMp);
    CHECK(d->triggers[0].mpDrainPct == 100);
    // ATK & SPD double below 10%.
    CHECK(d->triggers[1].threshold == 10);
    CHECK(d->triggers[1].action == content::TriggerDo::ScaleStatsSelf);
    CHECK(d->triggers[1].scaleAttackPct == 200);
    CHECK(d->triggers[1].scaleSpeedPct == 200);
    // The clone gag: at the brink (5%), a copy at 5% of full HP.
    CHECK(d->triggers[2].threshold == 5);
    CHECK(d->triggers[2].action == content::TriggerDo::SummonCloneSelf);
    CHECK(d->triggers[2].cloneHpPct == 5);
    // The engine prepares the clone slot at build time (the M75 machinery).
    const content::ContentDatabase& cdb = db;
    battle::Battle b = battle::buildBattle(maxedParty(cdb), dragonTeam(cdb), cdb);
    bool hasSlot = false;
    for (const battle::Combatant& u : b.units) {
        hasSlot = hasSlot || u.summonSlot;
    }
    CHECK(hasSlot);
}

// --- The gauntlet ------------------------------------------------------------

TEST_CASE("dragon: three seeded vigil waves, then the Dragon alone", "[dragon]") {
    const content::ContentDatabase db = loadContent();
    for (int w = 0; w < kDragonWaveCount; ++w) {
        const dungeon::EnemyTeam a = dragonEliteWaveTeam(db, w);
        const dungeon::EnemyTeam b = dragonEliteWaveTeam(db, w);
        CHECK(a.enemyIds == b.enemyIds);  // the same gauntlet every attempt
        CHECK(static_cast<int>(a.enemyIds.size()) == kDragonWaveSize);
        CHECK(a.statScalePct == kDragonScalePct);
        CHECK(a.bossId.empty());
        for (const std::string& id : a.enemyIds) {
            const content::EnemyDef* e = db.findEnemy(id);
            REQUIRE(e != nullptr);
            CHECK(e->tier == content::EnemyTier::Elite);
            CHECK_FALSE(e->bossOnly);
        }
    }
    CHECK(dragonEliteWaveTeam(db, kDragonWaveCount).enemyIds.empty());  // past the end
    const dungeon::EnemyTeam boss = dragonTeam(db);
    CHECK(boss.isBoss);
    CHECK(boss.bossId == std::string(kDragonBossId));
    CHECK(boss.statScalePct == kDragonScalePct);
    CHECK(boss.enemyIds.empty());
}

TEST_CASE("dragon: he appears in no roster that is not his", "[dragon]") {
    const content::ContentDatabase db = loadContent();
    // Not in the Boss Rush.
    const std::vector<std::string> order = bossRushOrder(db);
    CHECK(order.size() == 12);  // unchanged by M85
    CHECK(std::find(order.begin(), order.end(), std::string(kDragonBossId)) == order.end());
    // Not in any dungeon theme.
    for (const auto& [themeId, theme] : db.themes()) {
        (void)themeId;
        for (const std::string& id : theme.bosses) {
            CHECK(id != std::string(kDragonBossId));
        }
    }
    // Not generated into a dungeon — including the theme-less fallback sweep.
    for (const std::uint64_t seed : {5ull, 991ull}) {
        for (const std::string& themeId : {std::string(""), std::string("crystal_mine")}) {
            const dungeon::Dungeon d = dungeon::generate(seed, 20, db, themeId, 7);
            for (const dungeon::EnemyTeam& t : d.teams) {
                CHECK(t.bossId != std::string(kDragonBossId));
            }
        }
    }
    // Not in the Endless every-10th draw (it rides bossRushOrder + Masters).
    for (int i = 0; i < 30; ++i) {
        CHECK(endlessWaveTeam(db, i * 10 + 9).bossId != std::string(kDragonBossId));
    }
}

TEST_CASE("dragon: records improve like the Duck's; Wyrmbane fires", "[dragon]") {
    CastleRecords r;
    CHECK_FALSE(r.dragonDefeated());
    CHECK(dragonImproved(r, 40));
    CHECK_FALSE(dragonImproved(r, 0));
    r.dragonBestTurns = 40;
    CHECK(r.dragonDefeated());
    CHECK(dragonImproved(r, 30));
    CHECK_FALSE(dragonImproved(r, 45));

    Party p;
    CHECK_FALSE(achievementMet("wyrmbane", p, AchvContext{}));
    p.castleRecords.dragonBestTurns = 33;
    CHECK(achievementMet("wyrmbane", p, AchvContext{}));
    CHECK(findAchievement("wyrmbane") != nullptr);
}

TEST_CASE("dragon: the record round-trips the save", "[dragon][save]") {
    const content::ContentDatabase db = loadContent();
    const std::filesystem::path dir =
        std::filesystem::temp_directory_path() / "crystal_dragon_save_test";
    std::filesystem::remove_all(dir);
    save::SaveSystem saves(db, dir);
    Party p;
    p.members.push_back(createCharacter(*db.findClass("knight"), "Rolan", 5));
    p.castleRecords.dragonBestTurns = 41;
    content::LoadReport rep;
    REQUIRE(saves.save(save::SaveSlot::Manual1, p, rep));
    Party loaded;
    REQUIRE(saves.load(save::SaveSlot::Manual1, loaded, rep));
    CHECK(loaded.castleRecords.dragonBestTurns == 41);
    std::filesystem::remove_all(dir);
}

// --- Curio lore & the Pale Jester -------------------------------------------

TEST_CASE("dragon: every curio carries lore, and every lore names a curio", "[dragon]") {
    // The loader owns shape and duplicates; known-ness and coverage live here
    // because the curio table is a layer above the content loader.
    const content::ContentDatabase db = loadContent();
    CHECK(static_cast<int>(db.curioLoreCount()) == kCurioCount);
    for (const auto& [id, lore] : db.curioLores()) {
        INFO(id);
        CHECK(findCurio(id) != nullptr);
        CHECK_FALSE(lore.body.empty());
    }
    for (const CurioDef& c : kCurios) {
        INFO(c.id);
        CHECK(db.findCurioLore(c.id) != nullptr);
    }
    CHECK(db.findCurioLore("not_a_curio") == nullptr);  // the defensive path
}

TEST_CASE("dragon: the Pale Jester's beat is authored", "[dragon]") {
    const content::ContentDatabase db = loadContent();
    const content::StoryBeat* beat = db.findStoryBeat(kDragonJesterBeat);
    REQUIRE(beat != nullptr);
    CHECK(beat->speaker == "The Pale Jester");
    CHECK_FALSE(beat->body.empty());
    // Never part of the storyteller's unlock mask.
    CHECK(storyBit(kDragonJesterBeat) == 0);
}

// --- Clearability evidence (the castle counterplay bar) ----------------------

TEST_CASE("dragon: the gauntlet is clearable by the maxed party", "[dragon]") {
    const content::ContentDatabase db = loadContent();
    Party party = maxedParty(db);
    // M89: with the sustained breath pool (the owner-requested MP fix), the
    // Dragon finally DEMANDS the elemental defense layer the design always
    // named as its counter ("six party-wide elemental breaths — one per
    // element, met by the M81 ward charms"). The clearability bar therefore
    // carries the designed counterplay — the Motley Aegis (−50% all six
    // elements) — exactly as the King's bar carries relics and snacks. The
    // plain-accessory loadout that cleared the pre-M89 dry-dragon now loses
    // (verified while tuning), which is the fight working as intended.
    for (Character& m : party.members) {
        m.accessory = "motley_aegis";
        refreshCharacter(m, db);
    }
    int totalRounds = 0;
    bool cleared = true;
    std::vector<dungeon::EnemyTeam> teams;
    for (int w = 0; w < kDragonWaveCount; ++w) {
        teams.push_back(dragonEliteWaveTeam(db, w));
    }
    teams.push_back(dragonTeam(db));
    for (const dungeon::EnemyTeam& team : teams) {
        battle::Battle b = battle::buildBattle(party, team, db);
        const battle::SimResult r = battle::simulateInPlace(b, db, 400);
        totalRounds += r.rounds;
        std::cout << "[dragon]   " << team.name << ": "
                  << (r.outcome == battle::Outcome::Victory ? "won" : "LOST") << " in "
                  << r.rounds << " rounds\n";
        if (r.outcome != battle::Outcome::Victory) {
            cleared = false;
            break;
        }
        carryBack(party, b);
    }
    std::cout << "[dragon] gauntlet: " << (cleared ? "CLEARED" : "LOST") << " in "
              << totalRounds << " rounds at " << kDragonScalePct << "%\n";
    CHECK(cleared);
}

// --- M89 (rules v16): the sustained pool & the lunge -------------------------

TEST_CASE("dragon: the authored MP pool scales like magic and sustains the breaths (M89)",
          "[dragon]") {
    const content::ContentDatabase db = loadContent();
    const content::BossDef* d = db.findBoss(kDragonBossId);
    REQUIRE(d != nullptr);
    CHECK(d->maxMp == 80);
    CHECK(d->basicAttackEveryNth == 4);
    CHECK_FALSE(d->basicAttackText.empty());

    battle::Battle b = battle::buildBattle(maxedParty(db), dragonTeam(db), db);
    const battle::Combatant* dragon = nullptr;
    for (const battle::Combatant& u : b.units) {
        if (u.isBoss) {
            dragon = &u;
        }
    }
    REQUIRE(dragon != nullptr);
    // The override replaces the derived pool (500%-scaled magic gave ~130 —
    // six breaths, then a dry dragon: the owner-reported defect) and scales
    // exactly as magic would: 80 × 500% = 400.
    CHECK(dragon->maxMp == deriveMaxMp(80 * kDragonScalePct / 100));
    CHECK(dragon->maxMp >= 12 * 20);  // at least a dozen 20-MP breaths
    CHECK(dragon->basicAttackEveryNth == 4);
    CHECK_FALSE(dragon->basicAttackText.empty());
}

TEST_CASE("dragon: every 4th own turn is a lunge, not a cast (M89)", "[dragon]") {
    const content::ContentDatabase db = loadContent();
    battle::Battle b = battle::buildBattle(maxedParty(db), dragonTeam(db), db);
    int actor = -1;
    for (std::size_t i = 0; i < b.units.size(); ++i) {
        if (b.units[i].isBoss) {
            actor = static_cast<int>(i);
        }
    }
    REQUIRE(actor >= 0);
    // Walk twelve of the Dragon's own turns through the real per-turn seam:
    // turns 4/8/12 are lunges (plain swings), every other turn a breath —
    // MP never runs out because chooseEnemyAction only decides here.
    for (int turn = 1; turn <= 12; ++turn) {
        b.beginUnitTurn(actor);
        const battle::Combatant& self = b.units[static_cast<std::size_t>(actor)];
        CHECK(self.ownTurnsTaken == turn);
        const bool lunge = battle::basicAttackTurn(self);
        CHECK(lunge == (turn % 4 == 0));
        const battle::EnemyChoice c = battle::chooseEnemyAction(b, actor, db);
        CHECK(c.useSkill == !lunge);
        CHECK(c.target >= 0);
    }
    // The rule is off for every pre-M89 unit (basicAttackEveryNth == 0).
    battle::Combatant plain;
    plain.ownTurnsTaken = 8;
    CHECK_FALSE(battle::basicAttackTurn(plain));
}
