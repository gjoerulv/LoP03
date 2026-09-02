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
inline constexpr const char* kFirstMarket = "first_market";    // M34 black market
inline constexpr const char* kFirstCastle = "first_castle";        // M40 castle
inline constexpr const char* kFirstChallenge = "first_challenge";  // M40 challenges
inline constexpr const char* kFirstRelic = "first_relic";          // M44 royal relics
inline constexpr const char* kCarriedOut = "carried_out";          // M89 defeat rework

struct Beat {
    const char* id;
    const char* title;
    const char* body;
};

// All shipped beats, in teaching order (content lint tests iterate this).
inline constexpr Beat kBeats[] = {
    {kTownWelcome, "Welcome to Are P Geese",  // M108: the rebrand
     "Walk with the movement keys. Step onto a doorway and press Confirm to "
     "enter a building. Runs begin at the Guild - but the shops, the "
     "Training Hall, and the Inn can make your first one easier."},
    {kGuildPrepare, "The Guild",
     // M99 truth pass: the Floors picker (M82/M92) joined the preparation row.
     "Choose a theme, a depth, floors, and a seed, then enter the dungeon. "
     "Runs are ranked by completion first, then by fewest battle turns - "
     "decisive play beats cautious grinding. Retreating, or escaping the "
     "boss, forfeits the run's score."},
    {kDungeonFirst, "Into the dungeon",
     // M99 truth pass: "nothing ambushes you" died with the M93 patrols —
     // the counter IS the forewarning, so teach it instead of denying it.
     "Enemy teams hold their ground and are always visible - but the Patrol "
     "counter ticks down as you walk, and at zero a roused patrol attacks "
     "at once. Face a team to read its danger in the footer before you "
     "commit. Gate teams must be fought to reach the boss; side rooms are "
     "optional. Press Menu to retreat or check your party."},
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
     // M99 truth pass: the M32 town tag joined depth and level.
     "Your score counts completion, battle turns, optional danger defeated, "
     "treasure, boss and no-death bonuses - and any omen wager. The "
     "Scoreboard compares runs at the same town, depth and level."},
    {kTownReturn, "Back in town",
     // M99 truth pass: the Inn has charged gold since M30 (rest tokens are
     // the free path) — the owner caught the prompt still promising free.
     "Spend your spoils: the shops sell gear and supplies, the Training "
     "Hall levels a character instantly for gold, and the Inn heals the "
     "party for gold - or free with a rest token earned in the dungeons. "
     "Deeper dungeons pay better - when you are ready."},
    {kFirstTravel, "The road onward",
     // M99 truth pass: M50 moved travel to east/west edge walk-throughs —
     // the "bottom" roads this text described no longer exist.
     "Towns form a chain. Walk off the eastern edge to reach the next town "
     "- tougher foes, but a higher score bonus on every run. The western "
     "edge returns the way you came; no key press needed, just walk "
     "through. Clear one dungeon in a town to open the road onward."},
    {kFirstPenalty, "Raising the stakes",
     "A run that does not raise the stakes loses score - another 30% each "
     "time, down to a 99% floor. Raise the town or the depth above your last "
     "cleared run to reset it. The Guild shows the penalty before you enter, "
     "so you are never surprised."},
    {kFirstMarket, "The black market",
     "A hooded dealer sometimes appears after a stakes-raising run, selling one "
     "legendary piece for gold or for legendary tokens - which you earn from "
     "optional elite challenges in dungeons. Buy it or walk away; the offer "
     "keeps until you do."},
    {kFirstCastle, "The castle",
     // M99 truth pass: M85 made it four — the Dragon waits behind the curios,
     // named only as a rumor here (the prompt teaches, it does not spoil).
     "Above the seven towns stands the castle. Here wait the King's "
     "challenges - the Boss Rush, the Endless Rush, the Hollow King himself, "
     "and one more legend for those who gather what the dungeons hide. The "
     "castle keeps its own records, apart from your dungeon scores."},
    {kFirstChallenge, "The King's challenges",
     "A challenge gives NO free healing between fights - bring items and spend "
     "them wisely. Each pays a one-time reward the first time you clear it; the "
     "King grants a unique legendary and a title. Fall, and no gold is taken - "
     "but you are left at the gates at 1 HP, and the fallen stay fallen."},
    {kFirstRelic, "A Royal Relic",
     "Reliquaries are rare, and nobody alive agrees on what the relics inside "
     "actually DO. A goose. Tax sheets. A spoon. A crown. Each works exactly "
     "once, on one enemy, in battle - beyond that, all anyone has is the "
     "storytellers' ballad. Listen to every verse. And mind the spoon. "
     "Everyone says to mind the spoon. Nobody says why."},
    {kCarriedOut, "Carried out",
     "Defeat in a dungeon no longer mends the party. One member staggers back "
     "to town at 1 HP, the fallen stay fallen, MP keeps whatever remained - "
     "and half your gold is gone. The Inn restores everyone, and a Phoenix "
     "Tear can raise the fallen on the road."},
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
