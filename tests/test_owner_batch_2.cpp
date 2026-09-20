// M126 - owner batch 2: the loot summary rows, the learned-skill icon flow,
// the basic attack as a single pick in the decision encounters, and the
// guarded chest walled in behind its guardian (generation v25).

#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <queue>
#include <string>
#include <vector>

#include "battle/Battle.hpp"
#include "content/ContentDatabase.hpp"
#include "content/ContentLoader.hpp"
#include "dungeon/DungeonGenerator.hpp"
#include "dungeon/RoomLayout.hpp"
#include "game/LootSummary.hpp"
#include "game/Party.hpp"
#include "game/SpecialEncounter.hpp"
#include "game/Spoils.hpp"
#include "ui/TagFlow.hpp"

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

Party makeParty(std::initializer_list<const char*> classes) {
    Party party;
    for (const char* id : classes) {
        const content::ClassDef* cls = db().findClass(id);
        REQUIRE(cls != nullptr);
        party.members.push_back(createCharacter(*cls, id, 12));
    }
    return party;
}

// Can `to` be walked to from `from`? `guardSolid` makes the guard a wall.
bool reaches(const dungeon::RoomLayout& l, dungeon::RoomLayout::Point from,
             dungeon::RoomLayout::Point to, bool guardSolid) {
    std::vector<char> seen(static_cast<std::size_t>(l.width * l.height), 0);
    const auto blocked = [&](int x, int y) {
        return !l.walkable(x, y) || (guardSolid && l.guard.x == x && l.guard.y == y);
    };
    if (blocked(from.x, from.y)) {
        return false;
    }
    std::queue<dungeon::RoomLayout::Point> q;
    q.push(from);
    seen[static_cast<std::size_t>(from.y * l.width + from.x)] = 1;
    constexpr int dx[4] = {0, 1, 0, -1};
    constexpr int dy[4] = {-1, 0, 1, 0};
    while (!q.empty()) {
        const dungeon::RoomLayout::Point p = q.front();
        q.pop();
        if (p == to) {
            return true;
        }
        for (int i = 0; i < 4; ++i) {
            const int nx = p.x + dx[i];
            const int ny = p.y + dy[i];
            if (!l.inBounds(nx, ny) || blocked(nx, ny)) {
                continue;
            }
            const std::size_t ni = static_cast<std::size_t>(ny * l.width + nx);
            if (seen[ni] == 0) {
                seen[ni] = 1;
                q.push({nx, ny});
            }
        }
    }
    return false;
}

}  // namespace

// ------------------------------------------------------------ loot rows ----

TEST_CASE("loot: gold is one total line and comes first", "[m126][loot]") {
    LootSummary loot;
    CHECK(loot.empty());
    CHECK(loot.rows().empty());
    loot.addGold(26);
    loot.addGold(0);    // nothing
    loot.addGold(-50);  // a loss is not a receipt
    loot.addGold(200);
    CHECK_FALSE(loot.empty());
    CHECK(loot.gold() == 226);
    const std::vector<LootRow> rows = loot.rows();
    REQUIRE(rows.size() == 1);
    CHECK(rows[0].isGold);
    CHECK(rows[0].icon.empty());
    CHECK(rows[0].text() == "226 gold");
}

TEST_CASE("loot: a repeated piece is counted, not listed twice", "[m126][loot]") {
    const content::ItemDef* ring = db().findItem("power_ring");
    const content::ItemDef* crown = db().findItem("dragon_crown");
    REQUIRE(ring != nullptr);
    REQUIRE(crown != nullptr);
    LootSummary loot;
    loot.addItem("power_ring", db());
    loot.addItem("dragon_crown", db());
    loot.addItem("power_ring", db());
    loot.addItem("power_ring", db(), 2);
    loot.addItem("", db());              // nothing
    loot.addItem("dragon_crown", db(), 0);  // nothing
    loot.addGold(40);
    const std::vector<LootRow> rows = loot.rows();
    REQUIRE(rows.size() == 3);
    CHECK(rows[0].isGold);
    CHECK(rows[0].text() == "40 gold");
    // Items keep the order they were first received in.
    CHECK_FALSE(rows[1].isGold);
    CHECK(rows[1].itemId == "power_ring");
    CHECK(rows[1].count == 4);
    CHECK(rows[1].text() == ring->name + " x4");
    CHECK(rows[1].icon == content::gearIconTextureId(*ring));
    CHECK(rows[2].itemId == "dragon_crown");
    CHECK(rows[2].count == 1);
    CHECK(rows[2].text() == crown->name);  // a single piece wears no count
    loot.clear();
    CHECK(loot.empty());
}

TEST_CASE("loot: an id the content does not know keeps a readable row", "[m126][loot]") {
    LootSummary loot;
    loot.addItem("no_such_item", db());
    const std::vector<LootRow> rows = loot.rows();
    REQUIRE(rows.size() == 1);
    CHECK(rows[0].text() == "no_such_item");
    CHECK(rows[0].icon.empty());
}

// ------------------------------------------------------ learned skills -----

TEST_CASE("tag flow: tags fill a line and wrap to the next", "[m126][ui]") {
    // Three 60px tags with an 8px gap in 140px: two fit (128), the third wraps.
    const std::vector<ui::TagFlowLine> lines = ui::flowTags({60, 60, 60}, 8, 140);
    REQUIRE(lines.size() == 2);
    CHECK(lines[0].first == 0);
    CHECK(lines[0].count == 2);
    CHECK(lines[1].first == 2);
    CHECK(lines[1].count == 1);
    // Exactly full still fits; one pixel over does not.
    CHECK(ui::flowTags({60, 60}, 8, 128).size() == 1);
    CHECK(ui::flowTags({60, 60}, 8, 127).size() == 2);
    // A lone tag wider than the line keeps a line to itself, and the run goes on.
    const std::vector<ui::TagFlowLine> wide = ui::flowTags({300, 20, 20}, 8, 100);
    REQUIRE(wide.size() == 2);
    CHECK(wide[0].count == 1);
    CHECK(wide[1].first == 1);
    CHECK(wide[1].count == 2);
    CHECK(ui::flowTags({}, 8, 100).empty());
}

TEST_CASE("spoils: every learned skill carries its kind icon", "[m126][spoils]") {
    Party p;
    p.members.push_back(createCharacter(*db().findClass("cleric"), "Pell", 1));
    BattleSpoils s;
    s.xp = 5000;  // several levels, several learnset grants
    const SpoilsResult r = applySpoils(p, s, db());
    REQUIRE(r.levelUps.size() == 1);
    const LevelUpDiff& d = r.levelUps[0];
    REQUIRE_FALSE(d.newSkillNames.empty());
    REQUIRE(d.newSkillIcons.size() == d.newSkillNames.size());
    for (std::size_t i = 0; i < d.newSkillNames.size(); ++i) {
        const content::SkillDef* found = nullptr;
        for (const auto& [id, def] : db().skills()) {
            if (def.name == d.newSkillNames[i]) {
                found = &def;
            }
        }
        REQUIRE(found != nullptr);
        CHECK(d.newSkillIcons[i] == content::skillKindTextureId(found->kind));
        CHECK(d.newSkillIcons[i].rfind("ui.icon.skill.", 0) == 0);
    }
}

// ------------------------------------------- the Dragon's single pick ------

TEST_CASE("decision: a basic attack is one pick, even a sweeping one", "[m126][lore][chest]") {
    SpecialEncounter e;
    e.kind = SpecialKind::Chests;
    battle::Battle b =
        battle::buildBattle(makeParty({"dragon", "ranger", "mage", "cleric"}),
                            dungeon::EnemyTeam{}, db());
    const int first = appendPlaceholders(b, e);
    REQUIRE(b.units[0].attackHitsAll);  // the Dragon's swing sweeps every foe ...
    CHECK(b.hostileTargetCount(0, nullptr) == kSpecialPlaceholders);
    CHECK_FALSE(decisionActionIsAoe(b, 0, nullptr));  // ... yet here it is ONE pick
    CHECK_FALSE(decisionActionIsAoe(b, 1, nullptr));

    // A skill still answers to the battle's own targeting.
    int sweeps = 0;
    int singles = 0;
    for (const auto& [id, s] : db().skills()) {
        const bool aoe = decisionActionIsAoe(b, 1, &s);
        if (s.target == content::SkillTarget::AllEnemies) {
            CHECK(aoe);
            ++sweeps;
        } else {
            CHECK_FALSE(aoe);
            ++singles;
        }
    }
    CHECK(sweeps > 0);
    CHECK(singles > 0);

    // So the Dragon's swing at the paying chest pays, where a sweep wakes the Mimic.
    e.chests.roles = {ChestRole::Empty, ChestRole::Reward, ChestRole::Mimic};
    e.chests.rewardIsGold = true;
    SpecialEncounter swing = e;
    CHECK(resolveSpecial(swing, 1, decisionActionIsAoe(b, 0, nullptr)) ==
          SpecialResult::ChestGold);
    CHECK(swing.rewardGold == kChestGoldReward);
    const content::SkillDef* radiance = db().findSkill("radiance");
    REQUIRE(radiance != nullptr);
    SpecialEncounter sweep = e;
    CHECK(resolveSpecial(sweep, 1, decisionActionIsAoe(b, 1, radiance)) ==
          SpecialResult::MimicRevealed);

    // The uncontrolled path reads the same rule.
    b.units[0].uncontrolled = true;
    battle::EnemyChoice pick;
    pick.target = first + 2;
    const UncontrolledDecision d = uncontrolledDecisionFor(b, 0, pick, first, nullptr);
    CHECK(d.decides);
    CHECK(d.ordinal == 2);
    CHECK_FALSE(d.aoe);
}

TEST_CASE("decision: a Dragon can answer the Jester without being punished",
          "[m126][lore]") {
    SpecialEncounter e;
    e.kind = SpecialKind::Lore;
    e.lore.correctAnswer = 1;
    battle::Battle b = battle::buildBattle(makeParty({"dragon", "dragon", "dragon", "dragon"}),
                                           dungeon::EnemyTeam{}, db());
    appendPlaceholders(b, e);
    for (int i = 0; i < 4; ++i) {
        REQUIRE(b.units[static_cast<std::size_t>(i)].attackHitsAll);
        const bool aoe = decisionActionIsAoe(b, i, nullptr);
        SpecialEncounter right = e;
        CHECK(resolveSpecial(right, 2, aoe) == SpecialResult::LoreCorrect);
        SpecialEncounter wrong = e;
        CHECK(resolveSpecial(wrong, 1, aoe) == SpecialResult::LoreWrong);
        SpecialEncounter jester = e;
        CHECK(resolveSpecial(jester, 0, aoe) == SpecialResult::JesterPunished);
    }
}

// ----------------------------------------------------- guarded vaults ------

TEST_CASE("vault: generation v25 tags the walled-in guarded chest", "[m126][roomlayout]") {
    CHECK(dungeon::kGenerationVersion >= 25);
}

TEST_CASE("vault: the guard stands in front of a walled-in chest, the only way to it",
          "[m126][roomlayout]") {
    using dungeon::RoomLayout;
    int vaults = 0;
    int alcoves = 0;
    for (int i = 0; i < 60; ++i) {
        const std::uint64_t seed = static_cast<std::uint64_t>(i) * 104729u + 7u;
        for (const char* theme : {"ruined_keep", "crystal_mine", "hollow_forest"}) {
            const dungeon::Dungeon d = dungeon::generate(seed, 1 + i % 9, db(), theme);
            const std::vector<RoomLayout> layouts = dungeon::realizeAllRooms(d);
            for (std::size_t r = 0; r < layouts.size(); ++r) {
                const RoomLayout& l = layouts[r];
                const dungeon::Room& room = d.rooms[r];
                if (!room.chest.present) {
                    continue;
                }
                INFO("seed " << seed << " theme " << theme << " room " << r);
                REQUIRE(l.chest.valid());
                // The one door of a treasure dead-end, and its entry tile.
                dungeon::Dir doorDir = dungeon::Dir::North;
                int doors = 0;
                for (int di = 0; di < dungeon::kDirCount; ++di) {
                    if (room.hasDoor(static_cast<dungeon::Dir>(di))) {
                        if (doors == 0) {
                            doorDir = static_cast<dungeon::Dir>(di);
                        }
                        ++doors;
                    }
                }
                REQUIRE(doors == 1);
                const RoomLayout::Point entry = l.interiorGap(doorDir)[0];
                if (room.teamIndex < 0) {
                    // An unguarded chest stays open to the room.
                    ++alcoves;
                    CHECK_FALSE(l.guard.valid());
                    CHECK(reaches(l, entry, l.chest, false));
                    continue;
                }
                ++vaults;
                REQUIRE(l.guard.valid());
                // In front: one step from the chest, toward the door.
                CHECK(l.guard.x == l.chest.x + dungeon::dirDx(doorDir));
                CHECK(l.guard.y == l.chest.y + dungeon::dirDy(doorDir));
                // Walled in: the guard's tile is the chest's only open side.
                int open = 0;
                constexpr int dx[4] = {0, 1, 0, -1};
                constexpr int dy[4] = {-1, 0, 1, 0};
                for (int k = 0; k < 4; ++k) {
                    if (l.walkable(l.chest.x + dx[k], l.chest.y + dy[k])) {
                        ++open;
                    }
                }
                CHECK(open == 1);
                // Not passable: sealed while the guard stands, open once it falls.
                CHECK_FALSE(reaches(l, entry, l.chest, true));
                CHECK(reaches(l, entry, l.chest, false));
                // And the guard itself can be walked up to.
                bool faced = false;
                for (int k = 0; k < 4; ++k) {
                    const RoomLayout::Point n{l.guard.x + dx[k], l.guard.y + dy[k]};
                    if (!(n == l.chest) && reaches(l, entry, n, true)) {
                        faced = true;
                    }
                }
                CHECK(faced);
                CHECK(dungeon::validateLayout(d, static_cast<int>(r), l).empty());
            }
        }
    }
    CHECK(vaults >= 100);  // every dungeon guarantees one guarded chest
    CHECK(alcoves > 0);
}

TEST_CASE("vault: the validator rejects a way around the guard", "[m126][roomlayout]") {
    using dungeon::RoomLayout;
    const dungeon::Dungeon d = dungeon::generate(4242, 3, db(), "ruined_keep");
    const std::vector<RoomLayout> layouts = dungeon::realizeAllRooms(d);
    int checked = 0;
    for (std::size_t r = 0; r < layouts.size(); ++r) {
        if (!layouts[r].guard.valid()) {
            continue;
        }
        ++checked;
        REQUIRE(dungeon::validateLayout(d, static_cast<int>(r), layouts[r]).empty());
        // Knock out one flank wall: the chest can be reached past the guard.
        RoomLayout open = layouts[r];
        constexpr int dx[4] = {0, 1, 0, -1};
        constexpr int dy[4] = {-1, 0, 1, 0};
        bool knocked = false;
        for (int k = 0; k < 4 && !knocked; ++k) {
            const int x = open.chest.x + dx[k];
            const int y = open.chest.y + dy[k];
            const bool interior = x >= 1 && y >= 1 && x <= open.width - 2 && y <= open.height - 2;
            if (interior && !open.walkable(x, y)) {
                open.cells[static_cast<std::size_t>(y * open.width + x)] = RoomLayout::Cell::Floor;
                knocked = true;
            }
        }
        REQUIRE(knocked);
        CHECK_FALSE(dungeon::validateLayout(d, static_cast<int>(r), open).empty());
        // The old placement - the guard BESIDE the chest - is rejected too.
        RoomLayout beside = layouts[r];
        beside.guard = RoomLayout::Point{beside.chest.x + (beside.guard.x == beside.chest.x ? 1 : 0),
                                         beside.chest.y + (beside.guard.x == beside.chest.x ? 0 : 1)};
        CHECK_FALSE(dungeon::validateLayout(d, static_cast<int>(r), beside).empty());
    }
    CHECK(checked >= 1);
}
