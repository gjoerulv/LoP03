#include "states/DungeonState.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <memory>
#include <string>
#include <utility>

#include "audio/AudioManager.hpp"
#include "content/ContentDatabase.hpp"
#include "core/AppContext.hpp"
#include "core/FadeController.hpp"
#include "game/Party.hpp"
#include "input/Input.hpp"
#include "raylib.h"
#include "score/ScoreEntry.hpp"
#include "score/Scoreboard.hpp"
#include "score/Scoring.hpp"
#include "states/BattleState.hpp"
#include "states/DungeonMenuState.hpp"
#include "states/DungeonResultState.hpp"
#include "states/StateStack.hpp"
#include "town/Movement.hpp"
#include "ui/UiDraw.hpp"

namespace cd {

namespace {

constexpr int kRoomW = 26;
constexpr int kRoomH = 15;
constexpr int kTile = town::Tilemap::kTileSize;
constexpr float kSpeed = 78.0f;
constexpr float kPlayerSize = 12.0f;

struct TilePos {
    int x;
    int y;
};

std::array<TilePos, 2> borderGap(dungeon::Dir d) {
    switch (d) {
        case dungeon::Dir::North: return {{{12, 0}, {13, 0}}};
        case dungeon::Dir::South: return {{{12, kRoomH - 1}, {13, kRoomH - 1}}};
        case dungeon::Dir::East: return {{{kRoomW - 1, 7}, {kRoomW - 1, 8}}};
        case dungeon::Dir::West: return {{{0, 7}, {0, 8}}};
    }
    return {{{12, 0}, {13, 0}}};
}

std::array<TilePos, 2> interiorGap(dungeon::Dir d) {
    switch (d) {
        case dungeon::Dir::North: return {{{12, 1}, {13, 1}}};
        case dungeon::Dir::South: return {{{12, kRoomH - 2}, {13, kRoomH - 2}}};
        case dungeon::Dir::East: return {{{kRoomW - 2, 7}, {kRoomW - 2, 8}}};
        case dungeon::Dir::West: return {{{1, 7}, {1, 8}}};
    }
    return {{{12, 1}, {13, 1}}};
}

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

}  // namespace

DungeonState::DungeonState(StateStack& stack, AppContext& context, dungeon::Dungeon dungeon)
    : GameState(stack), context_(context), dungeon_(std::move(dungeon)),
      roomMap_(kRoomW, kRoomH) {
    teamTier_.reserve(dungeon_.teams.size());
    for (const dungeon::EnemyTeam& team : dungeon_.teams) {
        teamTier_.push_back(danger::assess(team, dungeon_.depth, context_.content));
    }
    context_.fade.start();
    context_.audio.setMusic(MusicTrack::Dungeon);
    enterRoom(dungeon_.startRoom, std::nullopt);
}

void DungeonState::buildRoom() {
    facingMarker_ = nullptr;
    onChest_ = false;
    doorTiles_.clear();
    markers_.clear();

    dungeon::Room& room = dungeon_.rooms[static_cast<std::size_t>(currentRoom_)];

    town::Tilemap map(kRoomW, kRoomH, town::Tile::Ground);
    for (int x = 0; x < kRoomW; ++x) {
        map.set(x, 0, town::Tile::Building);
        map.set(x, kRoomH - 1, town::Tile::Building);
    }
    for (int y = 0; y < kRoomH; ++y) {
        map.set(0, y, town::Tile::Building);
        map.set(kRoomW - 1, y, town::Tile::Building);
    }

    for (dungeon::Dir dir : {dungeon::Dir::North, dungeon::Dir::East, dungeon::Dir::South,
                             dungeon::Dir::West}) {
        const dungeon::Door& door = room.door(dir);
        if (door.neighbor < 0) {
            continue;
        }
        if (door.gated) {
            const TilePos inner = interiorGap(dir)[0];
            map.set(inner.x, inner.y, town::Tile::Building);  // blocked passage
            markers_.push_back({inner.x, inner.y, MarkerKind::GateTeam, door.teamIndex, dir});
        } else {
            for (const TilePos& bp : borderGap(dir)) {
                map.set(bp.x, bp.y, town::Tile::Door);
                doorTiles_.push_back({bp.x, bp.y, dir, door.neighbor});
            }
        }
    }

    if (room.type == dungeon::RoomType::Boss && room.teamIndex >= 0) {
        map.set(12, 7, town::Tile::Building);
        markers_.push_back({12, 7, MarkerKind::Boss, room.teamIndex, dungeon::Dir::North});
    }
    if (room.chest.present) {
        markers_.push_back({12, 7, MarkerKind::Chest, -1, dungeon::Dir::North});
        if (room.teamIndex >= 0) {
            map.set(13, 7, town::Tile::Building);
            markers_.push_back({13, 7, MarkerKind::GuardTeam, room.teamIndex, dungeon::Dir::North});
        }
    }

    roomMap_ = std::move(map);
}

void DungeonState::enterRoom(int index, std::optional<dungeon::Dir> entrySide) {
    currentRoom_ = index;
    dungeon_.rooms[static_cast<std::size_t>(index)].visited = true;
    buildRoom();

    const float inset = (kTile - kPlayerSize) * 0.5f;
    TilePos start{12, 7};
    if (entrySide) {
        start = interiorGap(*entrySide)[0];
    }
    player_ = Rect{static_cast<float>(start.x) * kTile + inset,
                   static_cast<float>(start.y) * kTile + inset, kPlayerSize, kPlayerSize};
}

void DungeonState::recomputeInteraction(int tx, int ty) {
    onChest_ = false;
    facingMarker_ = nullptr;
    for (const Marker& m : markers_) {
        if (m.kind == MarkerKind::Chest && m.x == tx && m.y == ty) {
            onChest_ = true;
        }
    }
    const int fx = facing_.x > 0.4f ? 1 : (facing_.x < -0.4f ? -1 : 0);
    const int fy = facing_.y > 0.4f ? 1 : (facing_.y < -0.4f ? -1 : 0);
    for (const Marker& m : markers_) {
        if (m.kind != MarkerKind::Chest && m.x == tx + fx && m.y == ty + fy) {
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
        message_ = "The chest is empty.";
        messageTimer_ = 2.0f;
        return;
    }
    if (room.chest.guarded) {
        message_ = "Guarded - defeat the team first.";
        messageTimer_ = 2.5f;
        return;
    }
    room.chest.opened = true;
    context_.audio.play(Sfx::Chest);
    context_.party.gold += room.chest.gold;
    ++run_.chestsOpened;
    run_.treasureGold += room.chest.gold;
    std::string msg = TextFormat("Found %d gold", room.chest.gold);
    if (!room.chest.itemId.empty()) {
        context_.party.inventory.add(room.chest.itemId, 1);
        const char* name = room.chest.itemId.c_str();
        if (const content::ItemDef* it = context_.content.findItem(room.chest.itemId)) {
            name = it->name.c_str();
        }
        msg += std::string(" + ") + name;
    }
    message_ = msg;
    messageTimer_ = 3.0f;
}

void DungeonState::interact() {
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
        case MarkerKind::Boss: kind = EncounterKind::Boss; break;
        case MarkerKind::Chest: return;
    }
    startBattle(facingMarker_->teamIndex, kind, facingMarker_->gateDir);
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
    battle::Battle b =
        battle::buildBattle(context_.party, dungeon_.teams[static_cast<std::size_t>(teamIndex)],
                            context_.content);
    stack().pushState(std::make_unique<BattleState>(stack(), context_, std::move(b), &battleResult_));
}

void DungeonState::onResume() {
    if (pendingKind_ == EncounterKind::None) {
        return;
    }
    const EncounterKind kind = pendingKind_;
    pendingKind_ = EncounterKind::None;
    const battle::Outcome outcome = battleResult_.outcome;

    // Returning from a battle: fade in and restore dungeon music (town states
    // override it again if we end up leaving).
    context_.fade.start();
    context_.audio.setMusic(MusicTrack::Dungeon);

    // Accumulate run statistics regardless of outcome.
    run_.battleTurns += battleResult_.rounds;
    if (battleResult_.partyKoOccurred) {
        run_.noDeath = false;
    }

    if (outcome == battle::Outcome::Escaped) {
        ++run_.escapes;
        return;  // gate intact; resume in the dungeon
    }
    if (outcome == battle::Outcome::Defeat) {
        healFull(context_.party);
        context_.party.gold /= 2;
        stack().popState();  // game over -> back to town
        return;
    }
    if (outcome != battle::Outcome::Victory) {
        return;
    }

    // Credit the danger defeated.
    if (pendingTeamIndex_ >= 0 && pendingTeamIndex_ < static_cast<int>(teamTier_.size())) {
        run_.dangerDefeated += danger::tierWeight(teamTier_[static_cast<std::size_t>(pendingTeamIndex_)]);
    }

    // Award XP and gold for the defeated team.
    std::string reward;
    if (pendingTeamIndex_ >= 0 && pendingTeamIndex_ < static_cast<int>(dungeon_.teams.size())) {
        const dungeon::EnemyTeam& team = dungeon_.teams[static_cast<std::size_t>(pendingTeamIndex_)];
        int xp = 0;
        int gold = 0;
        for (const std::string& id : team.enemyIds) {
            if (const content::EnemyDef* e = context_.content.findEnemy(id)) {
                xp += e->xpReward;
                gold += e->goldReward;
            }
        }
        if (const content::BossDef* boss = context_.content.findBoss(team.bossId)) {
            xp += boss->xpReward;
            gold += boss->goldReward;
        }
        context_.party.gold += gold;
        grantPartyXp(context_.party, xp, context_.content);
        if (xp > 0) {
            reward = TextFormat(" (+%d XP, +%dg)", xp, gold);
        }
    }

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
        message_ = "The gate is clear!" + reward;
        messageTimer_ = 2.5f;
    } else if (kind == EncounterKind::Guard) {
        room.teamIndex = -1;
        room.chest.guarded = false;
        buildRoom();
        message_ = "The guards fall." + reward;
        messageTimer_ = 2.5f;
    } else if (kind == EncounterKind::Boss) {
        completeDungeon();
    }
}

void DungeonState::completeDungeon() {
    score::RunSummary summary;
    summary.completed = true;
    summary.battleTurns = run_.battleTurns;
    summary.dangerDefeated = run_.dangerDefeated;
    summary.chestsOpened = run_.chestsOpened;
    summary.treasureGold = run_.treasureGold;
    summary.noDeath = run_.noDeath;
    summary.escapes = run_.escapes;

    const int total = score::computeScore(summary);

    score::ScoreEntry entry;
    entry.score = total;
    entry.battleTurns = summary.battleTurns;
    entry.dangerDefeated = summary.dangerDefeated;
    entry.chestsOpened = summary.chestsOpened;
    entry.noDeath = summary.noDeath;
    entry.depth = dungeon_.depth;
    entry.theme = dungeon_.themeName;
    entry.seed = dungeon_.seed;
    context_.scoreboard.add(entry);
    content::LoadReport saveReport;
    context_.scoreboard.save(saveReport);

    stack().pushState(std::make_unique<DungeonResultState>(stack(), context_, summary, total));
}

void DungeonState::handleInput(const Input& input) {
    moveX_ = (input.down(InputAction::MoveRight) ? 1.0f : 0.0f) -
             (input.down(InputAction::MoveLeft) ? 1.0f : 0.0f);
    moveY_ = (input.down(InputAction::MoveDown) ? 1.0f : 0.0f) -
             (input.down(InputAction::MoveUp) ? 1.0f : 0.0f);

    if (input.pressed(InputAction::Confirm)) {
        interact();
    }
    if (input.pressed(InputAction::Menu) || input.pressed(InputAction::Cancel)) {
        stack().pushState(std::make_unique<DungeonMenuState>(stack(), context_));
    }
}

void DungeonState::update(float dt) {
    const float length = std::sqrt(moveX_ * moveX_ + moveY_ * moveY_);
    if (length > 0.0001f) {
        const float nx = moveX_ / length;
        const float ny = moveY_ / length;
        facing_ = Vec2{nx, ny};
        const Vec2 moved =
            town::resolveMove(player_, nx * kSpeed * dt, ny * kSpeed * dt, roomMap_);
        player_.x = moved.x;
        player_.y = moved.y;
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
    const int ox = context_.virtualWidth - dungeon_.gridW * step - 4;
    const int oy = 4;

    for (const dungeon::Room& r : dungeon_.rooms) {
        const int cx = ox + r.gridX * step;
        const int cy = oy + r.gridY * step;
        // Door links to neighbors with a higher room... draw from each room's center.
        for (dungeon::Dir dir : {dungeon::Dir::East, dungeon::Dir::South}) {
            const dungeon::Door& door = r.door(dir);
            if (door.neighbor < 0) {
                continue;
            }
            const Color line = door.gated ? Color{210, 90, 90, 255} : Color{120, 120, 140, 255};
            DrawLine(cx + cell / 2, cy + cell / 2, cx + cell / 2 + dungeon::dirDx(dir) * step,
                     cy + cell / 2 + dungeon::dirDy(dir) * step, line);
        }
    }
    for (std::size_t i = 0; i < dungeon_.rooms.size(); ++i) {
        const dungeon::Room& r = dungeon_.rooms[i];
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
        if (static_cast<int>(i) == currentRoom_) {
            DrawRectangleLines(cx - 1, cy - 1, cell + 2, cell + 2, RAYWHITE);
        }
    }
}

void DungeonState::render() {
    ClearBackground(BLACK);
    for (int ty = 0; ty < roomMap_.height(); ++ty) {
        for (int tx = 0; tx < roomMap_.width(); ++tx) {
            DrawRectangle(tx * kTile, ty * kTile, kTile, kTile, tileColor(roomMap_.at(tx, ty)));
        }
    }

    // Markers.
    for (const Marker& m : markers_) {
        Color c{};
        const char* glyph = "?";
        switch (m.kind) {
            case MarkerKind::GateTeam:
            case MarkerKind::GuardTeam:
                c = Color{206, 84, 84, 255};
                glyph = "!";
                break;
            case MarkerKind::Boss:
                c = Color{184, 92, 206, 255};
                glyph = "B";
                break;
            case MarkerKind::Chest: {
                const bool opened =
                    dungeon_.rooms[static_cast<std::size_t>(currentRoom_)].chest.opened;
                c = opened ? Color{120, 100, 50, 255} : Color{232, 200, 96, 255};
                glyph = "C";
                break;
            }
        }
        DrawRectangle(m.x * kTile + 2, m.y * kTile + 2, kTile - 4, kTile - 4, c);
        ui::drawTextCentered(glyph, m.x * kTile + kTile / 2, m.y * kTile + 4, 8,
                             Color{20, 20, 20, 255});

        // Visible danger label above an enemy team.
        if (m.kind != MarkerKind::Chest && m.teamIndex >= 0 &&
            m.teamIndex < static_cast<int>(teamTier_.size())) {
            const danger::Tier tier = teamTier_[static_cast<std::size_t>(m.teamIndex)];
            ui::drawTextCentered(danger::tierName(tier), m.x * kTile + kTile / 2,
                                 m.y * kTile - 8, 8, tierColor(tier));
        }
    }

    // Player.
    DrawRectangle(static_cast<int>(player_.x), static_cast<int>(player_.y),
                  static_cast<int>(player_.w), static_cast<int>(player_.h),
                  Color{236, 224, 128, 255});

    // HUD: backdrop sized to the measured lines so text never spills past it.
    const std::string hudLine1 = TextFormat("%s  depth %d  gates %d", dungeon_.themeName.c_str(),
                                            dungeon_.depth, dungeon_.mandatoryGates);
    const std::string hudLine2 = TextFormat("Gold %d", context_.party.gold);
    const int hudW = std::max(ui::measureText(hudLine1, 8), ui::measureText(hudLine2, 8)) + 6;
    DrawRectangle(2, 2, hudW, 24, Color{0, 0, 0, 150});
    ui::drawTextFitted(hudLine1, 5, 4, context_.virtualWidth - 10, 8, RAYWHITE, "dungeon.hud");
    ui::drawTextFitted(hudLine2, 5, 14, context_.virtualWidth - 10, 8,
                       Color{230, 220, 150, 255}, "dungeon.hud");

    renderMinimap();

    const int h = context_.virtualHeight;
    DrawRectangle(0, h - 16, context_.virtualWidth, 16, Color{0, 0, 0, 165});
    const char* prompt = "Arrows/WASD: Move    Menu: Pause / Retreat";
    std::string promptBuf;
    if (!message_.empty()) {
        prompt = message_.c_str();
    } else if (onChest_) {
        const dungeon::Chest& chest = dungeon_.rooms[static_cast<std::size_t>(currentRoom_)].chest;
        const std::string rarity = chest.rarity.empty() ? "" : " (" + chest.rarity + ")";
        promptBuf = chest.guarded ? "Guarded chest" + rarity + " - defeat the guards to claim"
                                  : "Confirm: Open chest" + rarity;
        prompt = promptBuf.c_str();
    } else if (facingMarker_ != nullptr && facingMarker_->teamIndex >= 0 &&
               facingMarker_->teamIndex < static_cast<int>(teamTier_.size())) {
        const danger::Tier tier = teamTier_[static_cast<std::size_t>(facingMarker_->teamIndex)];
        const dungeon::EnemyTeam& team =
            dungeon_.teams[static_cast<std::size_t>(facingMarker_->teamIndex)];
        // Show the team name with tier/count/tags — the visible-encounter
        // contract (game_design.md §6, defect UI-INFO-005).
        promptBuf = std::string("Confirm: Fight ") + team.name + "  -  " +
                    danger::tierName(tier) + "  x" + std::to_string(team.count());
        if (!team.tags.empty()) {
            promptBuf += "  [";
            for (std::size_t i = 0; i < team.tags.size(); ++i) {
                if (i != 0) {
                    promptBuf += ",";
                }
                promptBuf += team.tags[i];
            }
            promptBuf += "]";
        }
        prompt = promptBuf.c_str();
    }
    const int promptW = ui::measureText(prompt, 8);
    const int promptX = std::max(4, (context_.virtualWidth - promptW) / 2);
    ui::drawTextFitted(prompt, promptX, h - 13, context_.virtualWidth - promptX - 4, 8,
                       Color{235, 230, 180, 255}, "dungeon.prompt");
}

}  // namespace cd
