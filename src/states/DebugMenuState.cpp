#include "states/DebugMenuState.hpp"

// The entire implementation is development-only. When CRYSTAL_DEBUG_OVERLAY is
// undefined (every Release build) this translation unit compiles to nothing, and
// the pause menus never reference the class, so it is structurally absent.
#ifdef CRYSTAL_DEBUG_OVERLAY

#include <algorithm>
#include <string>
#include <vector>

#include "audio/AudioManager.hpp"
#include "content/ContentDatabase.hpp"
#include "content/Definitions.hpp"
#include "core/AppContext.hpp"
#include "game/BlackMarket.hpp"
#include "game/BossDrops.hpp"  // legendaryDropPool
#include "game/Curios.hpp"       // M85 debug: grant curios toward the Dragon gate
#include "game/Party.hpp"
#include "game/Profile.hpp"
#include "game/TreasureMap.hpp"  // M85 debug: grant map pieces via the real rule
#include "game/WorldLadder.hpp"  // clampTown, kTownCount
#include "input/Input.hpp"
#include "input/PromptLabels.hpp"
#include "raylib.h"
#include "states/StateStack.hpp"
#include "ui/UiDraw.hpp"
#include "ui/UiStyle.hpp"

namespace cd {

namespace style = ui::style;

namespace {
constexpr int kListX = 40;
constexpr int kListY = 42;
constexpr int kListItemH = 16;
constexpr int kVisibleRows = 8;
constexpr int kLabelW = 300;
constexpr int kGoldMax = 9999999;
}  // namespace

DebugMenuState::DebugMenuState(StateStack& stack, AppContext& context, bool inDungeon)
    : GameState(stack), context_(context), inDungeon_(inDungeon) {
    rebuild();
}

void DebugMenuState::setMemberLevel(int index, int level) {
    if (index < 0 || index >= static_cast<int>(context_.party.members.size())) {
        return;
    }
    Character& m = context_.party.members[static_cast<std::size_t>(index)];
    m.level = std::clamp(level, 1, kMaxLevel);
    m.xp = 0;
    refreshCharacter(m, context_.content);
    m.hp = m.maxHp;  // full heal, as the footer promises
    m.mp = m.maxMp;
}

void DebugMenuState::rebuild() {
    rows_.clear();
    std::vector<ui::MenuItem> items;
    const Party& p = context_.party;
    const auto stepper = [](const std::string& value) { return "< " + value + " >"; };

    const auto add = [&](std::string label, Row kind, std::string suffix = {}, int param = 0) {
        ui::MenuItem item;
        item.label = std::move(label);
        item.suffix = std::move(suffix);
        item.enabled = true;
        items.push_back(std::move(item));
        rows_.push_back({kind, param});
    };

    for (std::size_t i = 0; i < p.members.size(); ++i) {
        add("Lv " + p.members[i].name, Row::Lv,
            stepper(std::to_string(p.members[i].level)), static_cast<int>(i));
    }
    add("Gold", Row::Gold, stepper(std::to_string(p.gold) + "g"));
    add("Legendary tokens", Row::Tokens, stepper(std::to_string(p.legendaryTokens)));
    add("Highest town", Row::Town, stepper(std::to_string(p.highestUnlockedTown)));
    add("Unlock castle", Row::UnlockCastle, p.castleUnlocked ? "done" : "");
    add("Grant 5x each consumable", Row::GrantConsumables);
    add("Grant 1x each legendary", Row::GrantLegendaries);
    if (!inDungeon_) {
        add("Spawn black market", Row::SpawnMarket, p.blackMarket.present ? "present" : "");
    }
    add("God mode", Row::GodMode, context_.cheats.godMode ? "ON" : "off");
    if (inDungeon_) {
        add("Instant dungeon clear", Row::InstantClear);
    }
    add("Unlock reward classes", Row::UnlockClasses,
        context_.profile.classesUnlocked() ? "done" : "");
    add("Fill bestiary", Row::FillBestiary,
        std::to_string(p.encountered.size()) + " foes");
    // M85 debug aids: the map cycle and the curio dozen (the Dragon's gate).
    add("Grant map piece", Row::GrantMapPiece,
        p.treasure.active ? "revealed: town " + std::to_string(p.treasure.town)
                          : std::to_string(p.mapPieces) + "/" +
                                std::to_string(kMapPiecesNeeded));
    add("Grant next curio", Row::GrantCurio,
        std::to_string(p.ownedCurios.size()) + "/" + std::to_string(kCurioCount));

    const int previous = menu_.cursor();
    menu_.setItems(std::move(items));
    menu_.setCursor(previous);
    scroll_.follow(static_cast<int>(menu_.size()), kVisibleRows, menu_.cursor());
}

void DebugMenuState::adjust(const RowDef& row, int dir) {
    Party& p = context_.party;
    switch (row.kind) {
        case Row::Lv:
            if (row.param >= 0 && row.param < static_cast<int>(p.members.size())) {
                setMemberLevel(row.param, p.members[static_cast<std::size_t>(row.param)].level + dir);
            }
            break;
        case Row::Gold:
            p.gold = std::clamp(p.gold + dir * 1000, 0, kGoldMax);
            break;
        case Row::Tokens:
            p.legendaryTokens = std::max(0, p.legendaryTokens + dir);
            break;
        case Row::Town:
            p.highestUnlockedTown = clampTown(p.highestUnlockedTown + dir);
            // Keep the current town inside the unlocked range (defensive).
            p.currentTown = std::min(p.currentTown, p.highestUnlockedTown);
            break;
        default:
            return;  // not a stepper
    }
    context_.audio.play(Sfx::Move);
    rebuild();
}

void DebugMenuState::activate(const RowDef& row) {
    Party& p = context_.party;
    switch (row.kind) {
        case Row::UnlockCastle:
            p.castleUnlocked = true;
            message_ = "Castle road unlocked.";
            break;
        case Row::GrantConsumables:
            for (const auto& [id, def] : context_.content.items()) {
                if (def.type == content::ItemType::Consumable) {
                    p.inventory.add(id, 5);
                }
            }
            message_ = "Granted 5x of every consumable.";
            break;
        case Row::GrantLegendaries: {
            const std::vector<std::string> ids = legendaryDropPool(context_.content);
            for (const std::string& id : ids) {
                p.inventory.add(id, 1);
            }
            message_ = "Granted 1x of every legendary.";
            break;
        }
        case Row::SpawnMarket: {
            std::vector<std::string> ids = legendaryDropPool(context_.content);
            std::sort(ids.begin(), ids.end());
            if (!ids.empty()) {
                BlackMarketOffer offer;
                offer.present = true;
                offer.town = p.currentTown;
                offer.itemId = ids.front();
                offer.priceGold = blackMarketPriceGold(p.currentTown);
                offer.tileX = kBlackMarketTiles[0].x;
                offer.tileY = kBlackMarketTiles[0].y;
                p.blackMarket = offer;
                // TownState builds its markers on enter, not every frame, so a
                // market spawned while standing in town needs a re-entry to show.
                message_ = "Black market set - re-enter town to see it.";
            }
            break;
        }
        case Row::GodMode:
            context_.cheats.godMode = !context_.cheats.godMode;
            message_ = context_.cheats.godMode
                           ? "God mode ON - party cannot be KO'd; scores won't post."
                           : "God mode off.";
            break;
        case Row::InstantClear:
            // Route through the real completion path: set the request, close this
            // menu and the pause menu below it, and let DungeonState::update pick
            // it up on its next tick. Do NOT touch members after popping.
            context_.cheats.requestDungeonClear = true;
            context_.audio.play(Sfx::Confirm);
            stack().popState();  // this debug menu
            stack().popState();  // the dungeon pause menu -> back in the dungeon
            return;
        case Row::UnlockClasses:
            context_.profile.recordKingDefeated();
            message_ = "Reward classes unlocked at character creation.";
            break;
        case Row::GrantMapPiece: {
            // Route through the REAL M65/M83 grant rule so the fourth piece
            // fires a genuine reveal (seeded guard roster, plausible scale) —
            // the debug path can never drift from the shipped one. The seed is
            // raylib-random: debug grants need no reload-proofness.
            const std::uint64_t seed =
                (static_cast<std::uint64_t>(GetRandomValue(1, 2000000000)) << 20) ^
                static_cast<std::uint64_t>(GetRandomValue(1, 2000000000));
            const int scalePct = combineTownScale(
                100 + context_.content.composition().statScalePct(8), p.currentTown);
            if (grantMapPiece(p.mapPieces, p.treasure, p.currentTown, context_.content,
                              seed, scalePct)) {
                message_ = "Fourth piece: treasure revealed in town " +
                           std::to_string(p.treasure.town) + ".";
            } else {
                message_ = "Map piece granted (" + std::to_string(p.mapPieces) + "/" +
                           std::to_string(kMapPiecesNeeded) + ").";
            }
            break;
        }
        case Row::GrantCurio: {
            // The next unowned curio in table order; twelve presses open the
            // Dragon's gate (M85).
            const CurioDef* next = nullptr;
            for (const CurioDef& c : kCurios) {
                if (!ownsCurio(p.ownedCurios, c.id)) {
                    next = &c;
                    break;
                }
            }
            if (next != nullptr) {
                p.ownedCurios.push_back(next->id);
                message_ = std::string("Granted ") + next->name + " (" +
                           std::to_string(p.ownedCurios.size()) + "/" +
                           std::to_string(kCurioCount) + ").";
            } else {
                message_ = "All twelve curios already held.";
            }
            break;
        }
        case Row::FillBestiary: {
            const auto record = [&p](const std::string& id) {
                if (std::find(p.encountered.begin(), p.encountered.end(), id) ==
                    p.encountered.end()) {
                    p.encountered.push_back(id);
                }
            };
            for (const auto& [id, def] : context_.content.enemies()) {
                (void)def;
                record(id);
            }
            for (const auto& [id, def] : context_.content.bosses()) {
                (void)def;
                record(id);
            }
            message_ = "Bestiary filled with every foe.";
            break;
        }
        default:
            return;  // steppers do nothing on Confirm
    }
    context_.audio.play(Sfx::Confirm);
    rebuild();
}

void DebugMenuState::handleInput(const Input& input) {
    if (input.navPressed(InputAction::MoveUp)) {
        menu_.moveUp();
        context_.audio.play(Sfx::Move);
    }
    if (input.navPressed(InputAction::MoveDown)) {
        menu_.moveDown();
        context_.audio.play(Sfx::Move);
    }
    scroll_.follow(static_cast<int>(menu_.size()), kVisibleRows, menu_.cursor());

    const int dir = (input.navPressed(InputAction::MoveRight) ? 1 : 0) -
                    (input.navPressed(InputAction::MoveLeft) ? 1 : 0);
    if (dir != 0 && menu_.cursor() >= 0 && menu_.cursor() < static_cast<int>(rows_.size())) {
        adjust(rows_[static_cast<std::size_t>(menu_.cursor())], dir);
    }
    if (input.pressed(InputAction::Confirm) && menu_.cursor() >= 0 &&
        menu_.cursor() < static_cast<int>(rows_.size())) {
        activate(rows_[static_cast<std::size_t>(menu_.cursor())]);
    }
    if (input.pressed(InputAction::Cancel) || input.pressed(InputAction::Menu)) {
        context_.audio.play(Sfx::Cancel);
        stack().popState();
    }
}

void DebugMenuState::render() {
    const int w = context_.virtualWidth;
    const int h = context_.virtualHeight;
    const style::Palette& p = style::palette();
    ui::drawModalDim(w, h);
    ui::drawTitlePlaque("Debug Menu", w / 2, 12, 12);

    ui::drawFrame(kListX - 24, kListY - 8, kLabelW + 60, kVisibleRows * kListItemH + 14,
                  ui::FrameStyle::Inset);
    ui::drawMenuScrolled(menu_, scroll_, kVisibleRows, kListX, kListY, kListItemH,
                         style::kFontMenu, kLabelW, p.text, p.disabled, p.cursor,
                         "debug.list", style::kFontSmall, p.gold);

    if (!message_.empty()) {
        ui::drawBanner(ui::BannerKind::Success, message_, 40, h - 44, w - 80, "debug.message");
    }

    const InputMap& map = context_.input.map();
    const ActiveDevice device = context_.input.activeDevice();
    ui::drawFooterHints(
        {{input::primaryLabel(map, InputAction::MoveLeft, device) + "/" +
              input::primaryLabel(map, InputAction::MoveRight, device),
          "Adjust"},
         {input::primaryLabel(map, InputAction::Confirm, device), "Do"},
         {input::primaryLabel(map, InputAction::Cancel, device), "Back"}},
        w, h, "debug.footer");
}

}  // namespace cd

#endif  // CRYSTAL_DEBUG_OVERLAY
