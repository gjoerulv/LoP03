#include "states/GuildPerkChoiceState.hpp"

#include <memory>
#include <string>

#include "audio/AudioManager.hpp"
#include "core/AppContext.hpp"
#include "game/Guild.hpp"
#include "game/Party.hpp"
#include "input/Input.hpp"
#include "input/PromptLabels.hpp"
#include "raylib.h"
#include "states/StateStack.hpp"
#include "ui/UiDraw.hpp"
#include "ui/UiStyle.hpp"

namespace cd {

GuildPerkChoiceState::GuildPerkChoiceState(StateStack& stack, AppContext& context)
    : GameState(stack), context_(context) {
    findPending();
}

bool GuildPerkChoiceState::findPending() {
    town_ = guildPendingPerkTown(context_.party.guild);
    cursor_ = 0;
    return town_ > 0;
}

#ifdef CRYSTAL_CAPTURE
void GuildPerkChoiceState::captureSelect(int town) {
    town_ = town;
    cursor_ = 0;
}
#endif

void GuildPerkChoiceState::handleInput(const Input& input) {
    if (town_ <= 0) {
        stack().popState();  // nothing pending (defensive; ctor already scanned)
        return;
    }
    if (input.navPressed(InputAction::MoveUp) || input.navPressed(InputAction::MoveDown)) {
        context_.audio.play(Sfx::Move);
        cursor_ = 1 - cursor_;
    }
    if (input.pressed(InputAction::Cancel)) {
        // Postpone: the pending choice simply re-prompts at the next town
        // visit. Never a hostage screen (the M63 rule).
        context_.audio.play(Sfx::Cancel);
        stack().popState();
        return;
    }
    if (input.pressed(InputAction::Confirm)) {
        const auto pair = guildPerkPair(town_);
        const GuildPerkDef* chosen = cursor_ == 0 ? pair.first : pair.second;
        if (chosen != nullptr) {
            guildRecord(context_.party.guild, town_).perkId = chosen->id;
            context_.audio.play(Sfx::Confirm);
        }
        // Drain the next pending town (records can queue up when the modal was
        // postponed across several first victories), or step aside.
        if (!findPending()) {
            stack().popState();
        }
    }
}

void GuildPerkChoiceState::render() {
    if (town_ <= 0) {
        return;
    }
    const auto pair = guildPerkPair(town_);
    if (pair.first == nullptr || pair.second == nullptr) {
        return;
    }
    const int w = context_.virtualWidth;
    const int h = context_.virtualHeight;
    const ui::style::Palette& p = ui::style::palette();
    ui::drawModalDim(w, h);

    const int boxW = 340;
    const int boxH = 168;
    const int boxX = w / 2 - boxW / 2;
    const int boxY = h / 2 - boxH / 2;
    ui::drawFrame(boxX, boxY, boxW, boxH, ui::FrameStyle::Reward);
    ui::drawTextCentered(TextFormat("TOWN %d MILESTONE", town_), w / 2, boxY + 10, 14, p.gold);
    ui::drawTextCentered("The Guild Master has fallen. The town grants a permanent boon.",
                         w / 2, boxY + 28, 10, p.textDim);

    const GuildPerkDef* options[2] = {pair.first, pair.second};
    const int optY[2] = {boxY + 44, boxY + 96};
    for (int i = 0; i < 2; ++i) {
        const GuildPerkDef& m = *options[i];
        const int y = optY[i];
        if (i == cursor_) {
            ui::drawSelectionSlab(boxX + 10, y - 2, boxW - 20, 48);
        }
        ui::drawText(m.name, boxX + 22, y + 2, 12, i == cursor_ ? p.text : p.textDim);
        ui::drawTextWrapped(m.description, boxX + 22, y + 18, boxW - 44, 9,
                            i == cursor_ ? p.text : p.textDim,
                            i == 0 ? "guildperk.opt.a" : "guildperk.opt.b", 3);
    }
    ui::drawFooterHints({{input::primaryLabel(context_.input.map(), InputAction::Confirm,
                                              context_.input.activeDevice()),
                          "Choose"},
                         {input::primaryLabel(context_.input.map(), InputAction::Cancel,
                                              context_.input.activeDevice()),
                          "Later"}},
                        w, h, "guildperk.footer");
}

bool maybePushGuildPerkChoice(StateStack& stack, AppContext& context) {
    if (guildPendingPerkTown(context.party.guild) > 0) {
        stack.pushState(std::make_unique<GuildPerkChoiceState>(stack, context));
        return true;
    }
    return false;
}

}  // namespace cd
