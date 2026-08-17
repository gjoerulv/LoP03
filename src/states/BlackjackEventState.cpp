#include "states/BlackjackEventState.hpp"

#include <string>

#include "audio/AudioManager.hpp"
#include "core/AppContext.hpp"
#include "game/Gamble.hpp"
#include "game/Party.hpp"
#include "input/Input.hpp"
#include "input/PromptLabels.hpp"
#include "states/StateStack.hpp"
#include "ui/UiDraw.hpp"
#include "ui/UiStyle.hpp"

namespace cd {

namespace style = ui::style;

namespace {
std::string handLine(const char* who, const std::vector<int>& cards, bool hideHole) {
    std::string s = who;
    for (std::size_t i = 0; i < cards.size(); ++i) {
        s += " ";
        s += (hideHole && i == 1) ? "?" : gamble::cardLabel(cards[i]);
    }
    if (!hideHole) {
        s += "  (" + std::to_string(gamble::handValue(cards)) + ")";
    }
    return s;
}
}  // namespace

BlackjackEventState::BlackjackEventState(StateStack& stack, AppContext& context, int bet,
                                         std::uint64_t seed, int room, dungeon::RoomEvent* ev)
    : GameState(stack), context_(context), bet_(bet), seed_(seed), room_(room), ev_(ev) {
    player_.push_back(draw());
    dealer_.push_back(draw());  // the upcard
    player_.push_back(draw());
    dealer_.push_back(draw());  // the hole card
    if (gamble::handValue(player_) == 21) {
        finishHand();  // dealt 21: nothing to decide
    }
}

int BlackjackEventState::draw() { return gamble::blackjackCard(seed_, room_, drawIndex_++); }

void BlackjackEventState::finishHand() {
    const int player = gamble::handValue(player_);
    if (player > 21) {
        resultText_ = "Bust. The dealer does not gloat. Visibly.";
    } else {
        while (gamble::handValue(dealer_) < gamble::kDealerStands) {
            dealer_.push_back(draw());
        }
        const int dealer = gamble::handValue(dealer_);
        if (dealer > 21 || player > dealer) {
            context_.party.gold += bet_ * 2;  // the bet back, doubled (owner spec)
            resultText_ = "You win! " + std::to_string(bet_ * 2) + "g slides across the table.";
        } else if (player == dealer) {
            context_.party.gold += bet_;  // push returns the bet
            resultText_ = "A push. Your " + std::to_string(bet_) + "g comes back, reluctantly.";
        } else {
            resultText_ = "The house takes it. The house usually does.";
        }
    }
    if (ev_ != nullptr) {
        ev_->resolved = true;  // one round per den (owner: "a round of black-jack")
    }
    done_ = true;
    context_.audio.play(Sfx::Interact);
}

void BlackjackEventState::handleInput(const Input& input) {
    if (done_) {
        if (input.pressed(InputAction::Confirm) || input.pressed(InputAction::Cancel)) {
            context_.audio.play(Sfx::Confirm);
            stack().popState();
        }
        return;
    }
    if (input.pressed(InputAction::Confirm)) {  // hit
        context_.audio.play(Sfx::Move);
        player_.push_back(draw());
        if (gamble::handValue(player_) >= 21) {
            finishHand();
        }
        return;
    }
    if (input.pressed(InputAction::Cancel)) {  // stand
        finishHand();
    }
}

void BlackjackEventState::render() {
    const int w = context_.virtualWidth;
    const int h = context_.virtualHeight;
    const style::Palette& p = style::palette();
    ui::drawModalDim(w, h);

    const int boxW = 300;
    const int boxH = 108;
    const int boxX = w / 2 - boxW / 2;
    const int boxY = h / 2 - boxH / 2;
    ui::drawFrame(boxX, boxY, boxW, boxH, ui::FrameStyle::Raised);
    ui::drawTextCentered(("Blackjack - " + std::to_string(bet_) + "g rides").c_str(), w / 2,
                         boxY + 8, style::kFontBody, p.gold);
    ui::drawTextFitted(handLine("You:", player_, false), boxX + 16, boxY + 28, boxW - 32,
                       style::kFontBody, p.text, "blackjack.player");
    ui::drawTextFitted(handLine("Dealer:", dealer_, !done_), boxX + 16, boxY + 44, boxW - 32,
                       style::kFontBody, p.text, "blackjack.dealer");
    if (done_) {
        ui::drawTextFitted(resultText_, boxX + 16, boxY + 64, boxW - 32, style::kFontBody,
                           p.textDim, "blackjack.result");
    }

    const InputMap& map = context_.input.map();
    const ActiveDevice device = context_.input.activeDevice();
    const std::string hint =
        done_ ? input::prompt(map, InputAction::Confirm, device, "Done")
              : input::prompt(map, InputAction::Confirm, device, "Hit") + "   " +
                    input::prompt(map, InputAction::Cancel, device, "Stand");
    ui::drawTextCentered(hint.c_str(), w / 2, boxY + boxH - 16, style::kFontSmall, p.textHint);
}

}  // namespace cd
