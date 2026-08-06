#include "states/TreasureFightState.hpp"

#include <algorithm>
#include <memory>
#include <string>

#include "audio/AudioManager.hpp"
#include "content/ContentDatabase.hpp"
#include "core/AppContext.hpp"
#include "core/FadeController.hpp"
#include "dungeon/DungeonModel.hpp"
#include "game/Party.hpp"
#include "game/Scrolls.hpp"
#include "game/TreasureMap.hpp"
#include "input/Input.hpp"
#include "input/PromptLabels.hpp"
#include "raylib.h"
#include "render/BattleBackdrop.hpp"
#include "states/BossIntroState.hpp"
#include "states/StateStack.hpp"
#include "ui/UiDraw.hpp"
#include "ui/UiStyle.hpp"

namespace cd {

TreasureFightState::TreasureFightState(StateStack& stack, AppContext& context)
    : GameState(stack), context_(context) {}

void TreasureFightState::onEnter() {
    if (done_) {
        return;  // a capture preset set the overlay; no fight
    }
    const TreasureReveal& t = context_.party.treasure;
    const content::BossDef* boss = context_.content.findBoss(t.bossId);
    if (!t.active || boss == nullptr) {
        finish(false);  // defensive: a ghost reveal shows a plain overlay
        resultText_ = "The dig turns up nothing but mud.";
        return;
    }
    // The guard team: the seeded roster boss with its authored court, at the
    // stored scale — exactly the fight that dungeon's own boss would be.
    dungeon::EnemyTeam team;
    team.isBoss = true;
    team.bossId = t.bossId;
    team.name = boss->name;
    team.enemyIds = boss->minions;
    team.statScalePct = t.scalePct;
    battle::Battle b = battle::buildBattle(context_.party, team, context_.content);
    // A boss team: the Crystal Shatter opens it; castleChallenge semantics keep
    // the defeat text honest (no dungeon gold claim). Plain backdrop — the dig
    // happens in town, not a dungeon.
    stack().pushState(std::make_unique<BossIntroState>(
        stack(), context_, std::move(b), &result_, MusicTrack::Boss, nullptr,
        /*castleChallenge=*/true, render::BackdropStage::Plain,
        0xD16D16ull ^ static_cast<std::uint64_t>(t.scalePct)));
}

#ifdef CRYSTAL_CAPTURE
void TreasureFightState::captureResult() {
    done_ = true;
    pickingMember_ = true;
    pendingScrollId_ = treasureScrollPool().front();
    const content::ItemDef* item = context_.content.findItem(pendingScrollId_);
    resultText_ = "The guardian falls! Buried beneath: " +
                  (item != nullptr ? item->name : pendingScrollId_) +
                  ". Its words fade fast - someone must learn it NOW.";
}
#endif

void TreasureFightState::onResume() {
    if (done_) {
        return;
    }
    if (result_.outcome == battle::Outcome::Ongoing) {
        return;  // spurious resume while the intro/battle still runs
    }
    context_.fade.start();
    finish(result_.outcome == battle::Outcome::Victory);
}

void TreasureFightState::finish(bool won) {
    done_ = true;
    Party& p = context_.party;
    if (!won) {
        clampCastleDefeat(p);  // the castle price; the reveal stays for a retry
        resultText_ =
            "The guardian proves too strong. You are carried off, barely breathing - "
            "the map still marks the spot.";
        return;
    }
    p.treasure = TreasureReveal{};  // dug up; a new cycle may begin
    // M83: the guild's IOUs pay out the moment the dig resolves, jump-starting
    // the next map cycle. The pouch never exceeds 3 (a fourth piece must come
    // with a run's town/guard context to fire a reveal), so a payout that
    // would overfill it pays what fits and KEEPS the rest banked for the next
    // dig — an IOU is never silently lost.
    std::string owedNote;
    if (p.mapPiecesOwed > 0) {
        const int pay = payMapDebt(p.mapPieces, p.mapPiecesOwed);
        if (pay > 0) {
            owedNote =
                TextFormat(" The guild pays its debt: %d banked map piece%s join the pouch.",
                           pay, pay == 1 ? "" : "s");
        } else {
            owedNote = " The pouch is full; the guild's map-piece debt stands.";
        }
    }
    const std::string scrollId = nextTreasureScroll(p.treasureScrollsAwarded);
    if (scrollId.empty()) {
        p.legendaryTokens += kTreasureTokenFallback;
        p.gold += kTreasureGoldFallback;
        resultText_ = TextFormat(
            "The guardian falls! The chest holds riches: +%d gold and +%d legendary token.",
            kTreasureGoldFallback, kTreasureTokenFallback) + owedNote;
        return;
    }
    pendingScrollId_ = scrollId;
    pickingMember_ = true;
    const content::ItemDef* item = context_.content.findItem(scrollId);
    resultText_ = "The guardian falls! Buried beneath: " +
                  (item != nullptr ? item->name : scrollId) +
                  ". Its words fade fast - someone must learn it NOW." + owedNote;
}

void TreasureFightState::awardTreasure(int memberIndex) {
    Party& p = context_.party;
    const content::ItemDef* item = context_.content.findItem(pendingScrollId_);
    if (item == nullptr || memberIndex < 0 ||
        memberIndex >= static_cast<int>(p.members.size())) {
        return;
    }
    Character& c = p.members[static_cast<std::size_t>(memberIndex)];
    const std::string refusal = scrollRefusal(c, *item, context_.content);
    if (!refusal.empty()) {
        resultText_ = refusal + " Choose another student.";
        context_.audio.play(Sfx::Error);
        return;
    }
    learnScroll(c, *item);
    p.treasureScrollsAwarded.push_back(pendingScrollId_);
    const content::SkillDef* skill = context_.content.findSkill(item->grantsSkill);
    resultText_ = c.name + " learns " + (skill != nullptr ? skill->name : item->grantsSkill) +
                  " - a skill found nowhere else!";
    pickingMember_ = false;
    pendingScrollId_.clear();
    context_.audio.play(Sfx::Victory);
}

void TreasureFightState::handleInput(const Input& input) {
    if (!done_) {
        return;
    }
    if (pickingMember_) {
        const int count = static_cast<int>(context_.party.members.size());
        if (input.navPressed(InputAction::MoveUp) && count > 0) {
            cursor_ = (cursor_ + count - 1) % count;
        }
        if (input.navPressed(InputAction::MoveDown) && count > 0) {
            cursor_ = (cursor_ + 1) % count;
        }
        if (input.pressed(InputAction::Confirm)) {
            awardTreasure(cursor_);
        }
        return;  // no Cancel: the words fade fast — someone must learn it
    }
    if (input.pressed(InputAction::Confirm) || input.pressed(InputAction::Cancel)) {
        stack().popState();  // back to town
    }
}

void TreasureFightState::render() {
    const int w = context_.virtualWidth;
    const int h = context_.virtualHeight;
    const ui::style::Palette& p = ui::style::palette();
    ClearBackground(p.canvas);
    if (!done_) {
        return;  // a battle is on top
    }
    const int boxW = 330;
    const int boxH = 168;
    const int boxX = w / 2 - boxW / 2;
    const int boxY = h / 2 - boxH / 2;
    ui::drawFrame(boxX, boxY, boxW, boxH, ui::FrameStyle::Reward);
    ui::drawTextCentered("The Buried Treasure", w / 2, boxY + 12, 16, p.gold);
    ui::drawDivider(boxX + 14, boxY + 34, boxW - 28);
    ui::drawTextWrapped(resultText_, boxX + 16, boxY + 42, boxW - 32, 10, p.text,
                        "treasure.result", 5);
    if (pickingMember_) {
        const auto& members = context_.party.members;
        const int listY = boxY + 96;
        for (std::size_t i = 0; i < members.size(); ++i) {
            const int y = listY + static_cast<int>(i) * 14;
            if (static_cast<int>(i) == cursor_) {
                ui::drawSelectionSlab(boxX + 60, y - 2, boxW - 120, 13);
            }
            ui::drawTextCentered(members[i].name.c_str(), w / 2, y, 10,
                                 static_cast<int>(i) == cursor_ ? p.text : p.textDim);
        }
    } else {
        ui::drawTextCentered(input::prompt(context_.input.map(), InputAction::Confirm,
                                           context_.input.activeDevice(), "Return to Town")
                                 .c_str(),
                             w / 2, boxY + boxH - 16, 10, p.gold);
    }
}

}  // namespace cd
