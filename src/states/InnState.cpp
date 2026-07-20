#include "states/InnState.hpp"

#include <utility>
#include <vector>

#include "audio/AudioManager.hpp"
#include "core/AppContext.hpp"
#include "game/Party.hpp"
#include "input/Input.hpp"
#include "input/PromptLabels.hpp"
#include "raylib.h"
#include "states/StateStack.hpp"
#include "ui/UiDraw.hpp"

namespace cd {

namespace {

bool partyFullyRested(const Party& party) {
    for (const Character& c : party.members) {
        if (c.hp < c.maxHp || c.mp < c.maxMp) {
            return false;
        }
    }
    return true;
}

}  // namespace

InnState::InnState(StateStack& stack, AppContext& context)
    : GameState(stack), context_(context) {}

void InnState::onEnter() { rebuild(); }

void InnState::rebuild() {
    // The paid-rest row is always index 0; the token row (index 1) is present
    // only when tokens are held — handleInput relies on that ordering.
    std::vector<ui::MenuItem> items;
    const int cost = restCost(context_.party);
    const bool rested = partyFullyRested(context_.party);
    if (rested) {
        items.push_back({"Rest  -  the party is already rested", false});
    } else if (context_.party.gold < cost) {
        items.push_back({TextFormat("Rest  -  %dg  (not enough gold)", cost), false});
    } else {
        items.push_back({TextFormat("Rest  -  %dg", cost), true});
    }
    if (context_.party.restTokens > 0) {
        items.push_back(
            {TextFormat("Free-rest token  (%d held)", context_.party.restTokens), !rested});
    }
    const int previous = menu_.cursor();
    menu_.setItems(std::move(items));
    menu_.setCursor(previous);
}

void InnState::handleInput(const Input& input) {
    if (input.navPressed(InputAction::MoveUp)) {
        menu_.moveUp();
        context_.audio.play(Sfx::Move);
    }
    if (input.navPressed(InputAction::MoveDown)) {
        menu_.moveDown();
        context_.audio.play(Sfx::Move);
    }
    if (input.pressed(InputAction::Confirm) && menu_.currentEnabled()) {
        const bool tokenRow = context_.party.restTokens > 0 && menu_.cursor() == 1;
        if (tokenRow) {
            context_.party.restTokens -= 1;
            healFull(context_.party);
            context_.audio.play(Sfx::Heal);
            message_ = "You spend a free-rest token. Fully restored.";
        } else {
            const int cost = restCost(context_.party);
            context_.party.gold -= cost;  // enabled row guarantees affordability
            healFull(context_.party);
            context_.audio.play(Sfx::Heal);
            message_ = "The party rests for " + std::to_string(cost) + "g. Fully restored.";
        }
        rebuild();
    }
    if (input.pressed(InputAction::Cancel)) {
        context_.audio.play(Sfx::Cancel);
        stack().popState();
    }
}

void InnState::render() {
    const int w = context_.virtualWidth;
    const int h = context_.virtualHeight;
    ui::drawSceneBackground(context_.resources, "bg.inn", Color{20, 16, 24, 255}, w, h,
                            context_.party.currentTown);

    ui::drawTextCentered("Inn", w / 2, 14, 16, RAYWHITE);
    ui::drawText(TextFormat("Gold: %d", context_.party.gold), w - 150, 12, 11,
                 Color{230, 220, 150, 255});
    ui::drawText(TextFormat("Tokens: %d", context_.party.restTokens), w - 150, 26, 11,
                 Color{240, 200, 140, 255});
    ui::drawTextCentered("Rest to fully restore HP and MP.", w / 2, 36, 10,
                         Color{180, 180, 200, 255});

    int y = 58;
    for (const Character& c : context_.party.members) {
        ui::drawTextFitted(TextFormat("%s  Lv.%d", c.name.c_str(), c.level), 40, y, 130, 10,
                           RAYWHITE, "inn.name");
        ui::drawText(TextFormat("HP %d/%d   MP %d/%d", c.hp, c.maxHp, c.mp, c.maxMp), 176, y, 10,
                     Color{170, 220, 170, 255});
        y += 16;
    }

    ui::drawMenu(menu_, 60, y + 10, 18, 11, RAYWHITE, Color{90, 90, 110, 255},
                 Color{240, 220, 120, 255});

    if (!message_.empty()) {
        ui::drawTextCentered(message_.c_str(), w / 2, h - 30, 10, Color{170, 220, 170, 255});
    }
    const InputMap& map = context_.input.map();
    const ActiveDevice device = context_.input.activeDevice();
    const std::string footer = input::prompt(map, InputAction::Confirm, device, "Rest") + "    " +
                               input::prompt(map, InputAction::Cancel, device, "Back");
    ui::drawTextCentered(footer.c_str(), w / 2, h - 14, 10, Color{150, 150, 170, 255});
}

}  // namespace cd
