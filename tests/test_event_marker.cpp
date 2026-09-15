// M118 - event markers: every event kind owns a distinct 12x12 sprite id,
// the seven shipped ids keep their names, the fifteen new ones follow the
// flavor id, and the shipped manifest carries them all.

#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <set>
#include <string>

#include "assets/AssetManifest.hpp"
#include "content/LoadReport.hpp"
#include "dungeon/ThemeEvents.hpp"

using cd::dungeon::RoomEventKind;

TEST_CASE("event markers: one distinct sprite id per kind, all in the shipped manifest",
          "[events][marker][lint]") {
    static_assert(cd::dungeon::kAllRoomEventKinds.size() == 22);
    static_assert(static_cast<int>(RoomEventKind::GoosyFlock) == 22,
                  "a new RoomEventKind needs a marker: add it to kAllRoomEventKinds and "
                  "eventMarkerSpriteId, then draw it");
    std::set<std::string> ids;
    for (const RoomEventKind k : cd::dungeon::kAllRoomEventKinds) {
        const char* id = cd::dungeon::eventMarkerSpriteId(k);
        REQUIRE(id != nullptr);
        INFO(id);
        CHECK(std::string(id).rfind("prop.event.", 0) == 0);
        ids.insert(id);
    }
    CHECK(ids.size() == cd::dungeon::kAllRoomEventKinds.size());
    CHECK(cd::dungeon::eventMarkerSpriteId(RoomEventKind::None) == nullptr);

    cd::assets::AssetManifest m;
    cd::content::LoadReport report;
    REQUIRE(m.load(std::filesystem::path(CRYSTAL_TEST_ASSETS_DIR), report));
    for (const std::string& id : ids) {
        INFO(id);
        const cd::assets::AssetEntry* e = m.find(id);
        REQUIRE(e != nullptr);
        CHECK(e->type == cd::assets::AssetType::Texture);
    }
}

TEST_CASE("event markers: the shipped seven keep their names, the rest follow the flavor id",
          "[events][marker]") {
    using cd::dungeon::eventFlavorId;
    using cd::dungeon::eventMarkerSpriteId;
    CHECK(std::string(eventMarkerSpriteId(RoomEventKind::Shrine)) == "prop.event.shrine");
    CHECK(std::string(eventMarkerSpriteId(RoomEventKind::HealingSpring)) == "prop.event.spring");
    CHECK(std::string(eventMarkerSpriteId(RoomEventKind::Merchant)) == "prop.event.merchant");
    CHECK(std::string(eventMarkerSpriteId(RoomEventKind::EliteChallenge)) == "prop.event.totem");
    CHECK(std::string(eventMarkerSpriteId(RoomEventKind::ScoreWager)) == "prop.event.omen");
    CHECK(std::string(eventMarkerSpriteId(RoomEventKind::RestToken)) == "prop.event.rest");
    CHECK(std::string(eventMarkerSpriteId(RoomEventKind::RoyalRelic)) == "prop.event.relic");
    const RoomEventKind legacy[] = {RoomEventKind::Shrine,     RoomEventKind::HealingSpring,
                                    RoomEventKind::Merchant,   RoomEventKind::EliteChallenge,
                                    RoomEventKind::ScoreWager, RoomEventKind::RestToken,
                                    RoomEventKind::RoyalRelic};
    int followed = 0;
    for (const RoomEventKind k : cd::dungeon::kAllRoomEventKinds) {
        bool isLegacy = false;
        for (const RoomEventKind l : legacy) {
            isLegacy = isLegacy || l == k;
        }
        if (isLegacy) {
            continue;
        }
        INFO(eventFlavorId(k));
        CHECK(std::string(eventMarkerSpriteId(k)) ==
              std::string("prop.event.") + eventFlavorId(k));
        ++followed;
    }
    CHECK(followed == 15);
}
