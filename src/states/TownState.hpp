#pragma once

#include "core/Geometry.hpp"
#include "states/GameState.hpp"
#include "town/TownData.hpp"

namespace cd {

struct AppContext;

// The walkable town hub: move a player around a compact centred tilemap (M50),
// stand on a building's door and press Confirm to enter it. The town edges are
// walk-through road triggers (no Confirm) that travel to the neighbouring towns
// or the castle. Menu/Cancel opens the pause overlay.
class TownState : public GameState {
public:
    // `entry` decides the arrival spawn (M50): defaults to the plaza spawn, so
    // New Game / load construct it unchanged.
    explicit TownState(StateStack& stack, AppContext& context,
                       town::TownEntry entry = town::TownEntry::Default);

    void onEnter() override;
    void onResume() override;
    void handleInput(const Input& input) override;
    void update(float dt) override;
    void render() override;

#ifdef CRYSTAL_CAPTURE
    // Capture-only (M50): park the player on the (unlocked) west road trigger,
    // disarmed so it renders the walk-through affordance without travelling.
    void captureStandAtWestExit();
#endif

private:
    const town::Building* buildingAtPlayerTile() const;
    const town::TownExit* exitAtPlayerTile() const;
    void enterLocation(const town::Building& building);
    // (re)build the layout for party.currentTown, placing the player per `entry`.
    void buildForCurrentTown(town::TownEntry entry = town::TownEntry::Default);
    void travelTo(int destTown, town::TownEntry entry);  // M32/M50: switch towns in place
    void applyTownAudio();        // town-indexed music + ambience

    bool blackMarketHere() const;  // an offer is present and belongs to this town
    bool onBlackMarketTile() const;
    bool onBardTile() const;  // M41: standing on the wandering storyteller's tile

    AppContext& context_;
    town::TownLayout town_;
    // M50: the town is drawn centred inside the M46 stage matte. player_ and every
    // tile query stay in tile-local pixel space (origin 0); only render() adds
    // originX_/originY_ — the DungeonState split.
    int originX_ = 0;
    int originY_ = 0;
    Rect player_;
    Vec2 facing_{0.0f, 1.0f};
    const town::Building* nearDoor_ = nullptr;
    const town::TownExit* nearExit_ = nullptr;
    bool nearMarket_ = false;  // M34: standing on the black-market NPC tile
    bool nearBard_ = false;    // M41: standing on the storyteller's tile
    // M50 anti-bounce latch: a walk-through exit fires only once the player has
    // stood on a non-trigger tile since the last spawn/resume, so arriving next
    // to an edge (or returning from the castle onto the north trigger) cannot
    // instantly travel back.
    bool travelArmed_ = false;
    float moveX_ = 0.0f;
    float moveY_ = 0.0f;
    float walkTime_ = 0.0f;  // walk-cycle clock; 0 while standing (stand frame)
    bool moving_ = false;
};

}  // namespace cd
