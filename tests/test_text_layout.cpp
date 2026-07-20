#include <catch2/catch_test_macros.hpp>

#include "ui/TextLayout.hpp"

using cd::ui::fitsWidth;
using cd::ui::lineHeight;
using cd::ui::TextMeasure;
using cd::ui::wrapText;

namespace {
// Fake measurer: every byte is `perChar` pixels wide, independent of size.
TextMeasure fixedMeasure(int perChar = 5) {
    return [perChar](const std::string& text, int) {
        return static_cast<int>(text.size()) * perChar;
    };
}
}  // namespace

TEST_CASE("wrap: short text stays on one line") {
    const auto lines = wrapText("hello world", 100, 10, fixedMeasure());
    REQUIRE(lines.size() == 1);
    CHECK(lines[0] == "hello world");
}

TEST_CASE("wrap: breaks at word boundaries within the width") {
    // 10 chars per line at 5px/char and width 50.
    const auto lines = wrapText("aaaa bbbb cccc", 50, 10, fixedMeasure());
    REQUIRE(lines.size() == 2);
    CHECK(lines[0] == "aaaa bbbb");
    CHECK(lines[1] == "cccc");
}

TEST_CASE("wrap: exact fit is not split") {
    // "aaaa bbbb" is 9 chars = 45px; width 45 fits exactly.
    const auto lines = wrapText("aaaa bbbb", 45, 10, fixedMeasure());
    REQUIRE(lines.size() == 1);
}

TEST_CASE("wrap: empty text produces one empty line") {
    const auto lines = wrapText("", 50, 10, fixedMeasure());
    REQUIRE(lines.size() == 1);
    CHECK(lines[0].empty());
}

TEST_CASE("wrap: explicit newlines start new lines and preserve empty ones") {
    const auto lines = wrapText("one\n\ntwo", 100, 10, fixedMeasure());
    REQUIRE(lines.size() == 3);
    CHECK(lines[0] == "one");
    CHECK(lines[1].empty());
    CHECK(lines[2] == "two");
}

TEST_CASE("wrap: oversized token is broken and nothing exceeds the width") {
    const auto measure = fixedMeasure();
    const auto lines = wrapText("abcdefghijkl", 25, 10, measure);  // 5 chars per line
    REQUIRE(lines.size() == 3);
    CHECK(lines[0] == "abcde");
    CHECK(lines[1] == "fghij");
    CHECK(lines[2] == "kl");
    for (const auto& line : lines) {
        CHECK(measure(line, 10) <= 25);
    }
}

TEST_CASE("wrap: oversized token mid-sentence flushes the current line first") {
    const auto lines = wrapText("ok abcdefghij end", 25, 10, fixedMeasure());
    REQUIRE(lines.size() == 4);
    CHECK(lines[0] == "ok");
    CHECK(lines[1] == "abcde");
    CHECK(lines[2] == "fghij");
    CHECK(lines[3] == "end");
}

TEST_CASE("wrap: degenerate width still emits at least one char per line") {
    const auto lines = wrapText("abc", 1, 10, fixedMeasure());  // nothing fits
    REQUIRE(lines.size() == 3);
    CHECK(lines[0] == "a");
}

TEST_CASE("wrap: repeated spaces collapse and do not create empty lines") {
    const auto lines = wrapText("a   b", 100, 10, fixedMeasure());
    REQUIRE(lines.size() == 1);
    CHECK(lines[0] == "a b");
}

TEST_CASE("wrap: utf-8 token is never split inside a codepoint") {
    // Two 2-byte codepoints (e.g. "éé" in UTF-8): 4 bytes total. With 5px per
    // byte and width 15, only 3 bytes "fit" — the split must back off to the
    // 2-byte boundary rather than cutting a sequence in half.
    const std::string twoCodepoints = "\xC3\xA9\xC3\xA9";
    const auto lines = wrapText(twoCodepoints, 15, 10, fixedMeasure());
    REQUIRE(lines.size() == 2);
    CHECK(lines[0] == "\xC3\xA9");
    CHECK(lines[1] == "\xC3\xA9");
}

TEST_CASE("fitsWidth and lineHeight basics") {
    CHECK(fitsWidth("abc", 15, 10, fixedMeasure()));
    CHECK_FALSE(fitsWidth("abcd", 15, 10, fixedMeasure()));
    CHECK(lineHeight(10) == 13);
    CHECK(lineHeight(8) == 11);
}
