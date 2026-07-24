#pragma once

#include <filesystem>
#include <set>
#include <string>
#include <vector>

#include "content/LoadReport.hpp"

// Achievements (M42): a fixed table of original, cross-game goals, persisted
// globally (tutorial.json-style) in achievements.json. Presentation/persistence
// only — no battle/generation/scoring surface. Predicates read persistent party
// state (+ a just-completed run for the run-quality ones); ids are stable once
// shipped.

namespace cd {

struct Party;

inline constexpr int kAchievementVersion = 1;

// M53 (owner decision 2026-07-24): Champion is no longer "beat the King at all"
// (that is Kingslayer) but "beat him efficiently". A party clears this when its
// persisted best King-fight turn count is at or under this bar. Tuned against the
// simulator (see the M53 note §J) and reported; changing it is a balance call.
inline constexpr int kChampionKingTurns = 15;

struct AchievementDef {
    const char* id;
    const char* name;
    const char* description;
};

inline constexpr AchievementDef kAchievements[] = {
    {"first_clear", "First Steps", "Clear your first dungeon."},
    {"trailblazer", "Trailblazer", "Reach the fourth town."},
    {"ladders_end", "Ladder's End", "Reach the seventh town."},
    {"untouchable", "Untouchable", "Clear a dungeon without a single death."},
    {"decisive", "Decisive", "Clear a dungeon in 20 battle turns or fewer."},
    {"deep_diver", "Deep Diver", "Clear a dungeon at depth 10 or deeper."},
    {"second_nature", "Second Nature", "Equip a passive skill on a hero."},
    {"high_roller", "High Roller", "Win a legendary token."},
    {"the_climb", "The Climb", "Open the road up to the castle."},
    {"kingslayer", "Kingslayer", "Defeat the Hollow King."},
    {"gauntlet", "Gauntlet", "Clear the castle's Boss Rush."},
    {"the_long_night", "The Long Night", "Reach wave 10 of the Endless Rush."},
    {"loremaster", "Loremaster", "Hear the whole Ballad of the Hollow King."},
    {"peerless", "Peerless", "Raise a hero to level 99."},
    {"champion", "Champion", "Defeat the Hollow King in 15 turns or fewer."},
    {"naturalist", "Naturalist", "Record 30 different foes in the bestiary."},
};
inline constexpr int kAchievementCount =
    static_cast<int>(sizeof(kAchievements) / sizeof(kAchievements[0]));

const AchievementDef* findAchievement(const std::string& id);

// Event context for the run-quality predicates (most read only party state).
struct AchvContext {
    bool clearedDungeon = false;  // a dungeon was just completed with a score
    bool runNoDeath = false;
    int runTurns = 0;
    int runDepth = 0;
};

// Is this achievement satisfied by the party's state (+ the run event)?
bool achievementMet(const std::string& id, const Party& party, const AchvContext& ctx);

// In-memory parse/serialize (exposed for headless tests). Malformed/foreign
// version -> fresh (empty) set + report, returns false.
bool parseAchievementsText(const std::string& text, std::set<std::string>& unlocked,
                           content::LoadReport& report);
std::string serializeAchievements(const std::set<std::string>& unlocked);

// Global store: a versioned achievements.json of unlocked ids (defensive load).
class AchievementStore {
public:
    explicit AchievementStore(std::filesystem::path file);

    std::set<std::string> unlocked;

    bool load(content::LoadReport& report);   // missing file -> fresh state (true)
    bool save(content::LoadReport& report) const;
    bool isUnlocked(const std::string& id) const { return unlocked.count(id) > 0; }

    // Evaluates every achievement against (party, ctx); records and returns the ids
    // NEWLY unlocked this call (for toasts), saving if any changed.
    std::vector<std::string> check(const Party& party, const AchvContext& ctx);

    const std::filesystem::path& file() const { return file_; }

private:
    std::filesystem::path file_;
};

}  // namespace cd
