#pragma once

#include "content/ContentDatabase.hpp"
#include "core/AppContext.hpp"
#include "input/Input.hpp"
#include "render/VirtualScreen.hpp"
#include "resource/ResourceManager.hpp"
#include "states/StateStack.hpp"

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
    content::ContentDatabase content_;
    AppContext context_;
    Input input_;
    StateStack stack_;
    bool debugOverlay_;
};

}  // namespace cd
