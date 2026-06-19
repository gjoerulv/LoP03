#include "core/Application.hpp"

#include <memory>

#include "core/GameConfig.hpp"
#include "core/Log.hpp"
#include "input/InputMap.hpp"
#include "raylib.h"
#include "states/TitleState.hpp"

namespace cd {

Application::WindowGuard::WindowGuard() {
    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_VSYNC_HINT);
    InitWindow(config::kWindowWidth, config::kWindowHeight, config::kWindowTitle);
    SetWindowMinSize(config::kVirtualWidth, config::kVirtualHeight);
    SetExitKey(KEY_NULL);  // quit is handled by game logic, not raylib's default ESC
    SetTargetFPS(config::kTargetFps);
}

Application::WindowGuard::~WindowGuard() { CloseWindow(); }

Application::Application()
    : window_(),
      screen_(config::kVirtualWidth, config::kVirtualHeight),
      resources_(),
      context_{resources_, config::kVirtualWidth, config::kVirtualHeight},
      input_(),
      stack_(),
      debugOverlay_(true) {
    stack_.pushState(std::make_unique<TitleState>(stack_, context_));
    stack_.applyPending();
    log::info("Crystal Dungeons initialized");
}

void Application::run() {
    while (!WindowShouldClose() && !stack_.empty()) {
        processFrame();
    }
}

void Application::processFrame() {
    const float dt = GetFrameTime();
    input_.setQuery(makeRaylibInputQuery(0));

    if (input_.pressed(InputAction::ToggleDebug)) {
        debugOverlay_ = !debugOverlay_;
    }

    stack_.handleInput(input_);
    stack_.update(dt);
    if (stack_.empty()) {
        return;  // a state requested exit; loop will terminate
    }

    // 1) Draw the world at the internal resolution.
    screen_.beginDraw(BLACK);
    stack_.render();
    screen_.endDraw();

    // 2) Present the scaled virtual screen, then any overlays at window scale.
    BeginDrawing();
    screen_.blitToWindow(GetScreenWidth(), GetScreenHeight(), BLACK);
#ifdef CRYSTAL_DEBUG_OVERLAY
    if (debugOverlay_) {
        drawDebugOverlay();
    }
#endif
    EndDrawing();
}

void Application::drawDebugOverlay() const {
    DrawRectangle(4, 4, 168, 64, Color{0, 0, 0, 170});
    DrawText(TextFormat("FPS:    %d", GetFPS()), 12, 10, 16, GREEN);
    DrawText(TextFormat("States: %d", static_cast<int>(stack_.size())), 12, 30, 16, GREEN);
    DrawText("F1: toggle overlay", 12, 50, 10, Color{180, 180, 180, 255});
}

}  // namespace cd
