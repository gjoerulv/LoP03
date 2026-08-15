#include "states/DungeonState.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <memory>
#include <string>
#include <utility>

#include "audio/AudioManager.hpp"
#include "battle/Battle.hpp"
#include "content/ContentDatabase.hpp"
#include "core/AppContext.hpp"
#include "core/FadeController.hpp"
#include "game/Achievements.hpp"
#include "game/BossDrops.hpp"
#include "game/Curios.hpp"      // M66: buried-treasure curio awards
#include "game/Milestones.hpp"  // M63: gold bonus + pending-choice prompt
#include "game/ItemCaps.hpp"
#include "game/Party.hpp"
#include "game/Relics.hpp"  // the M44 relic grant (seeded, reload-proof)
#include "game/WorldLadder.hpp"
#include "dungeon/DungeonGenerator.hpp"  // M93 patrolTeam
#include "dungeon/TeamInspect.hpp"  // M88 pre-fight team inspection
#include "game/ScrollTrove.hpp"      // M92 the Guild's trove
#include "states/ScrollChoiceState.hpp"
#include "dungeon/ThemeEvents.hpp"  // M55 per-theme rites
#include "states/ArmoryGhostState.hpp"
#include "input/Input.hpp"
#include "raylib.h"
#include "score/ScoreEntry.hpp"
#include "score/Scoreboard.hpp"
#include "score/Scoring.hpp"
#include "input/PromptLabels.hpp"
#include "render/SpriteDraw.hpp"
#include "resource/ResourceManager.hpp"
#include "settings/Settings.hpp"
#include "render/BattleBackdrop.hpp"
#include "states/AchievementToast.hpp"
#include "states/MilestoneChoiceState.hpp"  // M63
#include "states/BattleState.hpp"
#include "states/BossIntroState.hpp"
#include "states/CelebrationState.hpp"  // M71
#include "states/DungeonMenuState.hpp"
#include "states/DetailsOverlayState.hpp"
#include "states/DungeonResultState.hpp"
#include "states/StateStack.hpp"
#include "states/TutorialPromptState.hpp"
#include "tutorial/Tutorial.hpp"
#include "town/Movement.hpp"
#include "ui/UiDraw.hpp"
#include "ui/UiStyle.hpp"

namespace cd {

namespace {

constexpr int kTile = town::Tilemap::kTileSize;
constexpr float kSpeed = 78.0f;
// M93: the danger counter's fuse — 100 tiles between roused patrols (owner
// decision 5: visible countdown, immediate fight at 0, reset after).
constexpr int kDangerStepsPerPatrol = 100;
constexpr float kPlayerSize = 12.0f;

// M87: the two centered panels (event flavor / outcome) share one geometry
// family; their bodies wrap to this width (the indicator gutter reserved)
// and scroll past the visible budget instead of truncating. Content is set
// when a panel OPENS, so render stays const and no wrapping runs per frame.
constexpr int kPanelBoxW = 360;
constexpr int kPanelTextW = kPanelBoxW - 28 - ui::kScrollGutterW;
constexpr int kEventBodyLines = 4;    // visible flavor lines; more scrolls
constexpr int kOutcomeBodyLines = 3;  // visible outcome lines; more scrolls

Color tileColor(town::Tile t) {
    switch (t) {
        case town::Tile::Building: return Color{32, 28, 44, 255};
        case town::Tile::Door: return Color{96, 84, 60, 255};
        default: return Color{58, 52, 70, 255};
    }
}

Color tierColor(danger::Tier t) {
    switch (t) {
        case danger::Tier::Trivial: return Color{150, 150, 160, 255};
        case danger::Tier::Easy: return Color{120, 200, 120, 255};
        case danger::Tier::Fair: return Color{220, 210, 110, 255};
        case danger::Tier::Dangerous: return Color{230, 150, 80, 255};
        case danger::Tier::Deadly: return Color{225, 90, 90, 255};
        case danger::Tier::Boss: return Color{200, 110, 220, 255};
    }
    return WHITE;
}

// Message-speed setting scales every transient message duration.
float scaledMessageTime(const AppContext& context, float base) {
    return base * settings::messageDurationScale(context.settings.values.messageSpeed);
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

// Team-marker silhouette by stat-derived danger tier: shape, not color,
// carries the differentiation (plain / horned / crowned figures).
const char* tierSilhouetteId(danger::Tier tier) {
    if (tier == danger::Tier::Boss) {
        return "marker.enemy.boss";
    }
    if (tier == danger::Tier::Dangerous || tier == danger::Tier::Deadly) {
        return "marker.enemy.elite";
    }
    return "marker.enemy.normal";
}

// Per-theme dungeon music and ambience (M21, owner-approved model); unknown
// theme ids fall back to the Ruined Keep pair.
MusicTrack themeMusic(const std::string& themeId) {
    if (themeId == "crystal_mine") {
        return MusicTrack::DungeonMine;
    }
    if (themeId == "hollow_forest") {
        return MusicTrack::DungeonForest;
    }
    return MusicTrack::DungeonKeep;
}

AmbienceTrack themeAmbience(const std::string& themeId) {
    if (themeId == "crystal_mine") {
        return AmbienceTrack::Mine;
    }
    if (themeId == "hollow_forest") {
        return AmbienceTrack::Forest;
    }
    return AmbienceTrack::Keep;
}

}  // namespace

DungeonState::DungeonState(StateStack& stack, AppContext& context,
                           std::vector<dungeon::Dungeon> floors)
    : GameState(stack), context_(context), floors_(std::move(floors)),
      dungeon_(std::move(floors_.front())),
      layouts_(dungeon::realizeAllRooms(dungeon_)), roomMap_(1, 1) {
    rebuildTiers();
    context_.party.usedSummons.clear();  // M95: a fresh run, a fresh ledger
    context_.fade.start();
    // Theme music + ambience are applied in onEnter(), not here: entering the
    // dungeon pops the Guild, which fires TownState::onResume and re-asserts
    // the town audio *after* this constructor runs. onEnter() runs after that
    // pop, so the dungeon audio wins (previously the ambience stayed on the
    // town bed for the whole dungeon).
    enterRoom(dungeon_.startRoom, std::nullopt);
}

DungeonState::DungeonState(StateStack& stack, AppContext& context, dungeon::Dungeon dungeon)
    : DungeonState(stack, context,
                   std::vector<dungeon::Dungeon>{std::move(dungeon)}) {}

void DungeonState::rebuildTiers() {
    // M68: tiers are party-relative, snapshotted ONCE per floor at entry so
    // the labels and the danger-defeated score credit agree for the whole
    // floor (mid-run level-ups do not relabel the dungeon under the player).
    teamTier_.clear();
    teamTier_.reserve(dungeon_.teams.size());
    const int partyThreat = danger::partyThreat(context_.party.members);
    for (const dungeon::EnemyTeam& team : dungeon_.teams) {
        teamTier_.push_back(danger::assess(team, context_.content, partyThreat));
    }
}

void DungeonState::descendFloor() {
    // M82: one continuous run — run_ and victoryStats_ keep accumulating; only
    // the floor swaps. Descent is one-way (the spent floor is left moved-from).
    const int next = dungeon_.floorIndex + 1;
    if (next >= static_cast<int>(floors_.size())) {
        return;  // defensive: the last floor has no stairway
    }
    dungeon_ = std::move(floors_[static_cast<std::size_t>(next)]);
    layouts_ = dungeon::realizeAllRooms(dungeon_);
    rebuildTiers();
    chartFound_ = false;  // M66 state is per floor (each floor rolls its own chart)
    floorRevealed_ = false;  // M93: the Surveyor's reveal is per floor too
    context_.audio.play(Sfx::Door);
    context_.fade.start();
    enterRoom(dungeon_.startRoom, std::nullopt);
    message_ = TextFormat("Floor %d of %d", dungeon_.floorIndex + 1, dungeon_.floorCount);
    messageTimer_ = scaledMessageTime(context_, 2.5f);
}

void DungeonState::buildRoom() {
    facingMarker_ = nullptr;
    onChest_ = false;
    doorTiles_.clear();
    markers_.clear();

    dungeon::Room& room = dungeon_.rooms[static_cast<std::size_t>(currentRoom_)];
    const dungeon::RoomLayout& layout = layouts_[static_cast<std::size_t>(currentRoom_)];

    // Base collision from the realized layout (closed border + obstacles);
    // door gaps and encounter blocks are overlaid from live gate/guard state.
    town::Tilemap map(layout.width, layout.height, town::Tile::Ground);
    for (int y = 0; y < layout.height; ++y) {
        for (int x = 0; x < layout.width; ++x) {
            if (layout.at(x, y) == dungeon::RoomLayout::Cell::Wall) {
                map.set(x, y, town::Tile::Building);
            }
        }
    }

    for (dungeon::Dir dir : {dungeon::Dir::North, dungeon::Dir::East, dungeon::Dir::South,
                             dungeon::Dir::West}) {
        const dungeon::Door& door = room.door(dir);
        if (door.neighbor < 0) {
            continue;
        }
        if (door.gated) {
            const dungeon::RoomLayout::Point inner = layout.interiorGap(dir)[0];
            map.set(inner.x, inner.y, town::Tile::Building);  // blocked passage
            markers_.push_back({inner.x, inner.y, MarkerKind::GateTeam, door.teamIndex, dir});
        } else {
            for (const dungeon::RoomLayout::Point& bp : layout.doorGap(dir)) {
                map.set(bp.x, bp.y, town::Tile::Door);
                doorTiles_.push_back({bp.x, bp.y, dir, door.neighbor});
            }
        }
    }

    if (room.type == dungeon::RoomType::Boss && room.teamIndex >= 0 && layout.boss.valid()) {
        map.set(layout.boss.x, layout.boss.y, town::Tile::Building);
        markers_.push_back({layout.boss.x, layout.boss.y, MarkerKind::Boss, room.teamIndex,
                            dungeon::Dir::North});
    }
    // M82: once a stair floor's gate team falls, the stairway down stands where
    // the team stood (same anchored tile, facing-interact like the boss).
    if (room.type == dungeon::RoomType::Boss && room.teamIndex < 0 && !finalFloor() &&
        dungeon_.stairsOpen && layout.boss.valid()) {
        map.set(layout.boss.x, layout.boss.y, town::Tile::Building);
        markers_.push_back({layout.boss.x, layout.boss.y, MarkerKind::Stairs, -1,
                            dungeon::Dir::North});
    }
    if (room.chest.present && layout.chest.valid()) {
        markers_.push_back({layout.chest.x, layout.chest.y, MarkerKind::Chest, -1,
                            dungeon::Dir::North});
        if (room.teamIndex >= 0 && layout.guard.valid()) {
            map.set(layout.guard.x, layout.guard.y, town::Tile::Building);
            markers_.push_back({layout.guard.x, layout.guard.y, MarkerKind::GuardTeam,
                                room.teamIndex, dungeon::Dir::North});
        }
    }
    if (room.type == dungeon::RoomType::Event && !room.event.resolved &&
        layout.event.valid()) {
        map.set(layout.event.x, layout.event.y, town::Tile::Building);
        markers_.push_back({layout.event.x, layout.event.y, MarkerKind::Event, room.teamIndex,
                            dungeon::Dir::North});
    }
    // M65: a Secret Map Piece lies on this room's walkable center tile —
    // stand-on like a chest, never blocking.
    if (currentRoom_ == dungeon_.mapPieceRoom) {
        markers_.push_back({layout.centerSpawn.x, layout.centerSpawn.y, MarkerKind::MapPiece, -1,
                            dungeon::Dir::North});
    }
    // M66: the single-use dungeon treasure map, and — once it is read — the
    // buried spot it revealed. Same stand-on center-tile contract (the
    // generator keeps the three rooms distinct).
    if (currentRoom_ == dungeon_.chartRoom) {
        markers_.push_back({layout.centerSpawn.x, layout.centerSpawn.y, MarkerKind::Chart, -1,
                            dungeon::Dir::North});
    }
    if (chartFound_ && currentRoom_ == dungeon_.buriedRoom) {
        markers_.push_back({layout.centerSpawn.x, layout.centerSpawn.y, MarkerKind::Buried, -1,
                            dungeon::Dir::North});
    }

    roomMap_ = std::move(map);
    originX_ = (context_.virtualWidth - layout.width * kTile) / 2;
    originY_ = (context_.virtualHeight - ui::style::kFooterHeight - layout.height * kTile) / 2;
}

void DungeonState::enterRoom(int index, std::optional<dungeon::Dir> entrySide) {
    currentRoom_ = index;
    dungeon_.rooms[static_cast<std::size_t>(index)].visited = true;
    if (entrySide) {
        context_.audio.play(Sfx::Door);  // through a doorway, not the initial spawn
    }
    buildRoom();

    const dungeon::RoomLayout& layout = layouts_[static_cast<std::size_t>(index)];
    const float inset = (kTile - kPlayerSize) * 0.5f;
    const dungeon::RoomLayout::Point start =
        entrySide ? layout.interiorGap(*entrySide)[0] : layout.centerSpawn;
    player_ = Rect{static_cast<float>(start.x) * kTile + inset,
                   static_cast<float>(start.y) * kTile + inset, kPlayerSize, kPlayerSize};
}

#ifdef CRYSTAL_CAPTURE
bool DungeonState::captureFaceEvent(dungeon::RoomEventKind kind) {
    for (std::size_t i = 0; i < dungeon_.rooms.size(); ++i) {
        if (dungeon_.rooms[i].event.kind != kind) {
            continue;
        }
        enterRoom(static_cast<int>(i), std::nullopt);
        for (const Marker& m : markers_) {
            if (m.kind != MarkerKind::Event) {
                continue;
            }
            // Stand one tile above the marker, looking down at it, so the footer
            // shows the event's trade-off exactly as it does in play.
            const float inset = (kTile - kPlayerSize) * 0.5f;
            player_ = Rect{static_cast<float>(m.x) * kTile + inset,
                           static_cast<float>(m.y - 1) * kTile + inset, kPlayerSize, kPlayerSize};
            facing_ = Vec2{0.0f, 1.0f};
            recomputeInteraction(m.x, m.y - 1);
            return facingMarker_ != nullptr;
        }
    }
    return false;
}

bool DungeonState::captureOpenEventPanel(dungeon::RoomEventKind kind,
                                         const std::string& bodyOverride) {
    if (!captureFaceEvent(kind)) {
        return false;
    }
    const content::EventFlavorDef* flavor =
        context_.content.findEventFlavor(dungeon::eventFlavorId(kind));
    if (flavor == nullptr) {
        return false;
    }
    openEventPanel(*flavor);
    if (!bodyOverride.empty()) {
        // M87: a pseudo-translated body, so the scrolling flavor viewport is
        // lint-checked at expanded length (the title/trade-off stay real).
        eventFlavorView_.setContent(bodyOverride, kPanelTextW, ui::style::kFontBody,
                                    ui::raylibMeasure());
        eventFlavorView_.setVisibleLines(
            std::clamp(eventFlavorView_.lineCount(), 1, kEventBodyLines));
    }
    return true;
}

void DungeonState::captureShowOutcome(const std::string& title, const std::string& body) {
    showOutcome(title, body);
}

bool DungeonState::captureOpenStairs() {
    if (finalFloor()) {
        return false;
    }
    dungeon_.rooms[static_cast<std::size_t>(dungeon_.bossRoom)].teamIndex = -1;
    dungeon_.stairsOpen = true;
    enterRoom(dungeon_.bossRoom, std::nullopt);
    for (const Marker& m : markers_) {
        if (m.kind != MarkerKind::Stairs) {
            continue;
        }
        // The captureFaceEvent stance: one tile above, looking down at it.
        const float inset = (kTile - kPlayerSize) * 0.5f;
        player_ = Rect{static_cast<float>(m.x) * kTile + inset,
                       static_cast<float>(m.y - 1) * kTile + inset, kPlayerSize, kPlayerSize};
        facing_ = Vec2{0.0f, 1.0f};
        recomputeInteraction(m.x, m.y - 1);
        return facingMarker_ != nullptr;
    }
    return false;
}
#endif

void DungeonState::recomputeInteraction(int tx, int ty) {
    onChest_ = false;
    onMapPiece_ = false;  // M65
    onChart_ = false;     // M66 (M67 fix: these latched on forever once
    onBuried_ = false;    // touched — a stuck prompt that also ate Confirm)
    facingMarker_ = nullptr;
    for (const Marker& m : markers_) {
        if (m.kind == MarkerKind::Chest && m.x == tx && m.y == ty) {
            onChest_ = true;
        }
        if (m.kind == MarkerKind::MapPiece && m.x == tx && m.y == ty) {
            onMapPiece_ = true;  // M65: stand-on, like a chest
        }
        if (m.kind == MarkerKind::Chart && m.x == tx && m.y == ty) {
            onChart_ = true;  // M66
        }
        if (m.kind == MarkerKind::Buried && m.x == tx && m.y == ty) {
            onBuried_ = true;  // M66
        }
    }
    const int fx = facing_.x > 0.4f ? 1 : (facing_.x < -0.4f ? -1 : 0);
    const int fy = facing_.y > 0.4f ? 1 : (facing_.y < -0.4f ? -1 : 0);
    for (const Marker& m : markers_) {
        if (m.kind != MarkerKind::Chest && m.kind != MarkerKind::MapPiece &&
            m.kind != MarkerKind::Chart && m.kind != MarkerKind::Buried &&
            m.x == tx + fx && m.y == ty + fy) {
            facingMarker_ = &m;
        }
    }
}

void DungeonState::openChest() {
    dungeon::Room& room = dungeon_.rooms[static_cast<std::size_t>(currentRoom_)];
    if (!room.chest.present) {
        return;
    }
    if (room.chest.opened) {
        showOutcome("The Chest", "It is empty. It was empty the last time, too.");
        return;
    }
    if (room.chest.guarded) {
        message_ = "Guarded - defeat the team first.";
        messageTimer_ = scaledMessageTime(context_, 2.5f);
        return;
    }
    room.chest.opened = true;
    context_.audio.play(Sfx::Chest);
    // M84 (Appraiser's Eye): the perk fattens the PAYOUT at open time. The
    // score's treasureGold keeps the generated amount, so scoring stays
    // comparable across parties (no score-rule motion) — and generation is
    // untouched, the M76 interaction-time precedent.
    const int chestGold =
        room.chest.gold + room.chest.gold * guildChestGoldPct(context_.party.guild) / 100;
    context_.party.gold += chestGold;
    ++run_.chestsOpened;
    run_.treasureGold += room.chest.gold;
    std::string msg = TextFormat("Found %d gold", chestGold);
    if (!room.chest.itemId.empty()) {
        context_.party.inventory.add(room.chest.itemId, 1);
        const char* name = room.chest.itemId.c_str();
        if (const content::ItemDef* it = context_.content.findItem(room.chest.itemId)) {
            name = it->name.c_str();
        }
        msg += std::string(" + ") + name;
    }
    if (room.chest.trapped) {
        // Exactly the wound the prompt warned about: 25% max HP, never fatal.
        // M84 (Trap Sense): the perk shaves percentage points off the wound.
        const int woundPct = 25 - guildTrapGuardPct(context_.party.guild);
        for (Character& c : context_.party.members) {
            if (c.hp > 0) {
                c.hp = std::max(1, c.hp - c.maxHp * (woundPct > 0 ? woundPct : 0) / 100);
            }
        }
        msg = "The trap bites - the party is wounded! " + msg;
    }
    // M80 addendum: chest results ride the outcome panel.
    showOutcome("The Chest", msg);
}

void DungeonState::interact() {
    if (onMapPiece_) {
        takeMapPiece();  // M65
        return;
    }
    if (onChart_) {
        readChart();  // M66
        return;
    }
    if (onBuried_) {
        digBuried();  // M66
        return;
    }
    if (onChest_) {
        openChest();
        return;
    }
    if (facingMarker_ == nullptr) {
        return;
    }
    EncounterKind kind = EncounterKind::None;
    switch (facingMarker_->kind) {
        case MarkerKind::GateTeam: kind = EncounterKind::Gate; break;
        case MarkerKind::GuardTeam: kind = EncounterKind::Guard; break;
        case MarkerKind::Boss:
            // M82: on floors before the last, the boss slot holds the elite
            // stair-gate — its victory opens the stairway, not the reckoning.
            kind = finalFloor() ? EncounterKind::Boss : EncounterKind::StairGate;
            break;
        case MarkerKind::Stairs:
            descendFloor();  // M82: one press, one floor down — the run continues
            return;
        case MarkerKind::Event: {
            // M80: an authored event opens its centered flavor panel first
            // (Confirm inside it commits); an unauthored kind — or a deleted
            // flavor file — keeps the classic immediate path, so flavor can
            // never block an event.
            const dungeon::RoomEvent& ev =
                dungeon_.rooms[static_cast<std::size_t>(currentRoom_)].event;
            const content::EventFlavorDef* flavor =
                context_.content.findEventFlavor(dungeon::eventFlavorId(ev.kind));
            if (!ev.resolved && flavor != nullptr) {
                openEventPanel(*flavor);
                context_.audio.play(Sfx::Interact);
                return;
            }
            if (facingMarker_->teamIndex >= 0) {
                startBattle(facingMarker_->teamIndex, EncounterKind::Challenge,
                            facingMarker_->gateDir);
            } else {
                resolveEvent();
            }
            return;
        }
        case MarkerKind::Chest:
        case MarkerKind::MapPiece:
        case MarkerKind::Chart:
        case MarkerKind::Buried:
            return;
    }
    startBattle(facingMarker_->teamIndex, kind, facingMarker_->gateDir);
}

void DungeonState::readChart() {
    if (dungeon_.chartRoom != currentRoom_ || dungeon_.buriedRoom < 0) {
        return;
    }
    dungeon_.chartRoom = -1;
    chartFound_ = true;
    context_.audio.play(Sfx::Chest);
    message_ =
        "A treasure map of THIS dungeon! An X marks a room - it now glows on your minimap.";
    messageTimer_ = scaledMessageTime(context_, 4.0f);
    buildRoom();  // the chart marker clears (and the X may be in this room)
}

void DungeonState::digBuried() {
    if (!chartFound_ || dungeon_.buriedRoom != currentRoom_) {
        return;
    }
    dungeon_.buriedRoom = -1;
    chartFound_ = false;
    context_.audio.play(Sfx::Chest);
    Party& p = context_.party;
    const std::string curioId = pickCurio(p.ownedCurios, dungeon_.themeId, dungeon_.seed);
    if (curioId.empty()) {
        // The dozen is complete: buried treasures pay a legendary token now.
        p.legendaryTokens += 1;
        showOutcome("The Buried Treasure",
                    "Buried riches! +1 legendary token (your curio collection is complete).");
    } else {
        p.ownedCurios.push_back(curioId);
        const CurioDef* curio = findCurio(curioId);
        showOutcome("The Buried Treasure",
                    TextFormat("Buried treasure: %s! (curios: %d of %d - see Maps in town)",
                               curio != nullptr ? curio->name : curioId.c_str(),
                               static_cast<int>(p.ownedCurios.size()), kCurioCount));
        // Curator may fire the moment the dozen completes.
        pushAchievementToasts(stack(), context_, AchvContext{});
    }
    buildRoom();
}

void DungeonState::takeMapPiece() {
    if (dungeon_.mapPieceRoom != currentRoom_) {
        return;
    }
    dungeon_.mapPieceRoom = -1;
    context_.audio.play(Sfx::Chest);
    Party& p = context_.party;
    // The FOURTH piece completes the puzzle: the treasure lies in THIS run's
    // town, guarded at THIS dungeon's own boss scale (owner rule: "the same
    // level as the town + depth the final piece was found in"). M83: the
    // grant rule is shared with the 4-floor completion drop (grantMapPiece),
    // so the two paths cannot drift apart.
    int bossScale = 100;
    for (const dungeon::EnemyTeam& t : dungeon_.teams) {
        if (t.isBoss) {
            bossScale = t.statScalePct;
            break;
        }
    }
    if (grantMapPiece(p.mapPieces, p.treasure, dungeon_.town, context_.content, dungeon_.seed,
                      bossScale)) {
        message_ = TextFormat(
            "The final map piece! The treasure lies buried in Town %d - and something guards it.",
            p.treasure.town);
        messageTimer_ = scaledMessageTime(context_, 4.0f);
    } else {
        message_ = TextFormat("A Secret Map Piece! (%d of %d - see Maps in a town's pause menu)",
                              p.mapPieces, kMapPiecesNeeded);
        messageTimer_ = scaledMessageTime(context_, 3.5f);
    }
    buildRoom();  // the piece marker clears
}

// M80 addendum: defined below resolveEvent, used inside it.
static std::string outcomeTitleFor(const AppContext& context, dungeon::RoomEventKind kind);

// Applies a non-battle event exactly as its footer prompt stated it.
void DungeonState::resolveEvent() {
    dungeon::Room& room = dungeon_.rooms[static_cast<std::size_t>(currentRoom_)];
    dungeon::RoomEvent& ev = room.event;
    if (ev.resolved) {
        return;
    }
    switch (ev.kind) {
        case dungeon::RoomEventKind::Shrine: {
            if (context_.party.gold < ev.goldCost) {
                context_.audio.play(Sfx::Error);
                showOutcome(outcomeTitleFor(context_, ev.kind),
                            "The shrine asks " + std::to_string(ev.goldCost) +
                                "g - you cannot pay.");
                return;
            }
            context_.party.gold -= ev.goldCost;
            for (Character& c : context_.party.members) {
                c.hp = std::min(c.maxHp, c.hp + (c.maxHp - c.hp) / 2);
            }
            context_.audio.play(Sfx::Heal);
            showOutcome(outcomeTitleFor(context_, ev.kind),
                        "The shrine accepts your offering - the party's wounds half-mend.");
            break;
        }
        case dungeon::RoomEventKind::HealingSpring:
            for (Character& c : context_.party.members) {
                if (c.hp > 0) {
                    c.hp = std::min(c.maxHp, c.hp + c.maxHp * 40 / 100);
                }
            }
            context_.audio.play(Sfx::Heal);
            showOutcome(outcomeTitleFor(context_, ev.kind),
                        "The spring's water restores the party. It runs dry.");
            break;
        case dungeon::RoomEventKind::Merchant: {
            const content::ItemDef* it = context_.content.findItem(ev.itemId);
            // M78: a premium tonic costs full value (the generated street
            // price is untouched — interaction-time pricing, the peddler
            // precedent), and a full bag refuses the sale WITHOUT spending
            // the merchant, so the party may use an item and come back.
            const int price = it != nullptr ? merchantPriceFor(*it, ev.goldCost) : ev.goldCost;
            const int capBonus = guildCapBonus(context_.party.guild);  // M84 perk
            if (it != nullptr && !canBuyMore(context_.party.inventory, *it, capBonus)) {
                context_.audio.play(Sfx::Error);
                showOutcome(outcomeTitleFor(context_, ev.kind),
                            "You cannot carry more of " + it->name + " (max " +
                                std::to_string(capFor(*it, capBonus)) + "). The merchant waits.");
                return;
            }
            if (context_.party.gold < price) {
                context_.audio.play(Sfx::Error);
                showOutcome(outcomeTitleFor(context_, ev.kind),
                            "The merchant wants " + std::to_string(price) +
                                "g - you cannot pay.");
                return;
            }
            context_.party.gold -= price;
            context_.party.inventory.add(ev.itemId, 1);
            context_.audio.play(Sfx::Interact);
            showOutcome(outcomeTitleFor(context_, ev.kind),
                        "Bought " + (it != nullptr ? it->name : ev.itemId) +
                            ". The merchant moves on.");
            break;
        }
        case dungeon::RoomEventKind::ScoreWager:
            run_.wagerAccepted = true;
            context_.audio.play(Sfx::Interact);
            showOutcome(outcomeTitleFor(context_, ev.kind),
                        "The omen accepts your dare. Finish without a death!");
            break;
        case dungeon::RoomEventKind::RestToken:
            context_.party.restTokens += 1;
            context_.audio.play(Sfx::Interact);
            showOutcome(outcomeTitleFor(context_, ev.kind),
                        "You pocket a free-rest token - redeem it at the inn.");
            break;
        case dungeon::RoomEventKind::RoyalRelic: {
            // M44: which relic is granted is decided HERE, at resolution, from a
            // pure hash of (dungeon seed, room index) and what the party already
            // owns - so reloading and walking back in reproduces the same relic
            // rather than rerolling it.
            std::array<bool, kRelicCount> owned{};
            for (int i = 0; i < kRelicCount; ++i) {
                owned[static_cast<std::size_t>(i)] =
                    context_.party.inventory.count(relicIdAt(i)) >= 1;
            }
            const std::string relicId = relicIdAt(relicPickIndex(dungeon_.seed, currentRoom_, owned));
            context_.party.inventory.add(relicId, 1);
            const content::ItemDef* it = context_.content.findItem(relicId);
            context_.audio.play(Sfx::Interact);
            showOutcome(outcomeTitleFor(context_, ev.kind),
                        "The reliquary yields " + (it != nullptr ? it->name : relicId) +
                            "! Save it for a foe that deserves it.");
            break;
        }
        case dungeon::RoomEventKind::ArmoryGhost: {
            // M55 (Ruined Keep): interactive. The player picks which inventory
            // piece to offer; the picker applies the seeded upgrade and marks the
            // event resolved, so we hand off and return (onResume rebuilds the
            // room when the picker closes a completed trade).
            stack().pushState(std::make_unique<ArmoryGhostState>(
                stack(), context_, dungeon_.seed, currentRoom_, &ev));
            return;
        }
        case dungeon::RoomEventKind::MinersCache: {
            // M55 (Crystal Mine): a one-third max-HP wound to each standing member
            // (never fatal), for gold above a trapped chest plus a guaranteed item
            // (baked in at generation).
            for (Character& c : context_.party.members) {
                if (c.hp > 0) {
                    c.hp = std::max(1, c.hp - dungeon::minersCacheWound(c.maxHp));
                }
            }
            const int gold = dungeon::minersCacheGold(dungeon_.depth);
            context_.party.gold += gold;
            run_.treasureGold += gold;
            std::string reward = TextFormat("%d gold", gold);
            if (!ev.itemId.empty()) {
                context_.party.inventory.add(ev.itemId, 1);
                const content::ItemDef* it = context_.content.findItem(ev.itemId);
                reward += std::string(" + ") + (it != nullptr ? it->name : ev.itemId);
            }
            context_.audio.play(Sfx::Chest);
            showOutcome(outcomeTitleFor(context_, ev.kind),
                        "You clear the rockfall - battered, but richer: " + reward + ".");
            break;
        }
        case dungeon::RoomEventKind::ElderRoot: {
            // M55 (Hollow Forest): pay town-scaled gold for party XP (the inverse
            // of fighting for XP - no battle turns spent).
            if (context_.party.gold < ev.goldCost) {
                context_.audio.play(Sfx::Error);
                showOutcome(outcomeTitleFor(context_, ev.kind),
                            "The Elder Root asks " + std::to_string(ev.goldCost) +
                                "g for its wisdom - you cannot pay.");
                return;
            }
            context_.party.gold -= ev.goldCost;
            const int xp = dungeon::elderRootXp(dungeon_.town, dungeon_.depth);
            grantPartyXp(context_.party, xp, context_.content);
            context_.audio.play(Sfx::Interact);
            showOutcome(
                outcomeTitleFor(context_, ev.kind),
                TextFormat("The Elder Root drinks your offering - the party gains %d XP.", xp));
            maybePushMilestoneChoice(stack(), context_);  // M63: the level-up moment
            break;
        }
        case dungeon::RoomEventKind::DuckPeddler: {
            // M76: one per customer (the owner's rule) — while the party owns a
            // duckling the peddler will not deal, and the offer stays
            // unresolved for a duckless return. Checked here, at interaction,
            // so what a seed GENERATES never depends on the party's bag.
            if (context_.party.inventory.count(dungeon::kEvilDucklingItemId) >= 1) {
                context_.audio.play(Sfx::Error);
                showOutcome(outcomeTitleFor(context_, ev.kind),
                            "\"One per customer. Duck rules.\" The peddler will not budge.");
                return;
            }
            if (context_.party.gold < ev.goldCost) {
                context_.audio.play(Sfx::Error);
                showOutcome(outcomeTitleFor(context_, ev.kind),
                            "The peddler wants " + std::to_string(ev.goldCost) +
                                "g - you cannot pay.");
                return;
            }
            context_.party.gold -= ev.goldCost;
            context_.party.inventory.add(dungeon::kEvilDucklingItemId, 1);
            const content::ItemDef* it = context_.content.findItem(ev.itemId);
            context_.audio.play(Sfx::Interact);
            showOutcome(outcomeTitleFor(context_, ev.kind),
                        "The peddler hands over the " +
                            (it != nullptr ? it->name : std::string("Evil Duckling")) +
                            ". It looks... pleased.");
            break;
        }
        case dungeon::RoomEventKind::Surveyor: {
            // M93: 20 gold charts the CURRENT floor — the fog lifts, the map
            // completes. A floor somehow already charted is a free courtesy.
            if (floorRevealed_) {
                context_.audio.play(Sfx::Confirm);
                showOutcome(outcomeTitleFor(context_, ev.kind),
                            "The surveyor squints at your map. \"Nothing left to "
                            "chart here.\" No charge.");
                break;
            }
            if (context_.party.gold < ev.goldCost) {
                context_.audio.play(Sfx::Error);
                showOutcome(outcomeTitleFor(context_, ev.kind),
                            "The survey costs " + std::to_string(ev.goldCost) +
                                "g - you cannot pay.");
                return;
            }
            context_.party.gold -= ev.goldCost;
            floorRevealed_ = true;
            context_.audio.play(Sfx::Interact);
            showOutcome(outcomeTitleFor(context_, ev.kind),
                        "Ink scratches, corners unfold - the floor's map is yours. "
                        "The fog on the minimap lifts.");
            break;
        }
        case dungeon::RoomEventKind::Dragonform: {
            // M93 (owner decision 6): the next battle this run is fought as
            // Dragons, for a flat -100 score stated on the panel. Arming is
            // run-scoped and spends on the next battle start.
            dragonformArmed_ = true;
            context_.audio.play(Sfx::Status);
            showOutcome(outcomeTitleFor(context_, ev.kind),
                        "Scales itch under your skin. The NEXT battle is fought in "
                        "dragonform - and the reckoning docks 100 score.");
            break;
        }
        case dungeon::RoomEventKind::EliteChallenge:
        case dungeon::RoomEventKind::None:
            return;  // challenges resolve through battle, not here
    }
    ev.resolved = true;
    buildRoom();
    // M80 addendum: every branch above raised the outcome panel; the old
    // footer-message timer has nothing left to time.
}

// M80 addendum (owner, 2026-08-06): event and chest OUTCOMES ride the same
// centered treatment as the flavor text — a modal the player dismisses —
// instead of the transient footer line. The footer line remains for the
// incidental notices (map-piece pings, battle results, the guarded nudge).
void DungeonState::showOutcome(const std::string& title, std::string body) {
    outcomeTitle_ = title;
    outcomeBody_ = std::move(body);
    outcomePanelOpen_ = true;
    outcomeView_.setContent(outcomeBody_, kPanelTextW, ui::style::kFontBody,
                            ui::raylibMeasure());
    outcomeView_.setVisibleLines(
        std::clamp(outcomeView_.lineCount(), 1, kOutcomeBodyLines));
    outcomeView_.scrollToTop();
    message_.clear();
    messageTimer_ = 0.0f;
}

// The outcome's heading: the event's authored flavor title when it exists,
// so the panel that promised the trade-off also announces its result.
static std::string outcomeTitleFor(const AppContext& context, dungeon::RoomEventKind kind) {
    if (const content::EventFlavorDef* flavor =
            context.content.findEventFlavor(dungeon::eventFlavorId(kind))) {
        return flavor->title;
    }
    return "The Event";
}

void DungeonState::renderOutcomePanel() const {
    const int w = context_.virtualWidth;
    const int h = context_.virtualHeight;
    const ui::style::Palette& pal = ui::style::palette();
    constexpr int kBoxH = 86;
    const int boxX = (w - kPanelBoxW) / 2;
    const int boxY = (h - kBoxH) / 2;
    ui::drawModalDim(w, h);
    ui::drawFrame(boxX, boxY, kPanelBoxW, kBoxH, ui::FrameStyle::Crystal);
    ui::drawTextCentered(outcomeTitle_.c_str(), w / 2, boxY + 8, ui::style::kFontMenu,
                         pal.crystal);
    // M87: the body scrolls past the visible budget instead of truncating.
    ui::drawTextViewport(outcomeView_, boxX + 14, boxY + 26, pal.text);
    const InputMap& map = context_.input.map();
    const ActiveDevice device = context_.input.activeDevice();
    std::string hint = input::prompt(map, InputAction::Confirm, device, "Continue");
    if (outcomeView_.scrollable()) {
        hint = input::primaryLabel(map, InputAction::MoveUp, device) + "/" +
               input::primaryLabel(map, InputAction::MoveDown, device) + " Scroll   " + hint;
    }
    ui::drawTextCentered(hint.c_str(), w / 2, boxY + kBoxH - 13, ui::style::kFontSmall,
                         pal.textHint);
}

// M80: the panel's Confirm — the same dispatch interact() used to do
// directly (an elite challenge fights, everything else resolves).
void DungeonState::confirmEventPanel() {
    if (facingMarker_ == nullptr || facingMarker_->kind != MarkerKind::Event) {
        return;
    }
    if (facingMarker_->teamIndex >= 0) {
        startBattle(facingMarker_->teamIndex, EncounterKind::Challenge,
                    facingMarker_->gateDir);
    } else {
        resolveEvent();
    }
}

// M80: the centered flavor panel — title, dry-humor body, then the SAME
// trade-off line the footer used to carry (cost/risk stays visible before
// commitment, the M20 bar), with the step-away binding at the bottom.
// M87: the body is a bounded scrollable viewport (filled at open time); the
// title, trade-off line, and controls are FIXED — however long an authored
// or translated flavor grows, the cost stays on screen before Confirm.
void DungeonState::openEventPanel(const content::EventFlavorDef& flavor) {
    eventFlavorView_.setContent(flavor.body, kPanelTextW, ui::style::kFontBody,
                                ui::raylibMeasure());
    eventFlavorView_.setVisibleLines(
        std::clamp(eventFlavorView_.lineCount(), 1, kEventBodyLines));
    eventFlavorView_.scrollToTop();
    eventPanelOpen_ = true;
}

void DungeonState::renderEventPanel() const {
    const int w = context_.virtualWidth;
    const int h = context_.virtualHeight;
    const ui::style::Palette& pal = ui::style::palette();
    const dungeon::RoomEvent& ev =
        dungeon_.rooms[static_cast<std::size_t>(currentRoom_)].event;
    const content::EventFlavorDef* flavor =
        context_.content.findEventFlavor(dungeon::eventFlavorId(ev.kind));
    if (flavor == nullptr) {
        return;  // defensive: the panel only opens when flavor exists
    }
    constexpr int kBoxH = 118;
    const int boxX = (w - kPanelBoxW) / 2;
    const int boxY = (h - kBoxH) / 2;
    ui::drawModalDim(w, h);
    ui::drawFrame(boxX, boxY, kPanelBoxW, kBoxH, ui::FrameStyle::Crystal);
    ui::drawTextCentered(flavor->title.c_str(), w / 2, boxY + 8, ui::style::kFontMenu,
                         pal.crystal);
    ui::drawTextViewport(eventFlavorView_, boxX + 14, boxY + 26, pal.text);
    ui::drawTextWrapped(eventPromptText(), boxX + 14, boxY + 78, kPanelBoxW - 28,
                        ui::style::kFontBody, pal.gold, "dungeon.eventtrade", 2);
    const InputMap& map = context_.input.map();
    const ActiveDevice device = context_.input.activeDevice();
    std::string hint = input::prompt(map, InputAction::Cancel, device, "Step away");
    if (eventFlavorView_.scrollable()) {
        hint = input::primaryLabel(map, InputAction::MoveUp, device) + "/" +
               input::primaryLabel(map, InputAction::MoveDown, device) + " Scroll   " + hint;
    }
    ui::drawTextCentered(hint.c_str(), w / 2, boxY + kBoxH - 13, ui::style::kFontSmall,
                         pal.textHint);
}

// The visible trade-off, shown in the footer BEFORE the player confirms.
std::string DungeonState::eventPromptText() const {
    const dungeon::Room& room = dungeon_.rooms[static_cast<std::size_t>(currentRoom_)];
    const dungeon::RoomEvent& ev = room.event;
    const InputMap& map = context_.input.map();
    const ActiveDevice device = context_.input.activeDevice();
    switch (ev.kind) {
        case dungeon::RoomEventKind::Shrine:
            if (context_.party.gold < ev.goldCost) {
                return "The shrine asks " + std::to_string(ev.goldCost) +
                       "g for its blessing - you cannot pay.";
            }
            return input::prompt(map, InputAction::Confirm, device,
                                 "Offer " + std::to_string(ev.goldCost) + "g") +
                   " - the party heals half of all wounds";
        case dungeon::RoomEventKind::HealingSpring:
            return input::prompt(map, InputAction::Confirm, device, "Drink") +
                   " - party recovers 40% HP (single use)";
        case dungeon::RoomEventKind::Merchant: {
            const content::ItemDef* it = context_.content.findItem(ev.itemId);
            const std::string name = it != nullptr ? it->name : ev.itemId;
            // M78: the prompt quotes what the till will actually charge — a
            // premium tonic at full value, everything else at street price.
            const int price = it != nullptr ? merchantPriceFor(*it, ev.goldCost) : ev.goldCost;
            const bool premium = it != nullptr && it->notSoldInTown;
            const int capBonus = guildCapBonus(context_.party.guild);  // M84 perk
            if (it != nullptr && !canBuyMore(context_.party.inventory, *it, capBonus)) {
                return "Merchant sells " + name + " - you cannot carry more (max " +
                       std::to_string(capFor(*it, capBonus)) + ").";
            }
            if (context_.party.gold < price) {
                return "Merchant sells " + name + " for " + std::to_string(price) +
                       "g - you cannot pay.";
            }
            return input::prompt(map, InputAction::Confirm, device, "Buy " + name) + " for " +
                   std::to_string(price) + (premium ? "g (full price)" : "g (dungeon prices)");
        }
        case dungeon::RoomEventKind::ScoreWager:
            return input::prompt(map, InputAction::Confirm, device, "Accept the omen's wager") +
                   ": +150 score if no ally falls, -100 if one does";
        case dungeon::RoomEventKind::RestToken:
            return input::prompt(map, InputAction::Confirm, device, "Rest here") +
                   " - pocket a free-rest token for the inn";
        case dungeon::RoomEventKind::RoyalRelic:
            // M44: no cost and no catch - the trade-off is that it is used up in
            // one battle, which the prompt says before the player commits (M20).
            return input::prompt(map, InputAction::Confirm, device, "Open the reliquary") +
                   " - claim one Royal Relic (a single-use battle trick)";
        case dungeon::RoomEventKind::EliteChallenge: {
            if (room.teamIndex < 0 ||
                room.teamIndex >= static_cast<int>(dungeon_.teams.size())) {
                return "";
            }
            const dungeon::EnemyTeam& team =
                dungeon_.teams[static_cast<std::size_t>(room.teamIndex)];
            const danger::Tier tier = teamTier_[static_cast<std::size_t>(room.teamIndex)];
            return input::prompt(map, InputAction::Confirm, device, "Challenge ") + team.name +
                   "  -  " + danger::tierName(tier) + "  x" + std::to_string(team.count()) +
                   "  -  double danger score, no treasure";
        }
        case dungeon::RoomEventKind::ArmoryGhost:
            // M55: no cost, but you give up a piece of gear for a random one a
            // tier finer, sight unseen.
            return input::prompt(map, InputAction::Confirm, device, "Armory Ghost") +
                   " - trade gear for one a rarity finer, unseen";
        case dungeon::RoomEventKind::MinersCache:
            return input::prompt(map, InputAction::Confirm, device, "Miner's Cache") +
                   " - a third of each hero's HP for gold + an item";
        case dungeon::RoomEventKind::ElderRoot:
            if (context_.party.gold < ev.goldCost) {
                return "The Elder Root asks " + std::to_string(ev.goldCost) +
                       "g for its wisdom - you cannot pay.";
            }
            return input::prompt(map, InputAction::Confirm, device,
                                 "Feed the Elder Root " + std::to_string(ev.goldCost) + "g") +
                   " - the whole party gains XP (no fight)";
        case dungeon::RoomEventKind::DuckPeddler:
            // M76: the decline is stated up front — the trade-off bar (M20)
            // covers refusals too.
            if (context_.party.inventory.count(dungeon::kEvilDucklingItemId) >= 1) {
                return "The peddler eyes your pack. \"One per customer. Duck rules.\"";
            }
            if (context_.party.gold < ev.goldCost) {
                return "The peddler sells an Evil Duckling for " +
                       std::to_string(ev.goldCost) + "g - you cannot pay.";
            }
            return input::prompt(map, InputAction::Confirm, device, "Buy the Evil Duckling") +
                   " for " + std::to_string(ev.goldCost) +
                   "g - a single-use curse for one foe";
        case dungeon::RoomEventKind::Surveyor:  // M93
            if (floorRevealed_) {
                return "The surveyor has nothing left to chart on this floor.";
            }
            if (context_.party.gold < ev.goldCost) {
                return "The survey costs " + std::to_string(ev.goldCost) +
                       "g - you cannot pay.";
            }
            return input::prompt(map, InputAction::Confirm, device,
                                 "Buy the survey for " + std::to_string(ev.goldCost) + "g") +
                   " - reveals this floor's whole map";
        case dungeon::RoomEventKind::Dragonform:  // M93 (owner: flat -100 score)
            if (dragonformArmed_) {
                return "The scales already itch - the next battle is spoken for.";
            }
            return input::prompt(map, InputAction::Confirm, device, "Accept dragonform") +
                   " - fight the NEXT battle as Dragons, -100 score";
        case dungeon::RoomEventKind::None:
            break;
    }
    return "";
}

void DungeonState::startBattle(int teamIndex, EncounterKind kind, dungeon::Dir gateDir) {
    if (teamIndex < 0 || teamIndex >= static_cast<int>(dungeon_.teams.size())) {
        return;
    }
    pendingKind_ = kind;
    pendingRoom_ = currentRoom_;
    pendingTeamIndex_ = teamIndex;
    pendingGateDir_ = gateDir;
    battleResult_ = battle::BattleResult{};
    const dungeon::EnemyTeam& team = dungeon_.teams[static_cast<std::size_t>(teamIndex)];
    // M68: the battle itself pays the team's spoils on Victory and shows the
    // results panel; onResume no longer grants (it would double-pay).
    pendingSpoils_ = teamSpoils(team, context_.content);
    // M93: an armed dragonform spends itself on THIS battle — the members are
    // swapped for Dragons (HP/MP by percentage, no gear, the M45 kit) before
    // buildBattle reads them, and onResume restores the stash afterwards. The
    // battle itself needs no special cases, so the Simulator agrees.
    if (dragonformArmed_) {
        dragonformArmed_ = false;
        dragonformStash_ = enterDragonform(context_.party, context_.content);
    }
    battle::Battle b = battle::buildBattle(context_.party, team, context_.content);
    // M56: every battle wears the theme backdrop; a boss-team fight (bossId set)
    // opens with the Crystal Shatter intro, which then launches the same battle.
    const render::BackdropStage stage = render::stageForTheme(dungeon_.themeId);
    if (!team.bossId.empty()) {
        stack().pushState(std::make_unique<BossIntroState>(
            stack(), context_, std::move(b), &battleResult_, MusicTrack::None, &victoryStats_,
            /*castleChallenge=*/false, stage, dungeon_.seed, &pendingSpoils_));
    } else {
        stack().pushState(std::make_unique<BattleState>(stack(), context_, std::move(b),
                                                        &battleResult_, MusicTrack::None,
                                                        &victoryStats_, /*castleChallenge=*/false,
                                                        stage, &pendingSpoils_));
    }
}

void DungeonState::onEnter() {
    // Runs after the Guild pop's TownState::onResume, so these win and the
    // dungeon actually gets its own theme music + ambience bed.
    context_.audio.setMusic(themeMusic(dungeon_.themeId));
    context_.audio.setAmbience(themeAmbience(dungeon_.themeId));
    maybeTutorialPrompt(stack(), context_, tutorial::kDungeonFirst);
}

void DungeonState::onResume() {
    // M67: the run ended behind the result screen. The result pops only itself;
    // any state wedged between (the M63 milestone modal a boss kill pushes) gets
    // its turn on top, and once control falls back here the dungeon removes
    // itself — the player always lands in town.
    if (runComplete_) {
        stack().popState();
        return;
    }
    // M55: a pushed sub-state (the Armory Ghost picker) may have resolved the
    // event the player is standing at; rebuild so its marker clears. Only touches
    // a resolved event room, so it is harmless after a battle resume.
    {
        const dungeon::Room& here = dungeon_.rooms[static_cast<std::size_t>(currentRoom_)];
        if (here.type == dungeon::RoomType::Event && here.event.resolved) {
            buildRoom();
        }
    }
    if (pendingKind_ == EncounterKind::None) {
        return;
    }
    // M56 belt-and-braces: a boss fight now runs behind BossIntroState, so this
    // resume only fires once the battle has truly ended. If a result is somehow
    // still Ongoing (a spurious resume), leave the pending state intact and wait.
    if (battleResult_.outcome == battle::Outcome::Ongoing) {
        return;
    }
    const EncounterKind kind = pendingKind_;
    pendingKind_ = EncounterKind::None;
    const battle::Outcome outcome = battleResult_.outcome;

    // M93: a dragonform battle ended — restore the real party FIRST, with the
    // fight's outcome carried back by percentage (KO stays KO), so every path
    // below (carry-out, spoils already paid, completion) sees real members.
    if (!dragonformStash_.original.empty()) {
        leaveDragonform(context_.party, dragonformStash_);
        dragonformStash_.original.clear();
        ++dragonformFights_;
    }

    // Returning from a battle: fade in and restore the dungeon music *and*
    // ambience (town states override them again if we end up leaving).
    context_.fade.start();
    context_.audio.setMusic(themeMusic(dungeon_.themeId));
    context_.audio.setAmbience(themeAmbience(dungeon_.themeId));

    // Accumulate run statistics regardless of outcome.
    run_.battleTurns += battleResult_.rounds;
    if (battleResult_.partyKoOccurred) {
        run_.noDeath = false;
    }

    if (outcome == battle::Outcome::Escaped) {
        ++run_.escapes;
        // M93: fleeing a patrol still resets the counter — the fight happened
        // (and the escape penalty stands like any other).
        if (kind == EncounterKind::Patrol) {
            dangerSteps_ = kDangerStepsPerPatrol;
            ++patrolIndex_;
        }
        return;  // gate intact; resume in the dungeon
    }
    if (outcome == battle::Outcome::Defeat) {
        // M89: the castle carry-out (M47) reaches the dungeons — one member
        // staggers back at 1 HP, the fallen stay fallen, MP is untouched. The
        // half-gold price stays (owner decision: only the free full heal
        // goes). TownState teaches the new rule once, on arrival with fallen
        // members (tutorial::kCarriedOut).
        clampCastleDefeat(context_.party);
        context_.party.gold /= 2;
        stack().popState();  // game over -> back to town
        return;
    }
    if (outcome != battle::Outcome::Victory) {
        return;
    }

    if (kind != EncounterKind::Boss) {
        maybeTutorialPrompt(stack(), context_, tutorial::kVictoryFirst);
    }

    // M93: a beaten patrol resolves nothing in the dungeon — no danger
    // credit, no gate or chest (owner decision 7: XP only, which the spoils
    // already paid, gold-free). The counter simply rewinds.
    if (kind == EncounterKind::Patrol) {
        dangerSteps_ = kDangerStepsPerPatrol;
        ++patrolIndex_;
        message_ = "The patrol scatters. The dungeon quiets - for now.";
        messageTimer_ = scaledMessageTime(context_, 2.0f);
        return;
    }

    // Credit the danger defeated.
    if (pendingTeamIndex_ >= 0 && pendingTeamIndex_ < static_cast<int>(teamTier_.size())) {
        run_.dangerDefeated += danger::tierWeight(teamTier_[static_cast<std::size_t>(pendingTeamIndex_)]);
    }

    // M68: the battle already paid the team's XP and gold at its Done beat and
    // showed the results panel (game/Spoils.hpp — the one shared rule), so no
    // award happens here anymore. Only the level-up moment remains:
    // M63 choice prompts fire after the battle popped. On a boss kill the
    // modal lands under the result screen and surfaces right after it closes
    // (M67 unwind order).
    maybePushMilestoneChoice(stack(), context_);

    dungeon::Room& room = dungeon_.rooms[static_cast<std::size_t>(pendingRoom_)];
    if (kind == EncounterKind::Gate) {
        room.door(pendingGateDir_).gated = false;
        const int neighbor = room.door(pendingGateDir_).neighbor;
        if (neighbor >= 0) {
            dungeon_.rooms[static_cast<std::size_t>(neighbor)]
                .door(dungeon::opposite(pendingGateDir_))
                .gated = false;
        }
        buildRoom();
        message_ = "The gate is clear!";
        messageTimer_ = scaledMessageTime(context_, 2.5f);
    } else if (kind == EncounterKind::Guard) {
        room.teamIndex = -1;
        room.chest.guarded = false;
        buildRoom();
        message_ = "The guards fall.";
        messageTimer_ = scaledMessageTime(context_, 2.5f);
    } else if (kind == EncounterKind::Challenge) {
        // The challenge pays double danger: the base credit was added above,
        // so add the same weight once more.
        if (pendingTeamIndex_ >= 0 && pendingTeamIndex_ < static_cast<int>(teamTier_.size())) {
            run_.dangerDefeated +=
                danger::tierWeight(teamTier_[static_cast<std::size_t>(pendingTeamIndex_)]);
        }
        context_.party.legendaryTokens += 1;  // M34: elite fights fund the black market
        room.teamIndex = -1;
        room.event.resolved = true;
        buildRoom();
        message_ = "Challenge won - double danger, +1 legendary token.";
        messageTimer_ = scaledMessageTime(context_, 2.5f);
    } else if (kind == EncounterKind::StairGate) {
        // M82: the floor's climax falls and the stairway down stands revealed
        // where the wardens stood. The run continues — nothing scores yet.
        room.teamIndex = -1;
        dungeon_.stairsOpen = true;
        buildRoom();
        message_ = "The Stairway Wardens fall - the way down stands open.";
        messageTimer_ = scaledMessageTime(context_, 2.5f);
    } else if (kind == EncounterKind::Boss) {
        completeDungeon();
    }
}

void DungeonState::completeDungeon() {
    runComplete_ = true;  // M67: the next resume of this state pops it (see onResume)
    score::RunSummary summary;
    summary.completed = true;
    summary.battleTurns = run_.battleTurns;
    summary.dangerDefeated = run_.dangerDefeated;
    summary.chestsOpened = run_.chestsOpened;
    summary.treasureGold = run_.treasureGold;
    summary.noDeath = run_.noDeath;
    summary.escapes = run_.escapes;
    summary.wagerAccepted = run_.wagerAccepted;
    summary.townBonusPct = townScoreBonusPct(dungeon_.town);  // M32 town ladder
    // M33: the stakes penalty this run incurs is a function of the PRE-run stakes
    // state (unchanged since the Guild forewarned it), so compute it before the
    // state advances below.
    const int stakesPct =
        stakesPenaltyPct(context_.party.stakes, dungeon_.town, dungeon_.depth);
    summary.stakesPenaltyPct = stakesPct;
    // M45: the party's additive unlockable-class modifier (0 for any party of the
    // six original classes). Derived from the classes, never hand-set.
    summary.classModPct = partyClassModPct(context_.party, context_.content);
    // M93: dragonform pacts fought this run (a flat -100 each, owner decision 6).
    summary.dragonformFights = dragonformFights_;
    // M34: whether this run raises the stakes (the black-market spawn trigger),
    // read from the PRE-run state before it advances below.
    const bool raisedStakes =
        stakesRaised(context_.party.stakes, dungeon_.town, dungeon_.depth);

    const int total = score::computeScore(summary);

    // M32: completing a run unlocks the next town (persisted in the live party;
    // saved on the next save/autosave, like the run's gold and XP).
    context_.party.highestUnlockedTown =
        unlockAfterClear(context_.party.highestUnlockedTown, dungeon_.town);
    // M40: clearing any town-7 dungeon opens the road up to the castle.
    if (dungeon_.town >= kTownCount) {
        context_.party.castleUnlocked = true;
    }
    // M84: a 4-FLOOR clear earns this town's Guild Master audience (persisted
    // like the other unlocks; the guild screen's boss row goes live).
    if (dungeon_.floorCount >= 4) {
        guildRecord(context_.party.guild, dungeon_.town).unlocked = true;
    }
    // M33: advance the stakes baseline/penalty on a scoring completion. A
    // completed-but-zero run (extreme turn penalty) is treated like a score-0
    // run: it does not move the baseline (owner rule).
    if (total > 0) {
        context_.party.stakes =
            afterCompletedRun(context_.party.stakes, dungeon_.town, dungeon_.depth);
    }

    // M34/M52: the black market may spawn from a stakes-raising scoring run in
    // town >= 2 (the 20% path) OR from any beaten boss at town 7 depth >= 20 (the
    // independent 34% path). completeDungeon is only reached on a real completion,
    // so `completed` is true here; the score/stakes still gate the 20% path.
    // Seeded from this run, so reloading the entry autosave cannot reroll it; the
    // offer persists in the party save until purchased, and a later hit replaces
    // it.
    // M82: every run-level seeded system keys off the RUN seed (identical to
    // dungeon_.seed on 1-floor runs, so v14 behavior is untouched; on a
    // 4-floor run the final floor's sub-seed is not the identity the player
    // entered, the board records, or a reload reproduces).
    if (blackMarketShouldSpawn(total > 0, /*completed=*/true, raisedStakes, dungeon_.town,
                               dungeon_.runSeed, dungeon_.depth,
                               guildBlackMarketPct(context_.party.guild))) {  // M84 perk
        // Shared legendary pool (M39): the market and boss drops draw the same set.
        const std::vector<std::string> legendaryIds = legendaryDropPool(context_.content);
        if (!legendaryIds.empty()) {
            BlackMarketOffer offer;
            offer.present = true;
            offer.town = dungeon_.town;
            offer.itemId = legendaryIds[static_cast<std::size_t>(
                blackMarketItemIndex(dungeon_.runSeed, static_cast<int>(legendaryIds.size())))];
            offer.priceGold = blackMarketPriceGold(dungeon_.town);
            const MarketTile mt = kBlackMarketTiles[blackMarketTileIndex(dungeon_.runSeed)];
            offer.tileX = mt.x;
            offer.tileY = mt.y;
            context_.party.blackMarket = offer;
        }
    }

    // M39: seeded, reload-proof boss drops. On a boss kill in town >= 3 and
    // depth >= 4, roll legendary tokens and (independently) a legendary piece off
    // this run's dungeon seed, so replaying the same run reproduces the same drops
    // and no reload rerolls them. Applied to the live party (saved on the next
    // save/autosave, like the run's gold/XP); shown on the result screen.
    const BossDropResult drops =
        rollBossDrops(dungeon_.runSeed, dungeon_.town, dungeon_.depth, context_.content);
    if (drops.tokens > 0) {
        context_.party.legendaryTokens += drops.tokens;
    }
    if (drops.legendary) {
        context_.party.inventory.add(drops.legendaryId, 1);
    }

    // M83: a completed 4-FLOOR run in town >= 2 may pay a map piece. The roll
    // is a pure hash of the RUN seed (committed at entry — reloading the
    // entry autosave replays the same outcome). While a treasure already
    // stands revealed, the piece banks as an IOU (cap 3, overflow lost) and
    // pays out after the dig guardian falls. The line rides the result screen.
    std::string mapLine;
    if (dungeon_.floorCount >= 4 &&
        mapDropRolls(dungeon_.runSeed, dungeon_.town,
                     guildMapBonusPct(context_.party.guild))) {  // M84 perk (M83 hook)
        Party& p = context_.party;
        if (p.treasure.active) {
            if (bankMapDebt(p.mapPiecesOwed)) {
                mapLine = TextFormat(
                    "The guild owes you a map piece - dig up the treasure to collect (%d banked).",
                    p.mapPiecesOwed);
            } else {
                mapLine = "The guild would owe you a map piece, but the ledger is full.";
            }
        } else {
            int bossScale = 100;
            for (const dungeon::EnemyTeam& t : dungeon_.teams) {
                if (t.isBoss) {
                    bossScale = t.statScalePct;
                    break;
                }
            }
            if (grantMapPiece(p.mapPieces, p.treasure, dungeon_.town, context_.content,
                              dungeon_.runSeed, bossScale)) {
                mapLine = TextFormat(
                    "The final map piece! The treasure lies buried in Town %d.",
                    p.treasure.town);
            } else {
                mapLine = TextFormat("The descent pays a Secret Map Piece (%d of %d held).",
                                     p.mapPieces, kMapPiecesNeeded);
            }
        }
    }

    score::ScoreEntry entry;
    entry.score = total;
    entry.battleTurns = summary.battleTurns;
    entry.dangerDefeated = summary.dangerDefeated;
    entry.chestsOpened = summary.chestsOpened;
    entry.noDeath = summary.noDeath;
    entry.depth = dungeon_.depth;
    entry.theme = dungeon_.themeName;
    entry.seed = dungeon_.runSeed;  // M82: the re-enterable identity
    entry.floors = dungeon_.floorCount;  // M82: 1F and 4F rank on separate boards
    entry.generationVersion = dungeon::kGenerationVersion;
    entry.partyLevel = highestLevel(context_.party);
    entry.battleRulesVersion = battle::kBattleRulesVersion;
    entry.townIndex = dungeon_.town;  // M32
    entry.stakesPenaltyPct = stakesPct;      // M33
    entry.classModPct = summary.classModPct;  // M45 (comparability tag, never ranked)
    // M53: a run played with god mode on is not a fair score, so keep it off the
    // board. The guard is compiled in only for debug builds; in Release
    // submitScore is a constant true and the scoreboard write is unchanged.
    bool submitScore = true;
#ifdef CRYSTAL_DEBUG_OVERLAY
    submitScore = !context_.cheats.godMode;
#endif
    if (submitScore) {
        context_.scoreboard.add(entry);
        content::LoadReport saveReport;
        context_.scoreboard.save(saveReport);
    }

    // M42: fold this run's victory tallies into the party's personal records
    // (display-only, never ranked), then show the result with the run stats.
    if (victoryStats_.biggestHit > context_.party.recordBiggestHit) {
        context_.party.recordBiggestHit = victoryStats_.biggestHit;
    }
    if (victoryStats_.totalDamage > context_.party.recordRunDamage) {
        context_.party.recordRunDamage = victoryStats_.totalDamage;
    }
    // M92: a scoring, stakes-raising 20-floor clear earns the Guild's trove —
    // a pick-one skill scroll (game/ScrollTrove.hpp: pool, seeded offers, and
    // the trigger in one tested place). Pushed BEFORE the result so it
    // surfaces on the way back to town (the M67 unwind order), like the M63
    // milestone modal.
    if (scrollTroveEarned(dungeon_.floorCount, raisedStakes, total)) {
        const std::vector<std::string> offers =
            scrollTroveOffers(context_.party, context_.content, dungeon_.runSeed);
        if (!offers.empty()) {
            stack().pushState(std::make_unique<ScrollChoiceState>(stack(), context_, offers));
        }
    }
    stack().pushState(std::make_unique<DungeonResultState>(stack(), context_, summary, total, drops,
                                                           victoryStats_, mapLine));  // M83

    // M71: a CLEAN run earns the celebration, shown above the reckoning: the
    // score alone, the team jumping to their own beats, the MVP on the
    // pedestal, the fallen lying where they fell. Clean means (owner rules):
    // no stakes penalty, a positive score, and not a single escape.
    if (stakesPct == 0 && total > 0 && run_.escapes == 0) {
        stack().pushState(std::make_unique<CelebrationState>(
            stack(), context_, TextFormat("Score: %d", total), victoryStats_.mvpMember()));
    }

    // M42: unlock any achievements this run earned, and toast them (pushed above
    // the result, so they show first, then the reckoning).
    AchvContext actx;
    actx.clearedDungeon = total > 0;
    actx.runNoDeath = summary.noDeath;
    actx.runTurns = summary.battleTurns;
    actx.runDepth = dungeon_.depth;
    pushAchievementToasts(stack(), context_, actx);
}

void DungeonState::handleInput(const Input& input) {
    // M80 addendum: the outcome panel just wants to be read — anything
    // affirmative dismisses it. M87: Up/Down scrolls a long body first.
    if (outcomePanelOpen_) {
        moveX_ = 0.0f;
        moveY_ = 0.0f;
        if (input.navPressed(InputAction::MoveUp) && outcomeView_.scrollBy(-1)) {
            context_.audio.play(Sfx::Move);
        }
        if (input.navPressed(InputAction::MoveDown) && outcomeView_.scrollBy(1)) {
            context_.audio.play(Sfx::Move);
        }
        if (input.pressed(InputAction::Confirm) || input.pressed(InputAction::Cancel) ||
            input.pressed(InputAction::Menu)) {
            outcomePanelOpen_ = false;
        }
        return;
    }

    // M80: while the flavor panel is up it owns the input — Confirm accepts
    // the trade-off, Cancel (or Menu) steps away and the event keeps waiting.
    // M87: Up/Down scrolls a long flavor body; the trade-off line is fixed.
    if (eventPanelOpen_) {
        moveX_ = 0.0f;
        moveY_ = 0.0f;
        if (input.navPressed(InputAction::MoveUp) && eventFlavorView_.scrollBy(-1)) {
            context_.audio.play(Sfx::Move);
        }
        if (input.navPressed(InputAction::MoveDown) && eventFlavorView_.scrollBy(1)) {
            context_.audio.play(Sfx::Move);
        }
        if (input.pressed(InputAction::Confirm)) {
            eventPanelOpen_ = false;
            confirmEventPanel();
        } else if (input.pressed(InputAction::Cancel) || input.pressed(InputAction::Menu)) {
            eventPanelOpen_ = false;
            context_.audio.play(Sfx::Cancel);
        }
        return;
    }

    moveX_ = (input.down(InputAction::MoveRight) ? 1.0f : 0.0f) -
             (input.down(InputAction::MoveLeft) ? 1.0f : 0.0f);
    moveY_ = (input.down(InputAction::MoveDown) ? 1.0f : 0.0f) -
             (input.down(InputAction::MoveUp) ? 1.0f : 0.0f);

    if (input.pressed(InputAction::Confirm)) {
        interact();
    }
    if (input.pressed(InputAction::Details)) {
        openDetails();
    }
    if (input.pressed(InputAction::Menu) || input.pressed(InputAction::Cancel)) {
        stack().pushState(std::make_unique<DungeonMenuState>(stack(), context_));
    }
}

// M22: danger-tier reference plus whatever is currently faced. M88: a faced
// team now discloses its full roster — per member the scaled stats battle will
// actually build (content::scaledStats, the buildBattle multiply), affinities
// and passives (the same set the in-battle target panel shows while aiming) —
// so the player can size a fight and re-gear BEFORE engaging (owner item 5).
void DungeonState::openDetails() {
    std::string body;
    if (facingMarker_ != nullptr && facingMarker_->teamIndex >= 0 &&
        facingMarker_->teamIndex < static_cast<int>(dungeon_.teams.size())) {
        const dungeon::EnemyTeam& team =
            dungeon_.teams[static_cast<std::size_t>(facingMarker_->teamIndex)];
        const danger::Tier tier = teamTier_[static_cast<std::size_t>(facingMarker_->teamIndex)];
        body += dungeon::describeTeam(team, danger::tierName(tier), context_.content) + "\n\n";
    }
    body +=
        "Danger is computed from the actual enemies' stats, skills, and team "
        "synergy - never hand-picked. Tiers from weakest to strongest: "
        "Trivial, Easy, Fair, Dangerous, Deadly, Boss.\n\nGate teams must be "
        "fought to reach the boss. Guarded chests show their rarity before "
        "the fight; escaping the guard forfeits the chest. Events state "
        "their full trade-off in the footer before you Confirm. Each theme "
        "also hides one signature rite - the Keep's Armory Ghost, the Mine's "
        "Cache, the Forest's Elder Root. Deeper dungeons field bigger, "
        "stronger teams - and pay better.";
    stack().pushState(
        std::make_unique<DetailsOverlayState>(stack(), context_, "Dungeon Details", body));
}

void DungeonState::update(float dt) {
#ifdef CRYSTAL_DEBUG_OVERLAY
    // M53: the debug "Instant dungeon clear" request is set from the pause menu
    // and consumed here, once the menus have closed and the dungeon is ticking
    // again. It runs the real completeDungeon() path — genuine scoring, unlocks,
    // and market rolls (and, if god mode is on, the same honest scoreboard skip).
    if (context_.cheats.requestDungeonClear) {
        context_.cheats.requestDungeonClear = false;
        completeDungeon();
        return;
    }
    // M93 debug one-shots: burn the fuse / arm the form via the REAL paths.
    if (context_.cheats.requestPatrolNow) {
        context_.cheats.requestPatrolNow = false;
        dangerSteps_ = 1;
    }
    if (context_.cheats.requestArmDragonform) {
        context_.cheats.requestArmDragonform = false;
        dragonformArmed_ = true;
    }
#endif
    worldTime_ += dt;
    const float length = std::sqrt(moveX_ * moveX_ + moveY_ * moveY_);
    moving_ = length > 0.0001f;
    if (moving_) {
        const float nx = moveX_ / length;
        const float ny = moveY_ / length;
        facing_ = Vec2{nx, ny};
        const Vec2 moved =
            town::resolveMove(player_, nx * kSpeed * dt, ny * kSpeed * dt, roomMap_);
        player_.x = moved.x;
        player_.y = moved.y;
        walkTime_ += dt;
        context_.audio.play(Sfx::Step);  // cadence via the role's rate limit
    } else {
        walkTime_ = 0.0f;
    }

    const int tx = static_cast<int>((player_.x + player_.w * 0.5f) / kTile);
    const int ty = static_cast<int>((player_.y + player_.h * 0.5f) / kTile);

    for (const DoorTile& d : doorTiles_) {
        if (d.x == tx && d.y == ty) {
            enterRoom(d.neighbor, dungeon::opposite(d.dir));
            return;
        }
    }

    recomputeInteraction(tx, ty);

    // M93: the danger counter (owner decisions 5/7). A step is a tile: every
    // tile the party walks onto ticks the visible countdown, and at 0 the
    // roused patrol attacks IMMEDIATELY — composed by the dungeon's own
    // rules, seeded from (runSeed, patrolIndex), so the Nth patrol of a run
    // is deterministic and reload-honest. The counter resets after the
    // fight (see onResume) and keeps counting across floors.
    if (tx != lastTileX_ || ty != lastTileY_) {
        const bool counted = lastTileX_ >= 0;  // the spawn tile itself is free
        lastTileX_ = tx;
        lastTileY_ = ty;
        if (counted && --dangerSteps_ <= 0) {
            dungeon_.teams.push_back(dungeon::patrolTeam(context_.content, dungeon_.themeId,
                                                         dungeon_.town, dungeon_.depth,
                                                         dungeon_.runSeed, patrolIndex_));
            teamTier_.push_back(danger::assess(dungeon_.teams.back(), context_.content,
                                               danger::partyThreat(context_.party.members)));
            startBattle(static_cast<int>(dungeon_.teams.size()) - 1, EncounterKind::Patrol,
                        dungeon::Dir::North);
            return;
        }
    }

    // First-encounter beats fire when the relevant thing is faced (the
    // footer is already explaining it), never mid-walk.
    if (facingMarker_ != nullptr) {
        if (facingMarker_->kind == MarkerKind::GuardTeam) {
            maybeTutorialPrompt(stack(), context_, tutorial::kChestGuarded);
        } else if (facingMarker_->kind == MarkerKind::Event) {
            // M44: a reliquary is rare enough to deserve its own first-encounter
            // beat, and it explains what the relics are FOR before the King.
            const dungeon::RoomEvent& ev =
                dungeon_.rooms[static_cast<std::size_t>(currentRoom_)].event;
            maybeTutorialPrompt(stack(), context_,
                                ev.kind == dungeon::RoomEventKind::RoyalRelic
                                    ? tutorial::kFirstRelic
                                    : tutorial::kEventFirst);
        }
    }

    if (messageTimer_ > 0.0f) {
        messageTimer_ -= dt;
        if (messageTimer_ <= 0.0f) {
            message_.clear();
        }
    }
}

void DungeonState::renderMinimap() const {
    constexpr int cell = 6;
    constexpr int step = 7;
    const int ox = context_.virtualWidth - dungeon_.gridW * step - 10;
    const int oy = 10;

    // M93: fog of war on the multi-floor shapes (owner item; the classic
    // 1-floor map stays complete per the M82 "classic unchanged" rule).
    // Unvisited rooms are absent, not dimmed; a visited room's door to the
    // unknown shows as a half-length stub; the Surveyor's paid reveal (or a
    // 1-floor run) lifts the fog for the current floor. The M66 chart's X
    // still burns through — revealing the buried room is the chart's job.
    const bool fog = dungeon_.floorCount > 1 && !floorRevealed_;
    const auto roomShown = [&](int index) {
        if (!fog) {
            return true;
        }
        const dungeon::Room& room = dungeon_.rooms[static_cast<std::size_t>(index)];
        return room.visited || (chartFound_ && index == dungeon_.buriedRoom);
    };

    // Framed map well (M46) with the shared Inset construction.
    ui::drawFrame(ox - 6, oy - 6, dungeon_.gridW * step + 12, dungeon_.gridH * step + 12,
                  ui::FrameStyle::Inset);

    for (std::size_t ri = 0; ri < dungeon_.rooms.size(); ++ri) {
        const dungeon::Room& r = dungeon_.rooms[ri];
        if (!roomShown(static_cast<int>(ri))) {
            continue;
        }
        const int cx = ox + r.gridX * step;
        const int cy = oy + r.gridY * step;
        // Door links to neighbors with a higher room... draw from each room's center.
        for (dungeon::Dir dir : {dungeon::Dir::East, dungeon::Dir::South}) {
            const dungeon::Door& door = r.door(dir);
            if (door.neighbor < 0) {
                continue;
            }
            const Color line = door.gated ? Color{210, 90, 90, 255} : Color{120, 120, 140, 255};
            const bool full = roomShown(door.neighbor);
            const int len = full ? step : step / 2;  // M93: a stub hints the unknown
            DrawLine(cx + cell / 2, cy + cell / 2, cx + cell / 2 + dungeon::dirDx(dir) * len,
                     cy + cell / 2 + dungeon::dirDy(dir) * len, line);
        }
        // M93: west/north stubs out of a shown room toward a hidden one (the
        // full link is otherwise drawn by the neighbor, who is hidden).
        if (fog) {
            for (dungeon::Dir dir : {dungeon::Dir::West, dungeon::Dir::North}) {
                const dungeon::Door& door = r.door(dir);
                if (door.neighbor < 0 || roomShown(door.neighbor)) {
                    continue;
                }
                const Color line =
                    door.gated ? Color{210, 90, 90, 255} : Color{120, 120, 140, 255};
                DrawLine(cx + cell / 2, cy + cell / 2,
                         cx + cell / 2 + dungeon::dirDx(dir) * (step / 2),
                         cy + cell / 2 + dungeon::dirDy(dir) * (step / 2), line);
            }
        }
    }
    for (std::size_t i = 0; i < dungeon_.rooms.size(); ++i) {
        const dungeon::Room& r = dungeon_.rooms[i];
        if (!roomShown(static_cast<int>(i))) {
            continue;
        }
        const int cx = ox + r.gridX * step;
        const int cy = oy + r.gridY * step;
        Color c{120, 120, 130, 255};
        switch (r.type) {
            case dungeon::RoomType::Start: c = Color{110, 200, 120, 255}; break;
            case dungeon::RoomType::Boss: c = Color{220, 90, 90, 255}; break;
            case dungeon::RoomType::Treasure: c = Color{220, 190, 90, 255}; break;
            case dungeon::RoomType::Normal: c = Color{120, 120, 140, 255}; break;
        }
        if (!r.visited) {
            c.a = 110;
        }
        DrawRectangle(cx, cy, cell, cell, c);
        // M66: once the chart is read, the X it promised burns on the minimap.
        if (chartFound_ && static_cast<int>(i) == dungeon_.buriedRoom) {
            DrawRectangle(cx + 1, cy + 1, cell - 2, 1, Color{235, 214, 112, 255});
            DrawRectangle(cx + 1, cy + cell - 2, cell - 2, 1, Color{235, 214, 112, 255});
            DrawRectangle(cx + cell / 2, cy + 2, 1, cell - 4, Color{235, 214, 112, 255});
        }
        if (static_cast<int>(i) == currentRoom_) {
            DrawRectangleLines(cx - 1, cy - 1, cell + 2, cell + 2, RAYWHITE);
        }
    }
}

void DungeonState::render() {
    const ui::style::Palette& pal = ui::style::palette();
    ClearBackground(pal.canvas);

    // Stage matte (M46): a low-contrast band behind the room's span plus
    // stepped corner brackets and sparse theme accent pips connect the
    // playable room to the HUD without painting over it.
    const int roomW = roomMap_.width() * kTile;
    const int roomH = roomMap_.height() * kTile;
    DrawRectangle(0, originY_ - 8, context_.virtualWidth, roomH + 16, pal.panelInset);
    {
        const int bx = originX_ - 6;
        const int by = originY_ - 6;
        const int bw = roomW + 12;
        const int bh = roomH + 12;
        const Color bracket = pal.borderMid;
        DrawRectangle(bx, by, 10, 2, bracket);
        DrawRectangle(bx, by, 2, 10, bracket);
        DrawRectangle(bx + bw - 10, by, 10, 2, bracket);
        DrawRectangle(bx + bw - 2, by, 2, 10, bracket);
        DrawRectangle(bx, by + bh - 2, 10, 2, bracket);
        DrawRectangle(bx, by + bh - 10, 2, 10, bracket);
        DrawRectangle(bx + bw - 10, by + bh - 2, 10, 2, bracket);
        DrawRectangle(bx + bw - 2, by + bh - 10, 2, 10, bracket);
        // Theme accent pips beside the top brackets (shape language shared,
        // accent per theme: keep=bronze/stone, mine=crystal/violet,
        // forest=green/ivory).
        Color accentA = pal.rowBorder;
        Color accentB = pal.borderLight;
        if (dungeon_.themeId == "crystal_mine") {
            accentA = pal.crystal;
            accentB = pal.magic;
        } else if (dungeon_.themeId == "hollow_forest") {
            accentA = pal.success;
            accentB = pal.text;
        }
        const auto pip = [](int x, int y, Color c) {
            DrawRectangle(x, y + 1, 3, 1, c);
            DrawRectangle(x + 1, y, 1, 3, c);
        };
        pip(bx + 13, by - 1, accentA);
        pip(bx + 19, by - 1, accentB);
        pip(bx + bw - 16, by - 1, accentA);
        pip(bx + bw - 22, by - 1, accentB);
    }

    // Theme tiles from the catalog (M15); colored rectangles remain the
    // fallback for themes that have no art yet.
    const std::string tilePrefix = "tiles." + dungeon_.themeId + ".";
    const std::string wallId = tilePrefix + "wall";
    const std::string doorId = tilePrefix + "door";
    const std::string floorId = tilePrefix + "floor";
    const std::string accentId = tilePrefix + "accent";
    const bool hasAccent = context_.resources.hasTexture(accentId);
    const int accentSalt = currentRoom_ * 101 + static_cast<int>(dungeon_.seed % 251u);
    for (int ty = 0; ty < roomMap_.height(); ++ty) {
        for (int tx = 0; tx < roomMap_.width(); ++tx) {
            const town::Tile t = roomMap_.at(tx, ty);
            // Deterministic sparse accent variants break up plain floors
            // (crystal clusters, rubble, shrine stones) — presentation only.
            const bool accent = hasAccent && t == town::Tile::Ground &&
                                (tx * 31 + ty * 17 + accentSalt) % 13 == 0;
            const std::string& id = t == town::Tile::Building ? wallId
                                    : t == town::Tile::Door  ? doorId
                                    : accent                 ? accentId
                                                             : floorId;
            if (context_.resources.hasTexture(id)) {
                DrawTexture(context_.resources.texture(id), originX_ + tx * kTile,
                            originY_ + ty * kTile, WHITE);
            } else {
                DrawRectangle(originX_ + tx * kTile, originY_ + ty * kTile, kTile, kTile,
                              tileColor(t));
            }
        }
    }

    // World sprites: markers first, then the player; text labels and the
    // facing indicator draw in a final overlay pass so nothing occludes them.
    for (const Marker& m : markers_) {
        Color c{};
        const char* glyph = "?";
        const char* spriteId = nullptr;   // M17 silhouette (shape encodes tier)
        const char* fallbackId = nullptr; // M15 prop sprite
        Color tint = WHITE;
        switch (m.kind) {
            case MarkerKind::GateTeam:
            case MarkerKind::GuardTeam:
                c = Color{206, 84, 84, 255};
                glyph = "!";
                if (m.teamIndex >= 0 && m.teamIndex < static_cast<int>(teamTier_.size())) {
                    spriteId = tierSilhouetteId(teamTier_[static_cast<std::size_t>(m.teamIndex)]);
                }
                fallbackId = "prop.gate_marker";
                break;
            case MarkerKind::Boss:
                c = Color{184, 92, 206, 255};
                glyph = "B";
                spriteId = "marker.enemy.boss";
                fallbackId = "prop.boss_marker";
                // M82: on a stair floor the slot holds the elite gate, and the
                // marker tells that truth — elite silhouette, gate colors.
                if (!finalFloor()) {
                    c = Color{206, 84, 84, 255};
                    glyph = "!";
                    if (m.teamIndex >= 0 && m.teamIndex < static_cast<int>(teamTier_.size())) {
                        spriteId =
                            tierSilhouetteId(teamTier_[static_cast<std::size_t>(m.teamIndex)]);
                    }
                    fallbackId = "prop.gate_marker";
                }
                break;
            case MarkerKind::Stairs:  // M82: the way down (glyph marker,
                c = Color{110, 214, 220, 255};  // the M55-rite precedent)
                glyph = "v";
                break;
            case MarkerKind::Chest: {
                const dungeon::Chest& chest =
                    dungeon_.rooms[static_cast<std::size_t>(currentRoom_)].chest;
                c = chest.opened ? Color{120, 100, 50, 255} : Color{232, 200, 96, 255};
                glyph = chest.trapped && !chest.opened ? "T" : "C";
                fallbackId = chest.opened ? nullptr : "prop.chest";
                if (chest.trapped && !chest.opened) {
                    tint = Color{255, 150, 150, 255};  // visibly dangerous
                    c = Color{220, 120, 96, 255};
                }
                break;
            }
            case MarkerKind::Event: {
                switch (dungeon_.rooms[static_cast<std::size_t>(currentRoom_)].event.kind) {
                    case dungeon::RoomEventKind::Shrine:
                        c = Color{100, 220, 215, 255};
                        glyph = "S";
                        spriteId = "prop.event.shrine";
                        break;
                    case dungeon::RoomEventKind::HealingSpring:
                        c = Color{90, 150, 220, 255};
                        glyph = "~";
                        spriteId = "prop.event.spring";
                        break;
                    case dungeon::RoomEventKind::Merchant:
                        c = Color{230, 200, 110, 255};
                        glyph = "M";
                        spriteId = "prop.event.merchant";
                        break;
                    case dungeon::RoomEventKind::EliteChallenge:
                        c = Color{210, 120, 90, 255};
                        glyph = "!";
                        spriteId = "prop.event.totem";
                        break;
                    case dungeon::RoomEventKind::ScoreWager:
                        c = Color{180, 110, 220, 255};
                        glyph = "?";
                        spriteId = "prop.event.omen";
                        break;
                    case dungeon::RoomEventKind::RestToken:
                        c = Color{240, 170, 90, 255};
                        glyph = "R";
                        spriteId = "prop.event.rest";
                        break;
                    case dungeon::RoomEventKind::RoyalRelic:  // M44
                        c = Color{235, 225, 140, 255};
                        glyph = "*";
                        spriteId = "prop.event.relic";
                        break;
                    case dungeon::RoomEventKind::ArmoryGhost:  // M55: spectral
                        c = Color{200, 205, 235, 255};
                        glyph = "G";
                        break;
                    case dungeon::RoomEventKind::MinersCache:  // M55: crystal cache
                        c = Color{120, 220, 235, 255};
                        glyph = "$";
                        break;
                    case dungeon::RoomEventKind::DuckPeddler:  // M76: a sickly waddle
                        c = Color{170, 210, 100, 255};
                        glyph = "D";
                        break;
                    case dungeon::RoomEventKind::ElderRoot:  // M55: green root
                        c = Color{110, 190, 120, 255};
                        glyph = "T";
                        break;
                    case dungeon::RoomEventKind::Surveyor:  // M93: parchment
                        c = Color{225, 205, 150, 255};
                        glyph = "S";
                        break;
                    case dungeon::RoomEventKind::Dragonform:  // M93: scale-red
                        c = Color{220, 120, 90, 255};
                        glyph = "W";
                        break;
                    case dungeon::RoomEventKind::None:
                        break;
                }
                break;
            }
            case MarkerKind::MapPiece:  // M65: a golden scrap (glyph marker,
                c = Color{235, 214, 112, 255};  // the M55-rite precedent)
                glyph = "?";
                break;
            case MarkerKind::Chart:  // M66: a cyan chart scrap
                c = Color{110, 214, 220, 255};
                glyph = "M";
                break;
            case MarkerKind::Buried:  // M66: the X the chart promised
                c = Color{224, 96, 84, 255};
                glyph = "X";
                break;
        }
        const int mx = originX_ + m.x * kTile;
        const int my = originY_ + m.y * kTile;
        if (spriteId != nullptr && context_.resources.hasTexture(spriteId)) {
            DrawTexture(context_.resources.texture(spriteId), mx + 2, my + 2, tint);
        } else if (fallbackId != nullptr && context_.resources.hasTexture(fallbackId)) {
            DrawTexture(context_.resources.texture(fallbackId), mx + 2, my + 2, tint);
        } else {
            DrawRectangle(mx + 2, my + 2, kTile - 4, kTile - 4, c);
            ui::drawTextCentered(glyph, mx + kTile / 2, my + 4, 8, Color{20, 20, 20, 255});
        }
    }

    // Player: directional walk animation, then static sprite, then rectangle.
    const float pcx = originX_ + player_.x + player_.w * 0.5f;
    const float pcy = originY_ + player_.y + player_.h * 0.5f;
    const render::Facing facing = render::facingFrom(facing_.x, facing_.y);
    if (!render::drawAnimationCentered(context_.resources, walkAnimId(facing),
                                       moving_ ? walkTime_ : 0.0f, pcx, pcy) &&
        !render::drawTextureCentered(context_.resources, "actor.player.overworld", pcx, pcy)) {
        DrawRectangle(originX_ + static_cast<int>(player_.x),
                      originY_ + static_cast<int>(player_.y), static_cast<int>(player_.w),
                      static_cast<int>(player_.h), Color{236, 224, 128, 255});
    }

    // Overlay pass: danger labels above teams, pulsing brackets on the
    // interactable the player is facing (or standing on, for chests).
    for (const Marker& m : markers_) {
        if (m.kind != MarkerKind::Chest && m.teamIndex >= 0 &&
            m.teamIndex < static_cast<int>(teamTier_.size())) {
            const danger::Tier tier = teamTier_[static_cast<std::size_t>(m.teamIndex)];
            ui::drawTextCentered(danger::tierName(tier), originX_ + m.x * kTile + kTile / 2,
                                 originY_ + m.y * kTile - 8, 8, tierColor(tier));
        }
    }
    const Marker* highlight = facingMarker_;
    if (highlight == nullptr && onChest_) {
        for (const Marker& m : markers_) {
            if (m.kind == MarkerKind::Chest) {
                highlight = &m;
                break;
            }
        }
    }
    if (highlight != nullptr) {
        render::drawAnimationCentered(context_.resources, "anim.ui.facing_brackets", worldTime_,
                                      static_cast<float>(originX_ + highlight->x * kTile + kTile / 2),
                                      static_cast<float>(originY_ + highlight->y * kTile + kTile / 2));
    }

    // HUD (M46): theme/depth, gates, and gold as compact chips top-left.
    {
        Color themeAccent = pal.rowBorder;
        if (dungeon_.themeId == "crystal_mine") {
            themeAccent = pal.crystal;
        } else if (dungeon_.themeId == "hollow_forest") {
            themeAccent = pal.success;
        }
        int cx = 4;
        cx += ui::drawChip(
            dungeon_.floorCount > 1
                ? TextFormat("%s  D%d  F%d/%d", dungeon_.themeName.c_str(), dungeon_.depth,
                             dungeon_.floorIndex + 1, dungeon_.floorCount)
                : TextFormat("%s  D%d", dungeon_.themeName.c_str(), dungeon_.depth),
                           cx, 4, themeAccent) + 4;
        cx += ui::drawChip(TextFormat("Gates %d", dungeon_.mandatoryGates), cx, 4, pal.danger) + 4;
        cx += ui::drawChip(TextFormat("%dg", context_.party.gold), cx, 4, pal.gold) + 4;
        // M93: the danger counter, always visible (owner: "the player is
        // aware of the countdown") — urgent color inside the last stretch.
        cx += ui::drawChip(TextFormat("Patrol %d", dangerSteps_), cx, 4,
                           dangerSteps_ <= 20 ? pal.danger : pal.borderMid) + 4;
        // M93: the armed dragonform, until its battle spends it.
        if (dragonformArmed_) {
            cx += ui::drawChip("Dragonform: next battle", cx, 4, pal.danger) + 4;
        }
        if (chartFound_) {
            ui::drawChip("Treasure!", cx, 4, pal.gold);  // M66: the X awaits
        }
    }

    renderMinimap();

    const int h = context_.virtualHeight;
    const InputMap& map = context_.input.map();
    const ActiveDevice device = context_.input.activeDevice();
    std::string text;
    if (!message_.empty()) {
        text = message_;
    } else if (onChest_) {
        const dungeon::Chest& chest = dungeon_.rooms[static_cast<std::size_t>(currentRoom_)].chest;
        const std::string rarity = chest.rarity.empty() ? "" : " (" + chest.rarity + ")";
        if (chest.guarded) {
            text = "Guarded chest" + rarity + " - defeat the guards to claim";
        } else if (chest.trapped && !chest.opened) {
            // Trapped treasure: the wound is stated before the take.
            text = input::prompt(map, InputAction::Confirm, device,
                                 "Take trapped chest" + rarity) +
                   " - the party suffers 25% max-HP wounds";
        } else {
            text = input::prompt(map, InputAction::Confirm, device, "Open chest" + rarity);
        }
    } else if (onMapPiece_) {
        // M65: the find explains itself before the take.
        text = input::prompt(map, InputAction::Confirm, device, "Take the Secret Map Piece") +
               TextFormat("  (%d of %d held)", context_.party.mapPieces, kMapPiecesNeeded);
    } else if (onChart_) {
        text = input::prompt(map, InputAction::Confirm, device,
                             "Read the weathered map - it shows THIS dungeon");
    } else if (onBuried_) {
        text = input::prompt(map, InputAction::Confirm, device, "Dig up the buried treasure");
    } else if (facingMarker_ != nullptr && facingMarker_->kind == MarkerKind::Stairs) {
        // M82: the opened stairway states where it leads before the step.
        text = input::prompt(map, InputAction::Confirm, device,
                             TextFormat("Descend to floor %d of %d", dungeon_.floorIndex + 2,
                                        dungeon_.floorCount));
    } else if (facingMarker_ != nullptr && facingMarker_->kind == MarkerKind::Event) {
        text = eventPromptText();
    } else if (facingMarker_ != nullptr && facingMarker_->teamIndex >= 0 &&
               facingMarker_->teamIndex < static_cast<int>(teamTier_.size())) {
        const danger::Tier tier = teamTier_[static_cast<std::size_t>(facingMarker_->teamIndex)];
        const dungeon::EnemyTeam& team =
            dungeon_.teams[static_cast<std::size_t>(facingMarker_->teamIndex)];
        // Show the team name with tier/count/tags — the visible-encounter
        // contract (game_design.md §6, defect UI-INFO-005).
        text = input::prompt(map, InputAction::Confirm, device, "Fight ") + team.name +
               "  -  " + danger::tierName(tier) + "  x" + std::to_string(team.count());
        if (!team.tags.empty()) {
            text += "  [";
            for (std::size_t i = 0; i < team.tags.size(); ++i) {
                if (i != 0) {
                    text += ",";
                }
                text += team.tags[i];
            }
            text += "]";
        }
    }
    if (text.empty()) {
        // Idle: structured keycap hint groups in the shared footer strip.
        const std::string moveLabel =
            device == ActiveDevice::Keyboard
                ? input::primaryLabel(map, InputAction::MoveUp, device) + "/" +
                      input::primaryLabel(map, InputAction::MoveDown, device) + "/" +
                      input::primaryLabel(map, InputAction::MoveLeft, device) + "/" +
                      input::primaryLabel(map, InputAction::MoveRight, device)
                : "D-Pad/Stick";
        ui::drawFooterHints(
            {{moveLabel, "Move"},
             {input::primaryLabel(map, InputAction::Details, device), "Details"},
             {input::primaryLabel(map, InputAction::Menu, device), "Pause"}},
            context_.virtualWidth, h, "dungeon.footer");
    } else {
        // Contextual prompt or transient message: strip plus one fitted line.
        ui::drawFooterHints({}, context_.virtualWidth, h, "dungeon.footer");
        const int promptW = ui::measureText(text, 8);
        const int promptX = std::max(4, (context_.virtualWidth - promptW) / 2);
        ui::drawTextFitted(text, promptX, h - 12, context_.virtualWidth - promptX - 4, 8,
                           pal.text, "dungeon.prompt");
    }

    // M80: the flavor and outcome panels sit above everything (modal; they
    // are never open at once — the outcome follows the flavor's Confirm).
    if (eventPanelOpen_) {
        renderEventPanel();
    }
    if (outcomePanelOpen_) {
        renderOutcomePanel();
    }
}

}  // namespace cd
