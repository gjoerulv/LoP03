#pragma once

#include <string>
#include <vector>

#include "ui/ScrollWindow.hpp"
#include "ui/TextLayout.hpp"

// Pure scrollable wrapped-prose viewport (M87, policy C): wraps a body once
// per (text, width, fontSize) change, caps the visible height in lines, and
// scrolls the rest through the same pure ScrollWindow the row lists use — so
// prose viewports and menus share one clamping model. No raylib; unit-tested
// headlessly. Rendering (ui/UiDraw::drawTextViewport) consumes the viewport.

namespace cd::ui {

class TextViewport {
public:
    // Wraps and caches. Re-calling with identical inputs is free (the key is
    // compared before wrapping), so states may call this from render() every
    // frame without per-frame wrapping cost. A content/width/font change
    // rewraps deterministically and resets the scroll to the top.
    void setContent(const std::string& text, int maxWidth, int fontSize,
                    const TextMeasure& measure) {
        if (wrapped_ && text == text_ && maxWidth == width_ && fontSize == fontSize_) {
            return;
        }
        text_ = text;
        width_ = maxWidth;
        fontSize_ = fontSize;
        lines_ = wrapText(text, maxWidth, fontSize, measure);
        window_.reset();
        wrapped_ = true;
    }

    // Viewport height in lines (>= 1). Independent of content: the widget,
    // not the amount of text, owns the visible height.
    void setVisibleLines(int lines) { visible_ = lines < 1 ? 1 : lines; }

    // Scrolls by delta lines, clamped to [0, lineCount - visibleLines].
    // Returns true when the view actually moved (callers key move SFX on it).
    bool scrollBy(int delta) {
        const int before = window_.top();
        window_.scrollBy(lineCount(), visible_, delta);
        return window_.top() != before;
    }
    void scrollToTop() { window_.reset(); }

    int lineCount() const { return static_cast<int>(lines_.size()); }
    int visibleLines() const { return visible_; }
    int wrapWidth() const { return width_; }
    int fontSize() const { return fontSize_; }
    int top() const { return window_.top(); }
    int visibleCount() const { return window_.visibleCount(lineCount(), visible_); }
    bool moreAbove() const { return window_.moreAbove(); }
    bool moreBelow() const { return window_.moreBelow(lineCount(), visible_); }
    bool scrollable() const { return lineCount() > visible_; }
    const std::string& line(int index) const {
        return lines_[static_cast<std::size_t>(index)];
    }

private:
    std::string text_;
    int width_ = 0;
    int fontSize_ = 0;
    int visible_ = 1;
    bool wrapped_ = false;
    std::vector<std::string> lines_;
    ScrollWindow window_;
};

}  // namespace cd::ui
