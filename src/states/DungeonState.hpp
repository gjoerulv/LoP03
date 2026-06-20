#pragma once

#include <optional>
#include <string>
#include <vector>

#include "battle/Battle.hpp"
#include "core/Geometry.hpp"
#include "dungeon/DungeonModel.hpp"
#include "states/GameState.hpp"
#include "town/Tilemap.hpp"

namespace cd {

struct AppContext;

// Walkable dungeon explorer. Each room is a single-screen tilemap; walk room to
// room through open doors. Facing a team and pressing Confirm starts a battle.
// Victory clears the gate (or chest guard); beating the boss completes the
// dungeon; defeat ends the run. Inspect, open chests, retreat to town.
class DungeonState : public GameState {
public:
    DungeonState(StateStack& stack, AppContext& context, dungeon::Dungeon dungeon);

    void onResume() override;  // applies a battle outcome
    void handleInput(const Input& input) override;
    void update(float dt) override;
    void render() override;

private:
    enum class MarkerKind { GateTeam, GuardTeam, Boss, Chest };
    enum class EncounterKind { None, Gate, Guard, Boss };
    struct Marker {
        int x = 0;
        int y = 0;
        MarkerKind kind = MarkerKind::Chest;
        int teamIndex = -1;
        dungeon::Dir gateDir = dungeon::Dir::North;  // for GateTeam markers
    };
    struct DoorTile {
        int x = 0;
        int y = 0;
        dungeon::Dir dir = dungeon::Dir::North;
        int neighbor = -1;
    };

    void enterRoom(int index, std::optional<dungeon::Dir> entrySide);
    void buildRoom();  // rebuilds map/markers/doors for the current room (no move)
    void recomputeInteraction(int playerTileX, int playerTileY);
    void interact();
    void openChest();
    void startBattle(int teamIndex, EncounterKind kind, dungeon::Dir gateDir);
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

    // Pending battle context (applied in onResume).
    EncounterKind pendingKind_ = EncounterKind::None;
    int pendingRoom_ = 0;
    dungeon::Dir pendingGateDir_ = dungeon::Dir::North;
    battle::Outcome battleResult_ = battle::Outcome::Ongoing;

    std::string message_;
    float messageTimer_ = 0.0f;
    float moveX_ = 0.0f;
    float moveY_ = 0.0f;
};

}  // namespace cd
