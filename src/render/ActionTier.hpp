#pragma once

#include "content/Definitions.hpp"
#include "content/Enums.hpp"
#include "render/BattleSequencer.hpp"
#include "settings/Settings.hpp"

// M130: how big an action's animation is, DERIVED from the authored skill
// fields (the M121 SkillKind way — nothing in data/skills.json names an
// animation, and no schema moved). Pure, raylib-free, headless-tested.
//
//   Minor  every basic attack, small hit, heal, buff, debuff and item
//   Major  other all-enemies damage, single-target damage of power 14..19
//   Grand  all-enemies magic damage of power >= 10 (Inferno, Blizzard,
//          Radiance, Chain Lightning, the Cataclysm, the Dragon's breaths),
//          single-target damage of power >= 20, the Dragon's sweep
//
// Escalation applies to damage only: a support skill never animates longer
// than a spell. Summons keep the M107 apparition and get no ActionFx at all.
//
// Owner ruling (2026-09-22): on Battle Speed Fast the new animations are
// skipped entirely — the tier collapses to today's base windup and nothing is
// drawn — and Instant keeps its zero-length staging. Normal plays them. The
// windup grows with the tier; the impact beat is never changed (it drives the
// hit flash and shake everyone already knows), and the tail draws inside the
// settle pause that already exists, so a Grand action adds 0.24 s at most.

namespace cd::render {

enum class ActionTier { Minor, Major, Grand };

inline ActionTier actionTierForSkill(const content::SkillDef& s) {
    if (content::isSummonSkill(s) || !content::skillDealsDamage(s)) {
        return ActionTier::Minor;
    }
    if (s.target == content::SkillTarget::AllEnemies) {
        return (s.category == content::SkillCategory::Magic && s.power >= 10) ? ActionTier::Grand
                                                                               : ActionTier::Major;
    }
    if (s.power >= 20) {
        return ActionTier::Grand;
    }
    if (s.power >= 14) {
        return ActionTier::Major;
    }
    return ActionTier::Minor;
}

// A basic attack: Minor, unless it sweeps the whole side (the Dragon).
inline ActionTier actionTierForBasicAttack(bool hitsAll) {
    return hitsAll ? ActionTier::Grand : ActionTier::Minor;
}

// The ONE place the owner's speed rule lives.
inline bool actionFxEnabled(settings::BattleSpeed speed) {
    return speed == settings::BattleSpeed::Normal;
}

inline constexpr float kActionWindupMinor = 0.24f;
inline constexpr float kActionWindupMajor = 0.30f;
inline constexpr float kActionWindupGrand = 0.42f;
inline constexpr float kActionFrameTime = 0.07f;  // ~14 fps, the 80s cadence

// The sequencer windup for a tier at a speed: Fast and Instant return the
// base (today's beat, which the sequencer then scales exactly as before).
inline float actionWindupSeconds(ActionTier tier, settings::BattleSpeed speed) {
    if (!actionFxEnabled(speed)) {
        return kWindupBase;
    }
    switch (tier) {
        case ActionTier::Minor: return kActionWindupMinor;
        case ActionTier::Major: return kActionWindupMajor;
        case ActionTier::Grand: return kActionWindupGrand;
    }
    return kWindupBase;
}

// How long the burst keeps drawing after its last frame (inside Settle).
inline float actionTailSeconds(ActionTier tier) {
    switch (tier) {
        case ActionTier::Minor: return 0.20f;
        case ActionTier::Major: return 0.30f;
        case ActionTier::Grand: return 0.45f;
    }
    return 0.2f;
}

inline int actionFrames(ActionTier tier) {
    switch (tier) {
        case ActionTier::Minor: return 3;
        case ActionTier::Major: return 4;
        case ActionTier::Grand: return 6;
    }
    return 3;
}

// The acting unit's lunge in pixels (today's 4 px stays the Minor value).
inline float actionLungePixels(ActionTier tier) {
    switch (tier) {
        case ActionTier::Minor: return 4.0f;
        case ActionTier::Major: return 6.0f;
        case ActionTier::Grand: return 8.0f;
    }
    return 4.0f;
}

// Frame index at t seconds into a burst: holds the last frame (the tail);
// t <= 0 or degenerate inputs yield frame 0 (the render::frameAt shape).
inline int frameIndex(float t, int frames, float frameTime) {
    if (frames <= 1 || !(frameTime > 0.0f) || t <= 0.0f) {
        return 0;
    }
    const int raw = static_cast<int>(t / frameTime);
    return raw >= frames ? frames - 1 : raw;
}

// The whole burst's lifetime: its frames, then the tail.
inline float actionBurstSeconds(ActionTier tier) {
    return static_cast<float>(actionFrames(tier)) * kActionFrameTime + actionTailSeconds(tier);
}

}  // namespace cd::render
