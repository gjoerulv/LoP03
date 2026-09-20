#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#include <nlohmann/json_fwd.hpp>

#include "content/LoadReport.hpp"

namespace cd {

struct Party;
namespace content {
class ContentDatabase;
}

namespace save {

inline constexpr int kSaveVersion = 1;

// Five manual slots plus a dedicated autosave slot (M53; was three). Autosave is
// written on dungeon entry; loading any slot always places the party in town.
// The count grew by adding new slot filenames, not by changing the per-file
// schema, so kSaveVersion is unchanged and old slot files load untouched.
enum class SaveSlot { Auto, Manual1, Manual2, Manual3, Manual4, Manual5 };
inline constexpr int kSaveSlotCount = 6;

const char* slotFileStem(SaveSlot slot);
const char* slotDisplayName(SaveSlot slot);

struct SlotSummary {
    int partySize = 0;
    int highestLevel = 0;
    int gold = 0;
    std::string kingTitle;  // M40: the earned King title, shown on the load screen
    // M123: the party's total play time (the M109 ledger's counter; a save
    // from before it reads 0). Drawn by game/PlayTime.hpp's formatter.
    long long playSeconds = 0;
    // M127: who is in the party, in party order - the slot rows draw the
    // members' sprites (a fallen member lies down, as on the victory screen).
    struct Member {
        std::string classId;
        bool fallen = false;
    };
    std::vector<Member> members;
};

// Reads/writes versioned JSON saves under a directory (default: the user data
// dir). All loading is defensive: malformed/foreign saves are reported, never
// crash, and never partially corrupt the target party.
class SaveSystem {
public:
    SaveSystem(const content::ContentDatabase& db, std::filesystem::path directory);

    std::filesystem::path slotPath(SaveSlot slot) const;
    bool exists(SaveSlot slot) const;

    // M123: an Iron Man party (Party::ironMan) is refused here - the one choke
    // point every manual save and the autosave pass through.
    bool save(SaveSlot slot, const Party& party, content::LoadReport& report) const;
    bool autosave(const Party& party, content::LoadReport& report) const;
    bool load(SaveSlot slot, Party& outParty, content::LoadReport& report) const;

    // M124: the slot codec without the slot - exactly the text `save` writes
    // and exactly the validation `load` applies (`source` names the origin in
    // reports). The fallen-run store snapshots an Iron Man party with these;
    // `serialize` has no Iron Man refusal, which belongs to `save` alone.
    std::string serialize(const Party& party) const;
    bool parseText(const std::string& text, const std::string& source, Party& outParty,
                   content::LoadReport& report) const;

    // Lightweight info for menus; std::nullopt if the slot is missing or invalid.
    std::optional<SlotSummary> summary(SaveSlot slot) const;

private:
    bool parseRoot(const nlohmann::json& root, const std::string& source, Party& outParty,
                   content::LoadReport& report) const;

    const content::ContentDatabase& db_;
    std::filesystem::path dir_;
};

}  // namespace save
}  // namespace cd
