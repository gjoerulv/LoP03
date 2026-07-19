#include <catch2/catch_test_macros.hpp>

#include "render/BattleSequencer.hpp"

using cd::render::BattleSequencer;
using cd::render::BattleStage;
using cd::render::BattleStageParams;
using cd::settings::BattleSpeed;
using cd::settings::EffectLevel;

namespace {

BattleStageParams params(BattleSpeed speed = BattleSpeed::Normal,
                         EffectLevel flash = EffectLevel::Full,
                         EffectLevel shake = EffectLevel::Full) {
    BattleStageParams p;
    p.speed = speed;
    p.flash = flash;
    p.shake = shake;
    return p;
}

}  // namespace

TEST_CASE("sequencer: normal action stages windup, impact, settle", "[battleseq]") {
    BattleSequencer s;
    s.start(/*hasImpact=*/true, /*settle=*/0.9f, params());
    REQUIRE(s.stage() == BattleStage::Windup);
    CHECK_FALSE(s.takeCommit());  // nothing committed before impact

    s.update(0.10f);
    REQUIRE(s.stage() == BattleStage::Windup);
    CHECK(s.lunge() > 0.0f);

    s.update(0.10f);  // past 0.18 windup
    REQUIRE(s.stage() == BattleStage::Impact);
    CHECK(s.takeCommit());        // commit fires at impact...
    CHECK_FALSE(s.takeCommit());  // ...exactly once

    s.update(0.20f);  // past 0.14 impact
    REQUIRE(s.stage() == BattleStage::Settle);
    CHECK(s.flashStrength() == 0.0f);
    CHECK(s.shakeOffset() == 0);

    s.update(0.95f);
    REQUIRE(s.finished());
}

TEST_CASE("sequencer: fast speed halves staging durations", "[battleseq]") {
    BattleSequencer s;
    s.start(true, 0.45f, params(BattleSpeed::Fast));
    s.update(0.05f);
    REQUIRE(s.stage() == BattleStage::Windup);  // 0.09 windup not yet done
    s.update(0.05f);
    REQUIRE(s.stage() == BattleStage::Impact);
    s.update(0.08f);  // past 0.07 impact
    REQUIRE(s.stage() == BattleStage::Settle);
}

TEST_CASE("sequencer: instant speed skips staging entirely", "[battleseq]") {
    BattleSequencer s;
    s.start(true, 0.05f, params(BattleSpeed::Instant));
    REQUIRE(s.stage() == BattleStage::Settle);
    CHECK(s.takeCommit());  // outcome visible immediately
    s.update(0.06f);
    REQUIRE(s.finished());
}

TEST_CASE("sequencer: impact-less actions go straight to settle", "[battleseq]") {
    BattleSequencer s;
    s.start(/*hasImpact=*/false, 0.9f, params());
    REQUIRE(s.stage() == BattleStage::Settle);
    CHECK(s.takeCommit());
    CHECK(s.lunge() == 0.0f);
    CHECK(s.flashStrength() == 0.0f);
    CHECK(s.shakeOffset() == 0);
}

TEST_CASE("sequencer: skip finishes and still fires the commit once", "[battleseq]") {
    BattleSequencer s;
    s.start(true, 0.9f, params());
    s.update(0.05f);  // mid-windup
    s.skip();
    REQUIRE(s.finished());
    CHECK(s.takeCommit());
    CHECK_FALSE(s.takeCommit());

    // Skip after the commit already fired must not re-fire it.
    BattleSequencer s2;
    s2.start(true, 0.9f, params());
    s2.update(0.20f);  // into impact
    REQUIRE(s2.takeCommit());
    s2.skip();
    CHECK_FALSE(s2.takeCommit());
}

TEST_CASE("sequencer: flash and shake honor their settings", "[battleseq]") {
    BattleSequencer off;
    off.start(true, 0.9f, params(BattleSpeed::Normal, EffectLevel::Off, EffectLevel::Off));
    off.update(0.19f);  // into impact
    REQUIRE(off.stage() == BattleStage::Impact);
    CHECK(off.flashStrength() == 0.0f);
    CHECK(off.shakeOffset() == 0);

    BattleSequencer reduced;
    reduced.start(true, 0.9f,
                  params(BattleSpeed::Normal, EffectLevel::Reduced, EffectLevel::Reduced));
    reduced.update(0.19f);
    REQUIRE(reduced.stage() == BattleStage::Impact);
    CHECK(reduced.flashStrength() > 0.0f);
    CHECK(reduced.flashStrength() <= 0.5f);

    BattleSequencer full;
    full.start(true, 0.9f, params());
    full.update(0.19f);
    REQUIRE(full.stage() == BattleStage::Impact);
    CHECK(full.flashStrength() > 0.5f);

    // Shake magnitude is bounded by the setting (|full| <= 2, |reduced| <= 1).
    bool sawNonZero = false;
    for (int i = 0; i < 10; ++i) {
        full.update(0.005f);
        if (full.stage() != BattleStage::Impact) {
            break;
        }
        CHECK(full.shakeOffset() >= -2);
        CHECK(full.shakeOffset() <= 2);
        sawNonZero = sawNonZero || full.shakeOffset() != 0;
    }
    CHECK(sawNonZero);
}
