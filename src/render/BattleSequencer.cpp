#include "render/BattleSequencer.hpp"

#include <cmath>

namespace cd::render {

namespace {

constexpr float kWindupBase = 0.18f;
constexpr float kImpactBase = 0.14f;

float speedScale(settings::BattleSpeed s) {
    switch (s) {
        case settings::BattleSpeed::Normal: return 1.0f;
        case settings::BattleSpeed::Fast: return 0.5f;
        case settings::BattleSpeed::Instant: return 0.0f;
    }
    return 1.0f;
}

}  // namespace

void BattleSequencer::start(bool hasImpact, float settleSeconds, const BattleStageParams& params) {
    params_ = params;
    settleLen_ = settleSeconds < 0.0f ? 0.0f : settleSeconds;
    commitPending_ = false;
    committed_ = false;
    t_ = 0.0f;

    const float scale = speedScale(params.speed);
    if (!hasImpact || scale <= 0.0f) {
        windupLen_ = 0.0f;
        impactLen_ = 0.0f;
        commitPending_ = true;  // nothing to stage: show the outcome at once
        stage_ = BattleStage::Settle;
        return;
    }
    windupLen_ = kWindupBase * scale;
    impactLen_ = kImpactBase * scale;
    stage_ = BattleStage::Windup;
}

void BattleSequencer::enter(BattleStage s) {
    stage_ = s;
    t_ = 0.0f;
    if (s == BattleStage::Impact && !committed_) {
        commitPending_ = true;
    }
}

void BattleSequencer::update(float dt) {
    if (stage_ == BattleStage::Idle || stage_ == BattleStage::Finished) {
        return;
    }
    t_ += dt;
    switch (stage_) {
        case BattleStage::Windup:
            if (t_ >= windupLen_) {
                enter(BattleStage::Impact);
            }
            break;
        case BattleStage::Impact:
            if (t_ >= impactLen_) {
                enter(BattleStage::Settle);
            }
            break;
        case BattleStage::Settle:
            if (t_ >= settleLen_) {
                enter(BattleStage::Finished);
            }
            break;
        case BattleStage::Idle:
        case BattleStage::Finished:
            break;
    }
}

void BattleSequencer::skip() {
    if (stage_ == BattleStage::Idle) {
        return;
    }
    if (!committed_) {
        commitPending_ = true;
    }
    stage_ = BattleStage::Finished;
    t_ = 0.0f;
}

bool BattleSequencer::takeCommit() {
    if (!commitPending_) {
        return false;
    }
    commitPending_ = false;
    committed_ = true;
    return true;
}

float BattleSequencer::lunge() const {
    if (stage_ == BattleStage::Windup && windupLen_ > 0.0f) {
        const float p = t_ / windupLen_;
        return p > 1.0f ? 1.0f : p;
    }
    if (stage_ == BattleStage::Impact && impactLen_ > 0.0f) {
        const float p = 1.0f - t_ / impactLen_;
        return p < 0.0f ? 0.0f : p;
    }
    return 0.0f;
}

float BattleSequencer::flashStrength() const {
    if (stage_ != BattleStage::Impact || impactLen_ <= 0.0f ||
        params_.flash == settings::EffectLevel::Off) {
        return 0.0f;
    }
    float base = 1.0f - t_ / impactLen_;
    base = base < 0.0f ? 0.0f : base;
    return params_.flash == settings::EffectLevel::Reduced ? base * 0.5f : base;
}

int BattleSequencer::shakeOffset() const {
    if (stage_ != BattleStage::Impact || impactLen_ <= 0.0f ||
        params_.shake == settings::EffectLevel::Off) {
        return 0;
    }
    const float magnitude = params_.shake == settings::EffectLevel::Reduced ? 1.0f : 2.0f;
    const float decay = 1.0f - t_ / impactLen_;
    return static_cast<int>(std::lround(std::sin(t_ * 55.0f) * magnitude *
                                        (decay < 0.0f ? 0.0f : decay)));
}

}  // namespace cd::render
