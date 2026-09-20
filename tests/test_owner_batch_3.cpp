// M127 - owner batch 3: the party on the save slots (the summary's members and
// the shared victory hop), the curio icon ids and the treasure map's pieces.

#include <catch2/catch_test_macros.hpp>

#include <atomic>
#include <filesystem>
#include <set>
#include <string>

#include "assets/AssetManifest.hpp"
#include "content/ContentDatabase.hpp"
#include "content/ContentLoader.hpp"
#include "game/Curios.hpp"
#include "game/Party.hpp"
#include "game/TreasureMap.hpp"
#include "render/PartyHop.hpp"
#include "save/SaveSystem.hpp"

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

std::filesystem::path tempDir() {
    static std::atomic<int> counter{0};
    return std::filesystem::temp_directory_path() /
           ("cd_batch3_" + std::to_string(counter.fetch_add(1)));
}

}  // namespace

TEST_CASE("m127: a slot summary names the party, in order, and who has fallen",
          "[save][m127]") {
    const std::filesystem::path dir = tempDir();
    save::SaveSystem saves(db(), dir);
    Party p;
    resetForNewGame(p);
    for (const char* id : {"dragon", "knight", "mage", "cleric"}) {
        const content::ClassDef* cls = db().findClass(id);
        REQUIRE(cls != nullptr);
        p.members.push_back(createCharacter(*cls, id, 5));
    }
    p.members[2].hp = 0;  // the mage is down
    content::LoadReport rep;
    REQUIRE(saves.save(save::SaveSlot::Manual2, p, rep));

    const auto summary = saves.summary(save::SaveSlot::Manual2);
    REQUIRE(summary.has_value());
    REQUIRE(summary->members.size() == 4);
    CHECK(summary->members[0].classId == "dragon");
    CHECK(summary->members[1].classId == "knight");
    CHECK(summary->members[2].classId == "mage");
    CHECK(summary->members[3].classId == "cleric");
    CHECK_FALSE(summary->members[0].fallen);
    CHECK(summary->members[2].fallen);
    CHECK_FALSE(summary->members[3].fallen);
    CHECK(summary->partySize == 4);

    CHECK_FALSE(saves.summary(save::SaveSlot::Manual3).has_value());  // empty: no party
    std::error_code ec;
    std::filesystem::remove_all(dir, ec);
}

TEST_CASE("m127: the party hop lands on the ground and keeps its own beats", "[m127][render]") {
    for (int slot = 0; slot < 4; ++slot) {
        float peak = 0.0f;
        for (int i = 0; i < 2000; ++i) {
            const float t = static_cast<float>(i) * 0.01f;
            const float h = render::partyHop(slot, t);
            REQUIRE(h >= 0.0f);                          // never below the ground line
            REQUIRE(h <= render::kHopAmp[slot] + 0.001f);  // never above its own jump
            peak = h > peak ? h : peak;
        }
        CHECK(peak > render::kHopAmp[slot] * 0.95f);  // it really jumps
        // The save slots hop to the same beat, only smaller.
        const float t = 1.2345f;
        CHECK(render::partyHop(slot, t, 0.3f) <= render::partyHop(slot, t) * 0.3f + 0.001f);
        CHECK(render::partyHop(slot, t, 0.3f) >= render::partyHop(slot, t) * 0.3f - 0.001f);
    }
    // Nobody moves in step, a fifth member reuses the first beat, junk reads as slot 0.
    CHECK(render::partyHop(0, 0.5f) != render::partyHop(1, 0.5f));
    CHECK(render::partyHop(4, 0.5f) == render::partyHop(0, 0.5f));
    CHECK(render::partyHop(-3, 0.5f) == render::partyHop(0, 0.5f));
    CHECK(render::partyHop(2, 0.5f, 0.0f) == 0.0f);
}

TEST_CASE("m127: every curio has its own shipped icon, and the map its four pieces",
          "[lint][m127]") {
    assets::AssetManifest m;
    content::LoadReport report;
    REQUIRE(m.load(std::filesystem::path(CRYSTAL_TEST_ASSETS_DIR), report));
    const auto hasTexture = [&m](const std::string& id) {
        const assets::AssetEntry* e = m.find(id);
        return e != nullptr && e->type == assets::AssetType::Texture;
    };
    const std::filesystem::path root(CRYSTAL_TEST_ASSETS_DIR);
    std::set<std::string> ids;
    for (const CurioDef& c : kCurios) {
        const std::string tex = curioIconTextureId(c);
        INFO(tex);
        CHECK(tex == std::string("ui.icon.curio.") + c.id);
        CHECK(hasTexture(tex));
        CHECK(ids.insert(tex).second);  // its OWN icon: no two curios share one
        if (const assets::AssetEntry* e = m.find(tex)) {
            CHECK(std::filesystem::exists(root / e->path));
        }
    }
    CHECK(static_cast<int>(ids.size()) == kCurioCount);
    for (const char* piece : kMapPieceTextureIds) {
        INFO(piece);
        CHECK(hasTexture(piece));
        if (const assets::AssetEntry* e = m.find(piece)) {
            CHECK(std::filesystem::exists(root / e->path));
        }
    }
    CHECK(hasTexture(kMapPiecePropId));
}
