// M121 - skill kinds, the kind line, and milestone-aware skill text: the kind
// is derived when content loads (never authored), the text a member sees
// follows the milestones THAT member holds, the "touches" predicates agree
// with what the battle actually pays, and the loader guards the new
// milestones.json `skillTexts` field.

#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "battle/Battle.hpp"
#include "content/ContentDatabase.hpp"
#include "content/ContentLoader.hpp"
#include "content/Definitions.hpp"
#include "content/LoadReport.hpp"
#include "game/Character.hpp"
#include "game/Party.hpp"
#include "game/SkillInfo.hpp"

using namespace cd;
using content::SkillKind;

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

Character member(const std::string& classId, const std::string& m10 = "",
                 const std::string& m20 = "", const std::string& m30 = "") {
    Character c;
    c.name = "Tester";
    c.classId = classId;
    c.level = 30;
    c.milestone10 = m10;
    c.milestone20 = m20;
    c.milestone30 = m30;
    return c;
}

bool names(const std::vector<const content::MilestoneDef*>& list, const std::string& id) {
    for (const content::MilestoneDef* m : list) {
        if (m->id == id) {
            return true;
        }
    }
    return false;
}

content::Json parse(const char* text) { return content::Json::parse(text, nullptr, false); }

}  // namespace

TEST_CASE("skill kinds: every shipped skill carries its derived kind", "[skillinfo][m121]") {
    struct Row {
        const char* id;
        SkillKind kind;
    };
    const Row rows[] = {
        {"fireball", SkillKind::Fire},          {"frost_lance", SkillKind::Ice},
        {"spark", SkillKind::Lightning},        {"stone_edge", SkillKind::Earth},
        {"holy_ray", SkillKind::Holy},          {"shadow_bolt", SkillKind::Dark},
        {"shadow_strike", SkillKind::Dark},     {"strike", SkillKind::NonElemental},
        {"venom_mist", SkillKind::NonElemental}, {"thought_thief", SkillKind::NonElemental},
        {"mirrorbreak", SkillKind::NonElemental},
        // A Support skill with power wounds an enemy (M40): damage, not a debuff.
        {"blinding_curse", SkillKind::NonElemental},
        {"mend", SkillKind::Heal},              {"renew", SkillKind::Heal},
        {"purify", SkillKind::Heal},            {"honking_comfort", SkillKind::Heal},
        {"absolve", SkillKind::Heal},  // an uncurse on an ally is restorative
        {"battle_cry", SkillKind::Buff},        {"bulwark", SkillKind::Buff},
        {"taunt", SkillKind::Buff},             {"fade", SkillKind::Buff},
        {"vanishing_act", SkillKind::Buff},
        {"weaken", SkillKind::Debuff},          {"silence", SkillKind::Debuff},
        {"intimidate", SkillKind::Debuff},      {"veil_of_slumber", SkillKind::Debuff},
        {"everyone_is_welcome", SkillKind::Debuff},
        // A summon wins over whatever its creature does.
        {"summon_goose", SkillKind::Summon},    {"summon_sentinel", SkillKind::Summon},
        {"summon_spring", SkillKind::Summon},
    };
    for (const Row& row : rows) {
        INFO(row.id);
        CHECK(skill(row.id).kind == row.kind);
    }
    // Assigned at load for EVERY skill, by the one rule - and the rule's
    // invariants hold across the whole shipped set.
    for (const auto& [id, s] : db().skills()) {
        INFO(id);
        CHECK(s.kind == content::skillKindFor(s));
        if (content::isSummonSkill(s)) {
            CHECK(s.kind == SkillKind::Summon);
        } else if (s.category == content::SkillCategory::Heal) {
            CHECK(s.kind == SkillKind::Heal);
        } else if (content::skillDealsDamage(s)) {
            CHECK(s.kind == content::damageKindFor(s.element));
        } else if (s.kind != SkillKind::Heal) {
            CHECK(s.kind == (content::targetsEnemies(s) ? SkillKind::Debuff : SkillKind::Buff));
        }
        CHECK(skillKindLine(s).back() == '.');
    }
}

TEST_CASE("skill kinds: a hand-built skill derives the same way", "[skillinfo][m121]") {
    content::SkillDef s;
    s.category = content::SkillCategory::Magic;
    s.target = content::SkillTarget::AllEnemies;
    s.element = content::Element::Ice;
    s.power = 12;
    CHECK(content::skillKindFor(s) == SkillKind::Ice);
    s.power = 0;  // a powerless enemy-facing spell wounds nobody
    CHECK(content::skillKindFor(s) == SkillKind::Debuff);
    s.category = content::SkillCategory::Support;
    s.target = content::SkillTarget::AllAllies;
    CHECK(content::skillKindFor(s) == SkillKind::Buff);
    s.oncePerRun = true;  // once-per-run alone is not a summon...
    CHECK(content::skillKindFor(s) == SkillKind::Buff);
    s.summonName = "The Test Beast";  // ...a named creature is
    CHECK(content::skillKindFor(s) == SkillKind::Summon);

    // The icon vocabulary is one id per kind, in kind order.
    CHECK(content::skillKindTextureId(SkillKind::Fire) == "ui.icon.skill.fire");
    CHECK(content::skillKindTextureId(SkillKind::NonElemental) == "ui.icon.skill.neutral");
    CHECK(content::skillKindTextureId(SkillKind::Summon) == "ui.icon.skill.summon");
    CHECK(content::kSkillKindCount == 11);
}

TEST_CASE("skill kinds: the kind line says what a skill does - and when it wounds nobody",
          "[skillinfo][m121]") {
    CHECK(skillKindLine(skill("fireball")) == "Fire damage.");
    CHECK(skillKindLine(skill("strike")) == "Non-elemental damage.");
    CHECK(skillKindLine(skill("mend")) == "Healing - no damage.");
    CHECK(skillKindLine(skill("battle_cry")) == "Buff - no damage.");
    CHECK(skillKindLine(skill("weaken")) == "Debuff - no damage.");
    CHECK(skillKindLine(skill("summon_goose")) == "Summon - Non-elemental damage.");
    CHECK(skillKindLine(skill("summon_sentinel")) == "Summon - Holy damage.");
    CHECK(skillKindLine(skill("summon_spring")) == "Summon - healing, no damage.");
}

TEST_CASE("skill text: an authored adjustment shows only on the member who holds the milestone",
          "[skillinfo][m121]") {
    const content::SkillDef& renew = skill("renew");

    const SkillTextFor plain = skillTextFor(member("cleric"), renew, db());
    CHECK(plain.description == renew.description);
    CHECK_FALSE(plain.adjusted);
    CHECK_FALSE(plain.marked());

    const SkillTextFor blessed =
        skillTextFor(member("cleric", "", "cleric_20_b"), renew, db());
    CHECK(blessed.adjusted);
    CHECK(blessed.marked());
    CHECK(blessed.description.find("35%") != std::string::npos);
    CHECK(blessed.description.find("20%") == std::string::npos);
    CHECK(names(blessed.milestones, "cleric_20_b"));
    CHECK(blessed.noted.empty());  // the adjusted text already says what changed

    // Another member who learned Renew from a scroll holds no such milestone.
    Character knight = member("knight", "knight_10_a");
    knight.extraSkills.push_back("renew");
    CHECK(skillTextFor(knight, renew, db()).description == renew.description);

    // The other shipped by-name adjustments.
    CHECK(skillTextFor(member("cleric", "", "cleric_20_a"), skill("purify"), db()).adjusted);
    CHECK(skillTextFor(member("guardian", "", "guardian_20_a"), skill("taunt"), db()).adjusted);
    const Character goose = member("goose", "", "goose_20_a");
    CHECK(skillTextFor(goose, skill("honking_comfort"), db()).adjusted);
    CHECK(skillTextFor(goose, skill("generous_mending"), db()).adjusted);
    CHECK(skillTextFor(goose, skill("generous_mending"), db())
              .description.find("alas") == std::string::npos);
    // The un-adjusted Goose still braces the enemies, and says so.
    CHECK(skillTextFor(member("goose"), skill("generous_mending"), db())
              .description.find("alas") != std::string::npos);
}

TEST_CASE("skill text: a generic boost marks the skills it touches with its own sentence",
          "[skillinfo][m121]") {
    const Character mage = member("mage", "mage_10_b", "mage_20_b", "mage_30_b");

    const SkillTextFor fireball = skillTextFor(mage, skill("fireball"), db());
    CHECK_FALSE(fireball.adjusted);
    CHECK(fireball.description == skill("fireball").description);
    CHECK(names(fireball.noted, "mage_10_b"));        // Arcane Edge: magic skills
    CHECK_FALSE(names(fireball.noted, "mage_30_b"));  // Devastation: all-enemy spells only
    CHECK_FALSE(names(fireball.noted, "mage_20_b"));  // Lingering Hex: no status to extend

    const SkillTextFor inferno = skillTextFor(mage, skill("inferno"), db());
    CHECK(names(inferno.noted, "mage_10_b"));
    CHECK(names(inferno.noted, "mage_30_b"));

    // Lingering Hex follows any status the skill applies - never a turn-control one.
    CHECK(names(skillTextFor(mage, skill("venom_mist"), db()).noted, "mage_20_b"));
    CHECK(names(skillTextFor(mage, skill("weaken"), db()).noted, "mage_20_b"));
    CHECK_FALSE(skillTextFor(mage, skill("royal_decree"), db()).marked());  // Stunned

    // A physical skill is none of a Mage's business.
    CHECK_FALSE(skillTextFor(mage, skill("strike"), db()).marked());

    // The details sheet carries the sentence.
    const std::string sheet = skillDetailsBody(skill("fireball"), &mage, db(), 6);
    CHECK(sheet.find("MP cost 6.  Fire damage.") == 0);
    CHECK(sheet.find("Milestone - Arcane Edge: Magic skills deal +10% damage.") !=
          std::string::npos);
    // Nobody in particular (a scroll on the shelf): the base text, no milestone lines.
    const std::string shelf = skillDetailsBody(skill("fireball"), nullptr, db(), 6);
    CHECK(shelf.find("Milestone") == std::string::npos);
    // A block reason rides under the header line.
    const std::string blocked =
        skillDetailsBody(skill("fireball"), &mage, db(), 12, "Not enough MP: 12 needed, 3 left.");
    CHECK(blocked.find("Fire damage.\nNot enough MP: 12 needed, 3 left.") != std::string::npos);
}

TEST_CASE("skill text: Devotion reaches Purify only through Purifying Light",
          "[skillinfo][m121]") {
    const content::SkillDef& purify = skill("purify");
    // Devotion alone: a pure cleanse heals nothing, so there is nothing to boost.
    const SkillTextFor devout = skillTextFor(member("cleric", "cleric_10_a"), purify, db());
    CHECK_FALSE(devout.marked());
    // With Purifying Light the cleanse heals - and Devotion boosts that heal.
    const SkillTextFor both =
        skillTextFor(member("cleric", "cleric_10_a", "cleric_20_a"), purify, db());
    CHECK(both.adjusted);
    CHECK(names(both.milestones, "cleric_20_a"));
    CHECK(names(both.noted, "cleric_10_a"));
    // Devotion boosts ordinary heals either way.
    CHECK(names(skillTextFor(member("cleric", "cleric_10_a"), skill("mend"), db()).noted,
                "cleric_10_a"));
}

TEST_CASE("skill text: the predicates agree with what the battle pays", "[skillinfo][m121]") {
    // Blessed Renew's 35% beats Renew's 20% in the battle itself...
    const content::MilestoneDef* blessed = db().findMilestone("cleric_20_b");
    REQUIRE(blessed != nullptr);
    CHECK(milestoneTouchesSkill(*blessed, skill("renew"), false));
    // ...and never turns a plain heal into a revive, so Mend is untouched.
    CHECK_FALSE(milestoneTouchesSkill(*blessed, skill("mend"), false));
    // A revive share that would NOT win is no upgrade.
    content::MilestoneDef weak = *blessed;
    weak.magnitude = 20;
    CHECK_FALSE(milestoneTouchesSkill(weak, skill("renew"), false));

    // Selective Generosity only matters where a status is shared with the foes.
    const content::MilestoneDef* selective = db().findMilestone("goose_20_a");
    REQUIRE(selective != nullptr);
    CHECK(milestoneTouchesSkill(*selective, skill("honking_comfort"), false));
    CHECK_FALSE(milestoneTouchesSkill(*selective, skill("mending_wind"), false));

    // Effects that change every blow (or no skill at all) touch no skill.
    for (const char* id : {"knight_10_a", "knight_20_b", "ranger_20_a", "mage_20_a",
                           "rogue_30_b", "guardian_20_b", "dragon_30_a", "cleric_30_b"}) {
        const content::MilestoneDef* m = db().findMilestone(id);
        REQUIRE(m != nullptr);
        for (const auto& [sid, s] : db().skills()) {
            INFO(id << " vs " << sid);
            CHECK_FALSE(milestoneTouchesSkill(*m, s, false));
        }
    }
}

TEST_CASE("skill text: the loader guards milestones.json skillTexts", "[skillinfo][m121][content]") {
    SECTION("the shipped adjustments name real skills and parse") {
        const content::MilestoneDef* m = db().findMilestone("goose_20_a");
        REQUIRE(m != nullptr);
        REQUIRE(m->skillTexts.size() == 2);
        CHECK(m->skillTexts[0].skill == "honking_comfort");
        CHECK(m->skillTexts[1].skill == "generous_mending");
        int withTexts = 0;
        for (const auto& [id, def] : db().milestones()) {
            for (const content::MilestoneSkillText& t : def.skillTexts) {
                INFO(id << " -> " << t.skill);
                CHECK(db().hasSkill(t.skill));
                CHECK_FALSE(t.description.empty());
            }
            withTexts += def.skillTexts.empty() ? 0 : 1;
        }
        CHECK(withTexts == 4);  // Purify, Renew, Taunt, the Goose's two heals
    }
    SECTION("a malformed list is reported") {
        content::ContentDatabase d;
        content::LoadReport rep;
        content::parseMilestones(parse(R"({"version":1,"milestones":[
            {"id":"x_10_a","classId":"knight","level":10,"option":"a","name":"X",
             "description":"d","effect":"grant_counter","skillTexts":"renew"}]})"),
                                 "mem", d, rep);
        CHECK_FALSE(rep.ok());
    }
    SECTION("an entry needs a skill and a non-empty description, once per skill") {
        for (const char* texts :
             {R"([{"description":"d"}])", R"([{"skill":"renew"}])",
              R"([{"skill":"renew","description":""}])",
              R"([{"skill":"renew","description":"a"},{"skill":"renew","description":"b"}])",
              R"([42])"}) {
            INFO(texts);
            content::ContentDatabase d;
            content::LoadReport rep;
            const std::string json =
                std::string(R"({"version":1,"milestones":[{"id":"x_10_a","classId":"knight",)") +
                R"("level":10,"option":"a","name":"X","description":"d",)" +
                R"("effect":"grant_counter","skillTexts":)" + texts + "}]}";
            content::parseMilestones(parse(json.c_str()), "mem", d, rep);
            CHECK_FALSE(rep.ok());
        }
    }
    SECTION("an adjustment for an unknown skill fails the reference pass") {
        content::ContentDatabase d;
        content::LoadReport rep;
        content::parseSkills(parse(R"({"version":1,"skills":[
            {"id":"renew","name":"Renew","category":"heal","target":"single_ally"}]})"),
                             "mem", d, rep);
        content::parseClasses(parse(R"({"version":1,"classes":[
            {"id":"knight","name":"Knight","baseStats":{"hp":10,"attack":1,"magic":1,
             "defense":1,"speed":1}}]})"),
                              "mem", d, rep);
        content::parseMilestones(parse(R"({"version":1,"milestones":[
            {"id":"k_10_a","classId":"knight","level":10,"option":"a","name":"A",
             "description":"d","effect":"grant_counter",
             "skillTexts":[{"skill":"renew","description":"fine"}]},
            {"id":"k_10_b","classId":"knight","level":10,"option":"b","name":"B",
             "description":"d","effect":"grant_counter",
             "skillTexts":[{"skill":"no_such_skill","description":"lost"}]}]})"),
                                 "mem", d, rep);
        REQUIRE(rep.ok());  // shape is fine; the dangling id is a cross-entry rule
        content::validateReferences(d, rep);
        CHECK_FALSE(rep.ok());
        bool named = false;
        for (const auto& e : rep.errors()) {
            named = named || e.message.find("no_such_skill") != std::string::npos;
        }
        CHECK(named);
    }
}
