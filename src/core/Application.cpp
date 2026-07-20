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
#include "ui/UiStyle.hpp"

namespace cd {
namespace {

// Resolves a bundled directory ("data", "assets"): prefer the folder next to
// the executable (CMake copies both there), then the working directory.
std::filesystem::path resolveBundledDir(const char* name) {
  namespace fs = std::filesystem;
  const char* appDir = GetApplicationDirectory();
  if (appDir != nullptr) {
    fs::path candidate = fs::path(appDir) / name;
    std::error_code ec;
    if (fs::exists(candidate, ec) && !ec) {
      return candidate;
    }
  }
  return fs::path(name);
}

std::filesystem::path resolveDataDir() { return resolveBundledDir("data"); }

} // namespace

Application::WindowGuard::WindowGuard() {
#ifdef NDEBUG
  // Release hygiene (M24): raylib's INFO chatter stays out of the shipped
  // build; project warnings/errors still surface.
  SetTraceLogLevel(LOG_WARNING);
#endif
  SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_VSYNC_HINT);
  InitWindow(config::kWindowWidth, config::kWindowHeight, config::kWindowTitle);
  SetWindowMinSize(config::kVirtualWidth, config::kVirtualHeight);
  SetExitKey(KEY_NULL); // quit is handled by game logic, not raylib's default ESC
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
      input_(),
      settings_(paths::userDataDir() / "settings.json"),
      tutorial_(paths::userDataDir() / "tutorial.json"),
      context_{resources_, content_, saves_, party_, scoreboard_, audio_, fade_,
               input_, settings_, tutorial_, config::kVirtualWidth,
               config::kVirtualHeight},
      stack_(),
      // Off by default (audit UI-LAYOUT-009); F1 toggles it in debug builds.
      debugOverlay_(false) {
  loadContent();
  {
    content::LoadReport settingsReport;
    if (!settings_.load(input_.map(), settingsReport)) {
      log::warn("Settings could not be loaded; using defaults.");
    }
    for (const auto& e : settingsReport.errors()) {
      log::warn("  " + e.source + ": " + e.context + ": " + e.message);
    }
    audio_.setVolumes(settings_.values.masterVolume,
                      settings_.values.musicVolume,
                      settings_.values.sfxVolume);
    ui::style::setHighContrast(settings_.values.highContrast);
  }
  {
    content::LoadReport tutorialReport;
    if (!tutorial_.load(tutorialReport)) {
      log::warn("Tutorial state could not be loaded; starting fresh.");
    }
    for (const auto& e : tutorialReport.errors()) {
      log::warn("  " + e.source + ": " + e.context + ": " + e.message);
    }
  }
  loadAssets();
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
                         static_cast<int>(report.errorCount()),
                         dataDir.string().c_str()));
    for (const auto& e : report.errors()) {
      log::warn("  " + e.source + ": " + e.context + ": " + e.message);
    }
  }
}

void Application::loadAssets() {
  assetsDir_ = resolveBundledDir("assets");
  content::LoadReport report;
  if (!manifest_.load(assetsDir_, report)) {
    log::warn("Asset manifest could not be loaded; using fallbacks only.");
  }
  for (const auto& e : report.errors()) {
    log::warn("  " + e.source + ": " + e.context + ": " + e.message);
  }
  if (!manifest_.empty()) {
    log::info(TextFormat("Asset manifest: %d entr%s from '%s'",
                         static_cast<int>(manifest_.size()),
                         manifest_.size() == 1 ? "y" : "ies",
                         assetsDir_.string().c_str()));
  }
  resources_.setCatalog(&manifest_, assetsDir_);
  resources_.reload();
  audio_.applyManifest(manifest_, assetsDir_);
}

void Application::run() {
  while (!WindowShouldClose() && !stack_.empty()) {
    processFrame();
  }
}

void Application::processFrame() {
  const float dt = GetFrameTime();
  input_.setQuery(makeRaylibInputQuery(0));
  input_.update(dt);

#ifdef CRYSTAL_DEBUG_OVERLAY
  if (input_.pressed(InputAction::ToggleDebug)) {
    debugOverlay_ = !debugOverlay_;
  }
  // Manual development reload (owner-approved model: caches drop, callers
  // re-fetch by id; the current music restarts on the re-resolved tier).
  if (input_.pressed(InputAction::ReloadAssets)) {
    log::info("Reloading assets...");
    loadAssets();
  }
#endif

  // Settings-driven window mode (self-healing each frame).
  if (settings_.values.borderlessFullscreen !=
      IsWindowState(FLAG_BORDERLESS_WINDOWED_MODE)) {
    ToggleBorderlessWindowed();
  }

  GameState* const topBefore = stack_.top();
  stack_.handleInput(input_);
  stack_.update(dt);
  audio_.update();
  fade_.update(dt);
  if (stack_.empty()) {
    return; // a state requested exit; loop will terminate
  }
  // A state transition swallows buffered presses so a held Confirm cannot
  // activate something in the screen that just appeared.
  if (stack_.top() != topBefore) {
    input_.suppressUntilRelease();
  }

  // 1) Draw the world at the internal resolution, then the fade overlay.
  screen_.beginDraw(BLACK);
  stack_.render();
  if (fade_.active()) {
    const unsigned char a = static_cast<unsigned char>(fade_.coverAlpha() * 255.0f);
    DrawRectangle(0, 0, config::kVirtualWidth, config::kVirtualHeight,
                  Color{0, 0, 0, a});
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

#ifdef CRYSTAL_DEBUG_OVERLAY
void Application::drawDebugOverlay() const {
  DrawRectangle(4, 4, 168, 64, Color{0, 0, 0, 170});
  DrawText(TextFormat("FPS: %d", GetFPS()), 12, 10, 16, GREEN);
  DrawText(TextFormat("States: %d", static_cast<int>(stack_.size())), 12, 30, 16,
           GREEN);
  DrawText("F1: toggle overlay", 12, 50, 10, Color{180, 180, 180, 255});
}
#endif

} // namespace cd
