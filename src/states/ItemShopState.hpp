#pragma once

#include <string>
#include <vector>

#include "states/GameState.hpp"
#include "ui/Menu.hpp"
#include "ui/ScrollWindow.hpp"

namespace cd {

struct AppContext;

// The Item Shop: buy consumables (potions, ethers, antidotes, revives) with gold.
class ItemShopState : public GameState {
public:
    ItemShopState(StateStack& stack, AppContext& context);

    void onEnter() override;
    void handleInput(const Input& input) override;
    void render() override;

private:
    void rebuild();

    AppContext& context_;
    ui::Menu menu_;
    ui::ScrollWindow scroll_;
    std::vector<std::string> ids_;
    std::string message_;
};

}  // namespace cd
