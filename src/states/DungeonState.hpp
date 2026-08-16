#pragma once

#include <optional>
#include <string>
#include <vector>

#include "battle/Battle.hpp"
#include "core/Geometry.hpp"
#include "danger/DangerRating.hpp"
#include "game/Dragonform.hpp"  // M93
#include "game/RunStats.hpp"
#include "game/Spoils.hpp"
#include "dungeon/DungeonModel.hpp"
#include "dungeon/RoomLayout.hpp"
#include "states/GameState.hpp"
#include "town/Tilemap.hpp"
#include "ui/TextViewport.hpp"

namespace cd {

struct AppContext;
namespace content {
struct EventFlavorDef;
}

// Walkable dungeon explorer. Each room is a compact archetype layout realized
// from a derived room-local seed (M16), drawn centered in the exploration
// viewport; walk room to room through open doors. Facing a team and pressing
// Confirm starts a battle.
// Victory clears the gate (or chest guard); beating the boss completes the
// dungeon; defeat ends the run. Inspect, open chests, retreat to town.
class DungeonState : public GameState {
public:
    // M82: a run is a vector of floors (floors.front() is entered first; a
    // 1-floor vector is every pre-M82 run). The single-Dungeon overload wraps
    // it for the existing callers (capture, tests).
    DungeonState(StateStack& stack, AppContext& context, std::vector<dungeon::Dungeon> floors);
    DungeonState(StateStack& stack, AppContext& context, dungeon::Dungeon dungeon);

    void onEnter() override;   // first-dungeon tutorial beat
    void onResume() override;  // applies a battle outcome
    void handleInput(const Input& input) override;
    void openDetails();  // M22 contextual Details overlay
    void update(float dt) override;
    void render() override;

#ifdef CRYSTAL_CAPTURE
    // Capture-only (M44): jump to the room holding `kind` and stand facing its
    // marker, so the event's footer prompt renders deterministically for the
    // overflow check. Returns false when the dungeon has no such event. Not
    // present in shipping builds.
    bool captureFaceEvent(dungeon::RoomEventKind kind);
    // M80: face the event AND open its flavor panel (false when the dungeon
    // lacks the event or the kind has no authored flavor). M87: an optional
    // pseudo-translated body override stresses the scrolling viewport.
    bool captureOpenEventPanel(dungeon::RoomEventKind kind,
                               const std::string& bodyOverride = "");
    // M80 addendum: show the outcome panel with a representative result.
    void captureShowOutcome(const std::string& title, const std::string& body);
    // M82: clear the current floor's stair-gate, open the stairway, and stand
    // facing it, so the descend prompt + floor chip render deterministically.
    // False on a final floor (no stairway exists there).
    bool captureOpenStairs();
#endif

private:
    // M65 adds MapPiece; M66 adds the treasure-map Chart and the Buried spot;
    // M82 adds the stairway down a multi-floor run (Stairs appears once the
    // floor's stair-gate falls).
    enum class MarkerKind {
        GateTeam, GuardTeam, Boss, Chest, Event, MapPiece, Chart, Buried, Stairs
    };
    // M82: StairGate is the boss-slot fight on floors before the last — its
    // victory opens the stairs instead of completing the run.
    enum class EncounterKind { None, Gate, Guard, Boss, Challenge, StairGate, Patrol };
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
    void takeMapPiece();  // M65: pick up a Secret Map Piece (4th reveals the treasure)
    void readChart();     // M66: the single-use map reveals the buried spot
    void digBuried();     // M66: claim the buried treasure (a curio / a token)
    void resolveEvent();  // applies a non-battle event's stated trade-off
    std::string eventPromptText() const;  // the pre-confirmation trade-off line
    void confirmEventPanel();       // M80: the panel's Confirm — resolve or fight
    // M87: fills the flavor viewport and raises the panel (render stays const).
    void openEventPanel(const content::EventFlavorDef& flavor);
    void renderEventPanel() const;  // M80: the centered flavor + trade-off modal
    // M80 addendum (owner, 2026-08-06): event and chest OUTCOMES ride the same
    // centered treatment instead of the footer line.
    void showOutcome(const std::string& title, std::string body);
    void renderOutcomePanel() const;
    void startBattle(int teamIndex, EncounterKind kind, dungeon::Dir gateDir);
    void completeDungeon();
    void renderMinimap() const;
    // M82: is the current floor the run's last (its boss slot holds the boss)?
    bool finalFloor() const { return dungeon_.floorIndex + 1 >= dungeon_.floorCount; }
    void descendFloor();   // swap in the next floor; one continuous run
    void rebuildTiers();   // snapshot party-relative danger for dungeon_'s teams

    AppContext& context_;
    // M82: the run's floors. dungeon_ holds the CURRENT floor (moved out of
    // this vector — descent is one-way, so spent floors stay moved-from).
    // Declared before dungeon_: the constructor moves floors_.front() into it.
    std::vector<dungeon::Dungeon> floors_;
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
    bool onMapPiece_ = false;  // M65: standing on a Secret Map Piece
    bool onChart_ = false;     // M66: standing on the dungeon treasure map
    bool onBuried_ = false;    // M66: standing on the (revealed) buried spot
    bool chartFound_ = false;  // M66: the map was read this run
    // M93: the Surveyor's paid reveal — lifts the multi-floor fog for the
    // CURRENT floor only (reset on descent, like the chart).
    bool floorRevealed_ = false;
    // M93: Dragonform — armed by the event, spent on the next battle start
    // (the party fights it as Dragons; the reckoning docks a flat 100).
    bool dragonformArmed_ = false;
    int dragonformFights_ = 0;  // battles actually fought in dragonform (score line)
    DragonformStash dragonformStash_;  // the real members while a fight runs borrowed
    // M93: the danger counter (owner decisions 5/7): a visible 100-step
    // countdown; at 0 a seeded patrol attacks immediately, the counter
    // resets, and the patrol pays XP but no gold, items, or danger credit.
    int dangerSteps_ = 100;
    int patrolIndex_ = 0;  // how many patrols this run has rolled (seeds the next)
    int lastTileX_ = -1;   // the tile whose leaving ticked the counter last
    int lastTileY_ = -1;
    // M80: the centered event-flavor panel is open (movement and the other
    // dungeon inputs pause; Confirm accepts, Cancel steps away).
    bool eventPanelOpen_ = false;
    // M80 addendum: the outcome panel (event/chest results); any of
    // Confirm/Cancel/Menu dismisses it.
    bool outcomePanelOpen_ = false;
    std::string outcomeTitle_;
    std::string outcomeBody_;
    // M87: both panels' bodies are bounded scrollable viewports — the panel
    // (not the text) owns the height and Up/Down reaches the rest, so an
    // authored/translated body of any length works. The fixed trade-off line
    // and the control hints never scroll away.
    ui::TextViewport eventFlavorView_;
    ui::TextViewport outcomeView_;

    std::vector<danger::Tier> teamTier_;  // precomputed danger per team
    RunStats run_;
    cd::RunStats victoryStats_;  // M42: this-run victory tallies (damage/hits/MVP)

    // Pending battle context (applied in onResume).
    EncounterKind pendingKind_ = EncounterKind::None;
    int pendingRoom_ = 0;
    int pendingTeamIndex_ = -1;
    dungeon::Dir pendingGateDir_ = dungeon::Dir::North;
    battle::BattleResult battleResult_;
    // M68: the pending battle's payout. The battle applies it on Victory and
    // shows the results panel; must outlive the battle (like battleResult_).
    BattleSpoils pendingSpoils_;
    // M67: set by completeDungeon. The next resume pops this state, so the
    // return to town survives anything pushed between the dungeon and the
    // result screen (the M63 level-up modal a boss kill can wedge there).
    bool runComplete_ = false;

    std::string message_;
    float messageTimer_ = 0.0f;
    float moveX_ = 0.0f;
    float moveY_ = 0.0f;
    float walkTime_ = 0.0f;   // walk-cycle clock; 0 while standing
    float worldTime_ = 0.0f;  // always advancing (indicator pulses)
    bool moving_ = false;
};

}  // namespace cd
