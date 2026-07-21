#include "states/CastleState.hpp"

#include <memory>
#include <string>

#include "audio/AudioManager.hpp"
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
#include "states/TutorialPromptState.hpp"
#include "tutorial/Tutorial.hpp"
#include "ui/UiDraw.hpp"

namespace cd {

namespace {
constexpr int kBossRush = 0;
constexpr int kEndless = 1;
constexpr int kKing = 2;
constexpr int kInn = 3;
constexpr int kSave = 4;
constexpr int kLeave = 5;
}  // namespace

CastleState::CastleState(StateStack& stack, AppContext& context)
    : GameState(stack), context_(context) {
    menu_.setItems({{"Boss Rush", true},
                    {"Endless Rush", true},
                    {"Challenge the King", true},
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
    ClearBackground(Color{12, 10, 20, 255});

    // Title.
    ui::drawTextCentered("The Castle", w / 2, 18, 20, Color{220, 190, 110, 255});
    ui::drawTextCentered("above the seven towns", w / 2, 42, 10, Color{150, 140, 170, 255});

    // Challenge menu (left).
    const int menuX = 40;
    const int menuY = 78;
    ui::drawMenu(menu_, menuX, menuY, 18, 12, RAYWHITE, Color{90, 90, 110, 255},
                 Color{240, 220, 120, 255});

    // Records panel (right).
    const CastleRecords& rec = context_.party.castleRecords;
    const int recX = w - 210;
    const int recY = 70;
    ui::drawFramedPanel(context_.resources, recX, recY, 190, 120, Color{22, 18, 30, 235},
                        Color{120, 110, 150, 255});
    ui::drawText("Castle Records", recX + 12, recY + 10, 10, Color{220, 200, 130, 255});
    const Color label{200, 200, 215, 255};
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
                            Color{235, 210, 130, 255}, "castle.records.title", 2);
    }

    // The Jester spot is reserved for M41.
    ui::drawTextCentered("The throne hall is quiet. No jester has yet come to court.", w / 2,
                         h - 28, 8, Color{130, 120, 150, 255});
    ui::drawTextCentered(input::prompt(context_.input.map(), InputAction::Cancel,
                                       context_.input.activeDevice(), "Leave")
                             .c_str(),
                         w / 2, h - 14, 10, Color{200, 200, 160, 255});
}

}  // namespace cd
