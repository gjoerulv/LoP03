#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "states/GameState.hpp"
#include "ui/Menu.hpp"
#include "ui/ScrollWindow.hpp"

// M55 — The Armory Ghost (Ruined Keep rite). The player offers one inventory
// equipment piece and the ghost returns a seeded random piece one rarity tier up
// in the SAME slot, sight unseen (an epic can yield a legendary). Legendary input
// is declined. Pushed by DungeonState::resolveEvent; on a successful trade it
// consumes the offered piece, adds the upgrade, and marks the RoomEvent resolved
// (DungeonState rebuilds the room's markers when this state closes).

namespace cd {

class StateStack;
struct AppContext;
class Input;
namespace dungeon {
struct RoomEvent;
}

class ArmoryGhostState : public GameState {
public:
    ArmoryGhostState(StateStack& stack, AppContext& context, std::uint64_t seed, int roomIndex,
                     dungeon::RoomEvent* event);

    void handleInput(const Input& input) override;
    void render() override;

private:
    void rebuild();
    void trade();

    AppContext& context_;
    std::uint64_t seed_;
    int roomIndex_;
    dungeon::RoomEvent* event_;
    ui::Menu menu_;
    ui::ScrollWindow scroll_;
    std::vector<std::string> ids_;  // eligible inventory piece ids, parallel to rows
    std::string message_;
    bool done_ = false;  // a trade resolved; Confirm/Cancel now just closes
};

}  // namespace cd
