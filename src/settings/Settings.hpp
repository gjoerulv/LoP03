#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>

#include "content/LoadReport.hpp"
#include "input/InputMap.hpp"

// Persistent player settings (M13): a versioned settings.json in the user data
// dir holding audio volumes, window mode, gameplay speeds, and the remappable
// input bindings. Loading is defensive: a missing, malformed, or
// foreign-version file yields defaults (reported, never a crash), and a
// binding set that would strand the player is restored to defaults per
// action. Parse/serialize work on strings so tests run headlessly.

namespace cd::settings {

inline constexpr int kSettingsVersion = 1;

enum class BattleSpeed { Normal, Fast, Instant };
enum class MessageSpeed { Slow, Normal, Fast };

std::string_view battleSpeedName(BattleSpeed s);
std::optional<BattleSpeed> battleSpeedFromName(std::string_view name);
std::string_view messageSpeedName(MessageSpeed s);
std::optional<MessageSpeed> messageSpeedFromName(std::string_view name);

// Seconds a battle action's resolve pause lasts (Confirm always skips it).
float resolveSeconds(BattleSpeed s);
// Multiplier applied to transient on-screen message durations.
float messageDurationScale(MessageSpeed s);

struct Settings {
    float masterVolume = 1.0f;  // 0..1
    float musicVolume = 1.0f;   // 0..1
    float sfxVolume = 1.0f;     // 0..1
    bool borderlessFullscreen = false;
    BattleSpeed battleSpeed = BattleSpeed::Normal;
    MessageSpeed messageSpeed = MessageSpeed::Normal;
};

// In-memory parse/serialize (exposed for headless tests). parse fills
// `values` and applies valid bindings into `map`; on any structural problem
// it reports, keeps defaults for the broken part, and returns false only for
// unusable input (bad JSON / wrong version / not an object).
bool parseSettingsText(const std::string& text, Settings& values, InputMap& map,
                       content::LoadReport& report);
std::string serializeSettings(const Settings& values, const InputMap& map);

class SettingsStore {
public:
    explicit SettingsStore(std::filesystem::path file);

    Settings values;

    // Missing file: silent defaults (returns true). Malformed/foreign
    // version: defaults + report (returns false).
    bool load(InputMap& map, content::LoadReport& report);
    bool save(const InputMap& map, content::LoadReport& report) const;

    const std::filesystem::path& file() const { return file_; }

private:
    std::filesystem::path file_;
};

}  // namespace cd::settings
