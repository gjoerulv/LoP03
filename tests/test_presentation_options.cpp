#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <string>

#include "content/ContentDatabase.hpp"
#include "content/ContentLoader.hpp"
#include "content/Definitions.hpp"
#include "content/Enums.hpp"
#include "content/LoadReport.hpp"
#include "settings/Settings.hpp"
#include "states/AoeTint.hpp"
#include "states/TitlePhrases.hpp"
#include "ui/TextLayout.hpp"

// M51 — presentation & options. Pure helpers only (the raylib states are driven
// by hand in manual validation): the title-phrase table, the AoE-tint
// classification + alpha math.

using namespace cd;

namespace {
content::SkillDef skill(content::SkillTarget target, content::SkillCategory cat, int power,
                        content::StatusType status = content::StatusType::None) {
    content::SkillDef s;
    s.target = target;
    s.category = cat;
    s.power = power;
    s.statusEffect = status;
    return s;
}
}  // namespace

TEST_CASE("title phrases: the mandated line is present and none describes the genre",
          "[options][title]") {
    bool hasMandated = false;
    for (const char* phrase : kTitlePhrases) {
        const std::string s = phrase;
        REQUIRE_FALSE(s.empty());
        if (s == kMandatedTitlePhrase) {
            hasMandated = true;
        }
        // None of the phrases may describe what kind of game this is — the old
        // tagline did that and was removed. Guard the obvious genre words.
        std::string lower = s;
        std::transform(lower.begin(), lower.end(), lower.begin(),
                       [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        for (const char* banned : {"roguelite", "roguelike", "rpg", "jrpg", "dungeon-score",
                                   "turn-based"}) {
            INFO(phrase);
            CHECK(lower.find(banned) == std::string::npos);
        }
    }
    CHECK(hasMandated);
}

TEST_CASE("title phrases: every phrase fits the title width", "[options][title]") {
    // The subtitle row draws at the body font, centred, flanked by two pips; keep
    // a conservative margin so a phrase never collides with them. 7px/char at
    // font 8 over-estimates the real bitmap font, so a pass here fits in play.
    const ui::TextMeasure measure = [](const std::string& text, int fontSize) {
        return static_cast<int>(text.size()) * (fontSize * 7) / 10;
    };
    constexpr int kMaxWidth = 426 - 48;  // virtual width minus a pip/edge margin
    for (const char* phrase : kTitlePhrases) {
        INFO(phrase);
        CHECK(measure(phrase, 8) <= kMaxWidth);
    }
}

TEST_CASE("aoe tint: only all-target skills classify, with the right flavour",
          "[options][aoe]") {
    using content::SkillCategory;
    using content::SkillTarget;
    using content::StatusType;

    // Single-target actions never tint.
    CHECK(aoeTintForSkill(skill(SkillTarget::SingleEnemy, SkillCategory::Magic, 14)) ==
          AoeTint::None);
    CHECK(aoeTintForSkill(skill(SkillTarget::SingleAlly, SkillCategory::Heal, 18)) ==
          AoeTint::None);
    CHECK(aoeTintForSkill(skill(SkillTarget::Self, SkillCategory::Support, 0)) == AoeTint::None);

    // All-enemies damage -> Damage; all-enemies status-only -> Debuff.
    CHECK(aoeTintForSkill(skill(SkillTarget::AllEnemies, SkillCategory::Magic, 12)) ==
          AoeTint::Damage);
    CHECK(aoeTintForSkill(skill(SkillTarget::AllEnemies, SkillCategory::Support, 0,
                                StatusType::Poison)) == AoeTint::Debuff);

    // All-allies (a mass heal or buff) -> Heal.
    CHECK(aoeTintForSkill(skill(SkillTarget::AllAllies, SkillCategory::Heal, 16)) ==
          AoeTint::Heal);
    CHECK(aoeTintForSkill(skill(SkillTarget::AllAllies, SkillCategory::Support, 0,
                                StatusType::DefenseUp)) == AoeTint::Heal);
}

TEST_CASE("aoe tint: alpha respects the flash setting and caps at 0.12", "[options][aoe]") {
    using settings::EffectLevel;

    // None or Off -> nothing.
    CHECK(aoeTintAlpha(AoeTint::None, 1.0f, EffectLevel::Full) == 0.0f);
    CHECK(aoeTintAlpha(AoeTint::Damage, 1.0f, EffectLevel::Off) == 0.0f);
    CHECK(aoeTintAlpha(AoeTint::Damage, 0.0f, EffectLevel::Full) == 0.0f);

    // Full at peak strength caps at 0.12; Reduced is half.
    CHECK(aoeTintAlpha(AoeTint::Damage, 1.0f, EffectLevel::Full) == 0.12f);
    CHECK(aoeTintAlpha(AoeTint::Damage, 1.0f, EffectLevel::Reduced) == 0.06f);

    // It scales with the pulse strength (a single decay, never a strobe).
    const float mid = aoeTintAlpha(AoeTint::Heal, 0.5f, EffectLevel::Full);
    CHECK(mid > 0.0f);
    CHECK(mid < 0.12f);
}

TEST_CASE("shipped content: the elemental AoE spells classify as tinting actions",
          "[options][aoe][data]") {
    content::ContentDatabase db;
    content::LoadReport rep;
    REQUIRE(content::loadAll(std::filesystem::path(CRYSTAL_TEST_DATA_DIR), db, rep));

    // Inferno/Blizzard/Radiance are all_enemies damage; the Goose's ultimate is an
    // all_enemies mass debuff. Each must tint, proving the wiring reaches real data.
    for (const char* id : {"inferno", "blizzard", "radiance"}) {
        const content::SkillDef* s = db.findSkill(id);
        REQUIRE(s != nullptr);
        INFO(id);
        CHECK(aoeTintForSkill(*s) == AoeTint::Damage);
    }
    if (const content::SkillDef* ult = db.findSkill("everyone_is_welcome")) {
        CHECK(aoeTintForSkill(*ult) == AoeTint::Debuff);
    }
}
