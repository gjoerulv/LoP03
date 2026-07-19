#include "states/GuildState.hpp"

#include <algorithm>
#include <memory>
#include <utility>

#include "audio/AudioManager.hpp"
#include "content/ContentDatabase.hpp"
#include "content/LoadReport.hpp"
#include "core/AppContext.hpp"
#include "dungeon/DungeonGenerator.hpp"
#include "input/Input.hpp"
#include "input/PromptLabels.hpp"
#include "raylib.h"
#include "save/SaveSystem.hpp"
#include "states/DungeonState.hpp"
#include "states/StateStack.hpp"
#include "ui/UiDraw.hpp"

namespace cd {

namespace {
constexpr int kEnter = 0;
constexpr int kTheme = 1;
constexpr int kDepth = 2;
constexpr int kReroll = 3;
constexpr int kBack = 4;
constexpr int kMaxDepth = 20;

std::uint64_t randomSeed() {
    const std::uint64_t hi = static_cast<std::uint64_t>(GetRandomValue(1, 2000000000));
    const std::uint64_t lo = static_cast<std::uint64_t>(GetRandomValue(1, 2000000000));
    return (hi << 21) ^ lo ^ 0xD1B54A32D192ED03ull;
}
}  // namespace

GuildState::GuildState(StateStack& stack, AppContext& context)
    : GameState(stack), context_(context) {
    menu_.setItems({{"Enter Dungeon", true},
                    {"Theme", true},
                    {"Depth", true},
                    {"New Seed", true},
                    {"Back", true}});
}

void GuildState::onEnter() {
    context_.audio.setMusic(MusicTrack::Guild);  // preparation scene (M21)
    themeIds_.clear();
    for (const auto& [id, def] : context_.content.themes()) {
        (void)def;
        themeIds_.push_back(id);
    }
    std::sort(themeIds_.begin(), themeIds_.end());
    if (themeIndex_ >= static_cast<int>(themeIds_.size())) {
        themeIndex_ = 0;
    }
    seed_ = randomSeed();
}

void GuildState::onResume() {
    // Back from the dungeon (or a submenu): restore the preparation scene.
    context_.audio.setMusic(MusicTrack::Guild);
    context_.audio.setAmbience(AmbienceTrack::None);
}

std::string GuildState::currentThemeName() const {
    if (themeIds_.empty()) {
        return "Dungeon";
    }
    const std::string& id = themeIds_[static_cast<std::size_t>(themeIndex_)];
    if (const content::DungeonThemeDef* theme = context_.content.findTheme(id)) {
        return theme->name;
    }
    return id;
}

void GuildState::enterDungeon() {
    content::LoadReport report;
    context_.saves.autosave(context_.party, report);

    const std::string themeId = themeIds_.empty() ? "" : themeIds_[static_cast<std::size_t>(themeIndex_)];
    dungeon::Dungeon dungeon = dungeon::generate(seed_, depth_, context_.content, themeId);
    stack().popState();  // leave the Guild
    stack().pushState(std::make_unique<DungeonState>(stack(), context_, std::move(dungeon)));
}

void GuildState::handleInput(const Input& input) {
    if (input.navPressed(InputAction::MoveUp)) {
        menu_.moveUp();
    }
    if (input.navPressed(InputAction::MoveDown)) {
        menu_.moveDown();
    }

    const int dir = (input.navPressed(InputAction::MoveRight) ? 1 : 0) -
                    (input.navPressed(InputAction::MoveLeft) ? 1 : 0);
    if (dir != 0) {
        if (menu_.cursor() == kTheme && !themeIds_.empty()) {
            const int n = static_cast<int>(themeIds_.size());
            themeIndex_ = ((themeIndex_ + dir) % n + n) % n;
        } else if (menu_.cursor() == kDepth) {
            depth_ = std::clamp(depth_ + dir, 1, kMaxDepth);
        }
    }

    if (input.pressed(InputAction::Cancel)) {
        stack().popState();
        return;
    }
    if (input.pressed(InputAction::Confirm)) {
        switch (menu_.cursor()) {
            case kEnter: enterDungeon(); break;
            case kReroll: seed_ = randomSeed(); break;
            case kBack: stack().popState(); break;
            default: break;  // Theme/Depth are adjusted with Left/Right
        }
    }
}

void GuildState::render() {
    const int w = context_.virtualWidth;
    const int h = context_.virtualHeight;
    ClearBackground(Color{16, 16, 26, 255});

    ui::drawTextCentered("Guild", w / 2, 18, 18, RAYWHITE);
    ui::drawTextCentered("Choose a dungeon and enter.", w / 2, 40, 10, Color{180, 180, 200, 255});

    DrawText(TextFormat("Theme:  < %s >", currentThemeName().c_str()), 70, 78, 11,
             Color{200, 200, 230, 255});
    DrawText(TextFormat("Depth:  < %d >", depth_), 70, 96, 11, Color{200, 200, 230, 255});
    DrawText(TextFormat("Seed:   %llu", static_cast<unsigned long long>(seed_)), 70, 114, 10,
             Color{220, 220, 160, 255});

    ui::drawMenu(menu_, 70, 140, 16, 11, RAYWHITE, Color{90, 90, 110, 255},
                 Color{240, 220, 120, 255});

    const InputMap& map = context_.input.map();
    const ActiveDevice device = context_.input.activeDevice();
    const std::string footer =
        "[" + input::primaryLabel(map, InputAction::MoveLeft, device) + "/" +
        input::primaryLabel(map, InputAction::MoveRight, device) +
        "] Adjust Theme & Depth. Entering autosaves. " +
        input::prompt(map, InputAction::Cancel, device, "Back");
    ui::drawTextCentered(footer.c_str(), w / 2, h - 14, 9, Color{150, 150, 170, 255});
}

}  // namespace cd
