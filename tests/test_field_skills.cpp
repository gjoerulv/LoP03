// M122 - casting heals from the Party panel: which skills may be cast outside
// battle, where, why a cast is refused, and that a cast restores exactly what
// the battle would restore for the same caster (the shared battle/HealMath.hpp
// arithmetic plus the caster's milestones) for the same MP cost.

#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <string>

#include "battle/Battle.hpp"
#include "battle/HealMath.hpp"
#include "content/ContentDatabase.hpp"
#include "content/ContentLoader.hpp"
#include "content/LoadReport.hpp"
#include "dungeon/DungeonModel.hpp"
#include "game/Character.hpp"
#include "game/FieldSkills.hpp"
#include "game/Party.hpp"

using namespace cd;

namespace {

const content::ContentDatabase& db() {
    static const content::ContentDatabase loaded = [] {
        content::ContentDatabase d;
        content::LoadReport rep;
        content::loadAll(std::filesystem::path(CRYSTAL_TEST_DATA_DIR), d, rep);
        REQUIRE(rep.ok());
        return d;
    }();
    return loaded;
}

const content::SkillDef& skill(const std::string& id) {
    const content::SkillDef* s = db().findSkill(id);
    REQUIRE(s != nullptr);
    return *s;
}

Character member(const std::string& name, const std::string& classId, int hp, int maxHp, int mp,
                 int maxMp, int magic) {
    Character c;
    c.name = name;
    c.classId = classId;
    c.level = 30;
    c.hp = hp;
    c.maxHp = maxHp;
    c.mp = mp;
    c.maxMp = maxMp;
    c.stats.maxHp = maxHp;
    c.stats.magic = magic;
    return c;
}

// Cleric (magic 40, 50 MP) at full health; a hurt Knight; a fallen Rogue.
Party trio() {
    Party p;
    p.members.push_back(member("Mira", "cleric", 80, 80, 50, 50, 40));
    p.members.push_back(member("Rolan", "knight", 30, 120, 0, 10, 4));
    p.members.push_back(member("Sable", "rogue", 0, 90, 5, 20, 6));
    return p;
}

}  // namespace

TEST_CASE("field skills: only HP heals and revives leave the battle", "[fieldskills][m122]") {
    for (const char* id : {"mend", "group_mend", "greater_heal", "renew", "mending_wind",
                           "honking_comfort", "generous_mending"}) {
        INFO(id);
        CHECK(isFieldSkill(skill(id)));
    }
    // A pure cleanse has nothing to cleanse; a summon keeps its stagecraft;
    // nothing that is not a heal is ever a field skill.
    CHECK_FALSE(isFieldSkill(skill("purify")));
    CHECK_FALSE(isFieldSkill(skill("summon_spring")));
    CHECK_FALSE(isFieldSkill(skill("absolve")));
    CHECK_FALSE(isFieldSkill(skill("fireball")));
    CHECK_FALSE(isFieldSkill(skill("battle_cry")));
    int fieldCount = 0;
    for (const auto& [id, s] : db().skills()) {
        if (isFieldSkill(s)) {
            ++fieldCount;
            INFO(id);
            CHECK(s.category == content::SkillCategory::Heal);
            CHECK_FALSE(content::isSummonSkill(s));
        }
    }
    CHECK(fieldCount == 7);
}

TEST_CASE("field skills: a refused cast says why and costs nothing", "[fieldskills][m122]") {
    Party p = trio();
    const content::SkillDef& mend = skill("mend");

    // Town: never (the Inn is there for that). Dungeon: yes.
    CHECK(fieldSkillRefusal(p, 0, mend, /*inDungeon=*/false) ==
          "Heals are cast in dungeons - the Inn mends you in town.");
    CHECK(fieldSkillRefusal(p, 0, mend, true).empty());

    // Battle-only skills are refused anywhere, each with its own reason.
    CHECK(fieldSkillRefusal(p, 0, skill("summon_spring"), true) ==
          "A summon answers only in battle.");
    CHECK(fieldSkillRefusal(p, 0, skill("purify"), true) == "Nothing to cleanse outside battle.");
    CHECK(fieldSkillRefusal(p, 0, skill("fireball"), true) == "Its moment is in battle.");

    // MP and the fallen.
    CHECK(fieldSkillRefusal(p, 1, mend, true) == "Not enough MP: 5 needed, 0 left.");
    CHECK(fieldSkillRefusal(p, 2, mend, true) == "The fallen cast nothing.");
    CHECK(fieldSkillRefusal(p, 7, mend, true) == "Nobody to cast it.");

    // Nobody to help: a full-health party needs no mending; nobody fallen
    // needs no raising (Renew has power, so a hurt member is still a target).
    Party healthy = trio();
    healthy.members[1].hp = healthy.members[1].maxHp;
    healthy.members[2].hp = healthy.members[2].maxHp;
    CHECK(fieldSkillRefusal(healthy, 0, mend, true) == "Nobody needs healing.");
    CHECK(fieldSkillRefusal(healthy, 0, skill("renew"), true) == "Nobody needs healing.");

    // Per-target verdicts for a single-target skill.
    CHECK(fieldTargetRefusal(mend, p.members[1]).empty());
    CHECK(fieldTargetRefusal(mend, p.members[0]) == "Already at full HP.");
    CHECK(fieldTargetRefusal(mend, p.members[2]) == "The fallen need a revive, not a mend.");
    CHECK(fieldTargetRefusal(skill("renew"), p.members[2]).empty());
    CHECK(fieldSkillNeedsTarget(mend));
    CHECK_FALSE(fieldSkillNeedsTarget(skill("group_mend")));
}

TEST_CASE("field skills: a cast spends the MP and restores the battle's amount",
          "[fieldskills][m122]") {
    Party p = trio();
    const content::SkillDef& mend = skill("mend");  // power 20, MP 5
    REQUIRE(mend.power == 20);
    REQUIRE(mend.mpCost == 5);

    // power + magic/2 = 20 + 40/2 = 40 - the shared battle arithmetic.
    CHECK(fieldHealAmount(p.members[0], mend, db()) == 40);
    CHECK(battle::healBase(40, 20) == 40);

    const std::string line = applyFieldSkill(p, 0, 1, mend, db());
    CHECK(p.members[1].hp == 70);
    CHECK(p.members[0].mp == 45);
    CHECK(line == "Mira casts Mend. Rolan recovers 40 HP.");

    // The cap holds and the line reports what was actually gained.
    p.members[1].hp = 100;
    CHECK(applyFieldSkill(p, 0, 1, mend, db()) == "Mira casts Mend. Rolan recovers 20 HP.");
    CHECK(p.members[1].hp == 120);
    CHECK(p.members[0].mp == 40);
}

TEST_CASE("field skills: the caster's milestones ride along (Devotion, Blessed Renew)",
          "[fieldskills][m122]") {
    Party p = trio();
    p.members[0].milestone10 = "cleric_10_a";  // Devotion: heals +20%
    p.members[0].milestone20 = "cleric_20_b";  // Blessed Renew: raises at 35%
    CHECK(fieldHealAmount(p.members[0], skill("mend"), db()) == 48);  // 40 * 1.2

    const content::SkillDef& renew = skill("renew");  // revive 20%, power 12, MP 9
    REQUIRE(renew.reviveHpPct == 20);
    const std::string line = applyFieldSkill(p, 0, 2, renew, db());
    CHECK(p.members[2].hp == 31);  // 35% of 90, floored - the milestone's share wins
    CHECK(p.members[0].mp == 41);
    CHECK(line == "Mira casts Renew. Sable rises again!");

    // Without the milestone the skill's own 20% stands.
    Party plain = trio();
    applyFieldSkill(plain, 0, 2, renew, db());
    CHECK(plain.members[2].hp == 18);
    // And Renew still mends the living (power 12 + 20 = 32).
    applyFieldSkill(plain, 0, 1, renew, db());
    CHECK(plain.members[1].hp == 62);
}

TEST_CASE("field skills: a whole-party heal skips the fallen and the full, charges once",
          "[fieldskills][m122]") {
    Party p = trio();
    p.members[0].hp = 50;  // the caster is hurt too
    const content::SkillDef& group = skill("group_mend");  // power 14, MP 12
    const std::string line = applyFieldSkill(p, 0, -1, group, db());
    // 14 + 40/2 = 34 each: Mira 50 -> 80 (cap, +30), Rolan 30 -> 64 (+34); Sable stays down.
    CHECK(p.members[0].hp == 80);
    CHECK(p.members[1].hp == 64);
    CHECK(p.members[2].hp == 0);
    CHECK(p.members[0].mp == 38);
    CHECK(line == "Mira casts Group Mend. The party recovers 64 HP.");
}

TEST_CASE("field skills: the menu and the battle restore the same HP", "[fieldskills][m122]") {
    // The same Cleric, the same Mend, the same hurt Knight - once in a battle,
    // once from the panel. Devotion included.
    Party fieldParty = trio();
    fieldParty.members[0].milestone10 = "cleric_10_a";
    Party battleParty = fieldParty;

    dungeon::EnemyTeam team;
    REQUIRE_FALSE(db().enemies().empty());
    team.enemyIds.push_back(db().enemies().begin()->first);
    battle::Battle b = battle::buildBattle(battleParty, team, db());
    const int before = b.units[1].hp;
    b.useSkill(0, 1, skill("mend"));
    const int battleGain = b.units[1].hp - before;

    applyFieldSkill(fieldParty, 0, 1, skill("mend"), db());
    CHECK(fieldParty.members[1].hp - 30 == battleGain);
    CHECK(battleGain == 48);
}
