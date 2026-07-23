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
#include "ui/UiStyle.hpp"

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
    namespace style = ui::style;
    const style::Palette& p = style::palette();
    ui::drawSceneBackground(context_.resources, "bg.inn", p.canvas, w, h,
                            context_.party.currentTown);

    ui::drawHeaderBand("Inn", w, p.mpFill, context_.party.gold);
    ui::drawChip(TextFormat("Rest tokens %d", context_.party.restTokens), 4, 27, p.gold);
    ui::drawTextCentered("Rest to fully restore HP and MP.", w / 2, 30, style::kFontBody,
                         p.textDim);

    // Party condition panel: labelled framed meters plus the exact numerals.
    const int rows = static_cast<int>(context_.party.members.size());
    ui::drawFrame(28, 46, w - 56, rows * 16 + 14, ui::FrameStyle::Inset);
    int y = 54;
    for (const Character& c : context_.party.members) {
        ui::drawTextFitted(TextFormat("%s  Lv.%d", c.name.c_str(), c.level), 40, y, 126,
                           style::kFontBody, p.text, "inn.name");
        ui::drawText("HP", 172, y, style::kFontSmall, p.textDim);
        ui::drawMeter(186, y + 1, 46, 7, c.hp, c.maxHp, p.hpFill);
        ui::drawText(TextFormat("%d/%d", c.hp, c.maxHp), 238, y, style::kFontSmall, p.text);
        ui::drawText("MP", 292, y, style::kFontSmall, p.textDim);
        ui::drawMeter(306, y + 1, 40, 7, c.mp, c.maxMp, p.mpFill);
        ui::drawText(TextFormat("%d/%d", c.mp, c.maxMp), 352, y, style::kFontSmall, p.text);
        y += 16;
    }

    const int menuRows = static_cast<int>(menu_.size());
    ui::drawFrame(44, y + 6, 300, menuRows * 18 + 14, ui::FrameStyle::Standard);
    ui::drawMenu(menu_, 66, y + 15, 18, style::kFontMenu, p.text, p.disabled, p.cursor);

    if (!message_.empty()) {
        ui::drawBanner(ui::BannerKind::Success, message_, 60, h - 46, w - 120, "inn.message");
    }
    const InputMap& map = context_.input.map();
    const ActiveDevice device = context_.input.activeDevice();
    ui::drawFooterHints({{input::primaryLabel(map, InputAction::Confirm, device), "Rest"},
                         {input::primaryLabel(map, InputAction::Cancel, device), "Back"}},
                        w, h, "inn.footer");
}

}  // namespace cd
