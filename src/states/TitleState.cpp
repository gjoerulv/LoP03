#include "states/TitleState.hpp"

#include <cmath>
#include <memory>

#include "content/ContentDatabase.hpp"
#include "core/AppContext.hpp"
#include "core/Log.hpp"
#include "input/Input.hpp"
#include "raylib.h"
#include "resource/ResourceManager.hpp"
#include "states/MenuOverlayState.hpp"
#include "states/StateStack.hpp"

namespace cd {

TitleState::TitleState(StateStack& stack, AppContext& context)
    : GameState(stack), context_(context) {}

void TitleState::onEnter() { log::info("Entered TitleState"); }

void TitleState::handleInput(const Input& input) {
    if (input.pressed(InputAction::Confirm) || input.pressed(InputAction::Menu)) {
        stack().pushState(std::make_unique<MenuOverlayState>(stack(), context_));
    } else if (input.pressed(InputAction::Cancel)) {
        // Leaving the title empties the stack, which ends the application.
        stack().popState();
    }
}

void TitleState::update(float dt) { elapsed_ += dt; }

void TitleState::render() {
    const int w = context_.virtualWidth;
    const int h = context_.virtualHeight;

    // Background gradient-ish fill (two bands) — purely placeholder.
    ClearBackground(Color{18, 16, 30, 255});
    DrawRectangle(0, h - 70, w, 70, Color{26, 24, 44, 255});

    // Placeholder "logo": a missing texture resolves to the checker placeholder,
    // proving the ResourceManager graceful-failure path and the render pipeline.
    const Texture2D& logo = context_.resources.texture("title_logo", "textures/title_logo.png");
    DrawTexture(logo, w / 2 - logo.width / 2, 44, WHITE);

    const char* title = "CRYSTAL DUNGEONS";
    const int titleSize = 20;
    const int titleWidth = MeasureText(title, titleSize);
    DrawText(title, w / 2 - titleWidth / 2, 78, titleSize, RAYWHITE);

    const char* tag = "a 16-bit-inspired dungeon-score roguelite";
    const int tagSize = 10;
    DrawText(tag, w / 2 - MeasureText(tag, tagSize) / 2, 104, tagSize, Color{170, 170, 200, 255});

    // Blinking prompt.
    if (std::fmod(elapsed_, 1.0f) < 0.6f) {
        const char* prompt = "Enter / Tab:  Menu";
        DrawText(prompt, w / 2 - MeasureText(prompt, 10) / 2, 150, 10, RAYWHITE);
    }
    const char* quitPrompt = "Esc:  Quit";
    DrawText(quitPrompt, w / 2 - MeasureText(quitPrompt, 10) / 2, 168, 10,
             Color{150, 150, 170, 255});

    const content::ContentDatabase& db = context_.content;
    DrawText(TextFormat("Content: %d classes  %d skills  %d enemies  %d items",
                        static_cast<int>(db.classCount()), static_cast<int>(db.skillCount()),
                        static_cast<int>(db.enemyCount()), static_cast<int>(db.itemCount())),
             6, h - 28, 10, Color{120, 140, 120, 255});

    const char* milestone = "Milestone 2 - Core Data Model";
    DrawText(milestone, 6, h - 16, 10, Color{120, 120, 140, 255});
}

}  // namespace cd
