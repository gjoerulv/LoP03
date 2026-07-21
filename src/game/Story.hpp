#pragma once

// Story read-progress (M41): a 7-bit mask of which town installments of the
// wandering storyteller's serial the party has heard. Hearing all seven unlocks
// the Jester's punchline at the castle. Pure, headless-testable; the mask is an
// additive optional Party save field (old saves -> 0, nothing heard).

namespace cd {

inline constexpr int kStoryTownCount = 7;                    // storyteller installments
inline constexpr int kStoryAllMask = (1 << kStoryTownCount) - 1;  // 0x7F

// Bit for town 1..7 (0 for anything else, e.g. the castle's Jester beat, which is
// not part of the unlock mask).
inline int storyBit(int town) {
    return (town >= 1 && town <= kStoryTownCount) ? (1 << (town - 1)) : 0;
}
inline bool storyHeard(int storyMet, int town) { return (storyMet & storyBit(town)) != 0; }
inline bool storyAllHeard(int storyMet) { return (storyMet & kStoryAllMask) == kStoryAllMask; }

}  // namespace cd
