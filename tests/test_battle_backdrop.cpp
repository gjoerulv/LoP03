#include <catch2/catch_test_macros.hpp>

#include <vector>

#include "render/BattleBackdrop.hpp"

// M56 — per-theme battle backdrops. Pure geometry only; drawing is validated by
// the capture tool. These enforce the "subdued" rules mechanically.

using namespace cd::render;

namespace {
// The battle band as BattleState builds it: (0, 24, 426, 122).
constexpr BackdropBand kBand{0, 24, 426, 122};

bool overlaps(const BackdropRect& r, int x0, int y0, int x1, int y1) {
    return r.x < x1 && r.x + r.w > x0 && r.y < y1 && r.y + r.h > y0;
}

long coverage(const std::vector<BackdropRect>& rects) {
    long area = 0;
    for (const BackdropRect& r : rects) {
        area += static_cast<long>(r.w) * r.h;
    }
    return area;
}
}  // namespace

TEST_CASE("backdrop: theme id maps to a stage, unknown is Plain", "[backdrop]") {
    CHECK(stageForTheme("ruined_keep") == BackdropStage::Keep);
    CHECK(stageForTheme("crystal_mine") == BackdropStage::Mine);
    CHECK(stageForTheme("hollow_forest") == BackdropStage::Forest);
    CHECK(stageForTheme("") == BackdropStage::Plain);
    CHECK(stageForTheme("no_such_theme") == BackdropStage::Plain);
}

TEST_CASE("backdrop: Plain draws nothing", "[backdrop]") {
    CHECK(buildBackdrop(BackdropStage::Plain, kBand, 0, true).empty());
    CHECK(buildBackdrop(BackdropStage::Plain, kBand, 1, false).empty());
}

TEST_CASE("backdrop: every stage stays subdued and clear of the float corridor", "[backdrop]") {
    const BackdropStage stages[] = {BackdropStage::Keep, BackdropStage::Mine,
                                    BackdropStage::Forest, BackdropStage::Castle};
    // The central float/status corridor that must stay clear: inner span,
    // below the top skyline strip, above the bottom silhouette strip.
    const int corridorX0 = kBand.x + kCorridorXMarginPx;
    const int corridorX1 = kBand.x + kBand.w - kCorridorXMarginPx;
    const int corridorY0 = kBand.y + kSkylineStripPx;
    const int corridorY1 = kBand.y + static_cast<int>(kBand.h * kSilhouetteFloorFrac);
    const long bandArea = static_cast<long>(kBand.w) * kBand.h;
    const long maxCoverage = static_cast<long>(kBand.w * kBand.h * kMaxCoverageFrac);

    for (BackdropStage stage : stages) {
        for (int phase = 0; phase <= 1; ++phase) {
            for (bool accents : {true, false}) {
                const auto rects = buildBackdrop(stage, kBand, phase, accents);
                INFO("stage=" << static_cast<int>(stage) << " phase=" << phase
                              << " accents=" << accents);
                REQUIRE_FALSE(rects.empty());
                for (const BackdropRect& r : rects) {
                    // In-band.
                    CHECK(r.x >= kBand.x);
                    CHECK(r.y >= kBand.y);
                    CHECK(r.x + r.w <= kBand.x + kBand.w);
                    CHECK(r.y + r.h <= kBand.y + kBand.h);
                    // Never intrudes on the central corridor.
                    CHECK_FALSE(overlaps(r, corridorX0, corridorY0, corridorX1, corridorY1));
                    // High contrast: no accent roles present.
                    if (!accents) {
                        CHECK_FALSE(isAccentRole(r.role));
                    }
                }
                // Coverage stays subdued.
                CHECK(coverage(rects) <= maxCoverage);
                CHECK(coverage(rects) < bandArea);  // sanity
            }
        }
    }
}

TEST_CASE("backdrop: high contrast drops accents but keeps the silhouettes", "[backdrop]") {
    // Mine (crystal glint) and Castle (gold pips + keyline) carry accents; Keep
    // and Forest carry none. accents=false must never lose the silhouette bodies.
    for (BackdropStage stage : {BackdropStage::Mine, BackdropStage::Castle}) {
        const auto withAccents = buildBackdrop(stage, kBand, 0, true);
        const auto noAccents = buildBackdrop(stage, kBand, 0, false);
        int accentCount = 0;
        for (const BackdropRect& r : withAccents) {
            if (isAccentRole(r.role)) ++accentCount;
        }
        CHECK(accentCount > 0);                 // this stage does use accents
        CHECK(noAccents.size() < withAccents.size());  // some were dropped
        for (const BackdropRect& r : noAccents) {
            CHECK_FALSE(isAccentRole(r.role));  // none remain
        }
        CHECK_FALSE(noAccents.empty());         // silhouettes survive
    }
}

TEST_CASE("backdrop: geometry is deterministic", "[backdrop]") {
    for (BackdropStage stage : {BackdropStage::Keep, BackdropStage::Mine, BackdropStage::Forest,
                                BackdropStage::Castle}) {
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
