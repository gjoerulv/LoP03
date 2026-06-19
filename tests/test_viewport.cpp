#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "render/Viewport.hpp"

using cd::computeViewport;
using Catch::Approx;

TEST_CASE("viewport: exact integer multiple fills the window", "[viewport]") {
    const auto vp = computeViewport(1278, 720, 426, 240);
    REQUIRE(vp.scale == Approx(3.0f));
    REQUIRE(vp.width == Approx(1278.0f));
    REQUIRE(vp.height == Approx(720.0f));
    REQUIRE(vp.x == Approx(0.0f));
    REQUIRE(vp.y == Approx(0.0f));
}

TEST_CASE("viewport: too-wide window pillarboxes (bars left/right)", "[viewport]") {
    // Window is taller-than-needed in aspect: width limits the scale.
    const auto vp = computeViewport(1000, 720, 426, 240);
    REQUIRE(vp.scale == Approx(1000.0f / 426.0f));
    REQUIRE(vp.width == Approx(1000.0f));
    REQUIRE(vp.height == Approx(240.0f * (1000.0f / 426.0f)));
    REQUIRE(vp.x == Approx(0.0f));
    REQUIRE(vp.y == Approx((720.0f - vp.height) * 0.5f));
    REQUIRE(vp.y > 0.0f);
}

TEST_CASE("viewport: too-tall window letterboxes (bars top/bottom)", "[viewport]") {
    const auto vp = computeViewport(426, 480, 426, 240);
    REQUIRE(vp.scale == Approx(1.0f));
    REQUIRE(vp.width == Approx(426.0f));
    REQUIRE(vp.height == Approx(240.0f));
    REQUIRE(vp.x == Approx(0.0f));
    REQUIRE(vp.y == Approx(120.0f));
}

TEST_CASE("viewport: result is centered and within window bounds", "[viewport]") {
    const auto vp = computeViewport(900, 700, 426, 240);
    // This window is width-limited: the image fills horizontally and is
    // letterboxed vertically. Comparisons use tolerances because the scale
    // (900/426) is not exactly representable in float.
    REQUIRE(vp.x == Approx(0.0f).margin(0.01));
    REQUIRE(vp.y > 0.0f);
    REQUIRE(vp.width == Approx(900.0f));
    REQUIRE(vp.height < 700.0f);
    // Centered: equal bars on opposite sides.
    REQUIRE(2.0f * vp.x + vp.width == Approx(900.0f));
    REQUIRE(2.0f * vp.y + vp.height == Approx(700.0f));
}

TEST_CASE("viewport: degenerate input returns all zeros", "[viewport]") {
    for (const auto& vp : {computeViewport(0, 720, 426, 240), computeViewport(1278, 0, 426, 240),
                           computeViewport(1278, 720, 0, 240), computeViewport(1278, 720, 426, 0)}) {
        REQUIRE(vp.scale == Approx(0.0f));
        REQUIRE(vp.width == Approx(0.0f));
        REQUIRE(vp.height == Approx(0.0f));
    }
}
