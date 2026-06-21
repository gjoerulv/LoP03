#include "states/TrainingHallState.hpp"

#include <utility>
#include <vector>

#include "audio/AudioManager.hpp"
#include "content/ContentDatabase.hpp"
#include "core/AppContext.hpp"
#include "game/Party.hpp"
#include "input/Input.hpp"
#include "raylib.h"
#include "states/StateStack.hpp"
#include "ui/UiDraw.hpp"

namespace cd {

TrainingHallState::TrainingHallState(StateStack& stack, AppContext& context)
    : GameState(stack), context_(context) {}

void TrainingHallState::onEnter() { rebuild(); }

int TrainingHallState::trainingCost(int level) const { return 40 + level * 30; }

void TrainingHallState::rebuild() {
    std::vector<ui::MenuItem> items;
    for (const Character& c : context_.party.members) {
        if (c.level >= kMaxLevel) {
            items.push_back({TextFormat("%-10s  Lv.%d  (max)", c.name.c_str(), c.level), false});
        } else {
            items.push_back(
                {TextFormat("%-10s  Lv.%d  ->  %dg", c.name.c_str(), c.level, trainingCost(c.level)),
                 true});
        }
    }
    const int previous = menu_.cursor();
    menu_.setItems(std::move(items));
    menu_.setCursor(previous);
}

void TrainingHallState::handleInput(const Input& input) {
    if (input.pressed(InputAction::MoveUp)) {
        menu_.moveUp();
        context_.audio.play(Sfx::Move);
    }
    if (input.pressed(InputAction::MoveDown)) {
        menu_.moveDown();
        context_.audio.play(Sfx::Move);
    }
    if (input.pressed(InputAction::Confirm) && menu_.currentEnabled()) {
        Character& c = context_.party.members[static_cast<std::size_t>(menu_.cursor())];
        const int cost = trainingCost(c.level);
        if (context_.party.gold >= cost) {
            context_.party.gold -= cost;
            grantXp(c, xpToNext(c.level) - c.xp, context_.content);  // exactly one level
            context_.audio.play(Sfx::Heal);
            message_ = c.name + " trained to Lv." + std::to_string(c.level) + "!";
            rebuild();
        } else {
            context_.audio.play(Sfx::Cancel);
            message_ = "Not enough gold to train " + c.name;
        }
    }
    if (input.pressed(InputAction::Cancel)) {
        context_.audio.play(Sfx::Cancel);
        stack().popState();
    }
}

void TrainingHallState::render() {
    const int w = context_.virtualWidth;
    const int h = context_.virtualHeight;
    ClearBackground(Color{16, 16, 24, 255});
    ui::drawTextCentered("Training Hall", w / 2, 14, 16, RAYWHITE);
    DrawText(TextFormat("Gold: %d", context_.party.gold), w - 110, 14, 11,
             Color{230, 220, 150, 255});
    ui::drawTextCentered("Spend gold to raise a character's level.", w / 2, 36, 10,
                         Color{180, 180, 200, 255});

    ui::drawMenu(menu_, 60, 64, 18, 11, RAYWHITE, Color{90, 90, 110, 255},
                 Color{240, 220, 120, 255});

    if (!message_.empty()) {
        ui::drawTextCentered(message_.c_str(), w / 2, h - 30, 10, Color{170, 220, 170, 255});
    }
    ui::drawTextCentered("Confirm: Train    Cancel: Back", w / 2, h - 14, 10,
                         Color{150, 150, 170, 255});
}

}  // namespace cd
