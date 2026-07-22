#pragma once

#include <filesystem>
#include <string>

#include "content/LoadReport.hpp"

// Cross-save player profile (M45): the handful of facts that belong to the
// PLAYER rather than to a party. Right now that is exactly one — whether the
// Hollow King has ever been beaten, which unlocks the three reward classes at
// character creation.
//
// It cannot live on Party: a New Game resets the party before a class is picked,
// so a per-save `castleRecords.kingDefeated` could never gate the class list.
// The file follows the AchievementStore pattern (versioned JSON in the user data
// dir, defensive load, atomic save): a missing or malformed file means "nothing
// unlocked", never a crash.

namespace cd {

inline constexpr int kProfileVersion = 1;

struct ProfileData {
    bool kingDefeated = false;  // unlocks Dragon / Jester / Goose

    bool classesUnlocked() const { return kingDefeated; }
};

// In-memory parse/serialize (exposed for headless tests). Malformed or
// foreign-version input reports, leaves a fresh default profile, returns false.
bool parseProfileText(const std::string& text, ProfileData& data,
                      content::LoadReport& report);
std::string serializeProfile(const ProfileData& data);

class ProfileStore {
public:
    explicit ProfileStore(std::filesystem::path file);

    ProfileData data;

    bool load(content::LoadReport& report);  // missing file -> fresh state (true)
    bool save(content::LoadReport& report) const;

    // Records a King kill. Returns true when this CHANGED the profile (so the
    // caller can announce the unlock once); saves only then.
    bool recordKingDefeated();

    bool classesUnlocked() const { return data.classesUnlocked(); }

private:
    std::filesystem::path file_;
};

}  // namespace cd
