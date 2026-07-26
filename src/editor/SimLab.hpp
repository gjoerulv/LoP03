#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "content/ContentDatabase.hpp"
#include "editor/BattleRecorder.hpp"
#include "editor/EditorValidation.hpp"

// M60 — the sim lab's pure model: a party spec + an opponent spec swept over N
// seeds through the REAL battle::simulate, aggregated with per-skill /
// per-combatant telemetry from the record-only observer. No raylib; the shell
// renders it, tests drive it headlessly. Deterministic: the same config over
// the same content produces the same result, every time.

namespace cd::editor {

struct SimMemberSpec {
    std::string classId;
    std::string weapon;   // item ids; "" = bare slot
    std::string armor;
    std::string accessory;
    std::string passive;  // equipped passive id; "" = none
};

enum class OpponentMode { Manual, Boss, BossRush, King, Endless };

struct SimLabConfig {
    std::vector<SimMemberSpec> members;  // <= 4; empty classId rows are skipped
    int level = 1;

    OpponentMode mode = OpponentMode::Manual;
    std::vector<std::string> enemyIds;  // Manual
    std::string bossId;                 // Boss (its authored minions come along)
    int rushIndex = 0;                  // BossRush
    int endlessWave = 1;                // Endless
    int statScalePct = 100;             // Manual/Boss (castle modes use their own)

    int seeds = 100;  // 10 / 100 / 1000
    std::uint64_t baseSeed = 0xC0FFEE60ull;
};

struct SimLabResult {
    bool ok = false;
    std::string error;  // when !ok: what was missing (class/boss/enemy id)

    int runs = 0;
    int wins = 0;
    int defeats = 0;
    int stalls = 0;  // hit the round cap
    int minRounds = 0;
    int maxRounds = 0;
    double avgRounds = 0.0;
    double medianRounds = 0.0;
    double avgHpFraction = 0.0;  // party HP remaining, averaged over runs
    int partyKos = 0;            // total party falls across the sweep
    std::string dangerTier;      // danger::tierName of the opponent

    BattleRecorder telemetry;  // merged across every run

    double winRatePct() const { return runs > 0 ? 100.0 * wins / runs : 0.0; }
};

// Fills a member row from the shared deterministic pickers (presets).
SimMemberSpec presetMember(const content::ContentDatabase& db, const std::string& classId,
                           GearTier tier);

// Builds the config's opponent team ("" error on success).
dungeon::EnemyTeam buildOpponent(const SimLabConfig& config, const content::ContentDatabase& db,
                                 std::string& error);

// Runs the sweep. Synchronous; a 1000-seed sweep is a progress-bar moment,
// not a coffee break (the shell shows a live counter via the callback,
// which may be null).
using SweepProgress = void (*)(int done, int total, void* user);
SimLabResult runSweep(const SimLabConfig& config, const content::ContentDatabase& db,
                      SweepProgress progress = nullptr, void* user = nullptr);

// Report rendering (markdown table / CSV rows); `previous` adds delta columns.
std::string reportMarkdown(const SimLabConfig& config, const SimLabResult& result,
                           const SimLabResult* previous);
std::string reportCsv(const SimLabConfig& config, const SimLabResult& result);

}  // namespace cd::editor
