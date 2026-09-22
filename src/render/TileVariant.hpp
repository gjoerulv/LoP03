#pragma once

#include <cstdint>

// M128: deterministic per-cell tile variants (presentation only). A pure
// position hash — the SplitMix64 finalizer `dungeon::themeEventHash` uses,
// duplicated here so render/ never depends on dungeon/ — picks which of a
// tile's authored variants a cell shows. Nothing here touches generation, a
// layout or any RNG stream: the same (seed, x, y, salt) always yields the same
// variant, so captures and reloads are stable. The town has no seed and salts
// by its index, so every town's identical tree ring reads as its own organic
// pattern; a dungeon room salts by its index over the run seed. A linear
// `(x*a + y*b) % n` pick stripes diagonally (the pre-M128 accent hash did);
// this one does not (tests/test_tile_variant.cpp sweeps rows, columns and
// diagonals for any short period).

namespace cd::render {

inline std::uint64_t tileHash(std::uint64_t seed, int x, int y, std::uint64_t salt) {
    std::uint64_t v = seed;
    v ^= static_cast<std::uint64_t>(static_cast<std::int64_t>(x)) * 73856093ull;
    v ^= static_cast<std::uint64_t>(static_cast<std::int64_t>(y)) * 19349663ull;
    v ^= (salt + 1) * 0xD1B54A32D192ED03ull;
    v ^= v >> 30;
    v *= 0xBF58476D1CE4E5B9ull;
    v ^= v >> 27;
    v *= 0x94D049BB133111EBull;
    v ^= v >> 31;
    return v;
}

// Weighted pick: `weights` holds `count` non-negative integers (a bucket's
// share of every cell); returns the bucket the hash lands in, 0..count-1. An
// empty table or a zero total returns 0, so a caller can always index its
// first (plain) variant.
inline int tileVariant(std::uint64_t seed, int x, int y, std::uint64_t salt, const int* weights,
                       int count) {
    if (weights == nullptr || count <= 0) {
        return 0;
    }
    std::uint64_t total = 0;
    for (int i = 0; i < count; ++i) {
        total += weights[i] > 0 ? static_cast<std::uint64_t>(weights[i]) : 0u;
    }
    if (total == 0) {
        return 0;
    }
    std::uint64_t r = tileHash(seed, x, y, salt) % total;
    for (int i = 0; i < count; ++i) {
        const std::uint64_t w = weights[i] > 0 ? static_cast<std::uint64_t>(weights[i]) : 0u;
        if (r < w) {
            return i;
        }
        r -= w;
    }
    return count - 1;
}

// The shipped tables (art bible §6 / §8b): four full trees near-equal so a
// ring never favours one crown; a plain ground tile dominant over its tufted
// sibling; one plain wall course dominant over three motif variants.
inline constexpr int kTreeVariantCount = 4;
inline constexpr int kTreeVariantWeights[kTreeVariantCount] = {30, 25, 25, 20};
inline constexpr int kGroundVariantCount = 2;
inline constexpr int kGroundVariantWeights[kGroundVariantCount] = {80, 20};
inline constexpr int kWallVariantCount = 4;
inline constexpr int kWallVariantWeights[kWallVariantCount] = {55, 20, 15, 10};

}  // namespace cd::render
