#include "editor/SimLab.hpp"

#include <algorithm>
#include <cmath>

#include "battle/Battle.hpp"
#include "battle/Simulator.hpp"
#include "danger/DangerRating.hpp"
#include "dungeon/DungeonModel.hpp"
#include "game/Castle.hpp"
#include "game/Party.hpp"

namespace cd::editor {

namespace {

// Distinct, well-mixed per-run seeds from the base (splitmix increment).
constexpr std::uint64_t kSeedStep = 0x9E3779B97F4A7C15ull;

Party buildParty(const SimLabConfig& config, const content::ContentDatabase& db,
                 std::string& error) {
    Party party;
    for (const SimMemberSpec& spec : config.members) {
        if (spec.classId.empty()) {
            continue;
        }
        const content::ClassDef* cls = db.findClass(spec.classId);
        if (cls == nullptr) {
            error = "class '" + spec.classId + "' not found";
            return party;
        }
        Character c = createCharacter(*cls, cls->name, config.level);
        c.weapon = spec.weapon;
        c.armor = spec.armor;
        c.accessory = spec.accessory;
        if (!spec.passive.empty() && db.findPassive(spec.passive) != nullptr) {
            c.ownedPassives.push_back(spec.passive);
            c.equippedPassive = spec.passive;
        }
        refreshCharacter(c, db);
        party.members.push_back(std::move(c));
    }
    if (party.empty()) {
        error = "no party members configured";
    }
    healFull(party);
    return party;
}

}  // namespace

SimMemberSpec presetMember(const content::ContentDatabase& db, const std::string& classId,
                           GearTier tier) {
    SimMemberSpec spec;
    spec.classId = classId;
    const content::ClassDef* cls = db.findClass(classId);
    if (cls != nullptr) {
        spec.weapon = pickSimGear(db, *cls, content::EquipSlot::Weapon, tier);
        spec.armor = pickSimGear(db, *cls, content::EquipSlot::Armor, tier);
        spec.accessory = pickSimGear(db, *cls, content::EquipSlot::Accessory, tier);
    }
    return spec;
}

dungeon::EnemyTeam buildOpponent(const SimLabConfig& config, const content::ContentDatabase& db,
                                 std::string& error) {
    dungeon::EnemyTeam team;
    switch (config.mode) {
        case OpponentMode::Manual: {
            team.name = "Custom team";
            for (const std::string& id : config.enemyIds) {
                if (db.findEnemy(id) == nullptr) {
                    error = "enemy '" + id + "' not found";
                    return team;
                }
                team.enemyIds.push_back(id);
            }
            if (team.enemyIds.empty()) {
                error = "no enemies configured";
                return team;
            }
            team.statScalePct = config.statScalePct;
            break;
        }
        case OpponentMode::Boss: {
            const content::BossDef* boss = db.findBoss(config.bossId);
            if (boss == nullptr) {
                error = "boss '" + config.bossId + "' not found";
                return team;
            }
            team.name = boss->name;
            team.isBoss = true;
            team.bossId = boss->id;
            team.enemyIds = boss->minions;
            team.statScalePct = config.statScalePct;
            break;
        }
        case OpponentMode::BossRush: {
            team = bossRushTeam(db, config.rushIndex);
            if (team.bossId.empty()) {
                error = "no rush boss at index " + std::to_string(config.rushIndex);
            }
            break;
        }
        case OpponentMode::King: {
            if (db.findBoss(kKingBossId) == nullptr) {
                error = std::string("boss '") + kKingBossId + "' not found";
                return team;
            }
            team = kingTeam(db);
            break;
        }
        case OpponentMode::Endless: {
            team = endlessWaveTeam(db, config.endlessWave);
            if (team.enemyIds.empty()) {
                error = "endless wave produced no team";
            }
            break;
        }
    }
    return team;
}

SimLabResult runSweep(const SimLabConfig& config, const content::ContentDatabase& db,
                      SweepProgress progress, void* user) {
    SimLabResult result;
    Party party = buildParty(config, db, result.error);
    if (!result.error.empty()) {
        return result;
    }
    const dungeon::EnemyTeam team = buildOpponent(config, db, result.error);
    if (!result.error.empty()) {
        return result;
    }
    result.dangerTier =
        danger::tierName(danger::assess(team, /*depth=*/std::max(1, config.level / 2), db));

    std::vector<int> rounds;
    double hpFractionSum = 0.0;
    const int total = std::max(1, config.seeds);
    for (int i = 0; i < total; ++i) {
        battle::Battle battle = battle::buildBattle(party, team, db);
        battle.rngSeed = config.baseSeed + static_cast<std::uint64_t>(i) * kSeedStep;
        BattleRecorder recorder;
        recorder.bind(battle);
        battle.observer = &recorder;
        const battle::SimResult sim = battle::simulateInPlace(battle, db);

        ++result.runs;
        if (sim.outcome == battle::Outcome::Victory) {
            ++result.wins;
        } else if (sim.outcome == battle::Outcome::Defeat) {
            ++result.defeats;
        } else {
            ++result.stalls;
        }
        rounds.push_back(sim.rounds);
        hpFractionSum += sim.partyHpFraction();
        for (std::size_t u = 0; u < recorder.units().size(); ++u) {
            if (recorder.units()[u].party) {
                result.partyKos += recorder.units()[u].kos;
            }
        }
        result.telemetry.mergeFrom(recorder);
        if (progress != nullptr) {
            progress(i + 1, total, user);
        }
    }

    std::sort(rounds.begin(), rounds.end());
    result.minRounds = rounds.front();
    result.maxRounds = rounds.back();
    double sum = 0.0;
    for (int r : rounds) {
        sum += r;
    }
    result.avgRounds = sum / static_cast<double>(rounds.size());
    const std::size_t mid = rounds.size() / 2;
    result.medianRounds = rounds.size() % 2 == 1
                              ? rounds[mid]
                              : (rounds[mid - 1] + rounds[mid]) / 2.0;
    result.avgHpFraction = hpFractionSum / static_cast<double>(result.runs);
    result.ok = true;
    return result;
}

namespace {

std::string fixed1(double v) {
    const double r = std::round(v * 10.0) / 10.0;
    std::string s = std::to_string(r);
    const std::size_t dot = s.find('.');
    return dot != std::string::npos ? s.substr(0, dot + 2) : s;
}

std::string deltaCell(double now, double before) {
    const double d = std::round((now - before) * 10.0) / 10.0;
    if (d == 0.0) {
        return "=";
    }
    return (d > 0 ? "+" : "") + fixed1(d);
}

std::string describeOpponent(const SimLabConfig& config) {
    switch (config.mode) {
        case OpponentMode::Manual: {
            std::string out;
            for (const std::string& id : config.enemyIds) {
                out += (out.empty() ? "" : ", ") + id;
            }
            return out + " @" + std::to_string(config.statScalePct) + "%";
        }
        case OpponentMode::Boss:
            return config.bossId + " @" + std::to_string(config.statScalePct) + "%";
        case OpponentMode::BossRush:
            return "boss rush #" + std::to_string(config.rushIndex + 1);
        case OpponentMode::King:
            return "the King";
        case OpponentMode::Endless:
            return "endless wave " + std::to_string(config.endlessWave);
    }
    return "?";
}

}  // namespace

std::string reportMarkdown(const SimLabConfig& config, const SimLabResult& result,
                           const SimLabResult* previous) {
    std::string out = "# Sim sweep - " + describeOpponent(config) + "\n\n";
    out += "Party: level " + std::to_string(config.level) + " [";
    bool first = true;
    for (const SimMemberSpec& m : config.members) {
        if (m.classId.empty()) {
            continue;
        }
        out += (first ? "" : ", ") + m.classId;
        first = false;
    }
    out += "], " + std::to_string(result.runs) + " seeds, danger " + result.dangerTier + ".\n\n";
    out += "| metric | value |";
    out += previous != nullptr && previous->ok ? " delta |\n|---|---|---|\n" : "\n|---|---|\n";
    auto row = [&](const std::string& name, double value, double prev) {
        out += "| " + name + " | " + fixed1(value) + " |";
        if (previous != nullptr && previous->ok) {
            out += " " + deltaCell(value, prev) + " |";
        }
        out += "\n";
    };
    const SimLabResult& p = previous != nullptr ? *previous : result;
    row("win rate %", result.winRatePct(), p.winRatePct());
    row("avg rounds", result.avgRounds, p.avgRounds);
    row("median rounds", result.medianRounds, p.medianRounds);
    row("min rounds", result.minRounds, p.minRounds);
    row("max rounds", result.maxRounds, p.maxRounds);
    row("avg party HP %", result.avgHpFraction * 100.0, p.avgHpFraction * 100.0);
    row("party KOs", result.partyKos, p.partyKos);

    out += "\n## Actions\n\n| action | uses | damage | healing | avg/use |\n|---|---|---|---|---|\n";
    for (const auto& [id, tally] : result.telemetry.actions()) {
        const long total = tally.damage + tally.healing;
        out += "| " + id + " | " + std::to_string(tally.uses) + " | " +
               std::to_string(tally.damage) + " | " + std::to_string(tally.healing) + " | " +
               fixed1(tally.uses > 0 ? static_cast<double>(total) / tally.uses : 0.0) + " |\n";
    }

    out += "\n## Combatants\n\n| unit | side | dealt | taken | healed | KOs |\n|---|---|---|---|---|---|\n";
    for (const UnitTally& u : result.telemetry.units()) {
        out += "| " + u.name + " | " + (u.party ? "party" : "enemy") + " | " +
               std::to_string(u.dealt) + " | " + std::to_string(u.taken) + " | " +
               std::to_string(u.healingReceived) + " | " + std::to_string(u.kos) + " |\n";
    }
    return out;
}

std::string reportCsv(const SimLabConfig& config, const SimLabResult& result) {
    std::string out = "metric,value\nopponent," + describeOpponent(config) + "\n";
    out += "level," + std::to_string(config.level) + "\n";
    out += "seeds," + std::to_string(result.runs) + "\n";
    out += "danger," + result.dangerTier + "\n";
    out += "win_rate_pct," + fixed1(result.winRatePct()) + "\n";
    out += "avg_rounds," + fixed1(result.avgRounds) + "\n";
    out += "median_rounds," + fixed1(result.medianRounds) + "\n";
    out += "min_rounds," + std::to_string(result.minRounds) + "\n";
    out += "max_rounds," + std::to_string(result.maxRounds) + "\n";
    out += "avg_party_hp_pct," + fixed1(result.avgHpFraction * 100.0) + "\n";
    out += "party_kos," + std::to_string(result.partyKos) + "\n";
    out += "\naction,uses,damage,healing\n";
    for (const auto& [id, tally] : result.telemetry.actions()) {
        out += id + "," + std::to_string(tally.uses) + "," + std::to_string(tally.damage) + "," +
               std::to_string(tally.healing) + "\n";
    }
    out += "\nunit,side,dealt,taken,healed,kos\n";
    for (const UnitTally& u : result.telemetry.units()) {
        out += u.name + "," + (u.party ? "party" : "enemy") + "," + std::to_string(u.dealt) +
               "," + std::to_string(u.taken) + "," + std::to_string(u.healingReceived) + "," +
               std::to_string(u.kos) + "\n";
    }
    return out;
}

}  // namespace cd::editor
