#pragma once

#include <cmath>
#include <cstdint>
#include <vector>

#include "settings/Settings.hpp"

// M56 — the Crystal Shatter boss-intro timeline and shard layout. Pure,
// dt-injected, header-only (FadeController/BattleSequencer-shaped) so both are
// unit-tested headlessly and never touch the battle model or its RNG.

namespace cd {

// The Peak dim-pulse alpha: the M51 AoE-tint contract exactly — cap 0.12, scaled
// by the flash setting (Off -> 0, Reduced -> half, Full -> as given). `pulse` is
// BossIntroTimeline::peakPulse(). Pure; shared by the intro render and the tests.
inline float bossIntroPulseAlpha(float pulse, settings::EffectLevel flash) {
    if (pulse <= 0.0f || flash == settings::EffectLevel::Off) {
        return 0.0f;
    }
    const float cap = 0.12f;
    const float scale = flash == settings::EffectLevel::Reduced ? 0.5f : 1.0f;
    const float a = cap * pulse * scale;
    return a < 0.0f ? 0.0f : (a > cap ? cap : a);
}

struct BossIntroTimeline {
    enum class Phase { Hold, Build, Peak, Handoff, Done };

    static constexpr float kHold = 0.10f;     // live scene holds a beat
    static constexpr float kBuild = 0.45f;    // cracks grow from centre
    static constexpr float kPeak = 0.15f;     // dim pulse + shake, darkening completes
    static constexpr float kHandoff = 0.20f;  // shards fling over black
    static constexpr float kBuildEnd = kHold + kBuild;
    static constexpr float kPeakEnd = kBuildEnd + kPeak;
    static constexpr float kTotal = kPeakEnd + kHandoff;

    float t = 0.0f;
    bool skipped = false;

    static float clamp01(float v) { return v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v); }

    void advance(float dt) {
        if (dt > 0.0f) {
            t += dt;
        }
    }
    // Confirm/Cancel: jump to the start of Handoff so a brief shatter still plays
    // before the battle takes over.
    void skip() {
        if (t < kPeakEnd) {
            t = kPeakEnd;
        }
        skipped = true;
    }

    Phase phase() const {
        if (t < kHold) return Phase::Hold;
        if (t < kBuildEnd) return Phase::Build;
        if (t < kPeakEnd) return Phase::Peak;
        if (t < kTotal) return Phase::Handoff;
        return Phase::Done;
    }
    bool done() const { return t >= kTotal; }

    // Cracks grow through Build (0 during Hold, 1 from Peak on).
    float crackProgress() const {
        if (t <= kHold) return 0.0f;
        return clamp01((t - kHold) / kBuild);
    }
    // A single decay pulse: 1 at the start of Peak, 0 by its end, 0 elsewhere.
    // Multiply by the flash cap/scale at the call site (the M51 AoE-tint contract).
    float peakPulse() const {
        if (t < kBuildEnd || t >= kPeakEnd) return 0.0f;
        return clamp01(1.0f - (t - kBuildEnd) / kPeak);
    }
    // Shards fling out through Handoff (0 before, 1 by the end).
    float handoffProgress() const {
        if (t < kPeakEnd) return 0.0f;
        return clamp01((t - kPeakEnd) / kHandoff);
    }
    // Darkening is an UNCAPPED fade (not a flash): 0 -> 0.6 across Build, 0.6 ->
    // 1.0 across Peak, full black through Handoff.
    float darkness() const {
        if (t <= kHold) return 0.0f;
        if (t < kBuildEnd) return clamp01((t - kHold) / kBuild) * 0.6f;
        if (t < kPeakEnd) return 0.6f + 0.4f * clamp01((t - kBuildEnd) / kPeak);
        return 1.0f;
    }
};

// One flung shard: a unit-ish outward direction, a travel fraction, and a size.
// Presentation-only; laid out from a pure hash of the dungeon seed.
struct IntroShard {
    float dirX = 0.0f;
    float dirY = 0.0f;
    float speed = 0.0f;  // 0.55..1.0 fraction of max travel
    int size = 0;        // 4..10 px
};

// Deterministic shard layout from a presentation seed (never the battle RNG).
inline std::vector<IntroShard> buildIntroShards(std::uint64_t seed, int count) {
    if (count < 1) count = 1;
    std::vector<IntroShard> shards;
    shards.reserve(static_cast<std::size_t>(count));
    for (int i = 0; i < count; ++i) {
        std::uint64_t x = seed ^ (0x9E3779B97F4A7C15ull * (static_cast<std::uint64_t>(i) + 1));
        x ^= x >> 30;
        x *= 0xBF58476D1CE4E5B9ull;
        x ^= x >> 27;
        x *= 0x94D049BB133111EBull;
        x ^= x >> 31;
        const float angle = (static_cast<float>(x % 3600u) / 3600.0f) * 6.2831853f;  // 0..2pi
        const float speed = 0.55f + static_cast<float>((x >> 12) % 46u) / 100.0f;     // 0.55..1.0
        const int size = 4 + static_cast<int>((x >> 24) % 7u);                        // 4..10
        shards.push_back({std::cos(angle), std::sin(angle), speed, size});
    }
    return shards;
}

}  // namespace cd
