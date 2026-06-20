#include "states/GuildState.hpp"

#include <memory>
#include <utility>

#include "content/LoadReport.hpp"
#include "core/AppContext.hpp"
#include "dungeon/DungeonGenerator.hpp"
#include "input/Input.hpp"
#include "raylib.h"
#include "save/SaveSystem.hpp"
#include "states/DungeonState.hpp"
#include "states/StateStack.hpp"
#include "ui/UiDraw.hpp"

namespace cd {

namespace {
constexpr int kEnter = 0;
constexpr int kReroll = 1;
constexpr int kBack = 2;

std::uint64_t randomSeed() {
    const std::uint64_t hi = static_cast<std::uint64_t>(GetRandomValue(1, 2000000000));
    const std::uint64_t lo = static_cast<std::uint64_t>(GetRandomValue(1, 2000000000));
    return (hi << 21) ^ lo ^ 0xD1B54A32D192ED03ull;
}
}  // namespace

GuildState::GuildState(StateStack& stack, AppContext& context)
    : GameState(stack), context_(context) {
    menu_.setItems({{"Enter Dungeon", true}, {"New Seed", true}, {"Back", true}});
}

void GuildState::onEnter() { seed_ = randomSeed(); }

void GuildState::enterDungeon() {
    // Autosave on dungeon entry (M3 capability, M4 trigger).
    content::LoadReport report;
    context_.saves.autosave(context_.party, report);

    dungeon::Dungeon dungeon = dungeon::generate(seed_, depth_, context_.content);
    stack().popState();  // leave the Guild
    stack().pushState(std::make_unique<DungeonState>(stack(), context_, std::move(dungeon)));
}

void GuildState::handleInput(const Input& input) {
    if (input.pressed(InputAction::MoveUp)) {
        menu_.moveUp();
    }
    if (input.pressed(InputAction::MoveDown)) {
        menu_.moveDown();
    }
    if (input.pressed(InputAction::Cancel)) {
        stack().popState();
        return;
    }
    if (input.pressed(InputAction::Confirm)) {
        switch (menu_.cursor()) {
            case kEnter:
                enterDungeon();
                break;
            case kReroll:
                seed_ = randomSeed();
                break;
            case kBack:
                stack().popState();
                break;
            default:
                break;
        }
    }
}

void GuildState::render() {
    const int w = context_.virtualWidth;
    const int h = context_.virtualHeight;
    ClearBackground(Color{16, 16, 26, 255});

    ui::drawTextCentered("Guild", w / 2, 24, 18, RAYWHITE);
    ui::drawTextCentered("Choose a dungeon and enter.", w / 2, 50, 10, Color{180, 180, 200, 255});

    DrawText(TextFormat("Seed:  %llu", static_cast<unsigned long long>(seed_)), 70, 90, 12,
             Color{220, 220, 160, 255});
    DrawText(TextFormat("Depth: %d   Theme: Ruined Keep", depth_), 70, 110, 10,
             Color{180, 180, 200, 255});

    ui::drawMenu(menu_, 70, 140, 20, 12, RAYWHITE, Color{90, 90, 110, 255},
                 Color{240, 220, 120, 255});

    ui::drawTextCentered("Entering autosaves your party. Cancel: Back", w / 2, h - 16, 10,
                         Color{150, 150, 170, 255});
}

}  // namespace cd
