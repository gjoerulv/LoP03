#include "states/MilestoneChoiceState.hpp"

#include <memory>
#include <string>

#include "audio/AudioManager.hpp"
#include "content/ContentDatabase.hpp"
#include "core/AppContext.hpp"
#include "game/Milestones.hpp"
#include "game/Party.hpp"
#include "input/Input.hpp"
#include "input/PromptLabels.hpp"
#include "raylib.h"
#include "states/StateStack.hpp"
#include "ui/UiDraw.hpp"
#include "ui/UiStyle.hpp"

namespace cd {

MilestoneChoiceState::MilestoneChoiceState(StateStack& stack, AppContext& context)
    : GameState(stack), context_(context) {
    findPending();
}

bool MilestoneChoiceState::findPending() {
    for (std::size_t i = 0; i < context_.party.members.size(); ++i) {
        const int tier = pendingMilestoneTier(context_.party.members[i], context_.content);
        if (tier > 0) {
            member_ = static_cast<int>(i);
            tier_ = tier;
            cursor_ = 0;
            return true;
        }
    }
    member_ = -1;
    tier_ = 0;
    return false;
}

#ifdef CRYSTAL_CAPTURE
void MilestoneChoiceState::captureSelect(int member, int tier) {
    member_ = member;
    tier_ = tier;
    cursor_ = 0;
}
#endif

void MilestoneChoiceState::handleInput(const Input& input) {
    if (member_ < 0) {
        stack().popState();  // nothing pending (defensive; ctor already scanned)
        return;
    }
    if (input.navPressed(InputAction::MoveUp) || input.navPressed(InputAction::MoveDown)) {
        context_.audio.play(Sfx::Move);
        cursor_ = 1 - cursor_;
    }
    if (input.pressed(InputAction::Cancel)) {
        // Postpone: the pending choice simply re-prompts at the next
        // opportunity. Never a hostage screen.
        context_.audio.play(Sfx::Cancel);
        stack().popState();
        return;
    }
    if (input.pressed(InputAction::Confirm)) {
        Character& c = context_.party.members[static_cast<std::size_t>(member_)];
        const auto pair = context_.content.milestonePair(c.classId, tier_);
        const content::MilestoneDef* chosen = cursor_ == 0 ? pair.first : pair.second;
        if (chosen != nullptr) {
            if (std::string* slot = milestoneSlot(c, tier_)) {
                *slot = chosen->id;
            }
            refreshCharacter(c, context_.content);  // stat milestones apply now
            context_.audio.play(Sfx::Confirm);
        }
        // Drain the next pending choice (another tier or another member), or
        // step aside once everyone has chosen.
        if (!findPending()) {
            stack().popState();
        }
    }
}

void MilestoneChoiceState::render() {
    if (member_ < 0 || member_ >= static_cast<int>(context_.party.members.size())) {
        return;
    }
    const Character& c = context_.party.members[static_cast<std::size_t>(member_)];
    const auto pair = context_.content.milestonePair(c.classId, tier_);
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
    // M67: who is choosing, at a glance — the class portrait in the header
    // corner (the centered text keeps well clear of it).
    ui::drawActorPortrait(context_.resources, c.classId, boxX + 8, boxY + 7, 1);
    ui::drawTextCentered(TextFormat("LEVEL %d MILESTONE", tier_), w / 2, boxY + 10, 14, p.gold);
    ui::drawTextCentered((c.name + " chooses a permanent bonus.").c_str(), w / 2, boxY + 28, 10,
                         p.textDim);

    const content::MilestoneDef* options[2] = {pair.first, pair.second};
    const int optY[2] = {boxY + 44, boxY + 96};
    for (int i = 0; i < 2; ++i) {
        const content::MilestoneDef& m = *options[i];
        const int y = optY[i];
        if (i == cursor_) {
            ui::drawSelectionSlab(boxX + 10, y - 2, boxW - 20, 48);
        }
        ui::drawText(m.name, boxX + 22, y + 2, 12, i == cursor_ ? p.text : p.textDim);
        ui::drawTextWrapped(m.description, boxX + 22, y + 18, boxW - 44, 9,
                            i == cursor_ ? p.text : p.textDim,
                            i == 0 ? "milestone.opt.a" : "milestone.opt.b", 3);
    }
    ui::drawFooterHints({{input::primaryLabel(context_.input.map(), InputAction::Confirm,
                                              context_.input.activeDevice()),
                          "Choose"},
                         {input::primaryLabel(context_.input.map(), InputAction::Cancel,
                                              context_.input.activeDevice()),
                          "Later"}},
                        w, h, "milestone.footer");
}

bool maybePushMilestoneChoice(StateStack& stack, AppContext& context) {
    for (const Character& c : context.party.members) {
        if (pendingMilestoneTier(c, context.content) > 0) {
            stack.pushState(std::make_unique<MilestoneChoiceState>(stack, context));
            return true;
        }
    }
    return false;
}

}  // namespace cd
