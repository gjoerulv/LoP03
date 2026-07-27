#include "states/RoadForkState.hpp"

#include <memory>

#include "audio/AudioManager.hpp"
#include "core/AppContext.hpp"
#include "input/Input.hpp"
#include "input/PromptLabels.hpp"
#include "states/CastleState.hpp"
#include "states/GooseTownState.hpp"
#include "states/StateStack.hpp"
#include "ui/UiDraw.hpp"
#include "ui/UiStyle.hpp"

namespace cd {

RoadForkState::RoadForkState(StateStack& stack, AppContext& context)
    : GameState(stack), context_(context) {
    menu_.setItems({{"The Castle", true}, {"Goose Town", true}});
}

void RoadForkState::handleInput(const Input& input) {
    if (input.navPressed(InputAction::MoveUp)) {
        menu_.moveUp();
    }
    if (input.navPressed(InputAction::MoveDown)) {
        menu_.moveDown();
    }
    if (input.pressed(InputAction::Cancel)) {
        context_.audio.play(Sfx::Cancel);
        stack().popState();  // back onto the road
        return;
    }
    if (input.pressed(InputAction::Confirm)) {
        context_.audio.play(Sfx::Confirm);
        // Swap the prompt for the destination: the pop is queued first, so the
        // hub lands directly above the town (TownState's resume disarms the
        // road trigger exactly as it does returning from the castle).
        stack().popState();
        if (menu_.cursor() == 0) {
            stack().pushState(std::make_unique<CastleState>(stack(), context_));
        } else {
            stack().pushState(std::make_unique<GooseTownState>(stack(), context_));
        }
    }
}

void RoadForkState::render() {
    const int w = context_.virtualWidth;
    const int h = context_.virtualHeight;
    const ui::style::Palette& p = ui::style::palette();
    ui::drawModalDim(w, h);
    const int boxW = 190;
    const int boxH = 92;
    const int boxX = w / 2 - boxW / 2;
    const int boxY = h / 2 - boxH / 2;
    ui::drawFrame(boxX, boxY, boxW, boxH, ui::FrameStyle::Raised);
    ui::drawTextCentered("The road forks.", w / 2, boxY + 10, 12, p.gold);
    ui::drawMenu(menu_, boxX + 40, boxY + 30, 18, 12, p.text, p.disabled, p.cursor);
    ui::drawFooterHints({{input::primaryLabel(context_.input.map(), InputAction::Cancel,
                                              context_.input.activeDevice()),
                          "Back"}},
                        w, h, "roadfork.footer");
}

}  // namespace cd
