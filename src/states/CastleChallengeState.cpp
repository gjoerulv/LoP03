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
#include "game/Guild.hpp"
#include "game/Story.hpp"  // M85: kDragonJesterBeat
#include "states/AchievementToast.hpp"
#include "states/GuildPerkChoiceState.hpp"
#include "states/StoryDialogState.hpp"  // M85: the Pale Jester's introduction
#include "render/BattleBackdrop.hpp"
#include "states/BattleState.hpp"
#include "states/BossIntroState.hpp"
#include "states/CelebrationState.hpp"  // M71
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
        case CastleChallenge::DuckGauntlet: return "The Deadly Duck";  // M61
        case CastleChallenge::GuildBoss: return "The Guild Boss";      // M84 (fallback)
        case CastleChallenge::Dragon: return "The Last Dragon";        // M85
    }
    return "";
}

// The team for fight/wave `wave`; empty when the challenge is complete (the Boss
// Rush past its last boss, or the King past its single fight).
dungeon::EnemyTeam teamFor(CastleChallenge kind, int wave, const content::ContentDatabase& db,
                           int guildTown) {
    switch (kind) {
        case CastleChallenge::BossRush: return bossRushTeam(db, wave);
        case CastleChallenge::Endless: return endlessWaveTeam(db, wave);
        case CastleChallenge::King: return wave == 0 ? kingTeam(db) : dungeon::EnemyTeam{};
        case CastleChallenge::DuckGauntlet:  // M61: the Evil Geese, then the Duck
            if (wave == 0) {
                return gooseWaveTeam(db);
            }
            return wave == 1 ? duckTeam(db) : dungeon::EnemyTeam{};
        case CastleChallenge::GuildBoss:  // M84: the Guild Trial, then the Master
            if (wave == 0) {
                return guildWaveTeam(db, guildTown);
            }
            return wave == 1 ? guildMasterTeam(db, guildTown) : dungeon::EnemyTeam{};
        case CastleChallenge::Dragon:  // M85: three elite vigils, then the Dragon
            if (wave < kDragonWaveCount) {
                return dragonEliteWaveTeam(db, wave);
            }
            return wave == kDragonWaveCount ? dragonTeam(db) : dungeon::EnemyTeam{};
    }
    return {};
}

}  // namespace

CastleChallengeState::CastleChallengeState(StateStack& stack, AppContext& context,
                                           CastleChallenge kind, int guildTown)
    : GameState(stack), context_(context), kind_(kind), guildTown_(guildTown) {}

void CastleChallengeState::onEnter() {
    if (done_) {
        return;  // a capture preset (captureKingReward) set the overlay; no fight
    }
    context_.party.usedSummons.clear();  // M95: each challenge is its own run
    // Start the first fight, THEN push the tutorial prompt so it sits on top and is
    // read before the battle begins (dismissing it reveals the fight beneath).
    startNextFight();
    // M84: the first-challenge tutorial speaks of the castle; the guild
    // gauntlet's stakes are already spelled out on the guild screen.
    if (kind_ != CastleChallenge::GuildBoss) {
        maybeTutorialPrompt(stack(), context_, tutorial::kFirstChallenge);
    }
    // M85: until the Dragon first falls, the Pale Jester's introduction sits
    // on top of the opening fight (the tutorial-prompt pattern — read first,
    // dismissed to reveal the vigil beneath). Content-driven; a missing beat
    // simply skips the tale.
    if (kind_ == CastleChallenge::Dragon && !context_.party.castleRecords.dragonDefeated()) {
        if (const content::StoryBeat* beat = context_.content.findStoryBeat(kDragonJesterBeat)) {
            stack().pushState(std::make_unique<StoryDialogState>(
                stack(), context_, beat->speaker, beat->title, beat->body));
        }
    }
}

void CastleChallengeState::startNextFight() {
    const dungeon::EnemyTeam team = teamFor(kind_, wave_, context_.content, guildTown_);
    if (team.bossId.empty() && team.enemyIds.empty()) {
        finish(true);  // no more fights -> the gauntlet/King was cleared
        return;
    }
    battle::Battle b = battle::buildBattle(context_.party, team, context_.content);
    // M62: the Duck no longer borrows the King's theme — his pond, his own
    // anthem (MusicTrack::DuckBattle, battle-tier synth fallback). M84: a
    // Guild Master gets the ordinary boss anthem — a town fight, not a royal
    // one — and an Endless boss wave (every 10th) does too. M85: so does the
    // Dragon (no new audio this milestone; a bespoke anthem is an owner call).
    const MusicTrack music =
        kind_ == CastleChallenge::King
            ? MusicTrack::KingBattle
            : (kind_ == CastleChallenge::DuckGauntlet && wave_ == 1
                   ? MusicTrack::DuckBattle
                   : (!team.bossId.empty() &&
                              (kind_ == CastleChallenge::GuildBoss ||
                               kind_ == CastleChallenge::Endless ||
                               kind_ == CastleChallenge::Dragon)
                          ? MusicTrack::Boss
                          : MusicTrack::None));
    // M43: the `true` marks this as a castle fight, so a defeat message never
    // claims the dungeon's gold penalty. M56: castle fights wear the Castle
    // backdrop; a boss-team fight (a Boss Rush wave or the King) opens with the
    // Crystal Shatter, which then launches the same battle. Endless waves have no
    // bossId and stay plain. The intro seed is a stable function of the wave.
    const std::uint64_t introSeed = 0xB055C0DE0000ull + static_cast<std::uint64_t>(wave_);
    // M84: a guild fight happens in a town hall, not the throne room — the
    // neutral stage instead of the castle's.
    const render::BackdropStage stage = kind_ == CastleChallenge::GuildBoss
                                            ? render::BackdropStage::Plain
                                            : render::BackdropStage::Castle;
    // M71: the challenge accumulates its own damage tallies so the celebration
    // can put the true MVP on the pedestal.
    if (!team.bossId.empty()) {
        stack().pushState(std::make_unique<BossIntroState>(
            stack(), context_, std::move(b), &result_, music, &stats_, /*castleChallenge=*/true,
            stage, introSeed));
    } else {
        stack().pushState(std::make_unique<BattleState>(stack(), context_, std::move(b), &result_,
                                                        music, &stats_, /*castleChallenge=*/true,
                                                        stage));
    }
}

void CastleChallengeState::onResume() {
    if (done_) {
        return;  // the overlay is up; input pops us
    }
    // M56: the fight now runs behind BossIntroState for boss waves; only act once
    // the battle has truly ended (a still-Ongoing result means a spurious resume).
    if (result_.outcome == battle::Outcome::Ongoing) {
        return;
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
    // M84: the guild gauntlet returns to the Guild's scene, not the castle's.
    context_.audio.setMusic(kind_ == CastleChallenge::GuildBoss ? MusicTrack::Guild
                                                                : MusicTrack::Castle);
    CastleRecords& rec = context_.party.castleRecords;
    std::string msg;
    // M47: a lost challenge costs no gold and forfeits no run — but it is no
    // longer free. Whoever was still standing is carried to the gates at 1 HP,
    // the fallen stay fallen, MP is untouched, and a total wipe leaves exactly
    // one member on their feet. An escape takes this same path (finish(false)),
    // so fleeing keeps the battle-end HP/MP it earned, minus the free heal that
    // used to follow. The text below says so, so the screen and the battle's
    // defeat line agree.
    if (!cleared) {
        clampCastleDefeat(context_.party);
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
                // M61: fell the King with a Goose in the party and something
                // stirs by the pond. Party membership is the rule (fallen geese
                // honked their part too); the marker is the documented class-id
                // constant the M58 scare rule reads.
                if (!context_.party.gooseTownUnlocked) {
                    for (const Character& member : context_.party.members) {
                        if (member.classId == kGooseClassId) {
                            context_.party.gooseTownUnlocked = true;
                            msg += " Your geese honk in triumph - and something answers "
                                   "from a pond beyond the castle. Goose Town has opened.";
                            break;
                        }
                    }
                }
            } else {
                msg = "The King proves too mighty. Return stronger.";
            }
            break;
        case CastleChallenge::DuckGauntlet:  // M61
            if (cleared) {
                if (duckImproved(rec, totalRounds_)) {
                    rec.duckBestTurns = totalRounds_;
                }
                msg = "The Deadly Duck sinks beneath the pond in " +
                      std::to_string(totalRounds_) +
                      " turns! The geese fall silent. Nothing in the realm out-fights "
                      "you now.";
            } else {
                msg = wavesWon_ == 0
                          ? "The Evil Geese overwhelm you. The Duck never even surfaced."
                          : "The Deadly Duck proves deadlier. Return stronger.";
            }
            break;
        case CastleChallenge::Dragon:  // M85
            if (cleared) {
                if (dragonImproved(rec, totalRounds_)) {
                    rec.dragonBestTurns = totalRounds_;
                }
                msg = "The Last Dragon burns out in " + std::to_string(totalRounds_) +
                      " turns! The King, reached for comment, looks genuinely relieved. "
                      "The Pale Jester writes something down.";
            } else {
                msg = wavesWon_ < kDragonWaveCount
                          ? "The vigil holds. The Dragon never even uncoiled."
                          : "The Last Dragon proves too vast. Return stronger.";
            }
            break;
        case CastleChallenge::GuildBoss: {  // M84
            GuildTownRecord& g = guildRecord(context_.party.guild, guildTown_);
            const content::BossDef* master = findGuildMaster(context_.content, guildTown_);
            const std::string masterName =
                master != nullptr ? master->name : std::string("The Guild Master");
            if (cleared) {
                const bool first = !g.defeated();
                if (guildImproved(g, totalRounds_)) {
                    g.bestTurns = totalRounds_;
                }
                msg = masterName + " falls in " + std::to_string(totalRounds_) + " turns!";
                if (first) {
                    msg += " The town will remember this: its milestone is yours to choose.";
                    // The pick-1-of-2 town-perk modal goes on the stack FIRST,
                    // so the celebration and the toasts land above it and the
                    // choice greets the player right after the fanfare. Cancel
                    // postpones — the town re-offers it (the M63 rule).
                    maybePushGuildPerkChoice(stack(), context_);
                } else {
                    msg += " The guild quietly updates its ledger.";
                }
            } else {
                msg = wavesWon_ == 0
                          ? "The Guild Trial overwhelms you. The Master never rose from "
                            "the big chair."
                          : masterName + " proves mightier. Return stronger.";
            }
            break;
        }
    }
    if (!cleared) {
        msg += " You are carried to the gates, barely breathing. No gold is taken, "
               "but nothing is healed - find an inn.";
    }
    resultText_ = msg;
    // M71: beating the King, the Deadly Duck, the Boss Rush — or, since M84, a
    // Guild Master — earns the celebration (the Endless Rush has no "beating"):
    // shown above this state's result overlay, headline = the challenge's turns.
    if (cleared && kind_ != CastleChallenge::Endless) {
        stack().pushState(std::make_unique<CelebrationState>(
            stack(), context_,
            "Cleared in " + std::to_string(totalRounds_) + " turns!", stats_.mvpMember()));
    }
    // M42: a challenge win may unlock castle achievements; toast them above the
    // result overlay this state renders (and above the celebration).
    pushAchievementToasts(stack(), context_, AchvContext{});
}

#ifdef CRYSTAL_CAPTURE
void CastleChallengeState::captureGuildResult() {
    kind_ = CastleChallenge::GuildBoss;
    guildTown_ = 6;  // Grandmaster Ossia — the longest Master name for the title
    totalRounds_ = 88;
    done_ = true;
    const content::BossDef* master = findGuildMaster(context_.content, guildTown_);
    const std::string name = master != nullptr ? master->name : "The Guild Master";
    resultText_ = name + " falls in " + std::to_string(totalRounds_) +
                  " turns! The town will remember this: its milestone is yours to choose.";
}

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
    if (!done_) {
        return;
    }
    // M87: Up/Down scrolls a long result body first.
    if (input.navPressed(InputAction::MoveUp) && resultView_.scrollBy(-1)) {
        context_.audio.play(Sfx::Move);
    }
    if (input.navPressed(InputAction::MoveDown) && resultView_.scrollBy(1)) {
        context_.audio.play(Sfx::Move);
    }
    if (input.pressed(InputAction::Confirm) || input.pressed(InputAction::Cancel)) {
        stack().popState();  // back to whichever hub pushed us (castle or Goose Town)
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
    // M84: the guild overlay is titled by the town's Master itself.
    std::string title = challengeName(kind_);
    if (kind_ == CastleChallenge::GuildBoss) {
        if (const content::BossDef* master = findGuildMaster(context_.content, guildTown_)) {
            title = master->name;
        }
    }
    ui::drawTextCentered(title.c_str(), w / 2, boxY + 12, 16, p.gold);
    ui::drawDivider(boxX + 14, boxY + 34, boxW - 28);
    // M87: the body scrolls past six visible lines instead of truncating.
    resultView_.setContent(resultText_, boxW - 32 - ui::kScrollGutterW, 10,
                           ui::raylibMeasure());
    resultView_.setVisibleLines(std::clamp(resultView_.lineCount(), 1, 6));
    ui::drawTextViewport(resultView_, boxX + 16, boxY + 42, p.text);
    // M62: the Duck gauntlet pops back to Goose Town, not the castle — the
    // prompt says where you actually go. M84: the guild gauntlet, to the Guild.
    const char* returnLabel = kind_ == CastleChallenge::DuckGauntlet
                                  ? "Return to Goose Town"
                                  : (kind_ == CastleChallenge::GuildBoss
                                         ? "Return to the Guild"
                                         : "Return to the Castle");
    ui::drawTextCentered(input::prompt(context_.input.map(), InputAction::Confirm,
                                       context_.input.activeDevice(), returnLabel)
                             .c_str(),
                         w / 2, boxY + boxH - 16, 10, p.gold);
}

}  // namespace cd
