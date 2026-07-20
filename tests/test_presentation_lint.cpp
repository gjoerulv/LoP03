#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <string>
#include <vector>

#include "assets/AssetManifest.hpp"
#include "content/ContentDatabase.hpp"
#include "content/ContentLoader.hpp"
#include "content/LoadReport.hpp"
#include "dungeon/DungeonGenerator.hpp"
#include "dungeon/RoomLayout.hpp"
#include "ui/TextLayout.hpp"

// M23 presentation-content lint: every logical asset id the render code
// derives by convention must resolve in the shipped manifest, and every
// shipped description must fit its 2-line detail region. These are the
// checks that make a missing required asset a FAILING result instead of a
// silent fallback at runtime.

using cd::assets::AssetManifest;
using cd::content::ContentDatabase;
using cd::content::LoadReport;

namespace {

ContentDatabase loadShippedContent() {
    ContentDatabase db;
    LoadReport report;
    REQUIRE(cd::content::loadAll(std::filesystem::path(CRYSTAL_TEST_DATA_DIR), db, report));
    return db;
}

AssetManifest loadShippedManifest() {
    AssetManifest m;
    LoadReport report;
    REQUIRE(m.load(std::filesystem::path(CRYSTAL_TEST_ASSETS_DIR), report));
    return m;
}

bool hasTexture(const AssetManifest& m, const std::string& id) {
    const cd::assets::AssetEntry* e = m.find(id);
    return e != nullptr && e->type == cd::assets::AssetType::Texture;
}

}  // namespace

TEST_CASE("lint: every convention-derived texture id resolves in the manifest", "[lint]") {
    const ContentDatabase db = loadShippedContent();
    const AssetManifest m = loadShippedManifest();

    // Per-theme tiles (DungeonState: tiles.<themeId>.{floor,wall,door}).
    for (const auto& [id, theme] : db.themes()) {
        (void)theme;
        for (const char* suffix : {"floor", "wall", "door"}) {
            const std::string tex = "tiles." + id + "." + suffix;
            INFO(tex);
            CHECK(hasTexture(m, tex));
        }
    }
    // Battle actors (BattleState: actor.<classId>.battle) for every class.
    for (const auto& [id, cls] : db.classes()) {
        (void)cls;
        const std::string tex = "actor." + id + ".battle";
        INFO(tex);
        CHECK(hasTexture(m, tex));
    }
    // Tier fallbacks every enemy/boss resolves to when it has no own art.
    for (const char* id : {"enemy.normal.battle", "enemy.elite.battle", "boss.generic.battle",
                           "marker.enemy.normal", "marker.enemy.elite", "marker.enemy.boss",
                           "prop.chest", "prop.gate_marker", "prop.boss_marker",
                           "prop.event.shrine", "prop.event.spring", "prop.event.merchant",
                           "prop.event.totem", "prop.event.omen", "actor.player.overworld",
                           "actor.player.walk", "ui.frame.default"}) {
        INFO(id);
        CHECK(hasTexture(m, id));
    }
}

TEST_CASE("lint: names are present and descriptions fit the 2-line detail region",
          "[lint]") {
    const ContentDatabase db = loadShippedContent();
    // Conservative measure (7px/char at font 10 — overestimates the raylib
    // default font), against the shop/battle detail region: width 386
    // (426 - 2*20), at most 2 wrapped lines.
    const cd::ui::TextMeasure measure = [](const std::string& text, int fontSize) {
        return static_cast<int>(text.size()) * (fontSize * 7) / 10;
    };
    constexpr int kDetailW = 386;
    const auto fitsDetail = [&](const std::string& text) {
        return cd::ui::wrapText(text, kDetailW, 10, measure).size() <= 2;
    };

    for (const auto& [id, def] : db.items()) {
        INFO("item " << id);
        CHECK(!def.name.empty());
        CHECK(fitsDetail(def.description));
    }
    for (const auto& [id, def] : db.skills()) {
        INFO("skill " << id);
        CHECK(!def.name.empty());
    }
    for (const auto& [id, def] : db.enemies()) {
        INFO("enemy " << id);
        CHECK(!def.name.empty());
        CHECK(def.name.size() <= 16);  // battle-row name budget at font 10
    }
    for (const auto& [id, def] : db.bosses()) {
        INFO("boss " << id);
        CHECK(!def.name.empty());
        // Telegraphs render wrapped with maxLines 2 at width 394 (w - 32).
        CHECK(cd::ui::wrapText(def.telegraph, 394, 10, measure).size() <= 2);
    }
    for (const auto& [id, def] : db.classes()) {
        INFO("class " << id);
        CHECK(!def.name.empty());
    }
}

TEST_CASE("lint: mass room generation holds every layout invariant", "[lint][mass]") {
    const ContentDatabase db = loadShippedContent();
    const char* themes[] = {"ruined_keep", "crystal_mine", "hollow_forest"};
    int rooms = 0;
    for (std::uint64_t seed = 1; seed <= 40; ++seed) {
        for (int depth : {1, 3, 5, 8, 10}) {
            const cd::dungeon::Dungeon d = cd::dungeon::generate(
                seed * 7919u, depth, db, themes[seed % 3]);
            const std::vector<cd::dungeon::RoomLayout> layouts =
                cd::dungeon::realizeAllRooms(d);
            for (std::size_t i = 0; i < layouts.size(); ++i) {
                ++rooms;
                const std::vector<std::string> problems =
                    cd::dungeon::validateLayout(d, static_cast<int>(i), layouts[i]);
                INFO("seed " << seed * 7919u << " depth " << depth << " room " << i << ": "
                             << (problems.empty() ? "ok" : problems.front()));
                REQUIRE(problems.empty());
            }
        }
    }
    INFO("rooms validated: " << rooms);
    CHECK(rooms >= 2000);  // thousands-of-rooms bar (M23)
}
