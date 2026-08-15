#pragma once

#include <algorithm>
#include <cstdint>
#include <string>
#include <vector>

#include "content/ContentDatabase.hpp"
#include "content/Definitions.hpp"
#include "game/BlackMarket.hpp"  // blackMarketHash: the shared pure-hash idiom
#include "game/Party.hpp"
#include "game/Scrolls.hpp"
#include "game/TreasureMap.hpp"  // treasureScrollPool: the exclusive Lost Scrolls

// M92 (owner decision 1): completing a 20-FLOOR run that RAISES the stakes
// offers a pick-one skill scroll — the "normal" learnset scrolls (Fireball,
// Bulwark, ...) that were authored in M64 but obtainable nowhere. Pure and
// raylib-free: the pool, the seeded offer draw, and the trigger condition are
// all headless-tested. Offers are a pure hash of the run seed (the black-
// market idiom), so reloading the entry autosave replays the same three.

namespace cd {

inline constexpr int kScrollTroveOffers = 3;
inline constexpr int kScrollTroveFloors = 20;  // the long descent (M92)
inline constexpr std::uint64_t kScrollTroveSalt = 0x5C70FFull;

// Is this one of the treasure-dig exclusives (the M65 Lost Scrolls)?
inline bool scrollIsTreasureExclusive(const std::string& id) {
    for (const char* t : treasureScrollPool()) {
        if (id == t) {
            return true;
        }
    }
    return false;
}

// Every teachable "normal" scroll: Scroll-type items that are NOT the
// treasure-dig exclusives. Sorted for a stable, deterministic order.
inline std::vector<std::string> scrollTrovePool(const content::ContentDatabase& db) {
    std::vector<std::string> pool;
    for (const auto& [id, def] : db.items()) {
        if (def.type != content::ItemType::Scroll || scrollIsTreasureExclusive(id)) {
            continue;
        }
        pool.push_back(id);
    }
    std::sort(pool.begin(), pool.end());
    return pool;
}

// True when at least one member could still learn the scroll's skill (the M64
// refusal rule, member by member) — a scroll the whole party knows is never
// offered, so the choice is always real.
inline bool anyoneCanLearn(const Party& party, const content::ItemDef& scroll,
                           const content::ContentDatabase& db) {
    for (const Character& c : party.members) {
        if (scrollRefusal(c, scroll, db).empty()) {
            return true;
        }
    }
    return false;
}

// The three seeded offers for a qualifying run: learnable pool entries drawn
// without repetition by a pure hash of the run seed. Fewer than three remain
// learnable -> fewer offers (possibly none: the modal simply is not shown).
inline std::vector<std::string> scrollTroveOffers(const Party& party,
                                                  const content::ContentDatabase& db,
                                                  std::uint64_t runSeed) {
    std::vector<std::string> learnable;
    for (const std::string& id : scrollTrovePool(db)) {
        const content::ItemDef* def = db.findItem(id);
        if (def != nullptr && anyoneCanLearn(party, *def, db)) {
            learnable.push_back(id);
        }
    }
    std::vector<std::string> offers;
    std::uint64_t salt = kScrollTroveSalt;
    while (static_cast<int>(offers.size()) < kScrollTroveOffers && !learnable.empty()) {
        const std::size_t pick = static_cast<std::size_t>(
            blackMarketHash(runSeed, salt++) % learnable.size());
        offers.push_back(learnable[pick]);
        learnable.erase(learnable.begin() + static_cast<std::ptrdiff_t>(pick));
    }
    return offers;
}

// The trigger, in one testable place: a SCORING completion of a 20-floor run
// whose (town, depth) raises the stakes. Mirrors the stakes-advance rule
// (score-0 completions move nothing, so they also earn nothing).
inline bool scrollTroveEarned(int floorCount, bool raisedStakes, int total) {
    return floorCount >= kScrollTroveFloors && raisedStakes && total > 0;
}

}  // namespace cd
