#pragma once

#include <optional>
#include <string>
#include <vector>

#include "core/Geometry.hpp"
#include "dungeon/DungeonModel.hpp"
#include "states/GameState.hpp"
#include "town/Tilemap.hpp"

namespace cd {

struct AppContext;

// Walkable dungeon explorer. Each room is a single-screen tilemap; walk room to
// room through open doors. Gated doors are impassable (a team blocks them) until
// the battle milestone. Inspect teams, open unguarded chests, retreat to town.
class DungeonState : public GameState {
public:
    DungeonState(StateStack& stack, AppContext& context, dungeon::Dungeon dungeon);

    void handleInput(const Input& input) override;
    void update(float dt) override;
    void render() override;

private:
    enum class MarkerKind { GateTeam, GuardTeam, Boss, Chest };
    struct Marker {
        int x = 0;
        int y = 0;
        MarkerKind kind = MarkerKind::Chest;
        int teamIndex = -1;
    };
    struct DoorTile {
        int x = 0;
        int y = 0;
        dungeon::Dir dir = dungeon::Dir::North;
        int neighbor = -1;
    };

    void enterRoom(int index, std::optional<dungeon::Dir> entrySide);
    void recomputeInteraction(int playerTileX, int playerTileY);
    void interact();
    void openChest();
    void inspectTeam(int teamIndex);
    void renderMinimap() const;

    AppContext& context_;
    dungeon::Dungeon dungeon_;
    int currentRoom_ = 0;
    town::Tilemap roomMap_;
    Rect player_;
    Vec2 facing_{0.0f, 1.0f};

    std::vector<DoorTile> doorTiles_;
    std::vector<Marker> markers_;
    const Marker* facingMarker_ = nullptr;
    bool onChest_ = false;

    std::string message_;
    float messageTimer_ = 0.0f;
    float moveX_ = 0.0f;
    float moveY_ = 0.0f;
};

}  // namespace cd
