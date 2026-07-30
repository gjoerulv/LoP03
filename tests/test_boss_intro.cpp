#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <vector>

#include "settings/Settings.hpp"
#include "states/BossIntroTimeline.hpp"

// M56 — the Crystal Shatter intro timeline + shard layout. Pure and headless;
// never touches the battle model.

using namespace cd;

TEST_CASE("boss intro: phases run in order with the planned durations", "[boss-intro]") {
    BossIntroTimeline t;
    CHECK(t.phase() == BossIntroTimeline::Phase::Hold);
    t.t = 0.05f;
    CHECK(t.phase() == BossIntroTimeline::Phase::Hold);
    t.t = BossIntroTimeline::kHold + 0.01f;
    CHECK(t.phase() == BossIntroTimeline::Phase::Build);
    t.t = BossIntroTimeline::kBuildEnd + 0.01f;
    CHECK(t.phase() == BossIntroTimeline::Phase::Peak);
    t.t = BossIntroTimeline::kPeakEnd + 0.01f;
    CHECK(t.phase() == BossIntroTimeline::Phase::Handoff);
    t.t = BossIntroTimeline::kTotal + 0.01f;
    CHECK(t.phase() == BossIntroTimeline::Phase::Done);
    CHECK(t.done());
    // The four durations are the approved ones.
    CHECK(BossIntroTimeline::kHold == 0.10f);
    CHECK(BossIntroTimeline::kBuild == 0.45f);
    CHECK(BossIntroTimeline::kPeak == 0.15f);
    CHECK(BossIntroTimeline::kHandoff == 0.20f);
}

TEST_CASE("boss intro: advancing by dt reaches Done and never rewinds", "[boss-intro]") {
    BossIntroTimeline t;
    float lastCrack = -1.0f;
    for (int i = 0; i < 200 && !t.done(); ++i) {
        t.advance(1.0f / 60.0f);
        CHECK(t.crackProgress() >= lastCrack);  // monotonic non-decreasing
        lastCrack = t.crackProgress();
    }
    CHECK(t.done());
    CHECK(t.crackProgress() == 1.0f);
    CHECK(t.handoffProgress() == 1.0f);
    CHECK(t.darkness() == 1.0f);
}

TEST_CASE("boss intro: skip jumps to Handoff and still completes", "[boss-intro]") {
    BossIntroTimeline t;
    t.t = 0.2f;  // mid-Build
    t.skip();
    CHECK(t.skipped);
    CHECK(t.phase() == BossIntroTimeline::Phase::Handoff);
    CHECK_FALSE(t.done());
    // A skip never rewinds a later timeline.
    BossIntroTimeline late;
    late.t = BossIntroTimeline::kTotal - 0.01f;  // already in Handoff
    late.skip();
    CHECK(late.t >= BossIntroTimeline::kPeakEnd);
    // Advancing after a skip reaches Done.
    for (int i = 0; i < 60 && !t.done(); ++i) {
        t.advance(1.0f / 60.0f);
    }
    CHECK(t.done());
}

TEST_CASE("boss intro: the Peak pulse is a single decay, 0 outside Peak", "[boss-intro]") {
    BossIntroTimeline t;
    t.t = 0.05f;  // Hold
    CHECK(t.peakPulse() == 0.0f);
    t.t = BossIntroTimeline::kHold + 0.2f;  // Build
    CHECK(t.peakPulse() == 0.0f);
    t.t = BossIntroTimeline::kBuildEnd;  // Peak start
    CHECK(t.peakPulse() > 0.9f);
    t.t = BossIntroTimeline::kPeakEnd - 0.001f;  // Peak end
    CHECK(t.peakPulse() < 0.1f);
    t.t = BossIntroTimeline::kPeakEnd + 0.05f;  // Handoff
    CHECK(t.peakPulse() == 0.0f);
    // Monotonic decay within Peak (single pulse, never a strobe).
    float prev = 2.0f;
    for (float u = 0.0f; u < BossIntroTimeline::kPeak; u += 0.01f) {
        t.t = BossIntroTimeline::kBuildEnd + u;
        CHECK(t.peakPulse() <= prev);
        prev = t.peakPulse();
    }
}

TEST_CASE("boss intro: the pulse alpha obeys the flash setting and the 0.12 cap", "[boss-intro]") {
    // Off -> 0; Reduced -> half of Full; Full -> capped at 0.12.
    CHECK(bossIntroPulseAlpha(1.0f, settings::EffectLevel::Off) == 0.0f);
    CHECK(bossIntroPulseAlpha(1.0f, settings::EffectLevel::Full) <= 0.12f);
    CHECK(bossIntroPulseAlpha(1.0f, settings::EffectLevel::Full) > 0.11f);
    const float full = bossIntroPulseAlpha(0.8f, settings::EffectLevel::Full);
    const float reduced = bossIntroPulseAlpha(0.8f, settings::EffectLevel::Reduced);
    CHECK(std::fabs(reduced - full * 0.5f) < 1e-6f);
    CHECK(bossIntroPulseAlpha(0.0f, settings::EffectLevel::Full) == 0.0f);
}

TEST_CASE("boss intro: shard layout is bounded and deterministic", "[boss-intro]") {
    const std::vector<IntroShard> a = buildIntroShards(0xC0FFEEull, 14);
    const std::vector<IntroShard> b = buildIntroShards(0xC0FFEEull, 14);
    REQUIRE(a.size() == 14);
    REQUIRE(b.size() == a.size());
    for (std::size_t i = 0; i < a.size(); ++i) {
        // Determinism.
        CHECK(a[i].dirX == b[i].dirX);
        CHECK(a[i].dirY == b[i].dirY);
        CHECK(a[i].speed == b[i].speed);
        CHECK(a[i].size == b[i].size);
        // Bounds: unit-ish direction, travel fraction, size.
        CHECK(std::fabs(a[i].dirX) <= 1.0f);
        CHECK(std::fabs(a[i].dirY) <= 1.0f);
        const float mag = std::sqrt(a[i].dirX * a[i].dirX + a[i].dirY * a[i].dirY);
        CHECK(mag > 0.9f);  // roughly unit length
        CHECK(mag < 1.1f);
        CHECK(a[i].speed >= 0.55f);
        CHECK(a[i].speed <= 1.0f);
        CHECK(a[i].size >= 4);
        CHECK(a[i].size <= 10);
    }
    // A different seed gives a different layout (not all identical).
    const std::vector<IntroShard> c = buildIntroShards(0x1234ull, 14);
    bool differs = false;
    for (std::size_t i = 0; i < a.size(); ++i) {
        differs = differs || a[i].dirX != c[i].dirX || a[i].size != c[i].size;
    }
    CHECK(differs);
    // count < 1 is clamped to 1.
    CHECK(buildIntroShards(1, 0).size() == 1);
}
