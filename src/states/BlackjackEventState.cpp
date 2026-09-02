#include "states/BlackjackEventState.hpp"

#include <string>

#include "audio/AudioManager.hpp"
#include "core/AppContext.hpp"
#include "game/Gamble.hpp"
#include "game/Party.hpp"
#include "input/Input.hpp"
#include "input/PromptLabels.hpp"
#include "resource/ResourceManager.hpp"  // 2026-08-29: the card textures
#include "states/StateStack.hpp"
#include "ui/UiDraw.hpp"
#include "ui/UiStyle.hpp"

namespace cd {

namespace style = ui::style;

namespace {
// Owner request 2026-08-29: ACTUAL cards. The authored A/J/Q/K faces (the
// goose, the duck, the dark king), the blank face lettered with the rank for
// 2..10, and the woven back for the dealer's hole card. A missing texture
// falls back to the old text label — placeholder discipline, never a crash.
constexpr int kCardW = 18;
constexpr int kCardH = 24;
constexpr int kCardGap = 3;

void drawCard(AppContext& context, int x, int y, int rank, bool faceUp) {
    const style::Palette& p = style::palette();
    const char* id = !faceUp      ? "ui.card.back"
                     : rank == 1  ? "ui.card.ace"
                     : rank == 11 ? "ui.card.jack"
                     : rank == 12 ? "ui.card.queen"
                     : rank == 13 ? "ui.card.king"
                                  : "ui.card.face";
    if (!context.resources.hasTexture(id)) {
        ui::drawText(faceUp ? gamble::cardLabel(rank) : "?", x + 4, y + 8, 10, p.text);
        return;
    }
    DrawTextureEx(context.resources.texture(id),
                  Vector2{static_cast<float>(x), static_cast<float>(y)}, 0.0f, 1.0f, WHITE);
    if (faceUp && rank >= 2 && rank <= 10) {
        const std::string lbl = gamble::cardLabel(rank);
        const int lw = ui::measureText(lbl, 10);
        ui::drawText(lbl, x + (kCardW - lw) / 2, y + (kCardH - 10) / 2, 10, p.ink);
    }
}

void drawHand(AppContext& context, int x, int y, const std::vector<int>& cards, bool hideHole) {
    for (std::size_t i = 0; i < cards.size(); ++i) {
        drawCard(context, x + static_cast<int>(i) * (kCardW + kCardGap), y,
                 cards[static_cast<std::size_t>(i)], !(hideHole && i == 1));
    }
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

    // Owner request 2026-08-29: the table deals REAL cards — dealer's row
    // above (hole card face-down until the hand ends), yours below, values
    // beside the labels, the result line under both.
    const int boxW = 320;
    const int boxH = 158;
    const int boxX = w / 2 - boxW / 2;
    const int boxY = h / 2 - boxH / 2;
    ui::drawFrame(boxX, boxY, boxW, boxH, ui::FrameStyle::Raised);
    ui::drawTextCentered(("Blackjack - " + std::to_string(bet_) + "g rides").c_str(), w / 2,
                         boxY + 8, style::kFontBody, p.gold);
    const std::string dealerLabel =
        std::string("Dealer  (") +
        (done_ ? std::to_string(gamble::handValue(dealer_)) : std::string("?")) + ")";
    ui::drawTextFitted(dealerLabel, boxX + 16, boxY + 24, boxW - 32, style::kFontSmall,
                       p.textDim, "blackjack.dealer");
    drawHand(context_, boxX + 16, boxY + 33, dealer_, !done_);
    const std::string youLabel =
        "You  (" + std::to_string(gamble::handValue(player_)) + ")";
    ui::drawTextFitted(youLabel, boxX + 16, boxY + 63, boxW - 32, style::kFontSmall,
                       p.textDim, "blackjack.player");
    drawHand(context_, boxX + 16, boxY + 72, player_, false);
    if (done_) {
        ui::drawTextFitted(resultText_, boxX + 16, boxY + 104, boxW - 32, style::kFontBody,
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
