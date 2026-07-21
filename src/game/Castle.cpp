#include "game/Castle.hpp"

#include <algorithm>

#include "content/ContentDatabase.hpp"
#include "content/Definitions.hpp"

namespace cd {

std::vector<std::string> bossRushOrder(const content::ContentDatabase& content) {
    std::vector<std::string> ids;
    for (const auto& [id, def] : content.bosses()) {
        (void)def;
        if (id == kKingBossId) {
            continue;  // the King is its own challenge, not part of the rush roster
        }
        ids.push_back(id);
    }
    std::sort(ids.begin(), ids.end());
    return ids;
}

dungeon::EnemyTeam bossRushTeam(const content::ContentDatabase& content, int index) {
    dungeon::EnemyTeam team;
    const std::vector<std::string> order = bossRushOrder(content);
    if (index < 0 || index >= static_cast<int>(order.size())) {
        return team;  // empty bossId -> the runner treats this as "gauntlet cleared"
    }
    const std::string& id = order[static_cast<std::size_t>(index)];
    team.isBoss = true;
    team.bossId = id;
    team.statScalePct = kBossRushScalePct;
    if (const content::BossDef* b = content.findBoss(id)) {
        team.name = b->name;
    }
    return team;
}

dungeon::EnemyTeam endlessWaveTeam(const content::ContentDatabase& content, int wave) {
    dungeon::EnemyTeam team;
    std::vector<std::string> pool;
    for (const auto& [id, def] : content.enemies()) {
        (void)def;
        pool.push_back(id);
    }
    std::sort(pool.begin(), pool.end());
    if (pool.empty()) {
        return team;
    }
    const int size = endlessWaveSize(wave);
    const std::uint64_t base = static_cast<std::uint64_t>(wave < 0 ? 0 : wave) * 1000u;
    for (int i = 0; i < size; ++i) {
        const std::uint64_t h = blackMarketHash(kEndlessSeed, base + static_cast<std::uint64_t>(i));
        team.enemyIds.push_back(pool[static_cast<std::size_t>(h % pool.size())]);
    }
    team.statScalePct = endlessWaveScalePct(wave);
    team.name = "Wave " + std::to_string((wave < 0 ? 0 : wave) + 1);
    return team;
}

dungeon::EnemyTeam kingTeam(const content::ContentDatabase& content) {
    dungeon::EnemyTeam team;
    team.isBoss = true;
    team.bossId = kKingBossId;
    team.statScalePct = kKingScalePct;
    if (const content::BossDef* b = content.findBoss(kKingBossId)) {
        team.name = b->name;
    }
    return team;
}

}  // namespace cd
