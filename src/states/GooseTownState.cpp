#include "states/GooseTownState.hpp"

#include <memory>
#include <string>

#include "audio/AudioManager.hpp"
#include "content/ContentDatabase.hpp"
#include "core/AppContext.hpp"
#include "core/FadeController.hpp"
#include "game/Castle.hpp"
#include "game/Party.hpp"
#include "input/Input.hpp"
#include "input/PromptLabels.hpp"
#include "raylib.h"
#include "states/CastleChallengeState.hpp"
#include "states/InnState.hpp"
#include "states/SlotMenuState.hpp"
#include "states/StateStack.hpp"
#include "states/StoryDialogState.hpp"
#include "ui/UiDraw.hpp"
#include "ui/UiStyle.hpp"

namespace cd {

namespace {
constexpr int kDuck = 0;
constexpr int kJester = 1;
constexpr int kInn = 2;
constexpr int kSave = 3;
constexpr int kLeave = 4;
}  // namespace

GooseTownState::GooseTownState(StateStack& stack, AppContext& context)
    : GameState(stack), context_(context) {
    menu_.setItems({{"Fight the Deadly Duck", true},
                    {"Hear the Goofy Jester", true},
                    {"Rest at the Inn", true},
                    {"Save", true},
                    {"Leave to Town", true}});
}

void GooseTownState::applyMusic() {
    // The castle theme fits the "place above the ladder" register; the pond
    // gets its own ambience-free stillness, like the throne room.
    context_.audio.setMusic(MusicTrack::Castle);
    context_.audio.setAmbience(AmbienceTrack::None);
}

void GooseTownState::onEnter() {
    context_.fade.start();
    applyMusic();
}

void GooseTownState::onResume() {
    context_.fade.start();
    applyMusic();
}

void GooseTownState::handleInput(const Input& input) {
    if (input.navPressed(InputAction::MoveUp)) {
        menu_.moveUp();
    }
    if (input.navPressed(InputAction::MoveDown)) {
        menu_.moveDown();
    }
    if (input.pressed(InputAction::Cancel)) {
        context_.audio.play(Sfx::Cancel);
        stack().popState();  // back to town 7
        return;
    }
    if (input.pressed(InputAction::Confirm)) {
        context_.audio.play(Sfx::Confirm);
        switch (menu_.cursor()) {
            case kDuck:
                stack().pushState(std::make_unique<CastleChallengeState>(
                    stack(), context_, CastleChallenge::DuckGauntlet));
                break;
            case kJester:
                if (const content::StoryBeat* beat = context_.content.findStoryBeat(kGooseTown)) {
                    stack().pushState(std::make_unique<StoryDialogState>(
                        stack(), context_, beat->speaker, beat->title, beat->body));
                }
                break;
            case kInn:
                stack().pushState(std::make_unique<InnState>(stack(), context_));
                break;
            case kSave:
                stack().pushState(
                    std::make_unique<SlotMenuState>(stack(), context_, SlotMenuMode::Save));
                break;
            case kLeave:
                stack().popState();
                break;
            default:
                break;
        }
    }
}

void GooseTownState::render() {
    const int w = context_.virtualWidth;
    const int h = context_.virtualHeight;
    const ui::style::Palette& p = ui::style::palette();
    ClearBackground(p.canvas);

    ui::drawTitlePlaque("Goose Town", w / 2, 10, 20);
    ui::drawTextCentered("the pond beyond the castle", w / 2, 46, 10, p.textDim);

    // Challenge menu (left) — the castle hub's layout, one challenge shorter.
    const int menuX = 44;
    const int menuY = 84;
    const int menuRows = static_cast<int>(menu_.size());
    ui::drawFrame(24, menuY - 10, 188, menuRows * 18 + 16, ui::FrameStyle::Standard);
    ui::drawMenu(menu_, menuX, menuY, 18, 12, p.text, p.disabled, p.cursor);

    // Record panel (right).
    const CastleRecords& rec = context_.party.castleRecords;
    const int recX = w - 210;
    const int recY = 76;
    ui::drawFrame(recX, recY, 190, 76, ui::FrameStyle::Reward);
    ui::drawSectionHeader("Pond Records", recX + 12, recY + 10, 166);
    const std::string duck =
        rec.duckDefeated() ? std::to_string(rec.duckBestTurns) + " turns" : std::string("-");
    ui::drawText("The Duck:  " + duck, recX + 12, recY + 30, 10, p.textDim);
    if (rec.duckDefeated()) {
        ui::drawText("The pond lies quiet.", recX + 12, recY + 48, ui::style::kFontSmall, p.gold);
    } else {
        ui::drawText("Something huge dives below.", recX + 12, recY + 48, ui::style::kFontSmall, p.textHint);
    }

    ui::drawTextCentered("Geese line the shore. They are all staring at you.", w / 2, h - 28, ui::style::kFontSmall,
                         ui::lighten(p.success, 24));
    ui::drawFooterHints({{input::primaryLabel(context_.input.map(), InputAction::Cancel,
                                              context_.input.activeDevice()),
                          "Leave"}},
                        w, h, "goosetown.footer");
}

}  // namespace cd
