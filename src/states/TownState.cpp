#include "states/TownState.hpp"

#include <cmath>
#include <memory>
#include <string>
#include <vector>

#include "audio/AudioManager.hpp"
#include "content/ContentDatabase.hpp"
#include "core/AppContext.hpp"
#include "core/FadeController.hpp"
#include "game/Party.hpp"
#include "input/Input.hpp"
#include "input/PromptLabels.hpp"
#include "raylib.h"
#include "resource/ResourceManager.hpp"
#include "states/EquipShopState.hpp"
#include "states/GuildState.hpp"
#include "states/InnState.hpp"
#include "states/ItemShopState.hpp"
#include "states/ScoreboardState.hpp"
#include "states/TrainingHallState.hpp"
#include "states/SlotMenuState.hpp"
#include "states/StateStack.hpp"
#include "states/TownMenuState.hpp"
#include "town/Movement.hpp"
#include "ui/UiDraw.hpp"

namespace cd {

namespace {

constexpr float kPlayerSpeed = 72.0f;
constexpr float kPlayerSize = 12.0f;

Color tileColor(town::Tile tile) {
    switch (tile) {
        case town::Tile::Ground: return Color{72, 104, 72, 255};
        case town::Tile::Path: return Color{120, 108, 80, 255};
        case town::Tile::Grass: return Color{84, 126, 80, 255};
        case town::Tile::Tree: return Color{30, 66, 40, 255};
        case town::Tile::Water: return Color{52, 84, 150, 255};
        case town::Tile::Building: return Color{112, 100, 120, 255};
        case town::Tile::Door: return Color{156, 112, 70, 255};
    }
    return BLACK;
}

const char* tileTextureId(town::Tile tile) {
    switch (tile) {
        case town::Tile::Ground: return "tiles.town.ground";
        case town::Tile::Path: return "tiles.town.path";
        case town::Tile::Grass: return "tiles.town.grass";
        case town::Tile::Tree: return "tiles.town.tree";
        case town::Tile::Water: return "tiles.town.water";
        case town::Tile::Building: return "tiles.town.building";
        case town::Tile::Door: return "tiles.town.door";
    }
    return "";
}

}  // namespace

TownState::TownState(StateStack& stack, AppContext& context)
    : GameState(stack), context_(context), town_(town::buildTown()) {
    // Center the player body inside the spawn tile.
    const float inset = (town::Tilemap::kTileSize - kPlayerSize) * 0.5f;
    player_ = Rect{town_.spawnPixel.x + inset, town_.spawnPixel.y + inset, kPlayerSize, kPlayerSize};
}

void TownState::onEnter() {
    context_.fade.start();
    context_.audio.setMusic(MusicTrack::Town);
}

void TownState::onResume() {
    context_.fade.start();
    context_.audio.setMusic(MusicTrack::Town);
}

const town::Building* TownState::buildingAtPlayerTile() const {
    const int ts = town::Tilemap::kTileSize;
    const int tx = static_cast<int>((player_.x + player_.w * 0.5f) / ts);
    const int ty = static_cast<int>((player_.y + player_.h * 0.5f) / ts);
    for (const town::Building& b : town_.buildings) {
        if (b.doorX == tx && b.doorY == ty) {
            return &b;
        }
    }
    return nullptr;
}

void TownState::enterLocation(const town::Building& building) {
    switch (building.id) {
        case town::LocationId::Inn:
            stack().pushState(std::make_unique<InnState>(stack(), context_));
            break;
        case town::LocationId::SavePoint:
            stack().pushState(
                std::make_unique<SlotMenuState>(stack(), context_, SlotMenuMode::Save));
            break;
        case town::LocationId::TrainingHall:
            stack().pushState(std::make_unique<TrainingHallState>(stack(), context_));
            break;
        case town::LocationId::Guild:
            stack().pushState(std::make_unique<GuildState>(stack(), context_));
            break;
        case town::LocationId::ItemShop:
            stack().pushState(std::make_unique<ItemShopState>(stack(), context_));
            break;
        case town::LocationId::EquipShop:
            stack().pushState(std::make_unique<EquipShopState>(stack(), context_));
            break;
        case town::LocationId::Scoreboard:
            stack().pushState(std::make_unique<ScoreboardState>(stack(), context_));
            break;
    }
}

void TownState::handleInput(const Input& input) {
    moveX_ = (input.down(InputAction::MoveRight) ? 1.0f : 0.0f) -
             (input.down(InputAction::MoveLeft) ? 1.0f : 0.0f);
    moveY_ = (input.down(InputAction::MoveDown) ? 1.0f : 0.0f) -
             (input.down(InputAction::MoveUp) ? 1.0f : 0.0f);

    if (input.pressed(InputAction::Confirm) && nearDoor_ != nullptr) {
        context_.audio.play(Sfx::Confirm);
        enterLocation(*nearDoor_);
    }
    if (input.pressed(InputAction::Menu) || input.pressed(InputAction::Cancel)) {
        context_.audio.play(Sfx::Confirm);
        stack().pushState(std::make_unique<TownMenuState>(stack(), context_));
    }
}

void TownState::update(float dt) {
    const float length = std::sqrt(moveX_ * moveX_ + moveY_ * moveY_);
    if (length > 0.0001f) {
        const float nx = moveX_ / length;
        const float ny = moveY_ / length;
        facing_ = Vec2{nx, ny};
        const Vec2 moved =
            town::resolveMove(player_, nx * kPlayerSpeed * dt, ny * kPlayerSpeed * dt, town_.map);
        player_.x = moved.x;
        player_.y = moved.y;
    }
    nearDoor_ = buildingAtPlayerTile();
}

void TownState::render() {
    const int ts = town::Tilemap::kTileSize;
    const town::Tilemap& map = town_.map;

    ClearBackground(BLACK);
    for (int ty = 0; ty < map.height(); ++ty) {
        for (int tx = 0; tx < map.width(); ++tx) {
            const town::Tile tile = map.at(tx, ty);
            const char* id = tileTextureId(tile);
            if (context_.resources.hasTexture(id)) {
                DrawTexture(context_.resources.texture(id), tx * ts, ty * ts, WHITE);
            } else {
                DrawRectangle(tx * ts, ty * ts, ts, ts, tileColor(tile));
            }
        }
    }

    // Building name labels above each building.
    for (const town::Building& b : town_.buildings) {
        const int cx = b.x * ts + b.w * ts / 2;
        ui::drawTextCentered(b.name.c_str(), cx, b.y * ts - 9, 8, Color{225, 225, 235, 255});
    }

    // Player (sprite when the catalog has one) + facing tick.
    if (context_.resources.hasTexture("actor.player.overworld")) {
        DrawTexture(context_.resources.texture("actor.player.overworld"),
                    static_cast<int>(player_.x), static_cast<int>(player_.y), WHITE);
    } else {
        DrawRectangle(static_cast<int>(player_.x), static_cast<int>(player_.y),
                      static_cast<int>(player_.w), static_cast<int>(player_.h),
                      Color{236, 224, 128, 255});
    }
    DrawRectangle(static_cast<int>(player_.x + player_.w * 0.5f + facing_.x * 4.0f - 1.0f),
                  static_cast<int>(player_.y + player_.h * 0.5f + facing_.y * 4.0f - 1.0f), 3, 3,
                  Color{60, 50, 30, 255});

    // Party HUD (top-left); the backdrop is sized to the measured text so
    // long names never spill past it.
    std::string hud = "Party:";
    for (const Character& c : context_.party.members) {
        hud += " " + c.name;
    }
    const int hudW = ui::measureText(hud, 8) + 6;
    DrawRectangle(2, 2, hudW, 12, Color{0, 0, 0, 140});
    ui::drawTextFitted(hud, 5, 4, context_.virtualWidth - 10, 8, RAYWHITE, "town.hud");

    // Interaction prompt, generated from the live bindings (M13).
    const int h = context_.virtualHeight;
    const InputMap& bindings = context_.input.map();
    const ActiveDevice device = context_.input.activeDevice();
    if (nearDoor_ != nullptr) {
        DrawRectangle(0, h - 16, context_.virtualWidth, 16, Color{0, 0, 0, 160});
        const std::string text =
            input::prompt(bindings, InputAction::Confirm, device, "Enter " + nearDoor_->name) +
            "   " + input::prompt(bindings, InputAction::Menu, device, "Pause");
        ui::drawTextCentered(text.c_str(), context_.virtualWidth / 2, h - 13, 8,
                             Color{240, 230, 160, 255});
    } else {
        const std::string moveLabel =
            device == ActiveDevice::Keyboard
                ? input::primaryLabel(bindings, InputAction::MoveUp, device) + "/" +
                      input::primaryLabel(bindings, InputAction::MoveDown, device) + "/" +
                      input::primaryLabel(bindings, InputAction::MoveLeft, device) + "/" +
                      input::primaryLabel(bindings, InputAction::MoveRight, device)
                : "D-Pad/Stick";
        const std::string text = "[" + moveLabel + "] Move   " +
                                 input::prompt(bindings, InputAction::Menu, device, "Pause");
        ui::drawTextCentered(text.c_str(), context_.virtualWidth / 2, h - 13, 8,
                             Color{170, 170, 190, 255});
    }
}

}  // namespace cd
