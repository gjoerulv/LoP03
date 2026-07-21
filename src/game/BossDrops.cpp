#include "game/BossDrops.hpp"

#include <algorithm>

#include "content/ContentDatabase.hpp"
#include "content/Definitions.hpp"
#include "game/Castle.hpp"  // kKingLegendaryId (the King-only unique is not in the pool)

namespace cd {

std::vector<std::string> legendaryDropPool(const content::ContentDatabase& content) {
    std::vector<std::string> ids;
    for (const auto& [id, def] : content.items()) {
        if (id == kKingLegendaryId) {
            continue;  // M40: the King's unique regalia is won only from the King
        }
        if (def.rarity == content::Rarity::Legendary &&
            (def.type == content::ItemType::Equipment ||
             def.type == content::ItemType::Relic)) {
            ids.push_back(id);
        }
    }
    std::sort(ids.begin(), ids.end());
    return ids;
}

BossDropResult rollBossDrops(std::uint64_t seed, int town, int depth,
                             const content::ContentDatabase& content) {
    BossDropResult result;
    if (!bossDropEligible(town, depth)) {
        return result;
    }
    if (bossTokenRolls(seed, town, depth)) {
        result.tokens = bossTokenCount(town);
    }
    if (bossLegendaryRolls(seed, town, depth)) {
        const std::vector<std::string> pool = legendaryDropPool(content);
        if (!pool.empty()) {
            result.legendary = true;
            result.legendaryId =
                pool[static_cast<std::size_t>(bossLegendaryIndex(seed, static_cast<int>(pool.size())))];
        }
    }
    return result;
}

}  // namespace cd
