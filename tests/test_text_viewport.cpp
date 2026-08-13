// M87 — the pure scrollable wrapped-prose viewport (ui::TextViewport) and the
// policy-B preview (ui::previewText): the container behavior every prose panel
// now rides. Headless: a fake per-byte measurer, no raylib.

#include <catch2/catch_test_macros.hpp>

#include <string>

#include "ui/TextLayout.hpp"
#include "ui/TextViewport.hpp"

using cd::ui::previewText;
using cd::ui::TextMeasure;
using cd::ui::TextViewport;

namespace {
// Fake measurer: every byte is `perChar` pixels wide, independent of size.
TextMeasure fixedMeasure(int perChar = 5) {
    return [perChar](const std::string& text, int) {
        return static_cast<int>(text.size()) * perChar;
    };
}

// "one two three four five six" wrapped at 5 chars/line -> one word per line.
const char* kSixWords = "one two three four five six";
}  // namespace

TEST_CASE("viewport: text shorter than the viewport shows everything", "[viewport]") {
    TextViewport vp;
    vp.setContent("hello world", 100, 10, fixedMeasure());
    vp.setVisibleLines(4);
    CHECK(vp.lineCount() == 1);
    CHECK(vp.visibleCount() == 1);
    CHECK_FALSE(vp.moreAbove());
    CHECK_FALSE(vp.moreBelow());
    CHECK_FALSE(vp.scrollable());
    CHECK_FALSE(vp.scrollBy(1));  // nothing to scroll to
    CHECK(vp.top() == 0);
}

TEST_CASE("viewport: an exact vertical fit neither scrolls nor flags", "[viewport]") {
    TextViewport vp;
    vp.setContent(kSixWords, 30, 10, fixedMeasure());  // 6 lines
    vp.setVisibleLines(6);
    CHECK(vp.lineCount() == 6);
    CHECK(vp.visibleCount() == 6);
    CHECK_FALSE(vp.moreAbove());
    CHECK_FALSE(vp.moreBelow());
    CHECK_FALSE(vp.scrollable());
    CHECK_FALSE(vp.scrollBy(1));
}

TEST_CASE("viewport: one line beyond the viewport scrolls exactly once", "[viewport]") {
    TextViewport vp;
    vp.setContent(kSixWords, 30, 10, fixedMeasure());  // 6 lines
    vp.setVisibleLines(5);
    CHECK(vp.scrollable());
    CHECK_FALSE(vp.moreAbove());
    CHECK(vp.moreBelow());
    CHECK(vp.scrollBy(1));
    CHECK(vp.top() == 1);
    CHECK(vp.moreAbove());
    CHECK_FALSE(vp.moreBelow());
    CHECK_FALSE(vp.scrollBy(1));  // clamped at the bottom
    CHECK(vp.top() == 1);
}

TEST_CASE("viewport: many lines beyond - scrolling walks and clamps both ends",
          "[viewport]") {
    TextViewport vp;
    vp.setContent(kSixWords, 30, 10, fixedMeasure());  // 6 lines
    vp.setVisibleLines(2);
    CHECK(vp.moreBelow());
    CHECK(vp.scrollBy(3));
    CHECK(vp.top() == 3);
    CHECK(vp.moreAbove());
    CHECK(vp.moreBelow());
    // A huge delta clamps to the last window, never past it.
    CHECK(vp.scrollBy(100));
    CHECK(vp.top() == 4);
    CHECK_FALSE(vp.moreBelow());
    CHECK(vp.top() + vp.visibleCount() == vp.lineCount());  // every line reachable
    // And back up, clamped at zero.
    CHECK(vp.scrollBy(-100));
    CHECK(vp.top() == 0);
    CHECK_FALSE(vp.moreAbove());
    CHECK_FALSE(vp.scrollBy(-1));
}

TEST_CASE("viewport: explicit paragraph breaks survive as empty lines", "[viewport]") {
    TextViewport vp;
    vp.setContent("alpha\n\nbeta", 100, 10, fixedMeasure());
    vp.setVisibleLines(2);
    REQUIRE(vp.lineCount() == 3);
    CHECK(vp.line(0) == "alpha");
    CHECK(vp.line(1).empty());
    CHECK(vp.line(2) == "beta");
    CHECK(vp.moreBelow());
}

TEST_CASE("viewport: an oversized token is broken, nothing exceeds the width",
          "[viewport]") {
    const auto measure = fixedMeasure();
    TextViewport vp;
    vp.setContent("abcdefghijkl", 25, 10, measure);  // 5 chars per line
    vp.setVisibleLines(2);
    REQUIRE(vp.lineCount() == 3);
    for (int i = 0; i < vp.lineCount(); ++i) {
        CHECK(measure(vp.line(i), 10) <= 25);
    }
    CHECK(vp.moreBelow());
}

TEST_CASE("viewport: utf-8 content never splits inside a codepoint", "[viewport]") {
    // Two 2-byte codepoints; width admits 3 bytes -> the split backs off to
    // the codepoint boundary (the wrapText contract, exercised end-to-end).
    TextViewport vp;
    vp.setContent("\xC3\xA9\xC3\xA9", 15, 10, fixedMeasure());
    REQUIRE(vp.lineCount() == 2);
    CHECK(vp.line(0) == "\xC3\xA9");
    CHECK(vp.line(1) == "\xC3\xA9");
}

TEST_CASE("viewport: rewrap is deterministic and scrolling never mutates lines",
          "[viewport]") {
    TextViewport vp;
    vp.setContent(kSixWords, 30, 10, fixedMeasure());
    vp.setVisibleLines(2);
    const std::string firstLine = vp.line(0);
    const int count = vp.lineCount();
    vp.scrollBy(3);
    CHECK(vp.lineCount() == count);
    CHECK(vp.line(0) == firstLine);
    // Same content key: a no-op that keeps the scroll position.
    vp.setContent(kSixWords, 30, 10, fixedMeasure());
    CHECK(vp.top() == 3);
    CHECK(vp.lineCount() == count);
    // A content change rewraps and resets to the top.
    vp.setContent("fresh words entirely", 30, 10, fixedMeasure());
    CHECK(vp.top() == 0);
}

TEST_CASE("viewport: the visible height is the widget's, not the text's", "[viewport]") {
    TextViewport vp;
    vp.setContent(kSixWords, 30, 10, fixedMeasure());
    vp.setVisibleLines(0);  // clamped to >= 1
    CHECK(vp.visibleLines() == 1);
    vp.setVisibleLines(4);
    CHECK(vp.visibleLines() == 4);
    CHECK(vp.visibleCount() == 4);
}

TEST_CASE("preview: hasMore is explicit and the lines stop at the budget", "[viewport]") {
    const auto measure = fixedMeasure();
    const auto more = previewText(kSixWords, 30, 10, 2, measure);
    CHECK(more.hasMore);
    REQUIRE(more.lines.size() == 2);
    CHECK(more.lines[0] == "one");

    const auto fits = previewText("one two", 30, 10, 2, measure);
    CHECK_FALSE(fits.hasMore);
    CHECK(fits.lines.size() == 2);

    const auto unlimited = previewText(kSixWords, 30, 10, 0, measure);
    CHECK_FALSE(unlimited.hasMore);
    CHECK(unlimited.lines.size() == 6);
}
