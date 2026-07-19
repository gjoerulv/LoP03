#include <catch2/catch_test_macros.hpp>

#include <fstream>

#include "assets/AssetManifest.hpp"
#include "content/LoadReport.hpp"

using cd::assets::AssetManifest;
using cd::assets::AssetType;
using cd::content::LoadReport;

TEST_CASE("manifest: valid entries parse with metadata and defaults") {
    AssetManifest m;
    LoadReport report;
    const std::string text = R"({
        "version": 1,
        "assets": [
            {"id": "sfx.ui.confirm", "type": "sfx", "path": "audio/sfx/confirm.wav", "volume": 0.9},
            {"id": "music.town", "type": "music", "path": "audio/music/town.ogg", "loop": false},
            {"id": "font.ui.body", "type": "font", "path": "fonts/body.ttf", "size": 12},
            {"id": "ui.frame.default", "type": "texture", "path": "textures/ui/frame.png", "filter": "bilinear"}
        ]
    })";
    REQUIRE(m.parseText(text, report));
    CHECK(report.errorCount() == 0);
    REQUIRE(m.size() == 4);

    const auto* sfx = m.find("sfx.ui.confirm");
    REQUIRE(sfx != nullptr);
    CHECK(sfx->type == AssetType::Sfx);
    CHECK(sfx->volume == 0.9f);

    const auto* music = m.find("music.town");
    REQUIRE(music != nullptr);
    CHECK_FALSE(music->loop);
    CHECK(music->volume == 1.0f);  // default

    const auto* font = m.find("font.ui.body");
    REQUIRE(font != nullptr);
    CHECK(font->fontSize == 12);

    const auto* tex = m.find("ui.frame.default");
    REQUIRE(tex != nullptr);
    CHECK(tex->filter == "bilinear");
    CHECK(m.find("nope") == nullptr);
}

TEST_CASE("manifest: bad entries are skipped, good ones survive") {
    AssetManifest m;
    LoadReport report;
    const std::string text = R"({
        "version": 1,
        "assets": [
            {"id": "a.good", "type": "sfx", "path": "a.wav"},
            {"id": "b.dup", "type": "sfx", "path": "b.wav"},
            {"id": "b.dup", "type": "sfx", "path": "b2.wav"},
            {"id": "c.badtype", "type": "hologram", "path": "c.bin"},
            {"id": "d.unsafe", "type": "sfx", "path": "../../secrets.wav"},
            {"id": "e.absolute", "type": "sfx", "path": "C:/windows/x.wav"},
            "not an object",
            {"type": "sfx", "path": "missing_id.wav"}
        ]
    })";
    REQUIRE(m.parseText(text, report));
    CHECK(report.errorCount() > 0);
    CHECK(m.size() == 2);  // a.good + first b.dup
    CHECK(m.find("a.good") != nullptr);
    CHECK(m.find("b.dup") != nullptr);
    CHECK(m.find("c.badtype") == nullptr);
    CHECK(m.find("d.unsafe") == nullptr);
    CHECK(m.find("e.absolute") == nullptr);
}

TEST_CASE("manifest: malformed json and wrong version are unusable") {
    AssetManifest m;
    LoadReport report;
    CHECK_FALSE(m.parseText("{oops", report));
    CHECK_FALSE(m.parseText(R"({"version": 99, "assets": []})", report));
    CHECK(report.errorCount() > 0);
}

TEST_CASE("manifest: empty catalog and unknown fields are fine") {
    AssetManifest m;
    LoadReport report;
    CHECK(m.parseText(R"({"version": 1})", report));
    CHECK(m.empty());
    CHECK(m.parseText(R"({"version": 1, "assets": [], "futureBlock": {"x": 1}})", report));
    CHECK(report.errorCount() == 0);
}

TEST_CASE("manifest: volume out of range is clamped and reported") {
    AssetManifest m;
    LoadReport report;
    const std::string text = R"({
        "version": 1,
        "assets": [{"id": "s.loud", "type": "sfx", "path": "s.wav", "volume": 4.0}]
    })";
    REQUIRE(m.parseText(text, report));
    CHECK(report.errorCount() > 0);
    CHECK(m.find("s.loud")->volume == 1.0f);
}

TEST_CASE("manifest: load() drops entries whose files are missing") {
    namespace fs = std::filesystem;
    const fs::path root = fs::temp_directory_path() / "cd_test_assets";
    std::error_code ec;
    fs::remove_all(root, ec);
    fs::create_directories(root / "audio", ec);
    {
        std::ofstream wav(root / "audio" / "real.wav", std::ios::binary);
        wav << "RIFFfake";
    }
    {
        std::ofstream manifest(root / "manifest.json", std::ios::binary);
        manifest << R"({"version": 1, "assets": [
            {"id": "s.real", "type": "sfx", "path": "audio/real.wav"},
            {"id": "s.ghost", "type": "sfx", "path": "audio/ghost.wav"}
        ]})";
    }

    AssetManifest m;
    LoadReport report;
    REQUIRE(m.load(root, report));
    CHECK(m.size() == 1);
    CHECK(m.find("s.real") != nullptr);
    CHECK(m.find("s.ghost") == nullptr);
    CHECK(report.errorCount() > 0);
    fs::remove_all(root, ec);
}

TEST_CASE("manifest: missing manifest.json is a valid empty catalog") {
    namespace fs = std::filesystem;
    AssetManifest m;
    LoadReport report;
    REQUIRE(m.load(fs::temp_directory_path() / "cd_test_assets_none", report));
    CHECK(m.empty());
    CHECK(report.errorCount() == 0);
}

#ifdef CRYSTAL_TEST_ASSETS_DIR
TEST_CASE("manifest: the shipped assets/manifest.json validates with zero errors") {
    AssetManifest m;
    LoadReport report;
    REQUIRE(m.load(CRYSTAL_TEST_ASSETS_DIR, report));
    for (const auto& e : report.errors()) {
        UNSCOPED_INFO(e.source + ": " + e.context + ": " + e.message);
    }
    CHECK(report.errorCount() == 0);
}
#endif
