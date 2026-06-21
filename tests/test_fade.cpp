#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "core/FadeController.hpp"

using cd::FadeController;
using Catch::Approx;

TEST_CASE("fade: starts fully black and reveals over the duration", "[ui]") {
    FadeController f;
    REQUIRE_FALSE(f.active());
    REQUIRE(f.coverAlpha() == Approx(0.0f));

    f.start(1.0f);
    REQUIRE(f.active());
    REQUIRE(f.coverAlpha() == Approx(1.0f));

    f.update(0.5f);
    REQUIRE(f.coverAlpha() == Approx(0.5f));

    f.update(0.5f);
    REQUIRE(f.coverAlpha() == Approx(0.0f));
    REQUIRE_FALSE(f.active());
}

TEST_CASE("fade: never goes negative and stays inactive when done", "[ui]") {
    FadeController f;
    f.start(0.3f);
    f.update(10.0f);
    REQUIRE(f.coverAlpha() == Approx(0.0f));
    REQUIRE_FALSE(f.active());
}
