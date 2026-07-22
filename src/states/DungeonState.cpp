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
#include "game/Party.hpp"
#include "game/Relics.hpp"  // the M44 relic grant (seeded, reload-proof)
#include "game/WorldLadder.hpp"
#include "input/Input.hpp"
#include "raylib.h"
#include "score/ScoreEntry.hpp"
#include "score/Scoreboard.hpp"
#include "score/Scoring.hpp"
#include "input/PromptLabels.hpp"
#include "render/SpriteDraw.hpp"
#include "resource/ResourceManager.hpp"
#include "settings/Settings.hpp"
#include "states/AchievementToast.hpp"
#include "states/BattleState.hpp"
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
constexpr float kPlayerSize = 12.0f;

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

DungeonState::DungeonState(StateStack& stack, AppContext& context, dungeon::Dungeon dungeon)
    : GameState(stack), context_(context), dungeon_(std::move(dungeon)),
      layouts_(dungeon::realizeAllRooms(dungeon_)), roomMap_(1, 1) {
    teamTier_.reserve(dungeon_.teams.size());
    for (const dungeon::EnemyTeam& team : dungeon_.teams) {
        teamTier_.push_back(danger::assess(team, dungeon_.depth, context_.content));
    }
    context_.fade.start();
    // Theme music + ambience are applied in onEnter(), not here: entering the
    // dungeon pops the Guild, which fires TownState::onResume and re-asserts
    // the town audio *after* this constructor runs. onEnter() runs after that
    // pop, so the dungeon audio wins (previously the ambience stayed on the
    // town bed for the whole dungeon).
    enterRoom(dungeon_.startRoom, std::nullopt);
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
#endif

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
        messageTimer_ = scaledMessageTime(context_, 2.0f);
        return;
    }
    if (room.chest.guarded) {
        message_ = "Guarded - defeat the team first.";
        messageTimer_ = scaledMessageTime(context_, 2.5f);
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
    if (room.chest.trapped) {
        // Exactly the wound the prompt warned about: 25% max HP, never fatal.
        for (Character& c : context_.party.members) {
            if (c.hp > 0) {
                c.hp = std::max(1, c.hp - c.maxHp / 4);
            }
        }
        msg = "The trap bites - the party is wounded! " + msg;
    }
    message_ = msg;
    messageTimer_ = scaledMessageTime(context_, 3.0f);
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
        case MarkerKind::Event:
            if (facingMarker_->teamIndex >= 0) {
                startBattle(facingMarker_->teamIndex, EncounterKind::Challenge,
                            facingMarker_->gateDir);
            } else {
                resolveEvent();
            }
            return;
        case MarkerKind::Chest: return;
    }
    startBattle(facingMarker_->teamIndex, kind, facingMarker_->gateDir);
}

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
                message_ = "The shrine asks " + std::to_string(ev.goldCost) +
                           "g - you cannot pay.";
                messageTimer_ = scaledMessageTime(context_, 2.5f);
                return;
            }
            context_.party.gold -= ev.goldCost;
            for (Character& c : context_.party.members) {
                c.hp = std::min(c.maxHp, c.hp + (c.maxHp - c.hp) / 2);
            }
            context_.audio.play(Sfx::Heal);
            message_ = "The shrine accepts your offering - the party's wounds half-mend.";
            break;
        }
        case dungeon::RoomEventKind::HealingSpring:
            for (Character& c : context_.party.members) {
                if (c.hp > 0) {
                    c.hp = std::min(c.maxHp, c.hp + c.maxHp * 40 / 100);
                }
            }
            context_.audio.play(Sfx::Heal);
            message_ = "The spring's water restores the party. It runs dry.";
            break;
        case dungeon::RoomEventKind::Merchant: {
            if (context_.party.gold < ev.goldCost) {
                context_.audio.play(Sfx::Error);
                message_ = "The merchant wants " + std::to_string(ev.goldCost) +
                           "g - you cannot pay.";
                messageTimer_ = scaledMessageTime(context_, 2.5f);
                return;
            }
            context_.party.gold -= ev.goldCost;
            context_.party.inventory.add(ev.itemId, 1);
            const content::ItemDef* it = context_.content.findItem(ev.itemId);
            context_.audio.play(Sfx::Interact);
            message_ = "Bought " + (it != nullptr ? it->name : ev.itemId) +
                       ". The merchant moves on.";
            break;
        }
        case dungeon::RoomEventKind::ScoreWager:
            run_.wagerAccepted = true;
            context_.audio.play(Sfx::Interact);
            message_ = "The omen accepts your dare. Finish without a death!";
            break;
        case dungeon::RoomEventKind::RestToken:
            context_.party.restTokens += 1;
            context_.audio.play(Sfx::Interact);
            message_ = "You pocket a free-rest token - redeem it at the inn.";
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
            message_ = "The reliquary yields " + (it != nullptr ? it->name : relicId) +
                       "! Save it for a foe that deserves it.";
            break;
        }
        case dungeon::RoomEventKind::EliteChallenge:
        case dungeon::RoomEventKind::None:
            return;  // challenges resolve through battle, not here
    }
    ev.resolved = true;
    buildRoom();
    messageTimer_ = scaledMessageTime(context_, 3.0f);
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
            if (context_.party.gold < ev.goldCost) {
                return "Merchant sells " + name + " for " + std::to_string(ev.goldCost) +
                       "g - you cannot pay.";
            }
            return input::prompt(map, InputAction::Confirm, device, "Buy " + name) + " for " +
                   std::to_string(ev.goldCost) + "g (dungeon prices)";
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
    battle::Battle b =
        battle::buildBattle(context_.party, dungeon_.teams[static_cast<std::size_t>(teamIndex)],
                            context_.content);
    stack().pushState(std::make_unique<BattleState>(stack(), context_, std::move(b), &battleResult_,
                                                    MusicTrack::None, &victoryStats_));
}

void DungeonState::onEnter() {
    // Runs after the Guild pop's TownState::onResume, so these win and the
    // dungeon actually gets its own theme music + ambience bed.
    context_.audio.setMusic(themeMusic(dungeon_.themeId));
    context_.audio.setAmbience(themeAmbience(dungeon_.themeId));
    maybeTutorialPrompt(stack(), context_, tutorial::kDungeonFirst);
}

void DungeonState::onResume() {
    if (pendingKind_ == EncounterKind::None) {
        return;
    }
    const EncounterKind kind = pendingKind_;
    pendingKind_ = EncounterKind::None;
    const battle::Outcome outcome = battleResult_.outcome;

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

    if (kind != EncounterKind::Boss) {
        maybeTutorialPrompt(stack(), context_, tutorial::kVictoryFirst);
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
        messageTimer_ = scaledMessageTime(context_, 2.5f);
    } else if (kind == EncounterKind::Guard) {
        room.teamIndex = -1;
        room.chest.guarded = false;
        buildRoom();
        message_ = "The guards fall." + reward;
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
        message_ = "Challenge won - double danger, +1 legendary token." + reward;
        messageTimer_ = scaledMessageTime(context_, 2.5f);
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
    // M33: advance the stakes baseline/penalty on a scoring completion. A
    // completed-but-zero run (extreme turn penalty) is treated like a score-0
    // run: it does not move the baseline (owner rule).
    if (total > 0) {
        context_.party.stakes =
            afterCompletedRun(context_.party.stakes, dungeon_.town, dungeon_.depth);
    }

    // M34: a stakes-raising completed run in town >= 2 may spawn the black
    // market, seeded from this run so reloading the entry autosave cannot reroll
    // it. The offer persists in the party save until purchased; a later hit
    // replaces it. Only reached on a real completion (total > 0).
    if (blackMarketShouldSpawn(total > 0, raisedStakes, dungeon_.town, dungeon_.seed,
                               dungeon_.depth)) {
        // Shared legendary pool (M39): the market and boss drops draw the same set.
        const std::vector<std::string> legendaryIds = legendaryDropPool(context_.content);
        if (!legendaryIds.empty()) {
            BlackMarketOffer offer;
            offer.present = true;
            offer.town = dungeon_.town;
            offer.itemId = legendaryIds[static_cast<std::size_t>(
                blackMarketItemIndex(dungeon_.seed, static_cast<int>(legendaryIds.size())))];
            offer.priceGold = blackMarketPriceGold(dungeon_.town);
            const MarketTile mt = kBlackMarketTiles[blackMarketTileIndex(dungeon_.seed)];
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
        rollBossDrops(dungeon_.seed, dungeon_.town, dungeon_.depth, context_.content);
    if (drops.tokens > 0) {
        context_.party.legendaryTokens += drops.tokens;
    }
    if (drops.legendary) {
        context_.party.inventory.add(drops.legendaryId, 1);
    }

    score::ScoreEntry entry;
    entry.score = total;
    entry.battleTurns = summary.battleTurns;
    entry.dangerDefeated = summary.dangerDefeated;
    entry.chestsOpened = summary.chestsOpened;
    entry.noDeath = summary.noDeath;
    entry.depth = dungeon_.depth;
    entry.theme = dungeon_.themeName;
    entry.seed = dungeon_.seed;
    entry.generationVersion = dungeon::kGenerationVersion;
    entry.partyLevel = highestLevel(context_.party);
    entry.battleRulesVersion = battle::kBattleRulesVersion;
    entry.townIndex = dungeon_.town;  // M32
    entry.stakesPenaltyPct = stakesPct;      // M33
    entry.classModPct = summary.classModPct;  // M45 (comparability tag, never ranked)
    context_.scoreboard.add(entry);
    content::LoadReport saveReport;
    context_.scoreboard.save(saveReport);

    // M42: fold this run's victory tallies into the party's personal records
    // (display-only, never ranked), then show the result with the run stats.
    if (victoryStats_.biggestHit > context_.party.recordBiggestHit) {
        context_.party.recordBiggestHit = victoryStats_.biggestHit;
    }
    if (victoryStats_.totalDamage > context_.party.recordRunDamage) {
        context_.party.recordRunDamage = victoryStats_.totalDamage;
    }
    stack().pushState(std::make_unique<DungeonResultState>(stack(), context_, summary, total, drops,
                                                           victoryStats_));

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

// M22: danger-tier reference plus whatever is currently faced.
void DungeonState::openDetails() {
    std::string body;
    if (facingMarker_ != nullptr && facingMarker_->teamIndex >= 0 &&
        facingMarker_->teamIndex < static_cast<int>(dungeon_.teams.size())) {
        const dungeon::EnemyTeam& team =
            dungeon_.teams[static_cast<std::size_t>(facingMarker_->teamIndex)];
        const danger::Tier tier = teamTier_[static_cast<std::size_t>(facingMarker_->teamIndex)];
        body += "Facing: " + team.name + " - " + std::string(danger::tierName(tier)) + ", " +
                std::to_string(team.enemyIds.size() + (team.bossId.empty() ? 0 : 1)) +
                " enemies.\n\n";
    }
    body +=
        "Danger is computed from the actual enemies' stats, skills, and team "
        "synergy - never hand-picked. Tiers from weakest to strongest: "
        "Trivial, Easy, Fair, Dangerous, Deadly, Boss.\n\nGate teams must be "
        "fought to reach the boss. Guarded chests show their rarity before "
        "the fight; escaping the guard forfeits the chest. Events state "
        "their full trade-off in the footer before you Confirm. Deeper "
        "dungeons field bigger, stronger teams - and pay better.";
    stack().pushState(
        std::make_unique<DetailsOverlayState>(stack(), context_, "Dungeon Details", body));
}

void DungeonState::update(float dt) {
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

    // Framed map well (M46) with the shared Inset construction.
    ui::drawFrame(ox - 6, oy - 6, dungeon_.gridW * step + 12, dungeon_.gridH * step + 12,
                  ui::FrameStyle::Inset);

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
                    case dungeon::RoomEventKind::None:
                        break;
                }
                break;
            }
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
        cx += ui::drawChip(TextFormat("%s  D%d", dungeon_.themeName.c_str(), dungeon_.depth),
                           cx, 4, themeAccent) + 4;
        cx += ui::drawChip(TextFormat("Gates %d", dungeon_.mandatoryGates), cx, 4, pal.danger) + 4;
        ui::drawChip(TextFormat("%dg", context_.party.gold), cx, 4, pal.gold);
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
}

}  // namespace cd
