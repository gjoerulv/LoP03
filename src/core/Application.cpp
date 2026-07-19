#include "core/Application.hpp"

#include <filesystem>
#include <memory>

#include "content/ContentLoader.hpp"
#include "content/LoadReport.hpp"
#include "core/GameConfig.hpp"
#include "core/Log.hpp"
#include "input/InputMap.hpp"
#include "platform/Paths.hpp"
#include "raylib.h"
#include "states/MainMenuState.hpp"

namespace cd {

namespace {

// Resolves the content directory: prefer a "data" folder next to the executable
// (CMake copies it there), then fall back to the working directory.
std::filesystem::path resolveDataDir() {
    namespace fs = std::filesystem;
    const char* appDir = GetApplicationDirectory();
    if (appDir != nullptr) {
        fs::path candidate = fs::path(appDir) / "data";
        std::error_code ec;
        if (fs::exists(candidate, ec) && !ec) {
            return candidate;
        }
    }
    return fs::path("data");
}

}  // namespace

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
      content_(),
      party_(),
      saves_(content_, paths::userDataDir() / "saves"),
      scoreboard_(paths::userDataDir() / "scoreboard.json"),
      audio_(),
      fade_(),
      context_{resources_, content_,    saves_, party_, scoreboard_,
               audio_,     fade_,       config::kVirtualWidth, config::kVirtualHeight},
      input_(),
      stack_(),
      // Off by default (audit UI-LAYOUT-009); F1 toggles it in debug builds.
      debugOverlay_(false) {
    loadContent();
    fade_.start(0.6f);
    {
        content::LoadReport scoreReport;
        if (!scoreboard_.load(scoreReport)) {
            log::warn("Scoreboard could not be loaded; starting with an empty board.");
            for (const auto& e : scoreReport.errors()) {
                log::warn("  " + e.source + ": " + e.context + ": " + e.message);
            }
        }
    }
    stack_.pushState(std::make_unique<MainMenuState>(stack_, context_));
    stack_.applyPending();
    log::info("Crystal Dungeons initialized");
}

void Application::loadContent() {
    const std::filesystem::path dataDir = resolveDataDir();
    content::LoadReport report;
    const bool ok = content::loadAll(dataDir, content_, report);
    if (ok) {
        log::info(TextFormat("Content loaded: %d classes, %d skills, %d enemies, %d items",
                             static_cast<int>(content_.classCount()),
                             static_cast<int>(content_.skillCount()),
                             static_cast<int>(content_.enemyCount()),
                             static_cast<int>(content_.itemCount())));
    } else {
        // The game still runs (degraded). Surface every issue for the human.
        log::warn(TextFormat("Content loaded with %d issue(s) from '%s':",
                             static_cast<int>(report.errorCount()), dataDir.string().c_str()));
        for (const auto& e : report.errors()) {
            log::warn("  " + e.source + ": " + e.context + ": " + e.message);
        }
    }
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
    audio_.update();
    fade_.update(dt);
    if (stack_.empty()) {
        return;  // a state requested exit; loop will terminate
    }

    // 1) Draw the world at the internal resolution, then the fade overlay.
    screen_.beginDraw(BLACK);
    stack_.render();
    if (fade_.active()) {
        const unsigned char a = static_cast<unsigned char>(fade_.coverAlpha() * 255.0f);
        DrawRectangle(0, 0, config::kVirtualWidth, config::kVirtualHeight, Color{0, 0, 0, a});
    }
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
