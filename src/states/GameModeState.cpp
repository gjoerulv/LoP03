#include "states/GameModeState.hpp"

#include <memory>

#include "audio/AudioManager.hpp"
#include "core/AppContext.hpp"
#include "game/IronMan.hpp"
#include "input/Input.hpp"
#include "input/PromptLabels.hpp"
#include "raylib.h"
#include "states/ConfirmPromptState.hpp"
#include "states/PartyCreationState.hpp"
#include "states/StateStack.hpp"
#include "ui/TextLayout.hpp"
#include "ui/UiDraw.hpp"
#include "ui/UiStyle.hpp"

namespace cd {

namespace {
constexpr int kNormal = 0;
constexpr int kIronMan = 1;

// The mode list (left) and the explanation panel (right). The panel is as
// wide as the screen allows: the Iron Man rules are four full paragraphs.
constexpr int kListX = 16;
constexpr int kListW = 100;
constexpr int kPanelX = 124;
constexpr int kPanelY = 46;  // clear of the title plaque (the Danger frame wears a tab)
constexpr int kPanelH = 170;
constexpr int kParagraphGap = 4;
}  // namespace

GameModeState::GameModeState(StateStack& stack, AppContext& context)
    : GameState(stack), context_(context) {
    // Normal first and under the cursor: Iron Man is never one stray press away.
    menu_.setItems({{ironman::kNormalName, true}, {ironman::kIronManName, true}});
    menu_.setCursor(kNormal);
}

void GameModeState::choose() {
    if (menu_.cursor() == kIronMan) {
        askBeginIronMan();
        return;
    }
    stack().pushState(std::make_unique<PartyCreationState>(stack(), context_, /*ironMan=*/false));
}

void GameModeState::askBeginIronMan() {
    StateStack* stackPtr = &stack();
    AppContext* contextPtr = &context_;
    stack().pushState(std::make_unique<ConfirmPromptState>(
        stack(), context_, ironman::kBeginTitle, ironman::kBeginBody, ironman::kBeginConfirm,
        ironman::kBeginCancel, [stackPtr, contextPtr]() {
            stackPtr->pushState(
                std::make_unique<PartyCreationState>(*stackPtr, *contextPtr, /*ironMan=*/true));
        }));
}

#ifdef CRYSTAL_CAPTURE
void GameModeState::captureIronMan(bool askBegin) {
    menu_.setCursor(kIronMan);
    if (askBegin) {
        askBeginIronMan();
    }
}
#endif

void GameModeState::handleInput(const Input& input) {
    if (input.navPressed(InputAction::MoveUp)) {
        menu_.moveUp();
        context_.audio.play(Sfx::Move);
    }
    if (input.navPressed(InputAction::MoveDown)) {
        menu_.moveDown();
        context_.audio.play(Sfx::Move);
    }
    if (input.pressed(InputAction::Confirm)) {
        context_.audio.play(Sfx::Confirm);
        choose();
        return;
    }
    if (input.pressed(InputAction::Cancel)) {
        context_.audio.play(Sfx::Cancel);
        stack().popState();
    }
}

void GameModeState::render() {
    const int w = context_.virtualWidth;
    const int h = context_.virtualHeight;
    namespace style = ui::style;
    const style::Palette& p = style::palette();
    ClearBackground(p.canvas);

    ui::drawTitlePlaque("New Game", w / 2, 14, 16);

    ui::drawFrame(kListX, kPanelY, kListW, 58, ui::FrameStyle::Standard);
    ui::drawMenu(menu_, kListX + 22, kPanelY + 14, 18, 12, p.text, p.disabled, p.cursor);

    const bool iron = menu_.cursor() == kIronMan;
    const int panelW = w - kPanelX - 16;
    ui::drawFrame(kPanelX, kPanelY, panelW, kPanelH,
                  iron ? ui::FrameStyle::Danger : ui::FrameStyle::Standard);
    const int textX = kPanelX + 12;
    const int textW = panelW - 24;
    ui::drawSectionHeader(iron ? ironman::kIronManName : ironman::kNormalName, textX,
                          kPanelY + 10, textW);

    // Policy A per paragraph, with the line budget taken from what is left of
    // the panel - an overflowing rule fails the capture lint instead of
    // running out of the frame.
    const int step = ui::lineHeight(style::kFontBody);
    const int bottom = kPanelY + kPanelH - 6;
    int y = kPanelY + 28;
    if (iron) {
        for (const char* rule : ironman::kRules) {
            const int budget = (bottom - y) / step;
            y = ui::drawTextWrapped(rule, textX, y, textW, style::kFontBody, p.text,
                                    "gamemode.rule", budget > 0 ? budget : 1) +
                kParagraphGap;
        }
    } else {
        ui::drawTextWrapped(ironman::kNormalBlurb, textX, y, textW, style::kFontBody, p.text,
                            "gamemode.normal", (bottom - y) / step);
    }

    const InputMap& map = context_.input.map();
    const ActiveDevice device = context_.input.activeDevice();
    ui::drawFooterHints({{input::primaryLabel(map, InputAction::Confirm, device), "Choose"},
                         {input::primaryLabel(map, InputAction::Cancel, device), "Back"}},
                        w, h, "gamemode.footer");
}

}  // namespace cd
