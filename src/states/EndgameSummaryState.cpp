#include "states/EndgameSummaryState.hpp"

#include <string>
#include <utility>

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

constexpr int kListX = 22;
constexpr int kRowsY = 44;
constexpr int kRowH = 13;         // the 12px-pitch idiom at font 10, one more for descenders
constexpr int kVisibleRows = 13;  // rows between the page line and the footer

}  // namespace

EndgameSummaryState::EndgameSummaryState(StateStack& stack, AppContext& context, bool firstShow)
    : GameState(stack), context_(context), firstShow_(firstShow) {
    rebuildPage();
}

void EndgameSummaryState::onEnter() {
    // First showing: the victory stinger, then the Result loop; revisits go
    // straight to the loop. The town restores its own music on resume.
    if (firstShow_) {
        context_.audio.setMusicThen(MusicTrack::Victory, MusicTrack::Result);
    } else {
        context_.audio.setMusic(MusicTrack::Result);
    }
}

void EndgameSummaryState::rebuildPage() {
    rows_ = summaryRows(summaryPageAt(page_), context_.party, context_.content);
    std::vector<ui::MenuItem> items;
    for (const SummaryRow& r : rows_) {
        items.push_back({r.label, !r.header, r.value});
    }
    menu_.setItems(std::move(items));
    // Land on the first selectable row (a page may open with a header).
    for (std::size_t i = 0; i < rows_.size(); ++i) {
        if (!rows_[i].header) {
            menu_.setCursor(static_cast<int>(i));
            break;
        }
    }
    scroll_.reset();
    scroll_.follow(static_cast<int>(rows_.size()), kVisibleRows, menu_.cursor());
}

void EndgameSummaryState::handleInput(const Input& input) {
    if (input.pressed(InputAction::Cancel)) {
        context_.audio.play(Sfx::Cancel);
        stack().popState();
        return;
    }
    if (input.pressed(InputAction::CycleNext)) {
        page_ = (page_ + 1) % kSummaryPageCount;
        context_.audio.play(Sfx::Move);
        rebuildPage();
        return;
    }
    if (input.pressed(InputAction::CyclePrev)) {
        page_ = (page_ + kSummaryPageCount - 1) % kSummaryPageCount;
        context_.audio.play(Sfx::Move);
        rebuildPage();
        return;
    }
    const int total = static_cast<int>(rows_.size());
    if (total == 0) {
        return;
    }
    if (input.navPressed(InputAction::MoveDown)) {
        menu_.moveDown();
        context_.audio.play(Sfx::Move);
    } else if (input.navPressed(InputAction::MoveUp)) {
        menu_.moveUp();
        context_.audio.play(Sfx::Move);
    }
    scroll_.follow(total, kVisibleRows, menu_.cursor());
}

void EndgameSummaryState::render() {
    const int w = context_.virtualWidth;
    const int h = context_.virtualHeight;
    const ui::style::Palette& p = ui::style::palette();
    ClearBackground(p.canvas);
    ui::drawHeaderBand("End-game Summary", w, p.gold);

    const SummaryPage page = summaryPageAt(page_);
    const std::string pageLine = std::string(summaryPageName(page)) + "   " +
                                 std::to_string(page_ + 1) + "/" +
                                 std::to_string(kSummaryPageCount);
    ui::drawTextFitted(pageLine, kListX, ui::kHeaderBandH + 6, w - 2 * kListX,
                       ui::style::kFontBody, p.cursor, "summary.page");
    const int listW = w - 2 * kListX;
    ui::drawFrame(kListX - 10, kRowsY - 5, listW + 20, kVisibleRows * kRowH + 10,
                  ui::FrameStyle::Inset);
    ui::drawMenuScrolled(menu_, scroll_, kVisibleRows, kListX, kRowsY, kRowH,
                         ui::style::kFontBody, listW - 120, p.text, p.textDim, p.cursor,
                         "summary.rows", ui::style::kFontBody, p.gold);

    const InputMap& map = context_.input.map();
    const ActiveDevice device = context_.input.activeDevice();
    const std::string hint = input::prompt(map, InputAction::CyclePrev, device, "Prev") + " " +
                             input::prompt(map, InputAction::CycleNext, device, "Next page") +
                             "   " + input::prompt(map, InputAction::MoveUp, device, "") +
                             input::prompt(map, InputAction::MoveDown, device, "Scroll") +
                             "   " + input::prompt(map, InputAction::Cancel, device, "Back");
    ui::drawTextFitted(hint, 8, h - 13, w - 16, ui::style::kFontSmall, p.textHint,
                       "summary.footer");
}

#ifdef CRYSTAL_CAPTURE
void EndgameSummaryState::captureShowPage(int page) {
    page_ = ((page % kSummaryPageCount) + kSummaryPageCount) % kSummaryPageCount;
    rebuildPage();
}
#endif

}  // namespace cd
