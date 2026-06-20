#pragma once

#include <cstdint>

// Tiny deterministic PRNG (xorshift64*) for seeded, reproducible dungeon
// generation. Same seed + same call sequence => identical results on every
// platform. Pure and unit-tested.

namespace cd::dungeon {

class Rng {
public:
    explicit Rng(std::uint64_t seed)
        : state_(seed != 0 ? seed : 0x9E3779B97F4A7C15ull) {}

    std::uint64_t next() {
        std::uint64_t x = state_;
        x ^= x >> 12;
        x ^= x << 25;
        x ^= x >> 27;
        state_ = x;
        return x * 0x2545F4914F6CDD1Dull;
    }

    // Inclusive integer in [lo, hi]. Returns lo if hi <= lo.
    int range(int lo, int hi) {
        if (hi <= lo) {
            return lo;
        }
        const std::uint64_t span = static_cast<std::uint64_t>(hi - lo) + 1;
        return lo + static_cast<int>(next() % span);
    }

    // True with the given percent probability [0, 100].
    bool chance(int percent) { return static_cast<int>(next() % 100) < percent; }

private:
    std::uint64_t state_;
};

}  // namespace cd::dungeon
