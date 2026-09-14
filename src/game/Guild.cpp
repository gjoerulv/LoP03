#include "game/Guild.hpp"

#include <algorithm>

#include "content/ContentDatabase.hpp"
#include "content/Definitions.hpp"

namespace cd {

int guildScalePct(const content::ContentDatabase& content, int town) {
    // The same two rules the generator composes: the depth curve at the
    // gauntlet's fixed threat depth, then the town ladder. Town 7 lands on the
    // ladder's own 570 % ceiling — the guild is the town's summit, not the
    // castle's (whose floor sits above it by design).
    const content::CompositionDef& comp = content.composition();
    return combineTownScale(100 + comp.statScalePct(kGuildThreatDepth), clampTown(town));
}

const content::BossDef* findGuildMaster(const content::ContentDatabase& content, int town) {
    for (const auto& [id, def] : content.bosses()) {
        (void)id;
        if (def.guildTown == town) {
            return &def;
        }
    }
    return nullptr;
}

std::vector<std::string> guildTownPool(const content::ContentDatabase& content, int town) {
    std::vector<std::string> pool;
    for (const auto& [id, def] : content.enemies()) {
        // The endless-pool rule (bossOnly courts stay with their bosses),
        // town-gated the way the generator gates spawns (M38).
        if (def.bossOnly || def.specialOnly || def.minTown > town) {  // M111
            continue;
        }
        pool.push_back(id);
    }
    std::sort(pool.begin(), pool.end());
    return pool;
}

dungeon::EnemyTeam guildWaveTeam(const content::ContentDatabase& content, int town) {
    dungeon::EnemyTeam team;
    const std::vector<std::string> pool = guildTownPool(content, town);
    if (pool.empty()) {
        return team;
    }
    // A pure hash of the fixed guild seed: every attempt at this town fields
    // the same trial, so the best-turns record measures the party, not the
    // draw (the kEndlessSeed philosophy).
    const std::uint64_t base = static_cast<std::uint64_t>(clampTown(town)) * 1000u;
    for (int i = 0; i < kGuildWaveSize; ++i) {
        const std::uint64_t h = blackMarketHash(kGuildSeed, base + static_cast<std::uint64_t>(i));
        team.enemyIds.push_back(pool[static_cast<std::size_t>(h % pool.size())]);
    }
    team.statScalePct = guildScalePct(content, town);
    team.name = "The Guild Trial";
    return team;
}

dungeon::EnemyTeam guildMasterTeam(const content::ContentDatabase& content, int town) {
    dungeon::EnemyTeam team;
    const content::BossDef* master = findGuildMaster(content, town);
    if (master == nullptr) {
        return team;  // no Master shipped for this town: the runner ends the gauntlet
    }
    team.isBoss = true;
    team.bossId = master->id;
    team.name = master->name;
    team.enemyIds = master->minions;  // the Master brings its authored court
    team.statScalePct = guildScalePct(content, town);
    return team;
}

void applyGuildRelicOmen(std::vector<dungeon::Dungeon>& floors, std::uint64_t runSeed,
                         int omenPct) {
    if (omenPct <= 0) {
        return;
    }
    for (dungeon::Dungeon& d : floors) {
        // At most one relic event per dungeon (the M44 rule) — a floor that
        // already rolled one keeps it and the omen stays quiet.
        bool hasRelic = false;
        for (const dungeon::Room& room : d.rooms) {
            if (room.event.kind == dungeon::RoomEventKind::RoyalRelic) {
                hasRelic = true;
                break;
            }
        }
        if (hasRelic) {
            continue;
        }
        const std::uint64_t h = blackMarketHash(
            runSeed, kGuildOmenSalt + static_cast<std::uint64_t>(d.floorIndex));
        if (static_cast<int>(h % 100) >= omenPct) {
            continue;
        }
        for (dungeon::Room& room : d.rooms) {
            // Only a PLAIN rolled event may carry the omen: never a theme
            // rite, the Duckling Peddler, or an elite challenge (whose team
            // would be orphaned) — the peddler's own eligibility rule. The
            // relic itself is picked at resolution (M44), so the rolled
            // event's baked fields simply go unread.
            switch (room.event.kind) {
                case dungeon::RoomEventKind::Shrine:
                case dungeon::RoomEventKind::HealingSpring:
                case dungeon::RoomEventKind::Merchant:
                case dungeon::RoomEventKind::ScoreWager:
                case dungeon::RoomEventKind::RestToken:
                    break;
                default:
                    continue;
            }
            room.event.kind = dungeon::RoomEventKind::RoyalRelic;
            break;  // one upgrade per floor
        }
    }
}

}  // namespace cd
