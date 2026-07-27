#include "game/Castle.hpp"

#include <algorithm>

#include "content/ContentDatabase.hpp"
#include "content/Definitions.hpp"
#include "game/WorldLadder.hpp"

namespace cd {

int castleFloorScalePct(const content::ContentDatabase& content) {
    // The strongest multiplier the ladder can produce: the deepest depth (the
    // composition's own cap makes anything past it identical) at the last town.
    // Derived from the same two rules the generator uses — combineTownScale over
    // the composition's depth curve — so this can never drift from the dungeons
    // it is supposed to sit above.
    const content::CompositionDef& comp = content.composition();
    const int deepestDepthScaled = 100 + comp.scalePctMax;
    return combineTownScale(deepestDepthScaled, kTownCount);
}

std::vector<std::string> bossRushOrder(const content::ContentDatabase& content) {
    std::vector<std::string> ids;
    for (const auto& [id, def] : content.bosses()) {
        (void)def;
        if (id == kKingBossId) {
            continue;  // the King is its own challenge, not part of the rush roster
        }
        if (id == kDuckBossId) {
            continue;  // M61: the Duck has his own pond, same rule as the King
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
        // M49: a rush boss brings the same court it brings in a dungeon. A boss
        // with an empty minion list still fights alone, so this is one rule for
        // all thirteen rather than a rush-specific roster.
        team.enemyIds = b->minions;
    }
    return team;
}

dungeon::EnemyTeam endlessWaveTeam(const content::ContentDatabase& content, int wave) {
    dungeon::EnemyTeam team;
    std::vector<std::string> pool;
    for (const auto& [id, def] : content.enemies()) {
        // M49: the Royal Guards belong to the King's throne room and nowhere
        // else — the endless pool is the other place that sweeps the whole
        // enemy database, so it needs the same guard as the generator's.
        if (def.bossOnly) {
            continue;
        }
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
        team.enemyIds = b->minions;  // M49: the King's Royal Guards
    }
    return team;
}

dungeon::EnemyTeam gooseWaveTeam(const content::ContentDatabase& content) {
    // M61: the gauntlet's opening fight is the Duck's authored court — the five
    // Evil Geese live on his BossDef's `minions` list, so the pairing is
    // content, not a hardcoded roster.
    dungeon::EnemyTeam team;
    team.name = "The Evil Geese";
    team.statScalePct = kGooseTownScalePct;
    if (const content::BossDef* b = content.findBoss(kDuckBossId)) {
        team.enemyIds = b->minions;
    }
    return team;
}

dungeon::EnemyTeam duckTeam(const content::ContentDatabase& content) {
    // M61: the Duck fights ALONE — his court already fell in the first fight,
    // so unlike every other boss team his authored minions stay out of it.
    dungeon::EnemyTeam team;
    team.isBoss = true;
    team.bossId = kDuckBossId;
    team.statScalePct = kGooseTownScalePct;
    if (const content::BossDef* b = content.findBoss(kDuckBossId)) {
        team.name = b->name;
    }
    return team;
}

}  // namespace cd
