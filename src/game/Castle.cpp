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
        if (id == kKingBossId) {
            continue;  // the King is its own challenge, not part of the rush roster
        }
        if (id == kDuckBossId) {
            continue;  // M61: the Duck has his own pond, same rule as the King
        }
        if (id == kDragonBossId) {
            continue;  // M85: the Dragon waits behind twelve curios, nowhere else
        }
        if (def.guildTown != 0) {
            continue;  // M84: a Guild Master presides over its town's gauntlet
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
        // the whole rush roster rather than a rush-specific one.
        team.enemyIds = b->minions;
    }
    return team;
}

dungeon::EnemyTeam endlessWaveTeam(const content::ContentDatabase& content, int wave) {
    dungeon::EnemyTeam team;
    const int w = wave < 0 ? 0 : wave;
    // M84: every 10th wave (10, 20, 30, ...) the rush is interrupted by a
    // boss — a dungeon boss or a Guild Master — bringing its usual minions.
    // Drawn from the same fixed seed on a salt far above the per-slot ones,
    // so the sequence stays reproducible and every other wave is untouched.
    if ((w + 1) % 10 == 0) {
        std::vector<std::string> bossPool = bossRushOrder(content);
        for (const auto& [id, def] : content.bosses()) {
            if (def.guildTown != 0) {
                bossPool.push_back(id);
            }
        }
        std::sort(bossPool.begin(), bossPool.end());
        if (!bossPool.empty()) {
            const std::uint64_t h = blackMarketHash(
                kEndlessSeed, 0xB055000000ull + static_cast<std::uint64_t>(w));
            const std::string& id = bossPool[static_cast<std::size_t>(h % bossPool.size())];
            team.isBoss = true;
            team.bossId = id;
            team.statScalePct = endlessWaveScalePct(w);
            team.name = "Wave " + std::to_string(w + 1);
            if (const content::BossDef* b = content.findBoss(id)) {
                team.name = b->name;
                team.enemyIds = b->minions;
            }
            return team;
        }
        // A content set with no bosses at all: fall through to a plain wave.
    }
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

dungeon::EnemyTeam dragonEliteWaveTeam(const content::ContentDatabase& content, int wave) {
    // M85: the whole ELITE roster (never a bossOnly court — those belong to
    // their bosses), seeded from the fixed kDragonSeed so every attempt runs
    // the same three waves.
    dungeon::EnemyTeam team;
    if (wave < 0 || wave >= kDragonWaveCount) {
        return team;
    }
    std::vector<std::string> pool;
    for (const auto& [id, def] : content.enemies()) {
        if (def.bossOnly || def.tier != content::EnemyTier::Elite) {
            continue;
        }
        pool.push_back(id);
    }
    std::sort(pool.begin(), pool.end());
    if (pool.empty()) {
        return team;
    }
    const std::uint64_t base = static_cast<std::uint64_t>(wave) * 1000u;
    for (int i = 0; i < kDragonWaveSize; ++i) {
        const std::uint64_t h = blackMarketHash(kDragonSeed, base + static_cast<std::uint64_t>(i));
        team.enemyIds.push_back(pool[static_cast<std::size_t>(h % pool.size())]);
    }
    team.statScalePct = kDragonScalePct;
    team.name = "The Dragon's Vigil " + std::to_string(wave + 1);
    return team;
}

dungeon::EnemyTeam dragonTeam(const content::ContentDatabase& content) {
    // M85: the Dragon fights alone — the vigil waves were the only court it
    // ever needed (the Duck's shape).
    dungeon::EnemyTeam team;
    team.isBoss = true;
    team.bossId = kDragonBossId;
    team.statScalePct = kDragonScalePct;
    if (const content::BossDef* b = content.findBoss(kDragonBossId)) {
        team.name = b->name;
    }
    return team;
}

}  // namespace cd
