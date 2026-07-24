#pragma once

// Shared, long-lived services handed to states. Kept small; grows per milestone.

#include "core/DebugCheats.hpp"

namespace cd {

class ResourceManager;
class AudioManager;
class FadeController;
class Input;
struct Party;

namespace content {
class ContentDatabase;
}
namespace save {
class SaveSystem;
}
namespace score {
class Scoreboard;
}
namespace settings {
class SettingsStore;
}
namespace tutorial {
class TutorialStore;
}
class AchievementStore;
class ProfileStore;

struct AppContext {
    ResourceManager& resources;
    const content::ContentDatabase& content;
    save::SaveSystem& saves;
    Party& party;  // the active party/session (empty until New Game or Continue)
    score::Scoreboard& scoreboard;
    AudioManager& audio;
    FadeController& fade;
    // M13: the input system (bindings + device state, mutable for the remap
    // screen) and the persistent settings store.
    Input& input;
    settings::SettingsStore& settings;
    // M22: one-time onboarding prompt progress (tutorial.json).
    tutorial::TutorialStore& tutorial;
    // M42: global unlocked-achievements store (achievements.json).
    AchievementStore& achievements;
    // M45: cross-save player profile (profile.json) — currently just whether the
    // King has ever fallen, which unlocks the three reward classes.
    ProfileStore& profile;
    int virtualWidth;
    int virtualHeight;
    // M53: development-only cheat state. A value member (owned by the single
    // Application::context_) with a default, so the positional aggregate
    // initializer that stops at virtualHeight leaves it default-constructed. All
    // access is #ifdef CRYSTAL_DEBUG_OVERLAY; absent from Release behaviour.
    DebugCheats cheats{};
};

}  // namespace cd
