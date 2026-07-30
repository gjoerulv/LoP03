#pragma once

#include <algorithm>
#include <cstdint>
#include <string>
#include <vector>

#include "game/BlackMarket.hpp"  // blackMarketMix (the shared hash primitive)

// M66 — dungeon curios: twelve original trinkets (four per theme) paid out by
// the single-use dungeon treasure maps. A constexpr table (the kAchievements
// pattern — cosmetic collection content, no editor churn); owned ids persist
// as an optional Party save field. Completing the set fires the Curator
// achievement, after which buried treasures pay a legendary token instead.

namespace cd {

struct CurioDef {
    const char* id;
    const char* name;
    const char* themeId;  // the dungeon theme whose treasures prefer it
    const char* description;
};

// Names stay within the Maps screen's four-column grid (~14 chars at the
// small font — the capture overflow lint is the referee); the descriptions
// carry the flavor.
inline constexpr CurioDef kCurios[] = {
    {"keep_crown_shard", "Crown Shard", "ruined_keep",
     "A sliver of some forgotten coronet. It still thinks highly of itself."},
    {"keep_banner", "Watch Banner", "ruined_keep",
     "Threadbare, defiant, and still on duty."},
    {"keep_gate_key", "Rusted Key", "ruined_keep",
     "Opens nothing anymore, which the key considers a personal failing."},
    {"keep_gargoyle_ear", "Gargoyle Ear", "ruined_keep",
     "It has heard centuries of secrets and repeats none of them."},
    {"mine_singing_crystal", "Song Crystal", "crystal_mine",
     "Hums a note just off every tune you know."},
    {"mine_lucky_lamp", "Lucky Lamp", "crystal_mine",
     "Its last owner swore by it. Its last owner is not around to ask."},
    {"mine_geode_heart", "Geode Heart", "crystal_mine",
     "Plain outside, a cathedral inside."},
    {"mine_vein_etching", "Vein Etching", "crystal_mine",
     "A miner's scratched map of veins that ran out long ago."},
    {"forest_elder_acorn", "Elder Acorn", "hollow_forest",
     "Heavier than it should be, and faintly warm."},
    {"forest_owl_quill", "Owl Quill", "hollow_forest",
     "Writes only at night, and only the truth."},
    {"forest_moss_idol", "Moss Idol", "hollow_forest",
     "Whoever it depicts, the moss has decided they needed a beard."},
    {"forest_firefly_lantern", "Glow Lantern", "hollow_forest",
     "Empty, yet it still glows when nobody is looking."},
};
inline constexpr int kCurioCount = static_cast<int>(sizeof(kCurios) / sizeof(kCurios[0]));

inline const CurioDef* findCurio(const std::string& id) {
    for (const CurioDef& c : kCurios) {
        if (id == c.id) {
            return &c;
        }
    }
    return nullptr;
}

inline bool ownsCurio(const std::vector<std::string>& owned, const std::string& id) {
    return std::find(owned.begin(), owned.end(), id) != owned.end();
}

// The curio a buried treasure pays: a seeded pick among the UNOWNED — the
// current theme's own curios first, any unowned otherwise, empty when the
// collection is complete (the caller pays the legendary token instead).
// Deterministic per (owned, themeId, seed), so a reload cannot re-fish.
inline std::string pickCurio(const std::vector<std::string>& owned, const std::string& themeId,
                             std::uint64_t seed) {
    std::vector<std::string> pool;
    for (const CurioDef& c : kCurios) {
        if (c.themeId == themeId && !ownsCurio(owned, c.id)) {
            pool.push_back(c.id);
        }
    }
    if (pool.empty()) {
        for (const CurioDef& c : kCurios) {
            if (!ownsCurio(owned, c.id)) {
                pool.push_back(c.id);
            }
        }
    }
    if (pool.empty()) {
        return "";
    }
    constexpr std::uint64_t kSaltCurio = 0xC0B105ull;
    return pool[static_cast<std::size_t>(blackMarketMix(seed ^ kSaltCurio) % pool.size())];
}

}  // namespace cd
