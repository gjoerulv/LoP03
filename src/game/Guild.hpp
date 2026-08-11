#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include "dungeon/DungeonModel.hpp"  // EnemyTeam
#include "game/BlackMarket.hpp"      // blackMarketHash + kBlackMarketTokenPrice
#include "game/WorldLadder.hpp"      // kTownCount / clampTown

// Guild Masters & town milestones (M84). Every town's Guild hides a Master:
// clearing a 4-FLOOR dungeon in town N unlocks that town's "Fight the Guild
// Boss" — a two-wave gauntlet on the castle-challenge machinery (persistent
// HP/MP, the castle defeat price) at roughly depth-20 threat plus the town's
// own scaling. First victory grants a permanent pick-1-of-2 TOWN PERK (the
// M63 modal pattern, stored per town). Masters are refightable for a
// best-turns record. Everything here is pure and headless-tested; the
// content-driven team builders live in Guild.cpp.

namespace cd {

namespace content {
class ContentDatabase;
struct BossDef;
}

// --- Per-town records (optional save fields; old saves load all-locked) -----
struct GuildTownRecord {
    bool unlocked = false;  // a 4-floor clear in this town earns the audience
    int bestTurns = 0;      // 0 = the Master still presides; fewer is better
    std::string perkId;     // the chosen town perk ("" = not chosen yet)

    bool defeated() const { return bestTurns > 0; }
};
using GuildRecords = std::array<GuildTownRecord, kTownCount>;

inline GuildTownRecord& guildRecord(GuildRecords& g, int town) {
    return g[static_cast<std::size_t>(clampTown(town) - 1)];
}
inline const GuildTownRecord& guildRecord(const GuildRecords& g, int town) {
    return g[static_cast<std::size_t>(clampTown(town) - 1)];
}

// A record improves on a first victory or a better one (the duckImproved rule).
inline bool guildImproved(const GuildTownRecord& r, int turns) {
    return turns > 0 && (r.bestTurns == 0 || turns < r.bestTurns);
}

// --- The gauntlet ------------------------------------------------------------
// Wave 1 is five seeded-random enemies from the town's unlocked pool; the
// sequence is a pure hash of this fixed seed (the kEndlessSeed philosophy), so
// every attempt at a town fields the same trial and a best-turns record is a
// meaningful, reproducible measure.
inline constexpr std::uint64_t kGuildSeed = 0x6011DBA55E5EEDull;
inline constexpr int kGuildWaveSize = 5;
// The threat bar: the deepest dungeon composition (~depth 20) at the town's
// own ladder scaling. Derived in Guild.cpp from the same rules the generator
// uses, so it cannot drift from the dungeons it is meant to sit above.
inline constexpr int kGuildThreatDepth = 20;

// --- Town perks (pick 1 of 2, granted once, on first victory) ---------------
enum class GuildPerkEffect {
    MaxItems,     // +magnitude to every consumable cap (M78 capBonus, ceiling 9)
    ExpBonus,     // +magnitude % battle EXP
    MapChance,    // +magnitude %pts on the M83 4-floor map-piece roll
    EnemyGold,    // +magnitude % battle gold
    BlackMarket,  // +magnitude %pts on the M34 stakes-path market roll
    TrapGuard,    // -magnitude %pts off the chest trap's max-HP wound
    ChestGold,    // +magnitude % gold from opened chests
    SpoonOmen,    // +magnitude % relic-event omen per floor (worded cryptically)
    TokenPrice,   // the black market's token price becomes `magnitude`
};

struct GuildPerkDef {
    const char* id;
    int town;  // 1..kTownCount — which Master grants the pair
    GuildPerkEffect effect;
    int magnitude;
    const char* name;
    const char* description;
};

// The owner's table (2026-08-05), two options per town. Ids are stable once
// shipped (they persist in saves). The town-5 option A is deliberately
// cryptic — nobody says what stirs, everyone says to mind it.
inline constexpr GuildPerkDef kGuildPerks[] = {
    {"t1_pockets", 1, GuildPerkEffect::MaxItems, 1, "Deep Pockets",
     "Every consumable cap rises by 1 (to a ceiling of 9)."},
    {"t1_lessons", 1, GuildPerkEffect::ExpBonus, 10, "Guild Lessons",
     "The party earns 10% more EXP from battle."},
    {"t2_cartographers", 2, GuildPerkEffect::MapChance, 5, "Friends in Cartography",
     "Map pieces are 5% more likely after a four-floor clear."},
    {"t2_pockets", 2, GuildPerkEffect::MaxItems, 1, "Deeper Pockets",
     "Every consumable cap rises by 1 (to a ceiling of 9)."},
    {"t3_bounty", 3, GuildPerkEffect::EnemyGold, 10, "Bounty Clause",
     "Enemies pay 10% more gold."},
    {"t3_whispers", 3, GuildPerkEffect::BlackMarket, 10, "Whisper Network",
     "The black market finds you more often."},
    {"t4_trapsense", 4, GuildPerkEffect::TrapGuard, 5, "Trap Sense",
     "Chest traps wound the party less."},
    {"t4_appraisal", 4, GuildPerkEffect::ChestGold, 15, "Appraiser's Eye",
     "Chests hold 15% more gold."},
    {"t5_spoon", 5, GuildPerkEffect::SpoonOmen, 5, "Mind the Spoon",
     "Something in the dungeons stirs a little more often. Nobody says what. "
     "Mind it."},
    {"t5_cartographers", 5, GuildPerkEffect::MapChance, 5, "Old Friends in Cartography",
     "Map pieces are 5% more likely after a four-floor clear."},
    {"t6_pockets", 6, GuildPerkEffect::MaxItems, 1, "Deepest Pockets",
     "Every consumable cap rises by 1 (to a ceiling of 9)."},
    {"t6_lessons", 6, GuildPerkEffect::ExpBonus, 10, "Advanced Guild Lessons",
     "The party earns 10% more EXP from battle."},
    {"t7_patronage", 7, GuildPerkEffect::TokenPrice, 1, "Legendary Patronage",
     "The black market's legendary token price falls from 3 to 1."},
    {"t7_mastery", 7, GuildPerkEffect::ExpBonus, 15, "Guild Mastery",
     "The party earns 15% more EXP from battle."},
};
inline constexpr int kGuildPerkCount =
    static_cast<int>(sizeof(kGuildPerks) / sizeof(kGuildPerks[0]));

inline const GuildPerkDef* findGuildPerk(const std::string& id) {
    for (const GuildPerkDef& p : kGuildPerks) {
        if (id == p.id) {
            return &p;
        }
    }
    return nullptr;
}

// The town's two options, table order (option A first). Both are always
// non-null for towns 1..kTownCount — asserted by the [guild] table test.
inline std::pair<const GuildPerkDef*, const GuildPerkDef*> guildPerkPair(int town) {
    const GuildPerkDef* a = nullptr;
    const GuildPerkDef* b = nullptr;
    for (const GuildPerkDef& p : kGuildPerks) {
        if (p.town != town) {
            continue;
        }
        (a == nullptr ? a : b) = &p;
    }
    return {a, b};
}

// The first town whose perk choice is still pending (defeated, no stored
// choice) — 0 when nothing is pending. Cancel postpones, never forfeits
// (the M63 rule), so the modal simply re-asks at the next opportunity.
inline int guildPendingPerkTown(const GuildRecords& g) {
    for (int town = 1; town <= kTownCount; ++town) {
        const GuildTownRecord& r = guildRecord(g, town);
        if (r.defeated() && r.perkId.empty()) {
            return town;
        }
    }
    return 0;
}

// Sum of chosen-perk magnitudes for one effect (unknown/empty ids count 0, so
// a tampered save degrades instead of crashing).
inline int guildPerkSum(const GuildRecords& g, GuildPerkEffect effect) {
    int sum = 0;
    for (const GuildTownRecord& r : g) {
        if (r.perkId.empty()) {
            continue;
        }
        if (const GuildPerkDef* p = findGuildPerk(r.perkId)) {
            if (p->effect == effect) {
                sum += p->magnitude;
            }
        }
    }
    return sum;
}

// --- The effect queries, one per hook ---------------------------------------
inline int guildCapBonus(const GuildRecords& g) {       // M78 ItemCaps hook
    return guildPerkSum(g, GuildPerkEffect::MaxItems);
}
inline int guildExpBonusPct(const GuildRecords& g) {    // applySpoils XP
    return guildPerkSum(g, GuildPerkEffect::ExpBonus);
}
inline int guildMapBonusPct(const GuildRecords& g) {    // M83 mapDropRolls
    return guildPerkSum(g, GuildPerkEffect::MapChance);
}
inline int guildEnemyGoldPct(const GuildRecords& g) {   // applySpoils gold
    return guildPerkSum(g, GuildPerkEffect::EnemyGold);
}
inline int guildBlackMarketPct(const GuildRecords& g) { // M34 20% spawn path
    return guildPerkSum(g, GuildPerkEffect::BlackMarket);
}
inline int guildTrapGuardPct(const GuildRecords& g) {   // chest-trap wound
    return guildPerkSum(g, GuildPerkEffect::TrapGuard);
}
inline int guildChestGoldPct(const GuildRecords& g) {   // chest-open payout
    return guildPerkSum(g, GuildPerkEffect::ChestGold);
}
inline int guildSpoonOmenPct(const GuildRecords& g) {   // relic-omen post-pass
    return guildPerkSum(g, GuildPerkEffect::SpoonOmen);
}
// The token price the black market actually charges (perk: 3 -> 1). If the
// table ever carried two TokenPrice perks the LOWEST would win; today there
// is exactly one.
inline int guildTokenPrice(const GuildRecords& g) {
    int price = kBlackMarketTokenPrice;
    for (const GuildTownRecord& r : g) {
        if (r.perkId.empty()) {
            continue;
        }
        if (const GuildPerkDef* p = findGuildPerk(r.perkId)) {
            if (p->effect == GuildPerkEffect::TokenPrice && p->magnitude < price) {
                price = p->magnitude;
            }
        }
    }
    return price;
}

// --- Content-driven builders (Guild.cpp) ------------------------------------
// The gauntlet's stat scale for `town`: the depth-capped composition curve at
// kGuildThreatDepth, combined with the town ladder (the generator's own rule).
int guildScalePct(const content::ContentDatabase& content, int town);
// The town's Master: the unique BossDef with `guildTown == town` (nullptr when
// the content ships none — callers degrade, the [guild] lint asserts all 7).
const content::BossDef* findGuildMaster(const content::ContentDatabase& content, int town);
// The town's unlocked enemy pool: every non-bossOnly enemy with
// minTown <= town, sorted (the endless-pool rule, town-gated).
std::vector<std::string> guildTownPool(const content::ContentDatabase& content, int town);
// Wave 1: kGuildWaveSize seeded picks from the town pool at guildScalePct.
dungeon::EnemyTeam guildWaveTeam(const content::ContentDatabase& content, int town);
// Wave 2: the Master with its authored minions at guildScalePct (empty team
// when the content ships no Master for `town`).
dungeon::EnemyTeam guildMasterTeam(const content::ContentDatabase& content, int town);

// --- Mind the Spoon (the town-5 omen perk) ----------------------------------
// Applied AFTER generation, at the run's entry (the M76 peddler precedent):
// the generator never sees party state, so what a seed generates stays
// byte-identical for everyone and generation stays v15. For each floor that
// carries no Royal Relic event, one pure-hash roll (omenPct %) of the RUN
// seed may upgrade the floor's first plain rolled event (never a theme rite,
// the peddler, or an elite challenge — whose team would be orphaned) into the
// relic event. Deterministic and reload-proof; omenPct 0 is a no-op.
inline constexpr std::uint64_t kGuildOmenSalt = 0x5900B0DE5ull;
void applyGuildRelicOmen(std::vector<dungeon::Dungeon>& floors, std::uint64_t runSeed,
                         int omenPct);

}  // namespace cd
