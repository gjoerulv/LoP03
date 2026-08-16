// M96 (rules v18) — heirlooms: the fourth worn slot, whose effects are M75
// triggers attached party-side plus a conditional low-HP edge on the
// Brute-enrage pattern. A party wearing none resolves byte-identically to
// v17; everything here referees the worn case.

#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <string>

#include "battle/Battle.hpp"
#include "content/ContentDatabase.hpp"
#include "content/ContentLoader.hpp"
#include "content/LoadReport.hpp"
#include "game/Party.hpp"
#include "save/SaveSystem.hpp"

using namespace cd;

namespace {

const content::ContentDatabase& db() {
    static content::ContentDatabase database = [] {
        content::ContentDatabase d;
        content::LoadReport rep;
        REQUIRE(content::loadAll(std::filesystem::path(CRYSTAL_TEST_DATA_DIR), d, rep));
        return d;
    }();
    return database;
}

Party wearerParty(const std::string& heirloomId) {
    Party p;
    p.members.push_back(createCharacter(*db().findClass("knight"), "Rolan", 20));
    p.members[0].equippedHeirloom = heirloomId;
    refreshCharacter(p.members[0], db());
    return p;
}

dungeon::EnemyTeam anyFoe() {
    dungeon::EnemyTeam team;
    for (const auto& [id, def] : db().enemies()) {
        if (!def.bossOnly) {
            team.enemyIds = {id};
            team.name = def.name;
            break;
        }
    }
    return team;
}

}  // namespace

TEST_CASE("heirlooms: the shipped set is heirloom-shaped", "[heirloom][content]") {
    int heirlooms = 0;
    for (const auto& [id, def] : db().items()) {
        if (def.type != content::ItemType::Heirloom) {
            continue;
        }
        ++heirlooms;
        INFO(id);
        CHECK(def.slot == content::EquipSlot::Heirloom);
        CHECK(def.value == 0);  // story-granted, sold nowhere
        // An heirloom DOES something: triggers or the conditional edge.
        CHECK((!def.triggers.empty() || def.lowHpAttackPct > 0));
        // Never a stat stick — the balance bar (effects, not numbers).
        CHECK(def.statBonus.maxHp == 0);
        CHECK(def.statBonus.attack == 0);
        // The conditional pair comes whole or not at all.
        CHECK((def.lowHpThresholdPct > 0) == (def.lowHpAttackPct > 0));
    }
    CHECK(heirlooms == 16);  // 2 per M97 cutscene choice
}

TEST_CASE("heirlooms: triggers attach to the wearer and fire (rules v18)",
          "[heirloom]") {
    // The owner's headline effect: +15% HP the first time below 50%.
    Party p = wearerParty("heirloom_emberwake");
    battle::Battle b = battle::buildBattle(p, anyFoe(), db());
    battle::Combatant& wearer = b.units[0];
    REQUIRE_FALSE(wearer.triggers.empty());

    wearer.hp = wearer.maxHp * 2 / 5;  // 40%: under the 50% threshold
    const int before = wearer.hp;
    const std::string log = b.beginUnitTurn(0);  // the M75 turn seam fires it
    CHECK(wearer.hp > before);
    CHECK(wearer.hp <= wearer.maxHp);
    CHECK_FALSE(log.empty());  // the authored announcement rides the log

    // FirstTime semantics: dropping low again fires nothing more.
    wearer.hp = wearer.maxHp / 5;
    const int again = wearer.hp;
    b.beginUnitTurn(0);
    CHECK(wearer.hp == again);

    // An unworn party carries no triggers (v17-identical).
    Party bare = wearerParty("");
    battle::Battle b2 = battle::buildBattle(bare, anyFoe(), db());
    CHECK(b2.units[0].triggers.empty());
}

TEST_CASE("heirlooms: the low-HP edge is conditional, like the enrage",
          "[heirloom]") {
    Party p = wearerParty("heirloom_lastlight");
    dungeon::EnemyTeam foe = anyFoe();
    foe.statScalePct = 500;  // a tanky target, so overkill never clamps a delta

    battle::Battle b = battle::buildBattle(p, foe, db());
    battle::Combatant& wearer = b.units[0];
    REQUIRE(wearer.lowHpAttackPct > 0);
    REQUIRE(wearer.lowHpThresholdPct > 0);

    // The shared query IS the damage-path condition: above the threshold the
    // edge sleeps, at/below it bites, healed back over it sheathes again.
    wearer.hp = wearer.maxHp;
    CHECK_FALSE(battle::lowHpEdgeActive(wearer));
    wearer.hp = wearer.maxHp * wearer.lowHpThresholdPct / 100;
    CHECK(battle::lowHpEdgeActive(wearer));
    wearer.hp = wearer.maxHp;
    CHECK_FALSE(battle::lowHpEdgeActive(wearer));
    wearer.hp = 0;
    CHECK_FALSE(battle::lowHpEdgeActive(wearer));  // the fallen have no edge

    // And it genuinely lands harder: the same swing at full vs desperate HP.
    battle::Battle calm = battle::buildBattle(p, foe, db());
    const int calmBefore = calm.units[1].hp;
    calm.attack(0, 1);
    const int calmDelta = calmBefore - calm.units[1].hp;

    battle::Battle low = battle::buildBattle(p, foe, db());
    low.units[0].hp =
        low.units[0].maxHp * low.units[0].lowHpThresholdPct / 100;
    const int lowBefore = low.units[1].hp;
    low.attack(0, 1);
    const int lowDelta = lowBefore - low.units[1].hp;
    CHECK(lowDelta > calmDelta);
}

TEST_CASE("heirlooms: the fourth slot equips, round-trips, and drops unknowns",
          "[heirloom][save]") {
    // Everyone may wear one — even the class whose joke is refusing gear.
    Party p = wearerParty("heirloom_emberwake");
    CHECK(canEquipSlot(p.members[0], content::EquipSlot::Heirloom, db()));
    Party goose;
    if (const content::ClassDef* g = db().findClass("goose")) {
        goose.members.push_back(createCharacter(*g, "Honk", 5));
        CHECK(canEquipSlot(goose.members[0], content::EquipSlot::Heirloom, db()));
    }

    const std::filesystem::path dir =
        std::filesystem::temp_directory_path() / "crystal_heirloom_save_test";
    std::filesystem::remove_all(dir);
    save::SaveSystem saves(db(), dir);
    content::LoadReport rep;
    REQUIRE(saves.save(save::SaveSlot::Manual1, p, rep));
    Party loaded;
    REQUIRE(saves.load(save::SaveSlot::Manual1, loaded, rep));
    CHECK(loaded.members[0].equippedHeirloom == "heirloom_emberwake");

    // An id the content no longer knows drops silently (the gear rule).
    p.members[0].equippedHeirloom = "no_such_heirloom";
    REQUIRE(saves.save(save::SaveSlot::Manual2, p, rep));
    Party dropped;
    REQUIRE(saves.load(save::SaveSlot::Manual2, dropped, rep));
    CHECK(dropped.members[0].equippedHeirloom.empty());
    std::filesystem::remove_all(dir);
}
