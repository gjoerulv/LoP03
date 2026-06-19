#include <catch2/catch_test_macros.hpp>

#include "ui/TextInput.hpp"

using namespace cd::ui;

TEST_CASE("text input: accepts allowed chars, rejects others, respects max length", "[ui]") {
    TextInput t(5);
    t.appendCodepoint('A');
    t.appendCodepoint('!');  // disallowed
    t.appendCodepoint('b');
    t.appendCodepoint('1');
    t.appendCodepoint('-');
    t.appendCodepoint(' ');  // now "Ab1- " -> length 5
    REQUIRE(t.value() == "Ab1- ");
    REQUIRE(t.full());
    t.appendCodepoint('x');  // full, ignored
    REQUIRE(t.value().size() == 5);
}

TEST_CASE("text input: backspace and clear", "[ui]") {
    TextInput t(8, "Hero");
    t.backspace();
    REQUIRE(t.value() == "Her");
    t.clear();
    REQUIRE(t.empty());
    t.backspace();  // must not crash on empty
    REQUIRE(t.empty());
}

TEST_CASE("text input: setValue filters and truncates", "[ui]") {
    TextInput truncated(3, "abcdef");
    REQUIRE(truncated.value() == "abc");

    TextInput filtered(10, "a@b#c");
    REQUIRE(filtered.value() == "abc");
}

TEST_CASE("text input: trimmed removes surrounding spaces", "[ui]") {
    TextInput t(12, "  Rolan  ");
    REQUIRE(t.value() == "  Rolan  ");
    REQUIRE(t.trimmed() == "Rolan");

    TextInput blank(4, "    ");
    REQUIRE(blank.trimmed().empty());
}
