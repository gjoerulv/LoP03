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
// Battle effect intensity (M18, owner-approved 2026-07-19): optional
// settings fields, absent = Full. Flash is a brighten pulse (never a
// strobe); shake is a small screen offset.
enum class EffectLevel { Full, Reduced, Off };

std::string_view battleSpeedName(BattleSpeed s);
std::optional<BattleSpeed> battleSpeedFromName(std::string_view name);
std::string_view messageSpeedName(MessageSpeed s);
std::optional<MessageSpeed> messageSpeedFromName(std::string_view name);
std::string_view effectLevelName(EffectLevel s);
std::optional<EffectLevel> effectLevelFromName(std::string_view name);

// Seconds a battle action's resolve pause lasts (Confirm always skips it).
float resolveSeconds(BattleSpeed s);
// Multiplier applied to transient on-screen message durations.
float messageDurationScale(MessageSpeed s);

struct Settings {
    float masterVolume = 1.0f;  // 0..1
    float musicVolume = 1.0f;   // 0..1
    float sfxVolume = 1.0f;     // 0..1
    // M52 (owner-approved): ambience gets its own slider instead of following
    // SFX (the M27 chaining). Optional field; absent = 0.5 (default 5/10), so
    // pre-M52 files load quieter-by-design rather than at the old effective 1.0.
    float ambienceVolume = 0.5f;  // 0..1
    bool borderlessFullscreen = false;
    BattleSpeed battleSpeed = BattleSpeed::Normal;
    MessageSpeed messageSpeed = MessageSpeed::Normal;
    EffectLevel effectFlash = EffectLevel::Full;
    EffectLevel effectShake = EffectLevel::Full;
    // M22 (owner-approved): switchable high-contrast UI palette. Optional
    // field; absent = standard palette, so older files load unchanged.
    bool highContrast = false;
    // M51 (owner-approved), both optional bools, absent = false so older files
    // load unchanged:
    //  - crtEffect: a subtle scanline/mask shader at the window blit (Off default).
    //  - backgroundAudio: keep audio playing while the window is unfocused
    //    (Off default = mute when unfocused, a deliberate behaviour change).
    bool crtEffect = false;
    bool backgroundAudio = false;
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
