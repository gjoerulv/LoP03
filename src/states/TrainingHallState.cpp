#include "states/TrainingHallState.hpp"

#include <algorithm>
#include <utility>
#include <vector>

#include "audio/AudioManager.hpp"
#include "content/ContentDatabase.hpp"
#include "core/AppContext.hpp"
#include "game/Party.hpp"
#include "input/Input.hpp"
#include "input/PromptLabels.hpp"
#include "raylib.h"
#include "states/StateStack.hpp"
#include "ui/UiDraw.hpp"
#include "ui/UiStyle.hpp"

namespace cd {

TrainingHallState::TrainingHallState(StateStack& stack, AppContext& context)
    : GameState(stack), context_(context) {}

void TrainingHallState::onEnter() { rebuildMembers(); }

#ifdef CRYSTAL_CAPTURE
void TrainingHallState::captureEnterPassives() {
    selectedMember_ = 0;
    phase_ = Phase::Passives;
    passiveMenu_.setCursor(0);
    rebuildPassives();
}
#endif

int TrainingHallState::trainingCost(int level) const { return 40 + level * 30; }

namespace {
bool owns(const Character& c, const std::string& id) {
    return std::find(c.ownedPassives.begin(), c.ownedPassives.end(), id) != c.ownedPassives.end();
}
}  // namespace

void TrainingHallState::rebuildMembers() {
    std::vector<ui::MenuItem> items;
    for (const Character& c : context_.party.members) {
        std::string eq = "-";
        if (const content::PassiveDef* p = context_.content.findPassive(c.equippedPassive)) {
            eq = p->name;
        }
        items.push_back(
            {TextFormat("%s  Lv.%d  [%s]", c.name.c_str(), c.level, eq.c_str()), true});
    }
    const int prev = memberMenu_.cursor();
    memberMenu_.setItems(std::move(items));
    memberMenu_.setCursor(prev);
}

void TrainingHallState::rebuildCharMenu() {
    const Character& c = context_.party.members[static_cast<std::size_t>(selectedMember_)];
    std::vector<ui::MenuItem> items;
    if (c.level >= kMaxLevel) {
        items.push_back({"Train (max level)", false});
    } else {
        const int cost = trainingCost(c.level);
        items.push_back(
            {TextFormat("Train to Lv.%d  (%dg)", c.level + 1, cost), context_.party.gold >= cost});
    }
    items.push_back({"Manage Passives", true});
    charMenu_.setItems(std::move(items));
}

void TrainingHallState::rebuildPassives() {
    passiveIds_.clear();
    for (const auto& [id, def] : context_.content.passives()) {
        passiveIds_.push_back(id);
    }
    std::sort(passiveIds_.begin(), passiveIds_.end());  // stable, deterministic order

    const Character& c = context_.party.members[static_cast<std::size_t>(selectedMember_)];
    std::vector<ui::MenuItem> items;
    for (const std::string& id : passiveIds_) {
        const content::PassiveDef* p = context_.content.findPassive(id);
        std::string label = p != nullptr ? p->name : id;
        const bool equipped = c.equippedPassive == id;
        const bool owned = owns(c, id);
        const int price = p != nullptr ? p->price : 0;
        if (equipped) {
            label += "  [Equipped]";
        } else if (owned) {
            label += "  [Owned]";
        } else {
            label += "  " + std::to_string(price) + "g";
        }
        const bool enabled = equipped || owned || context_.party.gold >= price;
        items.push_back({std::move(label), enabled});
    }
    const int prev = passiveMenu_.cursor();
    passiveMenu_.setItems(std::move(items));
    passiveMenu_.setCursor(prev);
}

void TrainingHallState::trainSelected() {
    Character& c = context_.party.members[static_cast<std::size_t>(selectedMember_)];
    if (c.level >= kMaxLevel) {
        return;
    }
    const int cost = trainingCost(c.level);
    if (context_.party.gold >= cost) {
        context_.party.gold -= cost;
        grantXp(c, xpToNext(c.level) - c.xp, context_.content);  // exactly one level
        context_.audio.play(Sfx::Heal);
        message_ = c.name + " trained to Lv." + std::to_string(c.level) + "!";
        messageIsError_ = false;
    } else {
        context_.audio.play(Sfx::Error);
        message_ = "Not enough gold to train " + c.name;
        messageIsError_ = true;
    }
}

void TrainingHallState::onPassiveConfirm() {
    if (passiveIds_.empty() || !passiveMenu_.currentEnabled()) {
        return;
    }
    const std::string& id = passiveIds_[static_cast<std::size_t>(passiveMenu_.cursor())];
    Character& c = context_.party.members[static_cast<std::size_t>(selectedMember_)];
    const content::PassiveDef* p = context_.content.findPassive(id);
    const std::string name = p != nullptr ? p->name : id;
    if (c.equippedPassive == id) {
        c.equippedPassive.clear();  // swap freely: unequip
        message_ = c.name + " unequips " + name + ".";
        messageIsError_ = false;
    } else if (owns(c, id)) {
        c.equippedPassive = id;  // equip an owned passive
        message_ = c.name + " equips " + name + ".";
        messageIsError_ = false;
    } else if (p != nullptr && context_.party.gold >= p->price) {
        context_.party.gold -= p->price;
        c.ownedPassives.push_back(id);
        c.equippedPassive = id;  // auto-equip a fresh purchase
        context_.audio.play(Sfx::Heal);
        message_ = c.name + " learns " + name + "!";
        messageIsError_ = false;
        rebuildPassives();
        context_.audio.play(Sfx::Confirm);
        return;
    } else {
        context_.audio.play(Sfx::Error);
        message_ = "Not enough gold.";
        messageIsError_ = true;
        return;
    }
    context_.audio.play(Sfx::Confirm);
    rebuildPassives();
}

void TrainingHallState::handleInput(const Input& input) {
    const bool up = input.navPressed(InputAction::MoveUp);
    const bool down = input.navPressed(InputAction::MoveDown);

    switch (phase_) {
        case Phase::Members: {
            if (up) memberMenu_.moveUp();
            if (down) memberMenu_.moveDown();
            if (up || down) context_.audio.play(Sfx::Move);
            if (input.pressed(InputAction::Confirm)) {
                selectedMember_ = memberMenu_.cursor();
                phase_ = Phase::CharMenu;
                charMenu_.setCursor(0);
                rebuildCharMenu();
                message_.clear();
                context_.audio.play(Sfx::Confirm);
            }
            if (input.pressed(InputAction::Cancel)) {
                context_.audio.play(Sfx::Cancel);
                stack().popState();
            }
            break;
        }
        case Phase::CharMenu: {
            if (up) charMenu_.moveUp();
            if (down) charMenu_.moveDown();
            if (up || down) context_.audio.play(Sfx::Move);
            if (input.pressed(InputAction::Confirm) && charMenu_.currentEnabled()) {
                if (charMenu_.cursor() == 0) {
                    trainSelected();
                    rebuildCharMenu();
                    rebuildMembers();
                } else {
                    phase_ = Phase::Passives;
                    passiveMenu_.setCursor(0);
                    rebuildPassives();
                    message_.clear();
                }
                context_.audio.play(Sfx::Confirm);
            }
            if (input.pressed(InputAction::Cancel)) {
                phase_ = Phase::Members;
                rebuildMembers();
                message_.clear();
                context_.audio.play(Sfx::Cancel);
            }
            break;
        }
        case Phase::Passives: {
            if (up) passiveMenu_.moveUp();
            if (down) passiveMenu_.moveDown();
            if (up || down) context_.audio.play(Sfx::Move);
            if (input.pressed(InputAction::Confirm)) {
                onPassiveConfirm();
                rebuildMembers();  // keep the members list's equipped tag fresh
            }
            if (input.pressed(InputAction::Cancel)) {
                phase_ = Phase::CharMenu;
                rebuildCharMenu();
                message_.clear();
                context_.audio.play(Sfx::Cancel);
            }
            break;
        }
    }
}

void TrainingHallState::render() {
    const int w = context_.virtualWidth;
    const int h = context_.virtualHeight;
    namespace style = ui::style;
    const style::Palette& p = style::palette();
    ui::drawSceneBackground(context_.resources, "bg.training_hall", p.canvas,
                            context_.virtualWidth, context_.virtualHeight,
                            context_.party.currentTown);
    ui::drawHeaderBand("Training Hall", w, p.crystal, context_.party.gold);

    const InputMap& map = context_.input.map();
    const ActiveDevice device = context_.input.activeDevice();

    switch (phase_) {
        case Phase::Members: {
            ui::drawTextCentered("Choose a character to train or equip a passive.", w / 2, 30,
                                 style::kFontBody, p.textDim);
            const int rows = static_cast<int>(memberMenu_.size());
            ui::drawFrame(28, 50, w - 56, rows * 16 + 16, ui::FrameStyle::Inset);
            ui::drawMenu(memberMenu_, 48, 60, 16, style::kFontMenu, p.text, p.disabled, p.cursor);
            break;
        }
        case Phase::CharMenu: {
            const Character& c = context_.party.members[static_cast<std::size_t>(selectedMember_)];
            ui::drawTextCentered(TextFormat("%s  -  Lv.%d", c.name.c_str(), c.level), w / 2, 32,
                                 12, p.text);
            const int rows = static_cast<int>(charMenu_.size());
            ui::drawFrame(40, 56, 260, rows * 18 + 18, ui::FrameStyle::Inset);
            ui::drawMenu(charMenu_, 62, 66, 18, 12, p.text, p.disabled, p.cursor);
            break;
        }
        case Phase::Passives: {
            const Character& c = context_.party.members[static_cast<std::size_t>(selectedMember_)];
            ui::drawText(TextFormat("%s's passives  (own many, equip one)", c.name.c_str()), 24,
                         32, style::kFontBody, p.textDim);
            const int rows = static_cast<int>(passiveMenu_.size());
            ui::drawFrame(16, 44, 320, rows * 13 + 14, ui::FrameStyle::Inset);
            ui::drawMenu(passiveMenu_, 32, 51, 13, style::kFontBody, p.text, p.disabled, p.cursor);
            if (!passiveIds_.empty()) {
                const std::string& id =
                    passiveIds_[static_cast<std::size_t>(passiveMenu_.cursor())];
                if (const content::PassiveDef* pd = context_.content.findPassive(id)) {
                    ui::drawFrame(24, h - 50, w - 48, 30, ui::FrameStyle::Standard);
                    ui::drawTextWrapped(pd->description, 32, h - 44, w - 64, style::kFontBody,
                                        p.textDim, "training.passivedesc", 2);
                }
            }
            break;
        }
    }

    if (!message_.empty() && phase_ != Phase::Passives) {
        ui::drawBanner(messageIsError_ ? ui::BannerKind::Danger : ui::BannerKind::Success,
                       message_, 60, h - 50, w - 120, "training.message");
    }

    const char* verb = phase_ == Phase::Members   ? "Select"
                       : phase_ == Phase::CharMenu ? "Choose"
                                                   : "Buy/Equip";
    ui::drawFooterHints({{input::primaryLabel(map, InputAction::Confirm, device), verb},
                         {input::primaryLabel(map, InputAction::Cancel, device), "Back"}},
                        w, h, "training.footer");
}

}  // namespace cd
