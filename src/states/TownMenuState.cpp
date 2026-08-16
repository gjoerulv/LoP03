#include "states/TownMenuState.hpp"

#include <memory>

#include "core/AppContext.hpp"
#include "input/Input.hpp"
#include "raylib.h"
#include "states/AchievementsState.hpp"
#include "states/BestiaryState.hpp"
#include "states/EquipShopState.hpp"   // M90: partyMode equip flow
#include "states/InventoryState.hpp"   // M90: the Items screen
#include "states/MapsState.hpp"   // M65
#include "states/PartyState.hpp"  // M64
#include "states/QuitFlow.hpp"
#include "states/QuitPrompt.hpp"
#include "states/SettingsState.hpp"
#include "states/StateStack.hpp"
#include "ui/UiDraw.hpp"
#include "ui/UiStyle.hpp"
#ifdef CRYSTAL_DEBUG_OVERLAY
#include "states/DebugMenuState.hpp"
#endif

namespace cd {

namespace {
constexpr int kResume = 0;
constexpr int kParty = 1;    // M64
constexpr int kEquip = 2;    // M90: equip anywhere (the shop's flow, no shop)
constexpr int kItems = 3;    // M90: inspect the bag, use consumables
constexpr int kMaps = 4;     // M65
constexpr int kBestiary = 5;
constexpr int kAchievements = 6;
constexpr int kSettings = 7;
constexpr int kQuit = 8;
#ifdef CRYSTAL_DEBUG_OVERLAY
constexpr int kDebug = kQuit + 1;  // appended after Quit in debug builds only
#endif
}  // namespace

TownMenuState::TownMenuState(StateStack& stack, AppContext& context)
    : GameState(stack), context_(context) {
    menu_.setItems({{"Resume", true},
                    {"Party", true},
                    {"Equip Party", true},  // M90
                    {"Items", true},        // M90
                    {"Maps", true},
                    {"Bestiary", true},
                    {"Achievements", true},
                    {"Settings", true},
                    {"Quit", true}});
#ifdef CRYSTAL_DEBUG_OVERLAY
    menu_.addItem("Debug", true);
#endif
}

void TownMenuState::handleInput(const Input& input) {
    if (input.navPressed(InputAction::MoveUp)) {
        menu_.moveUp();
    }
    if (input.navPressed(InputAction::MoveDown)) {
        menu_.moveDown();
    }
    // M47: the key that opened the pause menu closes it again (Tab / Start),
    // alongside Cancel. One action, both devices, no new hint text.
    if (input.pressed(InputAction::Cancel) || input.pressed(InputAction::Menu)) {
        stack().popState();
        return;
    }
    if (input.pressed(InputAction::Confirm)) {
        switch (menu_.cursor()) {
            case kResume:
                stack().popState();
                break;
            case kParty:
                stack().pushState(std::make_unique<PartyState>(stack(), context_));
                break;
            case kEquip:  // M90
                stack().pushState(
                    std::make_unique<EquipShopState>(stack(), context_, /*partyMode=*/true));
                break;
            case kItems:  // M90
                stack().pushState(std::make_unique<InventoryState>(stack(), context_));
                break;
            case kMaps:
                stack().pushState(std::make_unique<MapsState>(stack(), context_));
                break;
            case kBestiary:
                stack().pushState(std::make_unique<BestiaryState>(stack(), context_));
                break;
            case kAchievements:
                stack().pushState(std::make_unique<AchievementsState>(stack(), context_));
                break;
            case kSettings:
                stack().pushState(std::make_unique<SettingsState>(stack(), context_));
                break;
            case kQuit:
                // Destructive-action confirmation (M22): progress since the last
                // save is lost, so quitting asks the question outright rather than
                // arming a second press on the same entry. M47 turned the single
                // answer into three (title / desktop / stay).
                pushQuitPrompt(stack(), context_, quit::kTownBody);
                break;
#ifdef CRYSTAL_DEBUG_OVERLAY
            case kDebug:
                stack().pushState(std::make_unique<DebugMenuState>(stack(), context_,
                                                                   /*inDungeon=*/false));
                break;
#endif
            default:
                break;
        }
    }
}

void TownMenuState::render() {
    const int w = context_.virtualWidth;
    const int h = context_.virtualHeight;

    const ui::style::Palette& p = ui::style::palette();
    ui::drawModalDim(w, h);

    const int boxW = 220;
    // M90: nine entries (+Equip Party, +Items) — the rows tightened from 20px
    // to 18px so the box (plus its debug row and the floating title plaque)
    // still clears the 240px frame with room over the plaque.
    int boxH = 190;
#ifdef CRYSTAL_DEBUG_OVERLAY
    boxH += 18;  // M53: the extra "Debug" row
#endif
    const int boxX = w / 2 - boxW / 2;
    const int boxY = h / 2 - boxH / 2;
    ui::drawFrame(boxX, boxY, boxW, boxH, ui::FrameStyle::Raised);
    ui::drawTitlePlaque("Paused", w / 2, boxY - 10, 12);
    ui::drawMenu(menu_, boxX + 44, boxY + 26, 18, 12, p.text, p.disabled, p.cursor);
}

}  // namespace cd
