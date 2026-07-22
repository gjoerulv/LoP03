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
#include "ui/UiStyle.hpp"

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
    const ui::style::Palette& p = ui::style::palette();
    ClearBackground(p.canvas);

    if (context_.resources.hasTexture("ui.emblem.crystal")) {
        DrawTexture(context_.resources.texture("ui.emblem.crystal"), w / 2 - 16, 8, WHITE);
    }
    // Title plaque with the crystal-pip corners; subtitle rides a subdued
    // caption row flanked by two glinting pips.
    const int plaqueBottom = ui::drawTitlePlaque("CRYSTAL DUNGEONS", w / 2, 42,
                                                 ui::style::kFontTitleHero);
    const char* tagline = "an 8-bit-plus dungeon-score roguelite";
    const int tagW = ui::measureText(tagline, ui::style::kFontBody);
    ui::drawTextCentered(tagline, w / 2, plaqueBottom + 6, ui::style::kFontBody, p.textDim);
    ui::drawCrystalPip(w / 2 - tagW / 2 - 10, plaqueBottom + 9);
    ui::drawCrystalPip(w / 2 + tagW / 2 + 7, plaqueBottom + 9);

    // Compact framed menu block, centered on its widest label (measured).
    int menuW = 0;
    for (const ui::MenuItem& item : menu_.items()) {
        menuW = std::max(menuW, ui::measureText(item.label, ui::style::kFontMenuLarge));
    }
    const int itemH = 20;
    const int rows = static_cast<int>(menu_.size());
    const int frameW = menuW + 52;
    const int frameH = rows * itemH + 16;
    const int frameX = w / 2 - frameW / 2;
    const int frameY = 106;
    ui::drawFrame(frameX, frameY, frameW, frameH, ui::FrameStyle::Crystal);
    ui::drawMenu(menu_, w / 2 - menuW / 2, frameY + 10, itemH, ui::style::kFontMenuLarge,
                 p.text, p.disabled, p.cursor);

    // Footer strip grounds the version/diagnostic rows.
    DrawRectangle(0, h - ui::style::kFooterHeight, w, ui::style::kFooterHeight, p.footerFill);
    DrawRectangle(0, h - ui::style::kFooterHeight, w, 1, p.ink);
    // Version stamp: always present, including Release — players reporting bugs
    // need a build to cite (M25 slice 1). Bottom-left, its own row.
    ui::drawText(TextFormat("v%s", version::kString), 4, h - 10, ui::style::kFontSmall,
                 p.textHint);

    // Developer diagnostic: Debug/dev builds only, removed from Release via the
    // CRYSTAL_SHIPPING_BUILD guard (M25 slice 1). Its own row (size 8) one line
    // above the version stamp, so the two never overlap.
#ifndef CRYSTAL_SHIPPING_BUILD
    const content::ContentDatabase& db = context_.content;
    ui::drawText(TextFormat("Content: %d classes  %d enemies  %d items",
                            static_cast<int>(db.classCount()), static_cast<int>(db.enemyCount()),
                            static_cast<int>(db.itemCount())),
                 4, h - 20, ui::style::kFontSmall, p.textHint);
#endif
}

}  // namespace cd
