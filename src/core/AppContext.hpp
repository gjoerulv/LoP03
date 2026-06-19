#pragma once

// Shared, long-lived services handed to states. Kept small; grows in later
// milestones (audio, save system, ...).

namespace cd {

class ResourceManager;

namespace content {
class ContentDatabase;
}

struct AppContext {
    ResourceManager& resources;
    const content::ContentDatabase& content;
    int virtualWidth;
    int virtualHeight;
};

}  // namespace cd
