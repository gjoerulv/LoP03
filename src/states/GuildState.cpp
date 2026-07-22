#include "states/GuildState.hpp"

#include <algorithm>
#include <memory>
#include <utility>

#include "audio/AudioManager.hpp"
#include "content/ContentDatabase.hpp"
#include "content/LoadReport.hpp"
#include "core/AppContext.hpp"
#include "dungeon/DungeonGenerator.hpp"
#include "game/Party.hpp"
#include "input/Input.hpp"
#include "input/PromptLabels.hpp"
#include "raylib.h"
#include "save/SaveSystem.hpp"
#include "states/DungeonState.hpp"
#include "states/StateStack.hpp"
#include "states/TutorialPromptState.hpp"
#include "tutorial/Tutorial.hpp"
#include "ui/UiDraw.hpp"
#include "ui/UiStyle.hpp"

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
    rebuild();
}

void GuildState::rebuild() {
    // The menu model still drives navigation; Theme/Depth values render in
    // framed capsules on their own rows (M46 stepper rows), so labels stay
    // plain. The value still sits on the row that changes it (M25 slice 4).
    const int previous = menu_.cursor();
    std::vector<ui::MenuItem> items;
    items.push_back({"Enter Dungeon", true});
    items.push_back({"Theme", true});
    items.push_back({"Depth", true});
    items.push_back({"New Seed", true});
    items.push_back({"Back", true});
    menu_.setItems(std::move(items));
    menu_.setCursor(previous);
}

void GuildState::onEnter() {
    context_.audio.setMusic(MusicTrack::Guild);  // preparation scene (M21)
    maybeTutorialPrompt(stack(), context_, tutorial::kGuildPrepare);
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
    rebuild();  // themeIds_ now populated: refresh the inline Theme label
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
    dungeon::Dungeon dungeon =
        dungeon::generate(seed_, depth_, context_.content, themeId, context_.party.currentTown);
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
            rebuild();  // reflect the new value inline immediately
        } else if (menu_.cursor() == kDepth) {
            depth_ = std::clamp(depth_ + dir, 1, kMaxDepth);
            rebuild();
        }
    }

    // M33: the first time the configured run would incur a stakes penalty, teach
    // it (fired here, never from render). maybeTutorialPrompt shows it at most once.
    if (stakesPenaltyPct(context_.party.stakes, context_.party.currentTown, depth_) > 0) {
        maybeTutorialPrompt(stack(), context_, tutorial::kFirstPenalty);
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
    namespace style = ui::style;
    const style::Palette& p = style::palette();
    ui::drawSceneBackground(context_.resources, "bg.guild", p.canvas,
                            context_.virtualWidth, context_.virtualHeight, context_.party.currentTown);

    ui::drawHeaderBand("Guild", w, p.crystal);
    ui::drawTextCentered("Choose a dungeon - entering autosaves.", w / 2, 28, style::kFontBody,
                         p.textDim);

    // Control panel: the CTA row on top, then the two stepper rows, then the
    // plain rows. The menu model still owns navigation.
    const int px = 90;
    const int py = 44;
    const int pw = 246;
    const int ph = 112;
    ui::drawFrame(px, py, pw, ph, ui::FrameStyle::Standard);
    const int rowX = px + 22;

    // "Enter Dungeon" — the dominant call to action.
    const int enterY = py + 10;
    const bool enterFocused = menu_.cursor() == kEnter;
    ui::drawFrame(px + 10, enterY - 4, pw - 20, 22,
                  enterFocused ? ui::FrameStyle::Reward : ui::FrameStyle::Raised);
    ui::drawTextCentered("Enter Dungeon", px + pw / 2, enterY, 12,
                         enterFocused ? p.cursor : p.text);
    if (enterFocused) {
        ui::drawChevron(px + 18, enterY + 2, p.cursor, ui::motionPhase());
    }
    ui::drawDivider(px + 10, py + 32, pw - 20);

    // Stepper rows: label, left arrow, framed value capsule, right arrow.
    const auto stepperRow = [&](int index, const char* label, const std::string& value, int y) {
        const bool focused = menu_.cursor() == index;
        if (focused) {
            ui::drawSelectionSlab(rowX - 12, y - 2, pw - 40, 15);
            ui::drawChevron(rowX - 9, y + 1, p.cursor, ui::motionPhase());
        }
        ui::drawText(label, rowX, y, style::kFontMenu, focused ? p.cursor : p.text);
        const int capW = 104;
        const int capX = px + pw - 22 - capW;
        ui::drawStepArrow(capX - 11, y + 2, -1, focused);
        ui::drawStepArrow(capX + capW + 6, y + 2, +1, focused);
        DrawRectangle(capX, y - 1, capW, 13, p.ink);
        DrawRectangle(capX + 1, y, capW - 2, 11, p.chipFill);
        if (focused) {
            DrawRectangleLines(capX, y - 1, capW, 13, p.rowBorder);
        }
        const int vw = ui::measureText(value, style::kFontBody);
        ui::drawTextFitted(value, capX + (capW - vw) / 2, y + 1, capW - 6, style::kFontBody,
                           focused ? p.cursor : p.text, "guild.capsule");
    };
    stepperRow(kTheme, "Theme", currentThemeName(), py + 40);
    stepperRow(kDepth, "Depth", std::to_string(depth_), py + 58);

    // Plain rows.
    const auto plainRow = [&](int index, const char* label, int y) {
        const bool focused = menu_.cursor() == index;
        if (focused) {
            ui::drawSelectionSlab(rowX - 12, y - 2, ui::measureText(label, style::kFontMenu) + 24,
                                  15);
            ui::drawChevron(rowX - 9, y + 1, p.cursor, ui::motionPhase());
        }
        ui::drawText(label, rowX, y, style::kFontMenu, focused ? p.cursor : p.text);
    };
    plainRow(kReroll, "New Seed", py + 78);
    plainRow(kBack, "Back", py + 94);

    // Seed: a subdued information chip (non-adjustable readout).
    ui::drawChip("Seed " + std::to_string(static_cast<unsigned long long>(seed_)), px,
                 py + ph + 8, p.borderMid);

    // M33 stakes forewarning: the penalty this run (currentTown + chosen depth)
    // will incur, updating live as Depth changes; matches what completeDungeon
    // applies. Honest and shown before entering (M19).
    const int pen = stakesPenaltyPct(context_.party.stakes, context_.party.currentTown, depth_);
    const int bannerY = py + ph + 26;
    if (pen > 0) {
        ui::drawBanner(ui::BannerKind::Danger,
                       TextFormat("Stakes penalty: -%d%% - raise town or depth to clear it.", pen),
                       70, bannerY, w - 140, "guild.stakes");
    } else {
        const char* okText = "No stakes penalty - this run raises the stakes.";
        const int tw = ui::measureText(okText, style::kFontBody);
        const int tx = w / 2 - tw / 2;
        DrawRectangle(tx - 10, bannerY + 4, 2, 2, p.success);  // stepped diamond, shape
        DrawRectangle(tx - 12, bannerY + 6, 6, 2, p.success);  // + color double signal
        DrawRectangle(tx - 10, bannerY + 8, 2, 2, p.success);
        ui::drawText(okText, tx, bannerY + 2, style::kFontBody, p.success);
    }

    const InputMap& map = context_.input.map();
    const ActiveDevice device = context_.input.activeDevice();
    ui::drawFooterHints(
        {{input::primaryLabel(map, InputAction::MoveLeft, device) + "/" +
              input::primaryLabel(map, InputAction::MoveRight, device),
          "Adjust"},
         {input::primaryLabel(map, InputAction::Confirm, device), "Select"},
         {input::primaryLabel(map, InputAction::Cancel, device), "Back"}},
        w, h, "guild.footer");
}

}  // namespace cd
