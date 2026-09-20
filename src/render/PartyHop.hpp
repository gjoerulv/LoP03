#pragma once

// The party's celebration hop (M71): each of the four party slots jumps to
// its own beat - a rectified sine, so feet always land on the ground line and
// nobody moves in step. Lifted out of CelebrationState in M127 so the save
// slots' highlighted party hops to the very same rhythm, only smaller. Pure
// (no raylib): the caller owns the clock and turns the height into pixels.

#include <cmath>

namespace cd::render {

inline constexpr float kHopFreq[4] = {2.4f, 3.1f, 2.0f, 2.8f};    // radians/second-ish
inline constexpr float kHopAmp[4] = {12.0f, 18.0f, 10.0f, 15.0f};  // pixels at scale 1
inline constexpr float kHopPhase[4] = {0.0f, 1.3f, 2.6f, 0.7f};

// How far above the ground party slot `slot` is at `time` seconds: 0 on the
// ground, at most kHopAmp[slot % 4] * scale at the top of the jump. Slots
// beyond the fourth reuse the beats in order; a negative slot reads as 0.
inline float partyHop(int slot, float time, float scale = 1.0f) {
    const int s = slot < 0 ? 0 : slot % 4;
    return std::fabs(std::sin(time * kHopFreq[s] + kHopPhase[s])) * kHopAmp[s] * scale;
}

}  // namespace cd::render
