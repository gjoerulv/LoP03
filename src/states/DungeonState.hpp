#pragma once

#include <optional>
#include <string>
#include <vector>

#include "battle/Battle.hpp"
#include "core/Geometry.hpp"
#include "danger/DangerRating.hpp"
#include "dungeon/DungeonModel.hpp"
#include "dungeon/RoomLayout.hpp"
#include "states/GameState.hpp"
#include "town/Tilemap.hpp"

namespace cd {

struct AppContext;

// Walkable dungeon explorer. Each room is a compact archetype layout realized
// from a derived room-local seed (M16), drawn centered in the exploration
// viewport; walk room to room through open doors. Facing a team and pressing
// Confirm starts a battle.
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
    enum class MarkerKind { GateTeam, GuardTeam, Boss, Chest, Event };
    enum class EncounterKind { None, Gate, Guard, Boss, Challenge };
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

    struct RunStats {
        int battleTurns = 0;
        int dangerDefeated = 0;
        int chestsOpened = 0;
        int treasureGold = 0;
        bool noDeath = true;
        int escapes = 0;
        bool wagerAccepted = false;  // M20 score-wager event
    };

    void enterRoom(int index, std::optional<dungeon::Dir> entrySide);
    void buildRoom();  // rebuilds map/markers/doors for the current room (no move)
    void recomputeInteraction(int playerTileX, int playerTileY);
    void interact();
    void openChest();
    void resolveEvent();  // applies a non-battle event's stated trade-off
    std::string eventPromptText() const;  // the pre-confirmation trade-off line
    void startBattle(int teamIndex, EncounterKind kind, dungeon::Dir gateDir);
    void completeDungeon();
    void renderMinimap() const;

    AppContext& context_;
    dungeon::Dungeon dungeon_;
    std::vector<dungeon::RoomLayout> layouts_;  // realized once, from pristine state
    int currentRoom_ = 0;
    town::Tilemap roomMap_;
    int originX_ = 0;  // pixel offset centering the room in the viewport
    int originY_ = 0;
    Rect player_;
    Vec2 facing_{0.0f, 1.0f};

    std::vector<DoorTile> doorTiles_;
    std::vector<Marker> markers_;
    const Marker* facingMarker_ = nullptr;
    bool onChest_ = false;

    std::vector<danger::Tier> teamTier_;  // precomputed danger per team
    RunStats run_;

    // Pending battle context (applied in onResume).
    EncounterKind pendingKind_ = EncounterKind::None;
    int pendingRoom_ = 0;
    int pendingTeamIndex_ = -1;
    dungeon::Dir pendingGateDir_ = dungeon::Dir::North;
    battle::BattleResult battleResult_;

    std::string message_;
    float messageTimer_ = 0.0f;
    float moveX_ = 0.0f;
    float moveY_ = 0.0f;
    float walkTime_ = 0.0f;   // walk-cycle clock; 0 while standing
    float worldTime_ = 0.0f;  // always advancing (indicator pulses)
    bool moving_ = false;
};

}  // namespace cd
