#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <string>

#include "content/ContentDatabase.hpp"
#include "content/ContentLoader.hpp"
#include "content/LoadReport.hpp"
#include "render/ActionTier.hpp"
#include "settings/Settings.hpp"

// M130: the animation tier is DERIVED from the authored skill fields — no
// schema moved — so the shipped data is pinned here: which skills are Grand,
// which Major, and that support never escalates. The speed rule (Fast skips
// the animations entirely) lives in one function, pinned too.

using cd::content::ContentDatabase;
using cd::content::LoadReport;
using cd::render::ActionTier;
using cd::settings::BattleSpeed;

namespace {

ContentDatabase loadShippedContent() {
    ContentDatabase db;
    LoadReport report;
    REQUIRE(cd::content::loadAll(std::filesystem::path(CRYSTAL_TEST_DATA_DIR), db, report));
    return db;
}

ActionTier tierOf(const ContentDatabase& db, const std::string& id) {
    const cd::content::SkillDef* s = db.findSkill(id);
    REQUIRE(s != nullptr);
    return cd::render::actionTierForSkill(*s);
}

}  // namespace

TEST_CASE("action tier: the shipped skills land where the plan put them", "[m130][content]") {
    const ContentDatabase db = loadShippedContent();
    for (const char* id : {"inferno", "blizzard", "radiance", "chain_lightning",
                           "sovereigns_cataclysm", "breath_of_cinders", "breath_of_rime",
                           "breath_of_storms", "breath_of_barrows", "breath_of_dawns",
                           "breath_of_gloaming", "meteor_dive", "eviscerate", "sovereign_smite",
                           "execute"}) {
        INFO(id);
        CHECK(tierOf(db, id) == ActionTier::Grand);
    }
    for (const char* id : {"whirlwind", "barrage", "cleave", "venom_mist", "aimed_shot",
                           "arcane_burst", "power_smash", "shadow_bolt", "holy_ray", "fireball",
                           "frost_lance"}) {
        INFO(id);
        CHECK(tierOf(db, id) == ActionTier::Major);
    }
    for (const char* id : {"strike", "spark", "stone_edge", "smite", "mend", "greater_heal",
                           "group_mend", "generous_mending", "weaken", "sunder", "war_drums",
                           "guard_aura", "intimidate", "veil_of_slumber", "everyone_is_welcome",
                           "summon_goose", "summon_sentinel", "summon_spring"}) {
        INFO(id);
        CHECK(tierOf(db, id) == ActionTier::Minor);
    }
    // Support never escalates, whatever its breadth or power.
    for (const auto& [id, s] : db.skills()) {
        if (!cd::content::skillDealsDamage(s) || cd::content::isSummonSkill(s)) {
            INFO(id);
            CHECK(cd::render::actionTierForSkill(s) == ActionTier::Minor);
        }
    }
    CHECK(cd::render::actionTierForBasicAttack(false) == ActionTier::Minor);
    CHECK(cd::render::actionTierForBasicAttack(true) == ActionTier::Grand);  // the Dragon's sweep
}

TEST_CASE("action tier: the speed rule and the timing table", "[m130]") {
    using cd::render::actionWindupSeconds;
    CHECK(cd::render::actionFxEnabled(BattleSpeed::Normal));
    CHECK_FALSE(cd::render::actionFxEnabled(BattleSpeed::Fast));
    CHECK_FALSE(cd::render::actionFxEnabled(BattleSpeed::Instant));
    // Normal: the windup grows with the tier; the base 0.18 is the floor.
    CHECK(actionWindupSeconds(ActionTier::Minor, BattleSpeed::Normal) == 0.24f);
    CHECK(actionWindupSeconds(ActionTier::Major, BattleSpeed::Normal) == 0.30f);
    CHECK(actionWindupSeconds(ActionTier::Grand, BattleSpeed::Normal) == 0.42f);
    CHECK(actionWindupSeconds(ActionTier::Minor, BattleSpeed::Normal) > cd::render::kWindupBase);
    // Fast and Instant: exactly today's beat, whatever the tier.
    for (ActionTier t : {ActionTier::Minor, ActionTier::Major, ActionTier::Grand}) {
        CHECK(actionWindupSeconds(t, BattleSpeed::Fast) == cd::render::kWindupBase);
        CHECK(actionWindupSeconds(t, BattleSpeed::Instant) == cd::render::kWindupBase);
    }
    // The whole burst (frames + tail) fits inside the Normal settle pause, so a
    // tail never lengthens a turn; a Grand action adds only its windup growth.
    for (ActionTier t : {ActionTier::Minor, ActionTier::Major, ActionTier::Grand}) {
        CHECK(cd::render::actionBurstSeconds(t) <= cd::settings::resolveSeconds(BattleSpeed::Normal));
    }
    CHECK(actionWindupSeconds(ActionTier::Grand, BattleSpeed::Normal) - cd::render::kWindupBase <=
          0.25f);
    CHECK(cd::render::actionLungePixels(ActionTier::Minor) == 4.0f);  // today's lunge
    CHECK(cd::render::actionFrames(ActionTier::Grand) <= 6);          // art bible: effects <= 6 frames
}

TEST_CASE("action tier: frame indexing holds the last frame", "[m130]") {
    using cd::render::frameIndex;
    CHECK(frameIndex(-1.0f, 4, 0.07f) == 0);
    CHECK(frameIndex(0.0f, 4, 0.07f) == 0);
    CHECK(frameIndex(0.069f, 4, 0.07f) == 0);
    CHECK(frameIndex(0.071f, 4, 0.07f) == 1);
    CHECK(frameIndex(0.21f, 4, 0.07f) == 3);
    CHECK(frameIndex(9.0f, 4, 0.07f) == 3);
    CHECK(frameIndex(0.5f, 1, 0.07f) == 0);
    CHECK(frameIndex(0.5f, 4, 0.0f) == 0);
}
