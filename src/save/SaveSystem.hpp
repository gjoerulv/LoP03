#pragma once

#include <filesystem>
#include <optional>
#include <string>

#include "content/LoadReport.hpp"

namespace cd {

struct Party;
namespace content {
class ContentDatabase;
}

namespace save {

inline constexpr int kSaveVersion = 1;

// Three manual slots plus a dedicated autosave slot. Autosave is written on
// dungeon entry (wired in the dungeon milestone); loading any slot always places
// the party in town.
enum class SaveSlot { Auto, Manual1, Manual2, Manual3 };
inline constexpr int kSaveSlotCount = 4;

const char* slotFileStem(SaveSlot slot);
const char* slotDisplayName(SaveSlot slot);

struct SlotSummary {
    int partySize = 0;
    int highestLevel = 0;
    int gold = 0;
};

// Reads/writes versioned JSON saves under a directory (default: the user data
// dir). All loading is defensive: malformed/foreign saves are reported, never
// crash, and never partially corrupt the target party.
class SaveSystem {
public:
    SaveSystem(const content::ContentDatabase& db, std::filesystem::path directory);

    std::filesystem::path slotPath(SaveSlot slot) const;
    bool exists(SaveSlot slot) const;

    bool save(SaveSlot slot, const Party& party, content::LoadReport& report) const;
    bool autosave(const Party& party, content::LoadReport& report) const;
    bool load(SaveSlot slot, Party& outParty, content::LoadReport& report) const;

    // Lightweight info for menus; std::nullopt if the slot is missing or invalid.
    std::optional<SlotSummary> summary(SaveSlot slot) const;

private:
    const content::ContentDatabase& db_;
    std::filesystem::path dir_;
};

}  // namespace save
}  // namespace cd
