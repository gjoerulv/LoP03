#include "states/TownState.hpp"

#include <algorithm>
#include <cmath>
#include <memory>
#include <string>
#include <vector>

#include "audio/AudioManager.hpp"
#include "content/ContentDatabase.hpp"
#include "core/AppContext.hpp"
#include "core/FadeController.hpp"
#include "game/Achievements.hpp"
#include "game/Cutscenes.hpp"  // M97: first-arrival scenes + the finale NPC
#include "game/Party.hpp"
#include "game/Profile.hpp"    // M97: the finale NPC waits on kingDefeated
#include "game/Story.hpp"
#include "game/WorldLadder.hpp"
#include "input/Input.hpp"
#include "input/PromptLabels.hpp"
#include "raylib.h"
#include "resource/ResourceManager.hpp"
#include "states/AchievementToast.hpp"
#include "states/BlackMarketState.hpp"
#include "states/GuildPerkChoiceState.hpp"  // M84
#include "states/MilestoneChoiceState.hpp"  // M63
#include "states/TreasureFightState.hpp"    // M65
#include "states/CastleState.hpp"
#include "game/Summary.hpp"  // M116
#include "states/EndgameSummaryState.hpp"  // M116
#include "states/EventChoiceState.hpp"  // M116
#include "states/CutsceneState.hpp"  // M97
#include "states/EquipShopState.hpp"
#include "states/RoadForkState.hpp"
#include "states/GuildState.hpp"
#include "states/InnState.hpp"
#include "states/ItemShopState.hpp"
#include "states/ScoreboardState.hpp"
#include "states/StoryDialogState.hpp"
#include "states/TrainingHallState.hpp"
#include "states/SlotMenuState.hpp"
#include "states/StateStack.hpp"
#include "states/TutorialPromptState.hpp"
#include "tutorial/Tutorial.hpp"
#include "states/TownMenuState.hpp"
#include "render/SpriteDraw.hpp"
#include "town/Movement.hpp"
#include "ui/UiDraw.hpp"
#include "ui/UiStyle.hpp"

namespace cd {

namespace {

constexpr float kPlayerSpeed = 72.0f;
constexpr float kPlayerSize = 12.0f;

// M41: the wandering storyteller's tile lives in town/TownData.hpp since M88
// (town::kBardTileX/Y) — the layout header is the single authority the bard,
// the market tiles, and the tests all read.

Color tileColor(town::Tile tile) {
    switch (tile) {
        case town::Tile::Ground: return Color{72, 104, 72, 255};
        case town::Tile::Path: return Color{120, 108, 80, 255};
        case town::Tile::Grass: return Color{84, 126, 80, 255};
        case town::Tile::Tree: return Color{30, 66, 40, 255};
        case town::Tile::Water: return Color{52, 84, 150, 255};
        case town::Tile::Building: return Color{112, 100, 120, 255};
        // M69: the interact tile reads as the doorstep path, not a flat door.
        case town::Tile::Door: return Color{120, 108, 80, 255};
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
        case town::Tile::Door: return "path";  // M69: doorstep, not a flat door
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
        // M69: the doors moved INTO the facades (structureSpriteId below); the
        // trigger tile itself is the doorstep.
        case town::Tile::Door: return "tiles.town.path";
    }
    return "";
}

// M69: the structure's exterior, drawn over its Building tiles — the five
// south-facing facades with integrated doors, the scoreboard stele, and the
// save crystal. The generic Building tiles beneath remain the fallback for a
// missing texture.
const char* structureSpriteId(town::LocationId id) {
    switch (id) {
        case town::LocationId::Inn: return "building.inn";
        case town::LocationId::ItemShop: return "building.item_shop";
        case town::LocationId::EquipShop: return "building.equip_shop";
        case town::LocationId::Guild: return "building.guild";
        case town::LocationId::TrainingHall: return "building.training_hall";
        case town::LocationId::Scoreboard: return "prop.scoreboard";
        case town::LocationId::SavePoint: return "prop.save_crystal";
    }
    return nullptr;
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

TownState::TownState(StateStack& stack, AppContext& context, town::TownEntry entry)
    : GameState(stack), context_(context), town_(town::buildTown()) {
    buildForCurrentTown(entry);
}

void TownState::buildForCurrentTown(town::TownEntry entry) {
    const int town = clampTown(context_.party.currentTown);
    const bool hasPrev = town > 1;
    const bool hasNext = town < kTownCount;
    const bool nextUnlocked = context_.party.highestUnlockedTown > town;
    const bool hasCastle = town == kTownCount;  // M40: the castle road leaves town 7
    town_ = town::buildTown(town, hasPrev, hasNext, nextUnlocked, hasCastle,
                            context_.party.castleUnlocked);
    // M50: centre the town inside the M46 stage matte, above the footer strip.
    // player_ stays in tile-local space; only render() adds this offset.
    originX_ = (context_.virtualWidth - town_.map.width() * town::Tilemap::kTileSize) / 2;
    originY_ = (context_.virtualHeight - ui::style::kFooterHeight -
                town_.map.height() * town::Tilemap::kTileSize) / 2;
    // Arrival spawn per entrance (M50), centred inside the spawn tile.
    const Vec2 spawn = town::townEntrySpawnPixel(entry);
    const float inset = (town::Tilemap::kTileSize - kPlayerSize) * 0.5f;
    player_ = Rect{spawn.x + inset, spawn.y + inset, kPlayerSize, kPlayerSize};
    nearDoor_ = nullptr;
    nearExit_ = nullptr;
    travelArmed_ = false;  // disarm walk-through exits until the player steps off
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

void TownState::travelTo(int destTown, town::TownEntry entry) {
    context_.party.currentTown = clampTown(destTown);
    context_.audio.play(Sfx::Door);
    buildForCurrentTown(entry);
    context_.fade.start();
    applyTownAudio();
    // M98: deferred — a first arrival at towns 2..7 pushes a story scene just
    // below; the prompt must follow the scene, not interrupt it.
    queueTutorial(tutorial::kFirstTravel);
    // M97: the hooded stranger meets the party on their FIRST arrival at each
    // new town (2..7). Marked seen before the push, so a save written after
    // the scene can never replay it; a reload from the entry stands here too.
    const std::string sceneId = game::townCutsceneId(clampTown(context_.party.currentTown));
    if (!sceneId.empty() && context_.content.findCutscene(sceneId) != nullptr &&
        !game::cutsceneSeen(context_.party, sceneId)) {
        game::markCutsceneSeen(context_.party, sceneId);
        stack().pushState(
            std::make_unique<CutsceneState>(stack(), context_, sceneId, /*replay=*/false));
    }
}

void TownState::onEnter() {
    context_.fade.start();
    applyTownAudio();
    // M98: deferred — at New Game the prologue cutscene is queued above this
    // state; the welcome prompt fires after it finishes, never over it.
    queueTutorial(tutorial::kTownWelcome);
    // M63: an old save (or a fresh load) may carry earned-but-unchosen level
    // milestones — prompt once on arrival. Event-driven sites (battle XP, the
    // Training Hall, the Elder Root) cover everything after this.
    maybePushMilestoneChoice(stack(), context_);
    // M84: same rule for a postponed town-milestone perk — the gauntlet's
    // first victory offered it; the town keeps offering until it is chosen.
    maybePushGuildPerkChoice(stack(), context_);
}

void TownState::playNextStrangerBeat() {
    // The M100 joke cycle, verbatim: one dry joke from the authored pool,
    // cycled by the persisted counter; no jokes authored -> retell the
    // finale rather than go mute. No rewards, no re-grants.
    const std::string joke =
        game::nextStrangerJokeId(context_.content, context_.party.strangerJokesTold);
    if (!joke.empty()) {
        context_.audio.play(Sfx::Confirm);
        ++context_.party.strangerJokesTold;
        stack().pushState(
            std::make_unique<CutsceneState>(stack(), context_, joke, /*replay=*/true));
    } else if (context_.content.findCutscene("finale") != nullptr) {
        context_.audio.play(Sfx::Confirm);
        stack().pushState(
            std::make_unique<CutsceneState>(stack(), context_, "finale", /*replay=*/false));
    }
}

void TownState::onResume() {
    context_.fade.start();
    applyTownAudio();
    // M50: a sub-state may have left the player standing on a trigger — notably
    // the castle return, which pops back onto the north road tile. Disarm the
    // walk-through latch until they step off, so nothing travels on resume.
    travelArmed_ = false;
    // A run cleared in this town may have unlocked the road onward (M32). Only
    // the next-town exit's locked state can change while sub-states are on top
    // (currentTown never changes without a rebuild), so refresh it in place
    // rather than rebuilding — which would teleport the player off a shop door.
    const int town = clampTown(context_.party.currentTown);
    for (town::TownExit& e : town_.exits) {
        if (e.toNext && !e.toCastle) {
            e.locked = context_.party.highestUnlockedTown <= town;
        }
        if (e.toCastle) {  // M40: a town-7 clear opens the castle road in place
            e.locked = !context_.party.castleUnlocked;
        }
    }
    // Fires once, after the player has seen their first run's reckoning.
    if (context_.tutorial.state.seen.count(tutorial::kResultFirst) > 0) {
        queueTutorial(tutorial::kTownReturn);  // M98: deferred (see queueTutorial)
    }
    // M89: arriving with fallen members (the dungeon carry-out replaced the
    // old free full heal) teaches the new defeat price exactly once.
    for (const Character& m : context_.party.members) {
        if (m.hp <= 0) {
            queueTutorial(tutorial::kCarriedOut);  // M98: deferred
            break;
        }
    }
    // M116: the End-game Summary unlocks the moment the finale's keepsake
    // choice is recorded (the finale pops back here) and shows itself ONCE;
    // pushed before the toasts so a finale toast dismisses first and reveals
    // the summary beneath. The game never ends: the counters keep updating
    // and P offers the summary from then on.
    if (shouldAutoShowSummary(context_.party)) {
        context_.party.summaryShown = true;
        stack().pushState(
            std::make_unique<EndgameSummaryState>(stack(), context_, /*firstShow=*/true));
    }
    // M42: catch achievements earned by a town action (e.g. equipping a passive or
    // training to level 50); the check is cheap and only toasts newly-unlocked ones.
    pushAchievementToasts(stack(), context_, AchvContext{});
}

#ifdef CRYSTAL_CAPTURE
void TownState::captureStandAtWestExit() {
    for (const town::TownExit& e : town_.exits) {
        if (!e.toNext && !e.toCastle) {  // the west (previous-town) road
            const float inset = (town::Tilemap::kTileSize - kPlayerSize) * 0.5f;
            player_ = Rect{static_cast<float>(e.tileX) * town::Tilemap::kTileSize + inset,
                           static_cast<float>(e.tileY) * town::Tilemap::kTileSize + inset,
                           kPlayerSize, kPlayerSize};
            facing_ = Vec2{-1.0f, 0.0f};
            travelArmed_ = false;  // stay parked on the trigger for the capture
            nearExit_ = &e;
            return;
        }
    }
}
#endif

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

void TownState::queueTutorial(const char* beatId) {
    // Seen-set dedupe happens at fire time (takeBeat); this only prevents the
    // same beat queuing twice before its first flush.
    if (std::find(pendingBeats_.begin(), pendingBeats_.end(), beatId) == pendingBeats_.end()) {
        pendingBeats_.push_back(beatId);
    }
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

bool TownState::onBardTile() const {
    const int ts = town::Tilemap::kTileSize;
    const int tx = static_cast<int>((player_.x + player_.w * 0.5f) / ts);
    const int ty = static_cast<int>((player_.y + player_.h * 0.5f) / ts);
    return tx == town::kBardTileX && ty == town::kBardTileY;
}

bool TownState::gooseNpcHere() const {  // M97
    // Town 7's eastern roadside, once the King has ever fallen (the profile
    // flag, like the class unlocks — the stranger waits for anyone who knows).
    return clampTown(context_.party.currentTown) == 7 && context_.profile.data.kingDefeated;
}

bool TownState::onGooseNpcTile() const {  // M97
    if (!gooseNpcHere()) {
        return false;
    }
    const int ts = town::Tilemap::kTileSize;
    const int tx = static_cast<int>((player_.x + player_.w * 0.5f) / ts);
    const int ty = static_cast<int>((player_.y + player_.h * 0.5f) / ts);
    return tx == town::kGooseNpcTileX && ty == town::kGooseNpcTileY;
}

bool TownState::digHere() const {  // M65
    return context_.party.treasure.active &&
           context_.party.treasure.town == clampTown(context_.party.currentTown);
}

bool TownState::onDigTile() const {  // M65
    if (!digHere()) {
        return false;
    }
    const int ts = town::Tilemap::kTileSize;
    const int tx = static_cast<int>((player_.x + player_.w * 0.5f) / ts);
    const int ty = static_cast<int>((player_.y + player_.h * 0.5f) / ts);
    return tx == kDigTileX && ty == kDigTileY;
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
    // M88: the monuments (Scoreboard, Save Point) answer from any orthogonally
    // adjacent tile, not just the marked doorstep. Doors keep priority above so
    // a tile that is somehow both resolves to the exact door first.
    for (const town::Building& b : town_.buildings) {
        if (town::monumentInteractsFromAllSides(b.id) && town::tileAdjacentToBuilding(b, tx, ty)) {
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
        } else if (nearDig_) {
            // M65: the puzzle map's dig spot — the guarded treasure fight.
            context_.audio.play(Sfx::Confirm);
            stack().pushState(std::make_unique<TreasureFightState>(stack(), context_));
        } else if (nearBard_) {
            // M41: hear the storyteller's installment for this town, and remember it.
            const int town = clampTown(context_.party.currentTown);
            if (const content::StoryBeat* beat = context_.content.findStoryBeat(town)) {
                context_.audio.play(Sfx::Confirm);
                context_.party.storyMet |= storyBit(town);
                stack().pushState(std::make_unique<StoryDialogState>(
                    stack(), context_, beat->speaker, beat->title, beat->body));
            }
        } else if (nearGoose_) {
            // M97/M100: the roadside stranger. The finale plays until its
            // choice is RECORDED (so a quit-mid-scene first play can still
            // earn its keepsake — the M97 guard); once the story is truly
            // done, every visit is one dry joke from the authored pool,
            // cycled by a persisted counter. No rewards, no re-grants.
            const bool finaleDone = !game::cutsceneChoiceFor(context_.party, "finale").empty();
            if (!finaleDone) {
                if (context_.content.findCutscene("finale") != nullptr) {
                    context_.audio.play(Sfx::Confirm);
                    game::markCutsceneSeen(context_.party, "finale");
                    stack().pushState(std::make_unique<CutsceneState>(
                        stack(), context_, "finale", /*replay=*/false));
                }
            } else {
                // M116: once the story is done, P offers a talk or the
                // End-game Summary (Cancel steps away with nothing spent).
                context_.audio.play(Sfx::Confirm);
                stack().pushState(std::make_unique<EventChoiceState>(
                    stack(), context_, "THE STRANGER \"P\"",
                    std::vector<std::string>{"Talk", "End-game Summary"}, [this](int pick) {
                        if (pick == 0) {
                            playNextStrangerBeat();
                        } else {
                            stack().pushState(std::make_unique<EndgameSummaryState>(
                                stack(), context_, /*firstShow=*/false));
                        }
                    }));
            }
        }
        // M50: town exits are walk-through triggers now — no Confirm here. Travel
        // is resolved in update() when the player walks onto an armed road tile.
    }
    if (input.pressed(InputAction::Menu) || input.pressed(InputAction::Cancel)) {
        context_.audio.play(Sfx::Confirm);
        stack().pushState(std::make_unique<TownMenuState>(stack(), context_));
    }
}

void TownState::update(float dt) {
    // M98: flush one deferred tutorial prompt per frame, and only while this
    // state is the active top — never over a cutscene or another modal. The
    // first prompt therefore follows the New Game prologue instead of
    // interrupting it (owner report, 2026-08-16).
    if (!pendingBeats_.empty() && stack().top() == this) {
        const char* beat = pendingBeats_.front();
        pendingBeats_.erase(pendingBeats_.begin());
        maybeTutorialPrompt(stack(), context_, beat);
    }
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
    nearDig_ = (nearDoor_ == nullptr && nearExit_ == nullptr && !nearMarket_) && onDigTile();
    nearBard_ = (nearDoor_ == nullptr && nearExit_ == nullptr && !nearMarket_ && !nearDig_) &&
                onBardTile();
    nearGoose_ = (nearDoor_ == nullptr && nearExit_ == nullptr && !nearMarket_ && !nearDig_ &&
                  !nearBard_) &&
                 onGooseNpcTile();  // M97

    // M50: walk-through travel. The latch arms once the player is off every
    // trigger, so arriving beside an edge (or resuming onto the castle road)
    // cannot instantly bounce. A locked road never travels — its footer hint is
    // shown instead, exactly as the old Confirm exit behaved.
    if (nearExit_ == nullptr) {
        travelArmed_ = true;
    } else if (travelArmed_ && !nearExit_->locked) {
        const town::TownExit exit = *nearExit_;  // copy: travel rebuilds town_
        travelArmed_ = false;
        if (exit.toCastle) {
            context_.audio.play(Sfx::Door);  // M40: climb to the castle (not a town)
            if (context_.party.gooseTownUnlocked) {
                // M61: with Goose Town open, the north road forks — one prompt,
                // two destinations, Cancel stepping back onto the road.
                stack().pushState(std::make_unique<RoadForkState>(stack(), context_));
            } else {
                stack().pushState(std::make_unique<CastleState>(stack(), context_));
            }
        } else {
            // Travelling east (toNext) lands you at the destination's WEST road,
            // and vice-versa, so movement reads as continuous.
            travelTo(exit.destTown,
                     exit.toNext ? town::TownEntry::FromWest : town::TownEntry::FromEast);
        }
    }
}

void TownState::render() {
    const int ts = town::Tilemap::kTileSize;
    const town::Tilemap& map = town_.map;
    const int ox = originX_;
    const int oy = originY_;
    const ui::style::Palette& pal = ui::style::palette();

    const int townIdx = clampTown(context_.party.currentTown);
    ClearBackground(pal.canvas);

    // Stage matte (M46, copied from DungeonState): a low-contrast band behind the
    // town's span plus stepped corner brackets, connecting the walkable town to
    // the HUD without painting over it.
    const int townW = map.width() * ts;
    const int townH = map.height() * ts;
    DrawRectangle(0, oy - 8, context_.virtualWidth, townH + 16, pal.panelInset);
    {
        const int bx = ox - 6;
        const int by = oy - 6;
        const int bw = townW + 12;
        const int bh = townH + 12;
        const Color bracket = pal.borderMid;
        DrawRectangle(bx, by, 10, 2, bracket);
        DrawRectangle(bx, by, 2, 10, bracket);
        DrawRectangle(bx + bw - 10, by, 10, 2, bracket);
        DrawRectangle(bx + bw - 2, by, 2, 10, bracket);
        DrawRectangle(bx, by + bh - 2, 10, 2, bracket);
        DrawRectangle(bx, by + bh - 10, 2, 10, bracket);
        DrawRectangle(bx + bw - 10, by + bh - 2, 10, 2, bracket);
        DrawRectangle(bx + bw - 2, by + bh - 10, 2, 10, bracket);
    }

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
                DrawTexture(context_.resources.texture(id), ox + tx * ts, oy + ty * ts, WHITE);
            } else {
                DrawRectangle(ox + tx * ts, oy + ty * ts, ts, ts, tileColor(tile));
            }
        }
    }

    // M69: the structures' real exteriors, over their generic Building tiles.
    for (const town::Building& b : town_.buildings) {
        const char* spr = structureSpriteId(b.id);
        if (spr != nullptr && context_.resources.hasTexture(spr)) {
            DrawTexture(context_.resources.texture(spr), ox + b.x * ts, oy + b.y * ts, WHITE);
        }
    }

    // Building name labels above each building.
    for (const town::Building& b : town_.buildings) {
        const int cx = ox + b.x * ts + b.w * ts / 2;
        ui::drawTextCentered(b.name.c_str(), cx, oy + b.y * ts - 10, ui::style::kFontSmall, pal.text);
    }

    // Exit signposts by each road (M50: side roads mid-height, castle road north).
    for (const town::TownExit& e : town_.exits) {
        const int cx = ox + e.tileX * ts + ts / 2;
        std::string label;
        if (e.toCastle) {
            label = e.locked ? "Castle (locked)" : "^ Castle";
        } else {
            label = e.locked ? "Locked"
                    : e.toNext ? "Town " + std::to_string(e.destTown) + " >"
                               : "< Town " + std::to_string(e.destTown);
        }
        const Color col = e.locked ? pal.dangerText : pal.gold;
        // Castle label below its top gap; side roads above the edge tile.
        const int labelY = e.toCastle ? oy + (e.tileY + 1) * ts + 1 : oy + (e.tileY - 1) * ts - 1;
        ui::drawTextCentered(label.c_str(), cx, labelY, ui::style::kFontSmall, col);
    }

    // Black-market NPC (M34): a hooded dealer at the offer's seeded plaza tile.
    if (blackMarketHere()) {
        const int mx = context_.party.blackMarket.tileX;
        const int my = context_.party.blackMarket.tileY;
        const float mcx = ox + mx * ts + ts * 0.5f;
        const float mcy = oy + my * ts + ts * 0.5f;
        if (!render::drawTextureCentered(context_.resources, "actor.market.overworld", mcx, mcy)) {
            DrawRectangle(ox + mx * ts + 3, oy + my * ts + 2, ts - 6, ts - 4,
                          Color{120, 70, 150, 255});
        }
        ui::drawTextCentered("Black Market", ox + mx * ts + ts / 2, oy + my * ts - 10, ui::style::kFontSmall,
                             ui::lighten(pal.magic, 40));
    }

    // M65: the treasure dig spot — a bold X on the plaza while the puzzle map
    // points at THIS town. Primitives (world-space art), no asset needed.
    if (digHere()) {
        const int dx = ox + kDigTileX * ts;
        const int dy = oy + kDigTileY * ts;
        for (int s = 2; s < ts - 2; ++s) {
            DrawRectangle(dx + s, dy + s, 2, 2, Color{224, 96, 84, 255});
            DrawRectangle(dx + ts - s, dy + s, 2, 2, Color{224, 96, 84, 255});
        }
        ui::drawTextCentered("Dig Site", dx + ts / 2, dy - 10, ui::style::kFontSmall, pal.danger);
    }

    // Wandering storyteller (M41): always present at a fixed plaza tile in every town.
    {
        const float bcx = ox + town::kBardTileX * ts + ts * 0.5f;
        const float bcy = oy + town::kBardTileY * ts + ts * 0.5f;
        if (!render::drawTextureCentered(context_.resources, "actor.bard.overworld", bcx, bcy)) {
            DrawRectangle(ox + town::kBardTileX * ts + 3, oy + town::kBardTileY * ts + 2, ts - 6,
                          ts - 4, Color{150, 120, 60, 255});
        }
        ui::drawTextCentered("Storyteller", ox + town::kBardTileX * ts + ts / 2,
                             oy + town::kBardTileY * ts - 10, ui::style::kFontSmall, pal.gold);
    }

    // M97: the hooded stranger at the would-be Town-8 roadside (town 7,
    // after the King has fallen) — the finale's door.
    if (gooseNpcHere()) {
        const float gcx = ox + town::kGooseNpcTileX * ts + ts * 0.5f;
        const float gcy = oy + town::kGooseNpcTileY * ts + ts * 0.5f;
        if (!render::drawTextureCentered(context_.resources, "actor.hooded_goose.overworld", gcx,
                                         gcy)) {
            DrawRectangle(ox + town::kGooseNpcTileX * ts + 3, oy + town::kGooseNpcTileY * ts + 2,
                          ts - 6, ts - 4, Color{120, 100, 150, 255});
        }
        ui::drawTextCentered("\"P\"", ox + town::kGooseNpcTileX * ts + ts / 2,  // M100
                             oy + town::kGooseNpcTileY * ts - 10, ui::style::kFontSmall, pal.gold);
    }

    // Player: directional walk animation, then static sprite, then rectangle
    // (with the old facing tick, which the sprites encode themselves).
    const float pcx = ox + player_.x + player_.w * 0.5f;
    const float pcy = oy + player_.y + player_.h * 0.5f;
    const render::Facing facing = render::facingFrom(facing_.x, facing_.y);
    if (!render::drawAnimationCentered(context_.resources, walkAnimId(facing),
                                       moving_ ? walkTime_ : 0.0f, pcx, pcy) &&
        !render::drawTextureCentered(context_.resources, "actor.player.overworld", pcx, pcy)) {
        DrawRectangle(static_cast<int>(ox + player_.x), static_cast<int>(oy + player_.y),
                      static_cast<int>(player_.w), static_cast<int>(player_.h),
                      Color{236, 224, 128, 255});
        DrawRectangle(static_cast<int>(pcx + facing_.x * 4.0f - 1.0f),
                      static_cast<int>(pcy + facing_.y * 4.0f - 1.0f), 3, 3,
                      Color{60, 50, 30, 255});
    }

    // Screen-space HUD (not offset): chips and the footer sit over the matte.
    // Party HUD (top-left) as a chip, sized to the measured text so long
    // names never spill past it.
    std::string hud = "Party:";
    for (const Character& c : context_.party.members) {
        hud += " " + c.name;
    }
    ui::drawChip(hud, 4, 4, pal.crystal);

    // Town indicator (top-right): which town of the ladder this is (M32).
    ui::drawChipRight("Town " + std::to_string(townIdx) + "/" + std::to_string(kTownCount),
                      context_.virtualWidth - 4, 4, pal.gold);

    // Interaction prompt, generated from the live bindings (M13).
    const int h = context_.virtualHeight;
    const InputMap& bindings = context_.input.map();
    const ActiveDevice device = context_.input.activeDevice();
    if (nearDoor_ != nullptr) {
        ui::drawFooterHints({}, context_.virtualWidth, h, "town.footer");
        const std::string text =
            input::prompt(bindings, InputAction::Confirm, device, "Enter " + nearDoor_->name) +
            "   " + input::prompt(bindings, InputAction::Menu, device, "Pause");
        ui::drawTextCentered(text.c_str(), context_.virtualWidth / 2, h - 12, ui::style::kFontSmall, pal.text);
    } else if (nearMarket_) {
        ui::drawFooterHints({}, context_.virtualWidth, h, "town.footer");
        const std::string text =
            input::prompt(bindings, InputAction::Confirm, device, "Browse the Black Market") +
            "   " + input::prompt(bindings, InputAction::Menu, device, "Pause");
        ui::drawTextCentered(text.c_str(), context_.virtualWidth / 2, h - 12, ui::style::kFontSmall,
                             ui::lighten(pal.magic, 40));
    } else if (nearDig_) {
        // M65: the fight is announced before the shovel bites.
        ui::drawFooterHints({}, context_.virtualWidth, h, "town.footer");
        const std::string text =
            input::prompt(bindings, InputAction::Confirm, device,
                          "Dig for the treasure - its guardian will not like it") +
            "   " + input::prompt(bindings, InputAction::Menu, device, "Pause");
        ui::drawTextCentered(text.c_str(), context_.virtualWidth / 2, h - 12, ui::style::kFontSmall, pal.danger);
    } else if (nearBard_) {
        ui::drawFooterHints({}, context_.virtualWidth, h, "town.footer");
        const std::string text =
            input::prompt(bindings, InputAction::Confirm, device, "Hear the storyteller") + "   " +
            input::prompt(bindings, InputAction::Menu, device, "Pause");
        ui::drawTextCentered(text.c_str(), context_.virtualWidth / 2, h - 12, ui::style::kFontSmall, pal.gold);
    } else if (nearGoose_) {
        // M97: the finale invitation — quiet gold, like the storyteller's.
        ui::drawFooterHints({}, context_.virtualWidth, h, "town.footer");
        const std::string text =
            input::prompt(bindings, InputAction::Confirm, device, "Speak with the stranger") +
            "   " + input::prompt(bindings, InputAction::Menu, device, "Pause");
        ui::drawTextCentered(text.c_str(), context_.virtualWidth / 2, h - 12, ui::style::kFontSmall, pal.gold);
    } else if (nearExit_ != nullptr) {
        // M50: walk-through roads. A locked one explains why and does not travel;
        // an unlocked one only shows here on a castle-return (the latch is holding
        // travel), so the hint is a plain signpost, not a Confirm prompt.
        ui::drawFooterHints({}, context_.virtualWidth, h, "town.footer");
        std::string text;
        if (nearExit_->locked) {
            text = nearExit_->toCastle
                       ? "Clear a town-7 dungeon to open the road to the castle"
                       : "Clear a dungeon in this town to open the road onward";
        } else if (nearExit_->toCastle) {
            text = "Walk up the road to the Castle";
        } else {
            text = "Walk out to Town " + std::to_string(nearExit_->destTown);
        }
        ui::drawTextCentered(text.c_str(), context_.virtualWidth / 2, h - 12, ui::style::kFontSmall,
                             nearExit_->locked ? pal.dangerText : pal.text);
    } else {
        const std::string moveLabel =
            device == ActiveDevice::Keyboard
                ? input::primaryLabel(bindings, InputAction::MoveUp, device) + "/" +
                      input::primaryLabel(bindings, InputAction::MoveDown, device) + "/" +
                      input::primaryLabel(bindings, InputAction::MoveLeft, device) + "/" +
                      input::primaryLabel(bindings, InputAction::MoveRight, device)
                : "D-Pad/Stick";
        ui::drawFooterHints({{moveLabel, "Move"},
                             {input::primaryLabel(bindings, InputAction::Menu, device), "Pause"}},
                            context_.virtualWidth, h, "town.footer");
    }
}

}  // namespace cd
