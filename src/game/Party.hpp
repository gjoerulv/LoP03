#pragma once

#include <cstddef>
#include <vector>

#include "content/Definitions.hpp"
#include "game/Character.hpp"
#include "game/Inventory.hpp"

namespace cd {

namespace content {
class ContentDatabase;
}


struct Party {
    std::vector<Character> members;
    Inventory inventory;
    int gold = 0;
    int restTokens = 0;  // free-rest tokens from dungeon events (M30)

    bool empty() const { return members.empty(); }
    std::size_t size() const { return members.size(); }
};

inline constexpr std::size_t kMaxPartySize = 4;

// Provisional MP pool until the combat milestone refines it: scales with magic.
int deriveMaxMp(int magic);

// Recomputes stats/maxHp/maxMp from the class at the character's current level,
// then clamps hp/mp into range. Use after loading or leveling.
void recomputeDerivedStats(Character& character, const content::ClassDef& cls);

// Recomputes stats from the class AND equipped items (looked up in the db), then
// clamps hp/mp. Use after equipment changes or on load.
void refreshCharacter(Character& character, const content::ContentDatabase& db);

// Creates a fresh, full-health character of the given class.
Character createCharacter(const content::ClassDef& cls, std::string name, int level = 1);

// Restores every member to full HP/MP.
void healFull(Party& party);

// Paid rest (M30). The inn cost scales with the highest party level so a full
// rest stays a real decision as income grows: kBase + kPerLevel*(level-1),
// clamped to [kBase, kMax]. Constants live here for balance tuning.
inline constexpr int kRestCostBase = 20;
inline constexpr int kRestCostPerLevel = 12;
inline constexpr int kRestCostMax = 500;
int restCost(const Party& party);

// Highest level among living/all members (0 if empty) — used for save summaries.
int highestLevel(const Party& party);

inline constexpr int kMaxLevel = 50;

// XP required to advance FROM `level` to the next level.
int xpToNext(int level);

// Adds XP to a character, leveling up (recomputing stats and healing the maxHP
// gained) as needed. Grants to the whole party with grantPartyXp.
void grantXp(Character& character, int xp, const content::ContentDatabase& db);
void grantPartyXp(Party& party, int xp, const content::ContentDatabase& db);

}  // namespace cd
