#pragma once

// Shared, long-lived services handed to states. Kept small; grows per milestone.

namespace cd {

class ResourceManager;
class AudioManager;
class FadeController;
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

struct AppContext {
    ResourceManager& resources;
    const content::ContentDatabase& content;
    save::SaveSystem& saves;
    Party& party;  // the active party/session (empty until New Game or Continue)
    score::Scoreboard& scoreboard;
    AudioManager& audio;
    FadeController& fade;
    int virtualWidth;
    int virtualHeight;
};

}  // namespace cd
