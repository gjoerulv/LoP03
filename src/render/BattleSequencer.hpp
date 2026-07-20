#pragma once

#include "settings/Settings.hpp"

// Staged battle-action presentation (M18). The simulation resolves an action
// synchronously and deterministically; this sequencer only paces how the
// already-computed outcome is shown: a brief windup (actor lunge), an impact
// beat (flash/shake; numbers, SFX, and displayed HP commit), then the
// message settle pause. Pure and time-injected — no raylib, fully
// headless-tested. Presentation can never change simulation results.

namespace cd::render {

enum class BattleStage { Idle, Windup, Impact, Settle, Finished };

struct BattleStageParams {
    settings::BattleSpeed speed = settings::BattleSpeed::Normal;
    settings::EffectLevel flash = settings::EffectLevel::Full;
    settings::EffectLevel shake = settings::EffectLevel::Full;
};

class BattleSequencer {
public:
    // Begins presenting one resolved action. hasImpact=false (guard and
    // other no-contact actions) skips windup/impact and goes straight to the
    // settle pause; Instant speed does the same. settleSeconds is the
    // message pause (already speed-resolved by the caller).
    void start(bool hasImpact, float settleSeconds, const BattleStageParams& params);

    void update(float dt);
    void skip();  // jump to Finished; the pending commit still fires

    BattleStage stage() const { return stage_; }
    bool finished() const { return stage_ == BattleStage::Finished; }
    bool idle() const { return stage_ == BattleStage::Idle; }

    // One-shot: true exactly once per start(), at the moment floating
    // numbers, SFX, and displayed HP should appear (impact start, or
    // immediately for impact-less/instant/skipped actions).
    bool takeCommit();

    // 0..1 lunge amount for the acting unit (rises through windup, falls
    // through impact).
    float lunge() const;
    // 0..1 brighten strength for hit units (impact only; honors flash level;
    // a pulse, never a strobe).
    float flashStrength() const;
    // Screen shake offset in pixels (impact only; honors shake level).
    int shakeOffset() const;

private:
    void enter(BattleStage s);

    BattleStage stage_ = BattleStage::Idle;
    float t_ = 0.0f;  // time within the current stage
    float windupLen_ = 0.0f;
    float impactLen_ = 0.0f;
    float settleLen_ = 0.0f;
    bool commitPending_ = false;
    bool committed_ = false;
    BattleStageParams params_;
};

}  // namespace cd::render
