#include "states/HallOfShameState.hpp"

#include <algorithm>
#include <memory>
#include <utility>

#include "audio/AudioManager.hpp"
#include "content/LoadReport.hpp"
#include "core/AppContext.hpp"
#include "game/FallenRuns.hpp"
#include "game/IronMan.hpp"
#include "game/Party.hpp"
#include "game/PlayTime.hpp"
#include "input/Input.hpp"
#include "input/PromptLabels.hpp"
#include "raylib.h"
#include "save/SaveSystem.hpp"
#include "states/EndgameSummaryState.hpp"
#include "states/StateStack.hpp"
#include "ui/UiDraw.hpp"
#include "ui/UiStyle.hpp"

namespace cd {

namespace {
constexpr int kRowX = 40;
constexpr int kRowsY = 50;
constexpr int kRowH = 25;       // a name line and a small where/who line
constexpr int kVisibleRows = 6;
constexpr int kClockRight = 40;  // from the right screen edge
}  // namespace

HallOfShameState::HallOfShameState(StateStack& stack, AppContext& context)
    : GameState(stack), context_(context) {}

int HallOfShameState::count() const { return static_cast<int>(context_.fallenRuns.runs.size()); }

#ifdef CRYSTAL_CAPTURE
void HallOfShameState::captureSelect(int row) {
    cursor_ = std::clamp(row, 0, std::max(0, count() - 1));
    scroll_.follow(count(), kVisibleRows, cursor_);
}
#endif

void HallOfShameState::open() {
    if (cursor_ < 0 || cursor_ >= count()) {
        return;
    }
    const FallenRun& run = context_.fallenRuns.runs[static_cast<std::size_t>(cursor_)];
    Party snapshot;
    content::LoadReport report;
    if (run.party.empty() ||
        !context_.saves.parseText(run.party, "fallen_runs.json", snapshot, report)) {
        // A record from a newer build, or content that has since changed: the
        // row still says where and who; only the pages are unavailable.
        context_.audio.play(Sfx::Error);
        message_ = "This run's details can no longer be read.";
        return;
    }
    snapshot.ironMan = true;  // the flag is never serialized; this party was one
    context_.audio.play(Sfx::Confirm);
    stack().pushState(std::make_unique<EndgameSummaryState>(
        stack(), context_, std::move(snapshot), ironman::FallenInfo{run.place, run.foes},
        /*leaveToTitle=*/false));
}

void HallOfShameState::handleInput(const Input& input) {
    const int total = count();
    if (input.pressed(InputAction::Cancel)) {
        context_.audio.play(Sfx::Cancel);
        stack().popState();
        return;
    }
    if (total == 0) {
        return;
    }
    if (input.navPressed(InputAction::MoveDown)) {
        cursor_ = (cursor_ + 1) % total;
        message_.clear();
        context_.audio.play(Sfx::Move);
    } else if (input.navPressed(InputAction::MoveUp)) {
        cursor_ = (cursor_ + total - 1) % total;
        message_.clear();
        context_.audio.play(Sfx::Move);
    }
    scroll_.follow(total, kVisibleRows, cursor_);
    if (input.pressed(InputAction::Confirm)) {
        open();
    }
}

void HallOfShameState::render() {
    const int w = context_.virtualWidth;
    const int h = context_.virtualHeight;
    namespace style = ui::style;
    const style::Palette& p = style::palette();
    ClearBackground(p.canvas);

    ui::drawTitlePlaque("Hall of Shame", w / 2, 14, 16);

    const int total = count();
    const int shown = std::min(kVisibleRows, std::max(1, total));
    ui::drawFrame(kRowX - 24, kRowsY - 10, w - 2 * (kRowX - 24), shown * kRowH + 16,
                  ui::FrameStyle::Standard);
    if (total == 0) {
        ui::drawTextCentered("Nobody has fallen. Yet.", w / 2, kRowsY + 4, style::kFontBody,
                             p.textDim);
    }
    const int top = scroll_.top();
    for (int row = 0; row < kVisibleRows && top + row < total; ++row) {
        const int index = top + row;
        const FallenRun& run = context_.fallenRuns.runs[static_cast<std::size_t>(index)];
        const int y = kRowsY + row * kRowH;
        const bool isCursor = index == cursor_;
        if (isCursor) {
            ui::drawSelectionSlab(kRowX - 14, y - 3, w - 2 * (kRowX - 14), kRowH - 1);
            ui::drawChevron(kRowX - 11, y + 1, p.cursor, ui::motionPhase());
        }
        // The clock first (it sets how far the name may run) - the save
        // slots' own format, greyed hour digits and all.
        const SlotPlayTime time = formatSlotPlayTime(run.playSeconds);
        const std::string grey = time.text.substr(0, static_cast<std::size_t>(time.greyChars));
        const std::string lit = time.text.substr(static_cast<std::size_t>(time.greyChars));
        const int clockX = w - kClockRight - ui::measureText(time.text, style::kFontBody);
        if (!grey.empty()) {
            ui::drawText(grey, clockX, y + 1, style::kFontBody, p.disabled);
        }
        ui::drawText(lit, w - kClockRight - ui::measureText(lit, style::kFontBody), y + 1,
                     style::kFontBody, p.text);

        const std::string name =
            (run.leader.empty() ? std::string("A nameless party") : run.leader + "'s party") +
            "  -  Lv." + std::to_string(run.highestLevel);
        ui::drawTextFitted(name, kRowX, y, clockX - 8 - kRowX, 12, isCursor ? p.cursor : p.text,
                           "shame.name");
        // Policy E (ui_style_guide §7): one line that cannot scroll, and the
        // full where/who is one Confirm away on the run's summary - so a line
        // too long for the row ends in "..." rather than overflowing.
        const std::string where = run.place + " - " + run.foes;
        ui::drawTextEllipsized(where, kRowX + 12, y + 12, w - kRowX - 12 - kClockRight,
                               style::kFontSmall, p.dangerText, "shame.where");
    }
    if (total > kVisibleRows) {
        ui::drawChipRight(std::to_string(cursor_ + 1) + " / " + std::to_string(total),
                          w - (kRowX - 24) - 6, kRowsY - 22, p.textDim);
    }

    if (!message_.empty()) {
        ui::drawBanner(ui::BannerKind::Danger, message_, 60, 212, w - 120, "shame.message");
    }
    const InputMap& map = context_.input.map();
    const ActiveDevice device = context_.input.activeDevice();
    if (total == 0) {
        ui::drawFooterHints({{input::primaryLabel(map, InputAction::Cancel, device), "Back"}}, w,
                            h, "shame.footer");
        return;
    }
    ui::drawFooterHints({{input::primaryLabel(map, InputAction::Confirm, device), "View"},
                         {input::primaryLabel(map, InputAction::Cancel, device), "Back"}},
                        w, h, "shame.footer");
}

}  // namespace cd
