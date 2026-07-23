#include "states/CastleState.hpp"

#include <memory>
#include <string>

#include "audio/AudioManager.hpp"
#include "content/ContentDatabase.hpp"
#include "core/AppContext.hpp"
#include "core/FadeController.hpp"
#include "game/Castle.hpp"
#include "game/Party.hpp"
#include "game/Story.hpp"
#include "input/Input.hpp"
#include "input/PromptLabels.hpp"
#include "raylib.h"
#include "states/CastleChallengeState.hpp"
#include "states/InnState.hpp"
#include "states/SlotMenuState.hpp"
#include "states/StateStack.hpp"
#include "states/StoryDialogState.hpp"
#include "states/TutorialPromptState.hpp"
#include "tutorial/Tutorial.hpp"
#include "ui/UiDraw.hpp"
#include "ui/UiStyle.hpp"

namespace cd {

namespace {
constexpr int kBossRush = 0;
constexpr int kEndless = 1;
constexpr int kKing = 2;
constexpr int kJester = 3;
constexpr int kInn = 4;
constexpr int kSave = 5;
constexpr int kLeave = 6;
}  // namespace

CastleState::CastleState(StateStack& stack, AppContext& context)
    : GameState(stack), context_(context) {
    menu_.setItems({{"Boss Rush", true},
                    {"Endless Rush", true},
                    {"Challenge the King", true},
                    {"Speak with the Jester", true},
                    {"Rest at the Inn", true},
                    {"Save", true},
                    {"Leave to Town", true}});
}

void CastleState::applyMusic() {
    context_.audio.setMusic(MusicTrack::Castle);
    context_.audio.setAmbience(AmbienceTrack::None);
}

void CastleState::onEnter() {
    context_.fade.start();
    applyMusic();
    maybeTutorialPrompt(stack(), context_, tutorial::kFirstCastle);
}

void CastleState::onResume() {
    context_.fade.start();
    applyMusic();
}

void CastleState::handleInput(const Input& input) {
    if (input.navPressed(InputAction::MoveUp)) {
        menu_.moveUp();
    }
    if (input.navPressed(InputAction::MoveDown)) {
        menu_.moveDown();
    }
    if (input.pressed(InputAction::Cancel)) {
        context_.audio.play(Sfx::Cancel);
        stack().popState();  // leave the castle -> town 7
        return;
    }
    if (input.pressed(InputAction::Confirm)) {
        context_.audio.play(Sfx::Confirm);
        switch (menu_.cursor()) {
            case kBossRush:
                stack().pushState(std::make_unique<CastleChallengeState>(
                    stack(), context_, CastleChallenge::BossRush));
                break;
            case kEndless:
                stack().pushState(std::make_unique<CastleChallengeState>(
                    stack(), context_, CastleChallenge::Endless));
                break;
            case kKing:
                stack().pushState(std::make_unique<CastleChallengeState>(stack(), context_,
                                                                         CastleChallenge::King));
                break;
            case kJester:
                if (storyAllHeard(context_.party.storyMet)) {
                    if (const content::StoryBeat* beat =
                            context_.content.findStoryBeat(kCastleTown)) {
                        stack().pushState(std::make_unique<StoryDialogState>(
                            stack(), context_, beat->speaker, beat->title, beat->body));
                    }
                } else {
                    // Teaser until the whole ballad (all 7 town verses) is heard.
                    stack().pushState(std::make_unique<StoryDialogState>(
                        stack(), context_, "The Jester", "The Jester",
                        "The Jester waggles a finger. \"Ah-ah - no spoilers! Go hear the whole "
                        "ballad from that wandering storyteller: one verse in each of the seven "
                        "towns. Come back when you know the tale, and I'll tell you how it REALLY "
                        "ends.\""));
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

void CastleState::render() {
    const int w = context_.virtualWidth;
    const int h = context_.virtualHeight;
    const ui::style::Palette& p = ui::style::palette();
    ClearBackground(p.canvas);

    // Title plaque with a violet royal accent.
    ui::drawTitlePlaque("The Castle", w / 2, 10, 20);
    ui::drawTextCentered("above the seven towns", w / 2, 46, 10, p.textDim);

    // Challenge menu (left).
    const int menuX = 44;
    const int menuY = 78;
    const int menuRows = static_cast<int>(menu_.size());
    ui::drawFrame(24, menuY - 10, 168, menuRows * 18 + 16, ui::FrameStyle::Standard);
    ui::drawMenu(menu_, menuX, menuY, 18, 12, p.text, p.disabled, p.cursor);

    // Records panel (right).
    const CastleRecords& rec = context_.party.castleRecords;
    const int recX = w - 210;
    const int recY = 70;
    ui::drawFrame(recX, recY, 190, 120, ui::FrameStyle::Reward);
    ui::drawSectionHeader("Castle Records", recX + 12, recY + 10, 166);
    const Color label = p.textDim;
    const std::string rush = rec.bossRushCleared()
                                 ? std::to_string(rec.bossRushBestTurns) + " turns"
                                 : std::string("-");
    const std::string endless =
        rec.endlessBestWave > 0 ? std::string("wave ") + std::to_string(rec.endlessBestWave) : "-";
    const std::string king =
        rec.kingDefeated ? std::to_string(rec.kingBestTurns) + " turns" : "-";
    ui::drawText("Boss Rush:  " + rush, recX + 12, recY + 30, 10, label);
    ui::drawText("Endless:    " + endless, recX + 12, recY + 46, 10, label);
    ui::drawText("The King:   " + king, recX + 12, recY + 62, 10, label);
    if (!rec.kingTitle.empty()) {
        ui::drawTextWrapped("Title: " + rec.kingTitle, recX + 12, recY + 82, 166, 10,
                            p.gold, "castle.records.title", 2);
    }

    // The Jester lounges by the throne (M41).
    ui::drawTextCentered("A jester lounges by the throne, waiting to share the tale's end.", w / 2,
                         h - 28, 8, ui::lighten(p.magic, 32));
    ui::drawFooterHints({{input::primaryLabel(context_.input.map(), InputAction::Cancel,
                                              context_.input.activeDevice()),
                          "Leave"}},
                        w, h, "castle.footer");
}

}  // namespace cd
