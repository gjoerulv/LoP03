#include "states/SparState.hpp"

#include <memory>

#include "audio/AudioManager.hpp"
#include "core/AppContext.hpp"
#include "input/Input.hpp"
#include "input/PromptLabels.hpp"
#include "raylib.h"
#include "states/BattleState.hpp"
#include "states/StateStack.hpp"
#include "ui/UiDraw.hpp"
#include "ui/UiStyle.hpp"

namespace cd {

SparState::SparState(StateStack& stack, AppContext& context, bool manual)
    : GameState(stack), context_(context), manual_(manual) {}

#ifdef CRYSTAL_CAPTURE
void SparState::captureShowClosing() {
    fought_ = true;  // onEnter must not push a battle in the capture
    restored_ = true;
    closing_ = "The echoes prevail. Sparring over - no harm done.";
}
#endif

void SparState::onEnter() {
    if (fought_) {
        return;
    }
    fought_ = true;
    savedParty_ = context_.party;  // the whole ledger: nothing can leak
    battle::Battle b = battle::buildSparBattle(context_.party, context_.content);
    stack().pushState(std::make_unique<BattleState>(
        stack(), context_, std::move(b), &result_, MusicTrack::Battle, &sparStats_,
        /*castleChallenge=*/false, render::BackdropStage::Plain,
        /*spoils=*/nullptr, /*manualEnemies=*/manual_));
}

void SparState::onResume() {
    if (restored_) {
        return;
    }
    restored_ = true;
    // Every trace of the spar unwinds — HP, MP, items spent, gold, records,
    // bestiary bits — regardless of how the fight ended.
    const battle::Outcome outcome = result_.outcome;
    context_.party = savedParty_;
    switch (outcome) {
        case battle::Outcome::Victory:
            closing_ = "The echoes yield. Sparring over - no harm done.";
            break;
        case battle::Outcome::Defeat:
            closing_ = "The echoes prevail. Sparring over - no harm done.";
            break;
        default:
            closing_ = "The spar breaks off. No harm done.";
            break;
    }
    context_.audio.play(Sfx::Confirm);
}

void SparState::handleInput(const Input& input) {
    if (!restored_) {
        return;  // the battle is still on top
    }
    if (input.pressed(InputAction::Confirm) || input.pressed(InputAction::Cancel)) {
        stack().popState();  // back to the Training Hall
    }
}

void SparState::render() {
    if (!restored_) {
        return;  // the battle renders; nothing to add yet
    }
    const int w = context_.virtualWidth;
    const int h = context_.virtualHeight;
    namespace style = ui::style;
    const style::Palette& p = style::palette();
    ui::drawModalDim(w, h);
    const int boxW = 280;
    const int boxH = 64;
    const int boxX = w / 2 - boxW / 2;
    const int boxY = h / 2 - boxH / 2;
    ui::drawFrame(boxX, boxY, boxW, boxH, ui::FrameStyle::Raised);
    ui::drawTitlePlaque("Sparring Hall", w / 2, boxY - 10, 12);
    ui::drawTextCentered(closing_.c_str(), w / 2, boxY + 18, style::kFontBody, p.text);
    const InputMap& map = context_.input.map();
    const ActiveDevice device = context_.input.activeDevice();
    ui::drawTextCentered(
        (input::primaryLabel(map, InputAction::Confirm, device) + "  Done").c_str(), w / 2,
        boxY + 40, style::kFontBody, p.textDim);
}

}  // namespace cd
