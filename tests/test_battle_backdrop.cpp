#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <set>
#include <string>
#include <vector>

#include "assets/AssetManifest.hpp"
#include "content/LoadReport.hpp"
#include "render/BattleBackdrop.hpp"

// M56 — per-theme battle backdrops; M119 (corrected 2026-09-16) — the grounded
// stage. Pure geometry only; drawing is validated by the capture tool. These
// enforce the layer contract mechanically: silhouettes out of the action field
// and off every footprint, a ground plane under every formation row, the
// painted PNGs at the band's exact size.

using namespace cd::render;

namespace {
// The battle band as BattleState builds it: y 24 to two pixels above the
// command panel (240 - 60 - 4 - 2), i.e. (0, 24, 426, 150).
constexpr BackdropBand kBand{0, 24, 426, 150};

// The formation's footprints, from BattleState::render / drawUnit: a row's
// sprite anchors bottom-centre at row + 16 (24 px sprites, 36 px bosses);
// party rows are 36 + 34k at x 302 (the sprite 310..334, the meters 302..342
// down to row + 27); enemy rows hang from the base line 36 (20 with five
// foes) at battle_ui::kEnemyRowPitch, at x 36 (a boss sprite spans 38..74,
// the meter 36..76 down to row + 22).
constexpr int kPartyRows[4] = {36, 70, 104, 138};
constexpr int kEnemyRowsFive[5] = {20, 54, 88, 122, 156};

struct Box {
    int x0 = 0;
    int y0 = 0;
    int x1 = 0;
    int y1 = 0;
};

bool overlaps(const BackdropRect& r, const Box& b) {
    return r.x < b.x1 && r.x + r.w > b.x0 && r.y < b.y1 && r.y + r.h > b.y0;
}

long coverage(const std::vector<BackdropRect>& rects) {
    long area = 0;
    for (const BackdropRect& r : rects) {
        area += static_cast<long>(r.w) * r.h;
    }
    return area;
}

// Every sprite canvas and meter the formations can place on the band.
std::vector<Box> footprints() {
    std::vector<Box> boxes;
    for (int row : kPartyRows) {
        boxes.push_back({310, row - 8, 334, row + 16});   // the 24 px sprite
        boxes.push_back({302, row + 17, 342, row + 27});  // HP + MP meters
    }
    for (int base : {36, 20}) {
        for (int slot = 0; slot < 5; ++slot) {
            const int row = base + slot * 34;
            boxes.push_back({38, row - 20, 74, row + 16});  // up to a 36 px boss canvas
            boxes.push_back({36, row + 17, 76, row + 22});  // the enemy meter
        }
    }
    return boxes;
}

bool pngSize(const std::filesystem::path& file, std::uint32_t& w, std::uint32_t& h) {
    std::ifstream in(file, std::ios::binary);
    unsigned char head[24] = {};
    if (!in.read(reinterpret_cast<char*>(head), sizeof(head))) {
        return false;
    }
    static const unsigned char sig[8] = {0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A};
    for (int i = 0; i < 8; ++i) {
        if (head[i] != sig[i]) return false;
    }
    // IHDR: width and height are big-endian at offsets 16 and 20.
    const auto be = [&](int at) {
        return (static_cast<std::uint32_t>(head[at]) << 24) |
               (static_cast<std::uint32_t>(head[at + 1]) << 16) |
               (static_cast<std::uint32_t>(head[at + 2]) << 8) |
               static_cast<std::uint32_t>(head[at + 3]);
    };
    w = be(16);
    h = be(20);
    return true;
}

constexpr BackdropStage kThemed[] = {BackdropStage::Keep, BackdropStage::Mine,
                                     BackdropStage::Forest, BackdropStage::Castle,
                                     BackdropStage::Goosy};
}  // namespace

TEST_CASE("backdrop: theme id maps to a stage, unknown is Plain", "[backdrop]") {
    CHECK(stageForTheme("ruined_keep") == BackdropStage::Keep);
    CHECK(stageForTheme("crystal_mine") == BackdropStage::Mine);
    CHECK(stageForTheme("hollow_forest") == BackdropStage::Forest);
    CHECK(stageForTheme("goosy_gauntlet") == BackdropStage::Goosy);
    CHECK(stageForTheme("") == BackdropStage::Plain);
    CHECK(stageForTheme("no_such_theme") == BackdropStage::Plain);
}

TEST_CASE("backdrop: Plain draws no silhouettes", "[backdrop]") {
    CHECK(buildBackdrop(BackdropStage::Plain, kBand, 0, true).empty());
    CHECK(buildBackdrop(BackdropStage::Plain, kBand, 1, false).empty());
}

TEST_CASE("backdrop: the band reaches the command panel and every footprint stands on the ground plane",
          "[backdrop]") {
    // BattleState: kPanelH 60, the panel at h - kPanelH - 4, the band ending
    // two pixels above it.
    CHECK(kBand.y + kBand.h == 240 - 60 - 4 - 2);
    const int horizon = kBand.y + kHorizonPx;
    const int bottom = kBand.y + kBand.h;
    for (int row : kPartyRows) {
        INFO("party row " << row);
        CHECK(row + 16 >= horizon);  // the feet on or below the horizon
        CHECK(row + 16 < bottom);    // and over the environment
        CHECK(row + 27 < bottom);    // the MP meter's bottom too
    }
    for (int row : kEnemyRowsFive) {
        INFO("five-foe enemy row " << row);
        CHECK(row + 16 >= horizon);
        CHECK(row + 16 < bottom);
    }
    // The four-foe base line (36) seats the enemies on the party's own rows.
    for (int row : kPartyRows) {
        CHECK(row + 16 >= horizon);
    }
}

TEST_CASE("backdrop: the ground plane is a far strip, a horizon, one ground mass and growing seams",
          "[backdrop]") {
    const auto plane = buildGroundPlane(kBand);
    int far = 0;
    int horizon = 0;
    int ground = 0;
    std::vector<int> seams;
    for (const GroundRect& r : plane) {
        CHECK(r.x == kBand.x);  // every piece spans the band's width
        CHECK(r.w == kBand.w);
        CHECK(r.h >= 1);
        switch (r.role) {
            case GroundRole::Far:
                ++far;
                CHECK(r.y == kBand.y);
                CHECK(r.h == kHorizonPx);
                break;
            case GroundRole::Ground:
                ++ground;
                CHECK(r.y == kBand.y + kHorizonPx);
                CHECK(r.y + r.h == kBand.y + kBand.h);
                break;
            case GroundRole::Horizon:
                ++horizon;
                CHECK(r.y == kBand.y + kHorizonPx);
                CHECK(r.h == 1);
                break;
            case GroundRole::Seam:
                CHECK(r.h == 1);
                seams.push_back(r.y);
                break;
        }
    }
    CHECK(far == 1);
    CHECK(ground == 1);
    CHECK(horizon == 1);
    REQUIRE(seams.size() >= 3);
    int lastGap = 0;
    for (std::size_t i = 0; i < seams.size(); ++i) {
        INFO("seam " << i << " at y " << seams[i]);
        CHECK(seams[i] > kBand.y + kHorizonPx);                  // below the horizon line
        CHECK(seams[i] < kBand.y + kBand.h - kNearStripPx);      // above the near strip
        const int gap = seams[i] - (i == 0 ? kBand.y + kHorizonPx : seams[i - 1]);
        CHECK(gap > lastGap);  // the spacing grows toward the near edge
        lastGap = gap;
    }
    // Deterministic.
    const auto again = buildGroundPlane(kBand);
    REQUIRE(again.size() == plane.size());
    for (std::size_t i = 0; i < plane.size(); ++i) {
        CHECK(again[i].y == plane[i].y);
        CHECK(again[i].role == plane[i].role);
    }
}

TEST_CASE("backdrop: the action field and the allowed silhouette zones", "[backdrop]") {
    const BackdropBand field = actionField(kBand);
    CHECK(field.x == 30);
    CHECK(field.y == 32);
    CHECK(field.w == 378);
    CHECK(field.h == 132);
    // Every party foot (x 322) and every four-foe enemy foot (x 56) lies inside the field.
    for (int row : kPartyRows) {
        CHECK(row + 16 >= field.y);
        CHECK(row + 16 < field.y + field.h);
    }
    CHECK(322 < field.x + field.w);
    CHECK(56 >= field.x);
    // Zone membership.
    CHECK(silhouetteAllowed({100, 24, 20, 8, BackdropRole::Ink}, kBand));    // skyline strip
    CHECK(silhouetteAllowed({0, 60, 30, 100, BackdropRole::Ink}, kBand));    // left margin
    CHECK(silhouetteAllowed({408, 60, 18, 100, BackdropRole::Ink}, kBand));  // right margin
    CHECK(silhouetteAllowed({90, 164, 200, 10, BackdropRole::Ink}, kBand));  // near centre
    CHECK_FALSE(silhouetteAllowed({100, 24, 20, 9, BackdropRole::Ink}, kBand));   // one row into the field
    CHECK_FALSE(silhouetteAllowed({0, 60, 31, 100, BackdropRole::Ink}, kBand));   // one column into the field
    CHECK_FALSE(silhouetteAllowed({407, 60, 19, 100, BackdropRole::Ink}, kBand)); // one column into the field
    CHECK_FALSE(silhouetteAllowed({89, 164, 200, 10, BackdropRole::Ink}, kBand)); // into the enemy column
    CHECK_FALSE(silhouetteAllowed({90, 163, 200, 11, BackdropRole::Ink}, kBand)); // one row above the near strip
    CHECK_FALSE(silhouetteAllowed({200, 100, 20, 20, BackdropRole::Ink}, kBand)); // mid-field
    CHECK_FALSE(silhouetteAllowed({0, 20, 20, 10, BackdropRole::Ink}, kBand));    // above the band
    CHECK_FALSE(silhouetteAllowed({0, 60, 0, 10, BackdropRole::Ink}, kBand));     // empty
}

TEST_CASE("backdrop: every silhouette stays out of the action field, off every footprint, and subdued",
          "[backdrop]") {
    const BackdropBand field = actionField(kBand);
    const Box fieldBox{field.x, field.y, field.x + field.w, field.y + field.h};
    const std::vector<Box> feet = footprints();
    const long bandArea = static_cast<long>(kBand.w) * kBand.h;
    const long maxCoverage = static_cast<long>(kBand.w * kBand.h * kMaxCoverageFrac);

    for (BackdropStage stage : kThemed) {
        for (int phase = 0; phase <= 1; ++phase) {
            for (bool accents : {true, false}) {
                const auto rects = buildBackdrop(stage, kBand, phase, accents);
                INFO("stage=" << static_cast<int>(stage) << " phase=" << phase
                              << " accents=" << accents);
                REQUIRE_FALSE(rects.empty());
                for (const BackdropRect& r : rects) {
                    INFO("rect " << r.x << "," << r.y << " " << r.w << "x" << r.h);
                    CHECK(silhouetteAllowed(r, kBand));
                    CHECK_FALSE(overlaps(r, fieldBox));
                    for (const Box& b : feet) {
                        CHECK_FALSE(overlaps(r, b));
                    }
                    if (!accents) {
                        CHECK_FALSE(isAccentRole(r.role));
                    }
                }
                CHECK(coverage(rects) <= maxCoverage);
                CHECK(coverage(rects) < bandArea);  // sanity
            }
        }
    }
}

TEST_CASE("backdrop: high contrast drops accents but keeps the silhouettes", "[backdrop]") {
    // Mine (crystal glint), Castle (gold pips + keyline) and Goosy (ripple
    // glint) carry accents; Keep and Forest carry none. accents=false must
    // never lose the silhouette bodies.
    for (BackdropStage stage : {BackdropStage::Mine, BackdropStage::Castle,
                                BackdropStage::Goosy}) {
        const auto withAccents = buildBackdrop(stage, kBand, 0, true);
        const auto noAccents = buildBackdrop(stage, kBand, 0, false);
        int accentCount = 0;
        for (const BackdropRect& r : withAccents) {
            if (isAccentRole(r.role)) ++accentCount;
        }
        CHECK(accentCount > 0);                        // this stage does use accents
        CHECK(noAccents.size() < withAccents.size());  // some were dropped
        for (const BackdropRect& r : noAccents) {
            CHECK_FALSE(isAccentRole(r.role));  // none remain
        }
        CHECK_FALSE(noAccents.empty());  // silhouettes survive
    }
    for (BackdropStage stage : {BackdropStage::Keep, BackdropStage::Forest}) {
        for (const BackdropRect& r : buildBackdrop(stage, kBand, 0, true)) {
            CHECK_FALSE(isAccentRole(r.role));
        }
    }
}

TEST_CASE("backdrop: geometry is deterministic", "[backdrop]") {
    for (BackdropStage stage : kThemed) {
        const auto a = buildBackdrop(stage, kBand, 0, true);
        const auto b = buildBackdrop(stage, kBand, 0, true);
        REQUIRE(a.size() == b.size());
        for (std::size_t i = 0; i < a.size(); ++i) {
            CHECK(a[i].x == b[i].x);
            CHECK(a[i].y == b[i].y);
            CHECK(a[i].w == b[i].w);
            CHECK(a[i].h == b[i].h);
            CHECK(a[i].role == b[i].role);
        }
    }
}

TEST_CASE("backdrop: M119 painted stages - one texture per themed stage, none for Plain, all shipped at the band's size",
          "[backdrop][lint]") {
    CHECK(battleStageTextureId(BackdropStage::Plain) == nullptr);
    std::set<std::string> ids;
    for (const BackdropStage s : kThemed) {
        const char* id = battleStageTextureId(s);
        REQUIRE(id != nullptr);
        CHECK(std::string(id).rfind("bg.battle.", 0) == 0);
        ids.insert(id);
    }
    CHECK(ids.size() == 5);
    cd::assets::AssetManifest m;
    cd::content::LoadReport report;
    REQUIRE(m.load(std::filesystem::path(CRYSTAL_TEST_ASSETS_DIR), report));
    CHECK(kStageTextureW == kBand.w);
    CHECK(kStageTextureH == kBand.h);
    for (const std::string& id : ids) {
        INFO(id);
        const cd::assets::AssetEntry* e = m.find(id);
        REQUIRE(e != nullptr);
        CHECK(e->type == cd::assets::AssetType::Texture);
        // Drawn unscaled at the band's origin, so the PNG must be the band.
        std::uint32_t w = 0;
        std::uint32_t h = 0;
        REQUIRE(pngSize(std::filesystem::path(CRYSTAL_TEST_ASSETS_DIR) / e->path, w, h));
        CHECK(w == static_cast<std::uint32_t>(kStageTextureW));
        CHECK(h == static_cast<std::uint32_t>(kStageTextureH));
    }
    // The title scene rides the same manifest (a missing one falls back to
    // the canvas fill, like every scene background).
    const cd::assets::AssetEntry* title = m.find("bg.title");
    REQUIRE(title != nullptr);
    CHECK(title->type == cd::assets::AssetType::Texture);
}
