#include "states/CastleChallengeState.hpp"

#include <memory>
#include <string>
#include <utility>

#include "audio/AudioManager.hpp"
#include "content/ContentDatabase.hpp"
#include "core/AppContext.hpp"
#include "core/FadeController.hpp"
#include "dungeon/DungeonModel.hpp"
#include "game/Party.hpp"
#include "game/Profile.hpp"
#include "input/Input.hpp"
#include "input/PromptLabels.hpp"
#include "raylib.h"
#include "game/Achievements.hpp"
#include "states/AchievementToast.hpp"
#include "states/BattleState.hpp"
#include "states/StateStack.hpp"
#include "states/TutorialPromptState.hpp"
#include "tutorial/Tutorial.hpp"
#include "ui/UiDraw.hpp"
#include "ui/UiStyle.hpp"

namespace cd {

namespace {

const char* challengeName(CastleChallenge kind) {
    switch (kind) {
        case CastleChallenge::BossRush: return "Boss Rush";
        case CastleChallenge::Endless: return "Endless Rush";
        case CastleChallenge::King: return "The Hollow King";
    }
    return "";
}

// The team for fight/wave `wave`; empty when the challenge is complete (the Boss
// Rush past its last boss, or the King past its single fight).
dungeon::EnemyTeam teamFor(CastleChallenge kind, int wave, const content::ContentDatabase& db) {
    switch (kind) {
        case CastleChallenge::BossRush: return bossRushTeam(db, wave);
        case CastleChallenge::Endless: return endlessWaveTeam(db, wave);
        case CastleChallenge::King: return wave == 0 ? kingTeam(db) : dungeon::EnemyTeam{};
    }
    return {};
}

}  // namespace

CastleChallengeState::CastleChallengeState(StateStack& stack, AppContext& context,
                                           CastleChallenge kind)
    : GameState(stack), context_(context), kind_(kind) {}

void CastleChallengeState::onEnter() {
    if (done_) {
        return;  // a capture preset (captureKingReward) set the overlay; no fight
    }
    // Start the first fight, THEN push the tutorial prompt so it sits on top and is
    // read before the battle begins (dismissing it reveals the fight beneath).
    startNextFight();
    maybeTutorialPrompt(stack(), context_, tutorial::kFirstChallenge);
}

void CastleChallengeState::startNextFight() {
    const dungeon::EnemyTeam team = teamFor(kind_, wave_, context_.content);
    if (team.bossId.empty() && team.enemyIds.empty()) {
        finish(true);  // no more fights -> the gauntlet/King was cleared
        return;
    }
    battle::Battle b = battle::buildBattle(context_.party, team, context_.content);
    const MusicTrack music =
        kind_ == CastleChallenge::King ? MusicTrack::KingBattle : MusicTrack::None;
    // M43: the last argument marks this as a castle fight, so a defeat message
    // never claims the dungeon's gold penalty.
    stack().pushState(std::make_unique<BattleState>(stack(), context_, std::move(b), &result_,
                                                    music, nullptr, true));
}

void CastleChallengeState::onResume() {
    if (done_) {
        return;  // the overlay is up; input pops us
    }
    context_.fade.start();
    totalRounds_ += result_.rounds;
    if (result_.outcome == battle::Outcome::Victory) {
        ++wavesWon_;
        ++wave_;
        startNextFight();  // next fight, or finish(true) when the rush/King ends
    } else {
        finish(false);  // a defeat/escape ends the run (endless still scores its waves)
    }
}

void CastleChallengeState::finish(bool cleared) {
    done_ = true;
    context_.audio.setMusic(MusicTrack::Castle);
    CastleRecords& rec = context_.party.castleRecords;
    std::string msg;
    // M43: a lost challenge costs nothing but the attempt. The party is patched
    // up at the castle gates — no gold penalty, no forfeited run — and the text
    // below says so, so the screen and the battle's defeat line agree.
    if (!cleared) {
        healFull(context_.party);
    }
    switch (kind_) {
        case CastleChallenge::BossRush:
            if (cleared) {
                const bool first = !rec.bossRushCleared();
                if (bossRushImproved(rec, totalRounds_)) {
                    rec.bossRushBestTurns = totalRounds_;
                }
                msg = "Boss Rush cleared in " + std::to_string(totalRounds_) + " turns!";
                if (first) {
                    context_.party.gold += kBossRushRewardGold;
                    context_.party.legendaryTokens += kBossRushRewardTokens;
                    msg += " First clear reward: +" + std::to_string(kBossRushRewardGold) +
                           " gold and +" + std::to_string(kBossRushRewardTokens) +
                           " legendary tokens.";
                }
            } else {
                // M43: the roster is derived, never a literal — adding a boss
                // must not leave this line quietly lying.
                msg = "The gauntlet fells you after " + std::to_string(wavesWon_) + " of " +
                      std::to_string(bossRushOrder(context_.content).size()) +
                      " bosses. Rest and return.";
            }
            break;
        case CastleChallenge::Endless: {
            const bool first = rec.endlessBestWave == 0 && wavesWon_ > 0;
            if (endlessImproved(rec, wavesWon_)) {
                rec.endlessBestWave = wavesWon_;
            }
            msg = "Endless Rush: you reached wave " + std::to_string(wavesWon_) + ".";
            if (first) {
                context_.party.gold += kEndlessRewardGold;
                context_.party.legendaryTokens += kEndlessRewardTokens;
                msg += " First run reward: +" + std::to_string(kEndlessRewardGold) +
                       " gold and +" + std::to_string(kEndlessRewardTokens) + " legendary tokens.";
            }
            break;
        }
        case CastleChallenge::King:
            if (cleared) {
                const bool first = !rec.kingDefeated;
                if (kingImproved(rec, totalRounds_)) {
                    rec.kingBestTurns = totalRounds_;
                }
                rec.kingDefeated = true;
                // M45: the kill belongs to the PLAYER, not this save — it unlocks
                // the three reward classes for every future New Game.
                const bool newlyUnlocked = context_.profile.recordKingDefeated();
                msg = "The Hollow King falls in " + std::to_string(totalRounds_) + " turns!";
                if (newlyUnlocked) {
                    msg += " The Dragon, the Jester and the Goose will answer your call from "
                           "now on - in any new party.";
                }
                if (first) {
                    rec.kingTitle = kKingTitle;
                    context_.party.inventory.add(kKingLegendaryId, 1);
                    context_.party.gold += kKingRewardGold;
                    context_.party.legendaryTokens += kKingRewardTokens;
                    msg += " You take the title \"" + std::string(kKingTitle) +
                           "\" and win the Sovereign's Regalia, +" +
                           std::to_string(kKingRewardGold) + " gold, +" +
                           std::to_string(kKingRewardTokens) + " tokens.";
                }
            } else {
                msg = "The King proves too mighty. Return stronger.";
            }
            break;
    }
    if (!cleared) {
        msg += " You are patched up at the gates; nothing is lost.";
    }
    resultText_ = msg;
    // M42: a challenge win may unlock castle achievements; toast them above the
    // result overlay this state renders.
    pushAchievementToasts(stack(), context_, AchvContext{});
}

#ifdef CRYSTAL_CAPTURE
void CastleChallengeState::captureKingReward() {
    kind_ = CastleChallenge::King;
    totalRounds_ = 18;
    done_ = true;
    resultText_ = std::string("The Hollow King falls in ") + std::to_string(totalRounds_) +
                  " turns! You take the title \"" + kKingTitle +
                  "\" and win the Sovereign's Regalia, +" + std::to_string(kKingRewardGold) +
                  " gold, +" + std::to_string(kKingRewardTokens) + " tokens.";
}
#endif

void CastleChallengeState::handleInput(const Input& input) {
    if (done_ && (input.pressed(InputAction::Confirm) || input.pressed(InputAction::Cancel))) {
        stack().popState();  // back to the castle hub
    }
}

void CastleChallengeState::render() {
    const int w = context_.virtualWidth;
    const int h = context_.virtualHeight;
    const ui::style::Palette& p = ui::style::palette();
    ClearBackground(p.canvas);
    if (!done_) {
        return;  // transient between fights (a BattleState is normally on top)
    }
    const int boxW = 330;
    const int boxH = 150;
    const int boxX = w / 2 - boxW / 2;
    const int boxY = h / 2 - boxH / 2;
    ui::drawFrame(boxX, boxY, boxW, boxH, ui::FrameStyle::Reward);
    ui::drawTextCentered(challengeName(kind_), w / 2, boxY + 12, 16, p.gold);
    ui::drawDivider(boxX + 14, boxY + 34, boxW - 28);
    ui::drawTextWrapped(resultText_, boxX + 16, boxY + 42, boxW - 32, 10, p.text,
                        "castle.challenge.result", 6);
    ui::drawTextCentered(input::prompt(context_.input.map(), InputAction::Confirm,
                                       context_.input.activeDevice(), "Return to the Castle")
                             .c_str(),
                         w / 2, boxY + boxH - 16, 10, p.gold);
}

}  // namespace cd
