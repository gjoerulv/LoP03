#pragma once

// Shared, long-lived services handed to states. Kept small; grows in later
// milestones (audio, save system, content database, ...).

namespace cd {

class ResourceManager;

struct AppContext {
    ResourceManager& resources;
    int virtualWidth;
    int virtualHeight;
};

}  // namespace cd
