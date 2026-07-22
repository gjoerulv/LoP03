#pragma once

#include "assets/AssetManifest.hpp"
#include "audio/AudioManager.hpp"
#include "content/ContentDatabase.hpp"
#include "core/AppContext.hpp"
#include "core/FadeController.hpp"
#include "game/Achievements.hpp"
#include "game/Party.hpp"
#include "input/Input.hpp"
#include "render/VirtualScreen.hpp"
#include "resource/ResourceManager.hpp"
#include "save/SaveSystem.hpp"
#include "score/Scoreboard.hpp"
#include "settings/Settings.hpp"
#include "states/StateStack.hpp"
#include "tutorial/Tutorial.hpp"

namespace cd {

// Owns the window, the virtual screen, shared services, and the main loop.
// Member order matters: the window guard is first so it is destroyed LAST,
// after every GPU resource has been released.
class Application {
public:
    Application();
    void run();

private:
    void loadContent();
    void loadAssets();  // (re)loads the manifest and re-points resources/audio
    void processFrame();
    void drawDebugOverlay() const;

    // RAII for InitWindow/CloseWindow so ordering is guaranteed by the language.
    struct WindowGuard {
        WindowGuard();
        ~WindowGuard();
        WindowGuard(const WindowGuard&) = delete;
        WindowGuard& operator=(const WindowGuard&) = delete;
    };

    WindowGuard window_;
    VirtualScreen screen_;
    ResourceManager resources_;
    assets::AssetManifest manifest_;
    std::filesystem::path assetsDir_;
    content::ContentDatabase content_;
    Party party_;
    save::SaveSystem saves_;
    score::Scoreboard scoreboard_;
    AudioManager audio_;
    FadeController fade_;
    // input_, settings_, and tutorial_ are declared before context_ because
    // the context holds references to them.
    Input input_;
    settings::SettingsStore settings_;
    tutorial::TutorialStore tutorial_;
    AchievementStore achievements_;
    AppContext context_;
    StateStack stack_;
    bool debugOverlay_;
};

}  // namespace cd
