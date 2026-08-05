// M77 — Enemy & boss offensive pass: the v15 vocabulary in enemy hands.
// Pure content on the v15 engine plus ONE scoped AI amendment (the support-loop
// gate for enemy-side all_enemies support skills — unreachable by any pre-M77
// content, see the M77 note §E). These cases prove the authored kits through
// the REAL defs and the shared battle model, pin the exact blast radius so
// untouched foes provably resolve as before, and lint the de-hinted telegraphs.

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <set>
#include <string>
#include <vector>

#include "battle/Battle.hpp"
#include "content/ContentDatabase.hpp"
#include "content/ContentLoader.hpp"
#include "dungeon/DungeonModel.hpp"
#include "game/Castle.hpp"
#include "game/Party.hpp"

using namespace cd;

namespace {

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

// A four-member L30 party (so speed/HP are realistic) against any team.
Party testParty() {
    Party p;
    for (const char* cls : {"knight", "ranger", "mage", "cleric"}) {
        p.members.push_back(createCharacter(*db().findClass(cls), cls, 30));
    }
    return p;
}

battle::Battle fightTeam(const dungeon::EnemyTeam& team) {
    battle::Battle b = battle::buildBattle(testParty(), team, db());
    REQUIRE(b.units.size() == 4 + static_cast<std::size_t>(team.count()));
    return b;
}

dungeon::EnemyTeam enemyOnly(std::vector<std::string> ids) {
    dungeon::EnemyTeam t;
    t.enemyIds = std::move(ids);
    return t;
}

dungeon::EnemyTeam bossTeam(const std::string& bossId, std::vector<std::string> minions = {}) {
    dungeon::EnemyTeam t;
    t.isBoss = true;
    t.bossId = bossId;
    t.enemyIds = std::move(minions);
    return t;
}

int unitBySource(const battle::Battle& b, const std::string& sourceId) {
    for (std::size_t i = 0; i < b.units.size(); ++i) {
        if (b.units[i].sourceId == sourceId) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

}  // namespace

// --- the new skills, as shipped ---------------------------------------------

TEST_CASE("offense: the new skills carry the intended shapes", "[offense]") {
    const auto skill = [](const char* id) {
        const content::SkillDef* s = db().findSkill(id);
        REQUIRE(s != nullptr);
        return s;
    };
    for (const char* id : {"thought_thief", "soul_tithe"}) {
        const content::SkillDef* s = skill(id);
        INFO(id);
        CHECK(s->category == content::SkillCategory::Magic);
        CHECK(s->mpDamagePct == 25);  // the owner's quarter rule
    }
    CHECK(skill("leaden_hex")->statusEffect == content::StatusType::Curse);
    CHECK(skill("leaden_hex")->power > 0);  // curses AND damages
    CHECK(skill("hexwings_grudge")->statusEffect == content::StatusType::Curse);
    CHECK(skill("hexwings_grudge")->target == content::SkillTarget::AllEnemies);
    {
        const content::SkillDef* s = skill("veil_of_slumber");
        CHECK(s->statusEffect == content::StatusType::Sleep);
        CHECK(s->target == content::SkillTarget::AllEnemies);
        CHECK(s->power == 0);  // a pure lullaby — no waking damage
    }
    CHECK(skill("drowsing_verdict")->statusEffect == content::StatusType::Sleep);
    CHECK(skill("drowsing_verdict")->power > 0);  // damage lands BEFORE the sleep rider
    // The King's stun is authored duration 2: turn-control statuses are exact
    // and tick at the bearer's turn start, so 2 costs exactly one turn (the
    // Tax Sheets precedent).
    CHECK(skill("royal_decree")->statusEffect == content::StatusType::Stunned);
    CHECK(skill("royal_decree")->statusDuration == 2);
    CHECK(skill("royal_decree")->power > 0);
    // The party-wide King and Duck abilities shipped as TRIGGERS, not skills —
    // an AI-chosen AoE with a short-lived status re-fires every turn (its gate
    // reopens before the boss's next action), which playtesting-by-battery
    // showed is a zero-agency lockdown. Triggers carry an authored cadence.
    CHECK(db().findSkill("sovereigns_lullaby") == nullptr);
    CHECK(db().findSkill("final_notice") == nullptr);
    CHECK(db().findSkill("duck_down") == nullptr);
}

// --- the blast radius, pinned ------------------------------------------------

TEST_CASE("offense: exactly the authored foes carry the new vocabulary", "[offense]") {
    // Pinning the exact sets proves every OTHER foe still resolves its battles
    // exactly as before M77 (no engine change reaches a foe outside these).
    std::set<std::string> triggered;
    std::set<std::string> initial;
    std::set<std::string> mannered;
    for (const auto& [id, def] : db().enemies()) {
        if (!def.triggers.empty()) {
            triggered.insert(id);
        }
        if (!def.initialStatuses.empty()) {
            initial.insert(id);
        }
        if (def.avoidSleepingTargets || def.noStunWhileAllFoesSleep) {
            mannered.insert(id);
        }
    }
    for (const auto& [id, def] : db().bosses()) {
        if (!def.triggers.empty()) {
            triggered.insert(id);
        }
        if (!def.initialStatuses.empty()) {
            initial.insert(id);
        }
        if (def.avoidSleepingTargets || def.noStunWhileAllFoesSleep || def.immuneToStatScale) {
            mannered.insert(id);
        }
    }
    CHECK(triggered == std::set<std::string>{"troll_berserker", "royal_guard_staff", "deep_king",
                                             "sand_warlord", "frost_monarch", "obsidian_colossus",
                                             "abyssal_tyrant", "the_hollow_king", "deadly_duck"});
    CHECK(initial == std::set<std::string>{"rune_sentry", "evil_goose_trickster",
                                           "crystal_sorcerer", "deadly_duck"});
    CHECK(mannered == std::set<std::string>{"the_hollow_king", "deadly_duck"});

    // The all-enemies support shape (the one the M77 AI amendment serves) is
    // carried by exactly these foes — no pre-M77 foe, which is the amendment's
    // byte-identity argument.
    std::set<std::string> aoeSupport;
    const auto scanKit = [&](const std::string& id, const std::vector<std::string>& kit) {
        for (const std::string& sid : kit) {
            const content::SkillDef* s = db().findSkill(sid);
            if (s != nullptr && s->category == content::SkillCategory::Support &&
                s->target == content::SkillTarget::AllEnemies) {
                aoeSupport.insert(id);
            }
        }
    };
    for (const auto& [id, def] : db().enemies()) {
        scanKit(id, def.skills);
    }
    for (const auto& [id, def] : db().bosses()) {
        scanKit(id, def.skills);
    }
    CHECK(aoeSupport == std::set<std::string>{"evil_goose_hexwing", "blight_chanter",
                                              "hollow_sovereign"});
}

TEST_CASE("offense: dark weaknesses are a few foes and exactly one boss", "[offense]") {
    std::set<std::string> weakEnemies;
    for (const auto& [id, def] : db().enemies()) {
        if (def.affinity.weakTo(content::Element::Dark)) {
            weakEnemies.insert(id);
        }
        CHECK_FALSE(def.affinity.immuneTo(content::Element::Dark));  // never a trap
    }
    CHECK(weakEnemies ==
          std::set<std::string>{"wisp", "crystal_guardian", "royal_guard_sword"});
    int weakBosses = 0;
    for (const auto& [id, def] : db().bosses()) {
        if (def.affinity.weakTo(content::Element::Dark)) {
            ++weakBosses;
            CHECK(id == "crystal_sorcerer");  // the owner's "one boss"
        }
        CHECK_FALSE(def.affinity.immuneTo(content::Element::Dark));
    }
    CHECK(weakBosses == 1);
}

TEST_CASE("offense: the reworked kits contain their new pieces", "[offense]") {
    const content::BossDef* king = db().findBoss(kKingBossId);
    REQUIRE(king != nullptr);
    const auto has = [](const std::vector<std::string>& kit, const char* id) {
        return std::find(kit.begin(), kit.end(), std::string(id)) != kit.end();
    };
    CHECK(has(king->skills, "drowsing_verdict"));
    CHECK(has(king->skills, "royal_decree"));
    CHECK(king->avoidSleepingTargets);
    REQUIRE(king->triggers.size() == 1);
    CHECK(king->triggers[0].when == content::TriggerWhen::EveryNthOwnTurn);
    CHECK(king->triggers[0].threshold == 5);
    CHECK(king->triggers[0].status == content::StatusType::Sleep);

    const content::BossDef* duck = db().findBoss(kDuckBossId);
    REQUIRE(duck != nullptr);
    CHECK(duck->skills.empty());  // his abilities are triggers, not casts
    CHECK(duck->immuneToStatScale);
    CHECK(duck->noStunWhileAllFoesSleep);
    REQUIRE(duck->initialStatuses.size() == 1);
    CHECK(duck->initialStatuses[0].type == content::StatusType::Reflect);
    REQUIRE(duck->triggers.size() == 2);
    CHECK(duck->triggers[0].when == content::TriggerWhen::EveryNthOwnTurn);
    CHECK(duck->triggers[0].threshold == 4);
    CHECK(duck->triggers[0].status == content::StatusType::Stunned);
    CHECK(duck->triggers[1].when == content::TriggerWhen::EveryNthHitTaken);
    CHECK(duck->triggers[1].threshold == 12);
    CHECK(duck->triggers[1].status == content::StatusType::Sleep);

    CHECK(has(db().findEnemy("evil_goose_hexwing")->skills, "hexwings_grudge"));
    CHECK(has(db().findEnemy("blight_chanter")->skills, "veil_of_slumber"));
    CHECK(has(db().findBoss("hollow_sovereign")->skills, "veil_of_slumber"));
    CHECK(has(db().findEnemy("royal_guard_sword")->skills, "leaden_hex"));
    CHECK(has(db().findBoss("blight_matron")->skills, "leaden_hex"));
    CHECK(has(db().findBoss("dread_sovereign")->skills, "leaden_hex"));
    CHECK(has(db().findEnemy("hex_wisp")->skills, "thought_thief"));
    CHECK(has(db().findEnemy("void_weaver")->skills, "thought_thief"));
    CHECK(has(db().findEnemy("soul_render")->skills, "soul_tithe"));
}

// --- the behaviors, through the shared model ---------------------------------

TEST_CASE("offense: an MP-drain skill takes HP and a quarter of it in MP", "[offense]") {
    battle::Battle b = fightTeam(enemyOnly({"hex_wisp"}));
    const int wisp = unitBySource(b, "hex_wisp");
    REQUIRE(wisp >= 0);
    battle::Combatant& hero = b.units[0];
    const int hpBefore = hero.hp;
    const int mpBefore = hero.mp;
    REQUIRE(mpBefore > 0);
    b.useSkill(wisp, 0, *db().findSkill("thought_thief"));
    const int dealt = hpBefore - hero.hp;
    CHECK(dealt > 0);
    CHECK(hero.mp == mpBefore - std::min(mpBefore, dealt * 25 / 100));
}

TEST_CASE("offense: the all-enemies support gate reads the party, not the caster",
          "[offense]") {
    battle::Battle b = fightTeam(enemyOnly({"evil_goose_hexwing"}));
    const int goose = unitBySource(b, "evil_goose_hexwing");
    REQUIRE(goose >= 0);
    b.units[static_cast<std::size_t>(goose)].mp = 99;           // isolate the gate from MP
    b.units[static_cast<std::size_t>(goose)].doNothingPct = 0;  // and from the quack roll
    b.turnsTaken = 1;

    // Nobody cursed: the grudge is the pick, aimed into the party.
    const battle::EnemyChoice first = battle::chooseEnemyAction(b, goose, db());
    REQUIRE(first.useSkill);
    CHECK(first.skillId == "hexwings_grudge");
    b.useSkill(goose, first.target, *db().findSkill("hexwings_grudge"));
    for (const battle::Combatant& u : b.units) {
        if (u.side == battle::Side::Party) {
            CHECK(battle::isCursed(u));
        }
    }

    // The profiled target is now cursed, so the gate closes — the goose does
    // something else instead of re-casting forever (the amendment's point).
    b.turnsTaken = 2;
    const battle::EnemyChoice second = battle::chooseEnemyAction(b, goose, db());
    CHECK((!second.useSkill || second.skillId != "hexwings_grudge"));
}

TEST_CASE("offense: the King's cradle-song rides his fifth turns and he spares sleepers",
          "[offense]") {
    battle::Battle b = fightTeam(kingTeam(db()));
    const int king = unitBySource(b, kKingBossId);
    REQUIRE(king >= 0);

    // Four of his turns pass quietly; the fifth sleeps the whole party.
    for (int t = 1; t <= 4; ++t) {
        CHECK(b.beginUnitTurn(king).find("cradle-song") == std::string::npos);
    }
    const std::string fifth = b.beginUnitTurn(king);
    CHECK(fifth.find("cradle-song") != std::string::npos);
    for (const battle::Combatant& u : b.units) {
        if (u.side == battle::Side::Party) {
            CHECK(battle::isAsleep(u));
        }
    }

    // Sleep-aware manners: one sleeper among the awake is never his target.
    for (battle::Combatant& u : b.units) {
        if (u.side == battle::Side::Party) {
            u.statuses.clear();
        }
    }
    b.units[0].statuses.push_back({content::StatusType::Sleep, 0, 9});
    for (int t = 1; t <= 12; ++t) {
        b.turnsTaken = t;
        const battle::EnemyChoice c = battle::chooseEnemyAction(b, king, db());
        if (c.target >= 0 &&
            b.units[static_cast<std::size_t>(c.target)].side == battle::Side::Party) {
            CHECK(c.target != 0);
        }
    }
}

TEST_CASE("offense: the verdict wounds then sleeps; the decree wounds then stuns",
          "[offense]") {
    battle::Battle b = fightTeam(kingTeam(db()));
    const int king = unitBySource(b, kKingBossId);
    REQUIRE(king >= 0);
    const int hp0 = b.units[0].hp;
    b.useSkill(king, 0, *db().findSkill("drowsing_verdict"));
    CHECK(b.units[0].hp < hp0);
    CHECK(battle::isAsleep(b.units[0]));  // the rider lands AFTER the damage

    const int hp1 = b.units[1].hp;
    b.useSkill(king, 1, *db().findSkill("royal_decree"));
    CHECK(b.units[1].hp < hp1);
    CHECK(battle::forcedActionFor(b.units[1]) == battle::ForcedAction::Skip);
}

TEST_CASE("offense: the Stave mirrors the King at low health, once", "[offense]") {
    battle::Battle b = fightTeam(kingTeam(db()));
    const int king = unitBySource(b, kKingBossId);
    const int stave = unitBySource(b, "royal_guard_staff");
    REQUIRE(king >= 0);
    REQUIRE(stave >= 0);
    CHECK_FALSE(battle::hasReflect(b.units[static_cast<std::size_t>(king)]));

    battle::Combatant& s = b.units[static_cast<std::size_t>(stave)];
    s.hp = s.maxHp * 30 / 100;
    const std::string fired = b.beginUnitTurn(stave);
    CHECK(fired.find("courtlight") != std::string::npos);
    CHECK(battle::hasReflect(b.units[static_cast<std::size_t>(king)]));
    CHECK(b.beginUnitTurn(stave).empty());  // first-time: never twice
}

TEST_CASE("offense: the Duck shrugs the Spoon, starts mirrored, minds his manners",
          "[offense]") {
    battle::Battle b = fightTeam(duckTeam(db()));
    const int duck = unitBySource(b, kDuckBossId);
    REQUIRE(duck >= 0);
    const battle::Combatant& d = b.units[static_cast<std::size_t>(duck)];
    CHECK(d.statScaleImmune);
    CHECK(battle::hasReflect(d));  // the mirror is up from the first breath
    const content::ItemDef* spoon = db().findItem("deadly_spoon");
    REQUIRE(spoon != nullptr);
    CHECK_FALSE(battle::itemAffects(b, duck, *spoon));  // the caller keeps it

    // Manners: while the whole party sleeps, his fourth-turn Final Notice
    // passes politely (the every-Nth beat is missed, not banked)...
    for (battle::Combatant& u : b.units) {
        if (u.side == battle::Side::Party) {
            u.statuses.push_back({content::StatusType::Sleep, 0, 99});
        }
    }
    for (int t = 1; t <= 4; ++t) {
        b.beginUnitTurn(duck);
    }
    for (const battle::Combatant& u : b.units) {
        if (u.side == battle::Side::Party) {
            CHECK_FALSE(battle::hasStatus(u, content::StatusType::Stunned));
        }
    }

    // ...and with anyone awake, the next fourth beat serves the whole party.
    for (battle::Combatant& u : b.units) {
        if (u.side == battle::Side::Party) {
            u.statuses.clear();
        }
    }
    for (int t = 5; t <= 8; ++t) {
        b.beginUnitTurn(duck);
    }
    for (const battle::Combatant& u : b.units) {
        if (u.side == battle::Side::Party) {
            CHECK(battle::hasStatus(u, content::StatusType::Stunned));
        }
    }
}

TEST_CASE("offense: the twelfth blow on the Duck shakes loose the sleeping down",
          "[offense]") {
    battle::Battle b = fightTeam(duckTeam(db()));
    const int duck = unitBySource(b, kDuckBossId);
    REQUIRE(duck >= 0);
    for (int hit = 1; hit <= 11; ++hit) {
        b.attack(hit % 2, duck);  // deliberate connecting hits, two attackers
        CHECK_FALSE(battle::isAsleep(b.units[0]));
    }
    b.attack(0, duck);  // the twelfth
    for (const battle::Combatant& u : b.units) {
        if (u.side == battle::Side::Party) {
            CHECK(battle::isAsleep(u));
        }
    }
}

TEST_CASE("offense: the goose wave opens with a mirror on the Trickster", "[offense]") {
    battle::Battle b = fightTeam(gooseWaveTeam(db()));
    const int trickster = unitBySource(b, "evil_goose_trickster");
    REQUIRE(trickster >= 0);
    CHECK(battle::hasReflect(b.units[static_cast<std::size_t>(trickster)]));
}

TEST_CASE("offense: the dungeon-boss triggers fire as authored", "[offense]") {
    SECTION("the Colossus rings back every fourth blow") {
        battle::Battle b = fightTeam(bossTeam("obsidian_colossus"));
        const int boss = unitBySource(b, "obsidian_colossus");
        REQUIRE(boss >= 0);
        for (int i = 0; i < 3; ++i) {
            b.attack(0, boss);
            CHECK_FALSE(battle::hasStatus(b.units[0], content::StatusType::Stunned));
        }
        b.attack(0, boss);  // the fourth deliberate connecting hit
        CHECK(battle::hasStatus(b.units[0], content::StatusType::Stunned));
        CHECK(battle::forcedActionFor(b.units[0]) == battle::ForcedAction::Skip);
    }
    SECTION("the Troll's fury feeds on its wounds, once") {
        battle::Battle b = fightTeam(enemyOnly({"troll_berserker"}));
        const int troll = unitBySource(b, "troll_berserker");
        REQUIRE(troll >= 0);
        battle::Combatant& t = b.units[static_cast<std::size_t>(troll)];
        const int atk = t.stats.attack;
        t.hp = t.maxHp / 2;
        CHECK(b.beginUnitTurn(troll).find("fury") != std::string::npos);
        CHECK(t.stats.attack == atk * 125 / 100);
        b.beginUnitTurn(troll);
        CHECK(t.stats.attack == atk * 125 / 100);  // first-time: never twice
    }
    SECTION("the Monarch freezes into a mirror at half") {
        battle::Battle b = fightTeam(bossTeam("frost_monarch"));
        const int boss = unitBySource(b, "frost_monarch");
        REQUIRE(boss >= 0);
        battle::Combatant& m = b.units[static_cast<std::size_t>(boss)];
        m.hp = m.maxHp / 2;
        b.beginUnitTurn(boss);
        CHECK(battle::hasReflect(m));
    }
    SECTION("the Deep King digs in at the last quarter") {
        battle::Battle b = fightTeam(bossTeam("deep_king"));
        const int boss = unitBySource(b, "deep_king");
        REQUIRE(boss >= 0);
        battle::Combatant& k = b.units[static_cast<std::size_t>(boss)];
        const int def = k.stats.defense;
        k.hp = k.maxHp / 5;
        b.beginUnitTurn(boss);
        CHECK(k.stats.defense == def * 150 / 100);
    }
    SECTION("the Tyrant surges over its first fallen follower") {
        battle::Battle b = fightTeam(bossTeam("abyssal_tyrant", {"corpse_hound"}));
        const int boss = unitBySource(b, "abyssal_tyrant");
        const int hound = unitBySource(b, "corpse_hound");
        REQUIRE(boss >= 0);
        REQUIRE(hound >= 0);
        battle::Combatant& t = b.units[static_cast<std::size_t>(boss)];
        const int atk = t.stats.attack;
        const int spd = t.stats.speed;
        CHECK(b.beginUnitTurn(boss).empty());  // nobody has fallen yet
        b.units[static_cast<std::size_t>(hound)].hp = 0;
        CHECK(b.beginUnitTurn(boss).find("howls") != std::string::npos);
        CHECK(t.stats.attack == atk * 125 / 100);
        CHECK(t.stats.speed == spd * 120 / 100);
    }
    SECTION("the Warlord storms every fourth of its turns") {
        battle::Battle b = fightTeam(bossTeam("sand_warlord"));
        const int boss = unitBySource(b, "sand_warlord");
        REQUIRE(boss >= 0);
        for (int t = 1; t <= 3; ++t) {
            b.beginUnitTurn(boss);
        }
        CHECK_FALSE(battle::isBlinded(b.units[0]));
        b.beginUnitTurn(boss);  // the fourth own turn
        for (const battle::Combatant& u : b.units) {
            if (u.side == battle::Side::Party) {
                CHECK(battle::isBlinded(u));
            }
        }
    }
}

// --- the de-hinted telegraphs -------------------------------------------------

TEST_CASE("offense: no telegraph coaches the player", "[offense]") {
    // The banned lexicon of the old coaching lines: mechanics, thresholds and
    // imperatives. Telegraphs are atmosphere now; the fight teaches itself.
    // ("spells" covers the old Spell Ward coaching; bare "ward" would snag the
    // Keep WARDen's own name.)
    const char* banned[] = {"half",   "first",  "guard",  "silence", "strike", "doubl",
                            "feeds",  "spells", "immune", "endure",  "rallies", "fizzle",
                            "below",  "stay down", "opening blow", "affliction"};
    for (const auto& [id, def] : db().bosses()) {
        std::string t = def.telegraph;
        std::transform(t.begin(), t.end(), t.begin(),
                       [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        for (const char* word : banned) {
            INFO(id << " telegraph contains banned '" << word << "': " << def.telegraph);
            CHECK(t.find(word) == std::string::npos);
        }
    }
}
