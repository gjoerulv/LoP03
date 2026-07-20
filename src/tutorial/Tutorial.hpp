#pragma once

#include <filesystem>
#include <set>
#include <string>
#include <string_view>

#include "content/LoadReport.hpp"

// One-time contextual onboarding (M22, owner-approved flow): each beat is a
// small dismissible prompt shown the first time its concept is encountered,
// never again. Progress persists in a versioned tutorial.json beside
// settings.json; parsing is defensive — a missing, malformed, or
// foreign-version file yields a fresh state (reported, never a crash) and
// never re-locks progress the player already made in the same session.
// Parse/serialize work on strings so tests run headlessly.

namespace cd::tutorial {

inline constexpr int kTutorialVersion = 1;

// Stable beat ids (persisted in "seen"; do not rename once shipped).
inline constexpr const char* kTownWelcome = "town_welcome";
inline constexpr const char* kGuildPrepare = "guild_prepare";
inline constexpr const char* kDungeonFirst = "dungeon_first";
inline constexpr const char* kBattleFirst = "battle_first";
inline constexpr const char* kChestGuarded = "chest_guarded";
inline constexpr const char* kEventFirst = "event_first";
inline constexpr const char* kVictoryFirst = "victory_first";
inline constexpr const char* kResultFirst = "result_first";
inline constexpr const char* kTownReturn = "town_return";
inline constexpr const char* kFirstTravel = "first_travel";    // M32 town ladder
inline constexpr const char* kFirstPenalty = "first_penalty";  // M33 stakes penalty

struct Beat {
    const char* id;
    const char* title;
    const char* body;
};

// All shipped beats, in teaching order (content lint tests iterate this).
inline constexpr Beat kBeats[] = {
    {kTownWelcome, "Welcome to Crystal Dungeons",
     "Walk with the movement keys. Step onto a doorway and press Confirm to "
     "enter a building. Runs begin at the Guild - but the shops, the "
     "Training Hall, and the Inn can make your first one easier."},
    {kGuildPrepare, "The Guild",
     "Choose a theme, a depth, and a seed, then enter the dungeon. Runs are "
     "ranked by completion first, then by fewest battle turns - decisive "
     "play beats cautious grinding. Retreating, or escaping the boss, "
     "forfeits the run's score."},
    {kDungeonFirst, "Into the dungeon",
     "Enemy teams are always visible - nothing ambushes you here. Face a "
     "team to read its danger in the footer before you commit. Gate teams "
     "must be fought to reach the boss; side rooms are optional. Press Menu "
     "to retreat or check your party."},
    {kBattleFirst, "Battle",
     "Turn order follows Speed. Attack, cast skills, use items, Guard to "
     "brace, or Escape - though escaping forfeits whatever the team was "
     "guarding. Every battle turn counts against your score, so finish "
     "fights decisively. The Details key explains any unit's stats and "
     "statuses."},
    {kChestGuarded, "Guarded treasure",
     "The best chests are guarded: the footer shows the guard's danger and "
     "the chest's rarity before you fight, and escaping the guard forfeits "
     "the chest. Unguarded chests can be trapped - a red tint and the "
     "footer warn you before you open one."},
    {kEventFirst, "Dungeon events",
     "Some side rooms hold shrines, springs, merchants, challenges, or "
     "omens. The full trade-off is always written in the footer before you "
     "press Confirm - and none of them are mandatory."},
    {kVictoryFirst, "Victory",
     "Battles pay experience and gold, and early levels come quickly. "
     "Remember: the score rewards fewest total battle turns, so a quick "
     "overwhelming win is worth more than a long safe one."},
    {kResultFirst, "The reckoning",
     "Your score counts completion, battle turns, optional danger defeated, "
     "treasure, boss and no-death bonuses - and any omen wager. The "
     "Scoreboard compares runs at the same depth and level."},
    {kTownReturn, "Back in town",
     "Spend your spoils: the shops sell gear and supplies, the Training "
     "Hall levels a character instantly for gold, and the Inn heals for "
     "free. Deeper dungeons pay better - when you are ready."},
    {kFirstTravel, "The road onward",
     "Towns form a chain. The road at the bottom right leads to the next "
     "town - tougher foes, but a higher score bonus on every run. The road "
     "at the bottom left returns the way you came. Clear one dungeon in a "
     "town to open the road onward from it."},
    {kFirstPenalty, "Raising the stakes",
     "A run that does not raise the stakes loses score - another 15% each "
     "time, down to a 90% floor. Raise the town or the depth above your last "
     "cleared run to reset it. The Guild shows the penalty before you enter, "
     "so you are never surprised."},
};
inline constexpr std::size_t kBeatCount = sizeof(kBeats) / sizeof(kBeats[0]);

const Beat* findBeat(std::string_view id);

struct Progress {
    bool enabled = true;
    std::set<std::string> seen;
};

// In-memory parse/serialize (exposed for headless tests). parse fills `p`;
// malformed or foreign-version input reports, leaves a fresh default state,
// and returns false.
bool parseTutorialText(const std::string& text, Progress& p, content::LoadReport& report);
std::string serializeTutorial(const Progress& p);

class TutorialStore {
public:
    explicit TutorialStore(std::filesystem::path file);

    Progress state;

    // Missing file: silent fresh state (returns true). Malformed/foreign
    // version: fresh state + report (returns false).
    bool load(content::LoadReport& report);
    bool save(content::LoadReport& report) const;

    // True when the beat should fire now; marks it seen and saves, so a
    // prompt shows at most once even across crashes.
    bool takeBeat(const std::string& id);

    // Clears seen beats (keeps the enabled flag) and saves.
    void reset();

    const std::filesystem::path& file() const { return file_; }

private:
    std::filesystem::path file_;
};

}  // namespace cd::tutorial
