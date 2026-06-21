#pragma once

#include "core/Geometry.hpp"
#include "states/GameState.hpp"
#include "town/TownData.hpp"

namespace cd {

struct AppContext;

// The walkable town hub: move a player around a single-screen tilemap, stand on
// a building's door and press Confirm to enter it. Menu/Cancel opens the pause
// overlay (Resume / Quit to Title).
class TownState : public GameState {
public:
    TownState(StateStack& stack, AppContext& context);

    void onEnter() override;
    void onResume() override;
    void handleInput(const Input& input) override;
    void update(float dt) override;
    void render() override;

private:
    const town::Building* buildingAtPlayerTile() const;
    void enterLocation(const town::Building& building);

    AppContext& context_;
    town::TownLayout town_;
    Rect player_;
    Vec2 facing_{0.0f, 1.0f};
    const town::Building* nearDoor_ = nullptr;
    float moveX_ = 0.0f;
    float moveY_ = 0.0f;
};

}  // namespace cd
