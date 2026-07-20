#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>

#include "assets/AssetManifest.hpp"
#include "content/LoadReport.hpp"
#include "render/Animation.hpp"

using cd::assets::AssetEntry;
using cd::assets::AssetManifest;
using cd::assets::AssetType;
using cd::content::LoadReport;

namespace {

AssetEntry makeAnim(int frameCount, float frameTime, bool loop, int row = 0) {
    AssetEntry e;
    e.type = AssetType::Animation;
    e.frameCount = frameCount;
    e.frameWidth = 12;
    e.frameHeight = 12;
    e.frameTime = frameTime;
    e.loop = loop;
    e.row = row;
    return e;
}

}  // namespace

TEST_CASE("manifest v2: animation entries parse and cross-reference", "[animation]") {
    AssetManifest m;
    LoadReport report;
    const std::string text = R"({
        "version": 2,
        "assets": [
            {"id": "actor.player.walk", "type": "texture", "path": "textures/actors/player_walk.png"},
            {"id": "anim.player.walk.down", "type": "animation", "texture": "actor.player.walk",
             "row": 0, "frameCount": 3, "frameWidth": 12, "frameHeight": 12, "frameTime": 0.15},
            {"id": "anim.player.walk.up", "type": "animation", "texture": "actor.player.walk",
             "row": 1, "frameCount": 3, "frameWidth": 12, "frameHeight": 12,
             "frameTime": 0.15, "loop": false}
        ]
    })";
    REQUIRE(m.parseText(text, report));
    CHECK(report.errorCount() == 0);
    REQUIRE(m.size() == 3);

    const AssetEntry* anim = m.find("anim.player.walk.down");
    REQUIRE(anim != nullptr);
    CHECK(anim->type == AssetType::Animation);
    CHECK(anim->texture == "actor.player.walk");
    CHECK(anim->row == 0);
    CHECK(anim->frameCount == 3);
    CHECK(anim->frameWidth == 12);
    CHECK(anim->frameHeight == 12);
    CHECK(anim->frameTime == 0.15f);
    CHECK(anim->loop);

    const AssetEntry* up = m.find("anim.player.walk.up");
    REQUIRE(up != nullptr);
    CHECK(up->row == 1);
    CHECK_FALSE(up->loop);
}

TEST_CASE("manifest v2: invalid animations are dropped, textures survive", "[animation]") {
    AssetManifest m;
    LoadReport report;
    const std::string text = R"({
        "version": 2,
        "assets": [
            {"id": "tex.a", "type": "texture", "path": "a.png"},
            {"id": "anim.dangling", "type": "animation", "texture": "tex.missing",
             "frameCount": 2, "frameWidth": 8, "frameHeight": 8},
            {"id": "anim.badframes", "type": "animation", "texture": "tex.a",
             "frameCount": 0, "frameWidth": 8, "frameHeight": 8},
            {"id": "anim.badtime", "type": "animation", "texture": "tex.a",
             "frameCount": 2, "frameWidth": 8, "frameHeight": 8, "frameTime": 0.0},
            {"id": "anim.notexture", "type": "animation",
             "frameCount": 2, "frameWidth": 8, "frameHeight": 8},
            {"id": "anim.good", "type": "animation", "texture": "tex.a",
             "frameCount": 2, "frameWidth": 8, "frameHeight": 8}
        ]
    })";
    REQUIRE(m.parseText(text, report));
    CHECK(report.errorCount() >= 4);
    CHECK(m.size() == 2);
    CHECK(m.find("tex.a") != nullptr);
    CHECK(m.find("anim.good") != nullptr);
    CHECK(m.find("anim.dangling") == nullptr);
    CHECK(m.find("anim.badframes") == nullptr);
    CHECK(m.find("anim.badtime") == nullptr);
    CHECK(m.find("anim.notexture") == nullptr);
}

TEST_CASE("manifest v2: version 1 files still load", "[animation]") {
    AssetManifest m;
    LoadReport report;
    const std::string text = R"({
        "version": 1,
        "assets": [{"id": "tex.a", "type": "texture", "path": "a.png"}]
    })";
    REQUIRE(m.parseText(text, report));
    CHECK(report.errorCount() == 0);
    CHECK(m.size() == 1);
}

TEST_CASE("animation: looping frame selection wraps", "[animation]") {
    const AssetEntry anim = makeAnim(3, 0.1f, /*loop=*/true);
    CHECK(cd::render::frameAt(anim, -1.0f) == 0);
    CHECK(cd::render::frameAt(anim, 0.0f) == 0);
    CHECK(cd::render::frameAt(anim, 0.05f) == 0);
    CHECK(cd::render::frameAt(anim, 0.15f) == 1);
    CHECK(cd::render::frameAt(anim, 0.25f) == 2);
    CHECK(cd::render::frameAt(anim, 0.35f) == 0);  // wrapped
    CHECK(cd::render::frameAt(anim, 30.05f) == 0);  // 300 full frames later, wrapped to 0
}

TEST_CASE("animation: non-looping holds the last frame", "[animation]") {
    const AssetEntry anim = makeAnim(3, 0.1f, /*loop=*/false);
    CHECK(cd::render::frameAt(anim, 0.25f) == 2);
    CHECK(cd::render::frameAt(anim, 0.35f) == 2);  // held
    CHECK(cd::render::frameAt(anim, 99.0f) == 2);
}

TEST_CASE("animation: degenerate metadata yields frame 0", "[animation]") {
    CHECK(cd::render::frameAt(makeAnim(1, 0.1f, true), 5.0f) == 0);
    CHECK(cd::render::frameAt(makeAnim(3, 0.0f, true), 5.0f) == 0);
}

TEST_CASE("animation: frame rectangles walk the strip row", "[animation]") {
    const AssetEntry anim = makeAnim(3, 0.1f, true, /*row=*/2);
    const cd::render::FrameRect r0 = cd::render::frameRect(anim, 0);
    CHECK(r0.x == 0);
    CHECK(r0.y == 24);
    CHECK(r0.w == 12);
    CHECK(r0.h == 12);
    const cd::render::FrameRect r2 = cd::render::frameRect(anim, 2);
    CHECK(r2.x == 24);
    CHECK(r2.y == 24);
    const cd::render::FrameRect clamped = cd::render::frameRect(anim, 99);
    CHECK(clamped.x == 24);
    CHECK(cd::render::frameRect(anim, -5).x == 0);
}

#ifdef CRYSTAL_TEST_ASSETS_DIR

namespace {

// Minimal PNG header read: IHDR width/height are big-endian at bytes 16..23.
bool pngDimensions(const std::filesystem::path& file, int& w, int& h) {
    std::ifstream in(file, std::ios::binary);
    if (!in) {
        return false;
    }
    unsigned char header[24] = {};
    in.read(reinterpret_cast<char*>(header), sizeof(header));
    if (!in || header[12] != 'I' || header[13] != 'H' || header[14] != 'D' ||
        header[15] != 'R') {
        return false;
    }
    w = (header[16] << 24) | (header[17] << 16) | (header[18] << 8) | header[19];
    h = (header[20] << 24) | (header[21] << 16) | (header[22] << 8) | header[23];
    return true;
}

}  // namespace

TEST_CASE("shipped manifest: every animation fits inside its strip texture", "[animation]") {
    const std::filesystem::path root(CRYSTAL_TEST_ASSETS_DIR);
    AssetManifest m;
    LoadReport report;
    REQUIRE(m.load(root, report));

    int animations = 0;
    for (const AssetEntry& entry : m.entries()) {
        if (entry.type != AssetType::Animation) {
            continue;
        }
        ++animations;
        const AssetEntry* tex = m.find(entry.texture);
        REQUIRE(tex != nullptr);  // guaranteed by dropDanglingAnimations
        int w = 0;
        int h = 0;
        INFO(entry.id << " -> " << tex->path);
        REQUIRE(pngDimensions(root / tex->path, w, h));
        CHECK(entry.frameCount * entry.frameWidth <= w);
        CHECK((entry.row + 1) * entry.frameHeight <= h);
    }
    INFO("shipped animations found: " << animations);
    CHECK(animations >= 5);  // player walk x4 + facing brackets (M17)
}

#endif  // CRYSTAL_TEST_ASSETS_DIR
