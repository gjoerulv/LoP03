#pragma once

#include <string>

#include "states/GameState.hpp"
#include "ui/Menu.hpp"

namespace cd {

struct AppContext;

// The black market (M34): a rare NPC selling one legendary piece for gold or
// legendary tokens. Opened from the town when an offer is present; a purchase
// grants the item, spends the currency, and clears the offer. Declining never
// blocks anything. The offer itself lives on Party (game/BlackMarket.hpp).
class BlackMarketState : public GameState {
public:
    BlackMarketState(StateStack& stack, AppContext& context);

    void onEnter() override;
    void handleInput(const Input& input) override;
    void render() override;

private:
    void rebuild();
    void grantOffered();  // add the item, clear the offer, mark purchased

    AppContext& context_;
    ui::Menu menu_;
    std::string itemId_;   // captured on enter so the display survives the purchase
    int priceGold_ = 0;
    bool purchased_ = false;
    std::string message_;
};

}  // namespace cd
