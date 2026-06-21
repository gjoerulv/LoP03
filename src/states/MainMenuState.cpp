#include "states/MainMenuState.hpp"

#include <memory>

#include "audio/AudioManager.hpp"
#include "content/ContentDatabase.hpp"
#include "core/AppContext.hpp"
#include "core/FadeController.hpp"
#include "input/Input.hpp"
#include "raylib.h"
#include "save/SaveSystem.hpp"
#include "states/HelpState.hpp"
#include "states/PartyCreationState.hpp"
#include "states/SlotMenuState.hpp"
#include "states/StateStack.hpp"
#include "ui/UiDraw.hpp"

namespace cd {

namespace {
constexpr int kNewGame = 0;
constexpr int kContinue = 1;
constexpr int kControls = 2;
constexpr int kQuit = 3;

bool anySaveExists(const save::SaveSystem& saves) {
    return saves.exists(save::SaveSlot::Auto) || saves.exists(save::SaveSlot::Manual1) ||
           saves.exists(save::SaveSlot::Manual2) || saves.exists(save::SaveSlot::Manual3);
}
}  // namespace

MainMenuState::MainMenuState(StateStack& stack, AppContext& context)
    : GameState(stack), context_(context) {}

void MainMenuState::onEnter() {
    context_.fade.start();
    context_.audio.setMusic(MusicTrack::Town);
    rebuild();
}
void MainMenuState::onResume() {
    context_.audio.setMusic(MusicTrack::Town);
    rebuild();  // refresh Continue after returning
}

void MainMenuState::rebuild() {
    const int previous = menu_.cursor();
    std::vector<ui::MenuItem> items;
    items.push_back({"New Game", true});
    items.push_back({"Continue", anySaveExists(context_.saves)});
    items.push_back({"Controls", true});
    items.push_back({"Quit", true});
    menu_.setItems(std::move(items));
    menu_.setCursor(previous);
}

void MainMenuState::handleInput(const Input& input) {
    if (input.pressed(InputAction::MoveUp)) {
        menu_.moveUp();
        context_.audio.play(Sfx::Move);
    }
    if (input.pressed(InputAction::MoveDown)) {
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

    ui::drawTextCentered("CRYSTAL DUNGEONS", w / 2, 50, 22, RAYWHITE);
    ui::drawTextCentered("a 16-bit-inspired dungeon-score roguelite", w / 2, 78, 10,
                         Color{170, 170, 200, 255});

    ui::drawMenu(menu_, w / 2 - 40, 120, 22, 14, RAYWHITE, Color{90, 90, 110, 255},
                 Color{240, 220, 120, 255});

    const content::ContentDatabase& db = context_.content;
    DrawText(TextFormat("Content: %d classes  %d enemies  %d items",
                        static_cast<int>(db.classCount()), static_cast<int>(db.enemyCount()),
                        static_cast<int>(db.itemCount())),
             6, h - 16, 10, Color{120, 120, 140, 255});
    DrawText("Milestone 8 - Presentation", w - 168, h - 16, 10, Color{120, 120, 140, 255});
}

}  // namespace cd
