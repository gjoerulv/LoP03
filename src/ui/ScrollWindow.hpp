#pragma once

#include <algorithm>

// Pure scrolling-window model for vertical lists: keeps a cursor (or a free
// scroll offset) inside a fixed number of visible rows. No raylib; unit-tested
// headlessly. Rendering (ui/UiDraw::drawMenuScrolled) consumes the window.

namespace cd::ui {

class ScrollWindow {
public:
    // Follow a cursor: after this, top() <= cursor < top() + visibleRows
    // (when total allows) and the window never shows blank rows at the end
    // while earlier items are hidden.
    void follow(int total, int visibleRows, int cursor) {
        visibleRows = std::max(1, visibleRows);
        if (total <= 0) {
            top_ = 0;
            return;
        }
        cursor = std::clamp(cursor, 0, total - 1);
        if (cursor < top_) {
            top_ = cursor;
        } else if (cursor >= top_ + visibleRows) {
            top_ = cursor - visibleRows + 1;
        }
        top_ = std::clamp(top_, 0, std::max(0, total - visibleRows));
    }

    // Free scrolling (no cursor), e.g. the scoreboard: move by delta rows.
    void scrollBy(int total, int visibleRows, int delta) {
        visibleRows = std::max(1, visibleRows);
        top_ = std::clamp(top_ + delta, 0, std::max(0, total - visibleRows));
    }

    int top() const { return top_; }
    void reset() { top_ = 0; }

    int visibleCount(int total, int visibleRows) const {
        return std::max(0, std::min(std::max(1, visibleRows), total - top_));
    }
    bool moreAbove() const { return top_ > 0; }
    bool moreBelow(int total, int visibleRows) const {
        return top_ + std::max(1, visibleRows) < total;
    }

private:
    int top_ = 0;
};

}  // namespace cd::ui
