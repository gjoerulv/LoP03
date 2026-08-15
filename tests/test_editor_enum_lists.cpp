// M59 — the enum-id lists (content/Enums) the editor's pickers offer. Every
// offered id must parse through the loader's own parse* function (one table,
// one truth), and the lists must be complete (a new enum value that is not
// listed would be un-authorable from the editor).

#include <catch2/catch_test_macros.hpp>

#include "content/Enums.hpp"

using namespace cd::content;

namespace {

template <typename Parser>
void requireAllParse(const std::vector<std::string_view>& ids, Parser parse) {
    REQUIRE_FALSE(ids.empty());
    for (std::string_view id : ids) {
        INFO(std::string(id));
        REQUIRE(parse(id).has_value());
    }
}

}  // namespace

TEST_CASE("editor: every listed enum id parses", "[editor]") {
    requireAllParse(elementIds(), parseElement);
    requireAllParse(skillCategoryIds(), parseSkillCategory);
    requireAllParse(skillEffectIds(), parseSkillEffect);
    requireAllParse(skillTargetIds(), parseSkillTarget);
    requireAllParse(enemyTagIds(), parseEnemyTag);
    requireAllParse(enemyTierIds(), parseEnemyTier);
    requireAllParse(enemyRoleIds(), parseEnemyRole);
    requireAllParse(itemTypeIds(), parseItemType);
    requireAllParse(equipSlotIds(), parseEquipSlot);
    requireAllParse(rarityIds(), parseRarity);
    requireAllParse(consumableEffectIds(), parseConsumableEffect);
    requireAllParse(statusTypeIds(), parseStatusType);
    requireAllParse(battleTargetIds(), parseBattleTarget);
    requireAllParse(bossArchetypeIds(), parseBossArchetype);
    requireAllParse(passiveHookIds(), parsePassiveHook);
    requireAllParse(triggerWhenIds(), parseTriggerWhen);  // M75
    requireAllParse(triggerDoIds(), parseTriggerDo);      // M75
}

TEST_CASE("editor: enum lists are complete", "[editor]") {
    // Counts pinned to the tables; adding an enum value without updating its
    // table already breaks parsing, so the lists stay complete by sharing it.
    REQUIRE(elementIds().size() == 7);        // none + 6 elements
    REQUIRE(skillCategoryIds().size() == 4);
    REQUIRE(skillEffectIds().size() == 7);    // none + 6 (M75: break_reflect, uncurse)
    REQUIRE(skillTargetIds().size() == 5);
    REQUIRE(enemyTagIds().size() == 4);
    REQUIRE(enemyTierIds().size() == 2);
    REQUIRE(enemyRoleIds().size() == 7);
    REQUIRE(itemTypeIds().size() == 5);       // +1 M96 (heirloom)
    REQUIRE(equipSlotIds().size() == 5);      // none + 4 (M96: heirloom)
    REQUIRE(rarityIds().size() == 5);
    REQUIRE(consumableEffectIds().size() == 5);  // none + 4
    REQUIRE(statusTypeIds().size() == 14);       // none + 13 (M75: reflect/sleep/curse)
    REQUIRE(battleTargetIds().size() == 2);
    REQUIRE(bossArchetypeIds().size() == 4);
    REQUIRE(passiveHookIds().size() == 10);      // "none" deliberately absent
    REQUIRE(triggerWhenIds().size() == 4);       // M75; "none" deliberately absent
    REQUIRE(triggerDoIds().size() == 8);         // M75; "none" absent; +1 M96 (heal_self_pct)
    // toString round-trips through the same table for a spot value.
    REQUIRE(parseElement(toString(Element::Fire)).value() == Element::Fire);
}
