#include "states/MainMenuState.hpp"

#include <algorithm>
#include <memory>

#include "audio/AudioManager.hpp"
#include "content/ContentDatabase.hpp"
#include "core/AppContext.hpp"
#include "core/FadeController.hpp"
#include "core/Version.hpp"
#include "input/Input.hpp"
#include "raylib.h"
#include "resource/ResourceManager.hpp"
#include "save/SaveSystem.hpp"
#include "states/HelpState.hpp"
#include "states/PartyCreationState.hpp"
#include "states/SettingsState.hpp"
#include "states/SlotMenuState.hpp"
#include "states/StateStack.hpp"
#include "ui/UiDraw.hpp"

namespace cd {

namespace {
constexpr int kNewGame = 0;
constexpr int kContinue = 1;
constexpr int kControls = 2;
constexpr int kSettings = 3;
constexpr int kQuit = 4;

bool anySaveExists(const save::SaveSystem& saves) {
    return saves.exists(save::SaveSlot::Auto) || saves.exists(save::SaveSlot::Manual1) ||
           saves.exists(save::SaveSlot::Manual2) || saves.exists(save::SaveSlot::Manual3);
}
}  // namespace

MainMenuState::MainMenuState(StateStack& stack, AppContext& context)
    : GameState(stack), context_(context) {}

void MainMenuState::onEnter() {
    context_.fade.start();
    context_.audio.setMusic(MusicTrack::Title);
    context_.audio.setAmbience(AmbienceTrack::None);
    rebuild();
}
void MainMenuState::onResume() {
    context_.audio.setMusic(MusicTrack::Title);
    context_.audio.setAmbience(AmbienceTrack::None);
    rebuild();  // refresh Continue after returning
}

void MainMenuState::rebuild() {
    const int previous = menu_.cursor();
    std::vector<ui::MenuItem> items;
    items.push_back({"New Game", true});
    items.push_back({"Continue", anySaveExists(context_.saves)});
    items.push_back({"Controls", true});
    items.push_back({"Settings", true});
    items.push_back({"Quit", true});
    menu_.setItems(std::move(items));
    menu_.setCursor(previous);
}

void MainMenuState::handleInput(const Input& input) {
    if (input.navPressed(InputAction::MoveUp)) {
        menu_.moveUp();
        context_.audio.play(Sfx::Move);
    }
    if (input.navPressed(InputAction::MoveDown)) {
        menu_.moveDown();
        context_.audio.play(Sfx::Move);
    }
    if (input.pressed(InputAction::Confirm) && menu_.currentEnabled()) {
        context_.audio.play(Sfx::Confirm);
        switch (menu_.cursor()) {
            case kNewGame:
                stack().pushState(std::make_unique<PartyCreationState>(stack(), context_));
                break;
            case kContinue:
                stack().pushState(
                    std::make_unique<SlotMenuState>(stack(), context_, SlotMenuMode::Load));
                break;
            case kControls:
                stack().pushState(std::make_unique<HelpState>(stack(), context_));
                break;
            case kSettings:
                stack().pushState(std::make_unique<SettingsState>(stack(), context_));
                break;
            case kQuit:
                stack().popState();  // empties the stack -> app exits
                break;
            default:
                break;
        }
    }
}

void MainMenuState::render() {
    const int w = context_.virtualWidth;
    const int h = context_.virtualHeight;
    ClearBackground(Color{18, 16, 30, 255});
    DrawRectangle(0, h - 60, w, 60, Color{26, 24, 44, 255});

    if (context_.resources.hasTexture("ui.emblem.crystal")) {
        DrawTexture(context_.resources.texture("ui.emblem.crystal"), w / 2 - 16, 10, WHITE);
    }
    ui::drawTextCentered("CRYSTAL DUNGEONS", w / 2, 50, 22, RAYWHITE);
    ui::drawTextCentered("a 16-bit-inspired dungeon-score roguelite", w / 2, 78, 10,
                         Color{170, 170, 200, 255});
    DrawText(TextFormat("v%s", version::kString), 4, h - 12, 8, Color{120, 120, 150, 255});

    // Center the menu block on its widest label (measured, not guessed).
    int menuW = 0;
    for (const ui::MenuItem& item : menu_.items()) {
        menuW = std::max(menuW, ui::measureText(item.label, 14));
    }
    ui::drawMenu(menu_, w / 2 - menuW / 2, 120, 22, 14, RAYWHITE, Color{90, 90, 110, 255},
                 Color{240, 220, 120, 255});

    const content::ContentDatabase& db = context_.content;
    DrawText(TextFormat("Content: %d classes  %d enemies  %d items",
                        static_cast<int>(db.classCount()), static_cast<int>(db.enemyCount()),
                        static_cast<int>(db.itemCount())),
             6, h - 16, 10, Color{120, 120, 140, 255});
}

}  // namespace cd
