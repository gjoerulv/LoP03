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
#include "game/WorldLadder.hpp"
#include "input/Input.hpp"
#include "input/PromptLabels.hpp"
#include "raylib.h"
#include "resource/ResourceManager.hpp"
#include "states/BlackMarketState.hpp"
#include "states/EquipShopState.hpp"
#include "states/GuildState.hpp"
#include "states/InnState.hpp"
#include "states/ItemShopState.hpp"
#include "states/ScoreboardState.hpp"
#include "states/TrainingHallState.hpp"
#include "states/SlotMenuState.hpp"
#include "states/StateStack.hpp"
#include "states/TutorialPromptState.hpp"
#include "tutorial/Tutorial.hpp"
#include "states/TownMenuState.hpp"
#include "render/SpriteDraw.hpp"
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

// Tile kind suffix shared by the base id ("tiles.town.<kind>") and the per-town
// exterior variant ("tiles.town.<N>.<kind>", M32).
const char* tileKind(town::Tile tile) {
    switch (tile) {
        case town::Tile::Ground: return "ground";
        case town::Tile::Path: return "path";
        case town::Tile::Grass: return "grass";
        case town::Tile::Tree: return "tree";
        case town::Tile::Water: return "water";
        case town::Tile::Building: return "building";
        case town::Tile::Door: return "door";
    }
    return "ground";
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

const char* walkAnimId(render::Facing f) {
    switch (f) {
        case render::Facing::Down: return "anim.player.walk.down";
        case render::Facing::Up: return "anim.player.walk.up";
        case render::Facing::Left: return "anim.player.walk.left";
        case render::Facing::Right: return "anim.player.walk.right";
    }
    return "anim.player.walk.down";
}

}  // namespace

TownState::TownState(StateStack& stack, AppContext& context)
    : GameState(stack), context_(context), town_(town::buildTown()) {
    buildForCurrentTown();
}

void TownState::buildForCurrentTown() {
    const int town = clampTown(context_.party.currentTown);
    const bool hasPrev = town > 1;
    const bool hasNext = town < kTownCount;
    const bool nextUnlocked = context_.party.highestUnlockedTown > town;
    town_ = town::buildTown(town, hasPrev, hasNext, nextUnlocked);
    // Center the player body inside the spawn tile.
    const float inset = (town::Tilemap::kTileSize - kPlayerSize) * 0.5f;
    player_ = Rect{town_.spawnPixel.x + inset, town_.spawnPixel.y + inset, kPlayerSize, kPlayerSize};
    nearDoor_ = nullptr;
    nearExit_ = nullptr;
    moving_ = false;
    walkTime_ = 0.0f;
}

void TownState::applyTownAudio() {
    // setTown before setMusic: when Town music is already current (returning to
    // or switching towns) setTown restarts it on the right variant; otherwise
    // setMusic starts it on the slot setTown just bound.
    context_.audio.setTown(context_.party.currentTown);
    context_.audio.setMusic(MusicTrack::Town);
    context_.audio.setAmbience(AmbienceTrack::Town);
}

void TownState::travelTo(int destTown) {
    context_.party.currentTown = clampTown(destTown);
    context_.audio.play(Sfx::Door);
    buildForCurrentTown();
    context_.fade.start();
    applyTownAudio();
    maybeTutorialPrompt(stack(), context_, tutorial::kFirstTravel);
}

void TownState::onEnter() {
    context_.fade.start();
    applyTownAudio();
    maybeTutorialPrompt(stack(), context_, tutorial::kTownWelcome);
}

void TownState::onResume() {
    context_.fade.start();
    applyTownAudio();
    // A run cleared in this town may have unlocked the road onward (M32). Only
    // the next-town exit's locked state can change while sub-states are on top
    // (currentTown never changes without a rebuild), so refresh it in place
    // rather than rebuilding — which would teleport the player off a shop door.
    const int town = clampTown(context_.party.currentTown);
    for (town::TownExit& e : town_.exits) {
        if (e.toNext) {
            e.locked = context_.party.highestUnlockedTown <= town;
        }
    }
    // Fires once, after the player has seen their first run's reckoning.
    if (context_.tutorial.state.seen.count(tutorial::kResultFirst) > 0) {
        maybeTutorialPrompt(stack(), context_, tutorial::kTownReturn);
    }
}

const town::TownExit* TownState::exitAtPlayerTile() const {
    const int ts = town::Tilemap::kTileSize;
    const int tx = static_cast<int>((player_.x + player_.w * 0.5f) / ts);
    const int ty = static_cast<int>((player_.y + player_.h * 0.5f) / ts);
    for (const town::TownExit& e : town_.exits) {
        if (e.tileX == tx && e.tileY == ty) {
            return &e;
        }
    }
    return nullptr;
}

bool TownState::blackMarketHere() const {
    return context_.party.blackMarket.present &&
           context_.party.blackMarket.town == clampTown(context_.party.currentTown);
}

bool TownState::onBlackMarketTile() const {
    if (!blackMarketHere()) {
        return false;
    }
    const int ts = town::Tilemap::kTileSize;
    const int tx = static_cast<int>((player_.x + player_.w * 0.5f) / ts);
    const int ty = static_cast<int>((player_.y + player_.h * 0.5f) / ts);
    return tx == context_.party.blackMarket.tileX && ty == context_.party.blackMarket.tileY;
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

    if (input.pressed(InputAction::Confirm)) {
        if (nearDoor_ != nullptr) {
            context_.audio.play(Sfx::Confirm);
            enterLocation(*nearDoor_);
        } else if (nearMarket_) {
            context_.audio.play(Sfx::Confirm);
            stack().pushState(std::make_unique<BlackMarketState>(stack(), context_));
        } else if (nearExit_ != nullptr) {
            if (nearExit_->locked) {
                context_.audio.play(Sfx::Error);  // road not yet open; hint is on screen
            } else {
                travelTo(nearExit_->destTown);
            }
        }
    }
    if (input.pressed(InputAction::Menu) || input.pressed(InputAction::Cancel)) {
        context_.audio.play(Sfx::Confirm);
        stack().pushState(std::make_unique<TownMenuState>(stack(), context_));
    }
}

void TownState::update(float dt) {
    const float length = std::sqrt(moveX_ * moveX_ + moveY_ * moveY_);
    moving_ = length > 0.0001f;
    if (moving_) {
        const float nx = moveX_ / length;
        const float ny = moveY_ / length;
        facing_ = Vec2{nx, ny};
        const Vec2 moved =
            town::resolveMove(player_, nx * kPlayerSpeed * dt, ny * kPlayerSpeed * dt, town_.map);
        player_.x = moved.x;
        player_.y = moved.y;
        walkTime_ += dt;
        context_.audio.play(Sfx::Step);  // cadence via the role's rate limit
    } else {
        walkTime_ = 0.0f;  // stand frame
    }
    nearDoor_ = buildingAtPlayerTile();
    nearExit_ = nearDoor_ == nullptr ? exitAtPlayerTile() : nullptr;
    nearMarket_ = (nearDoor_ == nullptr && nearExit_ == nullptr) && onBlackMarketTile();
}

void TownState::render() {
    const int ts = town::Tilemap::kTileSize;
    const town::Tilemap& map = town_.map;

    const int townIdx = clampTown(context_.party.currentTown);
    ClearBackground(BLACK);
    for (int ty = 0; ty < map.height(); ++ty) {
        for (int tx = 0; tx < map.width(); ++tx) {
            const town::Tile tile = map.at(tx, ty);
            std::string id = tileTextureId(tile);
            // Deterministic accent variant: occasional flower patches on grass.
            if (tile == town::Tile::Grass && (tx * 31 + ty * 17) % 11 == 0 &&
                context_.resources.hasTexture("tiles.town.flowers")) {
                id = "tiles.town.flowers";
            } else if (townIdx > 1) {
                // Per-town exterior variant (M32); falls back to the base tile.
                const std::string variant =
                    "tiles.town." + std::to_string(townIdx) + "." + tileKind(tile);
                if (context_.resources.hasTexture(variant)) {
                    id = variant;
                }
            }
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

    // Exit signposts above each road out (M32).
    for (const town::TownExit& e : town_.exits) {
        const int cx = e.tileX * ts + ts / 2;
        const std::string label = e.locked ? "Locked"
                                  : e.toNext ? "Town " + std::to_string(e.destTown) + " >"
                                             : "< Town " + std::to_string(e.destTown);
        const Color col = e.locked ? Color{150, 120, 120, 255} : Color{240, 230, 160, 255};
        ui::drawTextCentered(label.c_str(), cx, (e.tileY - 1) * ts - 1, 8, col);
    }

    // Black-market NPC (M34): a hooded dealer at the offer's seeded plaza tile.
    if (blackMarketHere()) {
        const int mx = context_.party.blackMarket.tileX;
        const int my = context_.party.blackMarket.tileY;
        const float mcx = mx * ts + ts * 0.5f;
        const float mcy = my * ts + ts * 0.5f;
        if (!render::drawTextureCentered(context_.resources, "actor.market.overworld", mcx, mcy)) {
            DrawRectangle(mx * ts + 3, my * ts + 2, ts - 6, ts - 4, Color{120, 70, 150, 255});
        }
        ui::drawTextCentered("Black Market", mx * ts + ts / 2, my * ts - 9, 8,
                             Color{210, 170, 240, 255});
    }

    // Player: directional walk animation, then static sprite, then rectangle
    // (with the old facing tick, which the sprites encode themselves).
    const float pcx = player_.x + player_.w * 0.5f;
    const float pcy = player_.y + player_.h * 0.5f;
    const render::Facing facing = render::facingFrom(facing_.x, facing_.y);
    if (!render::drawAnimationCentered(context_.resources, walkAnimId(facing),
                                       moving_ ? walkTime_ : 0.0f, pcx, pcy) &&
        !render::drawTextureCentered(context_.resources, "actor.player.overworld", pcx, pcy)) {
        DrawRectangle(static_cast<int>(player_.x), static_cast<int>(player_.y),
                      static_cast<int>(player_.w), static_cast<int>(player_.h),
                      Color{236, 224, 128, 255});
        DrawRectangle(static_cast<int>(pcx + facing_.x * 4.0f - 1.0f),
                      static_cast<int>(pcy + facing_.y * 4.0f - 1.0f), 3, 3,
                      Color{60, 50, 30, 255});
    }

    // Party HUD (top-left); the backdrop is sized to the measured text so
    // long names never spill past it.
    std::string hud = "Party:";
    for (const Character& c : context_.party.members) {
        hud += " " + c.name;
    }
    const int hudW = ui::measureText(hud, 8) + 6;
    DrawRectangle(2, 2, hudW, 12, Color{0, 0, 0, 140});
    ui::drawTextFitted(hud, 5, 4, context_.virtualWidth - 10, 8, RAYWHITE, "town.hud");

    // Town indicator (top-right): which town of the ladder this is (M32).
    const std::string townLabel =
        "Town " + std::to_string(townIdx) + "/" + std::to_string(kTownCount);
    const int tlW = ui::measureText(townLabel, 8) + 6;
    DrawRectangle(context_.virtualWidth - tlW - 2, 2, tlW, 12, Color{0, 0, 0, 140});
    ui::drawTextRight(townLabel.c_str(), context_.virtualWidth - 5, 4, 8,
                      Color{225, 215, 170, 255});

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
    } else if (nearMarket_) {
        DrawRectangle(0, h - 16, context_.virtualWidth, 16, Color{0, 0, 0, 160});
        const std::string text =
            input::prompt(bindings, InputAction::Confirm, device, "Browse the Black Market") +
            "   " + input::prompt(bindings, InputAction::Menu, device, "Pause");
        ui::drawTextCentered(text.c_str(), context_.virtualWidth / 2, h - 13, 8,
                             Color{215, 175, 245, 255});
    } else if (nearExit_ != nullptr) {
        DrawRectangle(0, h - 16, context_.virtualWidth, 16, Color{0, 0, 0, 160});
        std::string text;
        if (nearExit_->locked) {
            text = "Clear a dungeon in this town to open the road onward";
        } else {
            text = input::prompt(bindings, InputAction::Confirm, device,
                                 "Travel to Town " + std::to_string(nearExit_->destTown));
        }
        ui::drawTextCentered(text.c_str(), context_.virtualWidth / 2, h - 13, 8,
                             nearExit_->locked ? Color{200, 170, 170, 255}
                                               : Color{240, 230, 160, 255});
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
