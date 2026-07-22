#include "states/AchievementsState.hpp"

#include "core/AppContext.hpp"
#include "game/Achievements.hpp"
#include "input/Input.hpp"
#include "input/PromptLabels.hpp"
#include "raylib.h"
#include "states/StateStack.hpp"
#include "ui/UiDraw.hpp"
#include "ui/UiStyle.hpp"

namespace cd {

namespace style = ui::style;

namespace {
// The roster is laid out in two columns; it always splits evenly enough that the
// list ends clear of the description block and the footer hint.
constexpr int kColumnRows = (kAchievementCount + 1) / 2;
}  // namespace

AchievementsState::AchievementsState(StateStack& stack, AppContext& context)
    : GameState(stack), context_(context) {}

void AchievementsState::handleInput(const Input& input) {
    if (input.navPressed(InputAction::MoveUp) && cursor_ > 0) {
        --cursor_;
    }
    if (input.navPressed(InputAction::MoveDown) && cursor_ + 1 < kAchievementCount) {
        ++cursor_;
    }
    // The roster reads in two columns, so left/right jump a whole column.
    if (input.navPressed(InputAction::MoveLeft) && cursor_ >= kColumnRows) {
        cursor_ -= kColumnRows;
    }
    if (input.navPressed(InputAction::MoveRight) && cursor_ + kColumnRows < kAchievementCount) {
        cursor_ += kColumnRows;
    }
    if (input.pressed(InputAction::Cancel) || input.pressed(InputAction::Confirm)) {
        stack().popState();
    }
}

void AchievementsState::render() {
    const int w = context_.virtualWidth;
    const int h = context_.virtualHeight;
    ClearBackground(Color{16, 16, 24, 255});

    int unlockedCount = 0;
    for (const AchievementDef& a : kAchievements) {
        if (context_.achievements.isUnlocked(a.id)) {
            ++unlockedCount;
        }
    }
    ui::drawTextCentered("Achievements", w / 2, 14, style::kFontScreenTitle, style::palette().text);
    ui::drawTextCentered(TextFormat("%d / %d unlocked", unlockedCount, kAchievementCount), w / 2, 32,
                         style::kFontSmall, style::palette().textDim);

    // Two columns of kColumnRows, so the list ends well clear of the description
    // and the footer hint below it.
    const int rowsY = 50;
    const int rowH = 12;
    const int colW = 196;
    for (int i = 0; i < kAchievementCount; ++i) {
        const AchievementDef& a = kAchievements[i];
        const bool unlocked = context_.achievements.isUnlocked(a.id);
        const int col = i / kColumnRows;
        const int colX = 14 + col * (colW + 6);
        const int y = rowsY + (i % kColumnRows) * rowH;
        if (i == cursor_) {
            DrawRectangle(colX, y - 1, colW, rowH, Color{40, 44, 66, 255});
        }
        const Color c = unlocked ? style::palette().text : style::palette().textDim;
        ui::drawText(unlocked ? "[*]" : "[ ]", colX + 6, y, style::kFontBody, c);
        ui::drawTextFitted(unlocked ? a.name : "? ? ?", colX + 32, y, colW - 38, style::kFontBody,
                           c, "achievements.name");
    }

    // The cursor's description (hidden until unlocked, so it stays a goal),
    // centered under the list with clear air above the Back hint.
    const AchievementDef& cur = kAchievements[cursor_];
    const bool curUnlocked = context_.achievements.isUnlocked(cur.id);
    const int descY = rowsY + kColumnRows * rowH + 16;
    ui::drawTextWrappedCentered(
        curUnlocked ? std::string(cur.description)
                    : std::string("Locked - keep playing to discover this one."),
        w / 2, descY, w - 40, style::kFontBody,
        curUnlocked ? Color{200, 210, 160, 255} : style::palette().textDim, "achievements.desc", 3);

    ui::drawTextCentered(input::prompt(context_.input.map(), InputAction::Cancel,
                                       context_.input.activeDevice(), "Back")
                             .c_str(),
                         w / 2, h - style::kFooterHeight + 2, style::kFontBody,
                         style::palette().textHint);
}

}  // namespace cd
