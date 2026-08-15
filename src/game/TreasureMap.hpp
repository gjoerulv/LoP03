#pragma once

#include <algorithm>
#include <array>
#include <cstdint>
#include <string>
#include <vector>

#include "game/BlackMarket.hpp"  // blackMarketMix (the shared SplitMix64 primitive)
#include "game/Castle.hpp"       // bossRushOrder (the 12 dungeon bosses, sorted)

// M65 — the town puzzle map (a Heroes of Might and Magic 2 homage). Secret
// Map Pieces hide in ~10% of dungeons (one per dungeon, generation v12);
// gathering FOUR reveals a treasure in the town of the run that yielded the
// fourth piece, guarded by a seeded roster boss at that dungeon's own boss
// scale. The dig pays one of six exclusive teaching scrolls (learned
// immediately), drawn without repetition; an exhausted pool pays a legendary
// token + gold instead. Repeatable cycles (owner decision). Pure state +
// rules; the states render and fight.

namespace cd {

inline constexpr int kMapPiecesNeeded = 4;

// The revealed treasure (an optional Party save record; inert by default).
struct TreasureReveal {
    bool active = false;
    int town = 1;         // where the dig spot appears
    std::string bossId;   // the seeded guard (a dungeon-roster boss)
    int scalePct = 100;   // the guard's stat scale — that dungeon's own boss scale
};

// The town dig-spot tile. Plaza ground like the black-market candidates
// (kBlackMarketTiles); its walkability is pinned by a unit test so the
// constant can never silently land inside a building.
inline constexpr int kDigTileX = 9;
inline constexpr int kDigTileY = 10;

// The exclusive treasure scrolls, in award order (drawn without repetition —
// deterministic, no roll needed). value 0 keeps them out of every
// shop/chest/drop pool (the M44 valueless rule). M95 (owner: "new Map
// scrolls should include Summons"): after the six Lost Scrolls, the digs pay
// the three summon scrolls — then the token+gold fallback as before.
inline const std::array<const char*, 9>& treasureScrollPool() {
    static const std::array<const char*, 9> kPool = {
        "treasure_scroll_meteor",   "treasure_scroll_chain",  "treasure_scroll_mending",
        "treasure_scroll_wardrums", "treasure_scroll_doom",   "treasure_scroll_vanish",
        "summon_scroll_goose",      "summon_scroll_sentinel", "summon_scroll_spring",
    };
    return kPool;
}

// The next unawarded treasure scroll, or "" when the pool is spent (the dig
// then pays kTreasureTokenFallback + kTreasureGoldFallback instead).
inline std::string nextTreasureScroll(const std::vector<std::string>& awarded) {
    for (const char* id : treasureScrollPool()) {
        if (std::find(awarded.begin(), awarded.end(), id) == awarded.end()) {
            return id;
        }
    }
    return "";
}

inline constexpr int kTreasureTokenFallback = 1;
inline constexpr int kTreasureGoldFallback = 2500;

// The seeded treasure guard: a dungeon-roster boss (bossRushOrder — the 12
// sorted dungeon bosses; the King and the Duck keep their own arenas),
// picked by a pure hash of the dungeon seed that yielded the fourth piece.
inline std::string treasureGuardBossId(const content::ContentDatabase& content,
                                       std::uint64_t seed) {
    const std::vector<std::string> roster = bossRushOrder(content);
    if (roster.empty()) {
        return "";
    }
    constexpr std::uint64_t kSaltGuard = 0x4D415047ull;  // "MAPG"
    return roster[static_cast<std::size_t>(blackMarketMix(seed ^ kSaltGuard) % roster.size())];
}

// M65/M83 shared grant rule: one piece joins the pouch; the FOURTH converts
// into the reveal (in `town`, guarded by the seeded roster boss at
// `scalePct` — the yielding run's own boss scale) and the pouch resets.
// Returns true when the reveal fired. Pure on its arguments, so the
// in-dungeon pickup and the M83 completion drop cannot drift apart.
inline bool grantMapPiece(int& mapPieces, TreasureReveal& treasure, int town,
                          const content::ContentDatabase& content, std::uint64_t guardSeed,
                          int scalePct) {
    ++mapPieces;
    if (mapPieces < kMapPiecesNeeded) {
        return false;
    }
    mapPieces = 0;
    treasure.active = true;
    treasure.town = town;
    treasure.bossId = treasureGuardBossId(content, guardSeed);
    treasure.scalePct = scalePct;
    return true;
}

// --- M83: the 4-floor map economy -----------------------------------------
//
// Completing a 4-FLOOR run in town >= 2 rolls a map-piece drop; the chance
// climbs the ladder linearly (15% at town 2 -> 75% at town 7). The roll is a
// pure hash of the RUN seed — committed the moment the run is entered, so
// reloading the entry autosave replays the same outcome (the black-market /
// M65 discipline). While a treasure already stands revealed (not yet dug),
// successful rolls bank as OWED pieces (cap 3; beyond-full rolls are simply
// lost) and pay out right after the treasure-dig guardian falls.

inline constexpr int kMapDropMinTown = 2;
inline constexpr int kMapDropBasePct = 15;    // town 2
inline constexpr int kMapDropPerTownPct = 12; // +12 points per town
inline constexpr int kMapDropMaxPct = 75;     // town 7 (owner table)
inline constexpr int kMapPiecesOwedMax = 3;   // the IOU bank's hard cap

// Drop chance for a completed 4-floor run in `town`. `bonusPct` is the M84
// town-perk hook (+5% points), inert at 0 until authored.
inline int mapDropChancePct(int town, int bonusPct = 0) {
    if (town < kMapDropMinTown) {
        return 0;
    }
    const int base = kMapDropBasePct + kMapDropPerTownPct * (town - kMapDropMinTown);
    return std::clamp(std::min(base, kMapDropMaxPct) + bonusPct, 0, 100);
}

// The committed roll. Deterministic in (runSeed, town, bonus) — never a
// fresh RNG draw at result time.
inline bool mapDropRolls(std::uint64_t runSeed, int town, int bonusPct = 0) {
    const int chance = mapDropChancePct(town, bonusPct);
    if (chance <= 0) {
        return false;
    }
    constexpr std::uint64_t kSaltMapDrop = 0x4D415044ull;  // "MAPD"
    return static_cast<int>(blackMarketHash(runSeed, kSaltMapDrop) % 100) < chance;
}

// Banks one IOU. False when the ledger is already full — that roll is lost
// (the owner's cap rule), and the caller says so honestly.
inline bool bankMapDebt(int& mapPiecesOwed) {
    if (mapPiecesOwed >= kMapPiecesOwedMax) {
        return false;
    }
    ++mapPiecesOwed;
    return true;
}

// Pays the banked IOUs into the pouch after a dig victory. The pouch never
// exceeds kMapPiecesNeeded-1 (a fourth piece must arrive with a run's
// town/guard context to fire a reveal), so a payout that would overfill pays
// what fits and KEEPS the rest banked — an IOU is never silently lost.
// Returns the number of pieces actually paid.
inline int payMapDebt(int& mapPieces, int& mapPiecesOwed) {
    const int room = (kMapPiecesNeeded - 1) - mapPieces;
    const int pay = std::clamp(std::min(mapPiecesOwed, room), 0, kMapPiecesOwedMax);
    mapPieces += pay;
    mapPiecesOwed -= pay;
    return pay;
}

}  // namespace cd
