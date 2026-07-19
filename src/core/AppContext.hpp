#pragma once

// Shared, long-lived services handed to states. Kept small; grows per milestone.

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
    int virtualWidth;
    int virtualHeight;
};

}  // namespace cd
