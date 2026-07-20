#pragma once

#include <cstdint>
#include <string>

namespace cd::score {

// One recorded dungeon run on the scoreboard.
struct ScoreEntry {
    int score = 0;
    int battleTurns = 0;
    int dangerDefeated = 0;
    int chestsOpened = 0;
    bool noDeath = false;
    int depth = 1;
    std::string theme;
    std::uint64_t seed = 0;
    // Room-realization version the run was played under (see
    // dungeon::kGenerationVersion). 0 = recorded before versioning existed
    // (pre-M16). Optional in the file; the scoreboard format version is
    // unchanged so old files load as-is (owner decision, 2026-07-19).
    int generationVersion = 0;
    // Highest party level at completion (M19 comparability tag; owner
    // decision 2026-07-19). 0 = legacy entry, shown as "-". Optional in the
    // file, no format bump. Ranking never uses it — it makes comparison
    // conditions visible, not normalized.
    int partyLevel = 0;
    // Battle-rules version the run was played under (see
    // battle::kBattleRulesVersion). 0 = recorded before the M28 enmity/AI
    // change, when battles resolved differently. Optional in the file, no format
    // bump; a comparability tag like the two above, never used for ranking.
    int battleRulesVersion = 0;
    // Town-ladder index the run was cleared in (M32). 0 = legacy entry recorded
    // before the ladder existed, displayed as town 1. Optional in the file, no
    // format bump; shown for context, never used for ranking.
    int townIndex = 0;
};

}  // namespace cd::score
